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
    "CURRENT",
    "DIRECT_RX",
    "DIRECT_RX",
    "CURRENT",
    "CURRENT",
    "DIRECT_RX",
)
RUNS_PER_VARIANT = 3
GAIN_THRESHOLD_PCT = 1.0
NO_MATERIAL_BAND_PCT = 0.5
REPEATABILITY_SPREAD_MAX_PCT = 2.5


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

    raise RuntimeError("H4A04P2_ARDUINO_CLI_NOT_FOUND")


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
            f"H4A04P2_KEY_COUNT_{key}={len(found)}"
        )
    return found[0]


def number(text: str, key: str) -> float:
    return float(one(text, key))


def median(items: list[float]) -> float:
    if not items:
        raise RuntimeError("H4A04P2_EMPTY_MEDIAN")
    return float(statistics.median(items))


def spread_pct(items: list[float]) -> float:
    med = median(items)
    if med <= 0.0:
        raise RuntimeError("H4A04P2_NONPOSITIVE_MEDIAN")
    return (max(items) - min(items)) * 100.0 / med


def delta_pct(value: float, reference: float) -> float:
    if reference <= 0.0:
        raise RuntimeError("H4A04P2_NONPOSITIVE_REFERENCE")
    return (value / reference - 1.0) * 100.0


def effective_payload_mbps(
    payload_bytes: float,
    payload_us: float,
) -> float:
    if payload_us <= 0.0:
        raise RuntimeError("H4A04P2_NONPOSITIVE_PAYLOAD_TIME")
    return payload_bytes * 8.0 / payload_us


