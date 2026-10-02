from __future__ import annotations

import argparse
import csv
import json
import sys
import time
from pathlib import Path

THIS_DIR = Path(__file__).resolve().parent
sys.path.insert(0, str(THIS_DIR))

import eth14_raw_transport_benchmark as rawbench


def resolve_ip(dut: rawbench.DutSerial, timeout_s: float = 45.0) -> str:
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
            return ip
        time.sleep(0.25)
    raise RuntimeError("RAW_READY_TIMEOUT\n" + last.get("_RAW", ""))


def row_from_result(r: rawbench.BenchResult) -> dict[str, object]:
    return {
        "mode": r.mode,
        "duration_s": r.duration_s,
        "pc_bytes": r.pc_bytes,
        "dut_bytes": r.dut_bytes,
        "pc_operations": r.pc_operations,
        "dut_operations": r.dut_operations,
        "errors": r.errors,
        "pc_mbps": r.pc_mbps,
        "dut_mbps": r.dut_mbps,
        "loss_operations": r.loss_operations,
        "loss_percent": r.loss_percent,
        "wrong_size_packets": r.wrong_size_packets,
        "pass_functional": r.pass_functional,
    }


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--serial", default="COM14")
    ap.add_argument("--duration", type=float, default=300.0)
    ap.add_argument("--output-root", required=True)
    ap.add_argument("--tcp-chunk", type=int, default=4096)
    ap.add_argument("--udp-payload", type=int, default=1472)
    args = ap.parse_args()

    if args.duration < 60.0:
        raise ValueError("duration debe ser >= 60 s")

    root = Path(args.output_root)
    root.mkdir(parents=True, exist_ok=True)

    dut = rawbench.DutSerial(args.serial)
    rows: list[dict[str, object]] = []

    try:
        dut.open()
        host = resolve_ip(dut)
        (root / "dut_ip.txt").write_text(host + "\n", encoding="utf-8")

        cases = [
            ("TCP_RX", lambda: rawbench.tcp_rx_bench(
                host, 5001, args.duration, args.tcp_chunk, dut)),
            ("TCP_TX", lambda: rawbench.tcp_tx_bench(
                host, 5001, args.duration, args.tcp_chunk, dut)),
            ("UDP_RX_LEGACY", lambda: rawbench.udp_rx_bench(
                host, 5002, args.duration, args.udp_payload, dut)),
            ("UDP_TX", lambda: rawbench.udp_tx_bench(
                host, 5002, args.duration, args.udp_payload, dut)),
        ]

        for label, fn in cases:
            print(f"CASE_BEGIN={label}", flush=True)
            result = fn()
            row = row_from_result(result)
            row["mode"] = label

            # A second post-case snapshot is outside the PERFORMANCE window.
            # It preserves loop/SPI telemetry without printing during traffic.
            post = dut.snapshot()
            (root / f"{label.lower()}_snapshot.txt").write_text(
                post.get("_RAW", ""),
                encoding="utf-8",
            )
            row["loop_gap_avg_us"] = rawbench.intval(post, "LOOP_GAP_AVG_US")
            row["loop_gap_max_us"] = rawbench.intval(post, "LOOP_GAP_MAX_US")
            row["tcp_spi_lock_errors"] = rawbench.intval(post, "TCP_SPI_LOCK_ERRORS")
            row["udp_spi_lock_errors"] = rawbench.intval(post, "UDP_SPI_LOCK_ERRORS")
            row["tcp_spi_hold_total_us"] = rawbench.intval(post, "TCP_SPI_HOLD_TOTAL_US")
            row["tcp_spi_hold_avg_us"] = rawbench.intval(post, "TCP_SPI_HOLD_AVG_US")
            row["tcp_spi_hold_max_us"] = rawbench.intval(post, "TCP_SPI_HOLD_MAX_US")
            row["tcp_spi_occupancy_pct"] = (
                100.0 * row["tcp_spi_hold_total_us"] / (result.duration_s * 1_000_000.0)
                if result.duration_s > 0.0 else 0.0
            )

            rows.append(row)
            (root / f"{label.lower()}.json").write_text(
                json.dumps(row, indent=2),
                encoding="utf-8",
            )
            print(
                f"CASE_END={label} DUT_MBPS={row['dut_mbps']:.6f} "
                f"ERRORS={row['errors']} PASS={row['pass_functional']}",
                flush=True,
            )

    finally:
        dut.close()

    with (root / "SUMMARY.csv").open("w", newline="", encoding="utf-8") as f:
        writer = csv.DictWriter(f, fieldnames=list(rows[0].keys()))
        writer.writeheader()
        writer.writerows(rows)

    hard_fail = any(not bool(r["pass_functional"]) or int(r["errors"]) != 0 for r in rows)
    print(f"A14_FINAL_RAW_LEGACY={'FAIL' if hard_fail else 'PASS'}")
    return 2 if hard_fail else 0


if __name__ == "__main__":
    raise SystemExit(main())
