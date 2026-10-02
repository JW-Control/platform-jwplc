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
import a14_p5rtu_tcp_budget_frontier as budget


TARGETS: tuple[float | None, ...] = (
    None,
    100.0,
    250.0,
    500.0,
    750.0,
    1000.0,
)


def iv(values: dict[str, str], key: str, default: int = -1) -> int:
    return p5b.int_value(values, key, default)


def sv(values: dict[str, str], key: str) -> str:
    return values.get(key, "").strip()


def write_snapshot(path: Path, values: dict[str, str]) -> None:
    path.write_text(values.get("_RAW", ""), encoding="utf-8")


def send(ser, command: bytes, ack: str, timeout_s: float = 4.0) -> None:
    budget.send_command_wait(ser, command, ack, timeout_s)


def configure_fast(master, slave) -> None:
    send(master, b"X\n", p5b.MASTER_STOP_ACK, 6.0)

    send(master, b"9\n", "RTU_BAUD_REQUESTED=500000")
    send(slave, b"9\n", "RTU_BAUD_REQUESTED=500000")
    time.sleep(0.10)

    send(master, b"4\n", "RTU_FRAME_GAP_US=100")
    send(slave, b"4\n", "RTU_FRAME_GAP_US=100")

    send(master, b":\n", "RTU_RX_FIFO_FULL=9")
    send(slave, b"}\n", "RTU_RX_FIFO_FULL=8")

    send(master, b"+\n", "RTU_RX_MODE=BULK")
    send(slave, b"+\n", "RTU_RX_MODE=BULK")

    send(master, b"Z\n", "RTU_TX_MODE=QUEUED")
    send(slave, b"Z\n", "RTU_TX_MODE=QUEUED")

    send(master, b"<\n", "RTU_CRC_MODE=BITWISE")
    send(slave, b"<\n", "RTU_CRC_MODE=BITWISE")

    send(master, b")\n", "RTU_SERVER_FRAMING=GAP")
    send(slave, b"(\n", "RTU_SERVER_FRAMING=STRUCTURAL")

    send(master, b"U\n", "RTU_RATE_MODE=UNPACED")


def fast_profile_pass(master: dict[str, str], slave: dict[str, str]) -> bool:
    return (
        iv(master, "RTU_BAUD_EFFECTIVE") == 500000
        and iv(slave, "RTU_BAUD_EFFECTIVE") == 500000
        and iv(master, "RTU_RX_FIFO_FULL") == 9
        and iv(slave, "RTU_RX_FIFO_FULL") == 8
        and sv(master, "RTU_RX_MODE") == "BULK"
        and sv(slave, "RTU_RX_MODE") == "BULK"
        and sv(master, "RTU_TX_MODE") == "QUEUED"
        and sv(slave, "RTU_TX_MODE") == "QUEUED"
        and sv(master, "RTU_SERVER_FRAMING") == "GAP"
        and sv(slave, "RTU_SERVER_FRAMING") == "STRUCTURAL"
        and sv(master, "RTU_CRC_MODE") == "BITWISE"
        and sv(slave, "RTU_CRC_MODE") == "BITWISE"
        and sv(master, "RTU_WORKLOAD") == "EXP_MIX_2DI_2DO_2AI_2AO"
    )


def wait_ready(master, slave) -> tuple[str, dict[str, str], dict[str, str]]:
    deadline = time.perf_counter() + 45.0
    last_m: dict[str, str] = {}
    last_s: dict[str, str] = {}

    while time.perf_counter() < deadline:
        last_m = q.request_snapshot(master, echo=False)
        last_s = p5b.request_slave_snapshot(slave, 5.0)
        host = sv(last_m, "ETH_IP")

        if (
            sv(last_m, "FULL_RUNTIME_READY") == "YES"
            and sv(last_m, "ETH_READY") == "YES"
            and sv(last_m, "ETH_LINK") == "UP"
            and sv(last_s, "SLAVE_READY") == "YES"
            and host
            and host != "0.0.0.0"
        ):
            return host, last_m, last_s

        time.sleep(0.25)

    raise RuntimeError(
        "R4_READY_TIMEOUT\n"
        + last_m.get("_RAW", "")
        + "\n--- SLAVE ---\n"
        + last_s.get("_RAW", "")
    )


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


