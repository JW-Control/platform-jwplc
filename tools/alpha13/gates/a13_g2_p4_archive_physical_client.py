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


def emit_summary(values):
    for key in sorted(values):
        print(f"A13_G2_P4_CLIENT_{key}={values[key]}", flush=True)


def main():
    args = parse_args()
    required = {
        "IO_READY",
        "EN_IO_OUTPUT_ENABLE",
        "EN_IO_OUTPUT_LATCH",
        "INPUTS",
        "OUTPUTS",
        "LAST_SCAN_MS",
        "UPTIME_MS",
    }

    try:
        ser = serial.Serial(args.serial, args.baud, timeout=0.1)
    except Exception as exc:
        print(f"A13_G2_P4_SERIAL_OPEN_ERROR={type(exc).__name__}", flush=True)
        return 3

    complete_blocks = 0
    last_values = None
    start = time.monotonic()

    try:
        deadline = start + args.timeout_s
        in_block = False
        block = []

        while time.monotonic() < deadline:
            raw = ser.readline()
            if not raw:
                continue

            line = raw.decode("utf-8", errors="backslashreplace").strip()
            if not line:
                continue

            safe_print(line)

            if line == "A13_G2_P4_RESULT=BEGIN":
                in_block = True
                block = []
                continue

            if not in_block:
                continue

            if line == "A13_G2_P4_RESULT=END":
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
                    inputs = int(values["INPUTS"])
                    outputs = int(values["OUTPUTS"])
                    last_scan_ms = int(values["LAST_SCAN_MS"])
                    uptime_ms = int(values["UPTIME_MS"])
                except ValueError:
                    continue

                if not (0 <= inputs <= 255 and 0 <= outputs <= 255):
                    continue
                if last_scan_ms < 0 or uptime_ms < 0:
                    continue

                complete_blocks += 1
                last_values = values

                contract = (
                    values["IO_READY"] == "YES"
                    and values["EN_IO_OUTPUT_ENABLE"] == "YES"
                    and values["EN_IO_OUTPUT_LATCH"] == "HIGH"
                    and last_scan_ms > 0
                )

                if contract:
                    emit_summary(values)
                    elapsed_ms = int((time.monotonic() - start) * 1000)
                    print(f"A13_G2_P4_VALID_BLOCKS={complete_blocks}", flush=True)
                    print(f"A13_G2_P4_PASS_ELAPSED_MS={elapsed_ms}", flush=True)
                    print("A13_G2_P4_CLIENT_PASS=YES", flush=True)
                    return 0

                continue

            block.append(line)

        print(f"A13_G2_P4_VALID_BLOCKS={complete_blocks}", flush=True)
        if last_values is not None:
            emit_summary(last_values)
        if complete_blocks > 0:
            print("A13_G2_P4_CONTRACT_TIMEOUT=YES", flush=True)
            return 8

        print("A13_G2_P4_BLOCK_TIMEOUT=YES", flush=True)
        return 4
    finally:
        if ser.is_open:
            ser.close()


if __name__ == "__main__":
    sys.exit(main())
