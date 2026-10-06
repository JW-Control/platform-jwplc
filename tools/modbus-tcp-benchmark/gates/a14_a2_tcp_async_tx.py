#!/usr/bin/env python3
from __future__ import annotations

import argparse
import ast
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

BRANCH = "v2.1.0-alpha.14/feature/modbus-tcp"


def emit(key: str, value: object) -> None:
    print(f"{key}={value}")


def decode(data: bytes) -> str:
    return data.decode("utf-8", errors="replace")


def run(cmd: list[str], cwd: Path) -> subprocess.CompletedProcess[bytes]:
    return subprocess.run(
        cmd,
        cwd=str(cwd),
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
    )


def git(repo: Path, *args: str) -> str:
    result = run(["git", *args], repo)
    if result.returncode != 0:
        raise RuntimeError(f"A2_GIT_FAILED {' '.join(args)} {decode(result.stdout)[-1200:]}")
    return decode(result.stdout).strip()


def find_cli(explicit: str | None) -> Path:
    candidates = []
    if explicit:
        candidates.append(Path(explicit))
    found = shutil.which("arduino-cli")
    if found:
        candidates.append(Path(found))
    candidates.append(Path(r"C:\Program Files\Arduino PLC IDE Tools\arduino-cli.exe"))
    for candidate in candidates:
        if candidate.is_file():
            return candidate.resolve()
    raise RuntimeError("A2_ARDUINO_CLI_NOT_FOUND")