def output_maps_exact(master: dict[str, str], slave: dict[str, str]) -> bool:
    for index in range(2):
        if (
            iv(master, f"RTU_MIX_EXPECTED_COILS{index}")
            != iv(slave, f"RTU_MIX_COILS{index}")
        ):
            return False

    for index in range(4):
        if (
            iv(master, f"RTU_MIX_EXPECTED_HOLDING{index}")
            != iv(slave, f"RTU_MIX_HOLDING{index}")
        ):
            return False

    return True


def no_reset(
    pre_master: dict[str, str],
    post_master: dict[str, str],
    pre_slave: dict[str, str],
    post_slave: dict[str, str],
    elapsed_s: float,
) -> bool:
    min_delta_ms = int(elapsed_s * 1000.0 * 0.90)

    return (
        iv(pre_master, "BOOT_MARKER") > 0
        and iv(pre_master, "BOOT_MARKER") == iv(post_master, "BOOT_MARKER")
        and iv(pre_slave, "BOOT_MARKER") > 0
        and iv(pre_slave, "BOOT_MARKER") == iv(post_slave, "BOOT_MARKER")
        and (
            iv(post_master, "UPTIME_MS")
            - iv(pre_master, "UPTIME_MS")
        ) >= min_delta_ms
        and (
            iv(post_slave, "UPTIME_MS")
            - iv(pre_slave, "UPTIME_MS")
        ) >= min_delta_ms
    )


