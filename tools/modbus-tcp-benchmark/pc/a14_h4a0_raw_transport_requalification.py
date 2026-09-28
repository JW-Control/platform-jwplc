#!/usr/bin/env python3
import argparse
import statistics
import time
from pathlib import Path
import sys

THIS_DIR = Path(__file__).resolve().parent
if str(THIS_DIR) not in sys.path:
    sys.path.insert(0, str(THIS_DIR))

import eth14_raw_transport_benchmark as rawbench


def resolve_ip(dut: rawbench.DutSerial, timeout_s: float = 30.0) -> str:
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
            print(f"H4A0_DUT_IP={ip}")
            print("H4A0_DUT_READY=PASS")
            return ip
        time.sleep(0.25)

    print(last.get("_RAW", ""))
    raise RuntimeError("H4A0_DUT_READY_TIMEOUT")


def summarize(mode: str, results):
    pc = [r.pc_mbps for r in results]
    dut = [r.dut_mbps for r in results]
    loss = [r.loss_percent for r in results]
    errors = [r.errors for r in results]

    print(f"H4A0_{mode}_REPETITIONS={len(results)}")
    print(f"H4A0_{mode}_PC_MBPS_MEDIAN={statistics.median(pc):.6f}")
    print(f"H4A0_{mode}_PC_MBPS_MIN={min(pc):.6f}")
    print(f"H4A0_{mode}_PC_MBPS_MAX={max(pc):.6f}")
    print(f"H4A0_{mode}_DUT_MBPS_MEDIAN={statistics.median(dut):.6f}")
    print(f"H4A0_{mode}_DUT_MBPS_MIN={min(dut):.6f}")
    print(f"H4A0_{mode}_DUT_MBPS_MAX={max(dut):.6f}")
    print(f"H4A0_{mode}_LOSS_PERCENT_MAX={max(loss):.6f}")
    print(f"H4A0_{mode}_TRANSPORT_ERRORS_TOTAL={sum(errors)}")

    clean = all(r.pass_functional and r.errors == 0 for r in results)
    if mode.startswith("UDP"):
        clean = clean and max(loss) <= 0.10

    print(f"H4A0_{mode}_CLEAN={'YES' if clean else 'NO'}")
    return clean


def main():
    parser = argparse.ArgumentParser(
        description="A14 H4A0 post-H3E raw Ethernet requalification"
    )
    parser.add_argument("--serial", default="COM14")
    parser.add_argument("--duration", type=float, default=15.0)
    parser.add_argument("--repetitions", type=int, default=3)
    parser.add_argument("--tcp-chunk", type=int, default=4096)
    parser.add_argument("--udp-payload", type=int, default=1472)
    parser.add_argument("--tcp-port", type=int, default=5001)
    parser.add_argument("--udp-port", type=int, default=5002)
    args = parser.parse_args()

    if args.duration < 10.0:
        raise SystemExit("H4A0_DURATION_MUST_BE_AT_LEAST_10S")
    if args.repetitions < 3:
        raise SystemExit("H4A0_REPETITIONS_MUST_BE_AT_LEAST_3")
    if args.tcp_chunk <= 0:
        raise SystemExit("H4A0_TCP_CHUNK_INVALID")
    if not (8 <= args.udp_payload <= 1472):
        raise SystemExit("H4A0_UDP_PAYLOAD_INVALID")

    print("============================================================")
    print(" A14 H4A0 - POST-H3E RAW ETHERNET REQUALIFICATION")
    print(" TCP RX/TX + UDP RX/TX")
    print("============================================================")
    print(f"H4A0_SERIAL={args.serial}")
    print(f"H4A0_DURATION_PER_CASE_S={args.duration}")
    print(f"H4A0_REPETITIONS={args.repetitions}")
    print(f"H4A0_TCP_CHUNK={args.tcp_chunk}")
    print(f"H4A0_UDP_PAYLOAD={args.udp_payload}")

    dut = rawbench.DutSerial(args.serial)
    all_results = {
        "TCP_RX": [],
        "TCP_TX": [],
        "UDP_RX": [],
        "UDP_TX": [],
    }

    try:
        dut.open()
        host = resolve_ip(dut)

        for rep in range(1, args.repetitions + 1):
            print()
            print("============================================================")
            print(f" H4A0 REPETITION {rep}/{args.repetitions}")
            print("============================================================")

            cases = (
                ("TCP_RX", lambda: rawbench.tcp_rx_bench(
                    host, args.tcp_port, args.duration, args.tcp_chunk, dut
                )),
                ("TCP_TX", lambda: rawbench.tcp_tx_bench(
                    host, args.tcp_port, args.duration, args.tcp_chunk, dut
                )),
                ("UDP_RX", lambda: rawbench.udp_rx_bench(
                    host, args.udp_port, args.duration, args.udp_payload, dut
                )),
                ("UDP_TX", lambda: rawbench.udp_tx_bench(
                    host, args.udp_port, args.duration, args.udp_payload, dut
                )),
            )

            for mode, run_case in cases:
                print()
                print(f"=== H4A0 REP={rep} MODE={mode} ===")
                result = run_case()
                all_results[mode].append(result)
                rawbench.print_result(result)
                print(f"H4A0_REP{rep}_{mode}_PC_MBPS={result.pc_mbps:.6f}")
                print(f"H4A0_REP{rep}_{mode}_DUT_MBPS={result.dut_mbps:.6f}")
                print(f"H4A0_REP{rep}_{mode}_LOSS_PERCENT={result.loss_percent:.6f}")
                print(f"H4A0_REP{rep}_{mode}_ERRORS={result.errors}")
                print(
                    f"H4A0_REP{rep}_{mode}_PASS="
                    f"{'YES' if result.pass_functional else 'NO'}"
                )
                time.sleep(0.50)
    finally:
        dut.close()

    print()
    print("============================================================")
    print(" H4A0 MEDIAN SUMMARY")
    print("============================================================")

    clean_modes = []
    for mode in ("TCP_RX", "TCP_TX", "UDP_RX", "UDP_TX"):
        clean_modes.append(summarize(mode, all_results[mode]))

    print("H4A0_W5500_SPI_HZ=26000000")
    print("H4A0_RAW_TRANSPORT_FUNCTIONAL_PASS="
          f"{'YES' if all(clean_modes) else 'NO'}")
    print("A14_H4A0_RAW_ETHERNET_BASELINE="
          f"{'PASS' if all(clean_modes) else 'REVIEW'}")

    if not all(clean_modes):
        raise SystemExit(2)

    print("NEXT=RETURN_OUTPUT_TO_CHAT_FOR_RAW_CEILING_SWEEP_DECISION")


if __name__ == "__main__":
    main()
