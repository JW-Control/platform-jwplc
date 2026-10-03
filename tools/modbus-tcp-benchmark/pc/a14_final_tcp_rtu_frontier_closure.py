from __future__ import annotations

import argparse
import csv
import json
import sys
import time
from pathlib import Path

THIS_DIR = Path(__file__).resolve().parent
sys.path.insert(0, str(THIS_DIR))

import a14_final_full_runtime_campaign as cap
import a14_final_tcp_rtu_expmix_matrix as exp


RATE_COMMANDS: dict[int, tuple[bytes, str]] = {
    50: (b"A\n", "RTU_RATE_HZ=50"),
    100: (b"B\n", "RTU_RATE_HZ=100"),
    150: (b"C\n", "RTU_RATE_HZ=150"),
    200: (b"D\n", "RTU_RATE_HZ=200"),
    250: (b"E\n", "RTU_RATE_HZ=250"),
    300: (b"F\n", "RTU_RATE_HZ=300"),
    350: (b";\n", "RTU_RATE_HZ=350"),
    400: (b",\n", "RTU_RATE_HZ=400"),
    450: (b".\n", "RTU_RATE_HZ=450"),
    500: (b"/\n", "RTU_RATE_HZ=500"),
    550: (b"=\n", "RTU_RATE_HZ=550"),
    600: (b"_\n", "RTU_RATE_HZ=600"),
    650: (b"$\n", "RTU_RATE_HZ=650"),
    700: (b"%\n", "RTU_RATE_HZ=700"),
}

DEFAULT_RATES = "0,50,100,150,200,250,300,350,400,450,500,550,600,650,700"


def parse_rates(spec: str) -> list[int]:
    rates: list[int] = []
    for token in spec.split(","):
        item = token.strip()
        if not item:
            continue
        value = int(item)
        if value != 0 and value not in RATE_COMMANDS:
            raise ValueError(f"RTU rate no soportado: {value}")
        if value < 0:
            raise ValueError(f"RTU rate invalido: {value}")
        rates.append(value)
    if not rates:
        raise ValueError("lista RTU vacia")
    return rates


def set_rtu_rate(master, rate_hz: int) -> None:
    command = RATE_COMMANDS.get(rate_hz)
    if command is None:
        raise ValueError(f"RTU rate sin comando: {rate_hz}")
    exp.send(master, command[0], command[1], 4.0)


def fixed_profile_pass(
    master: dict[str, str],
    slave: dict[str, str],
    rate_hz: int,
) -> bool:
    if not exp.fast_profile_pass(master, slave):
        return False
    if rate_hz == 0:
        return True
    return (
        exp.sv(master, "RTU_RATE_MODE") == "PACED"
        and exp.iv(master, "RTU_TARGET_HZ", -1) == rate_hz
    )


