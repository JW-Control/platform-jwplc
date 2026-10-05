from __future__ import annotations

import argparse
import json
import time
from pathlib import Path

import serial

MASTER_END = "A14_RTU_MULTIFC_MASTER_SNAPSHOT=END"
SLAVE_END = "A14_RTU_MULTIFC_SLAVE_SNAPSHOT=END"

FUNCTION_CODES = (
    0x01, 0x02, 0x03, 0x04,
    0x05, 0x01, 0x06, 0x03,
    0x0F, 0x01, 0x10, 0x03,
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
    timeout_s: float = 4.0,
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


def function_totals(master: dict[str, str]) -> dict[str, dict[str, int]]:
    totals: dict[str, dict[str, int]] = {}

    for index, fc in enumerate(FUNCTION_CODES):
        key = f"FC{fc:02X}"
        entry = totals.setdefault(
            key,
            {"started": 0, "success": 0, "failed": 0},
        )
        entry["started"] += iv(master, f"OP{index}_STARTED", 0)
        entry["success"] += iv(master, f"OP{index}_SUCCESS", 0)
        entry["failed"] += iv(master, f"OP{index}_FAILED", 0)

    return totals


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--master-serial", default="COM14")
    parser.add_argument("--slave-serial", default="COM4")
    parser.add_argument("--duration", type=float, default=600.0)
    parser.add_argument("--output-root", required=True)
    args = parser.parse_args()

    if args.duration < 600.0:
        raise ValueError("R2 MULTI-FC final requiere >=600 s")

    root = Path(args.output_root)
    root.mkdir(parents=True, exist_ok=True)

    master = open_serial(args.master_serial)
    slave = open_serial(args.slave_serial)

    try:
        pre_master = snapshot(master, MASTER_END)
        pre_slave = snapshot(slave, SLAVE_END)

        (root / "master_pre.log").write_text(
            pre_master["_RAW"],
            encoding="utf-8",
        )
        (root / "slave_pre.log").write_text(
            pre_slave["_RAW"],
            encoding="utf-8",
        )

        command_wait(
            slave,
            b"R",
            "A14_RTU_MULTIFC_SLAVE_RESET=PASS",
        )
        command_wait(
            master,
            b"R",
            "A14_RTU_MULTIFC_MASTER_RESET=PASS",
        )
        command_wait(
            master,
            b"G",
            "A14_RTU_MULTIFC_MASTER_START=PASS",
        )

        start = time.perf_counter()
        time.sleep(args.duration)
        elapsed = time.perf_counter() - start

        command_wait(
            master,
            b"X",
            "A14_RTU_MULTIFC_MASTER_STOP=PASS",
            timeout_s=6.0,
        )
        time.sleep(0.10)

        post_master = snapshot(master, MASTER_END)
        post_slave = snapshot(slave, SLAVE_END)

        (root / "master_final.log").write_text(
            post_master["_RAW"],
            encoding="utf-8",
        )
        (root / "slave_final.log").write_text(
            post_slave["_RAW"],
            encoding="utf-8",
        )

    finally:
        master.close()
        slave.close()

    checks: dict[str, bool] = {
        "master_ready":
            post_master.get("MASTER_READY") == "YES",
        "slave_ready":
            post_slave.get("SLAVE_READY") == "YES",
        "master_500k":
            iv(post_master, "RTU_BAUD_EFFECTIVE") == 500000,
        "slave_500k":
            iv(post_slave, "RTU_BAUD_EFFECTIVE") == 500000,
        "master_fifo9":
            iv(post_master, "RTU_RX_FIFO_FULL") == 9,
        "slave_fifo8":
            iv(post_slave, "RTU_RX_FIFO_FULL") == 8,
        "master_bulk":
            post_master.get("RTU_RX_MODE") == "BULK",
        "slave_bulk":
            post_slave.get("RTU_RX_MODE") == "BULK",
        "master_queued":
            post_master.get("RTU_TX_MODE") == "QUEUED",
        "slave_queued":
            post_slave.get("RTU_TX_MODE") == "QUEUED",
        "master_gap":
            post_master.get("RTU_SERVER_FRAMING") == "GAP",
        "slave_structural":
            post_slave.get("RTU_SERVER_FRAMING") == "STRUCTURAL",
        "verify0":
            iv(post_master, "VERIFY_FAILS", 0) == 0,
        "rejected0":
            iv(post_master, "REQUEST_REJECTED", 0) == 0,
        "master_crc0":
            iv(post_master, "RTU_CRC_ERRORS", 0) == 0,
        "master_timeout0":
            iv(post_master, "RTU_MASTER_TIMEOUTS", 0) == 0,
        "slave_crc0":
            iv(post_slave, "RTU_CRC_ERRORS", 0) == 0,
        "slave_exception0":
            iv(post_slave, "RTU_EXCEPTIONS_SENT", 0) == 0,
        "cycles":
            iv(post_master, "CYCLES", 0) > 0,
    }

    total_success = 0

    for index, fc in enumerate(FUNCTION_CODES):
        started = iv(post_master, f"OP{index}_STARTED", 0)
        success = iv(post_master, f"OP{index}_SUCCESS", 0)
        failed = iv(post_master, f"OP{index}_FAILED", 0)

        checks[f"op{index}_fc{fc:02x}_covered"] = started > 0
        checks[f"op{index}_fc{fc:02x}_exact"] = (
            started == success
            and failed == 0
        )
        total_success += success

    totals = function_totals(post_master)

    for fc_name in (
        "FC01", "FC02", "FC03", "FC04",
        "FC05", "FC06", "FC0F", "FC10",
    ):
        entry = totals.get(fc_name, {})
        checks[f"{fc_name.lower()}_covered"] = (
            int(entry.get("success", 0)) > 0
        )
        checks[f"{fc_name.lower()}_clean"] = (
            int(entry.get("started", 0))
            == int(entry.get("success", 0))
            and int(entry.get("failed", 0)) == 0
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

    request_rate = total_success / elapsed if elapsed > 0.0 else 0.0

    result = {
        "duration_s": elapsed,
        "cycles": iv(post_master, "CYCLES", 0),
        "total_success": total_success,
        "request_rate_hz": request_rate,
        "transaction_max_us":
            iv(post_master, "TRANSACTION_MAX_US", 0),
        "function_totals": totals,
        "checks": checks,
        "status": "PASS" if all(checks.values()) else "FAIL",
    }

    (root / "result.json").write_text(
        json.dumps(result, indent=2),
        encoding="utf-8",
    )

    for fc_name, values in totals.items():
        print(
            f"{fc_name}_STARTED={values['started']} "
            f"{fc_name}_SUCCESS={values['success']} "
            f"{fc_name}_FAILED={values['failed']}"
        )

    print(f"CYCLES={result['cycles']}")
    print(f"TOTAL_SUCCESS={total_success}")
    print(f"REQUEST_RATE_HZ={request_rate:.3f}")
    print(
        "TRANSACTION_MAX_US="
        f"{result['transaction_max_us']}"
    )

    for key, value in checks.items():
        print(
            f"CHECK_{key.upper()}="
            f"{'PASS' if value else 'FAIL'}"
        )

    print(f"A14_RTU_MULTIFC_FINAL={result['status']}")

    return 0 if result["status"] == "PASS" else 2


if __name__ == "__main__":
    raise SystemExit(main())
