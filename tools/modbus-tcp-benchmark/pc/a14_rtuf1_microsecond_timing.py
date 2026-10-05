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


def iv(values: dict[str, str], key: str) -> int:
    return p5b.int_value(values, key, -1)


def run_case(master, slave, host: str, port: int, duration_s: float, tcp_target: int | None) -> dict[str, object]:
    label = "OFF" if tcp_target is None else str(tcp_target)

    print()
    print("-" * 78)
    print(f"RTUF1_CASE_BEGIN TCP_TARGET={label} RTU=UNPACED")
    print("-" * 78)

    budget.send_command_wait(master, b"X\n", p5b.MASTER_STOP_ACK)
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
        tcp = budget.run_paced_fc03(host, port, duration_s, float(tcp_target), 125)

    budget.send_command_wait(master, b"X\n", p5b.MASTER_STOP_ACK)
    time.sleep(0.10)

    ms = q.request_snapshot(master, echo=False)
    ss = p5b.request_slave_snapshot(slave, 5.0)

    master_gap_us = iv(ms, "RTU_FRAME_GAP_US")
    slave_gap_us = iv(ss, "RTU_FRAME_GAP_US")

    duration_ms = iv(ms, "RTU_TRAFFIC_DURATION_MS")
    started = iv(ms, "RTU_REQUESTS_STARTED")
    rejected = iv(ms, "RTU_REQUESTS_REJECTED")
    completed = iv(ms, "RTU_REQUESTS_COMPLETED")
    success = iv(ms, "RTU_REQUESTS_SUCCESS")
    failed = iv(ms, "RTU_REQUESTS_FAILED")
    verify = iv(ms, "RTU_VERIFY_FAILS")
    timeouts = iv(ms, "RTU_MASTER_TIMEOUTS")
    crc = iv(ms, "RTU_CRC_ERRORS")

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
        and timeouts == 0
        and crc == 0
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

    timing_pass = master_gap_us == 2000 and slave_gap_us == 2000
    runtime_clean = tcp_clean and rtu_clean and timing_pass and peripheral_failures == 0 and sd_failed == 0

    print(
        "RTUF1_CASE "
        f"TCP_TARGET={label} "
        f"TCP_REQ_S={float(tcp['achieved_req_s']):.3f} "
        f"TCP_TARGET_PCT={float(tcp['target_pct']):.3f} "
        f"TCP_TARGET_PASS={'YES' if tcp_target_pass else 'NO'} "
        f"TCP_USEFUL_MBPS={float(tcp['useful_mbps']):.4f} "
        f"TCP_AVG_US={float(tcp['avg_us']):.1f} "
        f"TCP_P95_US={float(tcp['p95_us']):.1f} "
        f"TCP_P99_US={float(tcp['p99_us']):.1f} "
        f"RTU_HZ={rtu_hz:.3f} "
        f"MASTER_GAP_US={master_gap_us} "
        f"SLAVE_GAP_US={slave_gap_us} "
        f"TIMING_PASS={'YES' if timing_pass else 'NO'} "
        f"TCP_CLEAN={'YES' if tcp_clean else 'NO'} "
        f"RTU_CLEAN={'YES' if rtu_clean else 'NO'} "
        f"RUNTIME_CLEAN={'YES' if runtime_clean else 'NO'}"
    )

    return {
        "tcp_target_pass": tcp_target_pass,
        "rtu_hz": rtu_hz,
        "runtime_clean": runtime_clean,
    }


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

    print("=" * 78)
    print(" A14 RTU-F1 - MICROSECOND TIMING")
    print("=" * 78)
    print(f"MASTER_SERIAL={args.master_serial}")
    print(f"SLAVE_SERIAL={args.slave_serial}")
    print(f"TARGET={args.host}:{args.port}")
    print(f"DURATION_PER_CASE_S={args.duration_per_case:.0f}")
    print("TIMING_ENGINE=MICROS")
    print("FRAME_GAP_US=2000")
    print("RTU_BAUD=115200")
    print("RTU_MODE=UNPACED")
    print("TCP_CASES=500,OFF")

    try:
        time.sleep(1.0)
        budget.send_command_wait(slave, b"2\n", "RTU_FRAME_GAP_MS=2")

        tcp500 = run_case(master, slave, args.host, args.port, args.duration_per_case, 500)
        rtu_off = run_case(master, slave, args.host, args.port, args.duration_per_case, None)

        if not bool(tcp500["runtime_clean"]) or not bool(rtu_off["runtime_clean"]):
            print("A14_RTU_F1=FAIL_RUNTIME")
            return 2
        if not bool(tcp500["tcp_target_pass"]):
            print("A14_RTU_F1=REVIEW_TCP500_TARGET")
            return 3
        if float(tcp500["rtu_hz"]) < 180.0:
            print("A14_RTU_F1=REVIEW_RTU500_REGRESSION")
            return 4
        if float(rtu_off["rtu_hz"]) < 195.0:
            print("A14_RTU_F1=REVIEW_RTU_CEILING_REGRESSION")
            return 5

        print()
        print(f"RTUF1_TCP500_RTU_HZ={float(tcp500['rtu_hz']):.3f}")
        print(f"RTUF1_RTU_OFF_HZ={float(rtu_off['rtu_hz']):.3f}")
        print("A14_RTU_F1=PASS_CHARACTERIZED")
        return 0
    finally:
        master.close()
        slave.close()


if __name__ == "__main__":
    raise SystemExit(main())
