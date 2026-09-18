import argparse
import csv
import socket
import struct
import sys
import time

import serial


def percentile(values, q):
    if not values:
        return 0.0

    data = sorted(values)

    if len(data) == 1:
        return float(data[0])

    pos = (len(data) - 1) * q
    lo = int(pos)
    hi = min(lo + 1, len(data) - 1)
    frac = pos - lo

    return (
        data[lo] * (1.0 - frac) +
        data[hi] * frac
    )


def recv_exact(sock, count):
    data = bytearray()

    while len(data) < count:
        chunk = sock.recv(count - len(data))

        if not chunk:
            raise ConnectionError(
                f"peer closed at "
                f"{len(data)}/{count}"
            )

        data.extend(chunk)

    return bytes(data)


def read_serial_line(ser):
    raw = ser.readline()

    if not raw:
        return None

    return raw.decode(
        "utf-8",
        errors="replace"
    ).strip()


def collect_snapshot(
    ser,
    timeout_s=5.0,
    echo=False
):
    result = {}

    deadline = (
        time.monotonic() +
        timeout_s
    )

    while time.monotonic() < deadline:
        line = read_serial_line(ser)

        if not line:
            continue

        if echo:
            print(f"SERIAL>{line}")

        if "=" in line:
            key, value = line.split(
                "=",
                1
            )

            result[
                key.strip()
            ] = value.strip()

        if (
            line ==
            "A14_PERF_SNAPSHOT=END"
        ):
            return result

    raise TimeoutError(
        "snapshot JWPLC incompleto"
    )


def request_snapshot(
    ser,
    echo=False
):
    ser.write(b"S\n")
    ser.flush()

    return collect_snapshot(
        ser,
        5.0,
        echo
    )


def wait_reset_ack(
    ser,
    timeout_s=3.0
):
    deadline = (
        time.monotonic() +
        timeout_s
    )

    while time.monotonic() < deadline:
        line = read_serial_line(ser)

        if not line:
            continue

        if (
            line ==
            "A14_PERF_RESET=PASS"
        ):
            return

    raise TimeoutError(
        "reset estadístico sin ACK"
    )


def reset_stats(ser):
    ser.write(b"R\n")
    ser.flush()

    wait_reset_ack(
        ser,
        3.0
    )


def intval(
    snapshot,
    key,
    default=-1
):
    try:
        return int(
            snapshot.get(
                key,
                str(default)
            )
        )
    except ValueError:
        return default


def wait_server_ready(
    ser,
    timeout_s=20.0,
    require_disconnected=False
):
    deadline = (
        time.monotonic() +
        timeout_s
    )

    saw_not_ready = False

    while time.monotonic() < deadline:
        snap = request_snapshot(
            ser,
            echo=False
        )

        ready = (
            snap.get(
                "SERVER_READY"
            ) == "YES"
        )

        disconnected = (
            snap.get(
                "CLIENT_CONNECTED"
            ) == "NO"
        )

        if not ready:
            saw_not_ready = True

        if (
            ready and
            (
                not require_disconnected or
                disconnected
            )
        ):
            if saw_not_ready:
                print(
                    "SERVER_READY_RECOVERY=PASS"
                )

            return snap

        time.sleep(0.25)

    raise TimeoutError(
        "JWPLC Server no recuperó READY "
        f"en {timeout_s:.1f} s"
    )


def wait_server_disconnected(
    ser,
    timeout_s=20.0
):
    return wait_server_ready(
        ser,
        timeout_s=timeout_s,
        require_disconnected=True
    )


def expected_body(quantity):
    body = bytearray()

    body.append(0x03)
    body.append(quantity * 2)

    for i in range(quantity):
        body.extend(
            struct.pack(
                ">H",
                0x1000 + i
            )
        )

    return bytes(body)


def drain_serial_after_case(ser):
    resets = 0

    while ser.in_waiting > 0:
        line = read_serial_line(ser)

        if not line:
            continue

        if (
            line.startswith(
                "A14_PERF_SERVER_CONFIG="
            ) or
            line.startswith(
                "A14_PERF_SERVER_READY="
            )
        ):
            resets += 1

    return resets


