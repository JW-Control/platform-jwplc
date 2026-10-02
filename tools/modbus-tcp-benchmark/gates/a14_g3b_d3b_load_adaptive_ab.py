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

FAST_GAP_US = 1600
FAST_STREAK = 3
SLOW_GAP_US = 1800
SLOW_STREAK = 2
IDLE_EXIT_US = 5000


def compile_variant(
    cli: Path,
    fqbn: str,
    repo: Path,
    sketch: Path,
    libs: Path,
    build: Path,
    variant: str,
    log: Path,
) -> None:
    d3 = variant == "D3"
    int_value = "1" if d3 else "0"
    load_value = "1" if d3 else "0"

    flags = (
        "-DJWPLC_ETHERNET_ENABLE_PROFILE_HOOKS=1 "
        "-DJWPLC_MODBUS_TCP_ENABLE_PROFILE_HOOKS=1 "
        f"-DJWPLC_MODBUS_TCP_INT_GUIDED_RX={int_value} "
        "-DJWPLC_MODBUS_TCP_INT_HOT_POLL_US=0 "
        f"-DJWPLC_MODBUS_TCP_INT_LOAD_ADAPTIVE={load_value}"
    )

    cmd = [
        str(cli),
        "compile",
        "--verbose",
        "--fqbn",
        fqbn,
        "--build-path",
        str(build),
        "--libraries",
        str(libs),
        "--build-property",
        f"compiler.cpp.extra_flags={flags}",
        str(sketch),
    ]

    rc = common.run_logged(cmd, log, repo)
    common.emit(f"D3B_{variant}_COMPILE_EXIT", rc)

    if rc != 0:
        print(common.decode(log.read_bytes())[-8000:])
        raise RuntimeError(f"D3B_{variant}_COMPILE_FAILED")

    text = common.decode(log.read_bytes()).replace("\\", "/").lower()

    checks = {
        "ETH_PROFILE": "-djwplc_ethernet_enable_profile_hooks=1" in text,
        "MODBUS_PROFILE": "-djwplc_modbus_tcp_enable_profile_hooks=1" in text,
        "INT": f"-djwplc_modbus_tcp_int_guided_rx={int_value}" in text,
        "HOT": "-djwplc_modbus_tcp_int_hot_poll_us=0" in text,
        "LOAD": f"-djwplc_modbus_tcp_int_load_adaptive={load_value}" in text,
        "MODBUS_SOURCE": "jwplc_modbustcp.cpp" in text,
        "SOCKET_SOURCE": "socket.cpp" in text,
    }

    for key, ok in checks.items():
        common.emit(
            f"D3B_{variant}_COMPILE_{key}",
            "PASS" if ok else "FAIL",
        )

    if not all(checks.values()):
        raise RuntimeError(f"D3B_{variant}_COMPILE_CONTRACT_FAIL")


def run_text(cmd: list[str], repo: Path) -> str:
    proc = common.run(cmd, repo)
    text = common.decode(proc.stdout)
    print(text)

    if proc.returncode != 0:
        raise RuntimeError(
            f"D3B_SUBPROCESS_FAILED RC={proc.returncode}"
        )

    return text


