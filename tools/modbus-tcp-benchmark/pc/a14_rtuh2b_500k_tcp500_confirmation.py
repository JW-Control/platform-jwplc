from __future__ import annotations

import argparse
import sys
import time
from pathlib import Path

THIS_DIR = Path(__file__).resolve().parent
sys.path.insert(0, str(THIS_DIR))

import a14_perf_fc03_qualification_sweep as q
import a14_p5b_master_slave_qualification as p5b
import a14_p5rtu_tcp_budget_frontier as budget


GAP_CASES = [
    (500, b"O\n"),
    (150, b"3\n"),
    (100, b"4\n"),
    (75, b"T\n"),
    (50, b"!\n"),
]


def iv(values: dict[str, str], key: str) -> int:
    return p5b.int_value(values, key, -1)


def sv(values: dict[str, str], key: str) -> str:
    return values.get(key, "").strip()


def yes(values: dict[str, str], key: str) -> bool:
    return sv(values, key).upper() == "YES"


def configure_500k(master, slave) -> None:
    budget.send_command_wait(
        master,
        b"X\n",
        p5b.MASTER_STOP_ACK,
    )
    time.sleep(0.10)

    ack = "RTU_BAUD_REQUESTED=500000"

    budget.send_command_wait(
        slave,
        b"9\n",
        ack,
    )
    budget.send_command_wait(
        master,
        b"9\n",
        ack,
    )
    time.sleep(0.10)

    ms = q.request_snapshot(master, echo=False)
    ss = p5b.request_slave_snapshot(slave, 5.0)

    checks = {
        "master_requested": iv(ms, "RTU_BAUD") == 500000,
        "slave_requested": iv(ss, "RTU_BAUD") == 500000,
        "master_effective": iv(ms, "RTU_BAUD_EFFECTIVE") == 500000,
        "slave_effective": iv(ss, "RTU_BAUD_EFFECTIVE") == 500000,
        "master_motor": sv(ms, "RTU_MOTOR") == "ASYNC",
        "slave_motor": sv(ss, "RTU_MOTOR") == "ASYNC",
        "master_tx": sv(ms, "RTU_TX_MODE") == "QUEUED",
        "slave_tx": sv(ss, "RTU_TX_MODE") == "QUEUED",
        "master_auto": yes(ms, "RS485_AUTO_DIRECTION"),
        "slave_auto": yes(ss, "RS485_AUTO_DIRECTION"),
        "master_queue": yes(ms, "RS485_QUEUED_TX_SUPPORTED"),
        "slave_queue": yes(ss, "RS485_QUEUED_TX_SUPPORTED"),
        "master_buffer": iv(ms, "RS485_TX_BUFFER_BYTES") >= 257,
        "slave_buffer": iv(ss, "RS485_TX_BUFFER_BYTES") >= 257,
    }

    print(
        "RTUH2B_500K_CONFIG "
        f"MASTER_REQUESTED={iv(ms, 'RTU_BAUD')} "
        f"SLAVE_REQUESTED={iv(ss, 'RTU_BAUD')} "
        f"MASTER_EFFECTIVE={iv(ms, 'RTU_BAUD_EFFECTIVE')} "
        f"SLAVE_EFFECTIVE={iv(ss, 'RTU_BAUD_EFFECTIVE')} "
        f"MASTER_MOTOR={sv(ms, 'RTU_MOTOR')} "
        f"SLAVE_MOTOR={sv(ss, 'RTU_MOTOR')} "
        f"MASTER_TX={sv(ms, 'RTU_TX_MODE')} "
        f"SLAVE_TX={sv(ss, 'RTU_TX_MODE')} "
        f"MASTER_BUFFER={iv(ms, 'RS485_TX_BUFFER_BYTES')} "
        f"SLAVE_BUFFER={iv(ss, 'RS485_TX_BUFFER_BYTES')} "
        f"PROFILE_PASS={'YES' if all(checks.values()) else 'NO'}"
    )

    if not all(checks.values()):
        raise RuntimeError(
            "perfil 500k/ASYNC/QUEUED invalido"
        )


