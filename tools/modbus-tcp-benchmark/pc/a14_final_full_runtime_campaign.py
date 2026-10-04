from __future__ import annotations

import argparse
import csv
import json
import sys
import time
from pathlib import Path

THIS_DIR = Path(__file__).resolve().parent
sys.path.insert(0, str(THIS_DIR))

import a14_perf_fc03_qualification_sweep as q
import a14_p5b_master_slave_qualification as p5b
import a14_p5e1_unpaced_fc03_ceiling as e1
import a14_p5rtu_tcp_budget_frontier as budget
import a14_rtuh3b_bulk_rx_ab as h3b


def iv(values: dict[str, str], key: str, default: int = -1) -> int:
    return p5b.int_value(values, key, default)


def sv(values: dict[str, str], key: str) -> str:
    return values.get(key, "").strip()


def write_snapshot(path: Path, values: dict[str, str]) -> None:
    raw = values.get("_RAW")
    if raw:
        path.write_text(raw, encoding="utf-8")
        return
    lines = [f"{k}={v}" for k, v in sorted(values.items()) if k != "_RAW"]
    path.write_text("\n".join(lines) + "\n", encoding="utf-8")


def send(ser, command: bytes, ack: str) -> None:
    budget.send_command_wait(ser, command, ack)


def stop_and_quiesce(master) -> None:
    send(master, b"X\n", p5b.MASTER_STOP_ACK)
    q.wait_server_disconnected(master, timeout_s=20.0)
    time.sleep(0.10)


def reset_case(master, slave) -> None:
    q.reset_stats(master)
    p5b.reset_slave_stats(slave, 3.0)


def configure_rtu50(master) -> None:
    send(master, b"A\n", "RTU_RATE_HZ=50")


def configure_fast_capacity(master, slave) -> None:
    h3b.configure_fixed_profile(master, slave)
    h3b.configure_mode(master, slave, "BULK", b"+\n")
    send(master, b")\n", "RTU_SERVER_FRAMING=GAP")
    send(slave, b"(\n", "RTU_SERVER_FRAMING=STRUCTURAL")
    send(master, b"Z\n", "RTU_TX_MODE=QUEUED")
    send(slave, b"Z\n", "RTU_TX_MODE=QUEUED")
    send(slave, b"}\n", "RTU_RX_FIFO_FULL=8")
    send(master, b":\n", "RTU_RX_FIFO_FULL=9")
    send(master, b"<\n", "RTU_CRC_MODE=BITWISE")
    send(slave, b"<\n", "RTU_CRC_MODE=BITWISE")
    send(master, b"U\n", "RTU_RATE_MODE=UNPACED")

    ms = q.request_snapshot(master, echo=False)
    ss = p5b.request_slave_snapshot(slave, 5.0)
    checks = {
        "master_baud": iv(ms, "RTU_BAUD_EFFECTIVE") == 500000,
        "slave_baud": iv(ss, "RTU_BAUD_EFFECTIVE") == 500000,
        "master_gap": iv(ms, "RTU_FRAME_GAP_US") == 100,
        "slave_gap": iv(ss, "RTU_FRAME_GAP_US") == 100,
        "master_fifo": iv(ms, "RTU_RX_FIFO_FULL") == 9,
        "slave_fifo": iv(ss, "RTU_RX_FIFO_FULL") == 8,
        "master_rx": sv(ms, "RTU_RX_MODE") == "BULK",
        "slave_rx": sv(ss, "RTU_RX_MODE") == "BULK",
        "master_tx": sv(ms, "RTU_TX_MODE") == "QUEUED",
        "slave_tx": sv(ss, "RTU_TX_MODE") == "QUEUED",
        "master_framing": sv(ms, "RTU_SERVER_FRAMING") == "GAP",
        "slave_framing": sv(ss, "RTU_SERVER_FRAMING") == "STRUCTURAL",
        "master_crc": sv(ms, "RTU_CRC_MODE") == "BITWISE",
        "slave_crc": sv(ss, "RTU_CRC_MODE") == "BITWISE",
    }
    if not all(checks.values()):
        raise RuntimeError("FAST_CAPACITY_PROFILE_INVALID=" + json.dumps(checks))


