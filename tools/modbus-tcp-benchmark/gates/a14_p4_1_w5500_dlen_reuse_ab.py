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
MICRO_DURATION_S = 3.0
PERF_DURATION_S = 15.0
TCP_CHUNK = 4096
SPI_HZ = 26_000_000

ORDER = (
    "BASELINE",
    "DLEN_REUSE",
    "DLEN_REUSE",
    "BASELINE",
    "BASELINE",
    "DLEN_REUSE",
)
RUNS_PER_VARIANT = 3

CONFIRMATION_ORDER = (
    "BASELINE",
    "DLEN_REUSE",
    "DLEN_REUSE",
    "BASELINE",
    "BASELINE",
    "DLEN_REUSE",
    "DLEN_REUSE",
    "BASELINE",
    "BASELINE",
    "DLEN_REUSE",
)
CONFIRMATION_RUNS_PER_VARIANT = 5

PAYLOAD_SPREAD_MAX_PCT = 0.75
PERF_REGRESSION_LIMIT_PCT = 0.50
PERF_GAIN_MIN_PCT = 0.25
SETUP_REDUCTION_MIN_PCT = 2.0
NO_MATERIAL_BAND_PCT = 0.25


def emit(key: str, value: object) -> None:
    print(f"{key}={value}")


def median(values: list[float]) -> float:
    if not values:
        raise RuntimeError("P4_1_EMPTY_MEDIAN")
    return float(statistics.median(values))


def spread_pct(values: list[float]) -> float:
    med = median(values)
    if med <= 0:
        raise RuntimeError("P4_1_NONPOSITIVE_MEDIAN")
    return (max(values) - min(values)) * 100.0 / med


