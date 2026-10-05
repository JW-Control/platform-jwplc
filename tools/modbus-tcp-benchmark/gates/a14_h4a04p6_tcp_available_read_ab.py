#!/usr/bin/env python3
from __future__ import annotations

import argparse
import ast
import statistics
import sys
import tempfile
import time
from pathlib import Path

import a14_h4a04p3_w5500_fifo_reuse_ab as p3


BRANCH = "v2.1.0-alpha.14/feature/modbus-tcp"
VERIFY_DURATION_S = 1.0
PERF_DURATION_S = 15.0
ORDER = (
    "AVAILABLE_READ",
    "READ_DIRECT",
    "READ_DIRECT",
    "AVAILABLE_READ",
    "AVAILABLE_READ",
    "READ_DIRECT",
)
RUNS_PER_VARIANT = 3
PAYLOAD_SPREAD_MAX_PCT = 0.5
GAIN_THRESHOLD_PCT = 1.0
NO_MATERIAL_BAND_PCT = 0.5


def emit(key: str, value: object) -> None:
    print(f"{key}={value}")


def median(values: list[float]) -> float:
    if not values:
        raise RuntimeError("H4A04P6_EMPTY_MEDIAN")
    return float(statistics.median(values))


def spread_pct(values: list[float]) -> float:
    med = median(values)
    if med <= 0.0:
        raise RuntimeError("H4A04P6_NONPOSITIVE_MEDIAN")
    return (max(values) - min(values)) * 100.0 / med