def main() -> int:
    parser = argparse.ArgumentParser(
        description=(
            "Alpha14 G3B-D3-B load-adaptive Modbus TCP A/B"
        )
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
    header = (
        libs
        / "JWPLC_ModbusTCP/src/JWPLC_ModbusTCP.h"
    )
    source = (
        libs
        / "JWPLC_ModbusTCP/src/JWPLC_ModbusTCP.cpp"
    )

    print("=" * 78)
    print(" ALPHA14 G3B-D3-B - LOAD-ADAPTIVE MODBUS TCP A/B")
    print("=" * 78)

    branch = common.git(repo, "branch", "--show-current")
    head = common.git(repo, "rev-parse", "HEAD")

    common.emit("BRANCH", branch)
    common.emit("HEAD", head)
    common.emit(
        "D3B_RATES_REQ_S",
        ",".join(f"{rate:.0f}" for rate in RATES),
    )
    common.emit("D3B_DURATION_S", f"{DURATION_S:.0f}")
    common.emit("D3B_IDLE_DURATION_S", f"{IDLE_DURATION_S:.0f}")
    common.emit("D3B_FAST_GAP_US", FAST_GAP_US)
    common.emit("D3B_FAST_STREAK", FAST_STREAK)
    common.emit("D3B_SLOW_GAP_US", SLOW_GAP_US)
    common.emit("D3B_SLOW_STREAK", SLOW_STREAK)
    common.emit("D3B_IDLE_EXIT_US", IDLE_EXIT_US)
    common.emit("D3B_REVISION", "R1_FALLBACK_PHASE_AND_HOTPATH")
    common.emit("D3B_PRODUCT_DEFAULT_CHANGED", "NO")

    if branch != BRANCH:
        raise RuntimeError("D3B_BRANCH_MISMATCH")

    if common.git(repo, "diff", "--name-only"):
        raise RuntimeError("D3B_TREE_DIRTY")

    if common.git(repo, "diff", "--cached", "--name-only"):
        raise RuntimeError("D3B_INDEX_DIRTY")

    header_text = header.read_text(encoding="utf-8")
    source_text = source.read_text(encoding="utf-8")

    source_contract = {
        "DEFAULT_INT_OFF":
            "#define JWPLC_MODBUS_TCP_INT_GUIDED_RX 0"
            in header_text,
        "DEFAULT_HOT_OFF":
            "#define JWPLC_MODBUS_TCP_INT_HOT_POLL_US 0UL"
            in header_text,
        "DEFAULT_D3_OFF":
            "#define JWPLC_MODBUS_TCP_INT_LOAD_ADAPTIVE 0"
            in header_text,
        "FAST_GAP":
            "JWPLC_MODBUS_TCP_LOAD_FAST_GAP_US = 1600U"
            in source_text,
        "FAST_STREAK":
            "JWPLC_MODBUS_TCP_LOAD_FAST_STREAK = 3U"
            in source_text,
        "SLOW_GAP":
            "JWPLC_MODBUS_TCP_LOAD_SLOW_GAP_US = 1800U"
            in source_text,
        "SLOW_STREAK":
            "JWPLC_MODBUS_TCP_LOAD_SLOW_STREAK = 2U"
            in source_text,
        "IDLE_EXIT":
            "JWPLC_MODBUS_TCP_LOAD_IDLE_EXIT_US = 5000U"
            in source_text,
        "FRAME_ACTIVITY":
            "_stats.rxFrames++;\n    noteRxIntFrameActivity();"
            in source_text,
    }

    for key, ok in source_contract.items():
        common.emit(
            f"D3B_SOURCE_{key}",
            "PASS" if ok else "FAIL",
        )

    if not all(source_contract.values()):
        raise RuntimeError("D3B_SOURCE_CONTRACT_FAIL")

    ast.parse(high.read_text(encoding="utf-8"))
    ast.parse(idle.read_text(encoding="utf-8"))

    cli = common.find_cli(args.arduino_cli)
    root = Path(
        tempfile.mkdtemp(
            prefix="jwplc_a14_g3b_d3b_"
        )
    )
    common.emit("D3B_RESULT_ROOT", root)

    builds = {
        "POLLING": root / "build_polling",
        "D3": root / "build_d3",
    }

    for variant in ("POLLING", "D3"):
        compile_variant(
            cli,
            args.fqbn,
            repo,
            sketch,
            libs,
            builds[variant],
            variant,
            root / f"compile_{variant.lower()}.log",
        )

    rows: dict[float, dict[str, dict[str, float | str]]] = {}

    for rate_index, rate in enumerate(RATES):
        order = (
            ("POLLING", "D3")
            if rate_index % 2 == 0
            else ("D3", "POLLING")
        )
        rows[rate] = {}

        for variant in order:
            print()
            print("=" * 78)
            print(
                f" D3B RATE={rate:.0f} VARIANT={variant}"
            )
            print("=" * 78)

            d2.upload(
                cli,
                args.fqbn,
                repo,
                sketch,
                builds[variant],
                args.serial,
                root
                / f"upload_{int(rate)}_{variant.lower()}.log",
            )
            time.sleep(3)

            is_d3 = variant == "D3"
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
                "INT_GUIDED" if is_d3 else "POLLING",
                "--expected-hot-poll-us",
                "0",
                "--expected-load-adaptive",
                "YES" if is_d3 else "NO",
                "--require-scheduler-profile",
            ]

            text = run_text(cmd, repo)
            (
                root
                / f"rate_{int(rate)}_{variant.lower()}.log"
            ).write_text(text, encoding="utf-8")

            if d2.one(text, "G3A_FUNCTIONAL_PASS") != "YES":
                raise RuntimeError(
                    f"D3B_FUNCTIONAL_FAIL RATE={rate} "
                    f"VARIANT={variant}"
                )

            rows[rate][variant] = {
                "req": d2.num(
                    text,
                    "G3A_ACHIEVED_REQ_S",
                ),
                "p95": d2.num(
                    text,
                    "G3A_P95_US",
                ),
                "p99": d2.num(
                    text,
                    "G3A_P99_US",
                ),
                "avg": d2.num(
                    text,
                    "G3A_LAT_AVG_US",
                ),
                "status": d2.num(
                    text,
                    "G3A_STATUS_CALLS",
                ),
                "available": d2.num(
                    text,
                    "G3A_AVAILABLE_CALLS",
                ),
                "loop_avg": d2.num(
                    text,
                    "G3A_LOOP_AVG_US",
                ),
                "d3_state": d2.one(
                    text,
                    "G3A_D3_STATE",
                ),
                "d3_frames": d2.num(
                    text,
                    "G3A_D3_COMPLETE_FRAMES",
                ),
                "d3_warm": d2.num(
                    text,
                    "G3A_D3_TO_WARM",
                ),
                "d3_active": d2.num(
                    text,
                    "G3A_D3_TO_ACTIVE_POLL",
                ),
                "d3_cooldown": d2.num(
                    text,
                    "G3A_D3_TO_COOLDOWN",
                ),
                "d3_idle": d2.num(
                    text,
                    "G3A_D3_TO_IDLE_INT",
                ),
                "d3_passes": d2.num(
                    text,
                    "G3A_D3_ACTIVE_POLL_PASSES",
                ),
                "d3_fallback": d2.num(
                    text,
                    "G3A_D3_FALLBACK_PASSES",
                ),
                "d3_realigns": d2.num(
                    text,
                    "G3A_D3_IDLE_FALLBACK_REALIGNS",
                ),
                "d3_last_gap": d2.num(
                    text,
                    "G3A_D3_LAST_FRAME_GAP_US",
                ),
            }

    idle_rows: dict[str, dict[str, float | str]] = {}

    for variant in ("POLLING", "D3"):
        print()
        print("=" * 78)
        print(f" D3B IDLE VARIANT={variant}")
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

        is_d3 = variant == "D3"
        cmd = [
            sys.executable,
            "-B",
            str(idle),
            "--serial",
            args.serial,
            "--duration",
            str(IDLE_DURATION_S),
            "--variant",
            "ADAPTIVE" if is_d3 else "POLLING",
            "--expected-hot-poll-us",
            "0",
            "--expected-load-adaptive",
            "YES" if is_d3 else "NO",
            "--require-scheduler-profile",
        ]

        text = run_text(cmd, repo)
        (
            root / f"idle_{variant.lower()}.log"
        ).write_text(text, encoding="utf-8")

        if d2.one(text, "D2_IDLE_FUNCTIONAL_PASS") != "YES":
            raise RuntimeError(
                f"D3B_IDLE_FUNCTIONAL_FAIL VARIANT={variant}"
            )

        idle_rows[variant] = {
            "status": d2.num(
                text,
                "D2_IDLE_STATUS_CALLS",
            ),
            "available": d2.num(
                text,
                "D2_IDLE_AVAILABLE_CALLS",
            ),
            "loop_avg": d2.num(
                text,
                "D2_IDLE_LOOP_AVG_US",
            ),
            "d3_state": d2.one(
                text,
                "D2_IDLE_D3_STATE",
            ),
            "d3_active": d2.num(
                text,
                "D2_IDLE_D3_TO_ACTIVE_POLL",
            ),
            "d3_idle": d2.num(
                text,
                "D2_IDLE_D3_TO_IDLE_INT",
            ),
            "d3_passes": d2.num(
                text,
                "D2_IDLE_D3_ACTIVE_POLL_PASSES",
            ),
        }

    summary: list[str] = []

    def emit(key: str, value: object) -> None:
        line = f"{key}={value}"
        summary.append(line)
        print(line)

    print()
    print("=" * 78)
    print(" G3B-D3-B SUMMARY")
    print("=" * 78)

    for rate in RATES:
        poll = rows[rate]["POLLING"]
        d3row = rows[rate]["D3"]

        emit(
            f"D3B_{int(rate)}_POLLING_REQ_S",
            f"{poll['req']:.3f}",
        )
        emit(
            f"D3B_{int(rate)}_D3_REQ_S",
            f"{d3row['req']:.3f}",
        )
        emit(
            f"D3B_{int(rate)}_POLLING_P95_US",
            f"{poll['p95']:.3f}",
        )
        emit(
            f"D3B_{int(rate)}_D3_P95_US",
            f"{d3row['p95']:.3f}",
        )
        emit(
            f"D3B_{int(rate)}_POLLING_P99_US",
            f"{poll['p99']:.3f}",
        )
        emit(
            f"D3B_{int(rate)}_D3_P99_US",
            f"{d3row['p99']:.3f}",
        )
        emit(
            f"D3B_{int(rate)}_D3_P95_DELTA_PCT",
            f"{d2.delta(d3row['p95'], poll['p95']):.3f}",
        )
        emit(
            f"D3B_{int(rate)}_D3_P99_DELTA_PCT",
            f"{d2.delta(d3row['p99'], poll['p99']):.3f}",
        )

        status_ratio = (
            d3row["status"] / poll["status"] * 100.0
            if poll["status"] > 0
            else 0.0
        )
        available_ratio = (
            d3row["available"] / poll["available"] * 100.0
            if poll["available"] > 0
            else 0.0
        )

        emit(
            f"D3B_{int(rate)}_D3_STATUS_RATIO_PCT",
            f"{status_ratio:.3f}",
        )
        emit(
            f"D3B_{int(rate)}_D3_AVAILABLE_RATIO_PCT",
            f"{available_ratio:.3f}",
        )
        emit(
            f"D3B_{int(rate)}_D3_LOOP_AVG_US",
            f"{d3row['loop_avg']:.0f}",
        )
        emit(
            f"D3B_{int(rate)}_D3_STATE",
            d3row["d3_state"],
        )
        emit(
            f"D3B_{int(rate)}_D3_COMPLETE_FRAMES",
            f"{d3row['d3_frames']:.0f}",
        )
        emit(
            f"D3B_{int(rate)}_D3_TO_WARM",
            f"{d3row['d3_warm']:.0f}",
        )
        emit(
            f"D3B_{int(rate)}_D3_TO_ACTIVE_POLL",
            f"{d3row['d3_active']:.0f}",
        )
        emit(
            f"D3B_{int(rate)}_D3_TO_COOLDOWN",
            f"{d3row['d3_cooldown']:.0f}",
        )
        emit(
            f"D3B_{int(rate)}_D3_TO_IDLE_INT",
            f"{d3row['d3_idle']:.0f}",
        )
        emit(
            f"D3B_{int(rate)}_D3_ACTIVE_POLL_PASSES",
            f"{d3row['d3_passes']:.0f}",
        )
        emit(
            f"D3B_{int(rate)}_D3_FALLBACK_PASSES",
            f"{d3row['d3_fallback']:.0f}",
        )
        emit(
            f"D3B_{int(rate)}_D3_IDLE_FALLBACK_REALIGNS",
            f"{d3row['d3_realigns']:.0f}",
        )
        emit(
            f"D3B_{int(rate)}_D3_LAST_FRAME_GAP_US",
            f"{d3row['d3_last_gap']:.0f}",
        )

    idle_status_ratio = (
        idle_rows["D3"]["status"]
        / idle_rows["POLLING"]["status"]
        * 100.0
    )
    idle_available_ratio = (
        idle_rows["D3"]["available"]
        / idle_rows["POLLING"]["available"]
        * 100.0
    )

    emit(
        "D3B_IDLE_D3_STATUS_RATIO_PCT",
        f"{idle_status_ratio:.3f}",
    )
    emit(
        "D3B_IDLE_D3_AVAILABLE_RATIO_PCT",
        f"{idle_available_ratio:.3f}",
    )
    emit(
        "D3B_IDLE_D3_STATE",
        idle_rows["D3"]["d3_state"],
    )
    emit(
        "D3B_IDLE_D3_TO_ACTIVE_POLL",
        f"{idle_rows['D3']['d3_active']:.0f}",
    )
    emit(
        "D3B_IDLE_D3_TO_IDLE_INT",
        f"{idle_rows['D3']['d3_idle']:.0f}",
    )
    emit(
        "D3B_IDLE_D3_ACTIVE_POLL_PASSES",
        f"{idle_rows['D3']['d3_passes']:.0f}",
    )

    idle_state_ok = (
        idle_rows["D3"]["d3_state"] == "IDLE_INT"
    )
    emit(
        "D3B_IDLE_STATE_POLICY",
        "PASS" if idle_state_ok else "REVIEW",
    )
    emit("D3B_FUNCTIONAL_MATRIX", "PASS")
    emit("D3B_PRODUCT_DEFAULT_CHANGED", "NO")
    emit(
        "D3B_NEXT",
        "REVIEW_D3B_LOAD_CURVE_BEFORE_D3C",
    )
    emit(
        "D3B_STATUS",
        "PASS_CHARACTERIZATION"
        if idle_state_ok
        else "REVIEW",
    )

    summary_path = root / "SUMMARY.log"
    summary_path.write_text(
        "\n".join(summary) + "\n",
        encoding="utf-8",
    )
    common.emit("D3B_SUMMARY_LOG", summary_path)

    return 0 if idle_state_ok else 2


if __name__ == "__main__":
    raise SystemExit(main())