def configure_gap(
    master,
    slave,
    gap_us: int,
    command: bytes,
) -> None:
    ack = f"RTU_FRAME_GAP_US={gap_us}"

    budget.send_command_wait(
        master,
        command,
        ack,
    )
    budget.send_command_wait(
        slave,
        command,
        ack,
    )
    time.sleep(0.05)

    ms = q.request_snapshot(master, echo=False)
    ss = p5b.request_slave_snapshot(slave, 5.0)

    master_gap = iv(ms, "RTU_FRAME_GAP_US")
    slave_gap = iv(ss, "RTU_FRAME_GAP_US")

    print(
        "RTUH2B_GAP_CONFIG "
        f"GAP_US={gap_us} "
        f"MASTER_GAP_US={master_gap} "
        f"SLAVE_GAP_US={slave_gap}"
    )

    if master_gap != gap_us or slave_gap != gap_us:
        raise RuntimeError(
            f"gap efectivo invalido para {gap_us} us"
        )


def run_case(
    master,
    slave,
    host: str,
    port: int,
    duration_s: float,
    gap_us: int,
) -> dict[str, object]:
    print()
    print("-" * 78)
    print(
        "RTUH2B_CASE_BEGIN "
        f"GAP_US={gap_us} "
        "BAUD=500000 TCP_TARGET=500 MOTOR=ASYNC TX=QUEUED RTU=UNPACED"
    )
    print("-" * 78)

    budget.send_command_wait(
        master,
        b"X\n",
        p5b.MASTER_STOP_ACK,
    )
    time.sleep(0.10)

    q.wait_server_disconnected(
        master,
        timeout_s=20.0,
    )

    budget.send_command_wait(
        master,
        b"U\n",
        "RTU_RATE_MODE=UNPACED",
    )

    q.reset_stats(master)
    p5b.reset_slave_stats(slave, 3.0)

    budget.send_command_wait(
        master,
        b"G\n",
        p5b.MASTER_START_ACK,
    )

    tcp = budget.run_paced_fc03(
        host,
        port,
        duration_s,
        500.0,
        125,
    )

    budget.send_command_wait(
        master,
        b"X\n",
        p5b.MASTER_STOP_ACK,
    )
    time.sleep(0.10)

    ms = q.request_snapshot(master, echo=False)
    ss = p5b.request_slave_snapshot(slave, 5.0)

    duration_ms = iv(ms, "RTU_TRAFFIC_DURATION_MS")
    started = iv(ms, "RTU_REQUESTS_STARTED")
    rejected = iv(ms, "RTU_REQUESTS_REJECTED")
    completed = iv(ms, "RTU_REQUESTS_COMPLETED")
    success = iv(ms, "RTU_REQUESTS_SUCCESS")
    failed = iv(ms, "RTU_REQUESTS_FAILED")
    verify = iv(ms, "RTU_VERIFY_FAILS")
    rtu_timeouts = iv(ms, "RTU_MASTER_TIMEOUTS")
    master_crc = iv(ms, "RTU_CRC_ERRORS")

    slave_rx = iv(ss, "RTU_RX_FRAMES")
    slave_tx = iv(ss, "RTU_TX_FRAMES")
    slave_ok = iv(ss, "RTU_REQUESTS_OK")
    slave_crc = iv(ss, "RTU_CRC_ERRORS")

    rtu_hz = (
        completed / (duration_ms / 1000.0)
        if duration_ms > 0
        else 0.0
    )

    profile_pass = (
        iv(ms, "RTU_BAUD") == 500000
        and iv(ss, "RTU_BAUD") == 500000
        and iv(ms, "RTU_BAUD_EFFECTIVE") == 500000
        and iv(ss, "RTU_BAUD_EFFECTIVE") == 500000
        and iv(ms, "RTU_FRAME_GAP_US") == gap_us
        and iv(ss, "RTU_FRAME_GAP_US") == gap_us
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

    peripheral_failures = iv(
        ms,
        "PERIPHERAL_FAILURE_COUNT",
    )
    sd_failed = iv(
        ms,
        "SD_DATALOG_FAILED_COMMITS",
    )

    runtime_clean = (
        profile_pass
        and rtu_clean
        and tcp_clean
        and tcp_target_pass
        and peripheral_failures == 0
        and sd_failed == 0
    )

    row = {
        "gap_us": gap_us,
        "tcp_req_s": float(tcp["achieved_req_s"]),
        "tcp_target_pct": float(tcp["target_pct"]),
        "tcp_useful_mbps": float(tcp["useful_mbps"]),
        "tcp_avg_us": float(tcp["avg_us"]),
        "tcp_p95_us": float(tcp["p95_us"]),
        "tcp_p99_us": float(tcp["p99_us"]),
        "rtu_hz": rtu_hz,
        "failed": failed,
        "timeouts": rtu_timeouts,
        "master_crc": master_crc,
        "slave_crc": slave_crc,
        "profile_pass": profile_pass,
        "tcp_clean": tcp_clean,
        "rtu_clean": rtu_clean,
        "runtime_clean": runtime_clean,
    }

    print(
        "RTUH2B_CASE "
        f"GAP_US={gap_us} "
        f"TCP_REQ_S={row['tcp_req_s']:.3f} "
        f"TCP_TARGET_PCT={row['tcp_target_pct']:.3f} "
        f"TCP_USEFUL_MBPS={row['tcp_useful_mbps']:.4f} "
        f"TCP_AVG_US={row['tcp_avg_us']:.1f} "
        f"TCP_P95_US={row['tcp_p95_us']:.1f} "
        f"TCP_P99_US={row['tcp_p99_us']:.1f} "
        f"RTU_HZ={rtu_hz:.3f} "
        f"RTU_FAILED={failed} "
        f"RTU_TIMEOUTS={rtu_timeouts} "
        f"MASTER_CRC={master_crc} "
        f"SLAVE_CRC={slave_crc} "
        f"PROFILE_PASS={'YES' if profile_pass else 'NO'} "
        f"TCP_CLEAN={'YES' if tcp_clean else 'NO'} "
        f"RTU_CLEAN={'YES' if rtu_clean else 'NO'} "
        f"RUNTIME_CLEAN={'YES' if runtime_clean else 'NO'}"
    )

    return row


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--master-serial", default="COM14")
    parser.add_argument("--slave-serial", default="COM4")
    parser.add_argument("--host", required=True)
    parser.add_argument("--port", type=int, default=502)
    parser.add_argument("--duration-per-case", type=float, default=60.0)
    args = parser.parse_args()

    if args.duration_per_case < 30.0:
        raise ValueError(
            "duration-per-case debe ser >= 30 s"
        )

    master = p5b.open_serial_no_dtr(
        args.master_serial
    )
    slave = p5b.open_serial_no_dtr(
        args.slave_serial
    )

    rows: list[dict[str, object]] = []

    print("=" * 78)
    print(" A14 RTU-H2B - 500K + TCP500 GAP CONFIRMATION")
    print("=" * 78)
    print(f"MASTER_SERIAL={args.master_serial}")
    print(f"SLAVE_SERIAL={args.slave_serial}")
    print(f"TARGET={args.host}:{args.port}")
    print(
        f"DURATION_PER_CASE_S="
        f"{args.duration_per_case:.0f}"
    )
    print("BAUD=500000")
    print("GAPS_US=500,150,100,75,50")
    print("TCP_TARGET_REQ_S=500")
    print("TCP_FC03_QUANTITY=125")
    print("MOTOR=ASYNC")
    print("TX_MODE=QUEUED")
    print("RTU_TIMEOUT_MS=25")

    try:
        time.sleep(1.0)
        configure_500k(master, slave)

        for gap_us, command in GAP_CASES:
            configure_gap(
                master,
                slave,
                gap_us,
                command,
            )

            rows.append(
                run_case(
                    master,
                    slave,
                    args.host,
                    args.port,
                    args.duration_per_case,
                    gap_us,
                )
            )

        print()
        print("=" * 78)
        print(" RTU-H2B SUMMARY")
        print("=" * 78)

        for row in rows:
            print(
                "RTUH2B_SUMMARY "
                f"GAP_US={row['gap_us']} "
                f"TCP_REQ_S={row['tcp_req_s']:.3f} "
                f"TCP_TARGET_PCT={row['tcp_target_pct']:.3f} "
                f"TCP_AVG_US={row['tcp_avg_us']:.1f} "
                f"TCP_P95_US={row['tcp_p95_us']:.1f} "
                f"TCP_P99_US={row['tcp_p99_us']:.1f} "
                f"RTU_HZ={row['rtu_hz']:.3f} "
                f"RUNTIME_CLEAN="
                f"{'YES' if row['runtime_clean'] else 'NO'}"
            )

        baseline = next(
            row for row in rows
            if int(row["gap_us"]) == 500
        )

        if not bool(baseline["runtime_clean"]):
            print(
                "A14_RTU_H2B="
                "REVIEW_500US_CONTROL_FAILURE"
            )
            return 2

        baseline_rtu = float(baseline["rtu_hz"])
        baseline_avg = float(baseline["tcp_avg_us"])
        baseline_p95 = float(baseline["tcp_p95_us"])
        baseline_p99 = float(baseline["tcp_p99_us"])

        for row in rows:
            rtu_gain = (
                (float(row["rtu_hz"]) / baseline_rtu - 1.0)
                * 100.0
            )

            print(
                "RTUH2B_DELTA "
                f"GAP_US={row['gap_us']} "
                f"RTU_GAIN_VS_500US_PCT={rtu_gain:.3f} "
                f"TCP_AVG_DELTA_US="
                f"{float(row['tcp_avg_us']) - baseline_avg:.1f} "
                f"TCP_P95_DELTA_US="
                f"{float(row['tcp_p95_us']) - baseline_p95:.1f} "
                f"TCP_P99_DELTA_US="
                f"{float(row['tcp_p99_us']) - baseline_p99:.1f}"
            )

        clean_rows = [
            row
            for row in rows
            if bool(row["runtime_clean"])
        ]

        lowest = min(
            clean_rows,
            key=lambda row: int(row["gap_us"]),
        )
        fastest = max(
            clean_rows,
            key=lambda row: float(row["rtu_hz"]),
        )

        print(
            "RTUH2B_LOWEST_CLEAN_GAP_US="
            f"{lowest['gap_us']}"
        )
        print(
            "RTUH2B_LOWEST_CLEAN_RTU_HZ="
            f"{lowest['rtu_hz']:.3f}"
        )
        print(
            "RTUH2B_FASTEST_CLEAN_GAP_US="
            f"{fastest['gap_us']}"
        )
        print(
            "RTUH2B_FASTEST_CLEAN_RTU_HZ="
            f"{fastest['rtu_hz']:.3f}"
        )

        candidate_gaps = [
            int(row["gap_us"])
            for row in rows
            if int(row["gap_us"]) in (150, 100, 75, 50)
            and bool(row["runtime_clean"])
        ]

        print(
            "RTUH2B_CLEAN_PRODUCT_CANDIDATES_US="
            + (
                ",".join(str(v) for v in candidate_gaps)
                if candidate_gaps
                else "NONE"
            )
        )

        print("A14_RTU_H2B=PASS_CHARACTERIZED")
        return 0

    finally:
        master.close()
        slave.close()


if __name__ == "__main__":
    raise SystemExit(main())
