#!/usr/bin/env python3
from __future__ import annotations

import argparse
import ast
import statistics
import tempfile
import time
from pathlib import Path
import sys

import a14_g2_tcp_int_guided_ab as g2


BRANCH = "v2.1.0-alpha.14/feature/modbus-tcp"

CONTROLLED_DURATION_S = 20.0
SATURATED_DURATION_S = 30.0

CONTROLLED_ORDER = (
    ("POLLING", False),
    ("INT_GUIDED", True),
    ("INT_GUIDED", False),
    ("POLLING", False),
    ("POLLING", False),
    ("INT_GUIDED", False),
)

SATURATED_ORDER = (
    ("INT_GUIDED", False),
    ("POLLING", False),
    ("POLLING", False),
    ("INT_GUIDED", False),
    ("INT_GUIDED", False),
    ("POLLING", False),
)


def median(values: list[float]) -> float:
    if not values:
        raise RuntimeError("G2R1_EMPTY_MEDIAN")
    return float(statistics.median(values))


def spread_pct(values: list[float]) -> float:
    med = median(values)
    if med <= 0:
        return 0.0
    return (max(values) - min(values)) * 100.0 / med


def delta_pct(value: float, reference: float) -> float:
    if reference <= 0:
        return 0.0
    return (value / reference - 1.0) * 100.0


