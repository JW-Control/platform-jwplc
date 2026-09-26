from __future__ import annotations

import argparse
import math
import socket
import struct
import sys
import time
from pathlib import Path

THIS_DIR = Path(__file__).resolve().parent
sys.path.insert(0, str(THIS_DIR))

import a14_perf_fc03_qualification_sweep as q
import a14_p5b_master_slave_qualification as p5b


def run_unpaced(
    host: str,
    port: int,
    duration_s: float,
    quantity: int,
    bucket_seconds: float,
) -> dict[str, object]:
    expected = q.expected_body(quantity)

    sock = socket.create_connection(
        (host, port),
        timeout=3.0,
    )
    sock.settimeout(1.0)

    sent = 0
    ok = 0
    timeouts = 0
    transport_errors = 0
    protocol_errors = 0
    tx_bytes = 0
    rx_bytes = 0
    latencies_us: list[float] = []
    success_elapsed_s: list[float] = []

    start = time.perf_counter()
    deadline = start + duration_s

    try:
        while time.perf_counter() < deadline:
            tid = (sent + 1) & 0xFFFF

            request = struct.pack(
                ">HHHBBHH",
                tid,
                0,
                6,
                1,
                3,
                0,
                quantity,
            )

            t0 = time.perf_counter_ns()

            try:
                sock.sendall(request)
                sent += 1
                tx_bytes += len(request)

                header = q.recv_exact(sock, 7)

                (
                    rx_tid,
                    pid,
                    length,
                    unit,
                ) = struct.unpack(
                    ">HHHB",
                    header,
                )

                if length < 2:
                    raise ValueError(
                        f"MBAP length invalido: {length}"
                    )

                body = q.recv_exact(
                    sock,
                    length - 1,
                )

                t1 = time.perf_counter_ns()
                latencies_us.append(
                    (t1 - t0) / 1000.0
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
                    rx_tid == tid
                    and pid == 0
                    and unit == 1
                    and length == expected_length
                    and body == expected
                )

                if not valid:
                    protocol_errors += 1
                    break

                ok += 1
                success_elapsed_s.append(
                    time.perf_counter() - start
                )

            except socket.timeout:
                timeouts += 1
                break

            except (
                ConnectionError,
                OSError,
            ):
                transport_errors += 1
                break

            except ValueError:
                protocol_errors += 1
                break
    finally:
        elapsed = (
            time.perf_counter() -
            start
        )

        try:
            sock.close()
        except Exception:
            pass

    achieved = (
        ok / elapsed
        if elapsed > 0
        else 0.0
    )

    useful_mbps = (
        ok *
        quantity *
        2 *
        8 /
        elapsed /
        1_000_000.0
        if elapsed > 0
        else 0.0
    )

    total_mbps = (
        (tx_bytes + rx_bytes) *
        8 /
        elapsed /
        1_000_000.0
        if elapsed > 0
        else 0.0
    )

    if latencies_us:
        latency_avg = (
            sum(latencies_us) /
            len(latencies_us)
        )
        latency_p95 = q.percentile(
            latencies_us,
            0.95,
        )
        latency_p99 = q.percentile(
            latencies_us,
            0.99,
        )
        latency_max = max(latencies_us)
    else:
        latency_avg = 0.0
        latency_p95 = 0.0
        latency_p99 = 0.0
        latency_max = 0.0

    buckets: list[dict[str, float | int]] = []

    if bucket_seconds > 0.0 and duration_s > 0.0:
        # Los buckets describen la ventana nominal solicitada, no el pequeño
        # exceso de elapsed causado por el último request iniciado antes del
        # deadline y completado unas fracciones después. Así 600 s / 60 s
        # produce exactamente 10 buckets y 60 s produce exactamente uno.
        bucket_count = max(
            1,
            int(math.ceil(duration_s / bucket_seconds)),
        )

        bucket_ok = [0] * bucket_count

        for success_elapsed in success_elapsed_s:
            index = int(
                success_elapsed // bucket_seconds
            )

            if index >= bucket_count:
                index = bucket_count - 1

            bucket_ok[index] += 1

        for index, count in enumerate(bucket_ok):
            bucket_start_s = index * bucket_seconds
            bucket_end_s = min(
                duration_s,
                (index + 1) * bucket_seconds,
            )
            bucket_duration_s = max(
                0.0,
                bucket_end_s - bucket_start_s,
            )
            bucket_req_s = (
                count / bucket_duration_s
                if bucket_duration_s > 0.0
                else 0.0
            )

            buckets.append(
                {
                    "index": index + 1,
                    "start_s": bucket_start_s,
                    "end_s": bucket_end_s,
                    "ok": count,
                    "req_s": bucket_req_s,
                }
            )

    return {
        "elapsed_s": elapsed,
        "sent": sent,
        "ok": ok,
        "timeouts": timeouts,
        "transport_errors": transport_errors,
        "protocol_errors": protocol_errors,
        "achieved_req_s": achieved,
        "useful_mbps": useful_mbps,
        "total_mbps": total_mbps,
        "latency_avg_us": latency_avg,
        "latency_p95_us": latency_p95,
        "latency_p99_us": latency_p99,
        "latency_max_us": latency_max,
        "buckets": buckets,
    }