def run_fixed_case(
    root: Path,
    master,
    slave,
    host: str,
    rate_hz: int,
    duration_s: float,
    tcp_target: float,
    label_prefix: str = "SWEEP",
) -> dict[str, object]:
    label = f"{label_prefix}_TCP{int(tcp_target)}_RTU{rate_hz}"
    case_root = root / label
    case_root.mkdir(parents=True, exist_ok=True)

    exp.send(master, b"X\n", exp.p5b.MASTER_STOP_ACK, 8.0)
    exp.q.wait_server_disconnected(master, timeout_s=20.0)

    if rate_hz > 0:
        set_rtu_rate(master, rate_hz)

    pre_master = exp.q.request_snapshot(master, echo=False)
    pre_slave = exp.p5b.request_slave_snapshot(slave, 5.0)
    exp.write_snapshot(case_root / "master_pre.log", pre_master)
    exp.write_snapshot(case_root / "slave_pre.log", pre_slave)

    if not fixed_profile_pass(pre_master, pre_slave, rate_hz):
        raise RuntimeError(f"R8_PROFILE_DRIFT_{label}")

    exp.p5b.reset_slave_stats(slave, 3.0)
    exp.q.reset_stats(master)

    if rate_hz > 0:
        exp.send(master, b"G\n", exp.p5b.MASTER_START_ACK)

    print(
        f"CASE_BEGIN={label} DURATION_S={duration_s:.0f} "
        f"TCP_TARGET={tcp_target:.0f} RTU_TARGET={rate_hz}",
        flush=True,
    )

    tcp = exp.budget.run_paced_fc03(
        host,
        502,
        duration_s,
        tcp_target,
        125,
    )

    if rate_hz > 0:
        exp.send(master, b"X\n", exp.p5b.MASTER_STOP_ACK, 8.0)

    time.sleep(0.10)

    post_master = exp.q.request_snapshot(master, echo=False)
    post_slave = exp.p5b.request_slave_snapshot(slave, 5.0)
    exp.write_snapshot(case_root / "master_final.log", post_master)
    exp.write_snapshot(case_root / "slave_final.log", post_slave)

    duration_ms = exp.iv(post_master, "RTU_TRAFFIC_DURATION_MS", 0)
    started = exp.iv(post_master, "RTU_REQUESTS_STARTED", 0)
    completed = exp.iv(post_master, "RTU_REQUESTS_COMPLETED", 0)
    success = exp.iv(post_master, "RTU_REQUESTS_SUCCESS", 0)
    failed = exp.iv(post_master, "RTU_REQUESTS_FAILED", 0)
    rejected = exp.iv(post_master, "RTU_REQUESTS_REJECTED", 0)
    verify = exp.iv(post_master, "RTU_VERIFY_FAILS", 0)
    scans = exp.iv(post_master, "RTU_MIX_SCANS", 0)
    skipped = exp.iv(post_master, "RTU_PERIODS_SKIPPED", 0)

    rtu_hz = (
        completed / (duration_ms / 1000.0)
        if duration_ms > 0
        else 0.0
    )
    scan_hz = (
        scans / (duration_ms / 1000.0)
        if duration_ms > 0
        else 0.0
    )
    scan_period_ms = 1000.0 / scan_hz if scan_hz > 0.0 else 0.0
    rtu_target_pct = (
        rtu_hz / rate_hz * 100.0
        if rate_hz > 0
        else 100.0
    )

    if rate_hz == 0:
        rtu_clean = (
            started == 0
            and completed == 0
            and success == 0
            and exp.iv(post_slave, "RTU_RX_FRAMES", 0) == 0
            and exp.iv(post_slave, "RTU_TX_FRAMES", 0) == 0
            and exp.iv(post_slave, "RTU_REQUESTS_OK", 0) == 0
        )
    else:
        mix_success = sum(
            exp.iv(post_master, key, 0)
            for key in (
                "RTU_MIX_DI_SUCCESS",
                "RTU_MIX_DO_SUCCESS",
                "RTU_MIX_AI_SUCCESS",
                "RTU_MIX_AO_SUCCESS",
            )
        )
        mix_failed = sum(
            exp.iv(post_master, key, 0)
            for key in (
                "RTU_MIX_DI_FAILED",
                "RTU_MIX_DO_FAILED",
                "RTU_MIX_AI_FAILED",
                "RTU_MIX_AO_FAILED",
            )
        )
        slave_rx = exp.iv(post_slave, "RTU_RX_FRAMES", 0)
        slave_tx = exp.iv(post_slave, "RTU_TX_FRAMES", 0)
        slave_ok = exp.iv(post_slave, "RTU_REQUESTS_OK", 0)

        rtu_clean = (
            started == completed == success == mix_success
            and failed == 0
            and mix_failed == 0
            and rejected == 0
            and verify == 0
            and success == scans * 8
            and exp.iv(post_master, "RTU_MIX_DI_SUCCESS", 0) == scans * 2
            and exp.iv(post_master, "RTU_MIX_DO_SUCCESS", 0) == scans * 2
            and exp.iv(post_master, "RTU_MIX_AI_SUCCESS", 0) == scans * 2
            and exp.iv(post_master, "RTU_MIX_AO_SUCCESS", 0) == scans * 2
            and exp.iv(post_master, "RTU_CRC_ERRORS", 0) == 0
            and exp.iv(post_master, "RTU_MASTER_TIMEOUTS", 0) == 0
            and exp.iv(post_slave, "RTU_CRC_ERRORS", 0) == 0
            and exp.iv(post_slave, "RTU_EXCEPTIONS_SENT", 0) == 0
            and exp.iv(post_slave, "RTU_SERVER_DISCARDED_TAILS", 0) == 0
            and exp.iv(post_slave, "RTU_SERVER_DISCARDED_BYTES", 0) == 0
            and slave_rx == success
            and slave_tx == success
            and slave_ok == success
            and exp.output_maps_exact(post_master, post_slave)
        )

    tcp_clean = (
        int(tcp["timeouts"]) == 0
        and int(tcp["transport_errors"]) == 0
        and int(tcp["protocol_errors"]) == 0
        and exp.iv(post_master, "FRAME_TIMEOUTS", 0) == 0
        and exp.iv(post_master, "BUS_LOCK_TIMEOUTS", 0) == 0
        and exp.iv(post_master, "PROTOCOL_ERRORS", 0) == 0
        and exp.iv(post_master, "REQUESTS_OK", 0) == int(tcp["ok"])
    )

    peripheral_clean = (
        exp.sv(post_master, "FULL_RUNTIME_READY") == "YES"
        and exp.sv(post_master, "SERVER_READY") == "YES"
        and exp.sv(post_master, "ETH_READY") == "YES"
        and exp.sv(post_master, "ETH_LINK") == "UP"
        and exp.sv(post_master, "DISPLAY_READY") == "YES"
        and exp.sv(post_master, "FRAM_READY") == "YES"
        and exp.sv(post_master, "SD_READY") == "YES"
        and exp.sv(post_master, "RTC_PRESENT") == "YES"
        and exp.sv(post_master, "IO_INITIALIZED") == "YES"
        and exp.sv(post_master, "BUTTONS_READY") == "YES"
        and exp.iv(post_master, "PERIPHERAL_FAILURE_COUNT", 0) == 0
        and exp.iv(post_master, "SD_DATALOG_FAILED_COMMITS", 0) == 0
        and exp.iv(post_master, "SPI_PROBE_FAILS", 0) == 0
        and exp.sv(post_slave, "DISPLAY_READY") == "YES"
    )

    reset_clean = exp.no_reset(
        pre_master,
        post_master,
        pre_slave,
        post_slave,
        float(tcp["elapsed_s"]),
    )

    runtime_clean = (
        rtu_clean
        and tcp_clean
        and peripheral_clean
        and reset_clean
        and fixed_profile_pass(post_master, post_slave, rate_hz)
    )

    tcp_target_pct = float(tcp["target_pct"])
    tcp_990 = tcp_target_pct >= 99.0
    tcp_995 = tcp_target_pct >= 99.5
    tcp_999 = tcp_target_pct >= 99.9
    rtu_target_pass = (
        rate_hz == 0
        or (rtu_target_pct >= 99.0 and skipped == 0)
    )

    if not runtime_clean:
        classification = "PRODUCT_FAILURE"
    elif not rtu_target_pass:
        classification = "RTU_TARGET_FAIL_CLEAN"
    elif tcp_999:
        classification = "PASS_TCP_99_9"
    elif tcp_995:
        classification = "PASS_TCP_99_5"
    elif tcp_990:
        classification = "PASS_TCP_99_0"
    else:
        classification = "TCP_SATURATION_FAIL_CLEAN"

    row: dict[str, object] = {
        "case": label,
        "tcp_target_req_s": tcp_target,
        "tcp_req_s": float(tcp["achieved_req_s"]),
        "tcp_target_pct": tcp_target_pct,
        "tcp_pass_99_0": tcp_990,
        "tcp_pass_99_5": tcp_995,
        "tcp_pass_99_9": tcp_999,
        "tcp_avg_us": float(tcp["avg_us"]),
        "tcp_p95_us": float(tcp["p95_us"]),
        "tcp_p99_us": float(tcp["p99_us"]),
        "tcp_max_us": float(tcp["max_us"]),
        "rtu_target_req_s": rate_hz,
        "rtu_req_s": rtu_hz,
        "rtu_target_pct": rtu_target_pct,
        "rtu_target_pass": rtu_target_pass,
        "rtu_periods_skipped": skipped,
        "rtu_scans_s": scan_hz,
        "rtu_scan_period_ms": scan_period_ms,
        "rtu_scans": scans,
        "rtu_total_success": success,
        "rtu_transaction_max_us":
            exp.iv(post_master, "RTU_TRANSACTION_MAX_US", 0),
        "loop_gap_avg_us":
            exp.iv(post_master, "LOOP_GAP_AVG_US", 0),
        "loop_gap_max_us":
            exp.iv(post_master, "LOOP_GAP_MAX_US", 0),
        "spi_probe_max_wait_us":
            exp.iv(post_master, "SPI_PROBE_MAX_WAIT_US", 0),
        "spi_probe_over_1ms":
            exp.iv(post_master, "SPI_PROBE_OVER_1MS", 0),
        "spi_probe_over_10ms":
            exp.iv(post_master, "SPI_PROBE_OVER_10MS", 0),
        "peripheral_failures":
            exp.iv(post_master, "PERIPHERAL_FAILURE_COUNT", 0),
        "rtu_clean": rtu_clean,
        "tcp_clean": tcp_clean,
        "peripheral_clean": peripheral_clean,
        "no_reset": reset_clean,
        "runtime_clean": runtime_clean,
        "classification": classification,
    }

    (case_root / "result.json").write_text(
        json.dumps(row, indent=2),
        encoding="utf-8",
    )

    print(
        f"CASE_END={label} "
        f"TCP={row['tcp_req_s']:.3f} "
        f"TCP_PCT={tcp_target_pct:.3f} "
        f"RTU={rtu_hz:.3f} "
        f"RTU_PCT={rtu_target_pct:.3f} "
        f"SCANS={scan_hz:.3f} "
        f"CLASS={classification}",
        flush=True,
    )

    print(
        "R8_DIAG "
        f"RTU_TARGET={rate_hz} "
        f"RTU_STARTED={started} "
        f"RTU_SUCCESS={success} "
        f"RTU_FAILED={failed} "
        f"RTU_REJECTED={rejected} "
        f"RTU_VERIFY={verify} "
        f"RTU_SKIPPED={skipped} "
        f"RTU_TIMEOUTS={exp.iv(post_master, 'RTU_MASTER_TIMEOUTS', 0)} "
        f"RTU_CRC={exp.iv(post_master, 'RTU_CRC_ERRORS', 0)} "
        f"SLAVE_TAILS={exp.iv(post_slave, 'RTU_SERVER_DISCARDED_TAILS', 0)} "
        f"SLAVE_TAIL_BYTES={exp.iv(post_slave, 'RTU_SERVER_DISCARDED_BYTES', 0)} "
        f"PERIPH_FAIL={exp.iv(post_master, 'PERIPHERAL_FAILURE_COUNT', 0)} "
        f"SPI_FAIL={exp.iv(post_master, 'SPI_PROBE_FAILS', 0)}",
        flush=True,
    )

    if classification == "PRODUCT_FAILURE":
        raise RuntimeError(f"R8_PRODUCT_FAILURE_{label}")

    return row