def zero_tcp(duration_s: float) -> dict[str, float | int]:
    start = time.perf_counter()
    time.sleep(duration_s)
    elapsed = time.perf_counter() - start
    return {
        "elapsed_s": elapsed,
        "sent": 0,
        "ok": 0,
        "timeouts": 0,
        "transport_errors": 0,
        "protocol_errors": 0,
        "achieved_req_s": 0.0,
        "target_pct": 100.0,
        "useful_mbps": 0.0,
        "total_mbps": 0.0,
        "avg_us": 0.0,
        "p95_us": 0.0,
        "p99_us": 0.0,
        "max_us": 0.0,
    }


def normalize_tcp(raw: dict[str, object], target: float | None) -> dict[str, float | int]:
    def pick(*names: str, default=0.0):
        for name in names:
            if name in raw:
                return raw[name]
        return default

    achieved = float(pick("achieved_req_s", default=0.0))
    target_pct = (
        achieved / target * 100.0
        if target is not None and target > 0.0
        else float(pick("target_pct", default=100.0))
    )
    return {
        "sent": int(pick("sent", default=0)),
        "ok": int(pick("ok", default=0)),
        "timeouts": int(pick("timeouts", default=0)),
        "transport_errors": int(pick("transport_errors", default=0)),
        "protocol_errors": int(pick("protocol_errors", default=0)),
        "achieved_req_s": achieved,
        "target_pct": target_pct,
        "useful_mbps": float(pick("useful_mbps", default=0.0)),
        "total_mbps": float(pick("total_mbps", default=0.0)),
        "avg_us": float(pick("avg_us", "latency_avg_us", default=0.0)),
        "p95_us": float(pick("p95_us", "latency_p95_us", default=0.0)),
        "p99_us": float(pick("p99_us", "latency_p99_us", default=0.0)),
        "max_us": float(pick("max_us", "latency_max_us", default=0.0)),
    }