def run_case(
    ser,
    host,
    port,
    rate,
    duration,
    quantity
):
    wait_server_disconnected(
        ser
    )

    reset_stats(
        ser
    )

    planned = int(
        round(
            rate * duration
        )
    )

    interval_s = 1.0 / rate

    sock = socket.create_connection(
        (host, port),
        timeout=3.0
    )

    sock.settimeout(1.0)

    expected = expected_body(
        quantity
    )

    latencies = []

    sent = 0
    ok = 0

    timeouts = 0
    transport_errors = 0
    protocol_errors = 0
    unexpected_resets = 0

    tx_bytes = 0
    rx_bytes = 0

    start = time.perf_counter()

    try:
        for i in range(planned):
            scheduled = (
                start +
                i * interval_s
            )

            now = time.perf_counter()

            if scheduled > now:
                time.sleep(
                    scheduled - now
                )

            tid = (
                (i + 1) &
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

            t0 = time.perf_counter_ns()

            try:
                sock.sendall(
                    request
                )

                sent += 1
                tx_bytes += len(
                    request
                )

                header = recv_exact(
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
                        "MBAP length "
                        f"inválido: "
                        f"{length}"
                    )

                body = recv_exact(
                    sock,
                    length - 1
                )

                t1 = (
                    time.perf_counter_ns()
                )

                latencies.append(
                    (t1 - t0) /
                    1000.0
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
                    length ==
                        expected_length and
                    body == expected
                )

                if valid:
                    ok += 1
                else:
                    protocol_errors += 1

            except socket.timeout:
                timeouts += 1

            except (
                ConnectionError,
                OSError
            ):
                transport_errors += 1
                break

            except ValueError:
                protocol_errors += 1

        target_end = (
            start +
            duration
        )

        now = time.perf_counter()

        if target_end > now:
            time.sleep(
                target_end - now
            )

        elapsed = (
            time.perf_counter() -
            start
        )

        unexpected_resets += (
            drain_serial_after_case(
                ser
            )
        )

        ser.write(b"S\n")
        ser.flush()

        snapshot = collect_snapshot(
            ser,
            5.0,
            echo=False
        )

    finally:
        try:
            sock.close()
        except Exception:
            pass

    achieved = (
        ok / elapsed
        if elapsed > 0
        else 0.0
    )

    if latencies:
        latency_min = min(
            latencies
        )

        latency_avg = (
            sum(latencies) /
            len(latencies)
        )

        latency_p50 = percentile(
            latencies,
            0.50
        )

        latency_p95 = percentile(
            latencies,
            0.95
        )

        latency_p99 = percentile(
            latencies,
            0.99
        )

        latency_max = max(
            latencies
        )

    else:
        latency_min = 0.0
        latency_avg = 0.0
        latency_p50 = 0.0
        latency_p95 = 0.0
        latency_p99 = 0.0
        latency_max = 0.0

    app_bytes_s = (
        (tx_bytes + rx_bytes) /
        elapsed
        if elapsed > 0
        else 0.0
    )

    server_connections = intval(
        snapshot,
        "CLIENT_CONNECTIONS"
    )

    server_rx = intval(
        snapshot,
        "RX_FRAMES"
    )

    server_tx = intval(
        snapshot,
        "TX_FRAMES"
    )

    server_ok = intval(
        snapshot,
        "REQUESTS_OK"
    )

    server_ex = intval(
        snapshot,
        "EXCEPTIONS_SENT"
    )

    server_protocol = intval(
        snapshot,
        "PROTOCOL_ERRORS"
    )

    server_timeouts = intval(
        snapshot,
        "FRAME_TIMEOUTS"
    )

    server_bus = intval(
        snapshot,
        "BUS_LOCK_TIMEOUTS"
    )

    loop_avg = intval(
        snapshot,
        "LOOP_GAP_AVG_US"
    )

    loop_max = intval(
        snapshot,
        "LOOP_GAP_MAX_US"
    )

    errors = (
        timeouts +
        transport_errors +
        protocol_errors
    )

    pc_pass = (
        sent == planned and
        ok == planned and
        errors == 0 and
        unexpected_resets == 0 and
        achieved >= (
            rate * 0.95
        )
    )

    server_pass = (
        server_connections == 1 and
        server_rx == planned and
        server_tx == planned and
        server_ok == planned and
        server_ex == 0 and
        server_protocol == 0 and
        server_timeouts == 0 and
        server_bus == 0 and
        snapshot.get(
            "SERVER_READY"
        ) == "YES" and
        snapshot.get(
            "CLIENT_CONNECTED"
        ) == "YES"
    )

    qualification_pass = (
        pc_pass and
        server_pass
    )

    return {
        "quantity": quantity,
        "requested_req_s": rate,
        "duration_s": duration,
        "planned_requests": planned,
        "requests_sent": sent,
        "requests_ok": ok,
        "achieved_req_s": achieved,
        "errors": errors,
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
        "application_bytes_s":
            app_bytes_s,
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
        "qualification_pass":
            qualification_pass,
    }


def print_case(row):
    status = (
        "PASS"
        if row[
            "qualification_pass"
        ]
        else "FAIL"
    )

    print(
        "CASE_RESULT "
        f"Q={row['quantity']} "
        f"RATE="
        f"{row['requested_req_s']:.0f} "
        f"ACHIEVED="
        f"{row['achieved_req_s']:.1f} "
        f"OK="
        f"{row['requests_ok']}/"
        f"{row['planned_requests']} "
        f"ERR={row['errors']} "
        f"P95_US="
        f"{row['latency_p95_us']:.1f} "
        f"P99_US="
        f"{row['latency_p99_us']:.1f} "
        f"MAX_US="
        f"{row['latency_max_us']:.1f} "
        f"LOOP_MAX_US="
        f"{row['loop_gap_max_us']} "
        f"RESULT={status}"
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
        default=5.0
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

    rates = [
        10,
        20,
        50,
        100,
        200,
        500,
        1000
    ]

    print()
    print("=" * 72)
    print(
        " A14.3 PERF-S1 FC03 "
        "QUALIFICATION SWEEP"
    )
    print("=" * 72)

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
        f"TOTAL_CASES="
        f"{len(quantities) * len(rates)}"
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

        initial_probe = request_snapshot(
            ser,
            echo=True
        )

        if (
            initial_probe.get(
                "SERVER_READY"
            ) == "YES"
        ):
            initial = initial_probe
        else:
            print(
                "INITIAL_SERVER_READY=NO"
            )

            print(
                "WAITING_SERVER_READY_RECOVERY=YES"
            )

            initial = wait_server_ready(
                ser,
                timeout_s=20.0,
                require_disconnected=True
            )

        print(
            "ACTIVE_SERIAL_HANDSHAKE=PASS"
        )

        print(
            "SERVER_READY_BEFORE_SWEEP="
            + initial.get(
                "SERVER_READY",
                "MISSING"
            )
        )

        total = (
            len(quantities) *
            len(rates)
        )

        index = 0

        for quantity in quantities:
            for rate in rates:
                index += 1

                print()
                print(
                    f"CASE_BEGIN="
                    f"{index}/{total} "
                    f"Q={quantity} "
                    f"RATE={rate}"
                )

                row = run_case(
                    ser,
                    args.host,
                    args.port,
                    float(rate),
                    args.duration,
                    quantity
                )

                rows.append(
                    row
                )

                print_case(
                    row
                )

                time.sleep(0.15)

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

    pass_count = sum(
        1
        for r in rows
        if r[
            "qualification_pass"
        ]
    )

    fail_count = (
        len(rows) -
        pass_count
    )

    print()
    print("=" * 72)
    print(" QUALIFICATION SUMMARY")
    print("=" * 72)

    print(
        f"CASES_EXECUTED="
        f"{len(rows)}"
    )

    print(
        f"QUALIFICATION_PASS_POINTS="
        f"{pass_count}"
    )

    print(
        f"QUALIFICATION_FAIL_POINTS="
        f"{fail_count}"
    )

    print(
        f"CSV={args.csv}"
    )

    for quantity in quantities:
        qr = [
            r
            for r in rows
            if r["quantity"] ==
                quantity
        ]

        passes = [
            r
            for r in qr
            if r[
                "qualification_pass"
            ]
        ]

        fails = [
            r
            for r in qr
            if not r[
                "qualification_pass"
            ]
        ]

        if passes:
            max_pass = max(
                r[
                    "requested_req_s"
                ]
                for r in passes
            )

            max_pass_text = (
                f"{max_pass:.0f}"
            )
        else:
            max_pass_text = "NONE"

        if fails:
            first_fail = min(
                r[
                    "requested_req_s"
                ]
                for r in fails
            )

            first_fail_text = (
                f"{first_fail:.0f}"
            )
        else:
            first_fail_text = "NONE"

        print(
            f"Q={quantity} "
            f"MAX_SHORT_PASS_REQ_S="
            f"{max_pass_text} "
            f"FIRST_SHORT_FAIL_REQ_S="
            f"{first_fail_text}"
        )

    print()
    print(
        "A14_3_FC03_QUALIFICATION_SWEEP="
        "PASS_EXECUTED"
    )

    print(
        "NOTE=SHORT_5S_POINTS_"
        "ARE_NOT_FINAL_STABLE_RATES"
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
            "A14_3_FC03_"
            "QUALIFICATION_SWEEP="
            "FAIL_HARNESS"
        )

        sys.exit(2)