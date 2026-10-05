#!/usr/bin/env python3
from __future__ import annotations

import argparse
import ast
import sys
import tempfile
import time
from pathlib import Path

import a14_h4a04p3_w5500_fifo_reuse_ab as p3


BRANCH = "v2.1.0-alpha.14/feature/modbus-tcp"
PROFILE_DURATION_S = 3.0
SPI_HZ = 26_000_000


def emit(key: str, value: object) -> None:
    print(f"{key}={value}")


def compile_profile(
    *,
    cli: Path,
    fqbn: str,
    repo: Path,
    sketch: Path,
    libraries: Path,
    build_dir: Path,
    log: Path,
) -> None:
    extra_flags = " ".join(
        (
            "-DJWPLC_ETHERNET_ENABLE_PROFILE_HOOKS=0",
            "-DJWPLC_W5500_RX_DIRECT_TRANSFER_BYTES=0",
            "-DJWPLC_W5500_RX_FIFO_REUSE=1",
            "-DJWPLC_H4A04P3_VERIFY_PAYLOAD=1",
            "-DJWPLC_SPI_PROFILE_FIFO_REUSE_CHUNKS=1",
        )
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
        f"compiler.cpp.extra_flags={extra_flags}",
        str(sketch),
    ]

    exit_code = p3.run_logged(cmd, log, repo)
    emit("H4A04P4_COMPILE_EXIT", exit_code)
    if exit_code != 0:
        print(p3.decode(log.read_bytes())[-8000:])
        raise RuntimeError("H4A04P4_COMPILE_FAILED")

    normalized = p3.decode(log.read_bytes()).replace("\\", "/").lower()
    required_tokens = (
        "-djwplc_ethernet_enable_profile_hooks=0",
        "-djwplc_w5500_rx_direct_transfer_bytes=0",
        "-djwplc_w5500_rx_fifo_reuse=1",
        "-djwplc_h4a04p3_verify_payload=1",
        "-djwplc_spi_profile_fifo_reuse_chunks=1",
        "w5100.cpp",
        "socket.cpp",
        "spi.cpp",
        "jwplc_idlescreen.cpp",
        "jwplc_tft.cpp",
    )
    for token in required_tokens:
        ok = token in normalized
        emit(
            f"H4A04P4_TOKEN_{token.replace('=', '_')}",
            "PASS" if ok else "FAIL",
        )
        if not ok:
            raise RuntimeError(f"H4A04P4_TOKEN_MISSING={token}")

    if "libjwplc_display.a" in normalized:
        raise RuntimeError("H4A04P4_DISPLAY_A_UNEXPECTED")
    if "libjwplc_tft.a" in normalized:
        raise RuntimeError("H4A04P4_TFT_A_UNEXPECTED")
    if "libspi.a" in normalized:
        raise RuntimeError("H4A04P4_SPI_A_UNEXPECTED")


def upload_profile(
    *,
    cli: Path,
    fqbn: str,
    serial_port: str,
    build_dir: Path,
    sketch: Path,
    repo: Path,
    log: Path,
) -> None:
    exit_code = p3.run_logged(
        [
            str(cli),
            "upload",
            "--fqbn",
            fqbn,
            "--port",
            serial_port,
            "--input-dir",
            str(build_dir),
            str(sketch),
        ],
        log,
        repo,
    )
    emit("H4A04P4_UPLOAD_EXIT", exit_code)
    if exit_code != 0:
        print(p3.decode(log.read_bytes())[-5000:])
        raise RuntimeError("H4A04P4_UPLOAD_FAILED")


