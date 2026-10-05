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


BRANCH = "v2.1.0-alpha.14/feature/modbus-tcp"
VERIFY_DURATION_S = 1.0
PERF_DURATION_S = 15.0
RECONNECT_DURATION_S = 2.0
ORDER = (
    "DOUBLE_STATUS",
    "SINGLE_STATUS",
    "SINGLE_STATUS",
    "DOUBLE_STATUS",
    "DOUBLE_STATUS",
    "SINGLE_STATUS",
)
RUNS_PER_VARIANT = 3
PAYLOAD_SPREAD_MAX_PCT = 0.5
STATUS_REDUCTION_MIN_PCT = 40.0
GAIN_THRESHOLD_PCT = 1.0
NO_MATERIAL_BAND_PCT = 0.5


def emit(key: str, value: object) -> None:
    print(f"{key}={value}")


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
    if variant == "DOUBLE_STATUS":
        reuse_status = "0"
    elif variant == "SINGLE_STATUS":
        reuse_status = "1"
    else:
        raise RuntimeError(f"H4A04P8_UNKNOWN_VARIANT={variant}")

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
            f"-DJWPLC_H4A04P8_REUSE_CONNECTED_RESULT={reuse_status}",
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
    emit(f"H4A04P8_{label}_COMPILE_EXIT", exit_code)
    if exit_code != 0:
        print(p3.decode(log.read_bytes())[-8000:])
        raise RuntimeError(f"H4A04P8_{label}_COMPILE_FAILED")

    normalized = p3.decode(log.read_bytes()).replace("\\", "/").lower()
    required_tokens = (
        f"-djwplc_ethernet_enable_profile_hooks={profile}",
        "-djwplc_w5500_rx_direct_transfer_bytes=0",
        "-djwplc_w5500_rx_fifo_reuse=1",
        f"-djwplc_h4a04p3_verify_payload={verify_flag}",
        "-djwplc_h4a04p6_direct_read=1",
        "-djwplc_h4a04p7_defer_tcp_commit=0",
        f"-djwplc_h4a04p8_reuse_connected_result={reuse_status}",
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
            f"H4A04P8_{label}_TOKEN_{token.replace('=', '_')}",
            "PASS" if ok else "FAIL",
        )
        if not ok:
            raise RuntimeError(f"H4A04P8_{label}_TOKEN_MISSING={token}")

    if "libjwplc_display.a" in normalized:
        raise RuntimeError(f"H4A04P8_{label}_DISPLAY_A_UNEXPECTED")
    if "libjwplc_tft.a" in normalized:
        raise RuntimeError(f"H4A04P8_{label}_TFT_A_UNEXPECTED")


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
    emit(f"H4A04P8_{label}_UPLOAD_EXIT", exit_code)
    if exit_code != 0:
        print(p3.decode(log.read_bytes())[-5000:])
        raise RuntimeError(f"H4A04P8_{label}_UPLOAD_FAILED")


def inspect_case(text: str) -> dict[str, float]:
    row = p6.inspect_case(text)
    row["status_calls"] = p3.number(
        text,
        "H4A04P1_TCP_PROF_SOCKET_STATUS_CALLS",
    )
    row["status_calls_per_mb"] = (
        row["status_calls"] * 1_000_000.0 / row["payload_bytes"]
    )
    row["status_us_per_byte"] = row["status_us"] / row["payload_bytes"]
    row["scheduler_us_per_byte"] = (
        row["recv_us"] + row["available_us"] + row["status_us"]
    ) / row["payload_bytes"]
    return row


