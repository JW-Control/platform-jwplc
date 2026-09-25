from __future__ import annotations

import argparse
import math
import sys
import time
from pathlib import Path

import serial

THIS_DIR = Path(__file__).resolve().parent
sys.path.insert(0, str(THIS_DIR))

import a14_perf_fc03_qualification_sweep as q
import a14_perf_full_runtime_realistic_1000rps_qualification as qual


SLAVE_SNAPSHOT_END = "A14_P5_SLAVE_SNAPSHOT=END"
SLAVE_RESET_ACK = "A14_P5_SLAVE_RESET=PASS"


def open_serial_no_dtr(port: str) -> serial.Serial:
    ser = serial.Serial()
    ser.port = port
    ser.baudrate = 115200
    ser.timeout = 0.05
    ser.write_timeout = 1.0
    ser.dtr = False
    ser.rts = False
    ser.open()
    return ser


def read_line(ser: serial.Serial) -> str | None:
    raw = ser.readline()
    if not raw:
        return None
    return raw.decode("utf-8", errors="replace").strip()


def collect_until(
    ser: serial.Serial,
    end_marker: str,
    timeout_s: float,
) -> dict[str, str]:
    values: dict[str, str] = {}
    raw_lines: list[str] = []
    deadline = time.monotonic() + timeout_s

    while time.monotonic() < deadline:
        line = read_line(ser)
        if not line:
            continue

        raw_lines.append(line)

        if "=" in line:
            key, value = line.split("=", 1)
            values[key.strip()] = value.strip()

        if line == end_marker:
            values["_RAW"] = "\n".join(raw_lines)
            return values

    values["_RAW"] = "\n".join(raw_lines)
    raise TimeoutError(
        f"snapshot incompleto; esperado {end_marker}; "
        f"recibido={values['_RAW']!r}"
    )


def request_slave_snapshot(
    ser: serial.Serial,
    timeout_s: float = 5.0,
) -> dict[str, str]:
    ser.reset_input_buffer()
    ser.write(b"S\n")
    ser.flush()
    return collect_until(
        ser,
        SLAVE_SNAPSHOT_END,
        timeout_s,
    )


def reset_slave_stats(
    ser: serial.Serial,
    timeout_s: float = 3.0,
) -> None:
    ser.reset_input_buffer()
    ser.write(b"R\n")
    ser.flush()

    deadline = time.monotonic() + timeout_s
    while time.monotonic() < deadline:
        line = read_line(ser)
        if line == SLAVE_RESET_ACK:
            return

    raise TimeoutError("slave reset estadistico sin ACK")


def int_value(
    values: dict[str, str],
    key: str,
    default: int = -1,
) -> int:
    try:
        return int(values.get(key, str(default)))
    except ValueError:
        return default


def save_snapshot(
    path: Path,
    values: dict[str, str],
) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)

    with path.open(
        "w",
        encoding="utf-8",
        newline="\n",
    ) as f:
        for key, value in values.items():
            if key == "_RAW":
                continue
            f.write(f"{key}={value}\n")


def load_snapshot(path: Path) -> dict[str, str]:
    values: dict[str, str] = {}

    with path.open(
        "r",
        encoding="utf-8",
    ) as f:
        for raw in f:
            line = raw.strip()
            if "=" not in line:
                continue
            key, value = line.split("=", 1)
            values[key.strip()] = value.strip()

    return values


def slave_initial_pass(
    values: dict[str, str],
) -> bool:
    return (
        values.get("SLAVE_READY") == "YES"
        and values.get("RTU_READY") == "YES"
        and values.get("RTU_ROLE") == "SLAVE"
        and values.get("RTU_SLAVE_ID") == "2"
        and values.get("RTU_BAUD") == "115200"
        and values.get("SLAVE_HR1") == str(0x55AA)
        and values.get("DISPLAY_READY") == "YES"
        and values.get("DISPLAY_RENDER_MODE")
        == "HMI_ON_DEMAND_DIRTY"
        and values.get("DISPLAY_REFRESH_MODE")
        == "USER_REFRESH_ON_DEMAND"
    )


