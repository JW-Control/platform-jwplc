#!/usr/bin/env python3
from __future__ import annotations

import argparse
import ast
import sys
import tempfile
import time
from pathlib import Path

import a14_h4a04p1_tcp_rx_bottleneck_profile as common
import a14_g3b_d2_scheduler_ab as d2

BRANCH = "v2.1.0-alpha.14/feature/modbus-tcp"
RATES = (100.0, 500.0, 750.0, 1000.0)
DURATION_S = 15.0
IDLE_DURATION_S = 10.0


def run_text(cmd: list[str], repo: Path) -> str:
    proc = common.run(cmd, repo)
    text = common.decode(proc.stdout)
    print(text)
    if proc.returncode != 0:
        raise RuntimeError(f"M0A_SUBPROCESS_FAILED RC={proc.returncode}")
    return text


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Alpha14 G3B-D3-M0A Modbus TCP load characterization"
    )
    parser.add_argument("--serial", default="COM14")
    parser.add_argument("--arduino-cli")
    parser.add_argument("--fqbn", default="jwplc_local:esp32:jwplcbasic")
    args = parser.parse_args()

    repo = Path(__file__).resolve().parents[3]
    sketch = (
        repo
        / "tools/modbus-tcp-benchmark/firmware"
        / "a14_g3a_modbus_tcp_int_product"
    )
    high = (
        repo
        / "tools/modbus-tcp-benchmark/pc"
        / "a14_g3a_modbus_tcp_int_case.py"
    )
    idle = (
        repo
        / "tools/modbus-tcp-benchmark/pc"
        / "a14_g3b_d2_idle_case.py"
    )
    libs = repo / "JWPLC/2.1.0/libraries"

    print("=" * 78)
    print(" ALPHA14 G3B-D3-M0A - MODBUS TCP LOAD CHARACTERIZATION")
    print("=" * 78)

    branch = common.git(repo, "branch", "--show-current")
    head = common.git(repo, "rev-parse", "HEAD")

    common.emit("BRANCH", branch)
    common.emit("HEAD", head)
    common.emit("M0A_RATES_REQ_S", ",".join(f"{x:.0f}" for x in RATES))
    common.emit("M0A_DURATION_S", f"{DURATION_S:.0f}")
    common.emit("M0A_IDLE_DURATION_S", f"{IDLE_DURATION_S:.0f}")
    common.emit("M0A_D2_HOT_POLL_US", d2.HOT_US)
    common.emit("M0A_PRODUCT_SOURCE_MUTATION", "NO")

    if branch != BRANCH:
        raise RuntimeError("M0A_BRANCH_MISMATCH")
    if common.git(repo, "diff", "--name-only"):
        raise RuntimeError("M0A_TREE_DIRTY")
    if common.git(repo, "diff", "--cached", "--name-only"):
        raise RuntimeError("M0A_INDEX_DIRTY")

    ast.parse(high.read_text(encoding="utf-8"))
    ast.parse(idle.read_text(encoding="utf-8"))

    cli = common.find_cli(args.arduino_cli)
    root = Path(tempfile.mkdtemp(prefix="jwplc_a14_g3b_d3_m0a_"))
    common.emit("M0A_RESULT_ROOT", root)

    builds = {
        "POLLING": root / "build_polling",
        "ADAPTIVE": root / "build_adaptive",
    }

    for variant in ("POLLING", "ADAPTIVE"):
        d2.compile_variant(
            cli,
            args.fqbn,
            repo,
            sketch,
            libs,
            builds[variant],
            variant,
            root / f"compile_{variant.lower()}.log",
        )

    rows: dict[float, dict[str, dict[str, float]]] = {}

    for rate_index, rate in enumerate(RATES):
        order = (
            ("POLLING", "ADAPTIVE")
            if rate_index % 2 == 0
            else ("ADAPTIVE", "POLLING")
        )
        rows[rate] = {}

        for variant in order:
            print()
            print("=" * 78)
            print(f" M0A RATE={rate:.0f} VARIANT={variant}")
            print("=" * 78)

            d2.upload(
                cli,
                args.fqbn,
                repo,
                sketch,
                builds[variant],
                args.serial,
                root / f"upload_{int(rate)}_{variant.lower()}.log",
            )
            time.sleep(3)

            runner_variant = (
                "INT_GUIDED" if variant == "ADAPTIVE" else "POLLING"
            )

            cmd = [
                sys.executable,
                "-B",
                str(high),
                "--serial",
                args.serial,
                "--duration",
                str(DURATION_S),
                "--rate",
                str(rate),
                "--variant",
                runner_variant,
            ]

            text = run_text(cmd, repo)
            (
                root / f"rate_{int(rate)}_{variant.lower()}.log"
            ).write_text(text, encoding="utf-8")

            if d2.one(text, "G3A_FUNCTIONAL_PASS") != "YES":
                raise RuntimeError(
                    f"M0A_FUNCTIONAL_FAIL RATE={rate} VARIANT={variant}"
                )

            rows[rate][variant] = {
                "req": d2.num(text, "G3A_ACHIEVED_REQ_S"),
                "avg": d2.num(text, "G3A_LAT_AVG_US"),
                "p95": d2.num(text, "G3A_P95_US"),
                "p99": d2.num(text, "G3A_P99_US"),
                "max": d2.num(text, "G3A_MAX_US"),
                "loop_avg": d2.num(text, "G3A_LOOP_AVG_US"),
                "loop_max": d2.num(text, "G3A_LOOP_MAX_US"),
                "status": d2.num(text, "G3A_STATUS_CALLS"),
                "available": d2.num(text, "G3A_AVAILABLE_CALLS"),
                "zero": d2.num(text, "G3A_AVAILABLE_ZERO"),
            }

    idle_rows: dict[str, dict[str, float]] = {}

    for variant in ("POLLING", "ADAPTIVE"):
        print()
        print("=" * 78)
        print(f" M0A IDLE VARIANT={variant}")
        print("=" * 78)

        d2.upload(
            cli,
            args.fqbn,
            repo,
            sketch,
            builds[variant],
            args.serial,
            root / f"idle_upload_{variant.lower()}.log",
        )
        time.sleep(3)

        hot = d2.HOT_US if variant == "ADAPTIVE" else 0
        cmd = [
            sys.executable,
            "-B",
            str(idle),
            "--serial",
            args.serial,
            "--duration",
            str(IDLE_DURATION_S),
            "--variant",
            variant,
            "--expected-hot-poll-us",
            str(hot),
        ]

        text = run_text(cmd, repo)
        (
            root / f"idle_{variant.lower()}.log"
        ).write_text(text, encoding="utf-8")

        if d2.one(text, "D2_IDLE_FUNCTIONAL_PASS") != "YES":
            raise RuntimeError(f"M0A_IDLE_FAIL VARIANT={variant}")

        idle_rows[variant] = {
            "status": d2.num(text, "D2_IDLE_STATUS_CALLS"),
            "available": d2.num(text, "D2_IDLE_AVAILABLE_CALLS"),
            "loop_avg": d2.num(text, "D2_IDLE_LOOP_AVG_US"),
            "loop_max": d2.num(text, "D2_IDLE_LOOP_MAX_US"),
        }

    summary: list[str] = []

    def emit(key: str, value: object) -> None:
        line = f"{key}={value}"
        summary.append(line)
        print(line)

    print()
    print("=" * 78)
    print(" G3B-D3-M0A SUMMARY")
    print("=" * 78)

    for rate in RATES:
        poll = rows[rate]["POLLING"]
        adaptive = rows[rate]["ADAPTIVE"]

        emit(f"M0A_{int(rate)}_POLLING_REQ_S", f"{poll['req']:.3f}")
        emit(f"M0A_{int(rate)}_ADAPTIVE_REQ_S", f"{adaptive['req']:.3f}")
        emit(f"M0A_{int(rate)}_POLLING_P95_US", f"{poll['p95']:.3f}")
        emit(f"M0A_{int(rate)}_ADAPTIVE_P95_US", f"{adaptive['p95']:.3f}")
        emit(f"M0A_{int(rate)}_POLLING_P99_US", f"{poll['p99']:.3f}")
        emit(f"M0A_{int(rate)}_ADAPTIVE_P99_US", f"{adaptive['p99']:.3f}")

        status_ratio = (
            adaptive["status"] / poll["status"] * 100.0
            if poll["status"] > 0
            else 0.0
        )
        available_ratio = (
            adaptive["available"] / poll["available"] * 100.0
            if poll["available"] > 0
            else 0.0
        )
        p95_delta = d2.delta(adaptive["p95"], poll["p95"])
        p99_delta = d2.delta(adaptive["p99"], poll["p99"])

        emit(
            f"M0A_{int(rate)}_ADAPTIVE_STATUS_RATIO_PCT",
            f"{status_ratio:.3f}",
        )
        emit(
            f"M0A_{int(rate)}_ADAPTIVE_AVAILABLE_RATIO_PCT",
            f"{available_ratio:.3f}",
        )
        emit(
            f"M0A_{int(rate)}_ADAPTIVE_P95_DELTA_PCT",
            f"{p95_delta:.3f}",
        )
        emit(
            f"M0A_{int(rate)}_ADAPTIVE_P99_DELTA_PCT",
            f"{p99_delta:.3f}",
        )
        emit(
            f"M0A_{int(rate)}_POLLING_LOOP_AVG_US",
            f"{poll['loop_avg']:.0f}",
        )
        emit(
            f"M0A_{int(rate)}_ADAPTIVE_LOOP_AVG_US",
            f"{adaptive['loop_avg']:.0f}",
        )

    idle_status_ratio = (
        idle_rows["ADAPTIVE"]["status"]
        / idle_rows["POLLING"]["status"]
        * 100.0
    )
    idle_available_ratio = (
        idle_rows["ADAPTIVE"]["available"]
        / idle_rows["POLLING"]["available"]
        * 100.0
    )

    emit("M0A_IDLE_ADAPTIVE_STATUS_RATIO_PCT", f"{idle_status_ratio:.3f}")
    emit(
        "M0A_IDLE_ADAPTIVE_AVAILABLE_RATIO_PCT",
        f"{idle_available_ratio:.3f}",
    )
    emit(
        "M0A_IDLE_POLLING_LOOP_AVG_US",
        f"{idle_rows['POLLING']['loop_avg']:.0f}",
    )
    emit(
        "M0A_IDLE_ADAPTIVE_LOOP_AVG_US",
        f"{idle_rows['ADAPTIVE']['loop_avg']:.0f}",
    )

    emit("M0A_FUNCTIONAL_MATRIX", "PASS")
    emit("M0A_THRESHOLDS_DECIDED", "NO")
    emit("M0A_PRODUCT_DEFAULT_CHANGED", "NO")
    emit("M0A_NEXT", "REVIEW_LOAD_CURVE_THEN_M0B_RAW_TCP")
    emit("M0A_STATUS", "PASS_CHARACTERIZATION")

    summary_path = root / "SUMMARY.log"
    summary_path.write_text("\n".join(summary) + "\n", encoding="utf-8")
    common.emit("M0A_SUMMARY_LOG", summary_path)

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
