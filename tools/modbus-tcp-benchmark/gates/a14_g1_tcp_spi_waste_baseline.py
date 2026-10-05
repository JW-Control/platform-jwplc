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
    ("IDLE", 15.0, "BASE"),
    ("IDLE", 15.0, "PROFILE"),
    ("CONTROLLED", 30.0, "BASE"),
    ("CONTROLLED", 30.0, "PROFILE"),
    ("SATURATED", 30.0, "BASE"),
    ("SATURATED", 30.0, "PROFILE"),
)


def one(text: str, key: str) -> str:
    return p1.one(text, key)


def number(text: str, key: str) -> float:
    return p1.number(text, key)


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Alpha14 G1 TCP SPI waste baseline"
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
        / "a14_g1_tcp_spi_waste_case.py"
    )
    libraries = repo / "JWPLC" / "2.1.0" / "libraries"

    print("=" * 78)
    print(" ALPHA14 G1 - TCP SPI WASTE BASELINE")
    print("=" * 78)

    branch = p1.git(repo, "branch", "--show-current")
    head = p1.git(repo, "rev-parse", "HEAD")
    p1.emit("BRANCH", branch)
    p1.emit("HEAD", head)
    p1.emit("G1_PROFILE_DEFAULT", "OFF")
    p1.emit("G1_NORMAL_PRODUCT_POLICY_CHANGED", "NO")
    p1.emit("G1_INT_IMPLEMENTED", "NO")
    p1.emit("G1_LOADS", "IDLE,CONTROLLED,SATURATED")
    p1.emit("G1_CONTROLLED_MODEL", "RAW_TCP_MODBUS_FC03_REQUEST_SHAPED_12B_1000HZ")
    p1.emit("G1_CONTROLLED_IS_FULL_MODBUS", "NO")
    p1.emit("G1_SATURATED_CHUNK_BYTES", 4096)

    if branch != BRANCH:
        raise RuntimeError("G1_BRANCH_MISMATCH")
    if p1.git(repo, "diff", "--name-only") or p1.git(
        repo,
        "diff",
        "--cached",
        "--name-only",
    ):
        raise RuntimeError("G1_TRACKED_TREE_NOT_CLEAN")

    ast.parse(runner.read_text(encoding="utf-8"), filename=str(runner))
    p1.emit("G1_RUNNER_AST", "PASS")

    cli = p1.find_cli(args.arduino_cli)
    p1.emit("ARDUINO_CLI", cli)

    result_root = Path(
        tempfile.mkdtemp(prefix="jwplc_a14_g1_tcp_spi_waste_")
    )
    p1.emit("G1_RESULT_ROOT", result_root)

    builds = {
        "BASE": result_root / "build_base",
        "PROFILE": result_root / "build_profile",
    }

    for variant in ("BASE", "PROFILE"):
        p1.compile_variant(
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

    for index, (load, duration, variant) in enumerate(CASES, 1):
        print()
        print("=" * 78)
        print(f" G1 CASE {index}/{len(CASES)} {load} {variant}")
        print("=" * 78)

        upload_log = result_root / f"{index:02d}_{load.lower()}_{variant.lower()}_upload.log"
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
        p1.emit(f"G1_{load}_{variant}_UPLOAD_EXIT", upload_exit)

        if upload_exit != 0:
            print(p1.decode(upload_log.read_bytes())[-5000:])
            raise RuntimeError(f"G1_{load}_{variant}_UPLOAD_FAILED")

        time.sleep(3.0)

        case_log = result_root / f"{index:02d}_{load.lower()}_{variant.lower()}.log"
        proc = p1.run(
            [
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
            ],
            repo,
        )
        case_log.write_bytes(proc.stdout)
        text = p1.decode(proc.stdout)
        print(text)

        if proc.returncode != 0:
            raise RuntimeError(f"G1_{load}_{variant}_CASE_FAILED")
        if one(text, "G1_FUNCTIONAL_PASS") != "YES":
            raise RuntimeError(f"G1_{load}_{variant}_FUNCTIONAL_FAIL")

        row = {
            "dut_mbps": number(text, "G1_DUT_MBPS"),
            "pc_rate": number(text, "G1_PC_OPERATIONS_PER_S"),
            "service_passes": number(text, "G1_TCP_SERVICE_PASSES"),
            "service_active": number(text, "G1_TCP_SERVICE_ACTIVE_PASSES"),
            "service_empty": number(text, "G1_TCP_SERVICE_EMPTY_PASSES"),
            "service_empty_pct": number(text, "G1_TCP_SERVICE_EMPTY_PCT"),
            "hold_count": number(text, "G1_TCP_SPI_HOLD_COUNT"),
            "hold_total_us": number(text, "G1_TCP_SPI_HOLD_TOTAL_US"),
            "hold_max_us": number(text, "G1_TCP_SPI_HOLD_MAX_US"),
            "occupancy_pct": number(text, "G1_SPI_HOLD_OCCUPANCY_PCT"),
        }

        if variant == "PROFILE":
            row.update(
                {
                    "status_calls": number(text, "G1_TCP_STATUS_CALLS"),
                    "status_us": number(text, "G1_TCP_STATUS_TOTAL_US"),
                    "available_calls": number(text, "G1_TCP_AVAILABLE_CALLS"),
                    "available_zero": number(text, "G1_TCP_AVAILABLE_ZERO"),
                    "available_nonzero": number(text, "G1_TCP_AVAILABLE_NONZERO"),
                    "available_zero_pct": number(text, "G1_TCP_AVAILABLE_ZERO_PCT"),
                    "available_us": number(text, "G1_TCP_AVAILABLE_TOTAL_US"),
                    "spi_us_per_byte": number(text, "G1_TCP_SPI_US_PER_RX_BYTE"),
                    "holds_per_rx_op": number(text, "G1_TCP_SPI_HOLDS_PER_RX_OPERATION"),
                }
            )

        results[(load, variant)] = row

    summary: list[str] = []

    def out(key: str, value: object) -> None:
        line = f"{key}={value}"
        summary.append(line)
        print(line)

    print()
    print("=" * 78)
    print(" G1 SUMMARY")
    print("=" * 78)

    for load in ("IDLE", "CONTROLLED", "SATURATED"):
        base = results[(load, "BASE")]
        prof = results[(load, "PROFILE")]

        out(f"G1_{load}_BASE_SERVICE_EMPTY_PCT", f"{base['service_empty_pct']:.3f}")
        out(f"G1_{load}_PROFILE_SERVICE_EMPTY_PCT", f"{prof['service_empty_pct']:.3f}")
        out(f"G1_{load}_PROFILE_AVAILABLE_ZERO_PCT", f"{prof['available_zero_pct']:.3f}")
        out(f"G1_{load}_BASE_SPI_HOLD_OCCUPANCY_PCT", f"{base['occupancy_pct']:.3f}")
        out(f"G1_{load}_PROFILE_SPI_HOLD_OCCUPANCY_PCT", f"{prof['occupancy_pct']:.3f}")
        out(f"G1_{load}_PROFILE_STATUS_CALLS", f"{prof['status_calls']:.0f}")
        out(f"G1_{load}_PROFILE_AVAILABLE_CALLS", f"{prof['available_calls']:.0f}")
        out(f"G1_{load}_PROFILE_AVAILABLE_ZERO", f"{prof['available_zero']:.0f}")
        out(f"G1_{load}_PROFILE_AVAILABLE_NONZERO", f"{prof['available_nonzero']:.0f}")

        if load != "IDLE" and base["dut_mbps"] > 0:
            delta = (prof["dut_mbps"] / base["dut_mbps"] - 1.0) * 100.0
            out(f"G1_{load}_PROFILE_VS_BASE_DUT_MBPS_PCT", f"{delta:.3f}")

        if load == "CONTROLLED":
            out("G1_CONTROLLED_ACHIEVED_PC_HZ_BASE", f"{base['pc_rate']:.3f}")
            out("G1_CONTROLLED_ACHIEVED_PC_HZ_PROFILE", f"{prof['pc_rate']:.3f}")

    out("G1_INT_TCP_OPPORTUNITY", "REVIEW_REQUIRED")
    out("G1_PRODUCT_RUNTIME_POLICY_CHANGE", "NO")
    out("G1_INT_IMPLEMENTED", "NO")
    out("G1_PHYSICAL_RUN", "COMPLETE")
    out("G1_STATUS", "PASS")

    summary_path = result_root / "SUMMARY.log"
    summary_path.write_text("\n".join(summary) + "\n", encoding="utf-8")
    p1.emit("G1_SUMMARY_LOG", summary_path)

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
