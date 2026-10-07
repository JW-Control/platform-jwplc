import argparse
import sys
import time

import serial


def parse_args():
    parser = argparse.ArgumentParser()
    parser.add_argument("--serial", required=True)
    parser.add_argument("--baud", type=int, default=115200)
    parser.add_argument("--timeout-s", type=float, default=8.0)
    parser.add_argument("--expected-step", type=int, required=True)
    return parser.parse_args()


def safe_print(line):
    encoding = getattr(sys.stdout, "encoding", None) or "utf-8"
    safe = (
        line
        .encode(encoding, errors="backslashreplace")
        .decode(encoding, errors="strict")
    )
    print(safe, flush=True)


def main():
    args = parse_args()

    if args.expected_step < 0 or args.expected_step > 5:
        print("A13_G2_CLIENT_BAD_EXPECTED_STEP=YES", flush=True)
        return 2

    ser = serial.Serial(
        args.serial,
        args.baud,
        timeout=0.1,
    )

    try:
        deadline = time.monotonic() + args.timeout_s
        in_block = False
        block = []

        while time.monotonic() < deadline:
            raw = ser.readline()
            if not raw:
                continue

            line = raw.decode(
                "utf-8",
                errors="backslashreplace",
            ).strip()

            if not line:
                continue

            safe_print(line)

            if line == "A13_G2_TCA_RESULT=BEGIN":
                in_block = True
                block = [line]
                continue

            if not in_block:
                continue

            block.append(line)

            if line != "A13_G2_TCA_RESULT=END":
                continue

            values = {}
            duplicate = False

            for entry in block[1:-1]:
                if "=" not in entry:
                    continue
                key, value = entry.split("=", 1)
                if key in values:
                    duplicate = True
                values[key] = value

            required = {
                "FAULT_STEP",
                "OP_OK_MASK",
                "EN_IO_HIGH_REQUESTED",
                "EN_IO_OUTPUT_ENABLE",
                "EN_IO_OUTPUT_LATCH",
                "EN_IO_PAD_READBACK",
                "PERIPHERALS_INITIALIZED",
                "IO_STATE_INITIALIZED",
                "IO_VIEW_READY",
                "REG_OUTPUT0_OK",
                "REG_OUTPUT0",
                "REG_OUTPUT1_OK",
                "REG_OUTPUT1",
                "REG_OUTPUT2_OK",
                "REG_OUTPUT2",
                "REG_CONFIG0_OK",
                "REG_CONFIG0",
                "REG_CONFIG1_OK",
                "REG_CONFIG1",
                "REG_CONFIG2_OK",
                "REG_CONFIG2",
            }

            if duplicate:
                print("A13_G2_CLIENT_DUPLICATE_KEY=YES", flush=True)
                return 3

            missing = sorted(required.difference(values))
            if missing:
                print(
                    "A13_G2_CLIENT_MISSING=" + ",".join(missing),
                    flush=True,
                )
                return 4

            try:
                observed_step = int(values["FAULT_STEP"])
            except ValueError:
                print("A13_G2_CLIENT_STEP_PARSE_FAIL=YES", flush=True)
                return 5

            if observed_step != args.expected_step:
                print(
                    f"A13_G2_CLIENT_STEP_MISMATCH={observed_step}",
                    flush=True,
                )
                return 6

            print("A13_G2_CLIENT_BLOCK=PASS", flush=True)
            return 0

        print("A13_G2_CLIENT_TIMEOUT=YES", flush=True)
        return 7
    finally:
        if ser.is_open:
            ser.close()


if __name__ == "__main__":
    sys.exit(main())
