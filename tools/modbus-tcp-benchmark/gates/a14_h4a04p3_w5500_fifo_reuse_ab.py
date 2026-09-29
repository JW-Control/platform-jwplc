#!/usr/bin/env python3
from __future__ import annotations

import argparse
import ast
import re
import shutil
import statistics
import subprocess
import sys
import tempfile
import time
from pathlib import Path

BRANCH = "v2.1.0-alpha.14/feature/modbus-tcp"
VERIFY_DURATION_S = 1.0
PERF_DURATION_S = 15.0
TCP_CHUNK = 4096
ORDER = (
    "DIRECT_RX",
    "FIFO_REUSE",
    "FIFO_REUSE",
    "DIRECT_RX",
    "DIRECT_RX",
    "FIFO_REUSE",
)
RUNS_PER_VARIANT = 3
GAIN_THRESHOLD_PCT = 1.0
NO_MATERIAL_BAND_PCT = 0.5
PAYLOAD_SPREAD_MAX_PCT = 0.5


def emit(key: str, value: object) -> None:
    print(f"{key}={value}")


def decode(data: bytes) -> str:
    for enc in ("utf-8-sig", "utf-8", "cp1252"):
        try:
            return data.decode(enc)
        except UnicodeDecodeError:
            pass
    return data.decode("utf-8", errors="replace")


def run(cmd: list[str], cwd: Path | None = None) -> subprocess.CompletedProcess[bytes]:
    return subprocess.run(
        cmd,
        cwd=str(cwd) if cwd else None,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
    )


def run_logged(cmd: list[str], log: Path, cwd: Path | None = None) -> int:
    proc = run(cmd, cwd)
    log.write_bytes(proc.stdout)
    return proc.returncode


def git(repo: Path, *args: str) -> str:
    proc = run(["git", "-C", str(repo), *args])
    if proc.returncode != 0:
        raise RuntimeError(
            f"GIT_FAILED {' '.join(args)} :: {decode(proc.stdout)[-1600:]}"
        )
    return decode(proc.stdout).strip()


def find_cli(explicit: str | None) -> Path:
    candidates: list[Path] = []
    if explicit:
        candidates.append(Path(explicit))

    which = shutil.which("arduino-cli")
    if which:
        candidates.append(Path(which))

    candidates.append(
        Path(r"C:\Program Files\Arduino PLC IDE Tools\arduino-cli.exe")
    )

    for candidate in candidates:
        if candidate.is_file():
            return candidate.resolve()

    raise RuntimeError("H4A04P3_ARDUINO_CLI_NOT_FOUND")


def values(text: str, key: str) -> list[str]:
    return [
        match.strip()
        for match in re.findall(
            rf"(?m)^{re.escape(key)}=(.*)\r?$",
            text,
        )
    ]


def one(text: str, key: str) -> str:
    found = values(text, key)
    if len(found) != 1:
        raise RuntimeError(
            f"H4A04P3_KEY_COUNT_{key}={len(found)}"
        )
    return found[0]


def number(text: str, key: str) -> float:
    return float(one(text, key))


def integer(text: str, key: str) -> int:
    return int(one(text, key))


def median(items: list[float]) -> float:
    if not items:
        raise RuntimeError("H4A04P3_EMPTY_MEDIAN")
    return float(statistics.median(items))


def spread_pct(items: list[float]) -> float:
    med = median(items)
    if med <= 0.0:
        raise RuntimeError("H4A04P3_NONPOSITIVE_MEDIAN")
    return (max(items) - min(items)) * 100.0 / med


def delta_pct(value: float, reference: float) -> float:
    if reference <= 0.0:
        raise RuntimeError("H4A04P3_NONPOSITIVE_REFERENCE")
    return (value / reference - 1.0) * 100.0


def effective_payload_mbps(payload_bytes: float, payload_us: float) -> float:
    if payload_us <= 0.0:
        raise RuntimeError("H4A04P3_NONPOSITIVE_PAYLOAD_TIME")
    return payload_bytes * 8.0 / payload_us