def collect_case(
    label: str,
    case_root: Path,
    master,
    slave,
    tcp_raw: dict[str, object],
    target: float | None,
    rtu_expected: str,
    tcp_mode: str,
    duration_s: float,
    pre_ms: dict[str, str],
    pre_ss: dict[str, str],
) -> dict[str, object]:
    time.sleep(0.10)
    ms = q.request_snapshot(master, echo=False)
    ss = p5b.request_slave_snapshot(slave, 5.0)
    write_snapshot(case_root / "master_final.txt", ms)
    write_snapshot(case_root / "slave_final.txt", ss)

    tcp = normalize_tcp(tcp_raw, target)
    duration_ms = iv(ms, "RTU_TRAFFIC_DURATION_MS", 0)
    started = iv(ms, "RTU_REQUESTS_STARTED", 0)
    completed = iv(ms, "RTU_REQUESTS_COMPLETED", 0)
    success = iv(ms, "RTU_REQUESTS_SUCCESS", 0)
    rejected = iv(ms, "RTU_REQUESTS_REJECTED", 0)
    failed = iv(ms, "RTU_REQUESTS_FAILED", 0)
    verify = iv(ms, "RTU_VERIFY_FAILS", 0)
    rtu_timeouts = iv(ms, "RTU_MASTER_TIMEOUTS", 0)
    rtu_crc = iv(ms, "RTU_CRC_ERRORS", 0)

    rtu_hz = completed / (duration_ms / 1000.0) if duration_ms > 0 else 0.0

    slave_rx = iv(ss, "RTU_RX_FRAMES", 0)
    slave_tx = iv(ss, "RTU_TX_FRAMES", 0)
    slave_ok = iv(ss, "RTU_REQUESTS_OK", 0)
    slave_crc = iv(ss, "RTU_CRC_ERRORS", 0)

    tcp_clean = (
        tcp["timeouts"] == 0
        and tcp["transport_errors"] == 0
        and tcp["protocol_errors"] == 0
        and iv(ms, "FRAME_TIMEOUTS", 0) == 0
        and iv(ms, "BUS_LOCK_TIMEOUTS", 0) == 0
        and iv(ms, "PROTOCOL_ERRORS", 0) == 0
        and iv(ms, "REQUESTS_OK", 0) == int(tcp["ok"])
    )

    if rtu_expected == "OFF":
        rtu_clean = (
            started == 0
            and completed == 0
            and slave_rx == 0
            and slave_tx == 0
        )
    else:
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

    peripheral_failures = iv(ms, "PERIPHERAL_FAILURE_COUNT", 0)
    sd_failed = iv(ms, "SD_DATALOG_FAILED_COMMITS", 0)
    spi_probe_fails = iv(ms, "SPI_PROBE_FAILS", 0)

    pre_master_boot = iv(pre_ms, "BOOT_MARKER", -1)
    post_master_boot = iv(ms, "BOOT_MARKER", -1)
    pre_slave_boot = iv(pre_ss, "BOOT_MARKER", -1)
    post_slave_boot = iv(ss, "BOOT_MARKER", -1)

    pre_master_uptime = iv(pre_ms, "UPTIME_MS", -1)
    post_master_uptime = iv(ms, "UPTIME_MS", -1)
    pre_slave_uptime = iv(pre_ss, "UPTIME_MS", -1)
    post_slave_uptime = iv(ss, "UPTIME_MS", -1)

    min_delta_ms = int(duration_s * 1000.0 * 0.90)
    boot_markers_stable = (
        pre_master_boot >= 0
        and pre_slave_boot >= 0
        and pre_master_boot == post_master_boot
        and pre_slave_boot == post_slave_boot
    )
    uptime_stable = (
        pre_master_uptime >= 0
        and pre_slave_uptime >= 0
        and post_master_uptime >= pre_master_uptime
        and post_slave_uptime >= pre_slave_uptime
        and (post_master_uptime - pre_master_uptime) >= min_delta_ms
        and (post_slave_uptime - pre_slave_uptime) >= min_delta_ms
    )
    no_reset = boot_markers_stable and uptime_stable

    ready_clean = (
        sv(ms, "FULL_RUNTIME_READY") == "YES"
        and sv(ss, "SLAVE_READY") == "YES"
        and sv(ss, "RTU_READY") == "YES"
        and sv(ss, "DISPLAY_READY") == "YES"
    )

    runtime_clean = (
        tcp_clean
        and rtu_clean
        and peripheral_failures == 0
        and sd_failed == 0
        and spi_probe_fails == 0
        and no_reset
        and ready_clean
    )

    target_pass = target is None or float(tcp["target_pct"]) >= 99.0
    classification = (
        "PASS"
        if runtime_clean and target_pass
        else "SATURATION_FAIL_CLEAN"
        if runtime_clean and not target_pass
        else "PRODUCT_FAILURE"
    )

    row: dict[str, object] = {
        "case": label,
        "tcp_mode": tcp_mode,
        "tcp_target_req_s": (
            "OFF"
            if tcp_mode == "OFF"
            else "UNPACED"
            if tcp_mode == "UNPACED"
            else target
        ),
        "tcp_req_s": tcp["achieved_req_s"],
        "tcp_target_pct": tcp["target_pct"],
        "tcp_avg_us": tcp["avg_us"],
        "tcp_p95_us": tcp["p95_us"],
        "tcp_p99_us": tcp["p99_us"],
        "tcp_max_us": tcp["max_us"],
        "tcp_timeouts": tcp["timeouts"],
        "tcp_transport_errors": tcp["transport_errors"],
        "tcp_protocol_errors": tcp["protocol_errors"],
        "rtu_profile": rtu_expected,
        "rtu_baud_effective": iv(ms, "RTU_BAUD_EFFECTIVE", 0),
        "rtu_frame_gap_us": iv(ms, "RTU_FRAME_GAP_US", 0),
        "rtu_master_fifo": iv(ms, "RTU_RX_FIFO_FULL", 0),
        "rtu_slave_fifo": iv(ss, "RTU_RX_FIFO_FULL", 0),
        "rtu_master_rx_mode": sv(ms, "RTU_RX_MODE"),
        "rtu_slave_rx_mode": sv(ss, "RTU_RX_MODE"),
        "rtu_master_framing": sv(ms, "RTU_SERVER_FRAMING"),
        "rtu_slave_framing": sv(ss, "RTU_SERVER_FRAMING"),
        "rtu_crc_mode": sv(ms, "RTU_CRC_MODE"),
        "rtu_hz": rtu_hz,
        "rtu_started": started,
        "rtu_completed": completed,
        "rtu_success": success,
        "rtu_failed": failed,
        "rtu_timeouts": rtu_timeouts,
        "rtu_crc_master": rtu_crc,
        "rtu_crc_slave": slave_crc,
        "peripheral_failures": peripheral_failures,
        "sd_failed_commits": sd_failed,
        "loop_gap_avg_us": iv(ms, "LOOP_GAP_AVG_US", 0),
        "loop_gap_max_us": iv(ms, "LOOP_GAP_MAX_US", 0),
        "rtu_service_gap_max_us": iv(ms, "RTU_SERVICE_GAP_MAX_US", 0),
        "rtu_transaction_max_us": iv(ms, "RTU_TRANSACTION_MAX_US", 0),
        "spi_probe_samples": iv(ms, "SPI_PROBE_SAMPLES", 0),
        "spi_probe_fails": spi_probe_fails,
        "spi_probe_max_wait_us": iv(ms, "SPI_PROBE_MAX_WAIT_US", 0),
        "spi_probe_over_1ms": iv(ms, "SPI_PROBE_OVER_1MS", 0),
        "spi_probe_over_10ms": iv(ms, "SPI_PROBE_OVER_10MS", 0),
        "tcp_clean": tcp_clean,
        "rtu_clean": rtu_clean,
        "runtime_clean": runtime_clean,
        "ready_clean": ready_clean,
        "unexpected_reset": not no_reset,
        "master_boot_marker": post_master_boot,
        "slave_boot_marker": post_slave_boot,
        "master_uptime_delta_ms": (
            post_master_uptime - pre_master_uptime
            if post_master_uptime >= pre_master_uptime >= 0
            else -1
        ),
        "slave_uptime_delta_ms": (
            post_slave_uptime - pre_slave_uptime
            if post_slave_uptime >= pre_slave_uptime >= 0
            else -1
        ),
        "tcp_target_pass": target_pass,
        "classification": classification,
    }

    (case_root / "result.json").write_text(
        json.dumps(row, indent=2), encoding="utf-8"
    )
    return row


