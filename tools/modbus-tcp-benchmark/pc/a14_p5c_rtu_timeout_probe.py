from __future__ import annotations

import argparse
import math
import time

import serial


MASTER_PREFLIGHT_END = "A14_P5_PREFLIGHT=END"
MASTER_RESET_ACK = "A14_PERF_RESET=PASS"
MASTER_STOP_ACK = "RTU_MASTER_TRAFFIC=OFF"

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
    return raw.decode(
        "utf-8",
        errors="replace",
    ).strip()


def send_wait_ack(
    ser: serial.Serial,
    command: bytes,
    ack: str,
    timeout_s: float,
) -> bool:
    ser.reset_input_buffer()
    ser.write(command)
    ser.flush()

    deadline = time.perf_counter() + timeout_s

    while time.perf_counter() < deadline:
        line = read_line(ser)
        if line == ack:
            return True

    return False


def collect_key_values(
    ser: serial.Serial,
    command: bytes,
    end_marker: str,
    timeout_s: float,
) -> dict[str, str]:
    ser.reset_input_buffer()
    ser.write(command)
    ser.flush()

    deadline = time.perf_counter() + timeout_s
    values: dict[str, str] = {}
    raw_lines: list[str] = []

    while time.perf_counter() < deadline:
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
        f"missing end marker {end_marker}; "
        f"raw={values['_RAW']!r}"
    )