def write_csv(path: Path, rows: list[dict[str, object]]) -> None:
    if not rows:
        return
    with path.open("w", newline="", encoding="utf-8") as handle:
        writer = csv.DictWriter(handle, fieldnames=list(rows[0].keys()))
        writer.writeheader()
        writer.writerows(rows)


def best_rate(
    rows: list[dict[str, object]],
    tcp_threshold_pct: float,
) -> dict[str, object] | None:
    valid = [
        row for row in rows
        if int(row["rtu_target_req_s"]) > 0
        and bool(row["runtime_clean"])
        and bool(row["rtu_target_pass"])
        and float(row["tcp_target_pct"]) >= tcp_threshold_pct
    ]
    if not valid:
        return None
    return max(valid, key=lambda row: int(row["rtu_target_req_s"]))


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--master-serial", default="COM14")
    parser.add_argument("--slave-serial", default="COM4")
    parser.add_argument("--output-root", required=True)
    parser.add_argument("--unpaced-duration", type=float, default=600.0)
    parser.add_argument("--tcp-only-duration", type=float, default=600.0)
    parser.add_argument("--sweep-duration", type=float, default=300.0)
    parser.add_argument("--confirm-duration", type=float, default=600.0)
    parser.add_argument("--tcp-target", type=float, default=1000.0)
    parser.add_argument("--rtu-rates", default=DEFAULT_RATES)
    args = parser.parse_args()

    if args.unpaced_duration < 600.0:
        raise ValueError("unpaced-duration debe ser >=600 s")
    if args.tcp_only_duration < 600.0:
        raise ValueError("tcp-only-duration debe ser >=600 s")
    if args.sweep_duration < 300.0:
        raise ValueError("sweep-duration debe ser >=300 s")
    if args.confirm_duration < 600.0:
        raise ValueError("confirm-duration debe ser >=600 s")
    if args.tcp_target <= 0.0:
        raise ValueError("tcp-target invalido")

    rates = parse_rates(args.rtu_rates)
    root = Path(args.output_root)
    root.mkdir(parents=True, exist_ok=True)

    master = exp.p5b.open_serial_no_dtr(args.master_serial)
    slave = exp.p5b.open_serial_no_dtr(args.slave_serial)

    r7_rows: list[dict[str, object]] = []
    fixed_rows: list[dict[str, object]] = []
    confirmation: dict[str, object] | None = None
    tcp_only: dict[str, object] | None = None

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
            raise RuntimeError("R7_R8_FAST_PROFILE_PREFLIGHT_FAIL")

        print("=" * 78)
        print(" A14 FINAL R7/R8 - TCP x RTU EXP-MIX FRONTIER CLOSURE")
        print("=" * 78)
        print(f"DUT_IP={host}")
        print(f"UNPACED_DURATION_S={args.unpaced_duration:.0f}")
        print(f"TCP_ONLY_DURATION_S={args.tcp_only_duration:.0f}")
        print(f"SWEEP_DURATION_S={args.sweep_duration:.0f}")
        print(f"CONFIRM_DURATION_S={args.confirm_duration:.0f}")
        print(f"TCP_TARGET_REQ_S={args.tcp_target:.0f}")
        print("RTU_RATES=" + ",".join(str(x) for x in rates))
        print("STRICT_TCP_COMPATIBILITY=99.5_PERCENT")
        print("RTU_TARGET_PASS=99.0_PERCENT_AND_NO_SKIPS")

        r7_root = root / "R7_UNPACED_FRONTIER"
        r7_root.mkdir(parents=True, exist_ok=True)

        for target in (None, 900.0, 950.0):
            row = exp.run_case(
                r7_root,
                master,
                slave,
                host,
                target,
                args.unpaced_duration,
            )
            r7_rows.append(row)

        tcp_only_root = root / "R7_TCP_ONLY_CEILING"
        tcp_only_root.mkdir(parents=True, exist_ok=True)
        tcp_only = cap.run_case(
            "TCP_ONLY_UNPACED",
            tcp_only_root,
            master,
            slave,
            host,
            args.tcp_only_duration,
            None,
            "UNPACED",
            "OFF",
        )

        exp.configure_fast(master, slave)

        sweep_root = root / "R8_TCP1000_RTU_FIXED_SWEEP"
        sweep_root.mkdir(parents=True, exist_ok=True)

        for rate_hz in rates:
            fixed_rows.append(
                run_fixed_case(
                    sweep_root,
                    master,
                    slave,
                    host,
                    rate_hz,
                    args.sweep_duration,
                    args.tcp_target,
                )
            )

        write_csv(root / "R8_TCP1000_RTU_SWEEP.csv", fixed_rows)

        best_990 = best_rate(fixed_rows, 99.0)
        best_995 = best_rate(fixed_rows, 99.5)
        best_999 = best_rate(fixed_rows, 99.9)

        selection = best_995 if best_995 is not None else best_990

        selection_doc = {
            "tcp_target_req_s": args.tcp_target,
            "strict_threshold_pct": 99.5,
            "best_rtu_at_tcp_99_0": (
                None if best_990 is None else int(best_990["rtu_target_req_s"])
            ),
            "best_rtu_at_tcp_99_5": (
                None if best_995 is None else int(best_995["rtu_target_req_s"])
            ),
            "best_rtu_at_tcp_99_9": (
                None if best_999 is None else int(best_999["rtu_target_req_s"])
            ),
            "confirmation_selected_rate": (
                None if selection is None else int(selection["rtu_target_req_s"])
            ),
            "selection_policy": (
                "BEST_99_5"
                if best_995 is not None
                else "BEST_99_0_FALLBACK"
                if best_990 is not None
                else "NONE"
            ),
        }
        (root / "R8_SELECTION.json").write_text(
            json.dumps(selection_doc, indent=2),
            encoding="utf-8",
        )

        if selection is not None:
            confirm_rate = int(selection["rtu_target_req_s"])
            confirm_root = root / "R8_CONFIRMATION"
            confirm_root.mkdir(parents=True, exist_ok=True)
            confirmation = run_fixed_case(
                confirm_root,
                master,
                slave,
                host,
                confirm_rate,
                args.confirm_duration,
                args.tcp_target,
                label_prefix="CONFIRM",
            )

    finally:
        master.close()
        slave.close()

    if r7_rows:
        write_csv(root / "R7_UNPACED_FRONTIER.csv", r7_rows)

    strict_found = best_995 is not None
    confirmation_strict = (
        confirmation is not None
        and bool(confirmation["runtime_clean"])
        and bool(confirmation["rtu_target_pass"])
        and float(confirmation["tcp_target_pct"]) >= 99.5
    )

    status = (
        "PASS_STRICT_CONFIRMED"
        if strict_found and confirmation_strict
        else "REVIEW_STRICT_NOT_CONFIRMED"
        if strict_found
        else "REVIEW_NO_POSITIVE_RTU_AT_TCP_99_5"
    )

    summary = {
        "status": status,
        "r7_unpaced_cases": r7_rows,
        "tcp_only_unpaced": tcp_only,
        "r8_fixed_sweep": fixed_rows,
        "selection": selection_doc,
        "confirmation": confirmation,
    }
    (root / "FINAL_SUMMARY.json").write_text(
        json.dumps(summary, indent=2),
        encoding="utf-8",
    )
    (root / "FINAL_STATUS.txt").write_text(
        "\n".join(
            [
                f"A14_R7_R8_FRONTIER_CLOSURE={status}",
                f"TCP1000_BEST_RTU_99_0={selection_doc['best_rtu_at_tcp_99_0']}",
                f"TCP1000_BEST_RTU_99_5={selection_doc['best_rtu_at_tcp_99_5']}",
                f"TCP1000_BEST_RTU_99_9={selection_doc['best_rtu_at_tcp_99_9']}",
                f"CONFIRM_SELECTED_RTU={selection_doc['confirmation_selected_rate']}",
            ]
        ) + "\n",
        encoding="utf-8",
    )

    print("=" * 78)
    print(" A14 R7/R8 FRONTIER CLOSURE SUMMARY")
    print("=" * 78)
    print(f"A14_R7_R8_FRONTIER_CLOSURE={status}")
    print(
        "TCP1000_BEST_RTU_99_0="
        f"{selection_doc['best_rtu_at_tcp_99_0']}"
    )
    print(
        "TCP1000_BEST_RTU_99_5="
        f"{selection_doc['best_rtu_at_tcp_99_5']}"
    )
    print(
        "TCP1000_BEST_RTU_99_9="
        f"{selection_doc['best_rtu_at_tcp_99_9']}"
    )
    print(
        "CONFIRM_SELECTED_RTU="
        f"{selection_doc['confirmation_selected_rate']}"
    )
    print(f"RESULT_ROOT={root}")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
