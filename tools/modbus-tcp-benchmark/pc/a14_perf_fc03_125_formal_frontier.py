import argparse
import csv
import socket
import struct
import sys
import time
from pathlib import Path

import serial

sys.path.insert(
    0,
    str(Path(__file__).resolve().parent)
)

import a14_perf_fc03_qualification_sweep as q
import a14_perf_fc03_saturation as sat


QUANTITY = 125
REQUEST_ADU_BYTES = 12
RESPONSE_ADU_BYTES = 259
TOTAL_ADU_BYTES = 271
USEFUL_DATA_BYTES = 250


def precise_wait_until(target):
    while True:
        now = time.perf_counter()
        remaining = target - now

        if remaining <= 0:
            return

        if remaining > 0.001:
            time.sleep(
                max(
                    0.0,
                    remaining - 0.0005
                )
            )
        else:
            # Spin corto para no convertir al scheduler
            # de Windows en el limitante cerca de 1 kHz.
            pass


def run_case(
    ser,
    host,
    port,
    rate,
    duration
):
    q.wait_server_ready(
        ser,
        timeout_s=20.0,
        require_disconnected=True
    )

    sock, connect_attempts = (
        sat.connect_with_ready_retry(
            ser,
            host,
            port,
            total_timeout_s=20.0
        )
    )

    # Reset DESPUÉS de establecer la sesión:
    # la preparación queda fuera de la medición.
    q.reset_stats(ser)

    expected = q.expected_body(
        QUANTITY
    )

    target_requests = int(
        round(
            rate * duration
        )
    )

    latencies_us = []

    sent = 0
    ok = 0

    timeouts = 0
    transport_errors = 0
    protocol_errors = 0
    unexpected_resets = 0

    tx_bytes = 0
    rx_bytes = 0
    useful_data_bytes = 0

    start = time.perf_counter()
    deadline = start + duration

    try:
        for i in range(target_requests):
            scheduled = (
                start +
                i / rate
            )

            if scheduled >= deadline:
                break

            if time.perf_counter() >= deadline:
                break

            precise_wait_until(
                scheduled
            )

            if time.perf_counter() >= deadline:
                break

            tid = (
                (sent + 1) &
                0xFFFF
            )

            request = struct.pack(
                ">HHHBBHH",
                tid,
                0,
                6,
                1,
                3,
                0,
                QUANTITY
            )

            t0_ns = time.perf_counter_ns()

            try:
                sock.sendall(
                    request
                )

                sent += 1
                tx_bytes += len(
                    request
                )

                header = q.recv_exact(
                    sock,
                    7
                )

                (
                    rx_tid,
                    pid,
                    length,
                    unit
                ) = struct.unpack(
                    ">HHHB",
                    header
                )

                if length < 2:
                    raise ValueError(
                        f"MBAP length inválido: "
                        f"{length}"
                    )

                body = q.recv_exact(
                    sock,
                    length - 1
                )

                t1_ns = (
                    time.perf_counter_ns()
                )

                latencies_us.append(
                    (t1_ns - t0_ns) /
                    1000.0
                )

                rx_bytes += (
                    len(header) +
                    len(body)
                )

                valid = (
                    rx_tid == tid and
                    pid == 0 and
                    unit == 1 and
                    length == 253 and
                    body == expected
                )

                if valid:
                    ok += 1

                    useful_data_bytes += (
                        USEFUL_DATA_BYTES
                    )
                else:
                    protocol_errors += 1
                    break

            except socket.timeout:
                timeouts += 1
                break

            except (
                ConnectionError,
                OSError
            ):
                transport_errors += 1
                break

            except ValueError:
                protocol_errors += 1
                break

        # La ventana formal siempre dura 30 s.
        now = time.perf_counter()

        if now < deadline:
            time.sleep(
                deadline - now
            )

        elapsed = (
            time.perf_counter() -
            start
        )

        unexpected_resets += (
            q.drain_serial_after_case(
                ser
            )
        )

        ser.write(b"S\n")
        ser.flush()

        snapshot = q.collect_snapshot(
            ser,
            5.0,
            echo=False
        )

    finally:
        try:
            sock.close()
        except Exception:
            pass

    if elapsed <= 0:
        elapsed = 0.000001

    achieved_req_s = (
        ok / elapsed
    )

    achieved_ratio = (
        achieved_req_s /
        rate
        if rate > 0
        else 0.0
    )

    tx_mbps = (
        tx_bytes *
        8.0 /
        elapsed /
        1_000_000.0
    )

    rx_mbps = (
        rx_bytes *
        8.0 /
        elapsed /
        1_000_000.0
    )

    total_mbps = (
        tx_mbps +
        rx_mbps
    )

    useful_mbps = (
        useful_data_bytes *
        8.0 /
        elapsed /
        1_000_000.0
    )

    target_total_mbps = (
        rate *
        TOTAL_ADU_BYTES *
        8.0 /
        1_000_000.0
    )

    target_useful_mbps = (
        rate *
        USEFUL_DATA_BYTES *
        8.0 /
        1_000_000.0
    )

    if latencies_us:
        latency_avg = (
            sum(latencies_us) /
            len(latencies_us)
        )

        latency_p95 = q.percentile(
            latencies_us,
            0.95
        )

        latency_p99 = q.percentile(
            latencies_us,
            0.99
        )

        latency_max = max(
            latencies_us
        )
    else:
        latency_avg = 0.0
        latency_p95 = 0.0
        latency_p99 = 0.0
        latency_max = 0.0

    server_connections = q.intval(
        snapshot,
        "CLIENT_CONNECTIONS"
    )

    server_rx = q.intval(
        snapshot,
        "RX_FRAMES"
    )

    server_tx = q.intval(
        snapshot,
        "TX_FRAMES"
    )

    server_ok = q.intval(
        snapshot,
        "REQUESTS_OK"
    )

    server_ex = q.intval(
        snapshot,
        "EXCEPTIONS_SENT"
    )

    server_protocol = q.intval(
        snapshot,
        "PROTOCOL_ERRORS"
    )

    server_timeouts = q.intval(
        snapshot,
        "FRAME_TIMEOUTS"
    )

    server_bus = q.intval(
        snapshot,
        "BUS_LOCK_TIMEOUTS"
    )

    loop_avg = q.intval(
        snapshot,
        "LOOP_GAP_AVG_US"
    )

    loop_max = q.intval(
        snapshot,
        "LOOP_GAP_MAX_US"
    )

    cross_count_pass = (
        server_connections == 1 and
        server_rx == sent and
        server_tx == ok and
        server_ok == ok
    )

    clean = (
        timeouts == 0 and
        transport_errors == 0 and
        protocol_errors == 0 and
        unexpected_resets == 0 and
        server_ex == 0 and
        server_protocol == 0 and
        server_timeouts == 0 and
        server_bus == 0 and
        cross_count_pass and
        snapshot.get(
            "SERVER_READY"
        ) == "YES"
    )

    rate_pass = (
        achieved_ratio >= 0.95
    )

    stable_pass = (
        clean and
        rate_pass
    )

    if stable_pass:
        classification = (
            "STABLE_PASS"
        )
    elif clean:
        classification = (
            "SATURATION_FAIL"
        )
    else:
        classification = (
            "ERROR_FAIL"
        )

    return {
        "requested_req_s": rate,
        "duration_s": elapsed,
        "target_requests":
            target_requests,
        "requests_sent": sent,
        "requests_ok": ok,
        "achieved_req_s":
            achieved_req_s,
        "achieved_percent":
            achieved_ratio * 100.0,
        "tcp_connect_attempts":
            connect_attempts,
        "tx_tcp_payload_mbps":
            tx_mbps,
        "rx_tcp_payload_mbps":
            rx_mbps,
        "total_tcp_payload_mbps":
            total_mbps,
        "useful_register_data_mbps":
            useful_mbps,
        "target_total_payload_mbps":
            target_total_mbps,
        "target_useful_data_mbps":
            target_useful_mbps,
        "latency_avg_us":
            latency_avg,
        "latency_p95_us":
            latency_p95,
        "latency_p99_us":
            latency_p99,
        "latency_max_us":
            latency_max,
        "timeouts": timeouts,
        "transport_errors":
            transport_errors,
        "protocol_errors":
            protocol_errors,
        "unexpected_resets":
            unexpected_resets,
        "server_connections":
            server_connections,
        "server_rx_frames":
            server_rx,
        "server_tx_frames":
            server_tx,
        "server_requests_ok":
            server_ok,
        "server_exceptions":
            server_ex,
        "server_protocol_errors":
            server_protocol,
        "server_frame_timeouts":
            server_timeouts,
        "server_bus_lock_timeouts":
            server_bus,
        "loop_gap_avg_us":
            loop_avg,
        "loop_gap_max_us":
            loop_max,
        "cross_count_pass":
            cross_count_pass,
        "clean_case": clean,
        "rate_pass": rate_pass,
        "stable_pass":
            stable_pass,
        "classification":
            classification,
    }