def run_case(
    root: Path,
    master,
    slave,
    host: str,
    target: float | None,
    duration_s: float,
) -> dict[str, object]:
    label = "TCP_OFF" if target is None else f"TCP_{int(target)}"
    case_root = root / label
    case_root.mkdir(parents=True, exist_ok=True)

    send(master, b"X\n", p5b.MASTER_STOP_ACK, 6.0)
    q.wait_server_disconnected(master, timeout_s=20.0)

    p5b.reset_slave_stats(slave, 3.0)
    q.reset_stats(master)

    pre_master = q.request_snapshot(master, echo=False)
    pre_slave = p5b.request_slave_snapshot(slave, 5.0)

    write_snapshot(case_root / "master_pre.log", pre_master)
    write_snapshot(case_root / "slave_pre.log", pre_slave)

    if not fast_profile_pass(pre_master, pre_slave):
        raise RuntimeError(f"R4_FAST_PROFILE_DRIFT_{label}")

    send(master, b"U\n", "RTU_RATE_MODE=UNPACED")
    send(master, b"G\n", p5b.MASTER_START_ACK)

    print(
        f"CASE_BEGIN={label} DURATION_S={duration_s:.0f} "
        f"RTU=EXP_MIX_2222_UNPACED",
        flush=True,
    )

    if target is None:
        tcp = zero_tcp(duration_s)
    else:
        tcp = budget.run_paced_fc03(
            host,
            502,
            duration_s,
            target,
            125,
        )

    send(master, b"X\n", p5b.MASTER_STOP_ACK, 8.0)
    time.sleep(0.10)

    post_master = q.request_snapshot(master, echo=False)
    post_slave = p5b.request_slave_snapshot(slave, 5.0)

    write_snapshot(case_root / "master_final.log", post_master)
    write_snapshot(case_root / "slave_final.log", post_slave)

    duration_ms = iv(post_master, "RTU_TRAFFIC_DURATION_MS", 0)
    started = iv(post_master, "RTU_REQUESTS_STARTED", 0)
    completed = iv(post_master, "RTU_REQUESTS_COMPLETED", 0)
    success = iv(post_master, "RTU_REQUESTS_SUCCESS", 0)
    failed = iv(post_master, "RTU_REQUESTS_FAILED", 0)
    rejected = iv(post_master, "RTU_REQUESTS_REJECTED", 0)
    verify = iv(post_master, "RTU_VERIFY_FAILS", 0)
    scans = iv(post_master, "RTU_MIX_SCANS", 0)

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

    mix_success = sum(
        iv(post_master, key, 0)
        for key in (
            "RTU_MIX_DI_SUCCESS",
            "RTU_MIX_DO_SUCCESS",
            "RTU_MIX_AI_SUCCESS",
            "RTU_MIX_AO_SUCCESS",
        )
    )
    mix_failed = sum(
        iv(post_master, key, 0)
        for key in (
            "RTU_MIX_DI_FAILED",
            "RTU_MIX_DO_FAILED",
            "RTU_MIX_AI_FAILED",
            "RTU_MIX_AO_FAILED",
        )
    )

    slave_rx = iv(post_slave, "RTU_RX_FRAMES", 0)
    slave_tx = iv(post_slave, "RTU_TX_FRAMES", 0)
    slave_ok = iv(post_slave, "RTU_REQUESTS_OK", 0)

    rtu_clean = (
        started == completed == success == mix_success
        and failed == 0
        and mix_failed == 0
        and rejected == 0
        and verify == 0
        and success == scans * 8
        and iv(post_master, "RTU_MIX_DI_SUCCESS", 0) == scans * 2
        and iv(post_master, "RTU_MIX_DO_SUCCESS", 0) == scans * 2
        and iv(post_master, "RTU_MIX_AI_SUCCESS", 0) == scans * 2
        and iv(post_master, "RTU_MIX_AO_SUCCESS", 0) == scans * 2
        and iv(post_master, "RTU_CRC_ERRORS", 0) == 0
        and iv(post_master, "RTU_MASTER_TIMEOUTS", 0) == 0
        and iv(post_slave, "RTU_CRC_ERRORS", 0) == 0
        and iv(post_slave, "RTU_EXCEPTIONS_SENT", 0) == 0
        and slave_rx == success
        and slave_tx == success
        and slave_ok == success
        and output_maps_exact(post_master, post_slave)
    )

    if target is None:
        tcp_clean = (
            iv(post_master, "REQUESTS_OK", 0) == 0
            and iv(post_master, "FRAME_TIMEOUTS", 0) == 0
            and iv(post_master, "BUS_LOCK_TIMEOUTS", 0) == 0
            and iv(post_master, "PROTOCOL_ERRORS", 0) == 0
        )
        target_pass = True
    else:
        tcp_clean = (
            int(tcp["timeouts"]) == 0
            and int(tcp["transport_errors"]) == 0
            and int(tcp["protocol_errors"]) == 0
            and iv(post_master, "FRAME_TIMEOUTS", 0) == 0
            and iv(post_master, "BUS_LOCK_TIMEOUTS", 0) == 0
            and iv(post_master, "PROTOCOL_ERRORS", 0) == 0
            and iv(post_master, "REQUESTS_OK", 0) == int(tcp["ok"])
        )
        target_pass = float(tcp["target_pct"]) >= 99.0

    peripheral_clean = (
        sv(post_master, "FULL_RUNTIME_READY") == "YES"
        and sv(post_master, "DISPLAY_READY") == "YES"
        and sv(post_master, "FRAM_READY") == "YES"
        and sv(post_master, "SD_READY") == "YES"
        and sv(post_master, "RTC_PRESENT") == "YES"
        and sv(post_master, "IO_INITIALIZED") == "YES"
        and sv(post_master, "BUTTONS_READY") == "YES"
        and iv(post_master, "PERIPHERAL_FAILURE_COUNT", 0) == 0
        and iv(post_master, "SD_DATALOG_FAILED_COMMITS", 0) == 0
        and iv(post_master, "SPI_PROBE_FAILS", 0) == 0
        and sv(post_slave, "DISPLAY_READY") == "YES"
    )

    reset_clean = no_reset(
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
        and fast_profile_pass(post_master, post_slave)
    )

    classification = (
        "PASS"
        if runtime_clean and target_pass
        else "SATURATION_FAIL_CLEAN"
        if runtime_clean and not target_pass
        else "PRODUCT_FAILURE"
    )

    row: dict[str, object] = {
        "case": label,
        "tcp_target_req_s": 0.0 if target is None else target,
        "tcp_req_s": float(tcp["achieved_req_s"]),
        "tcp_target_pct": float(tcp["target_pct"]),
        "tcp_avg_us": float(tcp["avg_us"]),
        "tcp_p95_us": float(tcp["p95_us"]),
        "tcp_p99_us": float(tcp["p99_us"]),
        "tcp_max_us": float(tcp["max_us"]),
        "rtu_req_s": rtu_hz,
        "rtu_scans_s": scan_hz,
        "rtu_scan_period_ms": scan_period_ms,
        "rtu_scans": scans,
        "rtu_total_success": success,
        "rtu_transaction_max_us":
            iv(post_master, "RTU_TRANSACTION_MAX_US", 0),
        "loop_gap_avg_us":
            iv(post_master, "LOOP_GAP_AVG_US", 0),
        "loop_gap_max_us":
            iv(post_master, "LOOP_GAP_MAX_US", 0),
        "spi_probe_max_wait_us":
            iv(post_master, "SPI_PROBE_MAX_WAIT_US", 0),
        "spi_probe_over_1ms":
            iv(post_master, "SPI_PROBE_OVER_1MS", 0),
        "spi_probe_over_10ms":
            iv(post_master, "SPI_PROBE_OVER_10MS", 0),
        "peripheral_failures":
            iv(post_master, "PERIPHERAL_FAILURE_COUNT", 0),
        "rtu_clean": rtu_clean,
        "tcp_clean": tcp_clean,
        "peripheral_clean": peripheral_clean,
        "no_reset": reset_clean,
        "tcp_target_pass": target_pass,
        "classification": classification,
    }

    (case_root / "result.json").write_text(
        json.dumps(row, indent=2),
        encoding="utf-8",
    )

    print(
        f"CASE_END={label} "
        f"TCP={row['tcp_req_s']:.3f} "
        f"RTU={rtu_hz:.3f} "
        f"SCANS={scan_hz:.3f} "
        f"SCAN_MS={scan_period_ms:.3f} "
        f"CLASS={classification}",
        flush=True,
    )

    if classification == "PRODUCT_FAILURE":
        raise RuntimeError(f"R4_PRODUCT_FAILURE_{label}")

    return row


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--master-serial", default="COM14")
    parser.add_argument("--slave-serial", default="COM4")
    parser.add_argument("--duration", type=float, default=300.0)
    parser.add_argument("--output-root", required=True)
    args = parser.parse_args()

    if args.duration < 300.0:
        raise ValueError("R4 requiere >=300 s por caso")

    root = Path(args.output_root)
    root.mkdir(parents=True, exist_ok=True)

    master = p5b.open_serial_no_dtr(args.master_serial)
    slave = p5b.open_serial_no_dtr(args.slave_serial)

    rows: list[dict[str, object]] = []

    try:
        time.sleep(1.0)
        host, boot_master, boot_slave = wait_ready(master, slave)

        write_snapshot(root / "master_boot.log", boot_master)
        write_snapshot(root / "slave_boot.log", boot_slave)
        (root / "dut_ip.txt").write_text(host + "\n", encoding="utf-8")

        configure_fast(master, slave)

        fast_master = q.request_snapshot(master, echo=False)
        fast_slave = p5b.request_slave_snapshot(slave, 5.0)

        write_snapshot(root / "master_fast_profile.log", fast_master)
        write_snapshot(root / "slave_fast_profile.log", fast_slave)

        if not fast_profile_pass(fast_master, fast_slave):
            raise RuntimeError("R4_FAST_PROFILE_PREFLIGHT_FAIL")

        for target in TARGETS:
            rows.append(
                run_case(
                    root,
                    master,
                    slave,
                    host,
                    target,
                    args.duration,
                )
            )

    finally:
        master.close()
        slave.close()

    with (root / "TCP_RTU_EXPMIX_MATRIX.csv").open(
        "w",
        newline="",
        encoding="utf-8",
    ) as handle:
        writer = csv.DictWriter(
            handle,
            fieldnames=list(rows[0].keys()),
        )
        writer.writeheader()
        writer.writerows(rows)

    status = (
        "PASS_CHARACTERIZED"
        if all(row["classification"] != "PRODUCT_FAILURE" for row in rows)
        else "FAIL"
    )

    (root / "result.json").write_text(
        json.dumps(
            {
                "workload": "2DI_2DO_2AI_2AO",
                "cases": rows,
                "status": status,
            },
            indent=2,
        ),
        encoding="utf-8",
    )

    print(f"A14_R4_EXPMIX_MATRIX={status}")

    return 0 if status == "PASS_CHARACTERIZED" else 2


if __name__ == "__main__":
    raise SystemExit(main())
