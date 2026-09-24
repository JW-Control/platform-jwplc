from __future__ import annotations

import argparse
import re
import time

import serial


SNAPSHOT_END = "A14_PERF_SNAPSHOT=END"


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


def read_snapshot(ser: serial.Serial, timeout_s: float = 3.0) -> dict[str, str]:
    ser.reset_input_buffer()
    ser.write(b"S")
    ser.flush()

    deadline = time.perf_counter() + timeout_s
    raw = bytearray()

    while time.perf_counter() < deadline:
        chunk = ser.read(max(1, ser.in_waiting))

        if chunk:
            raw.extend(chunk)

            if SNAPSHOT_END.encode() in raw:
                break

    text = raw.decode("utf-8", errors="replace")
    values: dict[str, str] = {}

    for line in text.splitlines():
        if "=" not in line:
            continue

        key, value = line.split("=", 1)
        values[key.strip()] = value.strip()

    values["_RAW"] = text
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

    try:
        ser.open()
        time.sleep(2.0)

        while time.perf_counter() < deadline:
            last = read_snapshot(ser)
            ip = last.get("ETH_IP", "")

            if (
                last.get("FULL_RUNTIME_READY") == "YES"
                and last.get("COMBINED_RUNTIME_READY") == "YES"
                and last.get("SERVER_READY") == "YES"
                and last.get("RTU_READY") == "YES"
                and last.get("ETH_READY") == "YES"
                and last.get("ETH_LINK") == "UP"
                and is_valid_ipv4(ip)
            ):
                print("P5_DUT_READY=YES")
                print(f"P5_DUT_IP_EFFECTIVE={ip}")
                print("P5_FULL_RUNTIME_READY=YES")
                print("P5_COMBINED_RUNTIME_READY=YES")
                print("P5_RTU_READY=YES")
                return 0

            time.sleep(0.25)

    finally:
        if ser.is_open:
            ser.close()

    print("P5_DUT_READY=NO")
    print("P5_LAST_SNAPSHOT_BEGIN")
    print(last.get("_RAW", ""))
    print("P5_LAST_SNAPSHOT_END")
    return 2


if __name__ == "__main__":
    raise SystemExit(main())