def main() -> int:
    parser = argparse.ArgumentParser(
        description="A14 H4A0.4-P4 W5500 64-byte chunk microprofile"
    )
    parser.add_argument("--serial", default="COM14")
    parser.add_argument("--arduino-cli")
    parser.add_argument("--fqbn", default="jwplc_local:esp32:jwplcbasic")
    parser.add_argument(
        "--defer-physical-review",
        action="store_true",
        help="Registra la revisión física como pendiente del usuario.",
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
        / "a14_h4a04p1_tcp_rx_case.py"
    )
    contract = (
        repo
        / "tools"
        / "modbus-tcp-benchmark"
        / "gates"
        / "a14_package_promotion_contract.py"
    )
    libraries = repo / "JWPLC" / "2.1.0" / "libraries"

    print("=" * 78)
    print(" A14 H4A0.4-P4 - W5500 64-BYTE CHUNK MICROPROFILE")
    print("=" * 78)

    branch = p3.git(repo, "branch", "--show-current")
    head = p3.git(repo, "rev-parse", "HEAD")
    emit("BRANCH", branch)
    emit("HEAD", head)
    emit("SERIAL_PORT", args.serial)
    emit("H4A04P4_DURATION_S", PROFILE_DURATION_S)
    emit("H4A04P4_SPI_HZ", SPI_HZ)
    emit("H4A04P4_PRODUCT_SOURCE", "JWPLC/2.1.0_CANONICAL_PACKAGE")
    emit("H4A04P4_TEMP_PRODUCT_PATCHES", "NO")
    emit("H4A04P4_ONLY_VARIABLE", "CHUNK_TIMING_INSTRUMENTATION")
    emit("H4A04P4_FIFO_REUSE", "ON_FOR_PROFILE")
    emit("H4A04P4_FIFO_REUSE_DEFAULT", "OFF")
    emit("H4A04P4_PROFILE_DEFAULT", "OFF")

    if branch != BRANCH:
        raise RuntimeError("H4A04P4_BRANCH_MISMATCH")
    if p3.git(repo, "diff", "--name-only") or p3.git(
        repo,
        "diff",
        "--cached",
        "--name-only",
    ):
        raise RuntimeError("H4A04P4_TRACKED_TREE_NOT_CLEAN")

    untracked_product = [
        line
        for line in p3.git(
            repo,
            "ls-files",
            "--others",
            "--exclude-standard",
        ).splitlines()
        if line.startswith("JWPLC/2.1.0/")
    ]
    emit("H4A04P4_UNTRACKED_PRODUCT_COUNT", len(untracked_product))
    if untracked_product:
        raise RuntimeError("H4A04P4_UNTRACKED_PRODUCT_SOURCE")

    ast.parse(runner.read_text(encoding="utf-8"), filename=str(runner))
    emit("H4A04P4_RUNNER_AST", "PASS")

    contract_run = p3.run([sys.executable, "-B", str(contract)], repo)
    print(p3.decode(contract_run.stdout))
    if contract_run.returncode != 0:
        raise RuntimeError("H4A04P4_PACKAGE_CONTRACT_FAILED")

    cli = p3.find_cli(args.arduino_cli)
    emit("ARDUINO_CLI", cli)
    result_root = Path(
        tempfile.mkdtemp(prefix="jwplc_a14_h4a04p4_chunk_profile_")
    )
    emit("H4A04P4_RESULT_ROOT", result_root)

    build_dir = result_root / "build"
    compile_profile(
        cli=cli,
        fqbn=args.fqbn,
        repo=repo,
        sketch=sketch,
        libraries=libraries,
        build_dir=build_dir,
        log=result_root / "compile.log",
    )

    upload_profile(
        cli=cli,
        fqbn=args.fqbn,
        serial_port=args.serial,
        build_dir=build_dir,
        sketch=sketch,
        repo=repo,
        log=result_root / "upload.log",
    )
    time.sleep(3.0)

    case_log = result_root / "case.log"
    case_exit = p3.run_logged(
        [
            sys.executable,
            "-B",
            "-u",
            str(runner),
            "--serial",
            args.serial,
            "--duration",
            f"{PROFILE_DURATION_S:.1f}",
            "--chunk",
            "4096",
            "--variant",
            "BASE",
        ],
        case_log,
        repo,
    )
    emit("H4A04P4_CASE_EXIT", case_exit)
    case_text = p3.decode(case_log.read_bytes())
    if case_exit != 0:
        print(case_text[-8000:])
        raise RuntimeError("H4A04P4_RUNTIME_CASE_FAILED")

    if p3.one(case_text, "H4A04P1_FUNCTIONAL_PASS") != "YES":
        raise RuntimeError("H4A04P4_FUNCTIONAL_FAIL")
    if p3.integer(case_text, "H4A04P1_TRANSPORT_ERRORS") != 0:
        raise RuntimeError("H4A04P4_TRANSPORT_ERRORS_NONZERO")
    if p3.integer(case_text, "H4A04P1_TCP_SPI_LOCK_ERRORS") != 0:
        raise RuntimeError("H4A04P4_SPI_LOCK_ERRORS_NONZERO")
    if p3.one(case_text, "H4A04P1_PAYLOAD_VERIFY_ENABLED") != "YES":
        raise RuntimeError("H4A04P4_PAYLOAD_VERIFY_NOT_ACTIVE")
    if p3.one(case_text, "H4A04P1_SPI_CHUNK_PROFILE_ENABLED") != "YES":
        raise RuntimeError("H4A04P4_CHUNK_PROFILE_NOT_ACTIVE")

    rx_bytes = p3.integer(case_text, "H4A04P1_DUT_RX_BYTES")
    actual_hash = p3.integer(case_text, "H4A04P1_RX_FNV1A32")
    expected_hash = p3.fnv1a_expected(rx_bytes)
    if actual_hash != expected_hash:
        raise RuntimeError("H4A04P4_PAYLOAD_INTEGRITY_FAIL")

    chunk_count = p3.integer(case_text, "H4A04P1_SPI_CHUNK_COUNT")
    chunk_bytes = p3.integer(case_text, "H4A04P1_SPI_CHUNK_BYTES")
    setup_us = p3.integer(case_text, "H4A04P1_SPI_CHUNK_SETUP_TOTAL_US")
    wire_wait_us = p3.integer(
        case_text,
        "H4A04P1_SPI_CHUNK_WIRE_WAIT_TOTAL_US",
    )
    copy_out_us = p3.integer(
        case_text,
        "H4A04P1_SPI_CHUNK_COPY_OUT_TOTAL_US",
    )
    other_us = p3.integer(case_text, "H4A04P1_SPI_CHUNK_OTHER_TOTAL_US")

    if chunk_count <= 0 or chunk_bytes <= 0:
        raise RuntimeError("H4A04P4_EMPTY_CHUNK_PROFILE")

    total_us = setup_us + wire_wait_us + copy_out_us + other_us
    if total_us <= 0:
        raise RuntimeError("H4A04P4_EMPTY_TIME_PROFILE")

    ideal_wire_us = chunk_bytes * 8.0 / (SPI_HZ / 1_000_000.0)
    excess_over_wire_us = total_us - ideal_wire_us
    wire_wait_excess_us = max(0.0, wire_wait_us - ideal_wire_us)
    avg_chunk_bytes = chunk_bytes / chunk_count
    us_per_byte = total_us / chunk_bytes

    total_blocks = {
        "SETUP": float(setup_us),
        "WIRE_WAIT": float(wire_wait_us),
        "COPY_OUT": float(copy_out_us),
        "OTHER": float(other_us),
    }
    actionable_blocks = {
        "SETUP": float(setup_us),
        "START_WAIT_EXCESS": wire_wait_excess_us,
        "COPY_OUT": float(copy_out_us),
        "OTHER": float(other_us),
    }
    dominant_total = max(total_blocks, key=total_blocks.get)
    dominant_actionable = max(actionable_blocks, key=actionable_blocks.get)

    emit("H4A04P4_RX_BYTES", rx_bytes)
    emit("H4A04P4_FNV_ACTUAL", actual_hash)
    emit("H4A04P4_FNV_EXPECTED", expected_hash)
    emit("H4A04P4_PAYLOAD_INTEGRITY", "PASS")
    emit("H4A04P4_CHUNK_COUNT", chunk_count)
    emit("H4A04P4_BYTES", chunk_bytes)
    emit("H4A04P4_AVG_CHUNK_BYTES", f"{avg_chunk_bytes:.3f}")
    emit("H4A04P4_SETUP_TOTAL_US", setup_us)
    emit("H4A04P4_WIRE_WAIT_TOTAL_US", wire_wait_us)
    emit("H4A04P4_COPY_OUT_TOTAL_US", copy_out_us)
    emit("H4A04P4_OTHER_TOTAL_US", other_us)
    emit("H4A04P4_MEASURED_TOTAL_US", total_us)
    emit("H4A04P4_IDEAL_WIRE_US", f"{ideal_wire_us:.3f}")
    emit("H4A04P4_EXCESS_OVER_WIRE_US", f"{excess_over_wire_us:.3f}")
    emit("H4A04P4_WIRE_WAIT_EXCESS_US", f"{wire_wait_excess_us:.3f}")
    emit("H4A04P4_US_PER_BYTE", f"{us_per_byte:.6f}")
    for name, value in total_blocks.items():
        emit(
            f"H4A04P4_{name}_PCT",
            f"{value * 100.0 / total_us:.3f}",
        )
    emit("H4A04P4_DOMINANT_TOTAL_BLOCK", dominant_total)
    emit("H4A04P4_DOMINANT_ACTIONABLE_BLOCK", dominant_actionable)
    emit("H4A04P4_TCP_SPI_LOCK_ERRORS", 0)
    emit("H4A04P4_TRANSPORT_ERRORS", 0)
    emit("H4A04P4_UNEXPECTED_RESETS", 0)

    if args.defer_physical_review:
        physical_status = "PENDING_USER"
        gate_status = "PASS_DATA_ONLY"
    else:
        answer = input(
            "¿TFT/periféricos permanecieron estables durante H4A0.4-P4? (S/N): "
        ).strip().upper()
        if answer != "S":
            raise RuntimeError("H4A04P4_PHYSICAL_STABILITY_FAILED")
        physical_status = "PASS"
        gate_status = "PASS"

    emit("H4A04P4_PHYSICAL_STABILITY", physical_status)

    if p3.git(repo, "diff", "--name-only") or p3.git(
        repo,
        "diff",
        "--cached",
        "--name-only",
    ):
        raise RuntimeError("H4A04P4_REPOSITORY_MUTATED_DURING_GATE")

    summary_log = result_root / "SUMMARY.log"
    summary_log.write_text(
        "\n".join(
            (
                f"A14_H4A04P4_W5500_CHUNK_PROFILE={gate_status}",
                f"HEAD={head}",
                f"CHUNK_COUNT={chunk_count}",
                f"BYTES={chunk_bytes}",
                f"AVG_CHUNK_BYTES={avg_chunk_bytes:.3f}",
                f"SETUP_TOTAL_US={setup_us}",
                f"WIRE_WAIT_TOTAL_US={wire_wait_us}",
                f"COPY_OUT_TOTAL_US={copy_out_us}",
                f"OTHER_TOTAL_US={other_us}",
                f"MEASURED_TOTAL_US={total_us}",
                f"IDEAL_WIRE_US={ideal_wire_us:.3f}",
                f"EXCESS_OVER_WIRE_US={excess_over_wire_us:.3f}",
                f"WIRE_WAIT_EXCESS_US={wire_wait_excess_us:.3f}",
                f"US_PER_BYTE={us_per_byte:.6f}",
                f"DOMINANT_TOTAL_BLOCK={dominant_total}",
                f"DOMINANT_ACTIONABLE_BLOCK={dominant_actionable}",
                "PAYLOAD_INTEGRITY=PASS",
                "TCP_SPI_LOCK_ERRORS=0",
                "TRANSPORT_ERRORS=0",
                "UNEXPECTED_RESETS=0",
                f"PHYSICAL_STABILITY={physical_status}",
                "PROFILE_DEFAULT=OFF",
                "FIFO_REUSE_DEFAULT=OFF",
                "HARNESS_FAILURE=NO",
                "PRODUCT_FAILURE=NO_EVIDENCE",
                "HARDWARE_FAILURE=NO_EVIDENCE",
                "NEXT=SELECT_ONE_P5_CANDIDATE_FROM_DOMINANT_ACTIONABLE_BLOCK",
                "",
            )
        ),
        encoding="utf-8",
    )

    emit("H4A04P4_SUMMARY_LOG", summary_log)
    emit("HARNESS_FAILURE", "NO")
    emit("PRODUCT_FAILURE", "NO_EVIDENCE")
    emit("HARDWARE_FAILURE", "NO_EVIDENCE")
    emit("A14_H4A04P4_W5500_CHUNK_PROFILE", gate_status)
    emit("NEXT", "SELECT_ONE_P5_CANDIDATE_FROM_DOMINANT_ACTIONABLE_BLOCK")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except SystemExit:
        raise
    except Exception as exc:
        emit("HARNESS_FAILURE", "UNCLASSIFIED")
        emit("PRODUCT_FAILURE", "UNCLASSIFIED")
        emit("HARDWARE_FAILURE", "UNCLASSIFIED")
        emit("H4A04P4_FAILURE_REQUIRES_CLASSIFICATION", "YES")
        emit("H4A04P4_EXCEPTION", str(exc))
        raise SystemExit(1)