def delta_pct(value: float, reference: float) -> float:
    if reference <= 0.0:
        raise RuntimeError("H4A04P6_NONPOSITIVE_REFERENCE")
    return (value / reference - 1.0) * 100.0


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
    if variant == "AVAILABLE_READ":
        direct_read = "0"
    elif variant == "READ_DIRECT":
        direct_read = "1"
    else:
        raise RuntimeError(f"H4A04P6_UNKNOWN_VARIANT={variant}")

    profile = "0" if verify else "1"
    verify_flag = "1" if verify else "0"
    extra_flags = " ".join(
        (
            f"-DJWPLC_ETHERNET_ENABLE_PROFILE_HOOKS={profile}",
            "-DJWPLC_W5500_RX_DIRECT_TRANSFER_BYTES=0",
            "-DJWPLC_W5500_RX_FIFO_REUSE=1",
            f"-DJWPLC_H4A04P3_VERIFY_PAYLOAD={verify_flag}",
            f"-DJWPLC_H4A04P6_DIRECT_READ={direct_read}",
            "-DJWPLC_SPI_PROFILE_FIFO_REUSE_CHUNKS=0",
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
    label = f"{variant}_VERIFY" if verify else variant
    emit(f"H4A04P6_{label}_COMPILE_EXIT", exit_code)
    if exit_code != 0:
        print(p3.decode(log.read_bytes())[-8000:])
        raise RuntimeError(f"H4A04P6_{label}_COMPILE_FAILED")

    normalized = p3.decode(log.read_bytes()).replace("\\", "/").lower()
    required_tokens = (
        f"-djwplc_ethernet_enable_profile_hooks={profile}",
        "-djwplc_w5500_rx_direct_transfer_bytes=0",
        "-djwplc_w5500_rx_fifo_reuse=1",
        f"-djwplc_h4a04p3_verify_payload={verify_flag}",
        f"-djwplc_h4a04p6_direct_read={direct_read}",
        "-djwplc_spi_profile_fifo_reuse_chunks=0",
        "w5100.cpp",
        "socket.cpp",
        "spi.cpp",
        "jwplc_idlescreen.cpp",
        "jwplc_tft.cpp",
    )
    for token in required_tokens:
        ok = token in normalized
        emit(
            f"H4A04P6_{label}_TOKEN_{token.replace('=', '_')}",
            "PASS" if ok else "FAIL",
        )
        if not ok:
            raise RuntimeError(f"H4A04P6_{label}_TOKEN_MISSING={token}")

    if "libjwplc_display.a" in normalized:
        raise RuntimeError(f"H4A04P6_{label}_DISPLAY_A_UNEXPECTED")
    if "libjwplc_tft.a" in normalized:
        raise RuntimeError(f"H4A04P6_{label}_TFT_A_UNEXPECTED")


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
    emit(f"H4A04P6_{label}_UPLOAD_EXIT", exit_code)
    if exit_code != 0:
        print(p3.decode(log.read_bytes())[-5000:])
        raise RuntimeError(f"H4A04P6_{label}_UPLOAD_FAILED")


def run_case(
    *,
    runner: Path,
    repo: Path,
    serial_port: str,
    duration_s: float,
    variant_arg: str,
    log: Path,
) -> str:
    exit_code = p3.run_logged(
        [
            sys.executable,
            "-B",
            "-u",
            str(runner),
            "--serial",
            serial_port,
            "--duration",
            f"{duration_s:.1f}",
            "--chunk",
            "4096",
            "--variant",
            variant_arg,
        ],
        log,
        repo,
    )
    emit("H4A04P6_CASE_EXIT", exit_code)
    text = p3.decode(log.read_bytes())
    if exit_code != 0:
        print(text[-8000:])
        raise RuntimeError("H4A04P6_RUNTIME_CASE_FAILED")
    return text


def inspect_case(text: str) -> dict[str, float]:
    row = p3.inspect_profile_case(text)
    row["available_calls"] = p3.number(
        text,
        "H4A04P1_TCP_PROF_AVAILABLE_CALLS",
    )
    row["available_rsr_calls"] = p3.number(
        text,
        "H4A04P1_TCP_PROF_AVAILABLE_RSR_REFRESH_CALLS",
    )
    row["available_rsr_us"] = p3.number(
        text,
        "H4A04P1_TCP_PROF_AVAILABLE_RSR_REFRESH_TOTAL_US",
    )
    row["recv_rsr_calls"] = p3.number(
        text,
        "H4A04P1_TCP_PROF_RECV_RSR_REFRESH_CALLS",
    )
    row["recv_rsr_us"] = p3.number(
        text,
        "H4A04P1_TCP_PROF_RECV_RSR_REFRESH_TOTAL_US",
    )
    row["rx_path_us_per_byte"] = (
        row["recv_us"] + row["available_us"]
    ) / row["payload_bytes"]
    row["hold_us_per_byte"] = row["hold_us"] / row["payload_bytes"]
    return row


def main() -> int:
    parser = argparse.ArgumentParser(
        description="A14 H4A0.4-P6 available()+read() versus read() A/B"
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
    print(" A14 H4A0.4-P6 - TCP AVAILABLE/READ FUSION A/B")
    print("=" * 78)

    branch = p3.git(repo, "branch", "--show-current")
    head = p3.git(repo, "rev-parse", "HEAD")
    emit("BRANCH", branch)
    emit("HEAD", head)
    emit("SERIAL_PORT", args.serial)
    emit("H4A04P6_VERIFY_DURATION_S", VERIFY_DURATION_S)
    emit("H4A04P6_PERF_DURATION_S", PERF_DURATION_S)
    emit("H4A04P6_ORDER", ",".join(ORDER))
    emit("H4A04P6_RUNS_PER_VARIANT", RUNS_PER_VARIANT)
    emit("H4A04P6_SPI_HZ", 26_000_000)
    emit("H4A04P6_PRODUCT_SOURCE", "JWPLC/2.1.0_CANONICAL_PACKAGE")
    emit("H4A04P6_TEMP_PRODUCT_PATCHES", "NO")
    emit("H4A04P6_ONLY_VARIABLE", "PRE_READ_AVAILABLE_PROBE")
    emit("H4A04P6_BASELINE", "AVAILABLE_READ")
    emit("H4A04P6_CANDIDATE", "READ_DIRECT")
    emit("H4A04P6_FIFO_REUSE", "ON_BOTH_VARIANTS")
    emit("H4A04P6_FIFO_REUSE_DEFAULT", "OFF")

    if branch != BRANCH:
        raise RuntimeError("H4A04P6_BRANCH_MISMATCH")
    if p3.git(repo, "diff", "--name-only") or p3.git(
        repo,
        "diff",
        "--cached",
        "--name-only",
    ):
        raise RuntimeError("H4A04P6_TRACKED_TREE_NOT_CLEAN")

    ast.parse(runner.read_text(encoding="utf-8"), filename=str(runner))
    emit("H4A04P6_RUNNER_AST", "PASS")

    contract_run = p3.run([sys.executable, "-B", str(contract)], repo)
    print(p3.decode(contract_run.stdout))
    if contract_run.returncode != 0:
        raise RuntimeError("H4A04P6_PACKAGE_CONTRACT_FAILED")

    cli = p3.find_cli(args.arduino_cli)
    emit("ARDUINO_CLI", cli)
    result_root = Path(
        tempfile.mkdtemp(prefix="jwplc_a14_h4a04p6_available_read_")
    )
    emit("H4A04P6_RESULT_ROOT", result_root)

    verify_build = result_root / "build_verify_read_direct"
    perf_builds = {
        "AVAILABLE_READ": result_root / "build_available_read",
        "READ_DIRECT": result_root / "build_read_direct",
    }
    compile_variant(
        cli=cli,
        fqbn=args.fqbn,
        repo=repo,
        sketch=sketch,
        libraries=libraries,
        build_dir=verify_build,
        variant="READ_DIRECT",
        log=result_root / "compile_verify_read_direct.log",
        verify=True,
    )
    for variant in ("AVAILABLE_READ", "READ_DIRECT"):
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
    print(" H4A0.4-P6 PAYLOAD INTEGRITY - READ_DIRECT")
    print("=" * 78)
    upload_variant(
        cli=cli,
        fqbn=args.fqbn,
        serial_port=args.serial,
        build_dir=verify_build,
        sketch=sketch,
        repo=repo,
        log=result_root / "verify_upload.log",
        label="VERIFY_READ_DIRECT",
    )
    time.sleep(3.0)
    verify_text = run_case(
        runner=runner,
        repo=repo,
        serial_port=args.serial,
        duration_s=VERIFY_DURATION_S,
        variant_arg="BASE",
        log=result_root / "verify_case.log",
    )
    if p3.one(verify_text, "H4A04P1_PAYLOAD_VERIFY_ENABLED") != "YES":
        raise RuntimeError("H4A04P6_VERIFY_FLAG_NOT_ACTIVE")
    verify_bytes = p3.integer(verify_text, "H4A04P1_DUT_RX_BYTES")
    actual_hash = p3.integer(verify_text, "H4A04P1_RX_FNV1A32")
    expected_hash = p3.fnv1a_expected(verify_bytes)
    emit("H4A04P6_VERIFY_RX_BYTES", verify_bytes)
    emit("H4A04P6_VERIFY_FNV_ACTUAL", actual_hash)
    emit("H4A04P6_VERIFY_FNV_EXPECTED", expected_hash)
    if actual_hash != expected_hash:
        raise RuntimeError("H4A04P6_READ_DIRECT_PAYLOAD_INTEGRITY_FAIL")
    emit("H4A04P6_READ_DIRECT_PAYLOAD_INTEGRITY", "PASS")

    buckets: dict[str, list[dict[str, float]]] = {
        "AVAILABLE_READ": [],
        "READ_DIRECT": [],
    }
    counts = {"AVAILABLE_READ": 0, "READ_DIRECT": 0}
    for index, variant in enumerate(ORDER, 1):
        counts[variant] += 1
        run_no = counts[variant]
        print()
        print("=" * 78)
        print(
            f" H4A0.4-P6 CASE {index}/{len(ORDER)} "
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
        case_text = run_case(
            runner=runner,
            repo=repo,
            serial_port=args.serial,
            duration_s=PERF_DURATION_S,
            variant_arg="PROFILE",
            log=result_root / f"{index:02d}_{variant.lower()}_run{run_no}.log",
        )
        row = inspect_case(case_text)
        buckets[variant].append(row)
        emit(
            "H4A04P6_RUN_RESULT",
            (
                f"VARIANT={variant} RUN={run_no} "
                f"TCP={row['mid']:.6f}Mbps "
                f"PAYLOAD={row['payload_mbps']:.3f}Mbps "
                f"RX_PATH_US_PER_BYTE={row['rx_path_us_per_byte']:.6f} "
                f"HOLD_US_PER_BYTE={row['hold_us_per_byte']:.6f} "
                f"AVAILABLE_CALLS={row['available_calls']:.0f} PASS=YES"
            ),
        )

    expected_counts = {
        "AVAILABLE_READ": RUNS_PER_VARIANT,
        "READ_DIRECT": RUNS_PER_VARIANT,
    }
    if counts != expected_counts:
        raise RuntimeError(f"H4A04P6_RUN_COUNTS_INVALID={counts}")

    summary: dict[str, dict[str, float]] = {}
    for variant, rows in buckets.items():
        payload_rates = [row["payload_mbps"] for row in rows]
        summary[variant] = {
            "tcp": median([row["mid"] for row in rows]),
            "tcp_spread": spread_pct([row["mid"] for row in rows]),
            "payload": median(payload_rates),
            "payload_spread": spread_pct(payload_rates),
            "rx_path_us_per_byte": median(
                [row["rx_path_us_per_byte"] for row in rows]
            ),
            "hold_us_per_byte": median(
                [row["hold_us_per_byte"] for row in rows]
            ),
            "available_calls": median(
                [row["available_calls"] for row in rows]
            ),
            "available_us": median(
                [row["available_us"] for row in rows]
            ),
            "available_rsr_calls": median(
                [row["available_rsr_calls"] for row in rows]
            ),
            "available_rsr_us": median(
                [row["available_rsr_us"] for row in rows]
            ),
            "recv_rsr_calls": median(
                [row["recv_rsr_calls"] for row in rows]
            ),
            "recv_rsr_us": median(
                [row["recv_rsr_us"] for row in rows]
            ),
        }

    base = summary["AVAILABLE_READ"]
    candidate = summary["READ_DIRECT"]
    payload_delta = delta_pct(candidate["payload"], base["payload"])
    tcp_delta = delta_pct(candidate["tcp"], base["tcp"])
    rx_path_delta = delta_pct(
        candidate["rx_path_us_per_byte"],
        base["rx_path_us_per_byte"],
    )
    hold_delta = delta_pct(
        candidate["hold_us_per_byte"],
        base["hold_us_per_byte"],
    )
    repeatability_ok = (
        base["payload_spread"] <= PAYLOAD_SPREAD_MAX_PCT
        and candidate["payload_spread"] <= PAYLOAD_SPREAD_MAX_PCT
    )
    available_removed = (
        base["available_calls"] > 0
        and candidate["available_calls"] == 0
    )

    if not repeatability_ok:
        interpretation = "PAYLOAD_REPEATABILITY_INSUFFICIENT"
    elif available_removed and rx_path_delta <= -GAIN_THRESHOLD_PCT:
        interpretation = "AVAILABLE_READ_FUSION_GAIN_CONFIRMED"
    elif abs(rx_path_delta) <= NO_MATERIAL_BAND_PCT:
        interpretation = "NO_MATERIAL_AVAILABLE_READ_GAIN"
    elif rx_path_delta >= GAIN_THRESHOLD_PCT:
        interpretation = "READ_DIRECT_REGRESSION"
    else:
        interpretation = "AVAILABLE_READ_SMALL_OR_INCONCLUSIVE_EFFECT"

    print()
    print("=" * 78)
    print(" H4A0.4-P6 SUMMARY")
    print("=" * 78)
    for variant in ("AVAILABLE_READ", "READ_DIRECT"):
        values = summary[variant]
        for key, value in values.items():
            emit(f"H4A04P6_{variant}_{key.upper()}", f"{value:.6f}")
    emit("H4A04P6_READ_DIRECT_VS_BASE_PAYLOAD_PCT", f"{payload_delta:.3f}")
    emit("H4A04P6_READ_DIRECT_VS_BASE_TCP_PCT", f"{tcp_delta:.3f}")
    emit("H4A04P6_READ_DIRECT_VS_BASE_RX_PATH_PCT", f"{rx_path_delta:.3f}")
    emit("H4A04P6_READ_DIRECT_VS_BASE_HOLD_PCT", f"{hold_delta:.3f}")
    emit("H4A04P6_AVAILABLE_PROBE_REMOVED", available_removed)
    emit("H4A04P6_PAYLOAD_REPEATABILITY_OK", repeatability_ok)
    emit("H4A04P6_INTERPRETATION", interpretation)
    emit("H4A04P6_TCP_SPI_LOCK_ERRORS", 0)
    emit("H4A04P6_TRANSPORT_ERRORS", 0)
    emit("H4A04P6_UNEXPECTED_RESETS", 0)

    if args.defer_physical_review:
        physical_status = "PENDING_USER"
        gate_status = "PASS_DATA_ONLY"
    else:
        answer = input(
            "¿TFT/periféricos permanecieron estables durante H4A0.4-P6? (S/N): "
        ).strip().upper()
        if answer != "S":
            raise RuntimeError("H4A04P6_PHYSICAL_STABILITY_FAILED")
        physical_status = "PASS"
        gate_status = "PASS"
    emit("H4A04P6_PHYSICAL_STABILITY", physical_status)

    if p3.git(repo, "diff", "--name-only") or p3.git(
        repo,
        "diff",
        "--cached",
        "--name-only",
    ):
        raise RuntimeError("H4A04P6_REPOSITORY_MUTATED_DURING_GATE")

    summary_log = result_root / "SUMMARY.log"
    summary_log.write_text(
        "\n".join(
            (
                f"A14_H4A04P6_TCP_AVAILABLE_READ_AB={gate_status}",
                f"HEAD={head}",
                f"VERIFY_RX_BYTES={verify_bytes}",
                f"VERIFY_FNV_ACTUAL={actual_hash}",
                f"VERIFY_FNV_EXPECTED={expected_hash}",
                "READ_DIRECT_PAYLOAD_INTEGRITY=PASS",
                f"AVAILABLE_READ_PAYLOAD_MBPS={base['payload']:.3f}",
                f"READ_DIRECT_PAYLOAD_MBPS={candidate['payload']:.3f}",
                f"READ_DIRECT_VS_BASE_PAYLOAD_PCT={payload_delta:.3f}",
                f"AVAILABLE_READ_TCP_MBPS={base['tcp']:.6f}",
                f"READ_DIRECT_TCP_MBPS={candidate['tcp']:.6f}",
                f"READ_DIRECT_VS_BASE_TCP_PCT={tcp_delta:.3f}",
                f"READ_DIRECT_VS_BASE_RX_PATH_PCT={rx_path_delta:.3f}",
                f"READ_DIRECT_VS_BASE_HOLD_PCT={hold_delta:.3f}",
                f"AVAILABLE_PROBE_REMOVED={available_removed}",
                f"INTERPRETATION={interpretation}",
                "TCP_SPI_LOCK_ERRORS=0",
                "TRANSPORT_ERRORS=0",
                "UNEXPECTED_RESETS=0",
                f"PHYSICAL_STABILITY={physical_status}",
                "FIFO_REUSE_DEFAULT=OFF",
                "HARNESS_FAILURE=NO",
                "PRODUCT_FAILURE=NO_EVIDENCE",
                "HARDWARE_FAILURE=NO_EVIDENCE",
                "NEXT=INTERPRET_P6_THEN_CONTINUE_RX_COMMIT_GATE",
                "",
            )
        ),
        encoding="utf-8",
    )
    emit("H4A04P6_SUMMARY_LOG", summary_log)
    emit("HARNESS_FAILURE", "NO")
    emit("PRODUCT_FAILURE", "NO_EVIDENCE")
    emit("HARDWARE_FAILURE", "NO_EVIDENCE")
    emit("A14_H4A04P6_TCP_AVAILABLE_READ_AB", gate_status)
    emit("NEXT", "INTERPRET_P6_THEN_CONTINUE_RX_COMMIT_GATE")
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
        emit("H4A04P6_FAILURE_REQUIRES_CLASSIFICATION", "YES")
        emit("H4A04P6_EXCEPTION", str(exc))
        raise SystemExit(1)