def main() -> int:
    parser = argparse.ArgumentParser()

    parser.add_argument(
        "--master-serial",
        default="COM14",
    )
    parser.add_argument(
        "--slave-serial",
        default="COM4",
    )
    parser.add_argument(
        "--host",
        required=True,
    )
    parser.add_argument(
        "--port",
        type=int,
        default=502,
    )
    parser.add_argument(
        "--duration",
        type=float,
        default=60.0,
    )
    parser.add_argument(
        "--quantity",
        type=int,
        default=125,
    )
    parser.add_argument(
        "--bucket-seconds",
        type=float,
        default=60.0,
    )

    args = parser.parse_args()

    if args.duration < 30.0:
        raise ValueError(
            "duration debe ser >= 30 s"
        )

    if not 1 <= args.quantity <= 125:
        raise ValueError(
            "quantity debe estar entre 1 y 125"
        )

    if args.bucket_seconds <= 0.0:
        raise ValueError(
            "bucket-seconds debe ser > 0"
        )

    print("=" * 76)
    print(
        " A14 P5-E1 - FULL RUNTIME FC03/125 UNPACED CEILING"
    )
    print("=" * 76)
    print(f"MASTER_SERIAL={args.master_serial}")
    print(f"SLAVE_SERIAL={args.slave_serial}")
    print(f"TARGET={args.host}:{args.port}")
    print(f"DURATION_S={args.duration:.0f}")
    print(f"BUCKET_SECONDS={args.bucket_seconds:.0f}")
    print(f"FC03_QUANTITY_REGISTERS={args.quantity}")
    print("TCP_PACING=NONE")
    print("TCP_OUTSTANDING_REQUESTS=1")
    print("RTU_TARGET_HZ=50")
    print("RTU_TIMEOUT_MS=25")
    print("PRODUCT_SOURCE_MUTATION=NO")

    master_ser = p5b.open_serial_no_dtr(
        args.master_serial
    )
    slave_ser = p5b.open_serial_no_dtr(
        args.slave_serial
    )

    master_snapshot: dict[str, str] = {}
    slave_snapshot: dict[str, str] = {}

    try:
        time.sleep(1.0)

        initial_slave = p5b.request_slave_snapshot(
            slave_ser,
            5.0,
        )

        if not p5b.slave_initial_pass(
            initial_slave
        ):
            print(
                "P5E1_SLAVE_PREFLIGHT=FAIL"
            )
            return 2

        print(
            "P5E1_SLAVE_PREFLIGHT=PASS"
        )

        q.wait_server_disconnected(
            master_ser,
            20.0,
        )

        p5b.send_master_command(
            master_ser,
            b"X\n",
            p5b.MASTER_STOP_ACK,
            3.0,
        )

        time.sleep(0.10)

        q.reset_stats(
            master_ser
        )

        p5b.reset_slave_stats(
            slave_ser,
            3.0,
        )

        p5b.send_master_command(
            master_ser,
            b"G\n",
            p5b.MASTER_START_ACK,
            3.0,
        )

        print(
            "P5E1_SYNC_RESET=PASS"
        )

        result = run_unpaced(
            args.host,
            args.port,
            args.duration,
            args.quantity,
            args.bucket_seconds,
        )

        p5b.send_master_command(
            master_ser,
            b"X\n",
            p5b.MASTER_STOP_ACK,
            3.0,
        )

        time.sleep(0.10)

        unexpected_resets = (
            q.drain_serial_after_case(
                master_ser
            )
        )

        master_ser.reset_input_buffer()
        master_ser.write(b"S\n")
        master_ser.flush()

        master_snapshot = q.collect_snapshot(
            master_ser,
            5.0,
            echo=False,
        )

        slave_snapshot = (
            p5b.request_slave_snapshot(
                slave_ser,
                5.0,
            )
        )

    finally:
        if master_ser.is_open:
            master_ser.close()

        if slave_ser.is_open:
            slave_ser.close()

    achieved = float(
        result["achieved_req_s"]
    )

    headroom_req_s = (
        achieved -
        1000.0
    )

    headroom_pct = (
        headroom_req_s /
        1000.0 *
        100.0
    )

    if achieved >= 1100.0:
        headroom_class = "COMFORTABLE"
        next_decision = "SKIP_P5E2"
    elif achieved >= 1000.0:
        headroom_class = "POSITIVE"
        next_decision = "SKIP_P5E2"
    else:
        headroom_class = "BELOW_1000"
        next_decision = "REVIEW_P5E2"

    server_connections = q.intval(
        master_snapshot,
        "CLIENT_CONNECTIONS",
    )
    server_rx = q.intval(
        master_snapshot,
        "RX_FRAMES",
    )
    server_tx = q.intval(
        master_snapshot,
        "TX_FRAMES",
    )
    server_ok = q.intval(
        master_snapshot,
        "REQUESTS_OK",
    )
    server_ex = q.intval(
        master_snapshot,
        "EXCEPTIONS_SENT",
    )
    server_protocol = q.intval(
        master_snapshot,
        "PROTOCOL_ERRORS",
    )
    server_timeouts = q.intval(
        master_snapshot,
        "FRAME_TIMEOUTS",
    )
    server_bus = q.intval(
        master_snapshot,
        "BUS_LOCK_TIMEOUTS",
    )

    tcp_clean = (
        int(result["timeouts"]) == 0
        and int(
            result[
                "transport_errors"
            ]
        ) == 0
        and int(
            result[
                "protocol_errors"
            ]
        ) == 0
        and unexpected_resets == 0
        and int(result["sent"])
        == int(result["ok"])
        and server_connections == 1
        and server_rx
        == int(result["ok"])
        and server_tx
        == int(result["ok"])
        and server_ok
        == int(result["ok"])
        and server_ex == 0
        and server_protocol == 0
        and server_timeouts == 0
        and server_bus == 0
    )

    started = p5b.int_value(
        master_snapshot,
        "RTU_REQUESTS_STARTED",
    )
    completed = p5b.int_value(
        master_snapshot,
        "RTU_REQUESTS_COMPLETED",
    )
    success = p5b.int_value(
        master_snapshot,
        "RTU_REQUESTS_SUCCESS",
    )
    failed = p5b.int_value(
        master_snapshot,
        "RTU_REQUESTS_FAILED",
    )
    verify_fails = p5b.int_value(
        master_snapshot,
        "RTU_VERIFY_FAILS",
    )
    master_crc = p5b.int_value(
        master_snapshot,
        "RTU_CRC_ERRORS",
    )
    master_timeouts = p5b.int_value(
        master_snapshot,
        "RTU_MASTER_TIMEOUTS",
    )
    duration_ms = p5b.int_value(
        master_snapshot,
        "RTU_TRAFFIC_DURATION_MS",
    )

    rtu_hz = (
        success /
        (duration_ms / 1000.0)
        if duration_ms > 0
        else 0.0
    )

    min_started = int(
        math.floor(
            args.duration *
            45.0
        )
    )

    slave_rx = p5b.int_value(
        slave_snapshot,
        "RTU_RX_FRAMES",
    )
    slave_tx = p5b.int_value(
        slave_snapshot,
        "RTU_TX_FRAMES",
    )
    slave_ok = p5b.int_value(
        slave_snapshot,
        "RTU_REQUESTS_OK",
    )
    slave_crc = p5b.int_value(
        slave_snapshot,
        "RTU_CRC_ERRORS",
    )
    slave_ex = p5b.int_value(
        slave_snapshot,
        "RTU_EXCEPTIONS_SENT",
    )

    cross_count_pass = (
        slave_rx == success
        and slave_tx == success
        and slave_ok == success
    )

    rtu_pass = (
        master_snapshot.get(
            "RTU_READY"
        ) == "YES"
        and master_snapshot.get(
            "RTU_ROLE"
        ) == "MASTER"
        and p5b.int_value(
            master_snapshot,
            "RTU_TIMEOUT_MS",
        ) == 25
        and started >= min_started
        and completed == started
        and success == completed
        and failed == 0
        and verify_fails == 0
        and master_crc == 0
        and master_timeouts == 0
        and 45.0 <= rtu_hz <= 52.0
        and slave_rx >= min_started
        and slave_crc == 0
        and slave_ex == 0
        and cross_count_pass
    )

    sd_committed = p5b.int_value(
        master_snapshot,
        "SD_DATALOG_COMMITTED_BYTES",
    )
    sd_failed_commits = (
        p5b.int_value(
            master_snapshot,
            "SD_DATALOG_FAILED_COMMITS",
        )
    )
    peripheral_failures = (
        p5b.int_value(
            master_snapshot,
            "PERIPHERAL_FAILURE_COUNT",
        )
    )

    runtime_pass = (
        master_snapshot.get(
            "FULL_RUNTIME_READY"
        ) == "YES"
        and master_snapshot.get(
            "SERVER_READY"
        ) == "YES"
        and master_snapshot.get(
            "ETH_READY"
        ) == "YES"
        and master_snapshot.get(
            "ETH_LINK"
        ) == "UP"
        and master_snapshot.get(
            "SD_READY"
        ) == "YES"
        and master_snapshot.get(
            "SD_WORKLOAD_MODE"
        ) == "BUFFERED_DATALOG"
        and master_snapshot.get(
            "SD_DATALOG_ACTIVE"
        ) == "YES"
        and sd_committed > 0
        and sd_failed_commits == 0
        and peripheral_failures == 0
    )

    characterization_pass = (
        tcp_clean
        and rtu_pass
        and runtime_pass
    )

    print()
    print("=" * 76)
    print(" P5-E1 UNPACED CEILING RESULT")
    print("=" * 76)
    for bucket in result["buckets"]:
        print(
            "P5E1_BUCKET "
            f"INDEX={int(bucket['index'])} "
            f"START_S={float(bucket['start_s']):.0f} "
            f"END_S={float(bucket['end_s']):.0f} "
            f"OK={int(bucket['ok'])} "
            f"REQ_S={float(bucket['req_s']):.3f}"
        )

    print(
        "P5E1_ELAPSED_S="
        f"{float(result['elapsed_s']):.3f}"
    )
    print(
        "P5E1_REQUESTS_SENT="
        f"{int(result['sent'])}"
    )
    print(
        "P5E1_REQUESTS_OK="
        f"{int(result['ok'])}"
    )
    print(
        "P5E1_ACHIEVED_REQ_S="
        f"{achieved:.3f}"
    )
    print(
        "P5E1_USEFUL_MBPS="
        f"{float(result['useful_mbps']):.4f}"
    )
    print(
        "P5E1_TOTAL_MODBUS_MBPS="
        f"{float(result['total_mbps']):.4f}"
    )
    print(
        "P5E1_LATENCY_AVG_US="
        f"{float(result['latency_avg_us']):.1f}"
    )
    print(
        "P5E1_LATENCY_P95_US="
        f"{float(result['latency_p95_us']):.1f}"
    )
    print(
        "P5E1_LATENCY_P99_US="
        f"{float(result['latency_p99_us']):.1f}"
    )
    print(
        "P5E1_LATENCY_MAX_US="
        f"{float(result['latency_max_us']):.1f}"
    )
    print(
        "P5E1_HEADROOM_REQ_S="
        f"{headroom_req_s:.3f}"
    )
    print(
        "P5E1_HEADROOM_PCT="
        f"{headroom_pct:.3f}"
    )
    print(
        f"P5E1_HEADROOM_CLASS={headroom_class}"
    )
    print(
        "P5E1_TCP_CLEAN="
        f"{'YES' if tcp_clean else 'NO'}"
    )
    print(
        "P5E1_RTU_PASS="
        f"{'YES' if rtu_pass else 'NO'}"
    )
    print(
        "P5E1_RTU_ACHIEVED_HZ="
        f"{rtu_hz:.3f}"
    )
    print(
        "P5E1_RTU_CROSS_COUNT_PASS="
        f"{'YES' if cross_count_pass else 'NO'}"
    )
    print(
        "P5E1_SD_COMMITTED_BYTES="
        f"{sd_committed}"
    )
    print(
        "P5E1_SD_FAILED_COMMITS="
        f"{sd_failed_commits}"
    )
    print(
        "P5E1_PERIPHERAL_FAILURE_COUNT="
        f"{peripheral_failures}"
    )
    print(
        "P5E1_RUNTIME_PASS="
        f"{'YES' if runtime_pass else 'NO'}"
    )
    print(
        f"P5E1_NEXT_DECISION={next_decision}"
    )
    print(
        "A14_P5E1_UNPACED_CEILING="
        f"{'PASS_CHARACTERIZED' if characterization_pass else 'FAIL'}"
    )

    return (
        0
        if characterization_pass
        else 2
    )


if __name__ == "__main__":
    raise SystemExit(main())
