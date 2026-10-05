"""Resolve the effective DHCP IP of the ETH14 raw DUT from Serial.

Uses the frozen raw benchmark DutSerial implementation so the Serial framing
and DTR/RTS behavior stay identical to the validated benchmark harness.
"""

from __future__ import annotations

import argparse
import importlib.util
from pathlib import Path
import sys
import time


RUNNER_PATH = (
    Path(__file__).resolve().parents[1]
    / "pc"
    / "eth14_raw_transport_benchmark.py"
)


def load_runner():
    if not RUNNER_PATH.is_file():
        raise RuntimeError(
            f"NB3_IP_RESOLVER_RUNNER_NOT_FOUND={RUNNER_PATH}"
        )

    spec = importlib.util.spec_from_file_location(
        "eth14_raw_transport_benchmark_ip_resolver",
        RUNNER_PATH,
    )

    if spec is None or spec.loader is None:
        raise RuntimeError(
            "NB3_IP_RESOLVER_IMPORT_SPEC_FAILED"
        )

    runner = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = runner
    spec.loader.exec_module(runner)
    return runner


def is_valid_ipv4(value: str) -> bool:
    parts = value.split(".")
    if len(parts) != 4:
        return False

    try:
        octets = [int(part) for part in parts]
    except ValueError:
        return False

    if any(octet < 0 or octet > 255 for octet in octets):
        return False

    return value != "0.0.0.0"


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Resolve JWPLC raw DUT IP from Serial snapshot",
    )
    parser.add_argument(
        "--serial",
        required=True,
    )
    parser.add_argument(
        "--timeout",
        type=float,
        default=15.0,
    )
    args = parser.parse_args()

    if args.timeout <= 0:
        raise RuntimeError(
            "NB3_IP_RESOLVER_TIMEOUT_MUST_BE_POSITIVE"
        )

    runner = load_runner()
    dut = runner.DutSerial(args.serial)

    deadline = time.perf_counter() + args.timeout
    last: dict[str, str] = {}

    try:
        dut.open()

        while time.perf_counter() < deadline:
            last = dut.snapshot()
            ip = last.get("IP", "")

            if (
                last.get("RAW_SERVER_READY") == "YES"
                and last.get("ETH_READY") == "YES"
                and last.get("ETH_LINK") == "UP"
                and is_valid_ipv4(ip)
            ):
                print("DUT_SERIAL_READY=YES")
                print(f"DUT_IP_EFFECTIVE={ip}")
                print(
                    "DUT_IP_SOURCE=RAW_SERIAL_SNAPSHOT"
                )
                return 0

            time.sleep(0.25)
    finally:
        dut.close()

    print("DUT_SERIAL_READY=NO")
    print(
        "DUT_LAST_RAW_SNAPSHOT_BEGIN"
    )
    print(last.get("_RAW", ""))
    print(
        "DUT_LAST_RAW_SNAPSHOT_END"
    )
    return 2


if __name__ == "__main__":
    raise SystemExit(main())
