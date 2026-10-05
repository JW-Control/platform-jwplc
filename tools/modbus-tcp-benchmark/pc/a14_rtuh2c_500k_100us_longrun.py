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
import a14_p5rtu_tcp_budget_frontier as budget


def iv(values: dict[str, str], key: str) -> int:
    return p5b.int_value(values, key, -1)


def sv(values: dict[str, str], key: str) -> str:
    return values.get(key, "").strip()


def yes(values: dict[str, str], key: str) -> bool:
    return sv(values, key).upper() == "YES"


def configure_profile(master, slave) -> None:
    budget.send_command_wait(
        master,
        b"X\n",
        p5b.MASTER_STOP_ACK,
    )
    time.sleep(0.10)

    budget.send_command_wait(
        slave,
        b"9\n",
        "RTU_BAUD_REQUESTED=500000",
    )
    budget.send_command_wait(
        master,
        b"9\n",
        "RTU_BAUD_REQUESTED=500000",
    )

    budget.send_command_wait(
        master,
        b"4\n",
        "RTU_FRAME_GAP_US=100",
    )
    budget.send_command_wait(
        slave,
        b"4\n",
        "RTU_FRAME_GAP_US=100",
    )

    budget.send_command_wait(
        master,
        b"U\n",
        "RTU_RATE_MODE=UNPACED",
    )

    time.sleep(0.10)

    ms = q.request_snapshot(master, echo=False)
    ss = p5b.request_slave_snapshot(slave, 5.0)

    checks = {
        "master_requested": iv(ms, "RTU_BAUD") == 500000,
        "slave_requested": iv(ss, "RTU_BAUD") == 500000,
        "master_effective": iv(ms, "RTU_BAUD_EFFECTIVE") == 500000,
        "slave_effective": iv(ss, "RTU_BAUD_EFFECTIVE") == 500000,
        "master_gap": iv(ms, "RTU_FRAME_GAP_US") == 100,
        "slave_gap": iv(ss, "RTU_FRAME_GAP_US") == 100,
        "master_motor": sv(ms, "RTU_MOTOR") == "ASYNC",
        "slave_motor": sv(ss, "RTU_MOTOR") == "ASYNC",
        "master_tx": sv(ms, "RTU_TX_MODE") == "QUEUED",
        "slave_tx": sv(ss, "RTU_TX_MODE") == "QUEUED",
        "master_auto": yes(ms, "RS485_AUTO_DIRECTION"),
        "slave_auto": yes(ss, "RS485_AUTO_DIRECTION"),
        "master_queue": yes(ms, "RS485_QUEUED_TX_SUPPORTED"),
        "slave_queue": yes(ss, "RS485_QUEUED_TX_SUPPORTED"),
    }

    print(
        "RTUH2C_PROFILE "
        f"MASTER_BAUD={iv(ms, 'RTU_BAUD')} "
        f"SLAVE_BAUD={iv(ss, 'RTU_BAUD')} "
        f"MASTER_EFFECTIVE={iv(ms, 'RTU_BAUD_EFFECTIVE')} "
        f"SLAVE_EFFECTIVE={iv(ss, 'RTU_BAUD_EFFECTIVE')} "
        f"MASTER_GAP_US={iv(ms, 'RTU_FRAME_GAP_US')} "
        f"SLAVE_GAP_US={iv(ss, 'RTU_FRAME_GAP_US')} "
        f"MASTER_MOTOR={sv(ms, 'RTU_MOTOR')} "
        f"SLAVE_MOTOR={sv(ss, 'RTU_MOTOR')} "
        f"MASTER_TX={sv(ms, 'RTU_TX_MODE')} "
        f"SLAVE_TX={sv(ss, 'RTU_TX_MODE')} "
        f"PROFILE_PASS={'YES' if all(checks.values()) else 'NO'}"
    )

    if not all(checks.values()):
        raise RuntimeError(
            "perfil H2C 500k/100us/ASYNC/QUEUED invalido"
        )


