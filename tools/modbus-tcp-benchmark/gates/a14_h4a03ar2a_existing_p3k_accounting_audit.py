#!/usr/bin/env python3
from __future__ import annotations

import argparse
import re
import statistics
from pathlib import Path

HIST = {
    "udp_b1": 13.860041,
    "udp_b2": 13.872847,
    "udp_all": 13.866349,
    "tcp_b1": 13.413063,
    "tcp_b2": 13.344293,
    "tcp_all": 13.412507,
}
R1_MINIMAL_TAIL0_MBPS = 12.501134
UDP_NOMINAL_TAIL_S = 0.40
TCP_NOMINAL_TAIL_S = 0.50


def emit(key: str, value: object) -> None:
    print(f"{key}={value}")


def decode(data: bytes) -> str:
    if data.startswith(b"\xff\xfe") or data.startswith(b"\xfe\xff"):
        return data.decode("utf-16")
    for enc in ("utf-8-sig", "cp1252"):
        try:
            return data.decode(enc)
        except UnicodeDecodeError:
            pass
    return data.decode("utf-8", errors="replace")


def values(text: str, key: str) -> list[str]:
    return [
        m.strip()
        for m in re.findall(
            rf"(?m)^{re.escape(key)}=(.*)\r?$",
            text,
        )
    ]


def unique(text: str, key: str) -> str:
    matches = values(text, key)
    if len(matches) != 1:
        raise RuntimeError(f"R2A_KEY_COUNT_{key}={len(matches)}")
    return matches[0]


def median(items: list[float]) -> float:
    if not items:
        raise RuntimeError("R2A_EMPTY_MEDIAN")
    return float(statistics.median(items))


def pct(current: float, reference: float) -> float:
    return (current / reference - 1.0) * 100.0