def main() -> int:
    parser = argparse.ArgumentParser()

    parser.add_argument(
        "--master-serial",
        default="COM14",
    )
    parser.add_argument(
        "--slave-serial",
        default="COM4",
    )
    parser.add_argument(
        "--host",
        required=True,
    )
    parser.add_argument(
        "--port",
        type=int,
        default=502,
    )
    parser.add_argument(
        "--rate",
        type=float,
        default=1000.0,
    )
    parser.add_argument(
        "--duration",
        type=float,
        default=60.0,
    )
    parser.add_argument(
        "--csv",
        required=True,
    )
    parser.add_argument(
        "--master-snapshot-out",
        required=True,
    )
    parser.add_argument(
        "--slave-snapshot-out",
        required=True,
    )

    args = parser.parse_args()

    master_snapshot_path = Path(
        args.master_snapshot_out
    )
    slave_snapshot_path = Path(
        args.slave_snapshot_out
    )

    print()
    print("=" * 76)
    print(
        " A14 P5-B MASTER/SLAVE COMBINED QUALIFICATION"
    )
    print("=" * 76)
    print(f"MASTER_SERIAL={args.master_serial}")
    print(f"SLAVE_SERIAL={args.slave_serial}")
    print(f"TARGET={args.host}:{args.port}")
    print(f"TCP_TARGET_REQ_S={args.rate:.0f}")
    print(f"DURATION_S={args.duration:.0f}")
    print("RTU_TARGET_HZ=50")
    print("RTU_SLAVE_ID=2")
    print("RTU_BAUD=115200")
    print("RTU_CONFIG=8N1")
    print("SYNC_RESET=MASTER_THEN_SLAVE_BEFORE_MEASUREMENT")

    slave_ser = open_serial_no_dtr(
        args.slave_serial
    )

    original_reset_stats = q.reset_stats

    try:
        time.sleep(2.0)

        initial_slave = request_slave_snapshot(
            slave_ser,
            5.0,
        )

        print()
        print("=== SLAVE INITIAL PREFLIGHT ===")
        print(initial_slave.get("_RAW", ""))

        initial_slave_ok = slave_initial_pass(
            initial_slave
        )

        print(
            "P5B_SLAVE_INITIAL_READY="
            f"{'YES' if initial_slave_ok else 'NO'}"
        )

        if not initial_slave_ok:
            print(
                "A14_P5B_AUTOMATED=FAIL_SLAVE_PREFLIGHT"
            )
            return 2

        sync_reset_count = 0

        def combined_reset(
            master_ser: serial.Serial,
        ) -> None:
            nonlocal sync_reset_count

            original_reset_stats(
                master_ser
            )

            reset_slave_stats(
                slave_ser,
                3.0,
            )

            sync_reset_count += 1

            print(
                "P5B_SYNC_RESET="
                f"PASS COUNT={sync_reset_count}"
            )

        q.reset_stats = combined_reset

        previous_argv = sys.argv[:]

        sys.argv = [
            str(
                THIS_DIR
                / "a14_perf_full_runtime_realistic_1000rps_qualification.py"
            ),
            "--serial",
            args.master_serial,
            "--baud",
            "115200",
            "--host",
            args.host,
            "--port",
            str(args.port),
            "--rate",
            str(args.rate),
            "--duration",
            str(args.duration),
            "--csv",
            args.csv,
            "--snapshot-out",
            str(master_snapshot_path),
        ]

        try:
            qual_rc = qual.main()
        finally:
            sys.argv = previous_argv

        print(
            f"P5B_TCP_FULL_RUNTIME_QUAL_RC={qual_rc}"
        )
        print(
            f"P5B_SYNC_RESET_COUNT={sync_reset_count}"
        )

        time.sleep(0.15)

        final_slave = request_slave_snapshot(
            slave_ser,
            5.0,
        )

        save_snapshot(
            slave_snapshot_path,
            final_slave,
        )

    finally:
        q.reset_stats = original_reset_stats

        if slave_ser.is_open:
            slave_ser.close()

    if not master_snapshot_path.exists():
        print(
            "A14_P5B_AUTOMATED="
            "FAIL_MASTER_SNAPSHOT_MISSING"
        )
        return 2

    master = load_snapshot(
        master_snapshot_path
    )

    slave = load_snapshot(
        slave_snapshot_path
    )

    print()
    print("=" * 76)
    print(" MASTER FINAL RTU SNAPSHOT")
    print("=" * 76)

    for key in (
        "RTU_READY",
        "RTU_ROLE",
        "RTU_TARGET_SLAVE_ID",
        "RTU_TRAFFIC_ENABLED",
        "RTU_TRAFFIC_DURATION_MS",
        "RTU_REQUESTS_STARTED",
        "RTU_REQUESTS_REJECTED",
        "RTU_REQUESTS_COMPLETED",
        "RTU_REQUESTS_SUCCESS",
        "RTU_REQUESTS_FAILED",
        "RTU_VERIFY_FAILS",
        "RTU_PERIODS_SKIPPED",
        "RTU_RX_FRAMES",
        "RTU_TX_FRAMES",
        "RTU_CRC_ERRORS",
        "RTU_MASTER_TIMEOUTS",
        "RTU_LAST_ERROR",
        "RTU_SERVICE_GAP_MAX_US",
        "DISPLAY_RENDER_MODE",
        "DISPLAY_REFRESH_MODE",
        "SD_READY",
        "PERIPHERAL_FAILURE_COUNT",
    ):
        print(
            f"{key}={master.get(key, 'MISSING')}"
        )

    print()
    print("=" * 76)
    print(" SLAVE FINAL SNAPSHOT")
    print("=" * 76)

    for key in (
        "SLAVE_READY",
        "RTU_READY",
        "RTU_ROLE",
        "RTU_SLAVE_ID",
        "RTU_BAUD",
        "RTU_RX_FRAMES",
        "RTU_TX_FRAMES",
        "RTU_REQUESTS_OK",
        "RTU_CRC_ERRORS",
        "RTU_EXCEPTIONS_SENT",
        "RTU_LAST_ERROR",
        "SLAVE_HR0",
        "SLAVE_HR1",
        "DISPLAY_READY",
        "DISPLAY_RENDER_MODE",
        "DISPLAY_REFRESH_MODE",
        "DISPLAY_SERVICE_GAP_MAX_MS",
    ):
        print(
            f"{key}={slave.get(key, 'MISSING')}"
        )

    started = int_value(
        master,
        "RTU_REQUESTS_STARTED",
    )
    rejected = int_value(
        master,
        "RTU_REQUESTS_REJECTED",
    )
    completed = int_value(
        master,
        "RTU_REQUESTS_COMPLETED",
    )
    success = int_value(
        master,
        "RTU_REQUESTS_SUCCESS",
    )
    failed = int_value(
        master,
        "RTU_REQUESTS_FAILED",
    )
    verify_fails = int_value(
        master,
        "RTU_VERIFY_FAILS",
    )
    periods_skipped = int_value(
        master,
        "RTU_PERIODS_SKIPPED",
    )
    master_crc = int_value(
        master,
        "RTU_CRC_ERRORS",
    )
    master_timeouts = int_value(
        master,
        "RTU_MASTER_TIMEOUTS",
    )
    duration_ms = int_value(
        master,
        "RTU_TRAFFIC_DURATION_MS",
    )

    rtu_hz = (
        success / (duration_ms / 1000.0)
        if duration_ms > 0
        else 0.0
    )

    min_started = int(
        math.floor(
            args.duration * 45.0
        )
    )

    master_rtu_pass = (
        master.get("RTU_READY") == "YES"
        and master.get("RTU_ROLE") == "MASTER"
        and master.get("RTU_TARGET_SLAVE_ID") == "2"
        and master.get("RTU_TRAFFIC_ENABLED") == "YES"
        and started >= min_started
        and rejected == 0
        and completed >= started - 1
        and completed <= started
        and success == completed
        and failed == 0
        and verify_fails == 0
        and master_crc == 0
        and master_timeouts == 0
        and master.get("RTU_LAST_ERROR") == "OK"
        and 45.0 <= rtu_hz <= 52.0
    )

    slave_rx = int_value(
        slave,
        "RTU_RX_FRAMES",
    )
    slave_tx = int_value(
        slave,
        "RTU_TX_FRAMES",
    )
    slave_ok = int_value(
        slave,
        "RTU_REQUESTS_OK",
    )
    slave_crc = int_value(
        slave,
        "RTU_CRC_ERRORS",
    )
    slave_ex = int_value(
        slave,
        "RTU_EXCEPTIONS_SENT",
    )

    tail_tolerance = 12

    rx_delta = abs(
        slave_rx - success
    )
    tx_delta = abs(
        slave_tx - success
    )
    ok_delta = abs(
        slave_ok - success
    )

    cross_count_pass = (
        rx_delta <= tail_tolerance
        and tx_delta <= tail_tolerance
        and ok_delta <= tail_tolerance
    )

    slave_rtu_pass = (
        slave.get("SLAVE_READY") == "YES"
        and slave.get("RTU_READY") == "YES"
        and slave.get("RTU_ROLE") == "SLAVE"
        and slave.get("RTU_SLAVE_ID") == "2"
        and slave.get("RTU_BAUD") == "115200"
        and slave.get("SLAVE_HR1")
        == str(0x55AA)
        and slave_rx >= min_started
        and slave_tx >= min_started
        and slave_ok >= min_started
        and slave_crc == 0
        and slave_ex == 0
        and cross_count_pass
        and slave.get("DISPLAY_READY") == "YES"
        and slave.get("DISPLAY_RENDER_MODE")
        == "HMI_ON_DEMAND_DIRTY"
        and slave.get("DISPLAY_REFRESH_MODE")
        == "USER_REFRESH_ON_DEMAND"
    )

    master_runtime_pass = (
        master.get("FULL_RUNTIME_READY") == "YES"
        and master.get("COMBINED_RUNTIME_READY") == "YES"
        and master.get("SERVER_READY") == "YES"
        and master.get("ETH_READY") == "YES"
        and master.get("ETH_LINK") == "UP"
        and master.get("SD_READY") == "YES"
        and master.get("PERIPHERAL_FAILURE_COUNT") == "0"
        and master.get("DISPLAY_RENDER_MODE")
        == "HMI_ON_DEMAND_DIRTY"
        and master.get("DISPLAY_REFRESH_MODE")
        == "USER_REFRESH_ON_DEMAND"
    )

    automated_pass = (
        qual_rc == 0
        and sync_reset_count == 1
        and master_rtu_pass
        and slave_rtu_pass
        and master_runtime_pass
    )

    print()
    print("=" * 76)
    print(" P5-B AUTOMATED SUMMARY")
    print("=" * 76)
    print(
        f"TCP_FULL_RUNTIME_PASS={'YES' if qual_rc == 0 else 'NO'}"
    )
    print(
        f"MASTER_RUNTIME_PASS={'YES' if master_runtime_pass else 'NO'}"
    )
    print(
        f"RTU_MASTER_PASS={'YES' if master_rtu_pass else 'NO'}"
    )
    print(
        f"RTU_SLAVE_PASS={'YES' if slave_rtu_pass else 'NO'}"
    )
    print(
        f"RTU_CROSS_COUNT_PASS={'YES' if cross_count_pass else 'NO'}"
    )
    print(
        f"RTU_SNAPSHOT_TAIL_TOLERANCE={tail_tolerance}"
    )
    print(
        f"RTU_SLAVE_RX_MASTER_SUCCESS_DELTA={rx_delta}"
    )
    print(
        f"RTU_SLAVE_TX_MASTER_SUCCESS_DELTA={tx_delta}"
    )
    print(
        f"RTU_SLAVE_OK_MASTER_SUCCESS_DELTA={ok_delta}"
    )
    print(
        f"RTU_REQUESTS_STARTED={started}"
    )
    print(
        f"RTU_REQUESTS_SUCCESS={success}"
    )
    print(
        f"RTU_PERIODS_SKIPPED={periods_skipped}"
    )
    print(
        "RTU_PERIODS_SKIPPED_GATE=NO"
    )
    print(
        f"RTU_ACHIEVED_HZ={rtu_hz:.3f}"
    )
    print(
        f"MASTER_SNAPSHOT_FILE={master_snapshot_path}"
    )
    print(
        f"SLAVE_SNAPSHOT_FILE={slave_snapshot_path}"
    )
    print(
        "A14_P5B_AUTOMATED="
        f"{'PASS' if automated_pass else 'REVIEW'}"
    )

    return 0 if automated_pass else 1


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception as exc:
        print()
        print(
            "P5B_FATAL="
            f"{type(exc).__name__}: {exc}"
        )
        print(
            "A14_P5B_AUTOMATED=FAIL_HARNESS"
        )
        raise SystemExit(2)
