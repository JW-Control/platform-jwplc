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
DURATION_S = 15.0
TCP_CHUNK = 4096
ORDER = (
    "BASE",
    "PROFILE",
    "PROFILE",
    "BASE",
    "BASE",
    "PROFILE",
)
RUNS_PER_VARIANT = 3


def emit(key: str, value: object) -> None:
    print(f"{key}={value}")


def decode(data: bytes) -> str:
    for enc in ("utf-8-sig", "utf-8", "cp1252"):
        try:
            return data.decode(enc)
        except UnicodeDecodeError:
            pass
    return data.decode("utf-8", errors="replace")


def run(
    cmd: list[str],
    cwd: Path | None = None,
) -> subprocess.CompletedProcess[bytes]:
    return subprocess.run(
        cmd,
        cwd=str(cwd) if cwd else None,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
    )


def run_logged(
    cmd: list[str],
    log: Path,
    cwd: Path | None = None,
) -> int:
    proc = run(cmd, cwd)
    log.write_bytes(proc.stdout)
    return proc.returncode


def git(repo: Path, *args: str) -> str:
    proc = run(["git", "-C", str(repo), *args])
    if proc.returncode != 0:
        raise RuntimeError(
            f"GIT_FAILED {' '.join(args)} :: "
            f"{decode(proc.stdout)[-1600:]}"
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

    raise RuntimeError("H4A04P1_ARDUINO_CLI_NOT_FOUND")


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
            f"H4A04P1_KEY_COUNT_{key}={len(found)}"
        )
    return found[0]


def number(text: str, key: str) -> float:
    return float(one(text, key))


def integer(text: str, key: str) -> int:
    return int(one(text, key))


def median(items: list[float]) -> float:
    if not items:
        raise RuntimeError("H4A04P1_EMPTY_MEDIAN")
    return float(statistics.median(items))


def spread_pct(items: list[float]) -> float:
    med = median(items)
    if med <= 0.0:
        raise RuntimeError("H4A04P1_NONPOSITIVE_MEDIAN")
    return (max(items) - min(items)) * 100.0 / med


def pct(numerator: float, denominator: float) -> float:
    if denominator <= 0.0:
        return 0.0
    return numerator * 100.0 / denominator


def delta_pct(value: float, reference: float) -> float:
    if reference <= 0.0:
        raise RuntimeError("H4A04P1_NONPOSITIVE_REFERENCE")
    return (value / reference - 1.0) * 100.0