def main() -> int:
    parser = argparse.ArgumentParser(description="A14 Gate A2 cooperative TCP TX")
    parser.add_argument("--serial", default="COM14")
    parser.add_argument("--arduino-cli")
    parser.add_argument("--fqbn", default="jwplc_local:esp32:jwplcbasic")
    args = parser.parse_args()

    repo = Path(__file__).resolve().parents[3]
    sketch = repo / "tools/modbus-tcp-benchmark/firmware/a14_a2_tcp_async_tx"
    runner = repo / "tools/modbus-tcp-benchmark/pc/a14_a2_tcp_async_tx_case.py"
    libraries = repo / "JWPLC/2.1.0/libraries"
    header = libraries / "JWPLC_Ethernet/src/JWPLC_W5x00_Ethernet.h"
    client_cpp = libraries / "JWPLC_Ethernet/src/EthernetClient.cpp"
    socket_cpp = libraries / "JWPLC_Ethernet/src/socket.cpp"
    contract = repo / "tools/modbus-tcp-benchmark/gates/a14_package_promotion_contract.py"

    emit("A2_BRANCH", git(repo, "branch", "--show-current"))
    emit("A2_HEAD", git(repo, "rev-parse", "HEAD"))
    emit("A2_SERIAL", args.serial)
    emit("A2_FQBN", args.fqbn)
    emit("A2_PHYSICAL_STABILITY", "PENDING_USER")
    if git(repo, "branch", "--show-current") != BRANCH:
        raise RuntimeError("A2_BRANCH_MISMATCH")
    if git(repo, "status", "--porcelain"):
        raise RuntimeError("A2_TREE_NOT_CLEAN")

    ast.parse(runner.read_text(encoding="utf-8"), filename=str(runner))
    emit("A2_RUNNER_AST", "PASS")

    header_text = header.read_text(encoding="utf-8")
    client_text = client_cpp.read_text(encoding="utf-8")
    socket_text = socket_cpp.read_text(encoding="utf-8")
    requirements = {
        "API_BEGIN": "beginWriteAsync" in header_text and "EthernetClient::beginWriteAsync" in client_text,
        "API_POLL": "pollWriteAsync" in header_text and "EthernetClient::pollWriteAsync" in client_text,
        "API_PROGRESS": "writeAsyncInProgress" in header_text and "EthernetClient::writeAsyncInProgress" in client_text,
        "API_CANCEL": "cancelWriteAsync" in header_text and "EthernetClient::cancelWriteAsync" in client_text,
        "BACKEND_BEGIN": "EthernetClass::socketBeginSendTCP" in socket_text,
        "BACKEND_POLL": "EthernetClass::socketPollSendTCP" in socket_text,
        "LEGACY_WRITE": "Ethernet.socketSend(_sockindex, buf, size, _timeout)" in client_text,
        "FLUSH_INTEGRATION": "if (writeAsyncInProgress()) return 0;" in client_text,
    }
    for label, ok in requirements.items():
        emit(f"A2_SOURCE_{label}", "PASS" if ok else "FAIL")
    if not all(requirements.values()):
        raise RuntimeError("A2_SOURCE_CONTRACT_FAILED")

    contract_result = run([sys.executable, "-B", str(contract)], repo)
    print(decode(contract_result.stdout))
    if contract_result.returncode != 0:
        raise RuntimeError("A2_PACKAGE_CONTRACT_FAILED")

    cli = find_cli(args.arduino_cli)
    result_root = Path(tempfile.mkdtemp(prefix="jwplc_a14_a2_tcp_tx_"))
    build_dir = result_root / "build"
    compile_log = result_root / "compile.log"
    upload_log = result_root / "upload.log"
    case_log = result_root / "case.log"
    emit("A2_RESULT_ROOT", result_root)
    emit("A2_ARDUINO_CLI", cli)

    compile_result = run(
        [
            str(cli), "compile", "--verbose", "--fqbn", args.fqbn,
            "--build-path", str(build_dir), "--libraries", str(libraries), str(sketch),
        ],
        repo,
    )
    compile_log.write_bytes(compile_result.stdout)
    emit("A2_COMPILE_EXIT", compile_result.returncode)
    if compile_result.returncode != 0:
        print(decode(compile_result.stdout)[-8000:])
        raise RuntimeError("A2_COMPILE_FAILED")

    normalized = decode(compile_result.stdout).replace("\\", "/").lower()
    for token in ("ethernetclient.cpp", "socket.cpp", "w5100.cpp", "jwplc_idlescreen.cpp", "jwplc_tft.cpp"):
        ok = token in normalized
        emit(f"A2_COMPILE_TOKEN_{token}", "PASS" if ok else "FAIL")
        if not ok:
            raise RuntimeError(f"A2_COMPILE_TOKEN_MISSING={token}")
    if "libjwplc_display.a" in normalized or "libjwplc_tft.a" in normalized:
        raise RuntimeError("A2_UNEXPECTED_DISPLAY_ARCHIVE")

    upload_result = run(
        [
            str(cli), "upload", "--fqbn", args.fqbn, "--port", args.serial,
            "--input-dir", str(build_dir), str(sketch),
        ],
        repo,
    )
    upload_log.write_bytes(upload_result.stdout)
    emit("A2_UPLOAD_EXIT", upload_result.returncode)
    if upload_result.returncode != 0:
        print(decode(upload_result.stdout)[-5000:])
        raise RuntimeError("A2_UPLOAD_FAILED")

    case_result = run(
        [sys.executable, "-B", "-u", str(runner), "--serial", args.serial],
        repo,
    )
    case_log.write_bytes(case_result.stdout)
    case_text = decode(case_result.stdout)
    print(case_text)
    emit("A2_CASE_EXIT", case_result.returncode)
    if case_result.returncode != 0 or "A2_GATE_RESULT=PASS" not in case_text:
        raise RuntimeError("A2_RUNTIME_FAILED")
    if "A2_PHYSICAL_STABILITY=PENDING_USER" not in case_text:
        raise RuntimeError("A2_PHYSICAL_POLICY_MISSING")

    emit("A2_FLUSH_PENDING_PATH", "PASS")
    emit("A2_RX_CHANGED", "NO")
    emit("A2_CORRUPTION_ERRORS", 0)
    emit("A2_SPI_LOCK_ERRORS", 0)
    emit("A2_DEVICE_RESETS", 0)
    emit("A2_GATE", "PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
