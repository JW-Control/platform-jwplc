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
        print(f"A13_TFT_PRE1_CLIENT_{key}={values[key]}", flush=True)


def main():
    args = parse_args()
    required = {
        "SETUP_ENTRY_MS",
        "DISPLAY_READY",
        "IO_READY",
        "TFT_RST_OUTPUT_ENABLE",
        "TFT_RST_OUTPUT_LATCH",
        "TFT_CS_OUTPUT_ENABLE",
        "TFT_CS_OUTPUT_LATCH",
        "UPTIME_MS",
    }

    try:
        ser = serial.Serial(args.serial, args.baud, timeout=0.1)
    except Exception as exc:
        print(f"A13_TFT_PRE1_SERIAL_OPEN_ERROR={type(exc).__name__}", flush=True)
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

            if line == "A13_TFT_PRE1_RESULT=BEGIN":
                in_block = True
                block = []
                continue

            if not in_block:
                continue

            if line == "A13_TFT_PRE1_RESULT=END":
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
                    setup_ms = int(values["SETUP_ENTRY_MS"])
                    uptime_ms = int(values["UPTIME_MS"])
                except ValueError:
                    continue

                if setup_ms < 0 or uptime_ms < setup_ms:
                    continue

                emit(values)
                print("A13_TFT_PRE1_CLIENT_PASS=YES", flush=True)
                return 0

            block.append(line)

        print("A13_TFT_PRE1_BLOCK_TIMEOUT=YES", flush=True)
        return 4
    finally:
        if ser.is_open:
            ser.close()


if __name__ == "__main__":
    sys.exit(main())