def inspect_case(text: str) -> dict[str, float]:
    if one(text, "H4A04P1_FUNCTIONAL_PASS") != "YES":
        raise RuntimeError("H4A04P2_FUNCTIONAL_FAIL")

    if one(text, "TCP_PROFILE_ENABLED") != "YES":
        raise RuntimeError("H4A04P2_PROFILE_NOT_ENABLED")

    for key in (
        "H4A04P1_TRANSPORT_ERRORS",
        "H4A04P1_TCP_SPI_LOCK_ERRORS",
    ):
        if number(text, key) != 0.0:
            raise RuntimeError(
                f"H4A04P2_{key}_NONZERO"
            )

    row = {
        "mid": number(text, "H4A04P1_DUT_MBPS_MID"),
        "lower": number(text, "H4A04P1_DUT_MBPS_LOWER"),
        "upper": number(text, "H4A04P1_DUT_MBPS_UPPER"),
        "hold_us": number(
            text,
            "H4A04P1_TCP_SPI_HOLD_TOTAL_US",
        ),
        "recv_us": number(
            text,
            "H4A04P1_TCP_PROF_RECV_TOTAL_US",
        ),
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
        "commit_us": number(
            text,
            "H4A04P1_TCP_PROF_COMMIT_TOTAL_US",
        ),
        "available_us": number(
            text,
            "H4A04P1_TCP_PROF_AVAILABLE_TOTAL_US",
        ),
        "status_us": number(
            text,
            "H4A04P1_TCP_PROF_SOCKET_STATUS_TOTAL_US",
        ),
    }

    if not (0.0 < row["lower"] <= row["mid"] <= row["upper"]):
        raise RuntimeError("H4A04P2_BOUNDS_INVALID")

    if row["payload_calls"] <= 0.0 or row["payload_bytes"] <= 0.0:
        raise RuntimeError("H4A04P2_PAYLOAD_PROFILE_EMPTY")

    row["payload_mbps"] = effective_payload_mbps(
        row["payload_bytes"],
        row["payload_us"],
    )
    row["payload_avg_us"] = (
        row["payload_us"]
        / row["payload_calls"]
    )
    row["payload_avg_bytes"] = (
        row["payload_bytes"]
        / row["payload_calls"]
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
    direct = "1" if variant == "DIRECT_RX" else "0"

    extra_flags = (
        "-DJWPLC_ETHERNET_ENABLE_PROFILE_HOOKS=1 "
        f"-DJWPLC_W5500_RX_DIRECT_TRANSFER_BYTES={direct}"
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
    emit(f"H4A04P2_{variant}_COMPILE_EXIT", exit_code)

    if exit_code != 0:
        print(decode(log.read_bytes())[-8000:])
        raise RuntimeError(
            f"H4A04P2_{variant}_COMPILE_FAILED"
        )

    normalized = decode(log.read_bytes()).replace("\\", "/").lower()

    required_tokens = (
        "-djwplc_ethernet_enable_profile_hooks=1",
        f"-djwplc_w5500_rx_direct_transfer_bytes={direct}",
        "w5100.cpp",
        "socket.cpp",
        "jwplc_idlescreen.cpp",
        "jwplc_tft.cpp",
    )

    for token in required_tokens:
        ok = token in normalized
        emit(
            f"H4A04P2_{variant}_COMPILE_TOKEN_{token.replace('=', '_')}",
            "PASS" if ok else "FAIL",
        )
        if not ok:
            raise RuntimeError(
                f"H4A04P2_{variant}_COMPILE_TOKEN_MISSING={token}"
            )

    if "libjwplc_display.a" in normalized:
        raise RuntimeError(
            f"H4A04P2_{variant}_DISPLAY_A_UNEXPECTED"
        )
    if "libjwplc_tft.a" in normalized:
        raise RuntimeError(
            f"H4A04P2_{variant}_TFT_A_UNEXPECTED"
        )


def main() -> int:
    parser = argparse.ArgumentParser(
        description="A14 H4A0.4-P2 W5500 direct-transfer A/B"
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

    print("=" * 78)
    print(" A14 H4A0.4-P2 - W5500 DIRECT TRANSFERBYTES TCP RX A/B")
    print("=" * 78)

    branch = git(repo, "branch", "--show-current")
    head = git(repo, "rev-parse", "HEAD")

    emit("BRANCH", branch)
    emit("HEAD", head)
    emit("SERIAL_PORT", args.serial)
    emit("H4A04P2_DURATION_S", DURATION_S)
    emit("H4A04P2_TCP_CHUNK", TCP_CHUNK)
    emit("H4A04P2_ORDER", ",".join(ORDER))
    emit("H4A04P2_RUNS_PER_VARIANT", RUNS_PER_VARIANT)
    emit("H4A04P2_SPI_HZ", 26000000)
    emit("H4A04P2_PROFILE_HOOKS", "ON_FOR_BOTH")
    emit("H4A04P2_PRODUCT_SOURCE", "JWPLC/2.1.0_CANONICAL_PACKAGE")
    emit("H4A04P2_TEMP_PRODUCT_PATCHES", "NO")
    emit("H4A04P2_ONLY_VARIABLE", "W5500_RX_DIRECT_TRANSFER_BYTES")
    emit("H4A04P2_DEFAULT_PRODUCT_BEHAVIOR", "CURRENT_OFF")

    if branch != BRANCH:
        raise RuntimeError("H4A04P2_BRANCH_MISMATCH")

    if git(repo, "diff", "--name-only") or git(
        repo,
        "diff",
        "--cached",
        "--name-only",
    ):
        raise RuntimeError("H4A04P2_TRACKED_TREE_NOT_CLEAN")

    if not sketch.is_dir() or not runner.is_file():
        raise RuntimeError("H4A04P2_REQUIRED_TOOLING_MISSING")

    ast.parse(
        runner.read_text(encoding="utf-8"),
        filename=str(runner),
    )
    emit("H4A04P2_RUNNER_AST", "PASS")

    contract_run = run(
        [sys.executable, "-B", str(contract)],
        repo,
    )
    print(decode(contract_run.stdout))
    if contract_run.returncode != 0:
        raise RuntimeError(
            "H4A04P2_PACKAGE_CONTRACT_FAILED"
        )

    cli = find_cli(args.arduino_cli)
    emit("ARDUINO_CLI", cli)

    result_root = Path(
        tempfile.mkdtemp(
            prefix="jwplc_a14_h4a04p2_direct_rx_"
        )
    )
    emit("H4A04P2_RESULT_ROOT", result_root)

    builds = {
        "CURRENT": result_root / "build_current",
        "DIRECT_RX": result_root / "build_direct_rx",
    }

    for variant in ("CURRENT", "DIRECT_RX"):
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
        "CURRENT": [],
        "DIRECT_RX": [],
    }
    counts = {"CURRENT": 0, "DIRECT_RX": 0}

    for case_index, variant in enumerate(ORDER, 1):
        counts[variant] += 1
        run_no = counts[variant]

        print()
        print("=" * 78)
        print(
            f" H4A0.4-P2 CASE {case_index}/6 "
            f"{variant} RUN {run_no}/3"
        )
        print("=" * 78)

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
            f"H4A04P2_{variant}_RUN{run_no}_UPLOAD_EXIT",
            upload_exit,
        )
        if upload_exit != 0:
            print(decode(upload_log.read_bytes())[-5000:])
            raise RuntimeError(
                f"H4A04P2_{variant}_RUN{run_no}_UPLOAD_FAILED"
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
                "PROFILE",
            ],
            case_log,
            repo,
        )

        emit(
            f"H4A04P2_{variant}_RUN{run_no}_CASE_EXIT",
            case_exit,
        )
        emit(
            f"H4A04P2_{variant}_RUN{run_no}_LOG",
            case_log,
        )

        case_text = decode(case_log.read_bytes())
        if case_exit != 0:
            print(case_text[-6000:])
            raise RuntimeError(
                f"H4A04P2_{variant}_RUN{run_no}_CASE_FAILED"
            )

        row = inspect_case(case_text)
        buckets[variant].append(row)

        emit(
            "H4A04P2_RUN_RESULT",
            (
                f"VARIANT={variant} RUN={run_no} "
                f"TCP={row['mid']:.6f}Mbps "
                f"PAYLOAD={row['payload_mbps']:.3f}Mbps "
                f"READ_AVG={row['payload_avg_us']:.3f}us "
                f"PASS=YES"
            ),
        )

    if counts != {"CURRENT": 3, "DIRECT_RX": 3}:
        raise RuntimeError(
            f"H4A04P2_RUN_COUNTS_INVALID={counts}"
        )

    summary: dict[str, dict[str, float]] = {}

    for variant in ("CURRENT", "DIRECT_RX"):
        rows = buckets[variant]
        mids = [row["mid"] for row in rows]

        summary[variant] = {
            "tcp": median(mids),
            "spread": spread_pct(mids),
            "payload_mbps": median(
                [row["payload_mbps"] for row in rows]
            ),
            "payload_avg_us": median(
                [row["payload_avg_us"] for row in rows]
            ),
            "payload_avg_bytes": median(
                [row["payload_avg_bytes"] for row in rows]
            ),
            "hold_us": median(
                [row["hold_us"] for row in rows]
            ),
            "recv_us": median(
                [row["recv_us"] for row in rows]
            ),
            "payload_us": median(
                [row["payload_us"] for row in rows]
            ),
        }

    cur = summary["CURRENT"]
    direct = summary["DIRECT_RX"]

    tcp_delta = delta_pct(
        direct["tcp"],
        cur["tcp"],
    )
    payload_delta = delta_pct(
        direct["payload_mbps"],
        cur["payload_mbps"],
    )
    read_time_delta = delta_pct(
        direct["payload_avg_us"],
        cur["payload_avg_us"],
    )

    repeatability_ok = (
        cur["spread"] <= REPEATABILITY_SPREAD_MAX_PCT
        and direct["spread"] <= REPEATABILITY_SPREAD_MAX_PCT
    )

    if not repeatability_ok:
        interpretation = "REPEATABILITY_INSUFFICIENT"
    elif tcp_delta <= -GAIN_THRESHOLD_PCT:
        interpretation = "DIRECT_RX_REGRESSION"
    elif (
        tcp_delta >= GAIN_THRESHOLD_PCT
        and payload_delta >= GAIN_THRESHOLD_PCT
        and read_time_delta <= -GAIN_THRESHOLD_PCT
    ):
        interpretation = "DIRECT_RX_GAIN_CONFIRMED"
    elif abs(tcp_delta) <= NO_MATERIAL_BAND_PCT:
        interpretation = "NO_MATERIAL_DIRECT_RX_GAIN"
    else:
        interpretation = "DIRECT_RX_SMALL_OR_INCONCLUSIVE_EFFECT"

    print()
    print("=" * 78)
    print(" H4A0.4-P2 SUMMARY")
    print("=" * 78)

    for variant in ("CURRENT", "DIRECT_RX"):
        s = summary[variant]
        emit(
            f"H4A04P2_{variant}_TCP_MEDIAN_MBPS",
            f"{s['tcp']:.6f}",
        )
        emit(
            f"H4A04P2_{variant}_TCP_SPREAD_PCT",
            f"{s['spread']:.3f}",
        )
        emit(
            f"H4A04P2_{variant}_PAYLOAD_EFFECTIVE_MBPS",
            f"{s['payload_mbps']:.3f}",
        )
        emit(
            f"H4A04P2_{variant}_PAYLOAD_READ_AVG_US",
            f"{s['payload_avg_us']:.3f}",
        )
        emit(
            f"H4A04P2_{variant}_PAYLOAD_AVG_BYTES",
            f"{s['payload_avg_bytes']:.1f}",
        )
        emit(
            f"H4A04P2_{variant}_HOLD_TOTAL_US",
            f"{s['hold_us']:.0f}",
        )
        emit(
            f"H4A04P2_{variant}_RECV_TOTAL_US",
            f"{s['recv_us']:.0f}",
        )
        emit(
            f"H4A04P2_{variant}_PAYLOAD_TOTAL_US",
            f"{s['payload_us']:.0f}",
        )

    emit(
        "H4A04P2_DIRECT_VS_CURRENT_TCP_PCT",
        f"{tcp_delta:.3f}",
    )
    emit(
        "H4A04P2_DIRECT_VS_CURRENT_PAYLOAD_MBPS_PCT",
        f"{payload_delta:.3f}",
    )
    emit(
        "H4A04P2_DIRECT_VS_CURRENT_READ_AVG_US_PCT",
        f"{read_time_delta:.3f}",
    )
    emit(
        "H4A04P2_REPEATABILITY_OK",
        repeatability_ok,
    )
    emit(
        "H4A04P2_GAIN_THRESHOLD_PCT",
        GAIN_THRESHOLD_PCT,
    )
    emit(
        "H4A04P2_NO_MATERIAL_BAND_PCT",
        NO_MATERIAL_BAND_PCT,
    )
    emit(
        "H4A04P2_INTERPRETATION",
        interpretation,
    )

    answer = input(
        "¿TFT permaneció estable durante H4A0.4-P2? (S/N): "
    ).strip().upper()
    if answer != "S":
        raise RuntimeError(
            "H4A04P2_TFT_PHYSICAL_REVIEW_FAILED"
        )

    emit("H4A04P2_TFT_PHYSICAL", "PASS")

    if git(repo, "diff", "--name-only") or git(
        repo,
        "diff",
        "--cached",
        "--name-only",
    ):
        raise RuntimeError(
            "H4A04P2_REPOSITORY_MUTATED_DURING_GATE"
        )

    summary_log = result_root / "SUMMARY.log"
    summary_log.write_text(
        "\n".join(
            [
                "A14_H4A04P2_W5500_DIRECT_RX_AB=PASS",
                f"HEAD={head}",
                f"CURRENT_TCP_MEDIAN_MBPS={cur['tcp']:.6f}",
                f"DIRECT_RX_TCP_MEDIAN_MBPS={direct['tcp']:.6f}",
                f"DIRECT_VS_CURRENT_TCP_PCT={tcp_delta:.3f}",
                f"CURRENT_PAYLOAD_EFFECTIVE_MBPS={cur['payload_mbps']:.3f}",
                f"DIRECT_RX_PAYLOAD_EFFECTIVE_MBPS={direct['payload_mbps']:.3f}",
                f"DIRECT_VS_CURRENT_PAYLOAD_MBPS_PCT={payload_delta:.3f}",
                f"DIRECT_VS_CURRENT_READ_AVG_US_PCT={read_time_delta:.3f}",
                f"INTERPRETATION={interpretation}",
                "PRODUCT_DEFAULT_DIRECT_RX=OFF",
                "TEMP_PRODUCT_PATCHES=NO",
                "TCP_TX_CHANGES=NO",
                "ASYNC_LIFECYCLE_CHANGES=NO",
                "HARNESS_FAILURE=NO",
                "PRODUCT_FAILURE=NO_EVIDENCE",
                "HARDWARE_FAILURE=NO_EVIDENCE",
                "NEXT=RETURN_TO_CHAT_INTERPRET_P2_DO_NOT_PROMOTE_AUTOMATICALLY",
                "",
            ]
        ),
        encoding="utf-8",
    )

    emit("H4A04P2_SUMMARY_LOG", summary_log)
    emit("HARNESS_FAILURE", "NO")
    emit("PRODUCT_FAILURE", "NO_EVIDENCE")
    emit("HARDWARE_FAILURE", "NO_EVIDENCE")
    emit(
        "A14_H4A04P2_W5500_DIRECT_RX_AB",
        "PASS",
    )
    emit(
        "NEXT",
        "RETURN_TO_CHAT_INTERPRET_P2_DO_NOT_PROMOTE_AUTOMATICALLY",
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
        emit("H4A04P2_FAILURE_REQUIRES_CLASSIFICATION", "YES")
        emit("H4A04P2_EXCEPTION", str(exc))
        raise SystemExit(1)