def inspect_case(text: str, variant: str) -> dict[str, float]:
    if one(text, "H4A04P1_FUNCTIONAL_PASS") != "YES":
        raise RuntimeError(
            f"H4A04P1_{variant}_FUNCTIONAL_FAIL"
        )

    expected_profile = (
        "YES"
        if variant == "PROFILE"
        else "NO"
    )

    if one(text, "TCP_PROFILE_ENABLED") != expected_profile:
        raise RuntimeError(
            f"H4A04P1_{variant}_PROFILE_MODE_INVALID"
        )

    for key in (
        "H4A04P1_TRANSPORT_ERRORS",
        "H4A04P1_TCP_SPI_LOCK_ERRORS",
    ):
        if number(text, key) != 0.0:
            raise RuntimeError(
                f"H4A04P1_{variant}_{key}_NONZERO"
            )

    row: dict[str, float] = {
        "lower": number(text, "H4A04P1_DUT_MBPS_LOWER"),
        "upper": number(text, "H4A04P1_DUT_MBPS_UPPER"),
        "mid": number(text, "H4A04P1_DUT_MBPS_MID"),
        "hold_count": number(
            text,
            "H4A04P1_TCP_SPI_HOLD_COUNT",
        ),
        "hold_total_us": number(
            text,
            "H4A04P1_TCP_SPI_HOLD_TOTAL_US",
        ),
        "hold_max_us": number(
            text,
            "H4A04P1_TCP_SPI_HOLD_MAX_US",
        ),
        "rx_bytes": number(
            text,
            "H4A04P1_DUT_RX_BYTES",
        ),
        "rx_ops": number(
            text,
            "H4A04P1_DUT_RX_OPERATIONS",
        ),
        "freeze_ack_s": number(
            text,
            "H4A04P1_FREEZE_ACK_TAIL_S",
        ),
    }

    if not (0.0 < row["lower"] <= row["mid"] <= row["upper"]):
        raise RuntimeError(
            f"H4A04P1_{variant}_BOUNDS_INVALID"
        )

    if variant == "PROFILE":
        profile_map = {
            "status_calls": "H4A04P1_TCP_PROF_SOCKET_STATUS_CALLS",
            "status_us": "H4A04P1_TCP_PROF_SOCKET_STATUS_TOTAL_US",
            "available_calls": "H4A04P1_TCP_PROF_AVAILABLE_CALLS",
            "available_us": "H4A04P1_TCP_PROF_AVAILABLE_TOTAL_US",
            "available_rsr_calls":
                "H4A04P1_TCP_PROF_AVAILABLE_RSR_REFRESH_CALLS",
            "available_rsr_us":
                "H4A04P1_TCP_PROF_AVAILABLE_RSR_REFRESH_TOTAL_US",
            "recv_calls": "H4A04P1_TCP_PROF_RECV_CALLS",
            "recv_us": "H4A04P1_TCP_PROF_RECV_TOTAL_US",
            "recv_rsr_calls":
                "H4A04P1_TCP_PROF_RECV_RSR_REFRESH_CALLS",
            "recv_rsr_us":
                "H4A04P1_TCP_PROF_RECV_RSR_REFRESH_TOTAL_US",
            "payload_calls":
                "H4A04P1_TCP_PROF_PAYLOAD_READ_CALLS",
            "payload_us":
                "H4A04P1_TCP_PROF_PAYLOAD_READ_TOTAL_US",
            "payload_bytes":
                "H4A04P1_TCP_PROF_PAYLOAD_BYTES",
            "commit_calls":
                "H4A04P1_TCP_PROF_COMMIT_CALLS",
            "commit_us":
                "H4A04P1_TCP_PROF_COMMIT_TOTAL_US",
        }

        for field, key in profile_map.items():
            row[field] = number(text, key)

        if int(row["payload_bytes"]) != int(row["rx_bytes"]):
            raise RuntimeError(
                "H4A04P1_PROFILE_PAYLOAD_BYTES_MISMATCH "
                f"PROFILE={int(row['payload_bytes'])} "
                f"RX={int(row['rx_bytes'])}"
            )

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
) -> None:
    hook_value = "1" if variant == "PROFILE" else "0"

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
        (
            "compiler.cpp.extra_flags="
            f"-DJWPLC_ETHERNET_ENABLE_PROFILE_HOOKS={hook_value}"
        ),
        str(sketch),
    ]

    exit_code = run_logged(cmd, log, repo)
    emit(f"H4A04P1_{variant}_COMPILE_EXIT", exit_code)

    if exit_code != 0:
        print(decode(log.read_bytes())[-8000:])
        raise RuntimeError(
            f"H4A04P1_{variant}_COMPILE_FAILED"
        )

    log_text = decode(log.read_bytes())
    normalized = log_text.replace("\\", "/").lower()

    requirements = {
        "ETHERNETCLIENT_SOURCE":
            "ethernetclient.cpp" in normalized,
        "SOCKET_SOURCE":
            "socket.cpp" in normalized,
        "DISPLAY_SOURCE":
            "jwplc_idlescreen.cpp" in normalized,
        "TFT_SOURCE":
            "jwplc_tft.cpp" in normalized,
        "DISPLAY_A_NOT_USED":
            "libjwplc_display.a" not in normalized,
        "TFT_A_NOT_USED":
            "libjwplc_tft.a" not in normalized,
        "PROFILE_DEFINE":
            (
                f"-djwplc_ethernet_enable_profile_hooks={hook_value}"
                in normalized
            ),
    }

    for label, ok in requirements.items():
        emit(
            f"H4A04P1_{variant}_COMPILE_{label}",
            "PASS" if ok else "FAIL",
        )

    failed = [
        label
        for label, ok in requirements.items()
        if not ok
    ]
    if failed:
        raise RuntimeError(
            f"H4A04P1_{variant}_COMPILE_CONTRACT_FAILED="
            + ",".join(failed)
        )


