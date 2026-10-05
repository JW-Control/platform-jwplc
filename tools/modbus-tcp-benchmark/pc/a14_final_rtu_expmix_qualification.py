from __future__ import annotations

import argparse
import csv
import json
import time
from pathlib import Path

import serial

MASTER_END = "A14_RTU_EXPMIX_MASTER_SNAPSHOT=END"
SLAVE_END = "A14_RTU_EXPMIX_SLAVE_SNAPSHOT=END"

PATTERNS = (
    {
        "name": "3DI_3DO_1AI_1AO",
        "command": b"A",
        "di_per_scan": 3,
        "do_per_scan": 3,
        "ai_per_scan": 1,
        "ao_per_scan": 1,
    },
    {
        "name": "2DI_2DO_2AI_2AO",
        "command": b"B",
        "di_per_scan": 2,
        "do_per_scan": 2,
        "ai_per_scan": 2,
        "ao_per_scan": 2,
    },
)


def open_serial(port: str) -> serial.Serial:
    ser = serial.Serial()
    ser.port = port
    ser.baudrate = 115200
    ser.timeout = 0.05
    ser.write_timeout = 1.0
    ser.dtr = False
    ser.rts = False
    ser.open()
    time.sleep(1.0)
    return ser


def command_wait(
    ser: serial.Serial,
    command: bytes,
    token: str,
    timeout_s: float = 5.0,
) -> None:
    ser.reset_input_buffer()
    ser.write(command)
    ser.flush()

    deadline = time.perf_counter() + timeout_s
    raw = bytearray()

    while time.perf_counter() < deadline:
        chunk = ser.read(max(1, ser.in_waiting))
        if chunk:
            raw.extend(chunk)
            if token.encode() in raw:
                return

    raise TimeoutError(
        f"ACK_TIMEOUT token={token} raw={raw.decode(errors='replace')}"
    )


def snapshot(
    ser: serial.Serial,
    end_token: str,
    timeout_s: float = 5.0,
) -> dict[str, str]:
    ser.reset_input_buffer()
    ser.write(b"S")
    ser.flush()

    deadline = time.perf_counter() + timeout_s
    raw = bytearray()

    while time.perf_counter() < deadline:
        chunk = ser.read(max(1, ser.in_waiting))
        if chunk:
            raw.extend(chunk)
            if end_token.encode() in raw:
                break

    text = raw.decode("utf-8", errors="replace")

    if end_token not in text:
        raise TimeoutError(f"SNAPSHOT_TIMEOUT {end_token}\n{text}")

    values: dict[str, str] = {"_RAW": text}

    for line in text.splitlines():
        if "=" in line:
            key, value = line.split("=", 1)
            values[key.strip()] = value.strip()

    return values


def iv(values: dict[str, str], key: str, default: int = -1) -> int:
    try:
        return int(values.get(key, str(default)))
    except ValueError:
        return default


def check_profile(
    master: dict[str, str],
    slave: dict[str, str],
) -> dict[str, bool]:
    return {
        "master_ready":
            master.get("MASTER_READY") == "YES",
        "slave_ready":
            slave.get("SLAVE_READY") == "YES",
        "master_500k":
            iv(master, "RTU_BAUD_EFFECTIVE") == 500000,
        "slave_500k":
            iv(slave, "RTU_BAUD_EFFECTIVE") == 500000,
        "master_fifo9":
            iv(master, "RTU_RX_FIFO_FULL") == 9,
        "slave_fifo8":
            iv(slave, "RTU_RX_FIFO_FULL") == 8,
        "master_bulk":
            master.get("RTU_RX_MODE") == "BULK",
        "slave_bulk":
            slave.get("RTU_RX_MODE") == "BULK",
        "master_queued":
            master.get("RTU_TX_MODE") == "QUEUED",
        "slave_queued":
            slave.get("RTU_TX_MODE") == "QUEUED",
        "master_gap":
            master.get("RTU_SERVER_FRAMING") == "GAP",
        "slave_structural":
            slave.get("RTU_SERVER_FRAMING") == "STRUCTURAL",
    }


