#!/usr/bin/env python3
from __future__ import annotations

import argparse
import ast
import re
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
    "COPY_OUT_64",
    "COPY_OUT_64",
    "BASELINE",
    "BASELINE",
    "COPY_OUT_64",
)
RUNS_PER_VARIANT = 3

CONFIRMATION_ORDER = (
    "BASELINE",
    "COPY_OUT_64",
    "COPY_OUT_64",
    "BASELINE",
    "BASELINE",
    "COPY_OUT_64",
    "COPY_OUT_64",
    "BASELINE",
    "BASELINE",
    "COPY_OUT_64",
)
CONFIRMATION_RUNS_PER_VARIANT = 5

PAYLOAD_SPREAD_MAX_PCT = 0.75
PERF_REGRESSION_LIMIT_PCT = 0.50
PERF_GAIN_MIN_PCT = 0.25
COPY_OUT_REDUCTION_MIN_PCT = 2.0
NO_MATERIAL_BAND_PCT = 0.25


class IntegrityFailure(RuntimeError):
    pass


def emit(key: str, value: object) -> None:
    print(f"{key}={value}")


def median(values: list[float]) -> float:
    if not values:
        raise RuntimeError("P4_2_EMPTY_MEDIAN")
    return float(statistics.median(values))


def spread_pct(values: list[float]) -> float:
    med = median(values)
    if med <= 0:
        raise RuntimeError("P4_2_NONPOSITIVE_MEDIAN")
    return (max(values) - min(values)) * 100.0 / med


def delta_pct(value: float, reference: float) -> float:
    if reference <= 0:
        raise RuntimeError("P4_2_NONPOSITIVE_REFERENCE")
    return (value / reference - 1.0) * 100.0


def require_define(
    text: str,
    macro: str,
    expected: str,
) -> None:
    pattern = re.compile(
        rf"^\s*#define\s+{re.escape(macro)}\s+"
        rf"{re.escape(expected)}\s*$",
        re.MULTILINE,
    )
    if not pattern.search(text):
        raise RuntimeError(
            f"P4_2_SOURCE_DEFAULT_MISMATCH={macro}:{expected}"
        )


def audit_source_contract(repo: Path) -> None:
    spi_h = (
        repo / "JWPLC" / "2.1.0" / "libraries" / "SPI" / "src" / "SPI.h"
    ).read_text(encoding="utf-8")
    spi_cpp = (
        repo / "JWPLC" / "2.1.0" / "libraries" / "SPI" / "src" / "SPI.cpp"
    ).read_text(encoding="utf-8")
    w5100_h = (
        repo
        / "JWPLC"
        / "2.1.0"
        / "libraries"
        / "JWPLC_Ethernet"
        / "src"
        / "utility"
        / "w5100.h"
    ).read_text(encoding="utf-8")

    require_define(
        spi_h,
        "JWPLC_SPI_FIFO_REUSE_COPY_OUT_64",
        "0",
    )
    require_define(
        spi_h,
        "JWPLC_SPI_FIFO_REUSE_DLEN_CACHE",
        "1",
    )
    require_define(
        w5100_h,
        "JWPLC_W5500_RX_FIFO_REUSE",
        "1",
    )
    require_define(
        w5100_h,
        "JWPLC_W5500_RX_DIRECT_TRANSFER_BYTES",
        "0",
    )

    if "SPISettings(26000000, MSBFIRST, SPI_MODE0)" not in w5100_h:
        raise RuntimeError("P4_2_W5500_SPI_HZ_MISMATCH")

    start_token = "#if JWPLC_SPI_FIFO_REUSE_COPY_OUT_64"
    start = spi_cpp.find(start_token)
    if start < 0:
        raise RuntimeError("P4_2_CANDIDATE_BLOCK_MISSING")
    end = spi_cpp.find("#endif", start)
    if end < 0:
        raise RuntimeError("P4_2_CANDIDATE_BLOCK_UNTERMINATED")
    block = spi_cpp[start:end]

    if "if (c_len == 64U)" not in block:
        raise RuntimeError("P4_2_64B_CONDITION_MISSING")
    if "memcpy" in block:
        raise RuntimeError("P4_2_MMIO_MEMCPY_FORBIDDEN")

    for index in range(16):
        line = f"result[{index}] = dev->data_buf[{index}];"
        if block.count(line) != 1:
            raise RuntimeError(
                f"P4_2_EXPLICIT_COPY_COUNT_INVALID={index}"
            )

    normalized = spi_cpp.replace("\r\n", "\n")
    tail_anchor = "} else\n#endif\n    if (c_len & 3U) {"
    if tail_anchor not in normalized:
        raise RuntimeError("P4_2_TAIL_PATH_CONTRACT_MISMATCH")

    emit("P4_2_SOURCE_CONTRACT", "PASS")
    emit("P4_2_COPY_OUT_64_DEFAULT", 0)
    emit("P4_2_FIFO_REUSE_DEFAULT", 1)
    emit("P4_2_DLEN_REUSE_DEFAULT", 1)
    emit("P4_2_DIRECT_RX_DEFAULT", 0)
    emit("P4_2_W5500_SPI_HZ_SOURCE", SPI_HZ)
    emit("P4_2_EXPLICIT_VOLATILE_WORD_COPIES", 16)
    emit("P4_2_MMIO_MEMCPY", "NO")
    emit("P4_2_TAIL_1_63_PATH", "PRESERVED")


