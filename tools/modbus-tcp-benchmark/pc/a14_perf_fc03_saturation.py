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


def connect_with_ready_retry(
    ser,
    host,
    port,
    total_timeout_s=20.0
):
    deadline = (
        time.monotonic() +
        total_timeout_s
    )

    attempt = 0
    last_error = None

    while time.monotonic() < deadline:
        attempt += 1

        q.wait_server_ready(
            ser,
            timeout_s=5.0,
            require_disconnected=True
        )

        print(
            f"TCP_CONNECT_ATTEMPT="
            f"{attempt}"
        )

        try:
            sock = socket.create_connection(
                (host, port),
                timeout=2.0
            )

            sock.settimeout(1.0)

            print(
                f"TCP_CONNECT_ATTEMPT_"
                f"{attempt}=PASS"
            )

            return sock, attempt

        except (
            TimeoutError,
            ConnectionRefusedError,
            OSError
        ) as exc:
            last_error = exc

            print(
                f"TCP_CONNECT_ATTEMPT_"
                f"{attempt}=RETRY "
                f"ERROR="
                f"{type(exc).__name__}: "
                f"{exc}"
            )

            time.sleep(0.50)

    raise TimeoutError(
        "TCP connect no se estableció "
        f"en {total_timeout_s:.1f} s; "
        f"último error: {last_error}"
    )


