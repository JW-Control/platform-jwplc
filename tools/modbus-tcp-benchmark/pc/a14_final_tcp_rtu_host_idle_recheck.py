from __future__ import annotations

import argparse
import csv
import json
import statistics
import sys
import time
from pathlib import Path

THIS_DIR = Path(__file__).resolve().parent
sys.path.insert(0, str(THIS_DIR))

import a14_final_full_runtime_campaign as cap
import a14_final_tcp_rtu_expmix_matrix as exp
import a14_final_tcp_rtu_frontier_closure as frontier


R7_TARGETS = (None, 900.0, 925.0, 950.0, 975.0, 1000.0)
R8_RATES = (0, 50, 100, 150, 200, 250, 300)


def write_csv(path: Path, rows: list[dict[str, object]]) -> None:
    if not rows:
        return
    with path.open("w", newline="", encoding="utf-8") as handle:
        writer = csv.DictWriter(handle, fieldnames=list(rows[0].keys()))
        writer.writeheader()
        writer.writerows(rows)


def tcp_only_case(
    root: Path,
    label: str,
    master,
    slave,
    host: str,
    duration_s: float,
) -> dict[str, object]:
    return cap.run_case(
        label,
        root,
        master,
        slave,
        host,
        duration_s,
        None,
        "UNPACED",
        "OFF",
    )


def aggregate_rate(rate: int, rows: list[dict[str, object]]) -> dict[str, object]:
    tcp = [float(row["tcp_req_s"]) for row in rows]
    tcp_pct = [float(row["tcp_target_pct"]) for row in rows]
    rtu = [float(row["rtu_req_s"]) for row in rows]
    rtu_pct = [float(row["rtu_target_pct"]) for row in rows]
    skipped = [int(row["rtu_periods_skipped"]) for row in rows]

    mean_tcp = statistics.fmean(tcp)
    stdev_tcp = statistics.pstdev(tcp) if len(tcp) > 1 else 0.0
    cv_tcp = (stdev_tcp / mean_tcp * 100.0) if mean_tcp > 0.0 else 0.0

    return {
        "rtu_target_req_s": rate,
        "runs": len(rows),
        "tcp_mean_req_s": mean_tcp,
        "tcp_min_req_s": min(tcp),
        "tcp_max_req_s": max(tcp),
        "tcp_range_req_s": max(tcp) - min(tcp),
        "tcp_cv_pct": cv_tcp,
        "tcp_min_target_pct": min(tcp_pct),
        "rtu_mean_req_s": statistics.fmean(rtu),
        "rtu_min_target_pct": min(rtu_pct),
        "rtu_skipped_total": sum(skipped),
        "all_runtime_clean": all(bool(row["runtime_clean"]) for row in rows),
        "all_rtu_target_pass": all(bool(row["rtu_target_pass"]) for row in rows),
        "all_tcp_99_0": all(float(row["tcp_target_pct"]) >= 99.0 for row in rows),
        "all_tcp_99_5": all(float(row["tcp_target_pct"]) >= 99.5 for row in rows),
        "all_tcp_99_9": all(float(row["tcp_target_pct"]) >= 99.9 for row in rows),
    }