def main() -> int:
    parser = argparse.ArgumentParser(
        description="A14 H4A0.4-P1 package-first TCP RX bottleneck gate"
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

    print("=" * 76)
    print(" A14 H4A0.4-P1 - PACKAGE-FIRST TCP RX BOTTLENECK PROFILING")
    print("=" * 76)

    branch = git(repo, "branch", "--show-current")
    head = git(repo, "rev-parse", "HEAD")

    emit("BRANCH", branch)
    emit("HEAD", head)
    emit("SERIAL_PORT", args.serial)
    emit("FQBN", args.fqbn)
    emit("H4A04P1_DURATION_S", DURATION_S)
    emit("H4A04P1_TCP_CHUNK", TCP_CHUNK)
    emit("H4A04P1_RUNS_PER_VARIANT", RUNS_PER_VARIANT)
    emit("H4A04P1_ORDER", ",".join(ORDER))
    emit("H4A04P1_FRESH_UPLOAD_PER_CASE", "YES")
    emit("H4A04P1_PRODUCT_SOURCE", "JWPLC/2.1.0_CANONICAL_PACKAGE")
    emit("H4A04P1_TEMP_PRODUCT_PATCHES", "NO")
    emit("H4A04P1_TCP_TX_CHANGES", "NO")
    emit("H4A04P1_ASYNC_LIFECYCLE_CHANGES", "NO")
    emit("H4A04P1_SPI_HZ", 26000000)

    if branch != BRANCH:
        raise RuntimeError("H4A04P1_BRANCH_MISMATCH")

    if git(repo, "diff", "--name-only") or git(
        repo,
        "diff",
        "--cached",
        "--name-only",
    ):
        raise RuntimeError("H4A04P1_TRACKED_TREE_NOT_CLEAN")

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
        "H4A04P1_UNTRACKED_PRODUCT_COUNT",
        len(untracked_product),
    )
    if untracked_product:
        raise RuntimeError(
            "H4A04P1_UNTRACKED_PRODUCT_SOURCE"
        )

    if not sketch.is_dir() or not runner.is_file():
        raise RuntimeError("H4A04P1_REQUIRED_TOOLING_MISSING")

    ast.parse(
        runner.read_text(encoding="utf-8"),
        filename=str(runner),
    )
    emit("H4A04P1_RUNNER_AST", "PASS")

    contract_run = run(
        [sys.executable, "-B", str(contract)],
        repo,
    )
    contract_text = decode(contract_run.stdout)
    print(contract_text)
    if contract_run.returncode != 0:
        raise RuntimeError(
            "H4A04P1_PACKAGE_PROMOTION_CONTRACT_FAILED"
        )

    cli = find_cli(args.arduino_cli)
    emit("ARDUINO_CLI", cli)

    result_root = Path(
        tempfile.mkdtemp(
            prefix="jwplc_a14_h4a04p1_tcp_rx_"
        )
    )
    emit("H4A04P1_RESULT_ROOT", result_root)

    builds = {
        "BASE": result_root / "build_base",
        "PROFILE": result_root / "build_profile",
    }

    for variant in ("BASE", "PROFILE"):
        compile_variant(
            cli=cli,
            fqbn=args.fqbn,
            repo=repo,
            sketch=sketch,
            libraries=libraries,
            build_dir=builds[variant],
            variant=variant,
            log=result_root
            / f"compile_{variant.lower()}.log",
        )

    buckets: dict[str, list[dict[str, float]]] = {
        "BASE": [],
        "PROFILE": [],
    }
    counts = {"BASE": 0, "PROFILE": 0}

    for case_index, variant in enumerate(ORDER, 1):
        counts[variant] += 1
        run_no = counts[variant]

        print()
        print("=" * 76)
        print(
            f" H4A0.4-P1 CASE {case_index}/6 "
            f"{variant} RUN {run_no}/3"
        )
        print("=" * 76)

        upload_log = (
            result_root
            / f"{case_index:02d}_{variant.lower()}_upload.log"
        )

        upload_exit = run_logged(
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

        emit(
            f"H4A04P1_{variant}_RUN{run_no}_UPLOAD_EXIT",
            upload_exit,
        )

        if upload_exit != 0:
            print(decode(upload_log.read_bytes())[-5000:])
            raise RuntimeError(
                f"H4A04P1_{variant}_RUN{run_no}_UPLOAD_FAILED"
            )

        time.sleep(3.0)

        case_log = (
            result_root
            / f"{case_index:02d}_{variant.lower()}_run{run_no}.log"
        )

        case_exit = run_logged(
            [
                sys.executable,
                "-B",
                "-u",
                str(runner),
                "--serial",
                args.serial,
                "--duration",
                f"{DURATION_S:.1f}",
                "--chunk",
                str(TCP_CHUNK),
                "--variant",
                variant,
            ],
            case_log,
            repo,
        )

        emit(
            f"H4A04P1_{variant}_RUN{run_no}_CASE_EXIT",
            case_exit,
        )
        emit(
            f"H4A04P1_{variant}_RUN{run_no}_LOG",
            case_log,
        )

        case_text = decode(case_log.read_bytes())

        if case_exit != 0:
            print(case_text[-6000:])
            raise RuntimeError(
                f"H4A04P1_{variant}_RUN{run_no}_CASE_FAILED"
            )

        row = inspect_case(case_text, variant)
        buckets[variant].append(row)

        emit(
            "H4A04P1_RUN_RESULT",
            (
                f"VARIANT={variant} RUN={run_no} "
                f"MID={row['mid']:.6f}Mbps "
                f"HOLD={row['hold_total_us']/1000.0:.3f}ms "
                f"FREEZE_ACK={row['freeze_ack_s']*1000.0:.3f}ms "
                f"PASS=YES"
            ),
        )

    if counts != {"BASE": 3, "PROFILE": 3}:
        raise RuntimeError(
            f"H4A04P1_RUN_COUNTS_INVALID={counts}"
        )

    base_mid = [row["mid"] for row in buckets["BASE"]]
    profile_mid = [
        row["mid"]
        for row in buckets["PROFILE"]
    ]

    base_median = median(base_mid)
    profile_median = median(profile_mid)
    profile_penalty = delta_pct(
        profile_median,
        base_median,
    )

    def med(field: str) -> float:
        return median(
            [
                row[field]
                for row in buckets["PROFILE"]
            ]
        )

    hold_us = med("hold_total_us")
    status_us = med("status_us")
    available_us = med("available_us")
    recv_us = med("recv_us")
    top_known_us = status_us + available_us + recv_us
    other_hold_us = max(0.0, hold_us - top_known_us)

    recv_rsr_us = med("recv_rsr_us")
    payload_us = med("payload_us")
    commit_us = med("commit_us")
    recv_other_us = max(
        0.0,
        recv_us - recv_rsr_us - payload_us - commit_us,
    )

    status_calls = med("status_calls")
    available_calls = med("available_calls")
    available_rsr_calls = med("available_rsr_calls")
    recv_calls = med("recv_calls")
    recv_rsr_calls = med("recv_rsr_calls")
    payload_calls = med("payload_calls")
    commit_calls = med("commit_calls")
    payload_bytes = med("payload_bytes")

    top_stages = {
        "SOCKET_STATUS": status_us,
        "AVAILABLE": available_us,
        "SOCKET_RECV": recv_us,
        "OTHER_HOLD": other_hold_us,
    }
    top_stage = max(
        top_stages,
        key=top_stages.get,
    )

    recv_stages = {
        "PAYLOAD_SPI_READ": payload_us,
        "RSR_REFRESH": recv_rsr_us,
        "RX_RD_RECV_COMMIT": commit_us,
        "RECV_OTHER": recv_other_us,
    }
    recv_top_stage = max(
        recv_stages,
        key=recv_stages.get,
    )

    print()
    print("=" * 76)
    print(" H4A0.4-P1 SUMMARY")
    print("=" * 76)

    emit(
        "H4A04P1_BASE_MEDIAN_MBPS",
        f"{base_median:.6f}",
    )
    emit(
        "H4A04P1_BASE_SPREAD_PCT",
        f"{spread_pct(base_mid):.3f}",
    )
    emit(
        "H4A04P1_PROFILE_MEDIAN_MBPS",
        f"{profile_median:.6f}",
    )
    emit(
        "H4A04P1_PROFILE_SPREAD_PCT",
        f"{spread_pct(profile_mid):.3f}",
    )
    emit(
        "H4A04P1_PROFILE_VS_BASE_PCT",
        f"{profile_penalty:.3f}",
    )

    emit(
        "H4A04P1_PROFILE_HOLD_TOTAL_US_MEDIAN",
        f"{hold_us:.0f}",
    )

    for name, value in (
        ("SOCKET_STATUS", status_us),
        ("AVAILABLE", available_us),
        ("SOCKET_RECV", recv_us),
        ("OTHER_HOLD", other_hold_us),
    ):
        emit(
            f"H4A04P1_STAGE_{name}_US",
            f"{value:.0f}",
        )
        emit(
            f"H4A04P1_STAGE_{name}_HOLD_PCT",
            f"{pct(value, hold_us):.2f}",
        )

    emit(
        "H4A04P1_TOP_LEVEL_DOMINANT_STAGE",
        top_stage,
    )

    for name, value in (
        ("PAYLOAD_SPI_READ", payload_us),
        ("RSR_REFRESH", recv_rsr_us),
        ("RX_RD_RECV_COMMIT", commit_us),
        ("RECV_OTHER", recv_other_us),
    ):
        emit(
            f"H4A04P1_RECV_{name}_US",
            f"{value:.0f}",
        )
        emit(
            f"H4A04P1_RECV_{name}_RECV_PCT",
            f"{pct(value, recv_us):.2f}",
        )

    emit(
        "H4A04P1_RECV_DOMINANT_STAGE",
        recv_top_stage,
    )

    emit(
        "H4A04P1_STATUS_CALLS_MEDIAN",
        f"{status_calls:.0f}",
    )
    emit(
        "H4A04P1_AVAILABLE_CALLS_MEDIAN",
        f"{available_calls:.0f}",
    )
    emit(
        "H4A04P1_AVAILABLE_RSR_REFRESH_CALLS_MEDIAN",
        f"{available_rsr_calls:.0f}",
    )
    emit(
        "H4A04P1_AVAILABLE_RSR_REFRESH_RATE_PCT",
        f"{pct(available_rsr_calls, available_calls):.2f}",
    )
    emit(
        "H4A04P1_RECV_CALLS_MEDIAN",
        f"{recv_calls:.0f}",
    )
    emit(
        "H4A04P1_RECV_RSR_REFRESH_CALLS_MEDIAN",
        f"{recv_rsr_calls:.0f}",
    )
    emit(
        "H4A04P1_PAYLOAD_READ_CALLS_MEDIAN",
        f"{payload_calls:.0f}",
    )
    emit(
        "H4A04P1_COMMIT_CALLS_MEDIAN",
        f"{commit_calls:.0f}",
    )
    emit(
        "H4A04P1_PAYLOAD_BYTES_MEDIAN",
        f"{payload_bytes:.0f}",
    )

    emit(
        "H4A04P1_STATUS_AVG_US",
        f"{status_us / status_calls:.3f}"
        if status_calls > 0
        else "0",
    )
    emit(
        "H4A04P1_AVAILABLE_AVG_US",
        f"{available_us / available_calls:.3f}"
        if available_calls > 0
        else "0",
    )
    emit(
        "H4A04P1_RECV_AVG_US",
        f"{recv_us / recv_calls:.3f}"
        if recv_calls > 0
        else "0",
    )
    emit(
        "H4A04P1_PAYLOAD_READ_AVG_US",
        f"{payload_us / payload_calls:.3f}"
        if payload_calls > 0
        else "0",
    )
    emit(
        "H4A04P1_COMMIT_AVG_US",
        f"{commit_us / commit_calls:.3f}"
        if commit_calls > 0
        else "0",
    )

    profile_intrusive = abs(profile_penalty) > 5.0
    emit(
        "H4A04P1_PROFILE_INTRUSIVE_GT_5PCT",
        profile_intrusive,
    )

    answer = input(
        "¿TFT muestra el título con rectángulo azul y texto blanco "
        "y permaneció estable durante P1? (S/N): "
    ).strip().upper()

    if answer != "S":
        raise RuntimeError(
            "H4A04P1_TFT_VISUAL_MARKER_OR_STABILITY_FAILED"
        )

    emit("H4A04P1_TFT_JWPLC_TFT_MARKER", "PASS")

    if git(repo, "diff", "--name-only") or git(
        repo,
        "diff",
        "--cached",
        "--name-only",
    ):
        raise RuntimeError(
            "H4A04P1_REPOSITORY_MUTATED_DURING_GATE"
        )

    summary = result_root / "SUMMARY.log"
    summary.write_text(
        "\n".join(
            [
                "A14_H4A04P1_TCP_RX_BOTTLENECK_PROFILE=PASS",
                f"HEAD={head}",
                f"BASE_MEDIAN_MBPS={base_median:.6f}",
                f"PROFILE_MEDIAN_MBPS={profile_median:.6f}",
                f"PROFILE_VS_BASE_PCT={profile_penalty:.3f}",
                f"TOP_LEVEL_DOMINANT_STAGE={top_stage}",
                f"RECV_DOMINANT_STAGE={recv_top_stage}",
                f"PROFILE_INTRUSIVE_GT_5PCT={profile_intrusive}",
                "TFT_JWPLC_TFT_MARKER=PASS",
                "PRODUCT_SOURCE=JWPLC/2.1.0_CANONICAL_PACKAGE",
                "TEMP_PRODUCT_PATCHES=NO",
                "TCP_TX_CHANGES=NO",
                "ASYNC_LIFECYCLE_CHANGES=NO",
                "HARNESS_FAILURE=NO",
                "PRODUCT_FAILURE=NO_EVIDENCE",
                "HARDWARE_FAILURE=NO_EVIDENCE",
                "NEXT=RETURN_TO_CHAT_INTERPRET_P1_ONE_CHANGE_AT_A_TIME",
                "",
            ]
        ),
        encoding="utf-8",
    )

    emit("H4A04P1_SUMMARY_LOG", summary)
    emit("HARNESS_FAILURE", "NO")
    emit("PRODUCT_FAILURE", "NO_EVIDENCE")
    emit("HARDWARE_FAILURE", "NO_EVIDENCE")
    emit(
        "A14_H4A04P1_TCP_RX_BOTTLENECK_PROFILE",
        "PASS",
    )
    emit(
        "NEXT",
        "RETURN_TO_CHAT_INTERPRET_P1_ONE_CHANGE_AT_A_TIME",
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
        emit("H4A04P1_FAILURE_REQUIRES_CLASSIFICATION", "YES")
        emit("H4A04P1_EXCEPTION", str(exc))
        raise SystemExit(1)
