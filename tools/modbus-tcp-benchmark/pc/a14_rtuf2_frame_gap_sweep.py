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
    (2000, b"H\n"),
    (1750, b"I\n"),
    (1500, b"J\n"),
    (1250, b"K\n"),
    (1000, b"L\n"),
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
        "RTUF2_GAP_CONFIG "
        f"GAP_US={gap_us} "
        f"MASTER_GAP_US={master_gap} "
        f"SLAVE_GAP_US={slave_gap}"
    )

    if master_gap != gap_us or slave_gap != gap_us:
        raise RuntimeError(
            f"gap efectivo invalido: master={master_gap} "
            f"slave={slave_gap} esperado={gap_us}"
        )


def run_case(
    master,
    slave,
    host: str,
    port: int,
    duration_s: float,
    gap_us: int,
    tcp_target: int | None,
) -> dict[str, object]:
    tcp_label = "OFF" if tcp_target is None else str(tcp_target)

    print()
    print("-" * 78)
    print(
        f"RTUF2_CASE_BEGIN GAP_US={gap_us} "
        f"TCP_TARGET={tcp_label} RTU=UNPACED"
    )
    print("-" * 78)

    budget.send_command_wait(master, b"X\n", p5b.MASTER_STOP_ACK)
    time.sleep(0.10)
    q.wait_server_disconnected(master, timeout_s=20.0)
    budget.send_command_wait(master, b"U\n", "RTU_RATE_MODE=UNPACED")

    q.reset_stats(master)
    p5b.reset_slave_stats(slave, 3.0)
    budget.send_command_wait(master, b"G\n", p5b.MASTER_START_ACK)

    if tcp_target is None:
        t0 = time.perf_counter()
        time.sleep(duration_s)
        elapsed = time.perf_counter() - t0
        tcp = {
            "ok": 0,
            "timeouts": 0,
            "transport_errors": 0,
            "protocol_errors": 0,
            "achieved_req_s": 0.0,
            "target_pct": 100.0,
            "useful_mbps": 0.0,
            "avg_us": 0.0,
            "p95_us": 0.0,
            "p99_us": 0.0,
        }
    else:
        tcp = budget.run_paced_fc03(
            host,
            port,
            duration_s,
            float(tcp_target),
            125,
        )

    budget.send_command_wait(master, b"X\n", p5b.MASTER_STOP_ACK)
    time.sleep(0.10)

    ms = q.request_snapshot(master, echo=False)
    ss = p5b.request_slave_snapshot(slave, 5.0)

    master_gap = iv(ms, "RTU_FRAME_GAP_US")
    slave_gap = iv(ss, "RTU_FRAME_GAP_US")
    timing_pass = master_gap == gap_us and slave_gap == gap_us

    duration_ms = iv(ms, "RTU_TRAFFIC_DURATION_MS")
    started = iv(ms, "RTU_REQUESTS_STARTED")
    rejected = iv(ms, "RTU_REQUESTS_REJECTED")
    completed = iv(ms, "RTU_REQUESTS_COMPLETED")
    success = iv(ms, "RTU_REQUESTS_SUCCESS")
    failed = iv(ms, "RTU_REQUESTS_FAILED")
    verify = iv(ms, "RTU_VERIFY_FAILS")
    rtu_timeouts = iv(ms, "RTU_MASTER_TIMEOUTS")
    rtu_crc = iv(ms, "RTU_CRC_ERRORS")

    rtu_hz = completed / (duration_ms / 1000.0) if duration_ms > 0 else 0.0

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

    if tcp_target is None:
        tcp_clean = True
        tcp_target_pass = True
    else:
        tcp_clean = (
            tcp["timeouts"] == 0
            and tcp["transport_errors"] == 0
            and tcp["protocol_errors"] == 0
            and iv(ms, "FRAME_TIMEOUTS") == 0
            and iv(ms, "BUS_LOCK_TIMEOUTS") == 0
            and iv(ms, "PROTOCOL_ERRORS") == 0
            and iv(ms, "REQUESTS_OK") == int(tcp["ok"])
        )
        tcp_target_pass = float(tcp["target_pct"]) >= 99.0

    peripheral_failures = iv(ms, "PERIPHERAL_FAILURE_COUNT")
    sd_failed = iv(ms, "SD_DATALOG_FAILED_COMMITS")

    runtime_clean = (
        timing_pass
        and tcp_clean
        and rtu_clean
        and peripheral_failures == 0
        and sd_failed == 0
    )

    row = {
        "gap_us": gap_us,
        "tcp_label": tcp_label,
        "tcp_req_s": float(tcp["achieved_req_s"]),
        "tcp_target_pct": float(tcp["target_pct"]),
        "tcp_target_pass": tcp_target_pass,
        "tcp_useful_mbps": float(tcp["useful_mbps"]),
        "tcp_avg_us": float(tcp["avg_us"]),
        "tcp_p95_us": float(tcp["p95_us"]),
        "tcp_p99_us": float(tcp["p99_us"]),
        "rtu_hz": rtu_hz,
        "failed": failed,
        "rtu_timeouts": rtu_timeouts,
        "rtu_crc": rtu_crc,
        "slave_crc": slave_crc,
        "runtime_clean": runtime_clean,
        "rtu_clean": rtu_clean,
    }

    print(
        "RTUF2_CASE "
        f"GAP_US={gap_us} "
        f"TCP_TARGET={tcp_label} "
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
    print(" A14 RTU-F2 - FRAME GAP SWEEP")
    print("=" * 78)
    print(f"MASTER_SERIAL={args.master_serial}")
    print(f"SLAVE_SERIAL={args.slave_serial}")
    print(f"TARGET={args.host}:{args.port}")
    print(f"DURATION_PER_CASE_S={args.duration_per_case:.0f}")
    print("GAPS_US=2000,1750,1500,1250,1000")
    print("TCP_CASES_PER_GAP=500,OFF")
    print("RTU_BAUD=115200")
    print("RTU_CONFIG=8N1")
    print("RTU_MODE=UNPACED")

    try:
        time.sleep(1.0)

        for gap_us, command in GAP_CASES:
            budget.send_command_wait(master, b"X\n", p5b.MASTER_STOP_ACK)
            time.sleep(0.10)
            configure_gap(master, slave, gap_us, command)

            rows.append(
                run_case(
                    master,
                    slave,
                    args.host,
                    args.port,
                    args.duration_per_case,
                    gap_us,
                    500,
                )
            )
            rows.append(
                run_case(
                    master,
                    slave,
                    args.host,
                    args.port,
                    args.duration_per_case,
                    gap_us,
                    None,
                )
            )

        print()
        print("=" * 78)
        print(" RTU-F2 SUMMARY")
        print("=" * 78)

        for row in rows:
            print(
                "RTUF2_SUMMARY "
                f"GAP_US={row['gap_us']} "
                f"TCP_TARGET={row['tcp_label']} "
                f"TCP_REQ_S={row['tcp_req_s']:.3f} "
                f"RTU_HZ={row['rtu_hz']:.3f} "
                f"RUNTIME_CLEAN="
                f"{'YES' if row['runtime_clean'] else 'NO'}"
            )

        baseline500 = next(
            row for row in rows
            if row["gap_us"] == 2000
            and row["tcp_label"] == "500"
        )
        baseline_off = next(
            row for row in rows
            if row["gap_us"] == 2000
            and row["tcp_label"] == "OFF"
        )

        print()
        print(
            "RTUF2_BASELINE_2000_TCP500_HZ="
            f"{baseline500['rtu_hz']:.3f}"
        )
        print(
            "RTUF2_BASELINE_2000_OFF_HZ="
            f"{baseline_off['rtu_hz']:.3f}"
        )

        for gap_us, _ in GAP_CASES:
            r500 = next(
                row for row in rows
                if row["gap_us"] == gap_us
                and row["tcp_label"] == "500"
            )
            roff = next(
                row for row in rows
                if row["gap_us"] == gap_us
                and row["tcp_label"] == "OFF"
            )
            gain500 = (
                (float(r500["rtu_hz"]) /
                 float(baseline500["rtu_hz"]) - 1.0)
                * 100.0
            )
            gainoff = (
                (float(roff["rtu_hz"]) /
                 float(baseline_off["rtu_hz"]) - 1.0)
                * 100.0
            )
            print(
                "RTUF2_GAIN "
                f"GAP_US={gap_us} "
                f"TCP500_GAIN_PCT={gain500:.3f} "
                f"OFF_GAIN_PCT={gainoff:.3f}"
            )

        clean500 = [
            row for row in rows
            if row["tcp_label"] == "500"
            and row["runtime_clean"]
            and row["tcp_target_pass"]
        ]
        cleanoff = [
            row for row in rows
            if row["tcp_label"] == "OFF"
            and row["runtime_clean"]
        ]

        if clean500:
            best500 = max(
                clean500,
                key=lambda row: float(row["rtu_hz"]),
            )
            print(
                "RTUF2_BEST_CLEAN_TCP500_GAP_US="
                f"{best500['gap_us']}"
            )
            print(
                "RTUF2_BEST_CLEAN_TCP500_RTU_HZ="
                f"{best500['rtu_hz']:.3f}"
            )

        if cleanoff:
            bestoff = max(
                cleanoff,
                key=lambda row: float(row["rtu_hz"]),
            )
            print(
                "RTUF2_BEST_CLEAN_OFF_GAP_US="
                f"{bestoff['gap_us']}"
            )
            print(
                "RTUF2_BEST_CLEAN_OFF_RTU_HZ="
                f"{bestoff['rtu_hz']:.3f}"
            )

        standard_rows = [
            row for row in rows
            if row["gap_us"] in (2000, 1750)
        ]

        if not all(bool(row["runtime_clean"]) for row in standard_rows):
            print("A14_RTU_F2=REVIEW_STANDARD_GAP_FAILURE")
            return 2

        standard500 = next(
            row for row in rows
            if row["gap_us"] == 1750
            and row["tcp_label"] == "500"
        )

        if not bool(standard500["tcp_target_pass"]):
            print("A14_RTU_F2=REVIEW_TCP500_AT_1750")
            return 3

        print("A14_RTU_F2=PASS_CHARACTERIZED")
        return 0

    finally:
        master.close()
        slave.close()


if __name__ == "__main__":
    raise SystemExit(main())
