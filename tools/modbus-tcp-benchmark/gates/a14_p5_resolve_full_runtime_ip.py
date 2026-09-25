from __future__ import annotations

import argparse
import time

import serial


PREFLIGHT_END = "A14_P5_PREFLIGHT=END"
RESET_ACK = "A14_PERF_RESET=PASS"


def is_valid_ipv4(value: str) -> bool:
    parts = value.split(".")
    if len(parts) != 4:
        return False

    try:
        octets = [int(part) for part in parts]
    except ValueError:
        return False

    return (
        all(0 <= octet <= 255 for octet in octets)
        and value != "0.0.0.0"
    )


def read_line(ser: serial.Serial) -> str | None:
    raw = ser.readline()

    if not raw:
        return None

    return raw.decode(
        "utf-8",
        errors="replace",
    ).strip()


def reset_stats(
    ser: serial.Serial,
    timeout_s: float = 3.0,
) -> bool:
    ser.reset_input_buffer()
    ser.write(b"R\n")
    ser.flush()

    deadline = time.perf_counter() + timeout_s

    while time.perf_counter() < deadline:
        line = read_line(ser)

        if line == RESET_ACK:
            return True

    return False


def read_preflight(
    ser: serial.Serial,
    timeout_s: float = 3.0,
) -> dict[str, str]:
    ser.reset_input_buffer()
    ser.write(b"P\n")
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

        if line == PREFLIGHT_END:
            values["_RAW"] = "\n".join(raw_lines)
            return values

    values["_RAW"] = "\n".join(raw_lines)
    return values


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--serial", default="COM14")
    parser.add_argument("--timeout", type=float, default=45.0)
    args = parser.parse_args()

    ser = serial.Serial()
    ser.port = args.serial
    ser.baudrate = 115200
    ser.timeout = 0.05
    ser.write_timeout = 1.0
    ser.dtr = False
    ser.rts = False

    deadline = time.perf_counter() + args.timeout
    last: dict[str, str] = {}
    attempt = 0

    try:
        ser.open()
        time.sleep(2.0)

        while time.perf_counter() < deadline:
            attempt += 1

            if not reset_stats(
                ser,
                timeout_s=3.0,
            ):
                last = {
                    "_RAW":
                        "A14_PERF_RESET_ACK_MISSING"
                }
                time.sleep(0.25)
                continue

            # Ventana quieta. No se solicita el snapshot completo porque
            # imprimir varios KB a 115200 perturba el loop cooperativo.
            quiet_until = min(
                deadline,
                time.perf_counter() + 3.0,
            )

            while time.perf_counter() < quiet_until:
                time.sleep(0.05)

            last = read_preflight(
                ser,
                timeout_s=3.0,
            )

            ip = last.get("ETH_IP", "")

            try:
                rtu_timeout_ms = int(
                    last.get(
                        "RTU_TIMEOUT_MS",
                        "-1",
                    )
                )
                rtu_success = int(
                    last.get(
                        "RTU_REQUESTS_SUCCESS",
                        "0",
                    )
                )
                rtu_failed = int(
                    last.get(
                        "RTU_REQUESTS_FAILED",
                        "0",
                    )
                )
                rtu_verify_fails = int(
                    last.get(
                        "RTU_VERIFY_FAILS",
                        "0",
                    )
                )
                rtu_crc_errors = int(
                    last.get(
                        "RTU_CRC_ERRORS",
                        "0",
                    )
                )
                rtu_timeouts = int(
                    last.get(
                        "RTU_MASTER_TIMEOUTS",
                        "0",
                    )
                )
                peripheral_failures = int(
                    last.get(
                        "PERIPHERAL_FAILURE_COUNT",
                        "-1",
                    )
                )
            except ValueError:
                rtu_timeout_ms = -1
                rtu_success = 0
                rtu_failed = -1
                rtu_verify_fails = -1
                rtu_crc_errors = -1
                rtu_timeouts = -1
                peripheral_failures = -1

            rtu_peer_ready = (
                last.get("RTU_ROLE") == "MASTER"
                and
                last.get(
                    "RTU_TARGET_SLAVE_ID"
                ) == "2"
                and
                last.get(
                    "RTU_TRAFFIC_ENABLED"
                ) == "YES"
                and rtu_timeout_ms == 25
                and rtu_success >= 10
                and rtu_failed == 0
                and rtu_verify_fails == 0
                and rtu_crc_errors == 0
                and rtu_timeouts == 0
            )

            ready = (
                last.get(
                    "FULL_RUNTIME_READY"
                ) == "YES"
                and
                last.get(
                    "COMBINED_RUNTIME_READY"
                ) == "YES"
                and
                last.get(
                    "SERVER_READY"
                ) == "YES"
                and
                last.get(
                    "SD_READY"
                ) == "YES"
                and
                last.get(
                    "SD_WORKLOAD_MODE"
                ) == "BUFFERED_DATALOG"
                and
                last.get(
                    "DISPLAY_READY"
                ) == "YES"
                and
                last.get(
                    "DISPLAY_RENDER_MODE"
                ) == "HMI_ON_DEMAND_DIRTY"
                and
                last.get(
                    "DISPLAY_REFRESH_MODE"
                ) == "USER_REFRESH_ON_DEMAND"
                and
                last.get(
                    "RTU_READY"
                ) == "YES"
                and rtu_peer_ready
                and
                last.get(
                    "ETH_READY"
                ) == "YES"
                and
                last.get(
                    "ETH_LINK"
                ) == "UP"
                and peripheral_failures == 0
                and is_valid_ipv4(ip)
            )

            if ready:
                print("P5_DUT_READY=YES")
                print(
                    f"P5_DUT_IP_EFFECTIVE={ip}"
                )
                print(
                    "P5_FULL_RUNTIME_READY=YES"
                )
                print(
                    "P5_COMBINED_RUNTIME_READY=YES"
                )
                print("P5_SD_READY=YES")
                print(
                    "P5_SD_WORKLOAD_MODE=BUFFERED_DATALOG"
                )
                print(
                    "P5_DISPLAY_HMI_DIRTY=YES"
                )
                print("P5_RTU_READY=YES")
                print("P5_RTU_TIMEOUT_MS=25")
                print(
                    "P5_RTU_PEER_SLAVE2=PASS"
                )
                print(
                    "P5_RTU_PREFLIGHT_SUCCESS="
                    f"{rtu_success}"
                )
                print(
                    "P5_PREFLIGHT_ATTEMPT="
                    f"{attempt}"
                )
                print(
                    "P5_PREFLIGHT_MODE="
                    "COMPACT_QUIET"
                )
                return 0

            time.sleep(0.25)

    finally:
        if ser.is_open:
            ser.close()

    print("P5_DUT_READY=NO")
    print("P5_PREFLIGHT_MODE=COMPACT_QUIET")
    print("P5_LAST_PREFLIGHT_BEGIN")
    print(last.get("_RAW", ""))
    print("P5_LAST_PREFLIGHT_END")
    return 2


if __name__ == "__main__":
    raise SystemExit(main())
