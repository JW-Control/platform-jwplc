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
    (600, b"N\n"),
    (500, b"O\n"),
    (300, b"W\n"),
]


def iv(values: dict[str, str], key: str) -> int:
    return p5b.int_value(values, key, -1)


def configure_gap(master, slave, gap_us: int, command: bytes) -> None:
    ack = f"RTU_FRAME_GAP_US={gap_us}"

    budget.send_command_wait(master, command, ack)
    budget.send_command_wait(slave, command, ack)
    time.sleep(0.05)

    ms = q.request_snapshot(master, echo=False)
    ss = p5b.request_slave_snapshot(slave, 5.0)

    master_gap = iv(ms, "RTU_FRAME_GAP_US")
    slave_gap = iv(ss, "RTU_FRAME_GAP_US")

    print(
        "RTUF2C_GAP_CONFIG "
        f"GAP_US={gap_us} "
        f"MASTER_GAP_US={master_gap} "
        f"SLAVE_GAP_US={slave_gap}"
    )

    if master_gap != gap_us or slave_gap != gap_us:
        raise RuntimeError(
            f"gap efectivo invalido: "
            f"master={master_gap} "
            f"slave={slave_gap} "
            f"esperado={gap_us}"
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
        f"RTUF2C_CASE_BEGIN "
        f"GAP_US={gap_us} "
        "TCP_TARGET=500 RTU=UNPACED"
    )
    print("-" * 78)

    budget.send_command_wait(master, b"X\n", p5b.MASTER_STOP_ACK)
    time.sleep(0.10)
    q.wait_server_disconnected(master, timeout_s=20.0)

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

    master_gap = iv(ms, "RTU_FRAME_GAP_US")
    slave_gap = iv(ss, "RTU_FRAME_GAP_US")
    timing_pass = (
        master_gap == gap_us
        and slave_gap == gap_us
    )

    duration_ms = iv(ms, "RTU_TRAFFIC_DURATION_MS")
    started = iv(ms, "RTU_REQUESTS_STARTED")
    rejected = iv(ms, "RTU_REQUESTS_REJECTED")
    completed = iv(ms, "RTU_REQUESTS_COMPLETED")
    success = iv(ms, "RTU_REQUESTS_SUCCESS")
    failed = iv(ms, "RTU_REQUESTS_FAILED")
    verify = iv(ms, "RTU_VERIFY_FAILS")
    rtu_timeouts = iv(ms, "RTU_MASTER_TIMEOUTS")
    rtu_crc = iv(ms, "RTU_CRC_ERRORS")

    rtu_hz = (
        completed / (duration_ms / 1000.0)
        if duration_ms > 0
        else 0.0
    )

    slave_rx = iv(ss, "RTU_RX_FRAMES")
    slave_tx = iv(ss, "RTU_TX_FRAMES")
    slave_ok = iv(ss, "RTU_REQUESTS_OK")
    slave_crc = iv(ss, "RTU_CRC_ERRORS")

    rtu_clean = (
        started == completed == success
        and rejected == 0
        and failed == 0
        and verify == 0
        and rtu_timeouts == 0
        and rtu_crc == 0
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
        timing_pass
        and tcp_clean
        and tcp_target_pass
        and rtu_clean
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
        "tcp_target_pass": tcp_target_pass,
        "rtu_hz": rtu_hz,
        "failed": failed,
        "rtu_timeouts": rtu_timeouts,
        "rtu_crc": rtu_crc,
        "slave_crc": slave_crc,
        "tcp_clean": tcp_clean,
        "rtu_clean": rtu_clean,
        "runtime_clean": runtime_clean,
    }

    print(
        "RTUF2C_CASE "
        f"GAP_US={gap_us} "
        f"TCP_REQ_S={row['tcp_req_s']:.3f} "
        f"TCP_TARGET_PCT={row['tcp_target_pct']:.3f} "
        f"TCP_TARGET_PASS={'YES' if tcp_target_pass else 'NO'} "
        f"TCP_USEFUL_MBPS={row['tcp_useful_mbps']:.4f} "
        f"TCP_AVG_US={row['tcp_avg_us']:.1f} "
        f"TCP_P95_US={row['tcp_p95_us']:.1f} "
        f"TCP_P99_US={row['tcp_p99_us']:.1f} "
        f"RTU_HZ={rtu_hz:.3f} "
        f"RTU_FAILED={failed} "
        f"RTU_TIMEOUTS={rtu_timeouts} "
        f"RTU_CRC={rtu_crc} "
        f"SLAVE_CRC={slave_crc} "
        f"TIMING_PASS={'YES' if timing_pass else 'NO'} "
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
        raise ValueError("duration-per-case debe ser >= 30 s")

    master = p5b.open_serial_no_dtr(args.master_serial)
    slave = p5b.open_serial_no_dtr(args.slave_serial)
    rows: list[dict[str, object]] = []

    print("=" * 78)
    print(" A14 RTU-F2C - TCP500 GAP CONFIRMATION")
    print("=" * 78)
    print(f"MASTER_SERIAL={args.master_serial}")
    print(f"SLAVE_SERIAL={args.slave_serial}")
    print(f"TARGET={args.host}:{args.port}")
    print(f"DURATION_PER_CASE_S={args.duration_per_case:.0f}")
    print("GAPS_US=600,500,300")
    print("TCP_TARGET_REQ_S=500")
    print("TCP_FC03_QUANTITY=125")
    print("RTU_BAUD=115200")
    print("RTU_CONFIG=8N1")
    print("RTU_MODE=UNPACED")

    try:
        time.sleep(1.0)

        for gap_us, command in GAP_CASES:
            budget.send_command_wait(
                master,
                b"X\n",
                p5b.MASTER_STOP_ACK,
            )
            time.sleep(0.10)

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
        print(" RTU-F2C SUMMARY")
        print("=" * 78)

        for row in rows:
            print(
                "RTUF2C_SUMMARY "
                f"GAP_US={row['gap_us']} "
                f"TCP_REQ_S={row['tcp_req_s']:.3f} "
                f"TCP_TARGET_PCT={row['tcp_target_pct']:.3f} "
                f"RTU_HZ={row['rtu_hz']:.3f} "
                f"RUNTIME_CLEAN="
                f"{'YES' if row['runtime_clean'] else 'NO'}"
            )

        if not all(
            bool(row["runtime_clean"])
            for row in rows
        ):
            print("A14_RTU_F2C=REVIEW_CANDIDATE_FAILURE")
            return 2

        best = max(
            rows,
            key=lambda row: float(row["rtu_hz"]),
        )

        print(
            "RTUF2C_BEST_CLEAN_GAP_US="
            f"{best['gap_us']}"
        )
        print(
            "RTUF2C_BEST_CLEAN_RTU_HZ="
            f"{best['rtu_hz']:.3f}"
        )

        row600 = next(
            row for row in rows
            if row["gap_us"] == 600
        )
        row500 = next(
            row for row in rows
            if row["gap_us"] == 500
        )
        row300 = next(
            row for row in rows
            if row["gap_us"] == 300
        )

        print(
            "RTUF2C_500_VS_600_GAIN_PCT="
            f"{((float(row500['rtu_hz']) / float(row600['rtu_hz'])) - 1.0) * 100.0:.3f}"
        )
        print(
            "RTUF2C_300_VS_500_GAIN_PCT="
            f"{((float(row300['rtu_hz']) / float(row500['rtu_hz'])) - 1.0) * 100.0:.3f}"
        )

        print("A14_RTU_F2C=PASS_CHARACTERIZED")
        return 0

    finally:
        master.close()
        slave.close()


if __name__ == "__main__":
    raise SystemExit(main())