def run_saturation_case(
    ser,
    host,
    port,
    duration,
    quantity
):
    q.wait_server_ready(
        ser,
        timeout_s=20.0,
        require_disconnected=True
    )

    sock, connect_attempts = (
        connect_with_ready_retry(
            ser,
            host,
            port,
            total_timeout_s=20.0
        )
    )

    # Las estadísticas se reinician DESPUÉS de conseguir
    # la sesión TCP. Así, retries de preparación no
    # contaminan la ventana de rendimiento.
    q.reset_stats(ser)

    expected = q.expected_body(
        quantity
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
        while time.perf_counter() < deadline:
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
                quantity
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

                t1_ns = time.perf_counter_ns()

                latency_us = (
                    t1_ns - t0_ns
                ) / 1000.0

                latencies_us.append(
                    latency_us
                )

                rx_bytes += (
                    len(header) +
                    len(body)
                )

                expected_length = (
                    3 +
                    2 * quantity
                )

                valid = (
                    rx_tid == tid and
                    pid == 0 and
                    unit == 1 and
                    length == expected_length and
                    body == expected
                )

                if valid:
                    ok += 1
                    useful_data_bytes += (
                        2 * quantity
                    )
                else:
                    protocol_errors += 1

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

    req_s = (
        ok / elapsed
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

    if latencies_us:
        latency_min = min(
            latencies_us
        )

        latency_avg = (
            sum(latencies_us) /
            len(latencies_us)
        )

        latency_p50 = q.percentile(
            latencies_us,
            0.50
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
        latency_min = 0.0
        latency_avg = 0.0
        latency_p50 = 0.0
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

    errors = (
        timeouts +
        transport_errors +
        protocol_errors
    )

    cross_count_pass = (
        server_connections == 1 and
        server_rx == sent and
        server_tx == sent and
        server_ok == ok
    )

    clean = (
        errors == 0 and
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

    return {
        "quantity": quantity,
        "duration_s": elapsed,
        "tcp_connect_attempts":
            connect_attempts,
        "requests_sent": sent,
        "requests_ok": ok,
        "req_s": req_s,
        "tx_bytes": tx_bytes,
        "rx_bytes": rx_bytes,
        "useful_data_bytes":
            useful_data_bytes,
        "tx_tcp_payload_mbps":
            tx_mbps,
        "rx_tcp_payload_mbps":
            rx_mbps,
        "total_tcp_payload_mbps":
            total_mbps,
        "useful_register_data_mbps":
            useful_mbps,
        "timeouts": timeouts,
        "transport_errors":
            transport_errors,
        "protocol_errors":
            protocol_errors,
        "unexpected_resets":
            unexpected_resets,
        "latency_min_us":
            latency_min,
        "latency_avg_us":
            latency_avg,
        "latency_p50_us":
            latency_p50,
        "latency_p95_us":
            latency_p95,
        "latency_p99_us":
            latency_p99,
        "latency_max_us":
            latency_max,
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
        "clean_case":
            clean,
    }


def print_case(row):
    status = (
        "CLEAN"
        if row["clean_case"]
        else "WITH_ERRORS"
    )

    print()
    print(
        "SATURATION_RESULT "
        f"Q={row['quantity']} "
        f"CONNECT_ATTEMPTS="
        f"{row['tcp_connect_attempts']} "
        f"REQ_S={row['req_s']:.1f} "
        f"TX_MBPS="
        f"{row['tx_tcp_payload_mbps']:.3f} "
        f"RX_MBPS="
        f"{row['rx_tcp_payload_mbps']:.3f} "
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
        f"RESULT={status}"
    )

    print(
        f"  REQUESTS_OK="
        f"{row['requests_ok']}/"
        f"{row['requests_sent']} "
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
        default=10.0
    )

    parser.add_argument(
        "--csv",
        required=True
    )

    args = parser.parse_args()

    quantities = [
        1,
        16,
        64,
        125
    ]

    print()
    print("=" * 76)
    print(
        " A14.3 PERF-S1 FC03 "
        "NO-WAIT SATURATION"
    )
    print("=" * 76)

    print(
        f"TARGET={args.host}:"
        f"{args.port}"
    )

    print(
        f"SERIAL={args.serial}"
    )

    print(
        f"DURATION_PER_CASE_S="
        f"{args.duration:.1f}"
    )

    print(
        "MBPS_DEFINITION="
        "TCP_PAYLOAD_ONLY_"
        "NO_ETH_IP_TCP_HEADERS"
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
            print(
                "INITIAL_SERVER_READY=NO"
            )

            q.wait_server_ready(
                ser,
                timeout_s=20.0,
                require_disconnected=True
            )

        print(
            "ACTIVE_SERIAL_HANDSHAKE=PASS"
        )

        for index, quantity in enumerate(
            quantities,
            start=1
        ):
            print()
            print(
                f"CASE_BEGIN="
                f"{index}/{len(quantities)} "
                f"Q={quantity} "
                f"MODE=NO_WAIT"
            )

            row = run_saturation_case(
                ser,
                args.host,
                args.port,
                args.duration,
                quantity
            )

            rows.append(
                row
            )

            print_case(
                row
            )

            time.sleep(0.25)

    finally:
        ser.close()

    fields = list(
        rows[0].keys()
    )

    with open(
        args.csv,
        "w",
        newline="",
        encoding="utf-8"
    ) as f:
        writer = csv.DictWriter(
            f,
            fieldnames=fields
        )

        writer.writeheader()
        writer.writerows(
            rows
        )

    clean_count = sum(
        1
        for r in rows
        if r["clean_case"]
    )

    peak_total = max(
        rows,
        key=lambda r:
            r["total_tcp_payload_mbps"]
    )

    peak_useful = max(
        rows,
        key=lambda r:
            r["useful_register_data_mbps"]
    )

    peak_req = max(
        rows,
        key=lambda r:
            r["req_s"]
    )

    print()
    print("=" * 76)
    print(" SATURATION SUMMARY")
    print("=" * 76)

    print(
        f"CASES_EXECUTED="
        f"{len(rows)}"
    )

    print(
        f"CLEAN_CASES="
        f"{clean_count}"
    )

    print(
        f"PEAK_REQ_S="
        f"{peak_req['req_s']:.1f}"
    )

    print(
        f"PEAK_REQ_S_Q="
        f"{peak_req['quantity']}"
    )

    print(
        f"PEAK_TOTAL_TCP_PAYLOAD_MBPS="
        f"{peak_total['total_tcp_payload_mbps']:.3f}"
    )

    print(
        f"PEAK_TOTAL_TCP_PAYLOAD_Q="
        f"{peak_total['quantity']}"
    )

    print(
        f"PEAK_USEFUL_REGISTER_DATA_MBPS="
        f"{peak_useful['useful_register_data_mbps']:.3f}"
    )

    print(
        f"PEAK_USEFUL_REGISTER_DATA_Q="
        f"{peak_useful['quantity']}"
    )

    print(
        f"CSV={args.csv}"
    )

    print()
    print(
        "A14_3_FC03_NO_WAIT_SATURATION="
        "PASS_EXECUTED"
    )

    print(
        "NOTE=NO_WAIT_VALUES_ARE_"
        "PEAK_MEASUREMENTS_NOT_"
        "FINAL_RECOMMENDED_RATES"
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
            "A14_3_FC03_NO_WAIT_"
            "SATURATION=FAIL_HARNESS"
        )

        sys.exit(2)