def int_value(
    values: dict[str, str],
    key: str,
    default: int = -1,
) -> int:
    try:
        return int(values.get(key, str(default)))
    except ValueError:
        return default


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
        "--duration",
        type=float,
        default=15.0,
    )
    parser.add_argument(
        "--expected-timeout-ms",
        type=int,
        required=True,
    )

    args = parser.parse_args()

    if args.duration < 10.0:
        raise RuntimeError(
            "P5C_DURATION_TOO_SHORT"
        )

    master = open_serial_no_dtr(
        args.master_serial
    )
    slave = open_serial_no_dtr(
        args.slave_serial
    )

    try:
        time.sleep(2.0)

        master_reset = send_wait_ack(
            master,
            b"R\n",
            MASTER_RESET_ACK,
            3.0,
        )

        slave_reset = send_wait_ack(
            slave,
            b"R\n",
            SLAVE_RESET_ACK,
            3.0,
        )

        print(
            "P5C_MASTER_RESET="
            f"{'PASS' if master_reset else 'FAIL'}"
        )
        print(
            "P5C_SLAVE_RESET="
            f"{'PASS' if slave_reset else 'FAIL'}"
        )

        if not master_reset or not slave_reset:
            return 2

        measurement_start = time.perf_counter()
        deadline = measurement_start + args.duration

        while time.perf_counter() < deadline:
            time.sleep(0.05)

        master_stop = send_wait_ack(
            master,
            b"X\n",
            MASTER_STOP_ACK,
            3.0,
        )

        print(
            "P5C_MASTER_STOP="
            f"{'PASS' if master_stop else 'FAIL'}"
        )

        if not master_stop:
            return 2

        # Dar tiempo a una transacción ya iniciada para completar/expirar.
        time.sleep(0.20)

        master_values = collect_key_values(
            master,
            b"P\n",
            MASTER_PREFLIGHT_END,
            3.0,
        )

        slave_values = collect_key_values(
            slave,
            b"S\n",
            SLAVE_SNAPSHOT_END,
            5.0,
        )

    finally:
        if master.is_open:
            master.close()

        if slave.is_open:
            slave.close()

    print()
    print("=== P5C MASTER COMPACT SNAPSHOT ===")
    print(master_values.get("_RAW", ""))

    print()
    print("=== P5C SLAVE SNAPSHOT ===")
    print(slave_values.get("_RAW", ""))

    timeout_ms = int_value(
        master_values,
        "RTU_TIMEOUT_MS",
    )
    duration_ms = int_value(
        master_values,
        "RTU_TRAFFIC_DURATION_MS",
    )
    started = int_value(
        master_values,
        "RTU_REQUESTS_STARTED",
    )
    rejected = int_value(
        master_values,
        "RTU_REQUESTS_REJECTED",
    )
    completed = int_value(
        master_values,
        "RTU_REQUESTS_COMPLETED",
    )
    success = int_value(
        master_values,
        "RTU_REQUESTS_SUCCESS",
    )
    failed = int_value(
        master_values,
        "RTU_REQUESTS_FAILED",
    )
    verify_fails = int_value(
        master_values,
        "RTU_VERIFY_FAILS",
    )
    periods_skipped = int_value(
        master_values,
        "RTU_PERIODS_SKIPPED",
    )
    crc_errors = int_value(
        master_values,
        "RTU_CRC_ERRORS",
    )
    timeouts = int_value(
        master_values,
        "RTU_MASTER_TIMEOUTS",
    )
    service_gap_us = int_value(
        master_values,
        "RTU_SERVICE_GAP_MAX_US",
    )
    loop_gap_us = int_value(
        master_values,
        "LOOP_GAP_MAX_US",
    )
    fram_max_us = int_value(
        master_values,
        "FRAM_MAX_US",
    )
    sd_append_max_us = int_value(
        master_values,
        "SD_APPEND_MAX_US",
    )
    sd_verify_max_us = int_value(
        master_values,
        "SD_VERIFY_MAX_US",
    )
    peripheral_failures = int_value(
        master_values,
        "PERIPHERAL_FAILURE_COUNT",
    )

    slave_rx = int_value(
        slave_values,
        "RTU_RX_FRAMES",
    )
    slave_tx = int_value(
        slave_values,
        "RTU_TX_FRAMES",
    )
    slave_ok = int_value(
        slave_values,
        "RTU_REQUESTS_OK",
    )
    slave_crc = int_value(
        slave_values,
        "RTU_CRC_ERRORS",
    )
    slave_ex = int_value(
        slave_values,
        "RTU_EXCEPTIONS_SENT",
    )

    if duration_ms > 0:
        achieved_hz = (
            success /
            (duration_ms / 1000.0)
        )
    else:
        achieved_hz = 0.0

    service_gap_ms_ceil = (
        int(math.ceil(service_gap_us / 1000.0))
        if service_gap_us >= 0
        else 999999
    )

    timeout_headroom_ms = (
        timeout_ms -
        service_gap_ms_ceil
    )

    rx_delta = abs(slave_rx - success)
    tx_delta = abs(slave_tx - success)
    ok_delta = abs(slave_ok - success)

    cross_count_pass = (
        rx_delta <= 12
        and tx_delta <= 12
        and ok_delta <= 12
    )

    zero_error_pass = (
        rejected == 0
        and failed == 0
        and verify_fails == 0
        and crc_errors == 0
        and timeouts == 0
        and slave_crc == 0
        and slave_ex == 0
        and peripheral_failures == 0
    )

    rate_pass = (
        45.0 <= achieved_hz <= 52.0
    )

    lifecycle_pass = (
        completed >= started - 1
        and completed <= started
        and success == completed
    )

    source_timeout_pass = (
        timeout_ms ==
        args.expected_timeout_ms
    )

    readiness_pass = (
        master_values.get(
            "FULL_RUNTIME_READY"
        ) == "YES"
        and master_values.get(
            "SD_READY"
        ) == "YES"
        and master_values.get(
            "DISPLAY_READY"
        ) == "YES"
        and master_values.get(
            "DISPLAY_RENDER_MODE"
        ) == "HMI_ON_DEMAND_DIRTY"
        and slave_values.get(
            "SLAVE_READY"
        ) == "YES"
        and slave_values.get(
            "SLAVE_HR1"
        ) == str(0x55AA)
    )

    # No basta con que una muestra dé cero errores: para productizar el
    # timeout se pide además 5 ms de margen frente al peor service gap
    # observado en la misma ventana.
    headroom_pass = (
        timeout_headroom_ms >= 5
    )

    candidate_pass = (
        source_timeout_pass
        and readiness_pass
        and zero_error_pass
        and rate_pass
        and lifecycle_pass
        and cross_count_pass
        and headroom_pass
    )

    print()
    print("=== P5C TIMEOUT RESULT ===")
    print(f"P5C_TIMEOUT_MS={timeout_ms}")
    print(f"P5C_DURATION_MS={duration_ms}")
    print(f"P5C_STARTED={started}")
    print(f"P5C_REJECTED={rejected}")
    print(f"P5C_COMPLETED={completed}")
    print(f"P5C_SUCCESS={success}")
    print(f"P5C_FAILED={failed}")
    print(f"P5C_VERIFY_FAILS={verify_fails}")
    print(f"P5C_TIMEOUTS={timeouts}")
    print(f"P5C_CRC_ERRORS={crc_errors}")
    print(f"P5C_PERIODS_SKIPPED={periods_skipped}")
    print(f"P5C_ACHIEVED_HZ={achieved_hz:.3f}")
    print(f"P5C_RTU_SERVICE_GAP_MAX_US={service_gap_us}")
    print(f"P5C_LOOP_GAP_MAX_US={loop_gap_us}")
    print(f"P5C_FRAM_MAX_US={fram_max_us}")
    print(f"P5C_SD_APPEND_MAX_US={sd_append_max_us}")
    print(f"P5C_SD_VERIFY_MAX_US={sd_verify_max_us}")
    print(f"P5C_TIMEOUT_HEADROOM_MS={timeout_headroom_ms}")
    print(f"P5C_SLAVE_RX={slave_rx}")
    print(f"P5C_SLAVE_TX={slave_tx}")
    print(f"P5C_SLAVE_OK={slave_ok}")
    print(f"P5C_RX_SUCCESS_DELTA={rx_delta}")
    print(f"P5C_TX_SUCCESS_DELTA={tx_delta}")
    print(f"P5C_OK_SUCCESS_DELTA={ok_delta}")
    print(
        "P5C_ZERO_ERROR_PASS="
        f"{'YES' if zero_error_pass else 'NO'}"
    )
    print(
        "P5C_RATE_PASS="
        f"{'YES' if rate_pass else 'NO'}"
    )
    print(
        "P5C_CROSS_COUNT_PASS="
        f"{'YES' if cross_count_pass else 'NO'}"
    )
    print(
        "P5C_HEADROOM_PASS="
        f"{'YES' if headroom_pass else 'NO'}"
    )
    print(
        "P5C_CANDIDATE_PASS="
        f"{'YES' if candidate_pass else 'NO'}"
    )

    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception as exc:
        print(
            "P5C_FATAL="
            f"{type(exc).__name__}: {exc}"
        )
        raise SystemExit(2)