def fnv1a_expected(byte_count: int) -> int:
    value = 2166136261
    for i in range(byte_count):
        value ^= i & 0xFF
        value = (value * 16777619) & 0xFFFFFFFF
    return value


def inspect_profile_case(text: str) -> dict[str, float]:
    if one(text, "H4A04P1_FUNCTIONAL_PASS") != "YES":
        raise RuntimeError("H4A04P3_FUNCTIONAL_FAIL")

    if one(text, "TCP_PROFILE_ENABLED") != "YES":
        raise RuntimeError("H4A04P3_PROFILE_NOT_ENABLED")

    if one(text, "H4A04P1_PAYLOAD_VERIFY_ENABLED") != "NO":
        raise RuntimeError("H4A04P3_VERIFY_UNEXPECTED_IN_PERF")

    for key in (
        "H4A04P1_TRANSPORT_ERRORS",
        "H4A04P1_TCP_SPI_LOCK_ERRORS",
    ):
        if number(text, key) != 0.0:
            raise RuntimeError(
                f"H4A04P3_{key}_NONZERO"
            )

    row = {
        "mid": number(text, "H4A04P1_DUT_MBPS_MID"),
        "lower": number(text, "H4A04P1_DUT_MBPS_LOWER"),
        "upper": number(text, "H4A04P1_DUT_MBPS_UPPER"),
        "payload_us": number(
            text,
            "H4A04P1_TCP_PROF_PAYLOAD_READ_TOTAL_US",
        ),
        "payload_calls": number(
            text,
            "H4A04P1_TCP_PROF_PAYLOAD_READ_CALLS",
        ),
        "payload_bytes": number(
            text,
            "H4A04P1_TCP_PROF_PAYLOAD_BYTES",
        ),
        "hold_us": number(
            text,
            "H4A04P1_TCP_SPI_HOLD_TOTAL_US",
        ),
        "recv_us": number(
            text,
            "H4A04P1_TCP_PROF_RECV_TOTAL_US",
        ),
        "status_us": number(
            text,
            "H4A04P1_TCP_PROF_SOCKET_STATUS_TOTAL_US",
        ),
        "available_us": number(
            text,
            "H4A04P1_TCP_PROF_AVAILABLE_TOTAL_US",
        ),
        "commit_us": number(
            text,
            "H4A04P1_TCP_PROF_COMMIT_TOTAL_US",
        ),
    }

    if row["payload_calls"] <= 0.0 or row["payload_bytes"] <= 0.0:
        raise RuntimeError("H4A04P3_EMPTY_PAYLOAD_PROFILE")

    if not (0.0 < row["lower"] <= row["mid"] <= row["upper"]):
        raise RuntimeError("H4A04P3_BOUNDS_INVALID")

    row["payload_mbps"] = effective_payload_mbps(
        row["payload_bytes"],
        row["payload_us"],
    )
    row["us_per_byte"] = row["payload_us"] / row["payload_bytes"]
    row["avg_read_us"] = row["payload_us"] / row["payload_calls"]
    row["avg_read_bytes"] = row["payload_bytes"] / row["payload_calls"]

    return row


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
    if variant == "DIRECT_RX":
        direct = "1"
        fifo = "0"
    elif variant == "FIFO_REUSE":
        direct = "0"
        fifo = "1"
    else:
        raise RuntimeError(
            f"H4A04P3_UNKNOWN_VARIANT={variant}"
        )

    profile = "0" if verify else "1"
    verify_flag = "1" if verify else "0"

    extra_flags = " ".join(
        (
            f"-DJWPLC_ETHERNET_ENABLE_PROFILE_HOOKS={profile}",
            f"-DJWPLC_W5500_RX_DIRECT_TRANSFER_BYTES={direct}",
            f"-DJWPLC_W5500_RX_FIFO_REUSE={fifo}",
            f"-DJWPLC_H4A04P3_VERIFY_PAYLOAD={verify_flag}",
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

    exit_code = run_logged(cmd, log, repo)
    label = (
        f"{variant}_VERIFY"
        if verify
        else variant
    )
    emit(f"H4A04P3_{label}_COMPILE_EXIT", exit_code)

    if exit_code != 0:
        print(decode(log.read_bytes())[-8000:])
        raise RuntimeError(
            f"H4A04P3_{label}_COMPILE_FAILED"
        )

    normalized = decode(log.read_bytes()).replace("\\", "/").lower()

    required_tokens = (
        f"-djwplc_ethernet_enable_profile_hooks={profile}",
        f"-djwplc_w5500_rx_direct_transfer_bytes={direct}",
        f"-djwplc_w5500_rx_fifo_reuse={fifo}",
        f"-djwplc_h4a04p3_verify_payload={verify_flag}",
        "w5100.cpp",
        "socket.cpp",
        "spi.cpp",
        "jwplc_idlescreen.cpp",
        "jwplc_tft.cpp",
    )

    for token in required_tokens:
        ok = token in normalized
        emit(
            f"H4A04P3_{label}_TOKEN_{token.replace('=', '_')}",
            "PASS" if ok else "FAIL",
        )
        if not ok:
            raise RuntimeError(
                f"H4A04P3_{label}_TOKEN_MISSING={token}"
            )

    if "libjwplc_display.a" in normalized:
        raise RuntimeError(
            f"H4A04P3_{label}_DISPLAY_A_UNEXPECTED"
        )

    if "libjwplc_tft.a" in normalized:
        raise RuntimeError(
            f"H4A04P3_{label}_TFT_A_UNEXPECTED"
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
    exit_code = run_logged(
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
    emit(f"H4A04P3_{label}_UPLOAD_EXIT", exit_code)

    if exit_code != 0:
        print(decode(log.read_bytes())[-5000:])
        raise RuntimeError(
            f"H4A04P3_{label}_UPLOAD_FAILED"
        )


def run_case(
    *,
    runner: Path,
    repo: Path,
    serial_port: str,
    duration_s: float,
    variant_arg: str,
    log: Path,
) -> str:
    exit_code = run_logged(
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
    emit("H4A04P3_CASE_EXIT", exit_code)

    text = decode(log.read_bytes())

    if exit_code != 0:
        print(text[-7000:])
        raise RuntimeError(
            "H4A04P3_RUNTIME_CASE_FAILED"
        )

    return text


def main() -> int:
    parser = argparse.ArgumentParser(
        description="A14 H4A0.4-P3 W5500 FIFO-reuse A/B"
    )
    parser.add_argument("--serial", default="COM14")
    parser.add_argument("--arduino-cli")
    parser.add_argument(
        "--fqbn",
        default="jwplc_local:esp32:jwplcbasic",
    )
    parser.add_argument(
        "--defer-physical-review",
        action="store_true",
        help=(
            "No solicita confirmación visual inmediata; registra la "
            "revisión física como pendiente del usuario."
        ),
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
    print(" A14 H4A0.4-P3 - W5500 FIFO REUSE TCP RX A/B")
    print("=" * 78)

    branch = git(repo, "branch", "--show-current")
    head = git(repo, "rev-parse", "HEAD")

    emit("BRANCH", branch)
    emit("HEAD", head)
    emit("SERIAL_PORT", args.serial)
    emit("H4A04P3_VERIFY_DURATION_S", VERIFY_DURATION_S)
    emit("H4A04P3_PERF_DURATION_S", PERF_DURATION_S)
    emit("H4A04P3_TCP_CHUNK", TCP_CHUNK)
    emit("H4A04P3_ORDER", ",".join(ORDER))
    emit("H4A04P3_RUNS_PER_VARIANT", RUNS_PER_VARIANT)
    emit("H4A04P3_SPI_HZ", 26000000)
    emit("H4A04P3_PRODUCT_SOURCE", "JWPLC/2.1.0_CANONICAL_PACKAGE")
    emit("H4A04P3_TEMP_PRODUCT_PATCHES", "NO")
    emit(
        "H4A04P3_ONLY_VARIABLE",
        "DUMMY_FIFO_REFILL_PER_64B_RX_CHUNK",
    )
    emit("H4A04P3_BASELINE", "DIRECT_RX")
    emit("H4A04P3_CANDIDATE", "FIFO_REUSE")
    emit("H4A04P3_DEFAULT_FIFO_REUSE", "OFF")
    emit("H4A04P3_GAIN_THRESHOLD_PCT", GAIN_THRESHOLD_PCT)
    emit("H4A04P3_NO_MATERIAL_BAND_PCT", NO_MATERIAL_BAND_PCT)
    emit("H4A04P3_PAYLOAD_SPREAD_MAX_PCT", PAYLOAD_SPREAD_MAX_PCT)

    if branch != BRANCH:
        raise RuntimeError("H4A04P3_BRANCH_MISMATCH")

    if git(repo, "diff", "--name-only") or git(
        repo,
        "diff",
        "--cached",
        "--name-only",
    ):
        raise RuntimeError("H4A04P3_TRACKED_TREE_NOT_CLEAN")

    untracked_product = [
        line
        for line in git(
            repo,
            "ls-files",
            "--others",
            "--exclude-standard",
        ).splitlines()
        if line.startswith("JWPLC/2.1.0/")
    ]
    emit(
        "H4A04P3_UNTRACKED_PRODUCT_COUNT",
        len(untracked_product),
    )
    if untracked_product:
        raise RuntimeError(
            "H4A04P3_UNTRACKED_PRODUCT_SOURCE"
        )

    ast.parse(
        runner.read_text(encoding="utf-8"),
        filename=str(runner),
    )
    emit("H4A04P3_RUNNER_AST", "PASS")

    contract_run = run(
        [sys.executable, "-B", str(contract)],
        repo,
    )
    print(decode(contract_run.stdout))
    if contract_run.returncode != 0:
        raise RuntimeError(
            "H4A04P3_PACKAGE_CONTRACT_FAILED"
        )

    cli = find_cli(args.arduino_cli)
    emit("ARDUINO_CLI", cli)

    result_root = Path(
        tempfile.mkdtemp(
            prefix="jwplc_a14_h4a04p3_fifo_reuse_"
        )
    )
    emit("H4A04P3_RESULT_ROOT", result_root)

    verify_build = result_root / "build_verify_fifo"
    perf_builds = {
        "DIRECT_RX": result_root / "build_direct_rx",
        "FIFO_REUSE": result_root / "build_fifo_reuse",
    }

    compile_variant(
        cli=cli,
        fqbn=args.fqbn,
        repo=repo,
        sketch=sketch,
        libraries=libraries,
        build_dir=verify_build,
        variant="FIFO_REUSE",
        log=result_root / "compile_verify_fifo.log",
        verify=True,
    )

    for variant in ("DIRECT_RX", "FIFO_REUSE"):
        compile_variant(
            cli=cli,
            fqbn=args.fqbn,
            repo=repo,
            sketch=sketch,
            libraries=libraries,
            build_dir=perf_builds[variant],
            variant=variant,
            log=result_root
            / f"compile_{variant.lower()}.log",
            verify=False,
        )

    print()
    print("=" * 78)
    print(" H4A0.4-P3 PAYLOAD INTEGRITY - FIFO_REUSE")
    print("=" * 78)

    upload(
        cli=cli,
        fqbn=args.fqbn,
        serial_port=args.serial,
        build_dir=verify_build,
        sketch=sketch,
        repo=repo,
        log=result_root / "verify_fifo_upload.log",
        label="VERIFY_FIFO",
    )
    time.sleep(3.0)

    verify_text = run_case(
        runner=runner,
        repo=repo,
        serial_port=args.serial,
        duration_s=VERIFY_DURATION_S,
        variant_arg="BASE",
        log=result_root / "verify_fifo_case.log",
    )

    if one(
        verify_text,
        "H4A04P1_PAYLOAD_VERIFY_ENABLED",
    ) != "YES":
        raise RuntimeError(
            "H4A04P3_VERIFY_FLAG_NOT_ACTIVE"
        )

    verify_bytes = integer(
        verify_text,
        "H4A04P1_DUT_RX_BYTES",
    )
    actual_hash = integer(
        verify_text,
        "H4A04P1_RX_FNV1A32",
    )
    expected_hash = fnv1a_expected(verify_bytes)

    emit("H4A04P3_VERIFY_RX_BYTES", verify_bytes)
    emit("H4A04P3_VERIFY_FNV_ACTUAL", actual_hash)
    emit("H4A04P3_VERIFY_FNV_EXPECTED", expected_hash)

    if actual_hash != expected_hash:
        raise RuntimeError(
            "H4A04P3_FIFO_REUSE_PAYLOAD_INTEGRITY_FAIL"
        )

    emit("H4A04P3_FIFO_REUSE_PAYLOAD_INTEGRITY", "PASS")

    buckets: dict[str, list[dict[str, float]]] = {
        "DIRECT_RX": [],
        "FIFO_REUSE": [],
    }
    counts = {"DIRECT_RX": 0, "FIFO_REUSE": 0}

    for case_index, variant in enumerate(ORDER, 1):
        counts[variant] += 1
        run_no = counts[variant]

        print()
        print("=" * 78)
        print(
            f" H4A0.4-P3 CASE {case_index}/6 "
            f"{variant} RUN {run_no}/3"
        )
        print("=" * 78)

        upload(
            cli=cli,
            fqbn=args.fqbn,
            serial_port=args.serial,
            build_dir=perf_builds[variant],
            sketch=sketch,
            repo=repo,
            log=result_root
            / f"{case_index:02d}_{variant.lower()}_upload.log",
            label=f"{variant}_RUN{run_no}",
        )
        time.sleep(3.0)

        case_text = run_case(
            runner=runner,
            repo=repo,
            serial_port=args.serial,
            duration_s=PERF_DURATION_S,
            variant_arg="PROFILE",
            log=result_root
            / f"{case_index:02d}_{variant.lower()}_run{run_no}.log",
        )

        row = inspect_profile_case(case_text)
        buckets[variant].append(row)

        emit(
            "H4A04P3_RUN_RESULT",
            (
                f"VARIANT={variant} RUN={run_no} "
                f"TCP={row['mid']:.6f}Mbps "
                f"PAYLOAD={row['payload_mbps']:.3f}Mbps "
                f"US_PER_BYTE={row['us_per_byte']:.6f} "
                f"PASS=YES"
            ),
        )

    if counts != {"DIRECT_RX": 3, "FIFO_REUSE": 3}:
        raise RuntimeError(
            f"H4A04P3_RUN_COUNTS_INVALID={counts}"
        )

    summary: dict[str, dict[str, float]] = {}

    for variant in ("DIRECT_RX", "FIFO_REUSE"):
        rows = buckets[variant]
        payload_rates = [
            row["payload_mbps"]
            for row in rows
        ]
        tcp_rates = [
            row["mid"]
            for row in rows
        ]

        summary[variant] = {
            "tcp": median(tcp_rates),
            "tcp_spread": spread_pct(tcp_rates),
            "payload_mbps": median(payload_rates),
            "payload_spread": spread_pct(payload_rates),
            "us_per_byte": median(
                [row["us_per_byte"] for row in rows]
            ),
            "avg_read_us": median(
                [row["avg_read_us"] for row in rows]
            ),
            "avg_read_bytes": median(
                [row["avg_read_bytes"] for row in rows]
            ),
            "payload_us": median(
                [row["payload_us"] for row in rows]
            ),
            "recv_us": median(
                [row["recv_us"] for row in rows]
            ),
            "hold_us": median(
                [row["hold_us"] for row in rows]
            ),
        }

    direct = summary["DIRECT_RX"]
    fifo = summary["FIFO_REUSE"]

    payload_gain = delta_pct(
        fifo["payload_mbps"],
        direct["payload_mbps"],
    )
    tcp_gain = delta_pct(
        fifo["tcp"],
        direct["tcp"],
    )
    us_per_byte_delta = delta_pct(
        fifo["us_per_byte"],
        direct["us_per_byte"],
    )

    repeatability_ok = (
        direct["payload_spread"] <= PAYLOAD_SPREAD_MAX_PCT
        and fifo["payload_spread"] <= PAYLOAD_SPREAD_MAX_PCT
    )

    if not repeatability_ok:
        interpretation = "PAYLOAD_REPEATABILITY_INSUFFICIENT"
    elif payload_gain <= -GAIN_THRESHOLD_PCT:
        interpretation = "FIFO_REUSE_REGRESSION"
    elif (
        payload_gain >= GAIN_THRESHOLD_PCT
        and us_per_byte_delta <= -GAIN_THRESHOLD_PCT
    ):
        interpretation = "FIFO_REUSE_GAIN_CONFIRMED"
    elif abs(payload_gain) <= NO_MATERIAL_BAND_PCT:
        interpretation = "NO_MATERIAL_FIFO_REUSE_GAIN"
    else:
        interpretation = "FIFO_REUSE_SMALL_OR_INCONCLUSIVE_EFFECT"

    print()
    print("=" * 78)
    print(" H4A0.4-P3 SUMMARY")
    print("=" * 78)

    for variant in ("DIRECT_RX", "FIFO_REUSE"):
        s = summary[variant]
        emit(
            f"H4A04P3_{variant}_TCP_MEDIAN_MBPS",
            f"{s['tcp']:.6f}",
        )
        emit(
            f"H4A04P3_{variant}_TCP_SPREAD_PCT",
            f"{s['tcp_spread']:.3f}",
        )
        emit(
            f"H4A04P3_{variant}_PAYLOAD_EFFECTIVE_MBPS",
            f"{s['payload_mbps']:.3f}",
        )
        emit(
            f"H4A04P3_{variant}_PAYLOAD_SPREAD_PCT",
            f"{s['payload_spread']:.3f}",
        )
        emit(
            f"H4A04P3_{variant}_US_PER_BYTE",
            f"{s['us_per_byte']:.6f}",
        )
        emit(
            f"H4A04P3_{variant}_AVG_READ_US",
            f"{s['avg_read_us']:.3f}",
        )
        emit(
            f"H4A04P3_{variant}_AVG_READ_BYTES",
            f"{s['avg_read_bytes']:.1f}",
        )
        emit(
            f"H4A04P3_{variant}_PAYLOAD_TOTAL_US",
            f"{s['payload_us']:.0f}",
        )
        emit(
            f"H4A04P3_{variant}_RECV_TOTAL_US",
            f"{s['recv_us']:.0f}",
        )
        emit(
            f"H4A04P3_{variant}_HOLD_TOTAL_US",
            f"{s['hold_us']:.0f}",
        )

    emit(
        "H4A04P3_FIFO_VS_DIRECT_PAYLOAD_PCT",
        f"{payload_gain:.3f}",
    )
    emit(
        "H4A04P3_FIFO_VS_DIRECT_TCP_PCT",
        f"{tcp_gain:.3f}",
    )
    emit(
        "H4A04P3_FIFO_VS_DIRECT_US_PER_BYTE_PCT",
        f"{us_per_byte_delta:.3f}",
    )
    emit(
        "H4A04P3_PAYLOAD_REPEATABILITY_OK",
        repeatability_ok,
    )
    emit(
        "H4A04P3_INTERPRETATION",
        interpretation,
    )

    if args.defer_physical_review:
        physical_status = "PENDING_USER"
        gate_status = "PASS_DATA_ONLY"
        emit(
            "H4A04P3_PHYSICAL_STABILITY",
            physical_status,
        )
    else:
        answer = input(
            "¿TFT/periféricos permanecieron estables durante H4A0.4-P3? (S/N): "
        ).strip().upper()

        if answer != "S":
            raise RuntimeError(
                "H4A04P3_PHYSICAL_STABILITY_FAILED"
            )

        physical_status = "PASS"
        gate_status = "PASS"
        emit(
            "H4A04P3_PHYSICAL_STABILITY",
            physical_status,
        )

    if git(repo, "diff", "--name-only") or git(
        repo,
        "diff",
        "--cached",
        "--name-only",
    ):
        raise RuntimeError(
            "H4A04P3_REPOSITORY_MUTATED_DURING_GATE"
        )

    summary_log = result_root / "SUMMARY.log"
    summary_log.write_text(
        "\n".join(
            [
                f"A14_H4A04P3_W5500_FIFO_REUSE_AB={gate_status}",
                f"HEAD={head}",
                f"VERIFY_RX_BYTES={verify_bytes}",
                f"VERIFY_FNV_ACTUAL={actual_hash}",
                f"VERIFY_FNV_EXPECTED={expected_hash}",
                "FIFO_REUSE_PAYLOAD_INTEGRITY=PASS",
                f"DIRECT_RX_PAYLOAD_EFFECTIVE_MBPS={direct['payload_mbps']:.3f}",
                f"FIFO_REUSE_PAYLOAD_EFFECTIVE_MBPS={fifo['payload_mbps']:.3f}",
                f"FIFO_VS_DIRECT_PAYLOAD_PCT={payload_gain:.3f}",
                f"DIRECT_RX_TCP_MEDIAN_MBPS={direct['tcp']:.6f}",
                f"FIFO_REUSE_TCP_MEDIAN_MBPS={fifo['tcp']:.6f}",
                f"FIFO_VS_DIRECT_TCP_PCT={tcp_gain:.3f}",
                f"FIFO_VS_DIRECT_US_PER_BYTE_PCT={us_per_byte_delta:.3f}",
                f"INTERPRETATION={interpretation}",
                f"PHYSICAL_STABILITY={physical_status}",
                "PRODUCT_DEFAULT_FIFO_REUSE=OFF",
                "TEMP_PRODUCT_PATCHES=NO",
                "TCP_TX_CHANGES=NO",
                "HARNESS_FAILURE=NO",
                "PRODUCT_FAILURE=NO_EVIDENCE",
                "HARDWARE_FAILURE=NO_EVIDENCE",
                (
                    "NEXT=CONTINUE_AUTONOMOUS_ROADMAP_WITHOUT_PROMOTION"
                    if args.defer_physical_review
                    else
                    "NEXT=RETURN_TO_CHAT_INTERPRET_P3_DO_NOT_PROMOTE_AUTOMATICALLY"
                ),
                "",
            ]
        ),
        encoding="utf-8",
    )

    emit("H4A04P3_SUMMARY_LOG", summary_log)
    emit("HARNESS_FAILURE", "NO")
    emit("PRODUCT_FAILURE", "NO_EVIDENCE")
    emit("HARDWARE_FAILURE", "NO_EVIDENCE")
    emit(
        "A14_H4A04P3_W5500_FIFO_REUSE_AB",
        gate_status,
    )
    emit(
        "NEXT",
        (
            "CONTINUE_AUTONOMOUS_ROADMAP_WITHOUT_PROMOTION"
            if args.defer_physical_review
            else
            "RETURN_TO_CHAT_INTERPRET_P3_DO_NOT_PROMOTE_AUTOMATICALLY"
        ),
    )
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
        emit("H4A04P3_FAILURE_REQUIRES_CLASSIFICATION", "YES")
        emit("H4A04P3_EXCEPTION", str(exc))
        raise SystemExit(1)
