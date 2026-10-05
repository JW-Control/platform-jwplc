#!/usr/bin/env python3
from __future__ import annotations

import argparse
import ast
import sys
import tempfile
import time
from pathlib import Path

import a14_h4a04p3_w5500_fifo_reuse_ab as p3
import a14_h4a04p6_tcp_available_read_ab as p6
import a14_h4a04p7_tcp_rx_commit_ab as p7
import a14_h4a04p8_socket_status_ab as p8


BRANCH = "v2.1.0-alpha.14/feature/modbus-tcp"
VERIFY_DURATION_S = 1.0
PERF_DURATION_S = 15.0
RECONNECT_DURATION_S = 2.0
ORDER = (
    "BATCH8",
    "BATCH16",
    "BATCH32",
    "BATCH32",
    "BATCH16",
    "BATCH8",
    "BATCH8",
    "BATCH32",
    "BATCH16",
)
RUNS_PER_VARIANT = 3
PAYLOAD_SPREAD_MAX_PCT = 0.5
GAIN_THRESHOLD_PCT = 1.0
HOLD_AVG_GUARD_PCT = 10.0
HOLD_MAX_GUARD_PCT = 25.0


def emit(key: str, value: object) -> None:
    print(f"{key}={value}")


def chunks_for(variant: str) -> int:
    try:
        return int(variant.removeprefix("BATCH"))
    except ValueError as exc:
        raise RuntimeError(f"H4A04P9_UNKNOWN_VARIANT={variant}") from exc


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
    verify: bool = False,
) -> None:
    max_chunks = chunks_for(variant)
    if max_chunks not in (8, 16, 32):
        raise RuntimeError(f"H4A04P9_INVALID_CHUNKS={max_chunks}")

    profile = "0" if verify else "1"
    verify_flag = "1" if verify else "0"
    extra_flags = " ".join(
        (
            f"-DJWPLC_ETHERNET_ENABLE_PROFILE_HOOKS={profile}",
            "-DJWPLC_W5500_RX_DIRECT_TRANSFER_BYTES=0",
            "-DJWPLC_W5500_RX_FIFO_REUSE=1",
            f"-DJWPLC_H4A04P3_VERIFY_PAYLOAD={verify_flag}",
            "-DJWPLC_H4A04P6_DIRECT_READ=1",
            "-DJWPLC_H4A04P7_DEFER_TCP_COMMIT=0",
            "-DJWPLC_H4A04P8_REUSE_CONNECTED_RESULT=1",
            f"-DJWPLC_H4A04P9_RX_MAX_CHUNKS={max_chunks}",
            "-DJWPLC_SPI_PROFILE_FIFO_REUSE_CHUNKS=0",
        )
    )
    exit_code = p3.run_logged(
        [
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
        ],
        log,
        repo,
    )
    label = f"{variant}_VERIFY" if verify else variant
    emit(f"H4A04P9_{label}_COMPILE_EXIT", exit_code)
    if exit_code != 0:
        print(p3.decode(log.read_bytes())[-8000:])
        raise RuntimeError(f"H4A04P9_{label}_COMPILE_FAILED")

    normalized = p3.decode(log.read_bytes()).replace("\\", "/").lower()
    required_tokens = (
        f"-djwplc_ethernet_enable_profile_hooks={profile}",
        "-djwplc_w5500_rx_direct_transfer_bytes=0",
        "-djwplc_w5500_rx_fifo_reuse=1",
        f"-djwplc_h4a04p3_verify_payload={verify_flag}",
        "-djwplc_h4a04p6_direct_read=1",
        "-djwplc_h4a04p7_defer_tcp_commit=0",
        "-djwplc_h4a04p8_reuse_connected_result=1",
        f"-djwplc_h4a04p9_rx_max_chunks={max_chunks}",
        "-djwplc_spi_profile_fifo_reuse_chunks=0",
        "ethernetclient.cpp",
        "w5100.cpp",
        "socket.cpp",
        "spi.cpp",
        "jwplc_idlescreen.cpp",
        "jwplc_tft.cpp",
    )
    for token in required_tokens:
        ok = token in normalized
        emit(
            f"H4A04P9_{label}_TOKEN_{token.replace('=', '_')}",
            "PASS" if ok else "FAIL",
        )
        if not ok:
            raise RuntimeError(f"H4A04P9_{label}_TOKEN_MISSING={token}")

    if "libjwplc_display.a" in normalized:
        raise RuntimeError(f"H4A04P9_{label}_DISPLAY_A_UNEXPECTED")
    if "libjwplc_tft.a" in normalized:
        raise RuntimeError(f"H4A04P9_{label}_TFT_A_UNEXPECTED")