def main() -> int:
    parser = argparse.ArgumentParser(
        description="A14 H4A0.4-P8 duplicate versus reused connected result"
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
    print(" A14 H4A0.4-P8 - SOCKET STATUS REDUNDANCY A/B")
    print("=" * 78)

    branch = p3.git(repo, "branch", "--show-current")
    head = p3.git(repo, "rev-parse", "HEAD")
    emit("BRANCH", branch)
    emit("HEAD", head)
    emit("SERIAL_PORT", args.serial)
    emit("H4A04P8_VERIFY_DURATION_S", VERIFY_DURATION_S)
    emit("H4A04P8_PERF_DURATION_S", PERF_DURATION_S)
    emit("H4A04P8_RECONNECT_DURATION_S", RECONNECT_DURATION_S)
    emit("H4A04P8_ORDER", ",".join(ORDER))
    emit("H4A04P8_RUNS_PER_VARIANT", RUNS_PER_VARIANT)
    emit("H4A04P8_SPI_HZ", 26_000_000)
    emit("H4A04P8_PRODUCT_SOURCE", "JWPLC/2.1.0_CANONICAL_PACKAGE")
    emit("H4A04P8_TEMP_PRODUCT_PATCHES", "NO")
    emit("H4A04P8_ONLY_VARIABLE", "SECOND_CONNECTED_PROBE_SAME_PASS")
    emit("H4A04P8_BASELINE", "DOUBLE_STATUS")
    emit("H4A04P8_CANDIDATE", "SINGLE_STATUS")
    emit("H4A04P8_STATUS_CACHE_SCOPE", "CURRENT_SCHEDULER_PASS_ONLY")
    emit("H4A04P8_READ_PATTERN", "READ_DIRECT_BOTH_VARIANTS")
    emit("H4A04P8_RX_COMMIT", "IMMEDIATE_BOTH_VARIANTS")
    emit("H4A04P8_FIFO_REUSE", "ON_BOTH_VARIANTS")
    emit("H4A04P8_FIFO_REUSE_DEFAULT", "OFF")

    if branch != BRANCH:
        raise RuntimeError("H4A04P8_BRANCH_MISMATCH")
    if p3.git(repo, "diff", "--name-only") or p3.git(
        repo,
        "diff",
        "--cached",
        "--name-only",
    ):
        raise RuntimeError("H4A04P8_TRACKED_TREE_NOT_CLEAN")

    ast.parse(runner.read_text(encoding="utf-8"), filename=str(runner))
    emit("H4A04P8_RUNNER_AST", "PASS")
    contract_run = p3.run([sys.executable, "-B", str(contract)], repo)
    print(p3.decode(contract_run.stdout))
    if contract_run.returncode != 0:
        raise RuntimeError("H4A04P8_PACKAGE_CONTRACT_FAILED")

    cli = p3.find_cli(args.arduino_cli)
    emit("ARDUINO_CLI", cli)
    result_root = Path(
        tempfile.mkdtemp(prefix="jwplc_a14_h4a04p8_socket_status_")
    )
    emit("H4A04P8_RESULT_ROOT", result_root)
    verify_build = result_root / "build_verify_single"
    perf_builds = {
        "DOUBLE_STATUS": result_root / "build_double",
        "SINGLE_STATUS": result_root / "build_single",
    }

    compile_variant(
        cli=cli,
        fqbn=args.fqbn,
        repo=repo,
        sketch=sketch,
        libraries=libraries,
        build_dir=verify_build,
        variant="SINGLE_STATUS",
        log=result_root / "compile_verify_single.log",
        verify=True,
    )
    for variant in ("DOUBLE_STATUS", "SINGLE_STATUS"):
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
    print(" H4A0.4-P8 PAYLOAD INTEGRITY - SINGLE_STATUS")
    print("=" * 78)
    upload_variant(
        cli=cli,
        fqbn=args.fqbn,
        serial_port=args.serial,
        build_dir=verify_build,
        sketch=sketch,
        repo=repo,
        log=result_root / "verify_upload.log",
        label="VERIFY_SINGLE_STATUS",
    )
    time.sleep(3.0)
    verify_text = p7.run_case(
        runner=runner,
        repo=repo,
        serial_port=args.serial,
        duration_s=VERIFY_DURATION_S,
        variant_arg="BASE",
        log=result_root / "verify_case.log",
    )
    p7.require_clean_case(verify_text, "P8_VERIFY")
    if p3.one(verify_text, "H4A04P1_PAYLOAD_VERIFY_ENABLED") != "YES":
        raise RuntimeError("H4A04P8_VERIFY_FLAG_NOT_ACTIVE")
    verify_bytes = p3.integer(verify_text, "H4A04P1_DUT_RX_BYTES")
    actual_hash = p3.integer(verify_text, "H4A04P1_RX_FNV1A32")
    expected_hash = p3.fnv1a_expected(verify_bytes)
    emit("H4A04P8_VERIFY_RX_BYTES", verify_bytes)
    emit("H4A04P8_VERIFY_FNV_ACTUAL", actual_hash)
    emit("H4A04P8_VERIFY_FNV_EXPECTED", expected_hash)
    if actual_hash != expected_hash:
        raise RuntimeError("H4A04P8_SINGLE_STATUS_PAYLOAD_INTEGRITY_FAIL")
    emit("H4A04P8_SINGLE_STATUS_PAYLOAD_INTEGRITY", "PASS")

    buckets: dict[str, list[dict[str, float]]] = {
        "DOUBLE_STATUS": [],
        "SINGLE_STATUS": [],
    }
    counts = {"DOUBLE_STATUS": 0, "SINGLE_STATUS": 0}
    for index, variant in enumerate(ORDER, 1):
        counts[variant] += 1
        run_no = counts[variant]
        print()
        print("=" * 78)
        print(
            f" H4A0.4-P8 CASE {index}/{len(ORDER)} "
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
        )
        p7.require_clean_case(case_text, f"P8_{variant}_RUN{run_no}")
        row = inspect_case(case_text)
        buckets[variant].append(row)
        emit(
            "H4A04P8_RUN_RESULT",
            (
                f"VARIANT={variant} RUN={run_no} "
                f"TCP={row['mid']:.6f}Mbps "
                f"PAYLOAD={row['payload_mbps']:.3f}Mbps "
                f"SCHEDULER_US_PER_BYTE={row['scheduler_us_per_byte']:.6f} "
                f"STATUS_CALLS_PER_MB={row['status_calls_per_mb']:.3f} "
                f"PASS=YES"
            ),
        )

    expected_counts = {
        "DOUBLE_STATUS": RUNS_PER_VARIANT,
        "SINGLE_STATUS": RUNS_PER_VARIANT,
    }
    if counts != expected_counts:
        raise RuntimeError(f"H4A04P8_RUN_COUNTS_INVALID={counts}")

    print()
    print("=" * 78)
    print(" H4A0.4-P8 RECONNECT - SINGLE_STATUS")
    print("=" * 78)
    upload_variant(
        cli=cli,
        fqbn=args.fqbn,
        serial_port=args.serial,
        build_dir=perf_builds["SINGLE_STATUS"],
        sketch=sketch,
        repo=repo,
        log=result_root / "reconnect_upload.log",
        label="RECONNECT_SINGLE_STATUS",
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
        )
        p7.require_clean_case(reconnect_text, f"P8_RECONNECT_{attempt}")
        if attempt == 1 and p3.one(
            reconnect_text,
            "H4A04P1_RECONNECT_CLEANUP",
        ) != "PASS":
            raise RuntimeError("H4A04P8_RECONNECT_CLEANUP_FAILED")
        emit(f"H4A04P8_RECONNECT_CASE_{attempt}", "PASS")

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
            "status_calls_per_mb": p6.median(
                [row["status_calls_per_mb"] for row in rows]
            ),
            "status_us_per_byte": p6.median(
                [row["status_us_per_byte"] for row in rows]
            ),
        }

    base = summary["DOUBLE_STATUS"]
    candidate = summary["SINGLE_STATUS"]
    payload_delta = p6.delta_pct(candidate["payload"], base["payload"])
    tcp_delta = p6.delta_pct(candidate["tcp"], base["tcp"])
    scheduler_delta = p6.delta_pct(
        candidate["scheduler_us_per_byte"],
        base["scheduler_us_per_byte"],
    )
    hold_delta = p6.delta_pct(
        candidate["hold_us_per_byte"],
        base["hold_us_per_byte"],
    )
    status_calls_delta = p6.delta_pct(
        candidate["status_calls_per_mb"],
        base["status_calls_per_mb"],
    )
    status_time_delta = p6.delta_pct(
        candidate["status_us_per_byte"],
        base["status_us_per_byte"],
    )
    repeatability_ok = (
        base["payload_spread"] <= PAYLOAD_SPREAD_MAX_PCT
        and candidate["payload_spread"] <= PAYLOAD_SPREAD_MAX_PCT
    )
    status_reduction_ok = status_calls_delta <= -STATUS_REDUCTION_MIN_PCT

    if not repeatability_ok:
        interpretation = "PAYLOAD_REPEATABILITY_INSUFFICIENT"
    elif (
        status_reduction_ok
        and scheduler_delta <= -GAIN_THRESHOLD_PCT
        and hold_delta <= -GAIN_THRESHOLD_PCT
    ):
        interpretation = "SINGLE_STATUS_GAIN_CONFIRMED"
    elif scheduler_delta >= GAIN_THRESHOLD_PCT or hold_delta >= GAIN_THRESHOLD_PCT:
        interpretation = "SINGLE_STATUS_REGRESSION"
    elif abs(scheduler_delta) <= NO_MATERIAL_BAND_PCT:
        interpretation = "NO_MATERIAL_SINGLE_STATUS_GAIN"
    else:
        interpretation = "SINGLE_STATUS_SMALL_OR_INCONCLUSIVE_EFFECT"

    print()
    print("=" * 78)
    print(" H4A0.4-P8 SUMMARY")
    print("=" * 78)
    for variant in ("DOUBLE_STATUS", "SINGLE_STATUS"):
        for key, value in summary[variant].items():
            emit(f"H4A04P8_{variant}_{key.upper()}", f"{value:.6f}")
    emit("H4A04P8_SINGLE_VS_BASE_PAYLOAD_PCT", f"{payload_delta:.3f}")
    emit("H4A04P8_SINGLE_VS_BASE_TCP_PCT", f"{tcp_delta:.3f}")
    emit("H4A04P8_SINGLE_VS_BASE_SCHEDULER_PCT", f"{scheduler_delta:.3f}")
    emit("H4A04P8_SINGLE_VS_BASE_HOLD_PCT", f"{hold_delta:.3f}")
    emit("H4A04P8_SINGLE_VS_BASE_STATUS_CALLS_PCT", f"{status_calls_delta:.3f}")
    emit("H4A04P8_SINGLE_VS_BASE_STATUS_TIME_PCT", f"{status_time_delta:.3f}")
    emit("H4A04P8_STATUS_REDUCTION_OK", status_reduction_ok)
    emit("H4A04P8_PAYLOAD_REPEATABILITY_OK", repeatability_ok)
    emit("H4A04P8_RECONNECT", "PASS")
    emit("H4A04P8_INTERPRETATION", interpretation)
    emit("H4A04P8_TCP_SPI_LOCK_ERRORS", 0)
    emit("H4A04P8_TRANSPORT_ERRORS", 0)
    emit("H4A04P8_UNEXPECTED_RESETS", 0)

    if args.defer_physical_review:
        physical_status = "PENDING_USER"
        gate_status = "PASS_DATA_ONLY"
    else:
        answer = input(
            "¿TFT/periféricos permanecieron estables durante H4A0.4-P8? (S/N): "
        ).strip().upper()
        if answer != "S":
            raise RuntimeError("H4A04P8_PHYSICAL_STABILITY_FAILED")
        physical_status = "PASS"
        gate_status = "PASS"
    emit("H4A04P8_PHYSICAL_STABILITY", physical_status)

    if p3.git(repo, "diff", "--name-only") or p3.git(
        repo,
        "diff",
        "--cached",
        "--name-only",
    ):
        raise RuntimeError("H4A04P8_REPOSITORY_MUTATED_DURING_GATE")

    summary_log = result_root / "SUMMARY.log"
    summary_log.write_text(
        "\n".join(
            (
                f"A14_H4A04P8_SOCKET_STATUS_AB={gate_status}",
                f"HEAD={head}",
                f"VERIFY_RX_BYTES={verify_bytes}",
                f"VERIFY_FNV_ACTUAL={actual_hash}",
                f"VERIFY_FNV_EXPECTED={expected_hash}",
                "SINGLE_STATUS_PAYLOAD_INTEGRITY=PASS",
                f"DOUBLE_STATUS_PAYLOAD_MBPS={base['payload']:.3f}",
                f"SINGLE_STATUS_PAYLOAD_MBPS={candidate['payload']:.3f}",
                f"SINGLE_VS_BASE_PAYLOAD_PCT={payload_delta:.3f}",
                f"DOUBLE_STATUS_TCP_MBPS={base['tcp']:.6f}",
                f"SINGLE_STATUS_TCP_MBPS={candidate['tcp']:.6f}",
                f"SINGLE_VS_BASE_TCP_PCT={tcp_delta:.3f}",
                f"SINGLE_VS_BASE_SCHEDULER_PCT={scheduler_delta:.3f}",
                f"SINGLE_VS_BASE_HOLD_PCT={hold_delta:.3f}",
                f"SINGLE_VS_BASE_STATUS_CALLS_PCT={status_calls_delta:.3f}",
                f"SINGLE_VS_BASE_STATUS_TIME_PCT={status_time_delta:.3f}",
                f"INTERPRETATION={interpretation}",
                "RECONNECT=PASS",
                "TCP_SPI_LOCK_ERRORS=0",
                "TRANSPORT_ERRORS=0",
                "UNEXPECTED_RESETS=0",
                f"PHYSICAL_STABILITY={physical_status}",
                "FIFO_REUSE_DEFAULT=OFF",
                "HARNESS_FAILURE=NO",
                "PRODUCT_FAILURE=NO_EVIDENCE",
                "HARDWARE_FAILURE=NO_EVIDENCE",
                "NEXT=INTERPRET_P8_THEN_CONTINUE_RX_BATCH_RAW_GATE",
                "",
            )
        ),
        encoding="utf-8",
    )
    emit("H4A04P8_SUMMARY_LOG", summary_log)
    emit("HARNESS_FAILURE", "NO")
    emit("PRODUCT_FAILURE", "NO_EVIDENCE")
    emit("HARDWARE_FAILURE", "NO_EVIDENCE")
    emit("A14_H4A04P8_SOCKET_STATUS_AB", gate_status)
    emit("NEXT", "INTERPRET_P8_THEN_CONTINUE_RX_BATCH_RAW_GATE")
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
        emit("H4A04P8_FAILURE_REQUIRES_CLASSIFICATION", "YES")
        emit("H4A04P8_EXCEPTION", str(exc))
        raise SystemExit(1)
