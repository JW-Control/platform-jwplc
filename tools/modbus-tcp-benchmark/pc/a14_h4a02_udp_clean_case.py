#!/usr/bin/env python3
import argparse
import time
from pathlib import Path
import sys

THIS_DIR = Path(__file__).resolve().parent
if str(THIS_DIR) not in sys.path:
    sys.path.insert(0, str(THIS_DIR))

import eth14_raw_transport_benchmark as rawbench


def wait_ready(dut: rawbench.DutSerial, timeout_s: float = 30.0) -> str:
    deadline = time.perf_counter() + timeout_s
    last = {}
    while time.perf_counter() < deadline:
        last = dut.snapshot()
        ip = last.get("IP", "").strip()
        if (
            last.get("RAW_SERVER_READY") == "YES"
            and last.get("ETH_READY") == "YES"
            and last.get("ETH_LINK") == "UP"
            and ip
            and ip != "0.0.0.0"
        ):
            print(f"H4A02_DUT_IP={ip}")
            print("H4A02_DUT_READY=PASS")
            return ip
        time.sleep(0.25)
    print(last.get("_RAW", ""))
    raise RuntimeError("H4A02_DUT_READY_TIMEOUT")


def emit_snapshot(snap: dict[str, str]) -> None:
    keys = (
        "RAW_SERVER_READY",
        "ETH_READY",
        "ETH_LINK",
        "IP",
        "MODE",
        "RX_BYTES",
        "RX_OPERATIONS",
        "TRANSPORT_ERRORS",
        "LOOP_GAP_AVG_US",
        "LOOP_GAP_MAX_US",
        "UDP_SPI_LOCK_ERRORS",
        "UDP_RX_PACKETS",
        "UDP_RX_SPI_READ_CALLS",
        "UDP_RX_SPI_READ_BYTES",
        "UDP_RX_SPI_READS_PER_PACKET_X1000",
        "UDP_RX_SERVICE_HOLD_COUNT",
        "UDP_RX_ACTIVE_HOLD_COUNT",
        "UDP_RX_ACTIVE_HOLD_US_AVG",
        "UDP_RX_ACTIVE_HOLD_US_MAX",
        "UDP_RX_EMPTY_HOLD_COUNT",
        "ETH_INT_CONFIGURED",
        "ETH_INT_PIN",
        "ETH_INT_UDP_SOCKET",
        "ETH_INT_ISR_COUNT",
        "UDP_RX_INT_SKIP_COUNT",
        "UDP_RX_INT_WAKE_COUNT",
        "UDP_RX_INT_LOW_FALLBACK_COUNT",
        "W5100_DIAG_READ_CALLS_TOTAL",
        "W5100_DIAG_READ_BYTES_TOTAL",
    )
    for key in keys:
        if key in snap:
            print(f"H4A02_SNAPSHOT_{key}={snap[key]}")


def main() -> int:
    parser = argparse.ArgumentParser(
        description="A14 H4A0.2 clean UDP RX single case"
    )
    parser.add_argument("--serial", default="COM14")
    parser.add_argument("--duration", type=float, default=15.0)
    parser.add_argument("--udp-payload", type=int, default=1016)
    parser.add_argument("--variant", required=True)
    args = parser.parse_args()

    if args.duration < 10.0:
        raise SystemExit("H4A02_DURATION_MUST_BE_AT_LEAST_10S")
    if not (8 <= args.udp_payload <= 1472):
        raise SystemExit("H4A02_UDP_PAYLOAD_INVALID")

    print("============================================================")
    print(" A14 H4A0.2 - CLEAN UDP RX SINGLE CASE")
    print("============================================================")
    print(f"H4A02_VARIANT={args.variant}")
    print(f"H4A02_SERIAL={args.serial}")
    print(f"H4A02_DURATION_S={args.duration}")
    print(f"H4A02_UDP_PAYLOAD={args.udp_payload}")

    dut = rawbench.DutSerial(args.serial)
    try:
        dut.open()
        host = wait_ready(dut)
        result = rawbench.udp_rx_bench(
            host,
            5002,
            args.duration,
            args.udp_payload,
            dut,
        )
        rawbench.print_result(result)

        # STOP in udp_rx_bench changes mode only; counters remain available.
        snap = dut.snapshot()
        emit_snapshot(snap)
    finally:
        dut.close()

    print(f"H4A02_DUT_MBPS={result.dut_mbps:.6f}")
    print(f"H4A02_PC_MBPS={result.pc_mbps:.6f}")
    print(f"H4A02_LOSS_PERCENT={result.loss_percent:.6f}")
    print(f"H4A02_TRANSPORT_ERRORS={result.errors}")
    print(
        "H4A02_FUNCTIONAL_PASS="
        f"{'YES' if result.pass_functional else 'NO'}"
    )

    return 0 if result.pass_functional and result.errors == 0 else 2


if __name__ == "__main__":
    raise SystemExit(main())