def variant_value(variant: str) -> str:
    if variant == "BASELINE":
        return "0"
    if variant == "COPY_OUT_64":
        return "1"
    raise RuntimeError(f"P4_2_UNKNOWN_VARIANT={variant}")


def extra_flags_for(
    *,
    variant: str,
    verify: bool,
    chunk_profile: bool,
) -> tuple[str, ...]:
    ethernet_profile = "0" if (verify or chunk_profile) else "1"
    verify_flag = "1" if verify else "0"
    chunk_profile_flag = "1" if chunk_profile else "0"

    return (
        f"-DJWPLC_ETHERNET_ENABLE_PROFILE_HOOKS={ethernet_profile}",
        "-DJWPLC_W5500_RX_DIRECT_TRANSFER_BYTES=0",
        "-DJWPLC_W5500_RX_FIFO_REUSE=1",
        f"-DJWPLC_H4A04P3_VERIFY_PAYLOAD={verify_flag}",
        f"-DJWPLC_SPI_PROFILE_FIFO_REUSE_CHUNKS={chunk_profile_flag}",
        (
            "-DJWPLC_SPI_FIFO_REUSE_COPY_OUT_64="
            f"{variant_value(variant)}"
        ),
    )


def validate_variant_flag_contract() -> None:
    for verify, chunk_profile in (
        (True, False),
        (False, True),
        (False, False),
    ):
        baseline = set(
            extra_flags_for(
                variant="BASELINE",
                verify=verify,
                chunk_profile=chunk_profile,
            )
        )
        candidate = set(
            extra_flags_for(
                variant="COPY_OUT_64",
                verify=verify,
                chunk_profile=chunk_profile,
            )
        )
        expected_difference = {
            "-DJWPLC_SPI_FIFO_REUSE_COPY_OUT_64=0",
            "-DJWPLC_SPI_FIFO_REUSE_COPY_OUT_64=1",
        }
        if baseline ^ candidate != expected_difference:
            raise RuntimeError("P4_2_VARIANT_FLAG_CONTRACT_MISMATCH")

        combined = baseline | candidate
        if any(
            flag.startswith("-DJWPLC_SPI_FIFO_REUSE_DLEN_CACHE=")
            for flag in combined
        ):
            raise RuntimeError("P4_2_DLEN_REUSE_OVERRIDE_FORBIDDEN")

    emit("P4_2_ONLY_VARIANT_FLAG", "JWPLC_SPI_FIFO_REUSE_COPY_OUT_64")
    emit("P4_2_BASELINE_FLAG", "JWPLC_SPI_FIFO_REUSE_COPY_OUT_64=0")
    emit("P4_2_CANDIDATE_FLAG", "JWPLC_SPI_FIFO_REUSE_COPY_OUT_64=1")
    emit("P4_2_DLEN_REUSE_BUILD_OVERRIDE", "NO")


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
    flags = extra_flags_for(
        variant=variant,
        verify=verify,
        chunk_profile=chunk_profile,
    )
    extra_flags = " ".join(flags)

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
    emit(f"P4_2_{label}_COMPILE_EXIT", exit_code)

    if exit_code != 0:
        print(p3.decode(log.read_bytes())[-9000:])
        raise RuntimeError(f"P4_2_{label}_COMPILE_FAILED")

    normalized = p3.decode(log.read_bytes()).replace("\\", "/").lower()

    for flag in flags:
        token = flag.lower()
        ok = token in normalized
        emit(
            f"P4_2_{label}_TOKEN_{token.replace('=', '_')}",
            "PASS" if ok else "FAIL",
        )
        if not ok:
            raise RuntimeError(f"P4_2_{label}_TOKEN_MISSING={token}")

    required_sources = (
        "w5100.cpp",
        "socket.cpp",
        "spi.cpp",
        "jwplc_idlescreen.cpp",
        "jwplc_tft.cpp",
    )
    for token in required_sources:
        if token not in normalized:
            raise RuntimeError(f"P4_2_{label}_SOURCE_MISSING={token}")

    if "-djwplc_spi_fifo_reuse_dlen_cache=" in normalized:
        raise RuntimeError(f"P4_2_{label}_DLEN_OVERRIDE_DETECTED")

    for archive in (
        "libjwplc_display.a",
        "libjwplc_tft.a",
        "libspi.a",
    ):
        if archive in normalized:
            raise RuntimeError(f"P4_2_{label}_UNEXPECTED_ARCHIVE={archive}")


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
    emit(f"P4_2_{label}_UPLOAD_EXIT", exit_code)
    if exit_code != 0:
        print(p3.decode(log.read_bytes())[-7000:])
        raise RuntimeError(f"P4_2_{label}_UPLOAD_FAILED")


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
    emit(f"P4_2_{label}_CASE_EXIT", exit_code)

    text = p3.decode(log.read_bytes())
    if exit_code not in (0, 2):
        print(text[-9000:])
        raise RuntimeError(f"P4_2_{label}_RUNTIME_FAILED")

    transport_errors = p3.integer(text, "H4A04P1_TRANSPORT_ERRORS")
    lock_errors = p3.integer(text, "H4A04P1_TCP_SPI_LOCK_ERRORS")
    functional = p3.one(text, "H4A04P1_FUNCTIONAL_PASS")

    if (
        exit_code != 0
        or functional != "YES"
        or transport_errors != 0
        or lock_errors != 0
    ):
        raise IntegrityFailure(
            f"P4_2_{label}_INTEGRITY_FAIL "
            f"FUNCTIONAL={functional} "
            f"TRANSPORT_ERRORS={transport_errors} "
            f"TCP_SPI_LOCK_ERRORS={lock_errors}"
        )

    return text


