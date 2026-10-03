from __future__ import annotations

import argparse
import json
import sys
import time
from pathlib import Path

THIS_DIR = Path(__file__).resolve().parent
sys.path.insert(0, str(THIS_DIR))

import a14_final_tcp_rtu_expmix_matrix as exp
import a14_final_tcp_rtu_frontier_closure as frontier


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--master-serial", default="COM14")
    parser.add_argument("--slave-serial", default="COM4")
    parser.add_argument("--output-root", required=True)
    parser.add_argument("--duration", type=float, default=600.0)
    parser.add_argument("--tcp-target", type=float, default=1000.0)
    parser.add_argument("--rtu-rate", type=int, default=100)
    args = parser.parse_args()

    if args.duration < 600.0:
        raise ValueError("duration debe ser >=600 s")
    if args.tcp_target != 1000.0:
        raise ValueError("este gate final exige TCP target = 1000 req/s")
    if args.rtu_rate != 100:
        raise ValueError("este gate final exige RTU target = 100 req/s")

    root = Path(args.output_root)
    root.mkdir(parents=True, exist_ok=True)

    master = exp.p5b.open_serial_no_dtr(args.master_serial)
    slave = exp.p5b.open_serial_no_dtr(args.slave_serial)

    try:
        time.sleep(1.0)
        host, boot_master, boot_slave = exp.wait_ready(master, slave)
        exp.write_snapshot(root / "master_boot.log", boot_master)
        exp.write_snapshot(root / "slave_boot.log", boot_slave)
        (root / "dut_ip.txt").write_text(host + "\n", encoding="utf-8")

        exp.configure_fast(master, slave)

        fast_master = exp.q.request_snapshot(master, echo=False)
        fast_slave = exp.p5b.request_slave_snapshot(slave, 5.0)
        exp.write_snapshot(root / "master_fast_profile.log", fast_master)
        exp.write_snapshot(root / "slave_fast_profile.log", fast_slave)

        if not exp.fast_profile_pass(fast_master, fast_slave):
            raise RuntimeError("STRICT_CONFIRM_FAST_PROFILE_PREFLIGHT_FAIL")

        print("=" * 78)
        print(" A14 FINAL STRICT CONFIRMATION - TCP1000 + RTU100 EXP-MIX")
        print("=" * 78)
        print(f"DUT_IP={host}")
        print(f"DURATION_S={args.duration:.0f}")
        print(f"TCP_TARGET_REQ_S={args.tcp_target:.0f}")
        print(f"RTU_TARGET_REQ_S={args.rtu_rate}")
        print("RTU_WORKLOAD=2DI_2DO_2AI_2AO")
        print("STRICT_TCP_THRESHOLD_PCT=99.9")
        print("RTU_TARGET_THRESHOLD_PCT=99.0")
        print("RTU_SKIPS_REQUIRED=0")

        case_root = root / "STRICT_CONFIRMATION"
        case_root.mkdir(parents=True, exist_ok=True)

        row = frontier.run_fixed_case(
            case_root,
            master,
            slave,
            host,
            args.rtu_rate,
            args.duration,
            args.tcp_target,
            label_prefix="FINAL_CONFIRM",
        )

    finally:
        master.close()
        slave.close()

    strict_pass = (
        bool(row["runtime_clean"])
        and bool(row["rtu_target_pass"])
        and float(row["tcp_target_pct"]) >= 99.9
        and int(row["rtu_periods_skipped"]) == 0
    )

    status = "PASS_STRICT_CONFIRMED" if strict_pass else "FAIL_STRICT_NOT_CONFIRMED"

    summary = {
        "status": status,
        "tcp_target_req_s": args.tcp_target,
        "rtu_target_req_s": args.rtu_rate,
        "duration_s": args.duration,
        "strict_tcp_threshold_pct": 99.9,
        "rtu_threshold_pct": 99.0,
        "rtu_skips_required": 0,
        "result": row,
    }

    (root / "FINAL_SUMMARY.json").write_text(
        json.dumps(summary, indent=2),
        encoding="utf-8",
    )

    final_lines = [
        f"A14_TCP1000_RTU100_STRICT_CONFIRMATION={status}",
        f"TCP_TARGET_REQ_S={args.tcp_target:.0f}",
        f"TCP_ACHIEVED_REQ_S={float(row['tcp_req_s']):.3f}",
        f"TCP_TARGET_PCT={float(row['tcp_target_pct']):.3f}",
        f"RTU_TARGET_REQ_S={args.rtu_rate}",
        f"RTU_ACHIEVED_REQ_S={float(row['rtu_req_s']):.3f}",
        f"RTU_TARGET_PCT={float(row['rtu_target_pct']):.3f}",
        f"RTU_PERIODS_SKIPPED={int(row['rtu_periods_skipped'])}",
        f"RUNTIME_CLEAN={bool(row['runtime_clean'])}",
        f"CLASSIFICATION={row['classification']}",
    ]
    (root / "FINAL_STATUS.txt").write_text(
        "\n".join(final_lines) + "\n",
        encoding="utf-8",
    )

    print("=" * 78)
    print(" A14 TCP1000 + RTU100 STRICT CONFIRMATION SUMMARY")
    print("=" * 78)
    for line in final_lines:
        print(line)
    print(f"RESULT_ROOT={root}")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