def run_pattern(
    root: Path,
    master: serial.Serial,
    slave: serial.Serial,
    pattern: dict[str, object],
    duration_s: float,
) -> dict[str, object]:
    name = str(pattern["name"])
    case_root = root / name
    case_root.mkdir(parents=True, exist_ok=True)

    pre_master = snapshot(master, MASTER_END)
    pre_slave = snapshot(slave, SLAVE_END)

    (case_root / "master_pre.log").write_text(
        pre_master["_RAW"],
        encoding="utf-8",
    )
    (case_root / "slave_pre.log").write_text(
        pre_slave["_RAW"],
        encoding="utf-8",
    )

    command_wait(
        slave,
        b"R",
        "A14_RTU_EXPMIX_SLAVE_RESET=PASS",
    )
    command_wait(
        master,
        b"R",
        "A14_RTU_EXPMIX_MASTER_RESET=PASS",
    )
    command_wait(
        master,
        pattern["command"],
        "A14_RTU_EXPMIX_MASTER_START=PASS",
    )

    start = time.perf_counter()
    time.sleep(duration_s)
    elapsed = time.perf_counter() - start

    command_wait(
        master,
        b"X",
        "A14_RTU_EXPMIX_MASTER_STOP=PASS",
        timeout_s=6.0,
    )
    time.sleep(0.10)

    post_master = snapshot(master, MASTER_END)
    post_slave = snapshot(slave, SLAVE_END)

    (case_root / "master_final.log").write_text(
        post_master["_RAW"],
        encoding="utf-8",
    )
    (case_root / "slave_final.log").write_text(
        post_slave["_RAW"],
        encoding="utf-8",
    )

    checks = check_profile(post_master, post_slave)

    checks["pattern"] = (
        post_master.get("PATTERN") == name
    )

    scans = iv(post_master, "SCANS", 0)
    checks["scans"] = scans > 0

    type_specs = (
        ("DI", int(pattern["di_per_scan"])),
        ("DO", int(pattern["do_per_scan"])),
        ("AI", int(pattern["ai_per_scan"])),
        ("AO", int(pattern["ao_per_scan"])),
    )

    total_success = 0

    for prefix, per_scan in type_specs:
        started = iv(post_master, f"{prefix}_STARTED", 0)
        success = iv(post_master, f"{prefix}_SUCCESS", 0)
        failed = iv(post_master, f"{prefix}_FAILED", 0)

        checks[f"{prefix.lower()}_exact"] = (
            started == success
            and failed == 0
            and success == scans * per_scan
        )

        total_success += success

    checks["eight_transactions_per_scan"] = (
        total_success == scans * 8
    )

    checks["verify0"] = (
        iv(post_master, "VERIFY_FAILS", 0) == 0
    )
    checks["rejected0"] = (
        iv(post_master, "REQUEST_REJECTED", 0) == 0
    )
    checks["master_crc0"] = (
        iv(post_master, "RTU_CRC_ERRORS", 0) == 0
    )
    checks["master_timeout0"] = (
        iv(post_master, "RTU_MASTER_TIMEOUTS", 0) == 0
    )
    checks["slave_crc0"] = (
        iv(post_slave, "RTU_CRC_ERRORS", 0) == 0
    )
    checks["slave_exception0"] = (
        iv(post_slave, "RTU_EXCEPTIONS_SENT", 0) == 0
    )

    checks["master_stats_exact"] = (
        iv(post_master, "RTU_REQUESTS_OK", 0)
        == total_success
    )

    checks["slave_cross_count"] = (
        iv(post_slave, "RTU_REQUESTS_OK", 0)
        == total_success
        and iv(post_slave, "RTU_RX_FRAMES", 0)
        == total_success
        and iv(post_slave, "RTU_TX_FRAMES", 0)
        == total_success
    )

    map_ok = True

    for index in range(3):
        expected = iv(post_master, f"EXPECTED_COILS{index}")
        actual = iv(post_slave, f"COILS{index}")
        if expected != actual:
            map_ok = False

    for index in range(4):
        expected = iv(post_master, f"EXPECTED_HOLDING{index}")
        actual = iv(post_slave, f"HOLDING{index}")
        if expected != actual:
            map_ok = False

    checks["final_output_maps_exact"] = map_ok

    pre_master_boot = iv(pre_master, "BOOT_MARKER")
    pre_slave_boot = iv(pre_slave, "BOOT_MARKER")

    checks["master_no_reset"] = (
        pre_master_boot > 0
        and pre_master_boot == iv(post_master, "BOOT_MARKER")
        and (
            iv(post_master, "UPTIME_MS")
            - iv(pre_master, "UPTIME_MS")
        )
        >= int(elapsed * 900.0)
    )

    checks["slave_no_reset"] = (
        pre_slave_boot > 0
        and pre_slave_boot == iv(post_slave, "BOOT_MARKER")
        and (
            iv(post_slave, "UPTIME_MS")
            - iv(pre_slave, "UPTIME_MS")
        )
        >= int(elapsed * 900.0)
    )

    request_rate = (
        total_success / elapsed
        if elapsed > 0.0
        else 0.0
    )

    scan_rate = (
        scans / elapsed
        if elapsed > 0.0
        else 0.0
    )

    scan_period_ms = (
        1000.0 / scan_rate
        if scan_rate > 0.0
        else 0.0
    )

    row: dict[str, object] = {
        "pattern": name,
        "duration_s": elapsed,
        "scans": scans,
        "scan_rate_hz": scan_rate,
        "scan_period_ms": scan_period_ms,
        "request_rate_hz": request_rate,
        "total_success": total_success,
        "di_success": iv(post_master, "DI_SUCCESS", 0),
        "do_success": iv(post_master, "DO_SUCCESS", 0),
        "ai_success": iv(post_master, "AI_SUCCESS", 0),
        "ao_success": iv(post_master, "AO_SUCCESS", 0),
        "transaction_max_us":
            iv(post_master, "TRANSACTION_MAX_US", 0),
        "verify_fails":
            iv(post_master, "VERIFY_FAILS", 0),
        "crc_master":
            iv(post_master, "RTU_CRC_ERRORS", 0),
        "crc_slave":
            iv(post_slave, "RTU_CRC_ERRORS", 0),
        "timeouts":
            iv(post_master, "RTU_MASTER_TIMEOUTS", 0),
        "exceptions":
            iv(post_slave, "RTU_EXCEPTIONS_SENT", 0),
        "status":
            "PASS"
            if all(checks.values())
            else "FAIL",
        "checks": checks,
    }

    (case_root / "result.json").write_text(
        json.dumps(row, indent=2),
        encoding="utf-8",
    )

    print(
        f"CASE={name} "
        f"SCANS={scans} "
        f"SCAN_HZ={scan_rate:.3f} "
        f"SCAN_PERIOD_MS={scan_period_ms:.3f} "
        f"REQ_HZ={request_rate:.3f} "
        f"STATUS={row['status']}"
    )

    for key, value in checks.items():
        print(
            f"CHECK_{name}_{key.upper()}="
            f"{'PASS' if value else 'FAIL'}"
        )

    return row


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--master-serial", default="COM14")
    parser.add_argument("--slave-serial", default="COM4")
    parser.add_argument("--duration", type=float, default=300.0)
    parser.add_argument("--output-root", required=True)
    args = parser.parse_args()

    if args.duration < 300.0:
        raise ValueError("R3 EXP-MIX requiere >=300 s por patrón")

    root = Path(args.output_root)
    root.mkdir(parents=True, exist_ok=True)

    master = open_serial(args.master_serial)
    slave = open_serial(args.slave_serial)

    rows: list[dict[str, object]] = []

    try:
        for pattern in PATTERNS:
            rows.append(
                run_pattern(
                    root,
                    master,
                    slave,
                    pattern,
                    args.duration,
                )
            )
    finally:
        master.close()
        slave.close()

    flat_rows = []

    for row in rows:
        flat_rows.append(
            {
                key: value
                for key, value in row.items()
                if key != "checks"
            }
        )

    with (root / "EXP_MIX_SUMMARY.csv").open(
        "w",
        newline="",
        encoding="utf-8",
    ) as handle:
        writer = csv.DictWriter(
            handle,
            fieldnames=list(flat_rows[0].keys()),
        )
        writer.writeheader()
        writer.writerows(flat_rows)

    final_status = (
        "PASS"
        if all(row["status"] == "PASS" for row in rows)
        else "FAIL"
    )

    (root / "result.json").write_text(
        json.dumps(
            {
                "patterns": rows,
                "status": final_status,
            },
            indent=2,
        ),
        encoding="utf-8",
    )

    print(f"A14_RTU_EXPMIX_FINAL={final_status}")

    return 0 if final_status == "PASS" else 2


if __name__ == "__main__":
    raise SystemExit(main())