def run_paced_fc03_bucketed(
    host: str,
    port: int,
    duration_s: float,
    target_req_s: float,
    quantity: int,
    bucket_seconds: float,
) -> dict[str, object]:
    expected = q.expected_body(quantity)
    interval_s = 1.0 / target_req_s
    bucket_count = int(
        math.ceil(duration_s / bucket_seconds)
    )

    bucket_ok = [0] * bucket_count
    bucket_latencies: list[list[float]] = [
        [] for _ in range(bucket_count)
    ]

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

    start = time.perf_counter()
    deadline = start + duration_s
    next_release = start
    next_progress_bucket = 1

    try:
        while True:
            now = time.perf_counter()

            if now >= deadline:
                break

            while (
                next_progress_bucket < bucket_count
                and now - start >=
                    next_progress_bucket * bucket_seconds
            ):
                idx = next_progress_bucket - 1
                print(
                    "RTUH2C_PROGRESS "
                    f"BUCKET={next_progress_bucket} "
                    f"OK={bucket_ok[idx]} "
                    f"REQ_S={bucket_ok[idx] / bucket_seconds:.3f}"
                )
                next_progress_bucket += 1

            if now < next_release:
                remaining = next_release - now

                if remaining > 0.0015:
                    time.sleep(
                        remaining - 0.0008
                    )

                while time.perf_counter() < next_release:
                    pass

            if time.perf_counter() >= deadline:
                break

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

                header = q.recv_exact(
                    sock,
                    7,
                )

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
                latency_us = (
                    t1 - t0
                ) / 1000.0

                rx_bytes += (
                    len(header) +
                    len(body)
                )

                valid = (
                    rx_tid == tid
                    and pid == 0
                    and unit == 1
                    and length == (3 + 2 * quantity)
                    and body == expected
                )

                if not valid:
                    protocol_errors += 1
                    break

                ok += 1
                latencies_us.append(
                    latency_us
                )

                success_elapsed = (
                    time.perf_counter() -
                    start
                )
                bucket_index = int(
                    success_elapsed //
                    bucket_seconds
                )

                if bucket_index >= bucket_count:
                    bucket_index = (
                        bucket_count - 1
                    )

                bucket_ok[bucket_index] += 1
                bucket_latencies[
                    bucket_index
                ].append(
                    latency_us
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

            next_release += interval_s

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
        if elapsed > 0.0
        else 0.0
    )

    target_pct = (
        achieved / target_req_s * 100.0
        if target_req_s > 0.0
        else 0.0
    )

    useful_mbps = (
        ok *
        quantity *
        2 *
        8 /
        elapsed /
        1_000_000.0
        if elapsed > 0.0
        else 0.0
    )

    total_mbps = (
        (tx_bytes + rx_bytes) *
        8 /
        elapsed /
        1_000_000.0
        if elapsed > 0.0
        else 0.0
    )

    if latencies_us:
        avg_us = (
            sum(latencies_us) /
            len(latencies_us)
        )
        p95_us = q.percentile(
            latencies_us,
            0.95,
        )
        p99_us = q.percentile(
            latencies_us,
            0.99,
        )
        max_us = max(
            latencies_us
        )
    else:
        avg_us = 0.0
        p95_us = 0.0
        p99_us = 0.0
        max_us = 0.0

    buckets: list[dict[str, float | int]] = []

    for index in range(bucket_count):
        start_s = (
            index *
            bucket_seconds
        )
        end_s = min(
            duration_s,
            (index + 1) *
            bucket_seconds,
        )
        bucket_duration = (
            end_s - start_s
        )
        count = bucket_ok[index]
        lats = bucket_latencies[index]

        req_s = (
            count / bucket_duration
            if bucket_duration > 0.0
            else 0.0
        )

        if lats:
            bucket_avg = (
                sum(lats) /
                len(lats)
            )
            bucket_p95 = q.percentile(
                lats,
                0.95,
            )
            bucket_p99 = q.percentile(
                lats,
                0.99,
            )
        else:
            bucket_avg = 0.0
            bucket_p95 = 0.0
            bucket_p99 = 0.0

        buckets.append(
            {
                "index": index + 1,
                "start_s": start_s,
                "end_s": end_s,
                "ok": count,
                "req_s": req_s,
                "avg_us": bucket_avg,
                "p95_us": bucket_p95,
                "p99_us": bucket_p99,
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
        "target_pct": target_pct,
        "useful_mbps": useful_mbps,
        "total_mbps": total_mbps,
        "avg_us": avg_us,
        "p95_us": p95_us,
        "p99_us": p99_us,
        "max_us": max_us,
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
        default=600.0,
    )
    parser.add_argument(
        "--bucket-seconds",
        type=float,
        default=60.0,
    )
    args = parser.parse_args()

    if args.duration < 600.0:
        raise ValueError(
            "duration debe ser >= 600 s"
        )

    if args.bucket_seconds <= 0.0:
        raise ValueError(
            "bucket-seconds debe ser > 0"
        )

    master = p5b.open_serial_no_dtr(
        args.master_serial
    )
    slave = p5b.open_serial_no_dtr(
        args.slave_serial
    )

    print("=" * 78)
    print(" A14 RTU-H2C - 500K / 100US / TCP500 LONG-RUN")
    print("=" * 78)
    print(f"MASTER_SERIAL={args.master_serial}")
    print(f"SLAVE_SERIAL={args.slave_serial}")
    print(f"TARGET={args.host}:{args.port}")
    print(f"DURATION_S={args.duration:.0f}")
    print(
        f"BUCKET_SECONDS="
        f"{args.bucket_seconds:.0f}"
    )
    print("BAUD=500000")
    print("FRAME_GAP_US=100")
    print("MOTOR=ASYNC")
    print("TX_MODE=QUEUED")
    print("TCP_TARGET_REQ_S=500")
    print("TCP_FC03_QUANTITY=125")
    print("RTU_MODE=UNPACED")
    print("RTU_TIMEOUT_MS=25")

    try:
        time.sleep(1.0)
        configure_profile(
            master,
            slave,
        )

        q.wait_server_disconnected(
            master,
            timeout_s=20.0,
        )

        q.reset_stats(master)
        p5b.reset_slave_stats(
            slave,
            3.0,
        )

        budget.send_command_wait(
            master,
            b"G\n",
            p5b.MASTER_START_ACK,
        )

        print(
            "RTUH2C_LONGRUN_BEGIN=YES"
        )

        tcp = run_paced_fc03_bucketed(
            args.host,
            args.port,
            args.duration,
            500.0,
            125,
            args.bucket_seconds,
        )

        budget.send_command_wait(
            master,
            b"X\n",
            p5b.MASTER_STOP_ACK,
        )
        time.sleep(0.10)

        ms = q.request_snapshot(
            master,
            echo=False,
        )
        ss = p5b.request_slave_snapshot(
            slave,
            5.0,
        )

        duration_ms = iv(
            ms,
            "RTU_TRAFFIC_DURATION_MS",
        )
        started = iv(
            ms,
            "RTU_REQUESTS_STARTED",
        )
        rejected = iv(
            ms,
            "RTU_REQUESTS_REJECTED",
        )
        completed = iv(
            ms,
            "RTU_REQUESTS_COMPLETED",
        )
        success = iv(
            ms,
            "RTU_REQUESTS_SUCCESS",
        )
        failed = iv(
            ms,
            "RTU_REQUESTS_FAILED",
        )
        verify = iv(
            ms,
            "RTU_VERIFY_FAILS",
        )
        rtu_timeouts = iv(
            ms,
            "RTU_MASTER_TIMEOUTS",
        )
        master_crc = iv(
            ms,
            "RTU_CRC_ERRORS",
        )

        slave_rx = iv(
            ss,
            "RTU_RX_FRAMES",
        )
        slave_tx = iv(
            ss,
            "RTU_TX_FRAMES",
        )
        slave_ok = iv(
            ss,
            "RTU_REQUESTS_OK",
        )
        slave_crc = iv(
            ss,
            "RTU_CRC_ERRORS",
        )

        rtu_hz = (
            completed /
            (duration_ms / 1000.0)
            if duration_ms > 0
            else 0.0
        )

        profile_pass = (
            iv(ms, "RTU_BAUD") == 500000
            and iv(ss, "RTU_BAUD") == 500000
            and iv(ms, "RTU_BAUD_EFFECTIVE") == 500000
            and iv(ss, "RTU_BAUD_EFFECTIVE") == 500000
            and iv(ms, "RTU_FRAME_GAP_US") == 100
            and iv(ss, "RTU_FRAME_GAP_US") == 100
            and sv(ms, "RTU_MOTOR") == "ASYNC"
            and sv(ss, "RTU_MOTOR") == "ASYNC"
            and sv(ms, "RTU_TX_MODE") == "QUEUED"
            and sv(ss, "RTU_TX_MODE") == "QUEUED"
        )

        rtu_clean = (
            started == completed == success
            and rejected == 0
            and failed == 0
            and verify == 0
            and rtu_timeouts == 0
            and master_crc == 0
            and slave_crc == 0
            and slave_rx == completed
            and slave_tx == completed
            and slave_ok == completed
        )

        tcp_clean = (
            tcp["timeouts"] == 0
            and tcp["transport_errors"] == 0
            and tcp["protocol_errors"] == 0
            and iv(ms, "FRAME_TIMEOUTS") == 0
            and iv(ms, "BUS_LOCK_TIMEOUTS") == 0
            and iv(ms, "PROTOCOL_ERRORS") == 0
            and iv(ms, "REQUESTS_OK") == int(tcp["ok"])
        )

        tcp_target_pass = (
            float(tcp["target_pct"]) >= 99.0
        )

        buckets = list(
            tcp["buckets"]
        )

        bucket_target_pass = all(
            float(bucket["req_s"]) >= 495.0
            for bucket in buckets
        )

        bucket_rates = [
            float(bucket["req_s"])
            for bucket in buckets
        ]

        bucket_min = min(
            bucket_rates
        )
        bucket_max = max(
            bucket_rates
        )

        half = len(
            bucket_rates
        ) // 2

        first_half = (
            bucket_rates[:half]
        )
        second_half = (
            bucket_rates[half:]
        )

        first_half_avg = (
            sum(first_half) /
            len(first_half)
        )
        second_half_avg = (
            sum(second_half) /
            len(second_half)
        )

        half_drift_pct = (
            (
                second_half_avg -
                first_half_avg
            ) /
            first_half_avg *
            100.0
            if first_half_avg > 0.0
            else 0.0
        )

        sd_active = yes(
            ms,
            "SD_DATALOG_ACTIVE",
        )
        sd_pending = iv(
            ms,
            "SD_DATALOG_PENDING_BYTES",
        )
        sd_capacity = iv(
            ms,
            "SD_DATALOG_BUFFER_BYTES",
        )
        sd_accepted = iv(
            ms,
            "SD_DATALOG_ACCEPTED_BYTES",
        )
        sd_committed = iv(
            ms,
            "SD_DATALOG_COMMITTED_BYTES",
        )
        sd_failed = iv(
            ms,
            "SD_DATALOG_FAILED_COMMITS",
        )
        sd_append_fails = iv(
            ms,
            "SD_APPEND_FAILS",
        )
        peripheral_failures = iv(
            ms,
            "PERIPHERAL_FAILURE_COUNT",
        )

        sd_clean = (
            sd_active
            and sd_capacity > 0
            and 0 <= sd_pending < sd_capacity
            and sd_accepted > 0
            and sd_committed > 0
            and sd_failed == 0
            and sd_append_fails == 0
        )

        runtime_clean = (
            profile_pass
            and rtu_clean
            and tcp_clean
            and tcp_target_pass
            and bucket_target_pass
            and sd_clean
            and peripheral_failures == 0
        )

        print()
        print("=" * 78)
        print(" RTU-H2C LONG-RUN SUMMARY")
        print("=" * 78)

        for bucket in buckets:
            print(
                "RTUH2C_BUCKET "
                f"INDEX={bucket['index']} "
                f"START_S={bucket['start_s']:.0f} "
                f"END_S={bucket['end_s']:.0f} "
                f"OK={bucket['ok']} "
                f"REQ_S={bucket['req_s']:.3f} "
                f"AVG_US={bucket['avg_us']:.1f} "
                f"P95_US={bucket['p95_us']:.1f} "
                f"P99_US={bucket['p99_us']:.1f}"
            )

        print(
            "RTUH2C_TCP_REQ_S="
            f"{float(tcp['achieved_req_s']):.3f}"
        )
        print(
            "RTUH2C_TCP_TARGET_PCT="
            f"{float(tcp['target_pct']):.3f}"
        )
        print(
            "RTUH2C_TCP_USEFUL_MBPS="
            f"{float(tcp['useful_mbps']):.4f}"
        )
        print(
            "RTUH2C_TCP_TOTAL_MBPS="
            f"{float(tcp['total_mbps']):.4f}"
        )
        print(
            "RTUH2C_TCP_AVG_US="
            f"{float(tcp['avg_us']):.1f}"
        )
        print(
            "RTUH2C_TCP_P95_US="
            f"{float(tcp['p95_us']):.1f}"
        )
        print(
            "RTUH2C_TCP_P99_US="
            f"{float(tcp['p99_us']):.1f}"
        )
        print(
            "RTUH2C_TCP_MAX_US="
            f"{float(tcp['max_us']):.1f}"
        )
        print(
            "RTUH2C_TCP_BUCKET_MIN_REQ_S="
            f"{bucket_min:.3f}"
        )
        print(
            "RTUH2C_TCP_BUCKET_MAX_REQ_S="
            f"{bucket_max:.3f}"
        )
        print(
            "RTUH2C_TCP_FIRST_HALF_AVG_REQ_S="
            f"{first_half_avg:.3f}"
        )
        print(
            "RTUH2C_TCP_SECOND_HALF_AVG_REQ_S="
            f"{second_half_avg:.3f}"
        )
        print(
            "RTUH2C_TCP_HALF_DRIFT_PCT="
            f"{half_drift_pct:.3f}"
        )

        print(
            "RTUH2C_RTU_HZ="
            f"{rtu_hz:.3f}"
        )
        print(
            "RTUH2C_RTU_STARTED="
            f"{started}"
        )
        print(
            "RTUH2C_RTU_COMPLETED="
            f"{completed}"
        )
        print(
            "RTUH2C_RTU_SUCCESS="
            f"{success}"
        )
        print(
            "RTUH2C_RTU_FAILED="
            f"{failed}"
        )
        print(
            "RTUH2C_RTU_TIMEOUTS="
            f"{rtu_timeouts}"
        )
        print(
            "RTUH2C_MASTER_CRC="
            f"{master_crc}"
        )
        print(
            "RTUH2C_SLAVE_CRC="
            f"{slave_crc}"
        )

        print(
            "RTUH2C_SD_ACTIVE="
            f"{'YES' if sd_active else 'NO'}"
        )
        print(
            "RTUH2C_SD_ACCEPTED_BYTES="
            f"{sd_accepted}"
        )
        print(
            "RTUH2C_SD_COMMITTED_BYTES="
            f"{sd_committed}"
        )
        print(
            "RTUH2C_SD_PENDING_BYTES="
            f"{sd_pending}"
        )
        print(
            "RTUH2C_SD_FAILED_COMMITS="
            f"{sd_failed}"
        )
        print(
            "RTUH2C_PERIPHERAL_FAILURE_COUNT="
            f"{peripheral_failures}"
        )

        print(
            "RTUH2C_PROFILE_PASS="
            f"{'YES' if profile_pass else 'NO'}"
        )
        print(
            "RTUH2C_TCP_CLEAN="
            f"{'YES' if tcp_clean else 'NO'}"
        )
        print(
            "RTUH2C_TCP_TARGET_PASS="
            f"{'YES' if tcp_target_pass else 'NO'}"
        )
        print(
            "RTUH2C_BUCKET_TARGET_PASS="
            f"{'YES' if bucket_target_pass else 'NO'}"
        )
        print(
            "RTUH2C_RTU_CLEAN="
            f"{'YES' if rtu_clean else 'NO'}"
        )
        print(
            "RTUH2C_SD_CLEAN="
            f"{'YES' if sd_clean else 'NO'}"
        )
        print(
            "RTUH2C_RUNTIME_CLEAN="
            f"{'YES' if runtime_clean else 'NO'}"
        )

        if not runtime_clean:
            print(
                "A14_RTU_H2C="
                "REVIEW_LONGRUN_FAILURE"
            )
            return 2

        print(
            "A14_RTU_H2C="
            "PASS_LONGRUN_600S"
        )
        return 0

    finally:
        master.close()
        slave.close()


if __name__ == "__main__":
    raise SystemExit(main())