def upload_variant(
    *,
    cli: Path,
    fqbn: str,
    serial_port: str,
    build_dir: Path,
    sketch: Path,
    repo: Path,
    log: Path,
    label: str,
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
    emit(f"H4A04P9_{label}_UPLOAD_EXIT", exit_code)
    if exit_code != 0:
        print(p3.decode(log.read_bytes())[-5000:])
        raise RuntimeError(f"H4A04P9_{label}_UPLOAD_FAILED")


def inspect_case(text: str) -> dict[str, float]:
    row = p8.inspect_case(text)
    row["hold_count"] = p3.number(text, "H4A04P1_TCP_SPI_HOLD_COUNT")
    row["hold_max_us"] = p3.number(text, "H4A04P1_TCP_SPI_HOLD_MAX_US")
    return row


def main() -> int:
    parser = argparse.ArgumentParser(
        description="A14 H4A0.4-P9 TCP RX raw batch 8/16/32 ceiling locator"
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
    print(" A14 H4A0.4-P9 - TCP RX RAW BATCH 8/16/32")
    print("=" * 78)

    branch = p3.git(repo, "branch", "--show-current")
    head = p3.git(repo, "rev-parse", "HEAD")
    emit("BRANCH", branch)
    emit("HEAD", head)
    emit("SERIAL_PORT", args.serial)
    emit("H4A04P9_VERIFY_DURATION_S", VERIFY_DURATION_S)
    emit("H4A04P9_PERF_DURATION_S", PERF_DURATION_S)
    emit("H4A04P9_RECONNECT_DURATION_S", RECONNECT_DURATION_S)
    emit("H4A04P9_ORDER", ",".join(ORDER))
    emit("H4A04P9_RUNS_PER_VARIANT", RUNS_PER_VARIANT)
    emit("H4A04P9_SPI_HZ", 26_000_000)
    emit("H4A04P9_PRODUCT_SOURCE", "JWPLC/2.1.0_CANONICAL_PACKAGE")
    emit("H4A04P9_TEMP_PRODUCT_PATCHES", "NO")
    emit("H4A04P9_ONLY_VARIABLE", "TCP_RX_MAX_CHUNKS_PER_SPI_OWNERSHIP")
    emit("H4A04P9_VARIANTS", "8,16,32")
    emit("H4A04P9_READ_PATTERN", "READ_DIRECT_ALL_VARIANTS")
    emit("H4A04P9_RX_COMMIT", "IMMEDIATE_ALL_VARIANTS")
    emit("H4A04P9_SINGLE_STATUS", "ON_ALL_VARIANTS")
    emit("H4A04P9_FIFO_REUSE", "ON_ALL_VARIANTS")
    emit("H4A04P9_FIFO_REUSE_DEFAULT", "OFF")
    emit("H4A04P9_AUTO_PROMOTION", "FORBIDDEN")

    if branch != BRANCH:
        raise RuntimeError("H4A04P9_BRANCH_MISMATCH")
    if p3.git(repo, "diff", "--name-only") or p3.git(
        repo,
        "diff",
        "--cached",
        "--name-only",
    ):
        raise RuntimeError("H4A04P9_TRACKED_TREE_NOT_CLEAN")

    ast.parse(runner.read_text(encoding="utf-8"), filename=str(runner))
    emit("H4A04P9_RUNNER_AST", "PASS")
    contract_run = p3.run([sys.executable, "-B", str(contract)], repo)
    print(p3.decode(contract_run.stdout))
    if contract_run.returncode != 0:
        raise RuntimeError("H4A04P9_PACKAGE_CONTRACT_FAILED")

    cli = p3.find_cli(args.arduino_cli)
    emit("ARDUINO_CLI", cli)
    result_root = Path(
        tempfile.mkdtemp(prefix="jwplc_a14_h4a04p9_rx_batch_")
    )
    emit("H4A04P9_RESULT_ROOT", result_root)
    verify_build = result_root / "build_verify_batch32"
    perf_builds = {
        variant: result_root / f"build_{variant.lower()}"
        for variant in ("BATCH8", "BATCH16", "BATCH32")
    }

    compile_variant(
        cli=cli,
        fqbn=args.fqbn,
        repo=repo,
        sketch=sketch,
        libraries=libraries,
        build_dir=verify_build,
        variant="BATCH32",
        log=result_root / "compile_verify_batch32.log",
        verify=True,
    )
    for variant in ("BATCH8", "BATCH16", "BATCH32"):
        compile_variant(
            cli=cli,
            fqbn=args.fqbn,
            repo=repo,
            sketch=sketch,
            libraries=libraries,
            build_dir=perf_builds[variant],
            variant=variant,
            log=result_root / f"compile_{variant.lower()}.log",
        )

    print()
    print("=" * 78)
    print(" H4A0.4-P9 PAYLOAD INTEGRITY - BATCH32")
    print("=" * 78)
    upload_variant(
        cli=cli,
        fqbn=args.fqbn,
        serial_port=args.serial,
        build_dir=verify_build,
        sketch=sketch,
        repo=repo,
        log=result_root / "verify_upload.log",
        label="VERIFY_BATCH32",
    )
    time.sleep(3.0)
    verify_text = p7.run_case(
        runner=runner,
        repo=repo,
        serial_port=args.serial,
        duration_s=VERIFY_DURATION_S,
        variant_arg="BASE",
        log=result_root / "verify_case.log",
        max_chunks=32,
    )
    p7.require_clean_case(verify_text, "P9_VERIFY")
    if p3.one(verify_text, "H4A04P1_PAYLOAD_VERIFY_ENABLED") != "YES":
        raise RuntimeError("H4A04P9_VERIFY_FLAG_NOT_ACTIVE")
    verify_bytes = p3.integer(verify_text, "H4A04P1_DUT_RX_BYTES")
    actual_hash = p3.integer(verify_text, "H4A04P1_RX_FNV1A32")
    expected_hash = p3.fnv1a_expected(verify_bytes)
    emit("H4A04P9_VERIFY_RX_BYTES", verify_bytes)
    emit("H4A04P9_VERIFY_FNV_ACTUAL", actual_hash)
    emit("H4A04P9_VERIFY_FNV_EXPECTED", expected_hash)
    if actual_hash != expected_hash:
        raise RuntimeError("H4A04P9_BATCH32_PAYLOAD_INTEGRITY_FAIL")
    emit("H4A04P9_BATCH32_PAYLOAD_INTEGRITY", "PASS")

    variants = ("BATCH8", "BATCH16", "BATCH32")
    buckets: dict[str, list[dict[str, float]]] = {
        variant: [] for variant in variants
    }
    counts = {variant: 0 for variant in variants}
    for index, variant in enumerate(ORDER, 1):
        counts[variant] += 1
        run_no = counts[variant]
        max_chunks = chunks_for(variant)
        print()
        print("=" * 78)
        print(
            f" H4A0.4-P9 CASE {index}/{len(ORDER)} "
            f"{variant} RUN {run_no}/{RUNS_PER_VARIANT}"
        )
        print("=" * 78)
        upload_variant(
            cli=cli,
            fqbn=args.fqbn,
            serial_port=args.serial,
            build_dir=perf_builds[variant],
            sketch=sketch,
            repo=repo,
            log=result_root / f"{index:02d}_{variant.lower()}_upload.log",
            label=f"{variant}_RUN{run_no}",
        )
        time.sleep(3.0)
        case_text = p7.run_case(
            runner=runner,
            repo=repo,
            serial_port=args.serial,
            duration_s=PERF_DURATION_S,
            variant_arg="PROFILE",
            log=result_root / f"{index:02d}_{variant.lower()}_run{run_no}.log",
            max_chunks=max_chunks,
        )
        p7.require_clean_case(case_text, f"P9_{variant}_RUN{run_no}")
        row = inspect_case(case_text)
        buckets[variant].append(row)
        emit(
            "H4A04P9_RUN_RESULT",
            (
                f"VARIANT={variant} RUN={run_no} "
                f"TCP={row['mid']:.6f}Mbps "
                f"PAYLOAD={row['payload_mbps']:.3f}Mbps "
                f"HOLD_US_PER_BYTE={row['hold_us_per_byte']:.6f} "
                f"HOLD_MAX_US={row['hold_max_us']:.0f} PASS=YES"
            ),
        )

    expected_counts = {variant: RUNS_PER_VARIANT for variant in variants}
    if counts != expected_counts:
        raise RuntimeError(f"H4A04P9_RUN_COUNTS_INVALID={counts}")

    print()
    print("=" * 78)
    print(" H4A0.4-P9 RECONNECT - BATCH32")
    print("=" * 78)
    upload_variant(
        cli=cli,
        fqbn=args.fqbn,
        serial_port=args.serial,
        build_dir=perf_builds["BATCH32"],
        sketch=sketch,
        repo=repo,
        log=result_root / "reconnect_upload.log",
        label="RECONNECT_BATCH32",
    )
    time.sleep(3.0)
    for attempt in (1, 2):
        reconnect_text = p7.run_case(
            runner=runner,
            repo=repo,
            serial_port=args.serial,
            duration_s=RECONNECT_DURATION_S,
            variant_arg="PROFILE",
            log=result_root / f"reconnect_case_{attempt}.log",
            prepare_next_reconnect=(attempt == 1),
            max_chunks=32,
        )
        p7.require_clean_case(reconnect_text, f"P9_RECONNECT_{attempt}")
        if attempt == 1 and p3.one(
            reconnect_text,
            "H4A04P1_RECONNECT_CLEANUP",
        ) != "PASS":
            raise RuntimeError("H4A04P9_RECONNECT_CLEANUP_FAILED")
        emit(f"H4A04P9_RECONNECT_CASE_{attempt}", "PASS")

    summary: dict[str, dict[str, float]] = {}
    for variant, rows in buckets.items():
        payload_rates = [row["payload_mbps"] for row in rows]
        summary[variant] = {
            "tcp": p6.median([row["mid"] for row in rows]),
            "tcp_spread": p6.spread_pct([row["mid"] for row in rows]),
            "payload": p6.median(payload_rates),
            "payload_spread": p6.spread_pct(payload_rates),
            "scheduler_us_per_byte": p6.median(
                [row["scheduler_us_per_byte"] for row in rows]
            ),
            "hold_us_per_byte": p6.median(
                [row["hold_us_per_byte"] for row in rows]
            ),
            "hold_max_us": p6.median(
                [row["hold_max_us"] for row in rows]
            ),
            "hold_count": p6.median([row["hold_count"] for row in rows]),
        }

    base = summary["BATCH8"]
    deltas: dict[str, dict[str, float | bool]] = {}
    for variant in ("BATCH16", "BATCH32"):
        values = summary[variant]
        tcp_delta = p6.delta_pct(values["tcp"], base["tcp"])
        payload_delta = p6.delta_pct(values["payload"], base["payload"])
        hold_delta = p6.delta_pct(
            values["hold_us_per_byte"],
            base["hold_us_per_byte"],
        )
        hold_max_delta = p6.delta_pct(values["hold_max_us"], base["hold_max_us"])
        fairness_ok = (
            hold_delta <= HOLD_AVG_GUARD_PCT
            and hold_max_delta <= HOLD_MAX_GUARD_PCT
        )
        deltas[variant] = {
            "tcp": tcp_delta,
            "payload": payload_delta,
            "hold": hold_delta,
            "hold_max": hold_max_delta,
            "fairness_ok": fairness_ok,
        }

    repeatability_ok = all(
        summary[variant]["payload_spread"] <= PAYLOAD_SPREAD_MAX_PCT
        for variant in variants
    )
    ceiling_variant = max(variants, key=lambda item: summary[item]["tcp"])
    ceiling_gain = p6.delta_pct(summary[ceiling_variant]["tcp"], base["tcp"])
    ceiling_fairness_ok = (
        True
        if ceiling_variant == "BATCH8"
        else bool(deltas[ceiling_variant]["fairness_ok"])
    )

    if not repeatability_ok:
        interpretation = "PAYLOAD_REPEATABILITY_INSUFFICIENT"
    elif ceiling_variant == "BATCH8" or ceiling_gain < GAIN_THRESHOLD_PCT:
        interpretation = "NO_MATERIAL_LARGER_BATCH_GAIN"
    elif not ceiling_fairness_ok:
        interpretation = "LARGER_BATCH_FAIRNESS_REGRESSION"
    else:
        interpretation = "LARGER_BATCH_CEILING_GAIN_PENDING_FAIRNESS_SOAK"

    print()
    print("=" * 78)
    print(" H4A0.4-P9 SUMMARY")
    print("=" * 78)
    for variant in variants:
        for key, value in summary[variant].items():
            emit(f"H4A04P9_{variant}_{key.upper()}", f"{value:.6f}")
    for variant in ("BATCH16", "BATCH32"):
        values = deltas[variant]
        emit(f"H4A04P9_{variant}_VS_8_TCP_PCT", f"{values['tcp']:.3f}")
        emit(f"H4A04P9_{variant}_VS_8_PAYLOAD_PCT", f"{values['payload']:.3f}")
        emit(f"H4A04P9_{variant}_VS_8_HOLD_PCT", f"{values['hold']:.3f}")
        emit(f"H4A04P9_{variant}_VS_8_HOLD_MAX_PCT", f"{values['hold_max']:.3f}")
        emit(f"H4A04P9_{variant}_FAIRNESS_OK", values["fairness_ok"])
    emit("H4A04P9_CEILING_VARIANT", ceiling_variant)
    emit("H4A04P9_CEILING_TCP_MBPS", f"{summary[ceiling_variant]['tcp']:.6f}")
    emit("H4A04P9_CEILING_VS_8_TCP_PCT", f"{ceiling_gain:.3f}")
    emit("H4A04P9_CEILING_FAIRNESS_OK", ceiling_fairness_ok)
    emit("H4A04P9_PAYLOAD_REPEATABILITY_OK", repeatability_ok)
    emit("H4A04P9_RECONNECT", "PASS")
    emit("H4A04P9_INTERPRETATION", interpretation)
    emit("H4A04P9_AUTO_PROMOTION", "NO")
    emit("H4A04P9_TCP_SPI_LOCK_ERRORS", 0)
    emit("H4A04P9_TRANSPORT_ERRORS", 0)
    emit("H4A04P9_UNEXPECTED_RESETS", 0)

    if args.defer_physical_review:
        physical_status = "PENDING_USER"
        gate_status = "PASS_DATA_ONLY"
    else:
        answer = input(
            "¿TFT/periféricos permanecieron estables durante H4A0.4-P9? (S/N): "
        ).strip().upper()
        if answer != "S":
            raise RuntimeError("H4A04P9_PHYSICAL_STABILITY_FAILED")
        physical_status = "PASS"
        gate_status = "PASS"
    emit("H4A04P9_PHYSICAL_STABILITY", physical_status)

    if p3.git(repo, "diff", "--name-only") or p3.git(
        repo,
        "diff",
        "--cached",
        "--name-only",
    ):
        raise RuntimeError("H4A04P9_REPOSITORY_MUTATED_DURING_GATE")

    summary_log = result_root / "SUMMARY.log"
    summary_lines = [
        f"A14_H4A04P9_RX_BATCH_RAW={gate_status}",
        f"HEAD={head}",
        f"VERIFY_RX_BYTES={verify_bytes}",
        f"VERIFY_FNV_ACTUAL={actual_hash}",
        f"VERIFY_FNV_EXPECTED={expected_hash}",
        "BATCH32_PAYLOAD_INTEGRITY=PASS",
    ]
    for variant in variants:
        summary_lines.extend(
            (
                f"{variant}_TCP_MBPS={summary[variant]['tcp']:.6f}",
                f"{variant}_PAYLOAD_MBPS={summary[variant]['payload']:.3f}",
                f"{variant}_HOLD_US_PER_BYTE={summary[variant]['hold_us_per_byte']:.6f}",
                f"{variant}_HOLD_MAX_US={summary[variant]['hold_max_us']:.0f}",
            )
        )
    summary_lines.extend(
        (
            f"CEILING_VARIANT={ceiling_variant}",
            f"CEILING_TCP_MBPS={summary[ceiling_variant]['tcp']:.6f}",
            f"CEILING_VS_8_TCP_PCT={ceiling_gain:.3f}",
            f"INTERPRETATION={interpretation}",
            "AUTO_PROMOTION=NO",
            "RECONNECT=PASS",
            "TCP_SPI_LOCK_ERRORS=0",
            "TRANSPORT_ERRORS=0",
            "UNEXPECTED_RESETS=0",
            f"PHYSICAL_STABILITY={physical_status}",
            "FIFO_REUSE_DEFAULT=OFF",
            "HARNESS_FAILURE=NO",
            "PRODUCT_FAILURE=NO_EVIDENCE",
            "HARDWARE_FAILURE=NO_EVIDENCE",
            "NEXT=INTERPRET_P9_THEN_DECIDE_RX_BUFFER_OR_TX_ASYNC",
            "",
        )
    )
    summary_log.write_text("\n".join(summary_lines), encoding="utf-8")
    emit("H4A04P9_SUMMARY_LOG", summary_log)
    emit("HARNESS_FAILURE", "NO")
    emit("PRODUCT_FAILURE", "NO_EVIDENCE")
    emit("HARDWARE_FAILURE", "NO_EVIDENCE")
    emit("A14_H4A04P9_RX_BATCH_RAW", gate_status)
    emit("NEXT", "INTERPRET_P9_THEN_DECIDE_RX_BUFFER_OR_TX_ASYNC")
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
        emit("H4A04P9_FAILURE_REQUIRES_CLASSIFICATION", "YES")
        emit("H4A04P9_EXCEPTION", str(exc))
        raise SystemExit(1)