def reduction_pct(value: float, reference: float) -> float:
    if reference <= 0:
        return 0.0
    return (1.0 - value / reference) * 100.0


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Alpha14 G2-R1 TCP INT repeatability confirmation"
    )
    parser.add_argument("--serial", default="COM14")
    parser.add_argument("--arduino-cli")
    parser.add_argument(
        "--fqbn",
        default="jwplc_local:esp32:jwplcbasic",
    )
    args = parser.parse_args()

    repo = Path(__file__).resolve().parents[3]
    sketch = (
        repo
        / "tools"
        / "modbus-tcp-benchmark"
        / "firmware"
        / "a14_h4a04p1_tcp_rx_profiler"
    )
    runner = (
        repo
        / "tools"
        / "modbus-tcp-benchmark"
        / "pc"
        / "a14_g2_tcp_int_case.py"
    )
    libraries = repo / "JWPLC" / "2.1.0" / "libraries"

    print("=" * 78)
    print(" ALPHA14 G2-R1 - TCP INT REPEATABILITY")
    print("=" * 78)

    branch = g2.p1.git(repo, "branch", "--show-current")
    head = g2.p1.git(repo, "rev-parse", "HEAD")

    g2.p1.emit("BRANCH", branch)
    g2.p1.emit("HEAD", head)
    g2.p1.emit("G2R1_RUNS_PER_VARIANT_PER_LOAD", 3)
    g2.p1.emit("G2R1_CONTROLLED_DURATION_S", CONTROLLED_DURATION_S)
    g2.p1.emit("G2R1_SATURATED_DURATION_S", SATURATED_DURATION_S)
    g2.p1.emit("G2R1_PROFILE_HOOKS", "ON_BOTH")
    g2.p1.emit("G2R1_SINGLE_STATUS_SAME_PASS", "ON_BOTH")
    g2.p1.emit("G2R1_FRESH_UPLOAD_PER_CASE", "YES")
    g2.p1.emit("G2R1_PRODUCT_POLICY_CHANGE", "NO")
    g2.p1.emit("G2R1_INT_PRODUCTIZED", "NO")

    if branch != BRANCH:
        raise RuntimeError("G2R1_BRANCH_MISMATCH")
    if g2.p1.git(repo, "diff", "--name-only") or g2.p1.git(
        repo,
        "diff",
        "--cached",
        "--name-only",
    ):
        raise RuntimeError("G2R1_TRACKED_TREE_NOT_CLEAN")

    ast.parse(runner.read_text(encoding="utf-8"), filename=str(runner))
    g2.p1.emit("G2R1_RUNNER_AST", "PASS")

    cli = g2.p1.find_cli(args.arduino_cli)
    g2.p1.emit("ARDUINO_CLI", cli)

    result_root = Path(
        tempfile.mkdtemp(prefix="jwplc_a14_g2r1_tcp_int_repeat_")
    )
    g2.p1.emit("G2R1_RESULT_ROOT", result_root)

    builds = {
        "POLLING": result_root / "build_polling",
        "INT_GUIDED": result_root / "build_int",
    }

    for variant in ("POLLING", "INT_GUIDED"):
        g2.compile_variant(
            cli=cli,
            fqbn=args.fqbn,
            repo=repo,
            sketch=sketch,
            libraries=libraries,
            build_dir=builds[variant],
            variant=variant,
            log=result_root / f"compile_{variant.lower()}.log",
        )

    rows: dict[tuple[str, str], list[dict[str, float]]] = {
        ("CONTROLLED", "POLLING"): [],
        ("CONTROLLED", "INT_GUIDED"): [],
        ("SATURATED", "POLLING"): [],
        ("SATURATED", "INT_GUIDED"): [],
    }

    reconnect_pass = False
    case_index = 0

    def execute_load(
        load: str,
        duration: float,
        order: tuple[tuple[str, bool], ...],
    ) -> None:
        nonlocal case_index, reconnect_pass

        counts = {"POLLING": 0, "INT_GUIDED": 0}

        for variant, reconnect in order:
            case_index += 1
            counts[variant] += 1
            run_no = counts[variant]

            print()
            print("=" * 78)
            print(
                f" G2-R1 CASE {case_index}/12 "
                f"{load} {variant} RUN {run_no}/3"
            )
            print("=" * 78)

            upload_log = result_root / (
                f"{case_index:02d}_{load.lower()}_"
                f"{variant.lower()}_upload.log"
            )
            upload_exit = g2.p1.run_logged(
                [
                    str(cli),
                    "upload",
                    "--fqbn",
                    args.fqbn,
                    "--port",
                    args.serial,
                    "--input-dir",
                    str(builds[variant]),
                    str(sketch),
                ],
                upload_log,
                repo,
            )
            g2.p1.emit(
                f"G2R1_{load}_{variant}_RUN{run_no}_UPLOAD_EXIT",
                upload_exit,
            )

            if upload_exit != 0:
                print(g2.p1.decode(upload_log.read_bytes())[-5000:])
                raise RuntimeError(
                    f"G2R1_{load}_{variant}_RUN{run_no}_UPLOAD_FAILED"
                )

            time.sleep(3.0)

            cmd = [
                sys.executable,
                "-B",
                str(runner),
                "--serial",
                args.serial,
                "--duration",
                str(duration),
                "--load",
                load,
                "--variant",
                variant,
            ]
            if reconnect:
                cmd.append("--reconnect-probe")

            case_log = result_root / (
                f"{case_index:02d}_{load.lower()}_"
                f"{variant.lower()}_run{run_no}.log"
            )
            proc = g2.p1.run(cmd, repo)
            case_log.write_bytes(proc.stdout)
            text = g2.p1.decode(proc.stdout)
            print(text)

            if proc.returncode != 0:
                raise RuntimeError(
                    f"G2R1_{load}_{variant}_RUN{run_no}_CASE_FAILED"
                )
            if g2.one(text, "G2_FUNCTIONAL_PASS") != "YES":
                raise RuntimeError(
                    f"G2R1_{load}_{variant}_RUN{run_no}_FUNCTIONAL_FAIL"
                )

            if reconnect:
                reconnect_pass = (
                    g2.one(text, "G2_RECONNECT_PASS") == "YES"
                )
                if not reconnect_pass:
                    raise RuntimeError("G2R1_RECONNECT_FAIL")

            row = {
                "dut_mbps": g2.number(text, "G2_DUT_MBPS"),
                "pc_rate": g2.number(text, "G2_PC_OPERATIONS_PER_S"),
                "hold_count": g2.number(text, "G2_TCP_SPI_HOLD_COUNT"),
                "occupancy_pct": g2.number(
                    text,
                    "G2_SPI_HOLD_OCCUPANCY_PCT",
                ),
                "service_empty_pct": g2.number(
                    text,
                    "G2_TCP_SERVICE_EMPTY_PCT",
                ),
                "status_calls": g2.number(
                    text,
                    "G2_TCP_STATUS_CALLS",
                ),
                "available_zero_pct": g2.number(
                    text,
                    "G2_TCP_AVAILABLE_ZERO_PCT",
                ),
                "int_skip": g2.number(
                    text,
                    "G2_TCP_INT_SKIP_COUNT",
                ),
                "int_wake": g2.number(
                    text,
                    "G2_TCP_INT_WAKE_COUNT",
                ),
            }

            rows[(load, variant)].append(row)

    execute_load(
        "CONTROLLED",
        CONTROLLED_DURATION_S,
        CONTROLLED_ORDER,
    )
    execute_load(
        "SATURATED",
        SATURATED_DURATION_S,
        SATURATED_ORDER,
    )

    summary: list[str] = []

    def out(key: str, value: object) -> None:
        line = f"{key}={value}"
        summary.append(line)
        print(line)

    print()
    print("=" * 78)
    print(" G2-R1 SUMMARY")
    print("=" * 78)

    stats: dict[tuple[str, str], dict[str, float]] = {}

    for load in ("CONTROLLED", "SATURATED"):
        for variant in ("POLLING", "INT_GUIDED"):
            bucket = rows[(load, variant)]
            dut_values = [r["dut_mbps"] for r in bucket]
            occ_values = [r["occupancy_pct"] for r in bucket]
            hold_values = [r["hold_count"] for r in bucket]
            status_values = [r["status_calls"] for r in bucket]
            empty_values = [r["service_empty_pct"] for r in bucket]
            zero_values = [r["available_zero_pct"] for r in bucket]
            rate_values = [r["pc_rate"] for r in bucket]

            item = {
                "dut_median": median(dut_values),
                "dut_spread": spread_pct(dut_values),
                "dut_min": min(dut_values),
                "dut_max": max(dut_values),
                "occ_median": median(occ_values),
                "occ_min": min(occ_values),
                "occ_max": max(occ_values),
                "hold_median": median(hold_values),
                "status_median": median(status_values),
                "empty_median": median(empty_values),
                "zero_median": median(zero_values),
                "rate_median": median(rate_values),
            }
            stats[(load, variant)] = item

            prefix = f"G2R1_{load}_{variant}"
            out(f"{prefix}_DUT_MBPS_MEDIAN", f"{item['dut_median']:.6f}")
            out(f"{prefix}_DUT_MBPS_SPREAD_PCT", f"{item['dut_spread']:.3f}")
            out(f"{prefix}_SPI_OCCUPANCY_MEDIAN_PCT", f"{item['occ_median']:.3f}")
            out(f"{prefix}_HOLD_COUNT_MEDIAN", f"{item['hold_median']:.0f}")
            out(f"{prefix}_STATUS_CALLS_MEDIAN", f"{item['status_median']:.0f}")
            out(f"{prefix}_SERVICE_EMPTY_MEDIAN_PCT", f"{item['empty_median']:.3f}")
            out(f"{prefix}_AVAILABLE_ZERO_MEDIAN_PCT", f"{item['zero_median']:.3f}")
            out(f"{prefix}_PC_RATE_MEDIAN", f"{item['rate_median']:.3f}")

    controlled_poll = stats[("CONTROLLED", "POLLING")]
    controlled_int = stats[("CONTROLLED", "INT_GUIDED")]
    saturated_poll = stats[("SATURATED", "POLLING")]
    saturated_int = stats[("SATURATED", "INT_GUIDED")]

    controlled_occ_delta = (
        controlled_int["occ_median"] - controlled_poll["occ_median"]
    )
    controlled_hold_reduction = reduction_pct(
        controlled_int["hold_median"],
        controlled_poll["hold_median"],
    )
    controlled_throughput_delta = delta_pct(
        controlled_int["dut_median"],
        controlled_poll["dut_median"],
    )

    saturated_occ_delta = (
        saturated_int["occ_median"] - saturated_poll["occ_median"]
    )
    saturated_hold_reduction = reduction_pct(
        saturated_int["hold_median"],
        saturated_poll["hold_median"],
    )
    saturated_throughput_delta = delta_pct(
        saturated_int["dut_median"],
        saturated_poll["dut_median"],
    )

    out(
        "G2R1_CONTROLLED_INT_SPI_OCCUPANCY_DELTA_PP",
        f"{controlled_occ_delta:.3f}",
    )
    out(
        "G2R1_CONTROLLED_INT_HOLD_COUNT_REDUCTION_PCT",
        f"{controlled_hold_reduction:.3f}",
    )
    out(
        "G2R1_CONTROLLED_INT_VS_POLLING_DUT_MBPS_PCT",
        f"{controlled_throughput_delta:.3f}",
    )
    out(
        "G2R1_SATURATED_INT_SPI_OCCUPANCY_DELTA_PP",
        f"{saturated_occ_delta:.3f}",
    )
    out(
        "G2R1_SATURATED_INT_HOLD_COUNT_REDUCTION_PCT",
        f"{saturated_hold_reduction:.3f}",
    )
    out(
        "G2R1_SATURATED_INT_VS_POLLING_DUT_MBPS_PCT",
        f"{saturated_throughput_delta:.3f}",
    )

    controlled_separated = (
        controlled_int["occ_max"] < controlled_poll["occ_min"]
    )
    controlled_nonregression = controlled_throughput_delta >= -1.0
    saturated_nonregression = saturated_throughput_delta >= -1.0
    saturated_gain = saturated_throughput_delta >= 2.0

    out(
        "G2R1_CONTROLLED_BUS_SEPARATION",
        "PASS" if controlled_separated else "REVIEW",
    )
    out(
        "G2R1_CONTROLLED_THROUGHPUT_NONREGRESSION",
        "PASS" if controlled_nonregression else "FAIL",
    )
    out(
        "G2R1_SATURATED_THROUGHPUT_NONREGRESSION",
        "PASS" if saturated_nonregression else "FAIL",
    )
    out(
        "G2R1_SATURATED_THROUGHPUT_GAIN",
        "CONFIRMED" if saturated_gain else "NOT_CONFIRMED",
    )
    out(
        "G2R1_INT_RECONNECT_LIVENESS",
        "PASS" if reconnect_pass else "FAIL",
    )

    ready = (
        controlled_separated
        and controlled_nonregression
        and saturated_nonregression
        and reconnect_pass
    )

    out(
        "G2R1_PRODUCTIZATION_DESIGN_READY",
        "YES" if ready else "NO",
    )
    out(
        "G2R1_NEXT",
        (
            "G3_INT_PRODUCTIZATION_DESIGN"
            if ready
            else "REVIEW_G2R1"
        ),
    )
    out("G2R1_PRODUCT_POLICY_CHANGE", "NO")
    out("G2R1_STATUS", "PASS" if ready else "REVIEW")

    summary_path = result_root / "SUMMARY.log"
    summary_path.write_text("\n".join(summary) + "\n", encoding="utf-8")
    g2.p1.emit("G2R1_SUMMARY_LOG", summary_path)

    return 0 if ready else 2


if __name__ == "__main__":
    raise SystemExit(main())