def inspect_micro(text: str) -> dict[str, float]:
    if p3.one(text, "H4A04P1_SPI_CHUNK_PROFILE_ENABLED") != "YES":
        raise RuntimeError("P4_2_MICRO_PROFILE_NOT_ACTIVE")

    count = p3.integer(text, "H4A04P1_SPI_CHUNK_COUNT")
    byte_count = p3.integer(text, "H4A04P1_SPI_CHUNK_BYTES")
    copy_us = p3.integer(text, "H4A04P1_SPI_CHUNK_COPY_OUT_TOTAL_US")

    if count <= 0 or byte_count <= 0 or copy_us <= 0:
        raise RuntimeError("P4_2_MICRO_EMPTY")

    equivalent_64b_chunks = byte_count / 64.0

    return {
        "count": float(count),
        "bytes": float(byte_count),
        "copy_us": float(copy_us),
        "equivalent_64b_chunks": equivalent_64b_chunks,
        "copy_us_per_64b_chunk": copy_us / equivalent_64b_chunks,
    }


def main() -> int:
    parser = argparse.ArgumentParser(
        description="A14 P4.2 W5500 FIFO COPY_OUT 64 B A/B"
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
    print(" A14 P4.2 - W5500 FIFO COPY_OUT 64 B A/B")
    print("=" * 78)

    branch = p3.git(repo, "branch", "--show-current")
    head = p3.git(repo, "rev-parse", "HEAD")

    emit("P4_2_BRANCH", branch)
    emit("P4_2_HEAD", head)
    emit("P4_2_SERIAL", args.serial)
    emit("P4_2_SPI_HZ", SPI_HZ)
    emit("P4_2_FIFO_REUSE", "ON_DEFAULT_BOTH_VARIANTS")
    emit("P4_2_DLEN_REUSE", "ON_DEFAULT_BOTH_VARIANTS")
    emit("P4_2_DIRECT_RX", "OFF_BOTH_VARIANTS")
    emit("P4_2_RX_COMMIT", "IMMEDIATE_BOTH_VARIANTS")
    emit("P4_2_LEGACY_APIS", "SAME_BOTH_VARIANTS")
    emit("P4_2_BASELINE", "DYNAMIC_WORD_LOOP")
    emit("P4_2_CANDIDATE", "EXPLICIT_16_WORD_COPY_FOR_64B")
    emit("P4_2_CANDIDATE_DEFAULT", "OFF")
    emit("P4_2_ONLY_VARIABLE", "COPY_OUT_64_POLICY")
    emit("P4_2_VERIFY_DURATION_S", VERIFY_DURATION_S)
    emit("P4_2_MICRO_DURATION_S", MICRO_DURATION_S)
    emit("P4_2_PERF_DURATION_S", PERF_DURATION_S)
    emit("P4_2_RUNS_PER_VARIANT", runs_per_variant)
    emit("P4_2_ORDER", ",".join(order))
    emit("P4_2_TCP_CHUNK", TCP_CHUNK)
    emit("P4_2_ETHERNET_STIMULUS", "H4A04P1_TCP_RX_CASE")
    emit("P4_2_PAYLOAD_PATTERN", "4096B_INCREMENTING_00_FF")
    emit("P4_2_HARDWARE", "UNCHANGED_FROM_P4_1")
    emit("P4_2_TOPOLOGY", "UNCHANGED_FROM_P4_1")

    if branch != BRANCH:
        raise RuntimeError("P4_2_BRANCH_MISMATCH")

    if p3.git(repo, "diff", "--name-only") or p3.git(
        repo,
        "diff",
        "--cached",
        "--name-only",
    ):
        raise RuntimeError("P4_2_TREE_NOT_CLEAN")

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
    emit("P4_2_UNTRACKED_PRODUCT_COUNT", len(untracked_product))
    if untracked_product:
        raise RuntimeError("P4_2_UNTRACKED_PRODUCT_SOURCE")

    audit_source_contract(repo)
    validate_variant_flag_contract()

    ast.parse(
        runner.read_text(encoding="utf-8"),
        filename=str(runner),
    )
    emit("P4_2_RUNNER_AST", "PASS")

    contract_run = p3.run(
        [sys.executable, "-B", str(contract)],
        repo,
    )
    print(p3.decode(contract_run.stdout))
    if contract_run.returncode != 0:
        raise RuntimeError("P4_2_PACKAGE_CONTRACT_FAILED")

    cli = p3.find_cli(args.arduino_cli)
    emit("ARDUINO_CLI", cli)

    result_root = Path(
        tempfile.mkdtemp(prefix="jwplc_a14_p4_2_copy_out_64_")
    )
    emit("P4_2_RESULT_ROOT", result_root)

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
        variant="COPY_OUT_64",
        log=result_root / "compile_verify_candidate.log",
        verify=True,
    )

    for variant, key in (
        ("BASELINE", "micro_baseline"),
        ("COPY_OUT_64", "micro_candidate"),
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
        ("COPY_OUT_64", "perf_candidate"),
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
    print(" P4.2 PAYLOAD INTEGRITY - CANDIDATE")
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
        raise RuntimeError("P4_2_VERIFY_NOT_ACTIVE")

    verify_bytes = p3.integer(verify_text, "H4A04P1_DUT_RX_BYTES")
    verify_actual = p3.integer(verify_text, "H4A04P1_RX_FNV1A32")
    verify_expected = p3.fnv1a_expected(verify_bytes)

    emit("P4_2_VERIFY_RX_BYTES", verify_bytes)
    emit("P4_2_FNV_ACTUAL", verify_actual)
    emit("P4_2_FNV_EXPECTED", verify_expected)

    if verify_actual != verify_expected:
        raise IntegrityFailure("P4_2_CANDIDATE_FNV_MISMATCH")

    emit("P4_2_PAYLOAD_INTEGRITY", "PASS")

    print()
    print("=" * 78)
    print(" P4.2 MICROPROFILE BASELINE / CANDIDATE")
    print("=" * 78)

    micro: dict[str, dict[str, float]] = {}

    for variant, key in (
        ("BASELINE", "micro_baseline"),
        ("COPY_OUT_64", "micro_candidate"),
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
            "P4_2_MICRO_RESULT",
            (
                f"VARIANT={variant} "
                f"CHUNKS={int(row['count'])} "
                f"BYTES={int(row['bytes'])} "
                f"COPY_OUT_TOTAL_US={int(row['copy_us'])} "
                "COPY_OUT_US_PER_64B_CHUNK="
                f"{row['copy_us_per_64b_chunk']:.6f}"
            ),
        )

    copy_out_delta = delta_pct(
        micro["COPY_OUT_64"]["copy_us_per_64b_chunk"],
        micro["BASELINE"]["copy_us_per_64b_chunk"],
    )

    emit(
        "P4_2_COPY_OUT_US_PER_64B_CHUNK_BASELINE",
        f"{micro['BASELINE']['copy_us_per_64b_chunk']:.6f}",
    )
    emit(
        "P4_2_COPY_OUT_US_PER_64B_CHUNK_CANDIDATE",
        f"{micro['COPY_OUT_64']['copy_us_per_64b_chunk']:.6f}",
    )
    emit("P4_2_COPY_OUT_64B_DELTA_PCT", f"{copy_out_delta:.3f}")
    emit(
        "P4_2_COPY_OUT_64B_NORMALIZATION",
        "COPY_OUT_TOTAL_US_DIVIDED_BY_BYTES_OVER_64",
    )

    print()
    print("=" * 78)
    print(" P4.2 UNPROFILED PERFORMANCE A/B")
    print("=" * 78)

    buckets: dict[str, list[dict[str, float]]] = {
        "BASELINE": [],
        "COPY_OUT_64": [],
    }
    counts = {"BASELINE": 0, "COPY_OUT_64": 0}

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
            f"--- P4.2 CASE {case_index}/{len(order)} "
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
            "P4_2_RUN_RESULT",
            (
                f"VARIANT={variant} RUN={run_no} "
                f"TCP={row['mid']:.6f}Mbps "
                f"PAYLOAD={row['payload_mbps']:.3f}Mbps "
                f"US_PER_BYTE={row['us_per_byte']:.6f}"
            ),
        )

    summary: dict[str, dict[str, float]] = {}
    for variant in ("BASELINE", "COPY_OUT_64"):
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
            "us_per_byte_spread": spread_pct(cost),
            "valid_runs": float(len(rows)),
        }

    base = summary["BASELINE"]
    candidate = summary["COPY_OUT_64"]

    payload_delta = delta_pct(candidate["payload"], base["payload"])
    tcp_delta = delta_pct(candidate["tcp"], base["tcp"])
    cost_delta = delta_pct(
        candidate["us_per_byte"],
        base["us_per_byte"],
    )

    valid_runs_ok = (
        int(base["valid_runs"]) == runs_per_variant
        and int(candidate["valid_runs"]) == runs_per_variant
    )
    repeatability_ok = (
        valid_runs_ok
        and base["payload_spread"] <= PAYLOAD_SPREAD_MAX_PCT
        and candidate["payload_spread"] <= PAYLOAD_SPREAD_MAX_PCT
    )

    mechanism_confirmed = (
        copy_out_delta <= -COPY_OUT_REDUCTION_MIN_PCT
    )
    regression = (
        payload_delta <= -PERF_REGRESSION_LIMIT_PCT
        or cost_delta >= PERF_REGRESSION_LIMIT_PCT
    )
    system_gain = (
        payload_delta >= PERF_GAIN_MIN_PCT
        and cost_delta <= -PERF_GAIN_MIN_PCT
    )

    if regression:
        interpretation = "P4_2_REGRESSION"
    elif not repeatability_ok:
        interpretation = "P4_2_REPEATABILITY_INSUFFICIENT"
    elif mechanism_confirmed and system_gain:
        interpretation = "P4_2_GAIN_CONFIRMED"
    elif (
        abs(payload_delta) <= NO_MATERIAL_BAND_PCT
        and abs(cost_delta) <= NO_MATERIAL_BAND_PCT
    ):
        interpretation = "P4_2_NO_MATERIAL_SYSTEM_GAIN"
    else:
        interpretation = "P4_2_SMALL_OR_INCONCLUSIVE_EFFECT"

    print()
    print("=" * 78)
    print(" P4.2 SUMMARY")
    print("=" * 78)

    for variant in ("BASELINE", "COPY_OUT_64"):
        s = summary[variant]
        report_label = (
            "BASELINE"
            if variant == "BASELINE"
            else "CANDIDATE"
        )
        emit(
            f"P4_2_{report_label}_PAYLOAD_MBPS",
            f"{s['payload']:.3f}",
        )
        emit(
            f"P4_2_{report_label}_PAYLOAD_SPREAD_PCT",
            f"{s['payload_spread']:.3f}",
        )
        emit(
            f"P4_2_{report_label}_TCP_MBPS",
            f"{s['tcp']:.6f}",
        )
        emit(
            f"P4_2_{report_label}_TCP_SPREAD_PCT",
            f"{s['tcp_spread']:.3f}",
        )
        emit(
            f"P4_2_{report_label}_US_PER_BYTE",
            f"{s['us_per_byte']:.6f}",
        )
        emit(
            f"P4_2_{report_label}_US_PER_BYTE_SPREAD_PCT",
            f"{s['us_per_byte_spread']:.3f}",
        )
        emit(
            f"P4_2_{report_label}_VALID_RUNS",
            int(s["valid_runs"]),
        )

    emit("P4_2_PAYLOAD_DELTA_PCT", f"{payload_delta:.3f}")
    emit("P4_2_TCP_DELTA_PCT", f"{tcp_delta:.3f}")
    emit("P4_2_US_PER_BYTE_DELTA_PCT", f"{cost_delta:.3f}")
    emit("P4_2_TRANSPORT_ERRORS", 0)
    emit("P4_2_TCP_SPI_LOCK_ERRORS", 0)
    # Every valid case ends with a freeze ACK and a final serial snapshot.
    # A reboot during the measured window invalidates that case instead of
    # being silently counted as a successful zero-reset run.
    emit("P4_2_UNEXPECTED_RESETS", 0)
    emit(
        "P4_2_UNEXPECTED_RESET_DETECTION",
        "FINAL_FREEZE_ACK_AND_SNAPSHOT_PER_VALID_CASE",
    )
    emit("P4_2_REPEATABILITY_OK", "YES" if repeatability_ok else "NO")
    emit(
        "P4_2_MECHANISM_CONFIRMED",
        "YES" if mechanism_confirmed else "NO",
    )
    emit("P4_2_INTERPRETATION", interpretation)

    if args.defer_physical_review:
        physical = "PENDING_USER"
        gate_status = "PASS_DATA_ONLY"
    else:
        answer = input(
            "¿TFT/periféricos permanecieron estables durante P4.2? (S/N): "
        ).strip().upper()
        if answer != "S":
            raise IntegrityFailure("P4_2_PHYSICAL_STABILITY_FAILED")
        physical = "PASS"
        gate_status = "PASS"

    emit("P4_2_PHYSICAL_STABILITY", physical)

    if p3.git(repo, "diff", "--name-only") or p3.git(
        repo,
        "diff",
        "--cached",
        "--name-only",
    ):
        raise RuntimeError("P4_2_REPOSITORY_MUTATED_DURING_GATE")

    next_action = {
        "P4_2_GAIN_CONFIRMED":
            "RETURN_TO_CHAT_PROMOTION_DECISION",
        "P4_2_NO_MATERIAL_SYSTEM_GAIN":
            "RETURN_TO_CHAT_REJECT_OR_CONFIRM",
        "P4_2_SMALL_OR_INCONCLUSIVE_EFFECT":
            "RETURN_TO_CHAT_DECIDE_CONFIRMATION",
        "P4_2_REPEATABILITY_INSUFFICIENT":
            "RETURN_TO_CHAT_DECIDE_CONFIRMATION",
        "P4_2_REGRESSION":
            "RETURN_TO_CHAT_REJECT",
    }[interpretation]

    summary_log = result_root / "SUMMARY.log"
    summary_log.write_text(
        "\n".join(
            [
                f"A14_P4_2_COPY_OUT_64={gate_status}",
                f"HEAD={head}",
                "ONLY_VARIABLE=COPY_OUT_64_POLICY",
                "BASELINE=JWPLC_SPI_FIFO_REUSE_COPY_OUT_64=0",
                "CANDIDATE=JWPLC_SPI_FIFO_REUSE_COPY_OUT_64=1",
                "CANDIDATE_DEFAULT=OFF",
                "FIFO_REUSE=ON_DEFAULT_BOTH_VARIANTS",
                "DLEN_REUSE=ON_DEFAULT_BOTH_VARIANTS",
                "DIRECT_RX=OFF_BOTH_VARIANTS",
                "RX_COMMIT=IMMEDIATE_BOTH_VARIANTS",
                f"VERIFY_RX_BYTES={verify_bytes}",
                f"FNV_ACTUAL={verify_actual}",
                f"FNV_EXPECTED={verify_expected}",
                "PAYLOAD_INTEGRITY=PASS",
                "TRANSPORT_ERRORS=0",
                "TCP_SPI_LOCK_ERRORS=0",
                "UNEXPECTED_RESETS=0",
                (
                    "COPY_OUT_US_PER_64B_CHUNK_BASELINE="
                    f"{micro['BASELINE']['copy_us_per_64b_chunk']:.6f}"
                ),
                (
                    "COPY_OUT_US_PER_64B_CHUNK_CANDIDATE="
                    f"{micro['COPY_OUT_64']['copy_us_per_64b_chunk']:.6f}"
                ),
                f"COPY_OUT_64B_DELTA_PCT={copy_out_delta:.3f}",
                f"BASELINE_PAYLOAD_MBPS={base['payload']:.3f}",
                f"CANDIDATE_PAYLOAD_MBPS={candidate['payload']:.3f}",
                f"PAYLOAD_DELTA_PCT={payload_delta:.3f}",
                f"BASELINE_TCP_MBPS={base['tcp']:.6f}",
                f"CANDIDATE_TCP_MBPS={candidate['tcp']:.6f}",
                f"TCP_DELTA_PCT={tcp_delta:.3f}",
                f"BASELINE_US_PER_BYTE={base['us_per_byte']:.6f}",
                f"CANDIDATE_US_PER_BYTE={candidate['us_per_byte']:.6f}",
                f"US_PER_BYTE_DELTA_PCT={cost_delta:.3f}",
                (
                    "BASELINE_PAYLOAD_SPREAD_PCT="
                    f"{base['payload_spread']:.3f}"
                ),
                (
                    "CANDIDATE_PAYLOAD_SPREAD_PCT="
                    f"{candidate['payload_spread']:.3f}"
                ),
                f"BASELINE_VALID_RUNS={int(base['valid_runs'])}",
                f"CANDIDATE_VALID_RUNS={int(candidate['valid_runs'])}",
                f"REPEATABILITY_OK={'YES' if repeatability_ok else 'NO'}",
                (
                    "MECHANISM_CONFIRMED="
                    f"{'YES' if mechanism_confirmed else 'NO'}"
                ),
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

    emit("P4_2_SUMMARY_LOG", summary_log)
    emit("HARNESS_FAILURE", "NO")
    emit("PRODUCT_FAILURE", "NO_EVIDENCE")
    emit("HARDWARE_FAILURE", "NO_EVIDENCE")
    emit("A14_P4_2_COPY_OUT_64", gate_status)
    emit("NEXT", next_action)
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except SystemExit:
        raise
    except IntegrityFailure as exc:
        emit("P4_2_INTERPRETATION", "P4_2_INTEGRITY_FAIL")
        emit("HARNESS_FAILURE", "NO_EVIDENCE")
        emit("PRODUCT_FAILURE", "POSSIBLE")
        emit("HARDWARE_FAILURE", "POSSIBLE")
        emit("P4_2_EXCEPTION", str(exc))
        raise SystemExit(2)
    except Exception as exc:
        emit("HARNESS_FAILURE", "UNCLASSIFIED")
        emit("PRODUCT_FAILURE", "UNCLASSIFIED")
        emit("HARDWARE_FAILURE", "UNCLASSIFIED")
        emit("P4_2_FAILURE_REQUIRES_CLASSIFICATION", "YES")
        emit("P4_2_EXCEPTION", str(exc))
        raise SystemExit(1)
