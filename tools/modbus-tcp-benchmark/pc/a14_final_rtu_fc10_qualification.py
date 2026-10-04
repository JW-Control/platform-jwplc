from __future__ import annotations

import argparse
import json
import time
from pathlib import Path

import serial

MASTER_END = "A14_RTU_FC10_MASTER_SNAPSHOT=END"
SLAVE_END = "A14_RTU_FC10_SLAVE_SNAPSHOT=END"


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
    timeout_s: float = 3.0,
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
    timeout_s: float = 4.0,
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


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--master-serial", default="COM14")
    parser.add_argument("--slave-serial", default="COM4")
    parser.add_argument("--duration", type=float, default=600.0)
    parser.add_argument("--output-root", required=True)
    args = parser.parse_args()

    if args.duration < 600.0:
        raise ValueError("R-FC10 final requiere >=600 s")

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
            "A14_RTU_FC10_SLAVE_RESET=PASS",
        )
        command_wait(
            master,
            b"R",
            "A14_RTU_FC10_MASTER_RESET=PASS",
        )
        command_wait(
            master,
            b"G",
            "A14_RTU_FC10_MASTER_START=PASS",
        )

        start = time.perf_counter()
        time.sleep(args.duration)
        elapsed = time.perf_counter() - start

        command_wait(
            master,
            b"X",
            "A14_RTU_FC10_MASTER_STOP=PASS",
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

    quantities = (1, 2, 4, 8, 16, 32, 64, 123)

    checks = {
        "master_ready":
            post_master.get("MASTER_READY") == "YES",
        "boundary_selftest":
            post_master.get("FC10_BOUNDARY_SELFTEST") == "PASS",
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
        "fc10_activity":
            iv(post_master, "FC10_SUCCESS", 0) > 0,
        "fc10_no_fail":
            iv(post_master, "FC10_FAILED", 0) == 0,
        "fc03_no_fail":
            iv(post_master, "FC03_FAILED", 0) == 0,
        "no_reject":
            iv(post_master, "REQUEST_REJECTED", 0) == 0,
        "no_verify_fail":
            iv(post_master, "VERIFY_FAILS", 0) == 0,
        "master_crc0":
            iv(post_master, "RTU_CRC_ERRORS", 0) == 0,
        "master_timeout0":
            iv(post_master, "RTU_MASTER_TIMEOUTS", 0) == 0,
        "slave_crc0":
            iv(post_slave, "RTU_CRC_ERRORS", 0) == 0,
        "slave_exception0":
            iv(post_slave, "RTU_EXCEPTIONS_SENT", 0) == 0,
    }

    for quantity in quantities:
        checks[f"q{quantity}_covered"] = (
            iv(post_master, f"Q{quantity}_PASSES", 0) > 0
        )

    total_success = (
        iv(post_master, "FC10_SUCCESS", 0)
        + iv(post_master, "FC03_SUCCESS", 0)
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

    result = {
        "duration_s": elapsed,
        "fc10_started":
            iv(post_master, "FC10_STARTED", 0),
        "fc10_success":
            iv(post_master, "FC10_SUCCESS", 0),
        "fc03_success":
            iv(post_master, "FC03_SUCCESS", 0),
        "cycles":
            iv(post_master, "CYCLES", 0),
        "transaction_max_us":
            iv(post_master, "TRANSACTION_MAX_US", 0),
        "master_requests_ok":
            iv(post_master, "RTU_REQUESTS_OK", 0),
        "slave_requests_ok":
            iv(post_slave, "RTU_REQUESTS_OK", 0),
        "checks": checks,
        "status":
            "PASS"
            if all(checks.values())
            else "FAIL",
    }

    (root / "result.json").write_text(
        json.dumps(result, indent=2),
        encoding="utf-8",
    )

    for key, value in checks.items():
        print(
            f"CHECK_{key.upper()}="
            f"{'PASS' if value else 'FAIL'}"
        )

    print(f"FC10_SUCCESS={result['fc10_success']}")
    print(f"FC03_SUCCESS={result['fc03_success']}")
    print(f"CYCLES={result['cycles']}")
    print(
        f"TRANSACTION_MAX_US="
        f"{result['transaction_max_us']}"
    )
    print(
        f"A14_RTU_FC10_FINAL="
        f"{result['status']}"
    )

    return 0 if result["status"] == "PASS" else 2


if __name__ == "__main__":
    raise SystemExit(main())