def run_case(
    label: str,
    root: Path,
    master,
    slave,
    host: str,
    duration_s: float,
    target: float | None,
    tcp_mode: str,
    rtu_mode: str,
) -> dict[str, object]:
    case_root = root / label
    case_root.mkdir(parents=True, exist_ok=True)

    stop_and_quiesce(master)

    if rtu_mode == "OFF":
        pass
    elif rtu_mode == "50HZ":
        configure_rtu50(master)
    elif rtu_mode == "UNPACED":
        send(master, b"U\n", "RTU_RATE_MODE=UNPACED")
    else:
        raise ValueError(f"rtu_mode desconocido: {rtu_mode}")

    # Outside the measured window: record boot/readiness before final counter reset.
    pre_ms = q.request_snapshot(master, echo=False)
    pre_ss = p5b.request_slave_snapshot(slave, 5.0)
    write_snapshot(case_root / "master_pre.txt", pre_ms)
    write_snapshot(case_root / "slave_pre.txt", pre_ss)

    # Reset after pre-snapshot so Serial work is excluded from PERFORMANCE.
    reset_case(master, slave)

    if rtu_mode != "OFF":
        send(master, b"G\n", p5b.MASTER_START_ACK)

    print(
        f"CASE_BEGIN={label} DURATION_S={duration_s:.0f} "
        f"TCP={tcp_mode} TARGET={target} RTU={rtu_mode}",
        flush=True,
    )

    if tcp_mode == "OFF":
        tcp_raw = zero_tcp(duration_s)
    elif tcp_mode == "UNPACED":
        tcp_raw = e1.run_unpaced(host, 502, duration_s, 125, duration_s)
    elif tcp_mode == "PACED":
        assert target is not None
        tcp_raw = budget.run_paced_fc03(host, 502, duration_s, target, 125)
    else:
        raise ValueError(f"tcp_mode desconocido: {tcp_mode}")

    if rtu_mode != "OFF":
        send(master, b"X\n", p5b.MASTER_STOP_ACK)

    row = collect_case(
        label,
        case_root,
        master,
        slave,
        tcp_raw,
        target,
        rtu_mode,
        tcp_mode,
        duration_s,
        pre_ms,
        pre_ss,
    )

    print(
        f"CASE_END={label} TCP={float(row['tcp_req_s']):.3f} "
        f"RTU={float(row['rtu_hz']):.3f} "
        f"CLASS={row['classification']}",
        flush=True,
    )

    if row["classification"] == "PRODUCT_FAILURE":
        raise RuntimeError(f"PRODUCT_FAILURE_IN_{label}")

    return row


