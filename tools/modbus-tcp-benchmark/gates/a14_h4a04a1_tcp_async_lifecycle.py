#!/usr/bin/env python3
from __future__ import annotations

import argparse
import ast
import re
import shutil
import subprocess
import sys
import tempfile
import time
from pathlib import Path

BRANCH = "v2.1.0-alpha.14/feature/modbus-tcp"
RUN_CYCLES = 3
API_CALL_LIMIT_US = 10000


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

    raise RuntimeError("H4A04A1_ARDUINO_CLI_NOT_FOUND")


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
            f"H4A04A1_KEY_COUNT_{key}={len(found)}"
        )
    return found[0]


def integer(text: str, key: str) -> int:
    return int(one(text, key))


def main() -> int:
    parser = argparse.ArgumentParser(
        description="A14 H4A0.4-A1 TCP async lifecycle qualification"
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
        / "a14_h4a04a1_tcp_async_lifecycle"
    )
    runner = (
        repo
        / "tools"
        / "modbus-tcp-benchmark"
        / "pc"
        / "a14_h4a04a1_tcp_async_lifecycle_case.py"
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
    print(" A14 H4A0.4-A1 - TCP ASYNC LIFECYCLE QUALIFICATION")
    print("=" * 76)

    branch = git(repo, "branch", "--show-current")
    head = git(repo, "rev-parse", "HEAD")

    emit("BRANCH", branch)
    emit("HEAD", head)
    emit("SERIAL_PORT", args.serial)
    emit("FQBN", args.fqbn)
    emit("H4A04A1_RUN_CYCLES", RUN_CYCLES)
    emit("H4A04A1_API_CALL_LIMIT_US", API_CALL_LIMIT_US)
    emit("H4A04A1_PRODUCT_SOURCE", "JWPLC/2.1.0_CANONICAL_PACKAGE")
    emit("H4A04A1_TEMP_PRODUCT_PATCHES", "NO")
    emit("H4A04A1_TCP_RX_THROUGHPUT_CHANGES", "NO")
    emit("H4A04A1_TCP_TX_IMPLEMENTATION_CHANGES", "NO")
    emit("H4A04A1_DIRECT_RX_DEFAULT", "OFF")
    emit(
        "H4A04A1_APIS",
        (
            "begin/poll/inProgress/cancel CONNECT + "
            "begin/poll/inProgress/cancel FLUSH + "
            "begin/poll/inProgress/cancel STOP"
        ),
    )

    if branch != BRANCH:
        raise RuntimeError("H4A04A1_BRANCH_MISMATCH")

    if git(repo, "diff", "--name-only") or git(
        repo,
        "diff",
        "--cached",
        "--name-only",
    ):
        raise RuntimeError("H4A04A1_TRACKED_TREE_NOT_CLEAN")

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
        "H4A04A1_UNTRACKED_PRODUCT_COUNT",
        len(untracked_product),
    )

    if untracked_product:
        raise RuntimeError(
            "H4A04A1_UNTRACKED_PRODUCT_SOURCE"
        )

    if not sketch.is_dir() or not runner.is_file():
        raise RuntimeError("H4A04A1_REQUIRED_TOOLING_MISSING")

    ast.parse(
        runner.read_text(encoding="utf-8"),
        filename=str(runner),
    )
    emit("H4A04A1_RUNNER_AST", "PASS")

    contract_run = run(
        [sys.executable, "-B", str(contract)],
        repo,
    )
    print(decode(contract_run.stdout))

    if contract_run.returncode != 0:
        raise RuntimeError(
            "H4A04A1_PACKAGE_CONTRACT_FAILED"
        )

    cli = find_cli(args.arduino_cli)
    emit("ARDUINO_CLI", cli)

    result_root = Path(
        tempfile.mkdtemp(
            prefix="jwplc_a14_h4a04a1_async_tcp_"
        )
    )
    build_dir = result_root / "build"
    compile_log = result_root / "compile.log"
    upload_log = result_root / "upload.log"
    case_log = result_root / "case.log"

    emit("H4A04A1_RESULT_ROOT", result_root)

    compile_exit = run_logged(
        [
            str(cli),
            "compile",
            "--verbose",
            "--fqbn",
            args.fqbn,
            "--build-path",
            str(build_dir),
            "--libraries",
            str(libraries),
            str(sketch),
        ],
        compile_log,
        repo,
    )
    emit("H4A04A1_COMPILE_EXIT", compile_exit)

    if compile_exit != 0:
        print(decode(compile_log.read_bytes())[-8000:])
        raise RuntimeError(
            "H4A04A1_COMPILE_FAILED"
        )

    normalized = (
        decode(compile_log.read_bytes())
        .replace("\\", "/")
        .lower()
    )

    compile_requirements = {
        "ETHERNETCLIENT_SOURCE":
            "ethernetclient.cpp" in normalized,
        "SOCKET_SOURCE":
            "socket.cpp" in normalized,
        "W5100_SOURCE":
            "w5100.cpp" in normalized,
        "DISPLAY_SOURCE":
            "jwplc_idlescreen.cpp" in normalized,
        "TFT_SOURCE":
            "jwplc_tft.cpp" in normalized,
        "DISPLAY_A_NOT_USED":
            "libjwplc_display.a" not in normalized,
        "TFT_A_NOT_USED":
            "libjwplc_tft.a" not in normalized,
    }

    for label, ok in compile_requirements.items():
        emit(
            f"H4A04A1_COMPILE_{label}",
            "PASS" if ok else "FAIL",
        )

    failed_compile = [
        label
        for label, ok in compile_requirements.items()
        if not ok
    ]

    if failed_compile:
        raise RuntimeError(
            "H4A04A1_COMPILE_CONTRACT_FAILED="
            + ",".join(failed_compile)
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
            str(build_dir),
            str(sketch),
        ],
        upload_log,
        repo,
    )
    emit("H4A04A1_UPLOAD_EXIT", upload_exit)

    if upload_exit != 0:
        print(decode(upload_log.read_bytes())[-5000:])
        raise RuntimeError(
            "H4A04A1_UPLOAD_FAILED"
        )

    time.sleep(3.0)

    case_exit = run_logged(
        [
            sys.executable,
            "-B",
            "-u",
            str(runner),
            "--serial",
            args.serial,
            "--run-cycles",
            str(RUN_CYCLES),
        ],
        case_log,
        repo,
    )

    emit("H4A04A1_CASE_EXIT", case_exit)
    emit("H4A04A1_CASE_LOG", case_log)

    case_text = decode(case_log.read_bytes())

    if case_exit != 0:
        print(case_text[-8000:])
        raise RuntimeError(
            "H4A04A1_RUNTIME_CASE_FAILED"
        )

    if one(case_text, "H4A04A1_FUNCTIONAL_PASS") != "YES":
        raise RuntimeError(
            "H4A04A1_FUNCTIONAL_NOT_PASS"
        )

    expected_connections = RUN_CYCLES + 1

    accepted = integer(
        case_text,
        "H4A04A1_PC_ACCEPTED",
    )
    closed = integer(
        case_text,
        "H4A04A1_PC_CLOSED",
    )

    if accepted != expected_connections:
        raise RuntimeError(
            "H4A04A1_PC_ACCEPT_COUNT_INVALID "
            f"EXPECTED={expected_connections} ACTUAL={accepted}"
        )

    if closed != expected_connections:
        raise RuntimeError(
            "H4A04A1_PC_CLOSE_COUNT_INVALID "
            f"EXPECTED={expected_connections} ACTUAL={closed}"
        )

    timing_keys = (
        "H4A04A1_CONNECT_BEGIN_MAX_US",
        "H4A04A1_CONNECT_POLL_MAX_US",
        "H4A04A1_FLUSH_BEGIN_MAX_US",
        "H4A04A1_FLUSH_POLL_MAX_US",
        "H4A04A1_STOP_BEGIN_MAX_US",
        "H4A04A1_STOP_POLL_MAX_US",
        "H4A04A1_CANCEL_CONNECT_CALL_US",
        "H4A04A1_CANCEL_STOP_CALL_US",
    )

    for key in timing_keys:
        value = integer(case_text, key)
        if value > API_CALL_LIMIT_US:
            raise RuntimeError(
                f"H4A04A1_API_CALL_TOO_SLOW_{key}={value}"
            )

    loop_ticks_min = integer(
        case_text,
        "H4A04A1_LOOP_TICKS_MIN",
    )

    if loop_ticks_min <= 0:
        raise RuntimeError(
            "H4A04A1_LOOP_DID_NOT_ADVANCE"
        )

    summary_keys = (
        "H4A04A1_DUT_IP",
        "H4A04A1_PC_SERVER_IP",
        "H4A04A1_PC_SERVER_PORT",
        "H4A04A1_CONNECT_BEGIN_MAX_US",
        "H4A04A1_CONNECT_POLL_MAX_US",
        "H4A04A1_CONNECT_TOTAL_MEDIAN_MS",
        "H4A04A1_CONNECT_PENDING_SEEN_RUNS",
        "H4A04A1_FLUSH_BEGIN_MAX_US",
        "H4A04A1_FLUSH_POLL_MAX_US",
        "H4A04A1_FLUSH_TOTAL_MEDIAN_MS",
        "H4A04A1_FLUSH_PENDING_SEEN_RUNS",
        "H4A04A1_STOP_BEGIN_MAX_US",
        "H4A04A1_STOP_POLL_MAX_US",
        "H4A04A1_STOP_TOTAL_MEDIAN_MS",
        "H4A04A1_STOP_PENDING_SEEN_RUNS",
        "H4A04A1_LOOP_TICKS_MIN",
        "H4A04A1_CANCEL_CONNECT_CALL_US",
        "H4A04A1_CANCEL_STOP_CALL_US",
        "H4A04A1_PC_ACCEPTED",
        "H4A04A1_PC_CLOSED",
        "H4A04A1_PC_RESETS",
    )

    print()
    print("=" * 76)
    print(" H4A0.4-A1 SUMMARY")
    print("=" * 76)

    for key in summary_keys:
        emit(key, one(case_text, key))

    flush_pending_runs = integer(
        case_text,
        "H4A04A1_FLUSH_PENDING_SEEN_RUNS",
    )

    flush_interpretation = (
        "PENDING_PATH_OBSERVED"
        if flush_pending_runs > 0
        else "IMMEDIATE_WITH_CURRENT_SYNC_WRITE"
    )

    emit(
        "H4A04A1_FLUSH_ASYNC_INTERPRETATION",
        flush_interpretation,
    )

    answer = input(
        "¿TFT/periféricos permanecieron estables durante A1? (S/N): "
    ).strip().upper()

    if answer != "S":
        raise RuntimeError(
            "H4A04A1_PHYSICAL_STABILITY_FAILED"
        )

    emit("H4A04A1_PHYSICAL_STABILITY", "PASS")

    if git(repo, "diff", "--name-only") or git(
        repo,
        "diff",
        "--cached",
        "--name-only",
    ):
        raise RuntimeError(
            "H4A04A1_REPOSITORY_MUTATED_DURING_GATE"
        )

    summary_log = result_root / "SUMMARY.log"
    summary_log.write_text(
        "\n".join(
            [
                "A14_H4A04A1_TCP_ASYNC_LIFECYCLE=PASS",
                f"HEAD={head}",
                f"CONNECT_BEGIN_MAX_US={one(case_text, 'H4A04A1_CONNECT_BEGIN_MAX_US')}",
                f"CONNECT_POLL_MAX_US={one(case_text, 'H4A04A1_CONNECT_POLL_MAX_US')}",
                f"CONNECT_TOTAL_MEDIAN_MS={one(case_text, 'H4A04A1_CONNECT_TOTAL_MEDIAN_MS')}",
                f"FLUSH_BEGIN_MAX_US={one(case_text, 'H4A04A1_FLUSH_BEGIN_MAX_US')}",
                f"FLUSH_POLL_MAX_US={one(case_text, 'H4A04A1_FLUSH_POLL_MAX_US')}",
                f"FLUSH_TOTAL_MEDIAN_MS={one(case_text, 'H4A04A1_FLUSH_TOTAL_MEDIAN_MS')}",
                f"FLUSH_INTERPRETATION={flush_interpretation}",
                f"STOP_BEGIN_MAX_US={one(case_text, 'H4A04A1_STOP_BEGIN_MAX_US')}",
                f"STOP_POLL_MAX_US={one(case_text, 'H4A04A1_STOP_POLL_MAX_US')}",
                f"STOP_TOTAL_MEDIAN_MS={one(case_text, 'H4A04A1_STOP_TOTAL_MEDIAN_MS')}",
                f"CANCEL_CONNECT_CALL_US={one(case_text, 'H4A04A1_CANCEL_CONNECT_CALL_US')}",
                f"CANCEL_STOP_CALL_US={one(case_text, 'H4A04A1_CANCEL_STOP_CALL_US')}",
                f"PC_ACCEPTED={accepted}",
                f"PC_CLOSED={closed}",
                "PRODUCT_SOURCE=JWPLC/2.1.0_CANONICAL_PACKAGE",
                "TEMP_PRODUCT_PATCHES=NO",
                "TCP_RX_THROUGHPUT_CHANGES=NO",
                "TCP_TX_IMPLEMENTATION_CHANGES=NO",
                "HARNESS_FAILURE=NO",
                "PRODUCT_FAILURE=NO_EVIDENCE",
                "HARDWARE_FAILURE=NO_EVIDENCE",
                "NEXT=RETURN_TO_CHAT_INTERPRET_A1_BEFORE_SPI_P3",
                "",
            ]
        ),
        encoding="utf-8",
    )

    emit("H4A04A1_SUMMARY_LOG", summary_log)
    emit("HARNESS_FAILURE", "NO")
    emit("PRODUCT_FAILURE", "NO_EVIDENCE")
    emit("HARDWARE_FAILURE", "NO_EVIDENCE")
    emit(
        "A14_H4A04A1_TCP_ASYNC_LIFECYCLE",
        "PASS",
    )
    emit(
        "NEXT",
        "RETURN_TO_CHAT_INTERPRET_A1_BEFORE_SPI_P3",
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
        emit("H4A04A1_FAILURE_REQUIRES_CLASSIFICATION", "YES")
        emit("H4A04A1_EXCEPTION", str(exc))
        raise SystemExit(1)