def inspect(path: Path, mode: str) -> dict[str, float]:
    text = decode(path.read_bytes())

    if unique(text, "RAW_BENCH_FUNCTIONAL_PASS") != "YES":
        raise RuntimeError(f"R2A_FUNCTIONAL_FAIL={path}")
    if unique(text, "P3J_R2_QUIESCENCE_PASS") != "YES":
        raise RuntimeError(f"R2A_QUIESCENCE_FAIL={path}")
    if unique(text, "P3J_R2_FINAL_SNAPSHOT_PRESENT") != "YES":
        raise RuntimeError(f"R2A_SNAPSHOT_MISSING={path}")
    if unique(text, "P3J_R2_FINAL_ETH_READY") != "YES":
        raise RuntimeError(f"R2A_ETH_NOT_READY={path}")
    if unique(text, "P3J_R2_FINAL_ETH_LINK") != "UP":
        raise RuntimeError(f"R2A_LINK_DOWN={path}")

    transport_errors = int(unique(text, "P3J_R2_FINAL_TRANSPORT_ERRORS"))
    udp_spi_errors = int(unique(text, "P3J_R2_FINAL_UDP_SPI_LOCK_ERRORS"))
    if transport_errors != 0 or udp_spi_errors != 0:
        raise RuntimeError(
            f"R2A_RUNTIME_ERRORS={path} "
            f"TRANSPORT={transport_errors} UDP_SPI={udp_spi_errors}"
        )

    duration_values = values(text, "DURATION_S")
    if len(duration_values) != 2:
        raise RuntimeError(
            f"R2A_DURATION_KEY_COUNT={len(duration_values)} PATH={path}"
        )

    requested = float(duration_values[0])
    actual = float(duration_values[1])
    if abs(requested - 5.0) > 0.000001:
        raise RuntimeError(
            f"R2A_REQUESTED_DURATION_NOT_5S={requested} PATH={path}"
        )

    dut_bytes = float(unique(text, "DUT_BYTES"))
    summary_key = (
        "SUMMARY_UDP_RX_DUT_MBPS"
        if mode == "UDP"
        else "SUMMARY_TCP_RX_DUT_MBPS"
    )
    reported = float(unique(text, summary_key))
    recomputed = dut_bytes * 8.0 / actual / 1_000_000.0

    if abs(recomputed - reported) > 0.00001:
        raise RuntimeError(
            f"R2A_REPORTED_RECOMPUTE_MISMATCH={path} "
            f"REPORTED={reported:.6f} RECOMPUTED={recomputed:.6f}"
        )

    tail = UDP_NOMINAL_TAIL_S if mode == "UDP" else TCP_NOMINAL_TAIL_S
    corrected = dut_bytes * 8.0 / (actual + tail) / 1_000_000.0

    return {
        "requested": requested,
        "actual": actual,
        "bytes": dut_bytes,
        "raw": reported,
        "corrected": corrected,
    }


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Offline accounting audit for an already completed historical P3K run"
    )
    parser.add_argument(
        "--root",
        required=True,
        help="Original P3K TEMP_ROOT containing INT_COMMIT2_R1",
    )
    args = parser.parse_args()

    root = Path(args.root)
    variant = root / "INT_COMMIT2_R1"

    print("=" * 72)
    print(" A14 H4A0.3A-R2A - EXISTING P3K ACCOUNTING AUDIT")
    print("=" * 72)
    emit("R2A_ROOT", root)
    emit("R2A_VARIANT_ROOT", variant)
    emit("R2A_PHYSICAL_REPLAY", "NO")
    emit("R2A_REUSE_EXISTING_12_RUNS", "YES")

    if not variant.is_dir():
        raise RuntimeError(f"R2A_VARIANT_ROOT_MISSING={variant}")

    raw = {"UDP": {1: [], 2: []}, "TCP": {1: [], 2: []}}
    corrected = {"UDP": {1: [], 2: []}, "TCP": {1: [], 2: []}}

    for block in (1, 2):
        for mode in ("UDP", "TCP"):
            for run_no in (1, 2, 3):
                path = variant / f"p3k_block_{block}_{mode}_RX_run_{run_no}.log"
                if not path.is_file():
                    raise RuntimeError(f"R2A_LOG_MISSING={path}")

                row = inspect(path, mode)
                raw[mode][block].append(row["raw"])
                corrected[mode][block].append(row["corrected"])

                emit(
                    f"R2A_B{block}_{mode}_R{run_no}",
                    f"RAW={row['raw']:.6f},"
                    f"CORRECTED={row['corrected']:.6f},"
                    f"DURATION={row['actual']:.6f},"
                    f"BYTES={int(row['bytes'])}",
                )

    raw_udp_b1 = median(raw["UDP"][1])
    raw_udp_b2 = median(raw["UDP"][2])
    raw_tcp_b1 = median(raw["TCP"][1])
    raw_tcp_b2 = median(raw["TCP"][2])
    raw_udp_all = median(raw["UDP"][1] + raw["UDP"][2])
    raw_tcp_all = median(raw["TCP"][1] + raw["TCP"][2])

    cor_udp_b1 = median(corrected["UDP"][1])
    cor_udp_b2 = median(corrected["UDP"][2])
    cor_tcp_b1 = median(corrected["TCP"][1])
    cor_tcp_b2 = median(corrected["TCP"][2])
    cor_udp_all = median(corrected["UDP"][1] + corrected["UDP"][2])
    cor_tcp_all = median(corrected["TCP"][1] + corrected["TCP"][2])

    print()
    print("=" * 72)
    print(" R2A HISTORICAL REPLAY + ACCOUNTING SUMMARY")
    print("=" * 72)

    emit("R2A_HISTORICAL_REPORTED_UDP_MBPS", f"{HIST['udp_all']:.6f}")
    emit("R2A_HISTORICAL_REPORTED_TCP_MBPS", f"{HIST['tcp_all']:.6f}")

    emit("R2A_REPLAY_RAW_UDP_B1_MBPS", f"{raw_udp_b1:.6f}")
    emit("R2A_REPLAY_RAW_TCP_B1_MBPS", f"{raw_tcp_b1:.6f}")
    emit("R2A_REPLAY_RAW_UDP_B2_MBPS", f"{raw_udp_b2:.6f}")
    emit("R2A_REPLAY_RAW_TCP_B2_MBPS", f"{raw_tcp_b2:.6f}")
    emit("R2A_REPLAY_RAW_UDP_MEDIAN_MBPS", f"{raw_udp_all:.6f}")
    emit("R2A_REPLAY_RAW_TCP_MEDIAN_MBPS", f"{raw_tcp_all:.6f}")
    emit("R2A_REPLAY_RAW_UDP_VS_TCP_PCT", f"{pct(raw_udp_all, raw_tcp_all):.2f}")
    emit("R2A_RAW_UDP_VS_HISTORICAL_PCT", f"{pct(raw_udp_all, HIST['udp_all']):.2f}")
    emit("R2A_RAW_TCP_VS_HISTORICAL_PCT", f"{pct(raw_tcp_all, HIST['tcp_all']):.2f}")
    emit(
        "R2A_RAW_UDP_WITHIN_0P5PCT_HISTORICAL",
        abs(pct(raw_udp_all, HIST["udp_all"])) <= 0.5,
    )
    emit(
        "R2A_RAW_TCP_WITHIN_0P5PCT_HISTORICAL",
        abs(pct(raw_tcp_all, HIST["tcp_all"])) <= 0.5,
    )

    emit("R2A_CORRECTED_UDP_B1_MBPS", f"{cor_udp_b1:.6f}")
    emit("R2A_CORRECTED_TCP_B1_MBPS", f"{cor_tcp_b1:.6f}")
    emit("R2A_CORRECTED_UDP_B2_MBPS", f"{cor_udp_b2:.6f}")
    emit("R2A_CORRECTED_TCP_B2_MBPS", f"{cor_tcp_b2:.6f}")
    emit("R2A_CORRECTED_UDP_MEDIAN_MBPS", f"{cor_udp_all:.6f}")
    emit("R2A_CORRECTED_TCP_MEDIAN_MBPS", f"{cor_tcp_all:.6f}")
    emit("R2A_CORRECTED_UDP_VS_TCP_PCT", f"{pct(cor_udp_all, cor_tcp_all):.2f}")
    emit(
        "R2A_UDP_ACCOUNTING_INFLATION_PCT",
        f"{pct(raw_udp_all, cor_udp_all):.2f}",
    )
    emit(
        "R2A_TCP_ACCOUNTING_INFLATION_PCT",
        f"{pct(raw_tcp_all, cor_tcp_all):.2f}",
    )
    emit(
        "R2A_CORRECTED_UDP_VS_R1_MINIMAL_TAIL0_PCT",
        f"{pct(cor_udp_all, R1_MINIMAL_TAIL0_MBPS):.2f}",
    )
    emit("R2A_R1_MINIMAL_TAIL0_MBPS", f"{R1_MINIMAL_TAIL0_MBPS:.6f}")
    emit(
        "R2A_CORRECTED_METRIC_SCOPE",
        "NOMINAL_TAIL_ACCOUNTING_NOT_EXACT_CAPTURE_TIMESTAMP",
    )

    emit("R2A_LOG_COUNT", 12)
    emit("HARNESS_FAILURE", "NO")
    emit("PRODUCT_FAILURE", "NO_EVIDENCE")
    emit("HARDWARE_FAILURE", "NO_EVIDENCE")
    emit("A14_H4A03AR2A_EXISTING_P3K_ACCOUNTING_AUDIT", "PASS")
    emit("NEXT", "RETURN_TO_CHAT_INTERPRET_REPLAY_DO_NOT_ABLATE")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except SystemExit:
        raise
    except Exception as exc:
        emit("R2A_EXCEPTION", str(exc))
        emit("HARNESS_FAILURE", "YES")
        emit("PRODUCT_FAILURE", "NO_EVIDENCE")
        emit("HARDWARE_FAILURE", "NO_EVIDENCE")
        raise SystemExit(1)
