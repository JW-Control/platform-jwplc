#!/usr/bin/env python3
from __future__ import annotations

import argparse
import ast
import tempfile
import time
from pathlib import Path
import sys

import a14_h4a04p1_tcp_rx_bottleneck_profile as p1


BRANCH = "v2.1.0-alpha.14/feature/modbus-tcp"
CASES = (
    ("IDLE", 15.0, "POLLING", False),
    ("IDLE", 15.0, "INT_GUIDED", False),
    ("CONTROLLED", 30.0, "INT_GUIDED", True),
    ("CONTROLLED", 30.0, "POLLING", False),
    ("SATURATED", 30.0, "POLLING", False),
    ("SATURATED", 30.0, "INT_GUIDED", False),
)


def one(text: str, key: str) -> str:
    return p1.one(text, key)


def number(text: str, key: str) -> float:
    return p1.number(text, key)


def delta_pct(value: float, reference: float) -> float:
    if reference <= 0:
        return 0.0
    return (value / reference - 1.0) * 100.0


def reduction_pct(value: float, reference: float) -> float:
    if reference <= 0:
        return 0.0
    return (1.0 - value / reference) * 100.0


def compile_variant(
    *,
    cli: Path,
    fqbn: str,
    repo: Path,
    sketch: Path,
    libraries: Path,
    build_dir: Path,
    variant: str,
    log: Path,
) -> None:
    int_value = "1" if variant == "INT_GUIDED" else "0"

    flags = (
        "-DJWPLC_ETHERNET_ENABLE_PROFILE_HOOKS=1 "
        "-DJWPLC_H4A04P8_REUSE_CONNECTED_RESULT=1 "
        f"-DJWPLC_G2_TCP_INT_GUIDED={int_value}"
    )

    cmd = [
        str(cli),
        "compile",
        "--verbose",
        "--fqbn",
        fqbn,
        "--build-path",
        str(build_dir),
        "--libraries",
        str(libraries),
        "--build-property",
        f"compiler.cpp.extra_flags={flags}",
        str(sketch),
    ]

    exit_code = p1.run_logged(cmd, log, repo)
    p1.emit(f"G2_{variant}_COMPILE_EXIT", exit_code)

    if exit_code != 0:
        print(p1.decode(log.read_bytes())[-8000:])
        raise RuntimeError(f"G2_{variant}_COMPILE_FAILED")

    normalized = p1.decode(log.read_bytes()).replace("\\", "/").lower()
    checks = {
        "PROFILE_HOOKS":
            "-djwplc_ethernet_enable_profile_hooks=1" in normalized,
        "SINGLE_STATUS":
            "-djwplc_h4a04p8_reuse_connected_result=1" in normalized,
        "INT_VARIANT":
            f"-djwplc_g2_tcp_int_guided={int_value}" in normalized,
        "ETHERNETCLIENT_SOURCE": "ethernetclient.cpp" in normalized,
        "SOCKET_SOURCE": "socket.cpp" in normalized,
    }

    for label, ok in checks.items():
        p1.emit(
            f"G2_{variant}_COMPILE_{label}",
            "PASS" if ok else "FAIL",
        )

    failed = [label for label, ok in checks.items() if not ok]
    if failed:
        raise RuntimeError(
            f"G2_{variant}_COMPILE_CONTRACT_FAILED="
            + ",".join(failed)
        )


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Alpha14 G2 TCP polling vs W5500 INT GPIO15"
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
    print(" ALPHA14 G2 - TCP POLLING VS W5500 INT GPIO15")
    print("=" * 78)

    branch = p1.git(repo, "branch", "--show-current")
    head = p1.git(repo, "rev-parse", "HEAD")

    p1.emit("BRANCH", branch)
    p1.emit("HEAD", head)
    p1.emit("G2_VARIANTS", "POLLING,INT_GUIDED")
    p1.emit("G2_LOADS", "IDLE,CONTROLLED,SATURATED")
    p1.emit("G2_PROFILE_HOOKS", "ON_BOTH")
    p1.emit("G2_SINGLE_STATUS_SAME_PASS", "ON_BOTH")
    p1.emit("G2_ETH_INT_PIN", 15)
    p1.emit("G2_ISR_SPI_ACCESS", "NO")
    p1.emit("G2_PRODUCT_POLICY_CHANGE", "NO")
    p1.emit("G2_INT_POLICY_PRODUCTIZED", "NO")

    if branch != BRANCH:
        raise RuntimeError("G2_BRANCH_MISMATCH")
    if p1.git(repo, "diff", "--name-only") or p1.git(
        repo,
        "diff",
        "--cached",
        "--name-only",
    ):
        raise RuntimeError("G2_TRACKED_TREE_NOT_CLEAN")

    ast.parse(runner.read_text(encoding="utf-8"), filename=str(runner))
    p1.emit("G2_RUNNER_AST", "PASS")

    cli = p1.find_cli(args.arduino_cli)
    p1.emit("ARDUINO_CLI", cli)

    result_root = Path(
        tempfile.mkdtemp(prefix="jwplc_a14_g2_tcp_int_ab_")
    )
    p1.emit("G2_RESULT_ROOT", result_root)

    builds = {
        "POLLING": result_root / "build_polling",
        "INT_GUIDED": result_root / "build_int",
    }

    for variant in ("POLLING", "INT_GUIDED"):
        compile_variant(
            cli=cli,
            fqbn=args.fqbn,
            repo=repo,
            sketch=sketch,
            libraries=libraries,
            build_dir=builds[variant],
            variant=variant,
            log=result_root / f"compile_{variant.lower()}.log",
        )

    results: dict[tuple[str, str], dict[str, float]] = {}
    reconnect_pass = False

    for index, (load, duration, variant, reconnect) in enumerate(CASES, 1):
        print()
        print("=" * 78)
        print(f" G2 CASE {index}/{len(CASES)} {load} {variant}")
        print("=" * 78)

        upload_log = (
            result_root
            / f"{index:02d}_{load.lower()}_{variant.lower()}_upload.log"
        )
        upload_exit = p1.run_logged(
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
        p1.emit(f"G2_{load}_{variant}_UPLOAD_EXIT", upload_exit)

        if upload_exit != 0:
            print(p1.decode(upload_log.read_bytes())[-5000:])
            raise RuntimeError(f"G2_{load}_{variant}_UPLOAD_FAILED")

        time.sleep(3.0)

        case_log = (
            result_root
            / f"{index:02d}_{load.lower()}_{variant.lower()}.log"
        )
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

        proc = p1.run(cmd, repo)
        case_log.write_bytes(proc.stdout)
        text = p1.decode(proc.stdout)
        print(text)

        if proc.returncode != 0:
            raise RuntimeError(f"G2_{load}_{variant}_CASE_FAILED")
        if one(text, "G2_FUNCTIONAL_PASS") != "YES":
            raise RuntimeError(f"G2_{load}_{variant}_FUNCTIONAL_FAIL")

        if reconnect:
            reconnect_pass = one(text, "G2_RECONNECT_PASS") == "YES"
            if not reconnect_pass:
                raise RuntimeError("G2_INT_RECONNECT_FAIL")

        results[(load, variant)] = {
            "dut_mbps": number(text, "G2_DUT_MBPS"),
            "pc_rate": number(text, "G2_PC_OPERATIONS_PER_S"),
            "hold_count": number(text, "G2_TCP_SPI_HOLD_COUNT"),
            "hold_total_us": number(text, "G2_TCP_SPI_HOLD_TOTAL_US"),
            "hold_max_us": number(text, "G2_TCP_SPI_HOLD_MAX_US"),
            "occupancy_pct": number(text, "G2_SPI_HOLD_OCCUPANCY_PCT"),
            "service_passes": number(text, "G2_TCP_SERVICE_PASSES"),
            "service_empty_pct": number(text, "G2_TCP_SERVICE_EMPTY_PCT"),
            "status_calls": number(text, "G2_TCP_STATUS_CALLS"),
            "available_calls": number(text, "G2_TCP_AVAILABLE_CALLS"),
            "available_zero": number(text, "G2_TCP_AVAILABLE_ZERO"),
            "available_zero_pct": number(text, "G2_TCP_AVAILABLE_ZERO_PCT"),
            "int_skip": number(text, "G2_TCP_INT_SKIP_COUNT"),
            "int_wake": number(text, "G2_TCP_INT_WAKE_COUNT"),
            "int_isr": number(text, "G2_ETH_INT_ISR_COUNT"),
        }

    summary: list[str] = []

    def out(key: str, value: object) -> None:
        line = f"{key}={value}"
        summary.append(line)
        print(line)

    print()
    print("=" * 78)
    print(" G2 SUMMARY")
    print("=" * 78)

    for load in ("IDLE", "CONTROLLED", "SATURATED"):
        poll = results[(load, "POLLING")]
        intr = results[(load, "INT_GUIDED")]

        out(f"G2_{load}_POLLING_SPI_OCCUPANCY_PCT", f"{poll['occupancy_pct']:.3f}")
        out(f"G2_{load}_INT_SPI_OCCUPANCY_PCT", f"{intr['occupancy_pct']:.3f}")
        out(
            f"G2_{load}_INT_SPI_OCCUPANCY_DELTA_PP",
            f"{intr['occupancy_pct'] - poll['occupancy_pct']:.3f}",
        )
        out(
            f"G2_{load}_INT_HOLD_COUNT_REDUCTION_PCT",
            f"{reduction_pct(intr['hold_count'], poll['hold_count']):.3f}",
        )
        out(f"G2_{load}_POLLING_AVAILABLE_ZERO_PCT", f"{poll['available_zero_pct']:.3f}")
        out(f"G2_{load}_INT_AVAILABLE_ZERO_PCT", f"{intr['available_zero_pct']:.3f}")
        out(
            f"G2_{load}_INT_STATUS_CALL_REDUCTION_PCT",
            f"{reduction_pct(intr['status_calls'], poll['status_calls']):.3f}",
        )
        out(f"G2_{load}_INT_SKIP_COUNT", f"{intr['int_skip']:.0f}")
        out(f"G2_{load}_INT_WAKE_COUNT", f"{intr['int_wake']:.0f}")
        out(f"G2_{load}_INT_ISR_COUNT", f"{intr['int_isr']:.0f}")

        if load != "IDLE":
            out(
                f"G2_{load}_INT_VS_POLLING_DUT_MBPS_PCT",
                f"{delta_pct(intr['dut_mbps'], poll['dut_mbps']):.3f}",
            )

    idle_bus = (
        results[("IDLE", "INT_GUIDED")]["occupancy_pct"]
        < results[("IDLE", "POLLING")]["occupancy_pct"]
    )
    controlled_bus = (
        results[("CONTROLLED", "INT_GUIDED")]["occupancy_pct"]
        < results[("CONTROLLED", "POLLING")]["occupancy_pct"]
    )

    out("G2_IDLE_BUS_EFFICIENCY_SIGNAL", "PASS" if idle_bus else "REVIEW")
    out(
        "G2_CONTROLLED_BUS_EFFICIENCY_SIGNAL",
        "PASS" if controlled_bus else "REVIEW",
    )
    out("G2_INT_RECONNECT_LIVENESS", "PASS" if reconnect_pass else "FAIL")
    out("G2_INT_TCP_DECISION", "REVIEW_REQUIRED")
    out("G2_PRODUCT_PROMOTION", "NOT_YET")
    out("G2_PRODUCT_POLICY_CHANGE", "NO")
    out("G2_STATUS", "PASS")

    summary_path = result_root / "SUMMARY.log"
    summary_path.write_text("\n".join(summary) + "\n", encoding="utf-8")
    p1.emit("G2_SUMMARY_LOG", summary_path)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