def strict_rate_ok(summary: dict[str, object]) -> bool:
    return (
        int(summary["rtu_target_req_s"]) > 0
        and bool(summary["all_runtime_clean"])
        and bool(summary["all_rtu_target_pass"])
        and bool(summary["all_tcp_99_9"])
    )


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--master-serial", default="COM14")
    parser.add_argument("--slave-serial", default="COM4")
    parser.add_argument("--output-root", required=True)
    parser.add_argument("--r7-duration", type=float, default=600.0)
    parser.add_argument("--r8-duration", type=float, default=300.0)
    parser.add_argument("--tcp-only-duration", type=float, default=600.0)
    parser.add_argument("--confirm-duration", type=float, default=600.0)
    parser.add_argument("--tcp-target", type=float, default=1000.0)
    args = parser.parse_args()

    if args.r7_duration < 600.0:
        raise ValueError("r7-duration debe ser >=600 s")
    if args.r8_duration < 300.0:
        raise ValueError("r8-duration debe ser >=300 s")
    if args.tcp_only_duration < 600.0:
        raise ValueError("tcp-only-duration debe ser >=600 s")
    if args.confirm_duration < 600.0:
        raise ValueError("confirm-duration debe ser >=600 s")
    if args.tcp_target <= 0.0:
        raise ValueError("tcp-target invalido")

    root = Path(args.output_root)
    root.mkdir(parents=True, exist_ok=True)

    master = exp.p5b.open_serial_no_dtr(args.master_serial)
    slave = exp.p5b.open_serial_no_dtr(args.slave_serial)

    tcp_only_rows: list[dict[str, object]] = []
    r7_rows: list[dict[str, object]] = []
    r8_rows: list[dict[str, object]] = []
    rate_summaries: list[dict[str, object]] = []
    confirmation: dict[str, object] | None = None

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
            raise RuntimeError("IDLE_RECHECK_FAST_PROFILE_PREFLIGHT_FAIL")

        print("=" * 78)
        print(" A14 FINAL HOST-IDLE RECHECK - TCP x RTU EXP-MIX")
        print("=" * 78)
        print(f"DUT_IP={host}")
        print("HOST_POLICY=PC_IDLE_NO_GAME_NO_STREAMING_NO_DOWNLOADS")
        print(f"R7_DURATION_S={args.r7_duration:.0f}")
        print(f"R8_DURATION_S={args.r8_duration:.0f}")
        print(f"TCP_ONLY_DURATION_S={args.tcp_only_duration:.0f}")
        print(f"CONFIRM_DURATION_S={args.confirm_duration:.0f}")
        print(f"TCP_TARGET_REQ_S={args.tcp_target:.0f}")
        print("R7_TARGETS=OFF,900,925,950,975,1000")
        print("R8_REP1=0,50,100,150,200,250,300")
        print("R8_REP2=300,250,200,150,100,50,0")
        print("R8_STRICT_TCP=99.9_PERCENT")
        print("R8_RTU_PASS=99.0_PERCENT_AND_NO_SKIPS")

        tcp_root = root / "TCP_ONLY_CONTROLS"
        tcp_root.mkdir(parents=True, exist_ok=True)

        tcp_only_rows.append(
            tcp_only_case(
                tcp_root,
                "TCP_ONLY_BEFORE",
                master,
                slave,
                host,
                args.tcp_only_duration,
            )
        )

        exp.configure_fast(master, slave)

        r7_root = root / "R7_UNPACED_RECHECK"
        r7_root.mkdir(parents=True, exist_ok=True)
        for target in R7_TARGETS:
            row = exp.run_case(
                r7_root,
                master,
                slave,
                host,
                target,
                args.r7_duration,
            )
            r7_rows.append(row)

        write_csv(root / "R7_UNPACED_RECHECK.csv", r7_rows)

        exp.configure_fast(master, slave)

        r8_root = root / "R8_MATCHED_REPEATS"
        r8_root.mkdir(parents=True, exist_ok=True)

        schedules = (
            ("REP1_ASC", list(R8_RATES)),
            ("REP2_DESC", list(reversed(R8_RATES))),
        )

        for rep_label, rates in schedules:
            rep_root = r8_root / rep_label
            rep_root.mkdir(parents=True, exist_ok=True)
            for rate in rates:
                row = frontier.run_fixed_case(
                    rep_root,
                    master,
                    slave,
                    host,
                    rate,
                    args.r8_duration,
                    args.tcp_target,
                    label_prefix=rep_label,
                )
                row = dict(row)
                row["repeat"] = rep_label
                r8_rows.append(row)

        write_csv(root / "R8_MATCHED_REPEATS.csv", r8_rows)

        for rate in R8_RATES:
            rows = [
                row for row in r8_rows
                if int(row["rtu_target_req_s"]) == rate
            ]
            rate_summaries.append(aggregate_rate(rate, rows))

        write_csv(root / "R8_RATE_SUMMARY.csv", rate_summaries)

        tcp_only_rows.append(
            tcp_only_case(
                tcp_root,
                "TCP_ONLY_AFTER",
                master,
                slave,
                host,
                args.tcp_only_duration,
            )
        )
        write_csv(root / "TCP_ONLY_CONTROLS.csv", tcp_only_rows)

        tcp_only_values = [float(row["tcp_req_s"]) for row in tcp_only_rows]
        tcp_only_mean = statistics.fmean(tcp_only_values)
        tcp_only_range = max(tcp_only_values) - min(tcp_only_values)

        rate0 = next(
            summary for summary in rate_summaries
            if int(summary["rtu_target_req_s"]) == 0
        )

        host_control = {
            "tcp_only_before_req_s": tcp_only_values[0],
            "tcp_only_after_req_s": tcp_only_values[1],
            "tcp_only_mean_req_s": tcp_only_mean,
            "tcp_only_range_req_s": tcp_only_range,
            "rtu0_tcp_mean_req_s": float(rate0["tcp_mean_req_s"]),
            "rtu0_tcp_range_req_s": float(rate0["tcp_range_req_s"]),
            "rtu0_tcp_cv_pct": float(rate0["tcp_cv_pct"]),
            "tcp_only_min_980": min(tcp_only_values) >= 980.0,
            "tcp_only_range_le_30": tcp_only_range <= 30.0,
            "rtu0_min_990": float(rate0["tcp_min_req_s"]) >= 990.0,
            "rtu0_range_le_30": float(rate0["tcp_range_req_s"]) <= 30.0,
            "rtu0_cv_le_1_5": float(rate0["tcp_cv_pct"]) <= 1.5,
        }
        host_control["host_idle_baseline_pass"] = all(
            bool(host_control[key])
            for key in (
                "tcp_only_min_980",
                "tcp_only_range_le_30",
                "rtu0_min_990",
                "rtu0_range_le_30",
                "rtu0_cv_le_1_5",
            )
        )

        (root / "HOST_IDLE_CONTROL.json").write_text(
            json.dumps(host_control, indent=2),
            encoding="utf-8",
        )

        strict_candidates = [
            summary for summary in rate_summaries
            if strict_rate_ok(summary)
        ]
        selected = (
            max(strict_candidates, key=lambda row: int(row["rtu_target_req_s"]))
            if strict_candidates
            else None
        )

        if bool(host_control["host_idle_baseline_pass"]) and selected is not None:
            confirm_rate = int(selected["rtu_target_req_s"])
            confirm_root = root / "R8_CONFIRMATION"
            confirm_root.mkdir(parents=True, exist_ok=True)
            confirmation = frontier.run_fixed_case(
                confirm_root,
                master,
                slave,
                host,
                confirm_rate,
                args.confirm_duration,
                args.tcp_target,
                label_prefix="CONFIRM_IDLE",
            )

    finally:
        master.close()
        slave.close()

    host_pass = bool(host_control["host_idle_baseline_pass"])
    selected_rate = (
        None if selected is None else int(selected["rtu_target_req_s"])
    )
    confirm_pass = (
        confirmation is not None
        and bool(confirmation["runtime_clean"])
        and bool(confirmation["rtu_target_pass"])
        and float(confirmation["tcp_target_pct"]) >= 99.9
    )

    r7_valid = [
        row for row in r7_rows
        if float(row["tcp_target_req_s"]) > 0.0
        and row["classification"] == "PASS"
    ]
    r7_best_target = (
        None
        if not r7_valid
        else max(float(row["tcp_target_req_s"]) for row in r7_valid)
    )

    if not host_pass:
        status = "REVIEW_HOST_OR_HARNESS_BASELINE_NOT_STABLE"
    elif selected is None:
        status = "CHARACTERIZED_NO_STRICT_POSITIVE_RTU_AT_TCP1000"
    elif confirm_pass:
        status = "PASS_STRICT_REPRODUCED_AND_CONFIRMED"
    else:
        status = "REVIEW_STRICT_CANDIDATE_CONFIRMATION_FAILED"

    summary = {
        "status": status,
        "host_idle_control": host_control,
        "r7_best_tcp_target_with_unpaced_rtu": r7_best_target,
        "r7_unpaced_recheck": r7_rows,
        "r8_rate_summary": rate_summaries,
        "r8_matched_repeats": r8_rows,
        "selected_strict_rtu_rate": selected_rate,
        "confirmation": confirmation,
        "tcp_only_controls": tcp_only_rows,
    }
    (root / "FINAL_SUMMARY.json").write_text(
        json.dumps(summary, indent=2),
        encoding="utf-8",
    )

    final_lines = [
        f"A14_HOST_IDLE_RECHECK={status}",
        f"HOST_IDLE_BASELINE_PASS={host_pass}",
        f"R7_BEST_TCP_TARGET_WITH_UNPACED_RTU={r7_best_target}",
        f"TCP1000_STRICT_RTU_SELECTED={selected_rate}",
        f"STRICT_CONFIRM_PASS={confirm_pass}",
        f"TCP_ONLY_BEFORE={tcp_only_values[0]:.3f}",
        f"TCP_ONLY_AFTER={tcp_only_values[1]:.3f}",
        f"R8_RTU0_TCP_MEAN={float(rate0['tcp_mean_req_s']):.3f}",
        f"R8_RTU0_TCP_RANGE={float(rate0['tcp_range_req_s']):.3f}",
    ]
    (root / "FINAL_STATUS.txt").write_text(
        "\n".join(final_lines) + "\n",
        encoding="utf-8",
    )

    print("=" * 78)
    print(" A14 HOST-IDLE RECHECK SUMMARY")
    print("=" * 78)
    for line in final_lines:
        print(line)
    print(f"RESULT_ROOT={root}")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
