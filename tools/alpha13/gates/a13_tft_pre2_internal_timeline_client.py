import argparse
import sys
import time

import serial


def parse_args():
    parser = argparse.ArgumentParser()
    parser.add_argument("--serial", required=True)
    parser.add_argument("--baud", type=int, default=115200)
    parser.add_argument("--timeout-s", type=float, default=10.0)
    return parser.parse_args()


def safe_print(line):
    encoding = getattr(sys.stdout, "encoding", None) or "utf-8"
    safe = (
        line
        .encode(encoding, errors="backslashreplace")
        .decode(encoding, errors="strict")
    )
    print(safe, flush=True)


def emit(values):
    for key in sorted(values):
        print(f"A13_TFT_PRE2_CLIENT_{key}={values[key]}", flush=True)


def main():
    args = parse_args()
    numeric_keys = [
        "INIT_ENTRY_US",
        "RST_LOW_US",
        "STATE_END_US",
        "I2C_END_US",
        "RTC_END_US",
        "FRAM_END_US",
        "SD_END_US",
        "BUTTONS_END_US",
        "DISPLAY_BEGIN_START_US",
        "DISPLAY_BEGIN_END_US",
        "DISPLAY_REFRESH_END_US",
        "TCA_INIT_END_US",
        "TCA_CONFIG_END_US",
        "EN_IO_HIGH_US",
        "INIT_END_US",
        "SETUP_ENTRY_US",
        "PROBE_PRINT_US",
    ]
    required = set(numeric_keys) | {
        "DISPLAY_BEGIN_OK",
        "DISPLAY_READY",
        "IO_READY",
    }

    try:
        ser = serial.Serial(args.serial, args.baud, timeout=0.1)
    except Exception as exc:
        print(f"A13_TFT_PRE2_SERIAL_OPEN_ERROR={type(exc).__name__}", flush=True)
        return 3

    start = time.monotonic()
    in_block = False
    block = []

    try:
        deadline = start + args.timeout_s
        while time.monotonic() < deadline:
            raw = ser.readline()
            if not raw:
                continue

            line = raw.decode("utf-8", errors="backslashreplace").strip()
            if not line:
                continue

            safe_print(line)

            if line == "A13_TFT_PRE2_RESULT=BEGIN":
                in_block = True
                block = []
                continue

            if not in_block:
                continue

            if line == "A13_TFT_PRE2_RESULT=END":
                values = {}
                duplicate = False
                for entry in block:
                    if "=" not in entry:
                        continue
                    key, value = entry.split("=", 1)
                    if key in values:
                        duplicate = True
                    values[key] = value

                in_block = False
                if duplicate or not required.issubset(values):
                    continue

                try:
                    nums = {key: int(values[key]) for key in numeric_keys}
                except ValueError:
                    continue

                if any(value < 0 for value in nums.values()):
                    continue

                ordered = [
                    nums["INIT_ENTRY_US"],
                    nums["RST_LOW_US"],
                    nums["STATE_END_US"],
                    nums["I2C_END_US"],
                    nums["RTC_END_US"],
                    nums["FRAM_END_US"],
                    nums["SD_END_US"],
                    nums["BUTTONS_END_US"],
                    nums["DISPLAY_BEGIN_START_US"],
                    nums["DISPLAY_BEGIN_END_US"],
                    nums["DISPLAY_REFRESH_END_US"],
                    nums["TCA_INIT_END_US"],
                    nums["TCA_CONFIG_END_US"],
                    nums["EN_IO_HIGH_US"],
                    nums["INIT_END_US"],
                    nums["SETUP_ENTRY_US"],
                    nums["PROBE_PRINT_US"],
                ]

                if any(b < a for a, b in zip(ordered, ordered[1:])):
                    continue

                if values["DISPLAY_BEGIN_OK"] != "YES":
                    emit(values)
                    print("A13_TFT_PRE2_DISPLAY_BEGIN_FAILED=YES", flush=True)
                    return 8

                if values["DISPLAY_READY"] != "YES" or values["IO_READY"] != "YES":
                    emit(values)
                    print("A13_TFT_PRE2_RUNTIME_NOT_READY=YES", flush=True)
                    return 9

                emit(values)
                print("A13_TFT_PRE2_CLIENT_PASS=YES", flush=True)
                return 0

            block.append(line)

        print("A13_TFT_PRE2_BLOCK_TIMEOUT=YES", flush=True)
        return 4
    finally:
        if ser.is_open:
            ser.close()


if __name__ == "__main__":
    sys.exit(main())