def delta_pct(value: float, reference: float) -> float:
    if reference <= 0:
        raise RuntimeError("P4_1_NONPOSITIVE_REFERENCE")
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
    chunk_profile: bool = False,
) -> None:
    if variant == "BASELINE":
        dlen_cache = "0"
    elif variant == "DLEN_REUSE":
        dlen_cache = "1"
    else:
        raise RuntimeError(f"P4_1_UNKNOWN_VARIANT={variant}")

    ethernet_profile = "0" if (verify or chunk_profile) else "1"
    verify_flag = "1" if verify else "0"
    chunk_profile_flag = "1" if chunk_profile else "0"

    extra_flags = " ".join(
        (
            f"-DJWPLC_ETHERNET_ENABLE_PROFILE_HOOKS={ethernet_profile}",
            "-DJWPLC_W5500_RX_DIRECT_TRANSFER_BYTES=0",
            "-DJWPLC_W5500_RX_FIFO_REUSE=1",
            f"-DJWPLC_H4A04P3_VERIFY_PAYLOAD={verify_flag}",
            f"-DJWPLC_SPI_PROFILE_FIFO_REUSE_CHUNKS={chunk_profile_flag}",
            f"-DJWPLC_SPI_FIFO_REUSE_DLEN_CACHE={dlen_cache}",
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

    mode = "VERIFY" if verify else "MICRO" if chunk_profile else "PERF"
    label = f"{variant}_{mode}"
    emit(f"P4_1_{label}_COMPILE_EXIT", exit_code)

    if exit_code != 0:
        print(p3.decode(log.read_bytes())[-9000:])
        raise RuntimeError(f"P4_1_{label}_COMPILE_FAILED")

    normalized = p3.decode(log.read_bytes()).replace("\\", "/").lower()

    required = (
        f"-djwplc_ethernet_enable_profile_hooks={ethernet_profile}",
        "-djwplc_w5500_rx_direct_transfer_bytes=0",
        "-djwplc_w5500_rx_fifo_reuse=1",
        f"-djwplc_h4a04p3_verify_payload={verify_flag}",
        f"-djwplc_spi_profile_fifo_reuse_chunks={chunk_profile_flag}",
        f"-djwplc_spi_fifo_reuse_dlen_cache={dlen_cache}",
        "w5100.cpp",
        "socket.cpp",
        "spi.cpp",
        "jwplc_idlescreen.cpp",
        "jwplc_tft.cpp",
    )

    for token in required:
        ok = token in normalized
        emit(
            f"P4_1_{label}_TOKEN_{token.replace('=', '_')}",
            "PASS" if ok else "FAIL",
        )
        if not ok:
            raise RuntimeError(f"P4_1_{label}_TOKEN_MISSING={token}")

    for archive in (
        "libjwplc_display.a",
        "libjwplc_tft.a",
        "libspi.a",
    ):
        if archive in normalized:
            raise RuntimeError(
                f"P4_1_{label}_UNEXPECTED_ARCHIVE={archive}"
            )


def upload(
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
    emit(f"P4_1_{label}_UPLOAD_EXIT", exit_code)
    if exit_code != 0:
        print(p3.decode(log.read_bytes())[-7000:])
        raise RuntimeError(f"P4_1_{label}_UPLOAD_FAILED")


def run_case(
    *,
    runner: Path,
    repo: Path,
    serial_port: str,
    duration_s: float,
    variant_arg: str,
    log: Path,
    label: str,
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
            str(TCP_CHUNK),
            "--variant",
            variant_arg,
        ],
        log,
        repo,
    )
    emit(f"P4_1_{label}_CASE_EXIT", exit_code)

    text = p3.decode(log.read_bytes())
    if exit_code != 0:
        print(text[-9000:])
        raise RuntimeError(f"P4_1_{label}_RUNTIME_FAILED")

    if p3.one(text, "H4A04P1_FUNCTIONAL_PASS") != "YES":
        raise RuntimeError(f"P4_1_{label}_FUNCTIONAL_FAIL")

    if p3.integer(text, "H4A04P1_TRANSPORT_ERRORS") != 0:
        raise RuntimeError(f"P4_1_{label}_TRANSPORT_ERRORS")

    if p3.integer(text, "H4A04P1_TCP_SPI_LOCK_ERRORS") != 0:
        raise RuntimeError(f"P4_1_{label}_SPI_LOCK_ERRORS")

    return text


def inspect_micro(text: str) -> dict[str, float]:
    if p3.one(text, "H4A04P1_SPI_CHUNK_PROFILE_ENABLED") != "YES":
        raise RuntimeError("P4_1_MICRO_PROFILE_NOT_ACTIVE")

    count = p3.integer(text, "H4A04P1_SPI_CHUNK_COUNT")
    byte_count = p3.integer(text, "H4A04P1_SPI_CHUNK_BYTES")
    setup_us = p3.integer(text, "H4A04P1_SPI_CHUNK_SETUP_TOTAL_US")
    wire_us = p3.integer(text, "H4A04P1_SPI_CHUNK_WIRE_WAIT_TOTAL_US")
    copy_us = p3.integer(text, "H4A04P1_SPI_CHUNK_COPY_OUT_TOTAL_US")
    other_us = p3.integer(text, "H4A04P1_SPI_CHUNK_OTHER_TOTAL_US")

    if count <= 0 or byte_count <= 0:
        raise RuntimeError("P4_1_MICRO_EMPTY")

    total_us = setup_us + wire_us + copy_us + other_us

    return {
        "count": float(count),
        "bytes": float(byte_count),
        "setup_us": float(setup_us),
        "setup_us_per_chunk": setup_us / count,
        "wire_us": float(wire_us),
        "copy_us": float(copy_us),
        "other_us": float(other_us),
        "total_us": float(total_us),
        "us_per_byte": total_us / byte_count,
    }


def main() -> int:
    parser = argparse.ArgumentParser(
        description="A14 P4.1 W5500 FIFO DLEN reuse A/B"
    )
    parser.add_argument("--serial", default="COM14")
    parser.add_argument("--arduino-cli")
    parser.add_argument(
        "--fqbn",
        default="jwplc_local:esp32:jwplcbasic",
    )
    parser.add_argument(
        "--confirmation",
        action="store_true",
        help="Usa cinco repeticiones por variante para confirmar un efecto pequeño.",
    )
    parser.add_argument(
        "--defer-physical-review",
        action="store_true",
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

    order = CONFIRMATION_ORDER if args.confirmation else ORDER
    runs_per_variant = (
        CONFIRMATION_RUNS_PER_VARIANT
        if args.confirmation
        else RUNS_PER_VARIANT
    )

    print("=" * 78)
    print(" A14 P4.1 - W5500 FIFO DLEN REUSE A/B")
    print("=" * 78)

    branch = p3.git(repo, "branch", "--show-current")
    head = p3.git(repo, "rev-parse", "HEAD")

    emit("P4_1_BRANCH", branch)
    emit("P4_1_HEAD", head)
    emit("P4_1_SERIAL", args.serial)
    emit("P4_1_SPI_HZ", SPI_HZ)
    emit("P4_1_FIFO_REUSE", "ON_BOTH_VARIANTS")
    emit("P4_1_DIRECT_RX", "OFF_BOTH_VARIANTS")
    emit("P4_1_BASELINE", "DLEN_WRITE_EVERY_CHUNK")
    emit("P4_1_CANDIDATE", "DLEN_CACHE_WITHIN_HELPER_CALL")
    emit("P4_1_CANDIDATE_DEFAULT", "OFF")
    emit("P4_1_ONLY_VARIABLE", "DLEN_PROGRAMMING_POLICY")
    emit("P4_1_VERIFY_DURATION_S", VERIFY_DURATION_S)
    emit("P4_1_MICRO_DURATION_S", MICRO_DURATION_S)
    emit("P4_1_PERF_DURATION_S", PERF_DURATION_S)
    emit("P4_1_RUNS_PER_VARIANT", runs_per_variant)
    emit("P4_1_ORDER", ",".join(order))

    if branch != BRANCH:
        raise RuntimeError("P4_1_BRANCH_MISMATCH")

    if p3.git(repo, "diff", "--name-only") or p3.git(
        repo,
        "diff",
        "--cached",
        "--name-only",
    ):
        raise RuntimeError("P4_1_TREE_NOT_CLEAN")

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
    emit("P4_1_UNTRACKED_PRODUCT_COUNT", len(untracked_product))
    if untracked_product:
        raise RuntimeError("P4_1_UNTRACKED_PRODUCT_SOURCE")

    ast.parse(
        runner.read_text(encoding="utf-8"),
        filename=str(runner),
    )
    emit("P4_1_RUNNER_AST", "PASS")

    contract_run = p3.run(
        [sys.executable, "-B", str(contract)],
        repo,
    )
    print(p3.decode(contract_run.stdout))
    if contract_run.returncode != 0:
        raise RuntimeError("P4_1_PACKAGE_CONTRACT_FAILED")

    cli = p3.find_cli(args.arduino_cli)
    emit("ARDUINO_CLI", cli)

    result_root = Path(
        tempfile.mkdtemp(prefix="jwplc_a14_p4_1_dlen_reuse_")
    )
    emit("P4_1_RESULT_ROOT", result_root)

    builds = {
        "verify": result_root / "build_verify_candidate",
        "micro_baseline": result_root / "build_micro_baseline",
        "micro_candidate": result_root / "build_micro_candidate",
        "perf_baseline": result_root / "build_perf_baseline",
        "perf_candidate": result_root / "build_perf_candidate",
    }

    compile_variant(
        cli=cli,
        fqbn=args.fqbn,
        repo=repo,
        sketch=sketch,
        libraries=libraries,
        build_dir=builds["verify"],
        variant="DLEN_REUSE",
        log=result_root / "compile_verify_candidate.log",
        verify=True,
    )

    for variant, key in (
        ("BASELINE", "micro_baseline"),
        ("DLEN_REUSE", "micro_candidate"),
    ):
        compile_variant(
            cli=cli,
            fqbn=args.fqbn,
            repo=repo,
            sketch=sketch,
            libraries=libraries,
            build_dir=builds[key],
            variant=variant,
            log=result_root / f"compile_{key}.log",
            chunk_profile=True,
        )

    for variant, key in (
        ("BASELINE", "perf_baseline"),
        ("DLEN_REUSE", "perf_candidate"),
    ):
        compile_variant(
            cli=cli,
            fqbn=args.fqbn,
            repo=repo,
            sketch=sketch,
            libraries=libraries,
            build_dir=builds[key],
            variant=variant,
            log=result_root / f"compile_{key}.log",
        )

    print()
    print("=" * 78)
    print(" P4.1 PAYLOAD INTEGRITY - CANDIDATE")
    print("=" * 78)

    upload(
        cli=cli,
        fqbn=args.fqbn,
        serial_port=args.serial,
        build_dir=builds["verify"],
        sketch=sketch,
        repo=repo,
        log=result_root / "verify_upload.log",
        label="VERIFY",
    )
    time.sleep(3.0)

    verify_text = run_case(
        runner=runner,
        repo=repo,
        serial_port=args.serial,
        duration_s=VERIFY_DURATION_S,
        variant_arg="BASE",
        log=result_root / "verify_case.log",
        label="VERIFY",
    )

    if p3.one(verify_text, "H4A04P1_PAYLOAD_VERIFY_ENABLED") != "YES":
        raise RuntimeError("P4_1_VERIFY_NOT_ACTIVE")

    verify_bytes = p3.integer(verify_text, "H4A04P1_DUT_RX_BYTES")
    verify_actual = p3.integer(verify_text, "H4A04P1_RX_FNV1A32")
    verify_expected = p3.fnv1a_expected(verify_bytes)

    emit("P4_1_VERIFY_RX_BYTES", verify_bytes)
    emit("P4_1_VERIFY_FNV_ACTUAL", verify_actual)
    emit("P4_1_VERIFY_FNV_EXPECTED", verify_expected)

    if verify_actual != verify_expected:
        raise RuntimeError("P4_1_CANDIDATE_FNV_MISMATCH")

    emit("P4_1_PAYLOAD_INTEGRITY", "PASS")

    print()
    print("=" * 78)
    print(" P4.1 MICROPROFILE BASELINE / CANDIDATE")
    print("=" * 78)

    micro: dict[str, dict[str, float]] = {}

    for variant, key in (
        ("BASELINE", "micro_baseline"),
        ("DLEN_REUSE", "micro_candidate"),
    ):
        upload(
            cli=cli,
            fqbn=args.fqbn,
            serial_port=args.serial,
            build_dir=builds[key],
            sketch=sketch,
            repo=repo,
            log=result_root / f"micro_{variant.lower()}_upload.log",
            label=f"MICRO_{variant}",
        )
        time.sleep(3.0)

        text = run_case(
            runner=runner,
            repo=repo,
            serial_port=args.serial,
            duration_s=MICRO_DURATION_S,
            variant_arg="BASE",
            log=result_root / f"micro_{variant.lower()}_case.log",
            label=f"MICRO_{variant}",
        )
        micro[variant] = inspect_micro(text)

        row = micro[variant]
        emit(
            "P4_1_MICRO_RESULT",
            (
                f"VARIANT={variant} "
                f"CHUNKS={int(row['count'])} "
                f"BYTES={int(row['bytes'])} "
                f"SETUP_US_PER_CHUNK={row['setup_us_per_chunk']:.6f} "
                f"PROFILED_US_PER_BYTE={row['us_per_byte']:.6f}"
            ),
        )

    setup_delta = delta_pct(
        micro["DLEN_REUSE"]["setup_us_per_chunk"],
        micro["BASELINE"]["setup_us_per_chunk"],
    )
    micro_cost_delta = delta_pct(
        micro["DLEN_REUSE"]["us_per_byte"],
        micro["BASELINE"]["us_per_byte"],
    )

    emit(
        "P4_1_SETUP_US_PER_CHUNK_BASELINE",
        f"{micro['BASELINE']['setup_us_per_chunk']:.6f}",
    )
    emit(
        "P4_1_SETUP_US_PER_CHUNK_CANDIDATE",
        f"{micro['DLEN_REUSE']['setup_us_per_chunk']:.6f}",
    )
    emit("P4_1_SETUP_DELTA_PCT", f"{setup_delta:.3f}")
    emit("P4_1_MICRO_US_PER_BYTE_DELTA_PCT", f"{micro_cost_delta:.3f}")

    print()
    print("=" * 78)
    print(" P4.1 UNPROFILED PERFORMANCE A/B")
    print("=" * 78)

    buckets: dict[str, list[dict[str, float]]] = {
        "BASELINE": [],
        "DLEN_REUSE": [],
    }
    counts = {"BASELINE": 0, "DLEN_REUSE": 0}

    for case_index, variant in enumerate(order, 1):
        counts[variant] += 1
        run_no = counts[variant]
        build_key = (
            "perf_baseline"
            if variant == "BASELINE"
            else "perf_candidate"
        )

        print()
        print(
            f"--- P4.1 CASE {case_index}/{len(order)} "
            f"{variant} RUN {run_no}/{runs_per_variant} ---"
        )

        upload(
            cli=cli,
            fqbn=args.fqbn,
            serial_port=args.serial,
            build_dir=builds[build_key],
            sketch=sketch,
            repo=repo,
            log=result_root
            / f"{case_index:02d}_{variant.lower()}_upload.log",
            label=f"{variant}_RUN{run_no}",
        )
        time.sleep(3.0)

        text = run_case(
            runner=runner,
            repo=repo,
            serial_port=args.serial,
            duration_s=PERF_DURATION_S,
            variant_arg="PROFILE",
            log=result_root
            / f"{case_index:02d}_{variant.lower()}_run{run_no}.log",
            label=f"{variant}_RUN{run_no}",
        )

        row = p3.inspect_profile_case(text)
        buckets[variant].append(row)

        emit(
            "P4_1_RUN_RESULT",
            (
                f"VARIANT={variant} RUN={run_no} "
                f"TCP={row['mid']:.6f}Mbps "
                f"PAYLOAD={row['payload_mbps']:.3f}Mbps "
                f"US_PER_BYTE={row['us_per_byte']:.6f}"
            ),
        )

    summary: dict[str, dict[str, float]] = {}
    for variant in ("BASELINE", "DLEN_REUSE"):
        rows = buckets[variant]
        payload = [row["payload_mbps"] for row in rows]
        tcp = [row["mid"] for row in rows]
        cost = [row["us_per_byte"] for row in rows]

        summary[variant] = {
            "payload": median(payload),
            "payload_spread": spread_pct(payload),
            "tcp": median(tcp),
            "tcp_spread": spread_pct(tcp),
            "us_per_byte": median(cost),
        }

    base = summary["BASELINE"]
    candidate = summary["DLEN_REUSE"]

    payload_delta = delta_pct(candidate["payload"], base["payload"])
    tcp_delta = delta_pct(candidate["tcp"], base["tcp"])
    cost_delta = delta_pct(candidate["us_per_byte"], base["us_per_byte"])

    repeatability_ok = (
        base["payload_spread"] <= PAYLOAD_SPREAD_MAX_PCT
        and candidate["payload_spread"] <= PAYLOAD_SPREAD_MAX_PCT
    )

    mechanism_confirmed = setup_delta <= -SETUP_REDUCTION_MIN_PCT
    regression = (
        payload_delta <= -PERF_REGRESSION_LIMIT_PCT
        or cost_delta >= PERF_REGRESSION_LIMIT_PCT
    )
    system_gain = (
        payload_delta >= PERF_GAIN_MIN_PCT
        and cost_delta <= -PERF_GAIN_MIN_PCT
    )

    if not mechanism_confirmed:
        interpretation = "DLEN_REUSE_MECHANISM_NOT_CONFIRMED"
    elif regression:
        interpretation = "DLEN_REUSE_REGRESSION"
    elif not repeatability_ok:
        interpretation = "DLEN_REUSE_REPEATABILITY_INSUFFICIENT"
    elif system_gain:
        interpretation = "DLEN_REUSE_GAIN_CONFIRMED"
    elif (
        abs(payload_delta) <= NO_MATERIAL_BAND_PCT
        and abs(cost_delta) <= NO_MATERIAL_BAND_PCT
    ):
        interpretation = "DLEN_REUSE_NO_MATERIAL_SYSTEM_GAIN"
    else:
        interpretation = "DLEN_REUSE_SMALL_OR_INCONCLUSIVE_EFFECT"

    print()
    print("=" * 78)
    print(" P4.1 SUMMARY")
    print("=" * 78)

    for variant in ("BASELINE", "DLEN_REUSE"):
        s = summary[variant]
        emit(
            f"P4_1_{variant}_PAYLOAD_MBPS",
            f"{s['payload']:.3f}",
        )
        emit(
            f"P4_1_{variant}_PAYLOAD_SPREAD_PCT",
            f"{s['payload_spread']:.3f}",
        )
        emit(
            f"P4_1_{variant}_TCP_MBPS",
            f"{s['tcp']:.6f}",
        )
        emit(
            f"P4_1_{variant}_TCP_SPREAD_PCT",
            f"{s['tcp_spread']:.3f}",
        )
        emit(
            f"P4_1_{variant}_US_PER_BYTE",
            f"{s['us_per_byte']:.6f}",
        )

    emit("P4_1_PAYLOAD_DELTA_PCT", f"{payload_delta:.3f}")
    emit("P4_1_TCP_DELTA_PCT", f"{tcp_delta:.3f}")
    emit("P4_1_US_PER_BYTE_DELTA_PCT", f"{cost_delta:.3f}")
    emit("P4_1_REPEATABILITY_OK", "YES" if repeatability_ok else "NO")
    emit(
        "P4_1_MECHANISM_CONFIRMED",
        "YES" if mechanism_confirmed else "NO",
    )
    emit("P4_1_INTERPRETATION", interpretation)

    if args.defer_physical_review:
        physical = "PENDING_USER"
        gate_status = "PASS_DATA_ONLY"
    else:
        answer = input(
            "¿TFT/periféricos permanecieron estables durante P4.1? (S/N): "
        ).strip().upper()
        if answer != "S":
            raise RuntimeError("P4_1_PHYSICAL_STABILITY_FAILED")
        physical = "PASS"
        gate_status = "PASS"

    emit("P4_1_PHYSICAL_STABILITY", physical)

    if p3.git(repo, "diff", "--name-only") or p3.git(
        repo,
        "diff",
        "--cached",
        "--name-only",
    ):
        raise RuntimeError("P4_1_REPOSITORY_MUTATED_DURING_GATE")

    next_action = {
        "DLEN_REUSE_GAIN_CONFIRMED":
            "RETURN_TO_CHAT_PROMOTION_DECISION_BEFORE_P4_2",
        "DLEN_REUSE_SMALL_OR_INCONCLUSIVE_EFFECT":
            "RETURN_TO_CHAT_DECIDE_P4_1_CONFIRMATION",
        "DLEN_REUSE_REPEATABILITY_INSUFFICIENT":
            "RETURN_TO_CHAT_DECIDE_P4_1_CONFIRMATION",
        "DLEN_REUSE_NO_MATERIAL_SYSTEM_GAIN":
            "RETURN_TO_CHAT_REJECT_OR_CONFIRM_P4_1",
        "DLEN_REUSE_REGRESSION":
            "RETURN_TO_CHAT_REJECT_P4_1",
        "DLEN_REUSE_MECHANISM_NOT_CONFIRMED":
            "RETURN_TO_CHAT_REJECT_P4_1",
    }[interpretation]

    summary_log = result_root / "SUMMARY.log"
    summary_log.write_text(
        "\n".join(
            [
                f"A14_P4_1_DLEN_REUSE={gate_status}",
                f"HEAD={head}",
                "ONLY_VARIABLE=DLEN_PROGRAMMING_POLICY",
                "FIFO_REUSE=ON_BOTH_VARIANTS",
                "DIRECT_RX=OFF_BOTH_VARIANTS",
                "CANDIDATE_DEFAULT=OFF",
                f"VERIFY_RX_BYTES={verify_bytes}",
                f"VERIFY_FNV_ACTUAL={verify_actual}",
                f"VERIFY_FNV_EXPECTED={verify_expected}",
                "PAYLOAD_INTEGRITY=PASS",
                f"SETUP_US_PER_CHUNK_BASELINE={micro['BASELINE']['setup_us_per_chunk']:.6f}",
                f"SETUP_US_PER_CHUNK_CANDIDATE={micro['DLEN_REUSE']['setup_us_per_chunk']:.6f}",
                f"SETUP_DELTA_PCT={setup_delta:.3f}",
                f"MICRO_US_PER_BYTE_DELTA_PCT={micro_cost_delta:.3f}",
                f"BASELINE_PAYLOAD_MBPS={base['payload']:.3f}",
                f"CANDIDATE_PAYLOAD_MBPS={candidate['payload']:.3f}",
                f"PAYLOAD_DELTA_PCT={payload_delta:.3f}",
                f"BASELINE_TCP_MBPS={base['tcp']:.6f}",
                f"CANDIDATE_TCP_MBPS={candidate['tcp']:.6f}",
                f"TCP_DELTA_PCT={tcp_delta:.3f}",
                f"BASELINE_US_PER_BYTE={base['us_per_byte']:.6f}",
                f"CANDIDATE_US_PER_BYTE={candidate['us_per_byte']:.6f}",
                f"US_PER_BYTE_DELTA_PCT={cost_delta:.3f}",
                f"REPEATABILITY_OK={'YES' if repeatability_ok else 'NO'}",
                f"MECHANISM_CONFIRMED={'YES' if mechanism_confirmed else 'NO'}",
                f"INTERPRETATION={interpretation}",
                f"PHYSICAL_STABILITY={physical}",
                "HARNESS_FAILURE=NO",
                "PRODUCT_FAILURE=NO_EVIDENCE",
                "HARDWARE_FAILURE=NO_EVIDENCE",
                f"NEXT={next_action}",
                "",
            ]
        ),
        encoding="utf-8",
    )

    emit("P4_1_SUMMARY_LOG", summary_log)
    emit("HARNESS_FAILURE", "NO")
    emit("PRODUCT_FAILURE", "NO_EVIDENCE")
    emit("HARDWARE_FAILURE", "NO_EVIDENCE")
    emit("A14_P4_1_DLEN_REUSE", gate_status)
    emit("NEXT", next_action)
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
        emit("P4_1_FAILURE_REQUIRES_CLASSIFICATION", "YES")
        emit("P4_1_EXCEPTION", str(exc))
        raise SystemExit(1)