def write_csv(path: Path, rows: list[dict[str, object]]) -> None:
    if not rows:
        return
    with path.open("w", newline="", encoding="utf-8") as f:
        w = csv.DictWriter(f, fieldnames=list(rows[0].keys()))
        w.writeheader()
        w.writerows(rows)


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--master-serial", default="COM14")
    ap.add_argument("--slave-serial", default="COM4")
    ap.add_argument("--host", required=True)
    ap.add_argument("--output-root", required=True)
    ap.add_argument("--full-duration", type=float, default=600.0)
    ap.add_argument("--sweep-duration", type=float, default=300.0)
    ap.add_argument("--profile-duration", type=float, default=300.0)
    args = ap.parse_args()

    if args.full_duration < 600.0:
        raise ValueError("full-duration debe ser >= 600 s")
    if args.sweep_duration < 300.0:
        raise ValueError("sweep-duration debe ser >= 300 s")
    if args.profile_duration < 300.0:
        raise ValueError("profile-duration debe ser >= 300 s")

    root = Path(args.output_root)
    root.mkdir(parents=True, exist_ok=True)

    master = p5b.open_serial_no_dtr(args.master_serial)
    slave = p5b.open_serial_no_dtr(args.slave_serial)
    rows: list[dict[str, object]] = []

    try:
        time.sleep(1.0)

        # F2: full runtime, RTU OFF, TCP unpaced ceiling.
        rows.append(run_case(
            "F2_TCP_ONLY_CEILING",
            root, master, slave, args.host,
            args.full_duration, None, "UNPACED", "OFF",
        ))

        # F3/F4: industrial coexistence profile already proven historically.
        rows.append(run_case(
            "F3_TCP1000_RTU50",
            root, master, slave, args.host,
            args.full_duration, 1000.0, "PACED", "50HZ",
        ))
        rows.append(run_case(
            "F4_TCP500_RTU50",
            root, master, slave, args.host,
            args.full_duration, 500.0, "PACED", "50HZ",
        ))

        # First PROFILE point must still use the industrial 115200/RTU50
        # state loaded by P5B. Run it before switching the UARTs to FAST.
        profile_rows: list[dict[str, object]] = []
        profile_industrial = run_case(
            "F7P_TCP1000_RTU50",
            root, master, slave, args.host,
            args.profile_duration, 1000.0, "PACED", "50HZ",
        )
        rows.append(profile_industrial)
        profile_rows.append(profile_industrial)

        # F5A: 115200 industrial-profile unpaced reference.
        rows.append(run_case(
            "F5A_RTU_ONLY_115200_UNPACED",
            root, master, slave, args.host,
            args.full_duration, None, "OFF", "UNPACED",
        ))

        # Capacity profile: validated for FC03, not a universal RTU default.
        stop_and_quiesce(master)
        configure_fast_capacity(master, slave)

        # F5B: absolute RTU ceiling in the validated FC03 capacity profile.
        f5 = run_case(
            "F5B_RTU_ONLY_FAST_CEILING",
            root, master, slave, args.host,
            args.full_duration, None, "OFF", "UNPACED",
        )
        rows.append(f5)
        rtu_ceiling = float(f5["rtu_hz"])

        # F6: requested TCP sweep vs maximum clean RTU.
        matrix: list[dict[str, object]] = []
        for target in (100.0, 250.0, 500.0, 750.0, 1000.0):
            row = run_case(
                f"F6_TCP{int(target)}_RTU_FAST",
                root, master, slave, args.host,
                args.sweep_duration, target, "PACED", "UNPACED",
            )
            rows.append(row)
            matrix.append(row)

        write_csv(root / "TCP_RTU_MATRIX.csv", matrix)

        clean_target = [
            r for r in matrix
            if bool(r["runtime_clean"]) and bool(r["tcp_target_pass"])
        ]
        preserve90 = [
            r for r in clean_target
            if rtu_ceiling > 0.0 and float(r["rtu_hz"]) >= 0.90 * rtu_ceiling
        ]
        if preserve90:
            best = max(preserve90, key=lambda r: float(r["tcp_target_req_s"]))
            criterion = "HIGHEST_TCP_WITH_RTU_GE_90PCT_CEILING"
        elif clean_target:
            best = max(clean_target, key=lambda r: float(r["rtu_hz"]))
            criterion = "MAX_RTU_AMONG_TCP_TARGET_PASS"
        else:
            raise RuntimeError("NO_CLEAN_F6_POINT_FOR_CONFIRMATION")

        selected_target = float(best["tcp_target_req_s"])
        (root / "F8_SELECTION.json").write_text(
            json.dumps(
                {
                    "criterion": criterion,
                    "rtu_ceiling_hz": rtu_ceiling,
                    "selected_tcp_target_req_s": selected_target,
                    "selected_source_case": best["case"],
                    "selected_rtu_hz": best["rtu_hz"],
                },
                indent=2,
            ),
            encoding="utf-8",
        )

        # F7 continued: FAST profile points. Serial remains silent during
        # each window; counters are dumped only after the case finishes.
        for label, target in (
            ("F7P_TCP500_RTU_FAST", 500.0),
            ("F7P_TCP1000_RTU_FAST", 1000.0),
        ):
            stop_and_quiesce(master)
            configure_fast_capacity(master, slave)
            row = run_case(
                label,
                root, master, slave, args.host,
                args.profile_duration, target, "PACED", "UNPACED",
            )
            rows.append(row)
            profile_rows.append(row)
        write_csv(root / "PROFILE_SUMMARY.csv", profile_rows)

        # F8: 10 minute confirmation of the automatically selected equilibrium.
        stop_and_quiesce(master)
        configure_fast_capacity(master, slave)
        f8 = run_case(
            f"F8_CONFIRM_TCP{int(selected_target)}_RTU_FAST",
            root, master, slave, args.host,
            args.full_duration, selected_target, "PACED", "UNPACED",
        )
        rows.append(f8)

    finally:
        master.close()
        slave.close()

    write_csv(root / "FULL_RUNTIME_SUMMARY.csv", rows)

    product_fail = any(r["classification"] == "PRODUCT_FAILURE" for r in rows)
    (root / "FINAL_STATUS.txt").write_text(
        (
            "A14_FINAL_CAPABILITY_CAMPAIGN="
            + ("FAIL" if product_fail else "PASS_CHARACTERIZED")
            + "\n"
        ),
        encoding="utf-8",
    )

    print(
        "A14_FINAL_CAPABILITY_CAMPAIGN="
        + ("FAIL" if product_fail else "PASS_CHARACTERIZED")
    )
    return 2 if product_fail else 0


if __name__ == "__main__":
    raise SystemExit(main())
