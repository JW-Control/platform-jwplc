from __future__ import annotations

import argparse
import time

import serial


TERMINAL_PREFIX = "A14_P5F_DATALOG_RESULT="


def open_serial_no_dtr(
    port: str,
    baud: int,
) -> serial.Serial:
    ser = serial.Serial()
    ser.port = port
    ser.baudrate = baud
    ser.timeout = 0.10
    ser.write_timeout = 1.0
    ser.dtr = False
    ser.rts = False
    ser.open()
    return ser


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--port", default="COM14")
    parser.add_argument("--baud", type=int, default=115200)
    parser.add_argument("--timeout", type=float, default=30.0)
    args = parser.parse_args()

    ser = open_serial_no_dtr(
        args.port,
        args.baud,
    )

    terminal = ""

    try:
        deadline = (
            time.monotonic() +
            args.timeout
        )

        while time.monotonic() < deadline:
            raw = ser.readline()

            if not raw:
                continue

            line = raw.decode(
                "utf-8",
                errors="replace",
            ).strip()

            if not line:
                continue

            print(line)

            if line.startswith(
                TERMINAL_PREFIX
            ):
                terminal = line
                break

    finally:
        ser.close()

    if not terminal:
        print(
            "P5F_SERIAL_TERMINAL=TIMEOUT"
        )
        return 2

    print(
        f"P5F_SERIAL_TERMINAL={terminal}"
    )

    if terminal != (
        "A14_P5F_DATALOG_RESULT=PASS"
    ):
        return 1

    print(
        "P5F_AUTOSERVICE_PHYSICAL=PASS"
    )

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