def print_result(row):
    print()
    print(
        "FORMAL_RESULT "
        f"RATE="
        f"{row['requested_req_s']:.0f} "
        f"ACHIEVED="
        f"{row['achieved_req_s']:.1f} "
        f"ACHIEVED_PCT="
        f"{row['achieved_percent']:.2f} "
        f"TOTAL_MBPS="
        f"{row['total_tcp_payload_mbps']:.3f} "
        f"USEFUL_MBPS="
        f"{row['useful_register_data_mbps']:.3f} "
        f"P95_US="
        f"{row['latency_p95_us']:.1f} "
        f"P99_US="
        f"{row['latency_p99_us']:.1f} "
        f"MAX_US="
        f"{row['latency_max_us']:.1f} "
        f"LOOP_AVG_US="
        f"{row['loop_gap_avg_us']} "
        f"LOOP_MAX_US="
        f"{row['loop_gap_max_us']} "
        f"RESULT="
        f"{row['classification']}"
    )

    print(
        f"  OK="
        f"{row['requests_ok']}/"
        f"{row['target_requests']} "
        f"SENT="
        f"{row['requests_sent']} "
        f"CONNECT_ATTEMPTS="
        f"{row['tcp_connect_attempts']} "
        f"TIMEOUTS="
        f"{row['timeouts']} "
        f"TRANSPORT_ERRORS="
        f"{row['transport_errors']} "
        f"PROTOCOL_ERRORS="
        f"{row['protocol_errors']} "
        f"BUS_LOCK_TIMEOUTS="
        f"{row['server_bus_lock_timeouts']} "
        f"CROSS_COUNT_PASS="
        f"{'YES' if row['cross_count_pass'] else 'NO'}"
    )


def main():
    parser = argparse.ArgumentParser()

    parser.add_argument(
        "--serial",
        default="COM14"
    )

    parser.add_argument(
        "--baud",
        type=int,
        default=115200
    )

    parser.add_argument(
        "--host",
        default="192.168.0.31"
    )

    parser.add_argument(
        "--port",
        type=int,
        default=502
    )

    parser.add_argument(
        "--duration",
        type=float,
        default=30.0
    )

    parser.add_argument(
        "--csv",
        required=True
    )

    args = parser.parse_args()

    rates = [
        1000,
        1100,
        1150,
        1200,
        1225,
        1250
    ]

    print()
    print("=" * 76)
    print(
        " A14.3 PERF-S1 FC03/125 "
        "FORMAL FRONTIER"
    )
    print("=" * 76)

    print(
        f"TARGET="
        f"{args.host}:{args.port}"
    )

    print(
        f"SERIAL="
        f"{args.serial}"
    )

    print(
        f"DURATION_PER_CASE_S="
        f"{args.duration:.1f}"
    )

    print(
        f"QUANTITY_REGISTERS="
        f"{QUANTITY}"
    )

    print(
        "STABLE_CRITERION="
        "CLEAN_AND_ACHIEVED_GTE_95_PERCENT"
    )

    print(
        "NO_WAIT_REFERENCE_REQ_S=1142.4"
    )

    print(
        "NO_WAIT_REFERENCE_TOTAL_MBPS=2.477"
    )

    ser = serial.Serial(
        args.serial,
        args.baud,
        timeout=0.10,
        write_timeout=1.0
    )

    rows = []

    try:
        time.sleep(0.3)

        ser.reset_input_buffer()

        initial = q.request_snapshot(
            ser,
            echo=True
        )

        if (
            initial.get(
                "SERVER_READY"
            ) != "YES"
        ):
            q.wait_server_ready(
                ser,
                timeout_s=20.0,
                require_disconnected=True
            )

        print(
            "ACTIVE_SERIAL_HANDSHAKE=PASS"
        )

        for index, rate in enumerate(
            rates,
            start=1
        ):
            print()
            print(
                f"CASE_BEGIN="
                f"{index}/{len(rates)} "
                f"RATE={rate} "
                f"Q={QUANTITY}"
            )

            row = run_case(
                ser,
                args.host,
                args.port,
                float(rate),
                args.duration
            )

            rows.append(
                row
            )

            print_result(
                row
            )

            time.sleep(0.25)

    finally:
        ser.close()

    with open(
        args.csv,
        "w",
        newline="",
        encoding="utf-8"
    ) as f:
        writer = csv.DictWriter(
            f,
            fieldnames=list(
                rows[0].keys()
            )
        )

        writer.writeheader()
        writer.writerows(
            rows
        )

    stable_rows = [
        r
        for r in rows
        if r["stable_pass"]
    ]

    unstable_rows = [
        r
        for r in rows
        if not r["stable_pass"]
    ]

    print()
    print("=" * 76)
    print(" FORMAL FRONTIER SUMMARY")
    print("=" * 76)

    print(
        f"CASES_EXECUTED="
        f"{len(rows)}"
    )

    print(
        f"STABLE_PASS_POINTS="
        f"{len(stable_rows)}"
    )

    print(
        f"UNSTABLE_POINTS="
        f"{len(unstable_rows)}"
    )

    if stable_rows:
        best = max(
            stable_rows,
            key=lambda r:
                r["requested_req_s"]
        )

        print(
            f"MAX_30S_STABLE_REQ_S="
            f"{best['requested_req_s']:.0f}"
        )

        print(
            f"MAX_30S_STABLE_ACHIEVED_REQ_S="
            f"{best['achieved_req_s']:.1f}"
        )

        print(
            f"MAX_30S_STABLE_TOTAL_MBPS="
            f"{best['total_tcp_payload_mbps']:.3f}"
        )

        print(
            f"MAX_30S_STABLE_USEFUL_MBPS="
            f"{best['useful_register_data_mbps']:.3f}"
        )

        print(
            f"MAX_30S_STABLE_P99_US="
            f"{best['latency_p99_us']:.1f}"
        )
    else:
        print(
            "MAX_30S_STABLE_REQ_S=NONE"
        )

    if unstable_rows:
        first_fail = min(
            unstable_rows,
            key=lambda r:
                r["requested_req_s"]
        )

        print(
            f"FIRST_30S_UNSTABLE_REQ_S="
            f"{first_fail['requested_req_s']:.0f}"
        )

        print(
            f"FIRST_30S_UNSTABLE_REASON="
            f"{first_fail['classification']}"
        )
    else:
        print(
            "FIRST_30S_UNSTABLE_REQ_S=NONE"
        )

    print(
        f"CSV={args.csv}"
    )

    print()
    print(
        "A14_3_FC03_125_FORMAL_FRONTIER="
        "PASS_EXECUTED"
    )

    print(
        "NEXT=5MIN_BOUNDARY_CONFIRMATION"
    )

    return 0


if __name__ == "__main__":
    try:
        sys.exit(
            main()
        )

    except Exception as exc:
        print()

        print(
            "HARNESS_FATAL="
            f"{type(exc).__name__}: "
            f"{exc}"
        )

        print(
            "A14_3_FC03_125_FORMAL_FRONTIER="
            "FAIL_HARNESS"
        )

        sys.exit(2)