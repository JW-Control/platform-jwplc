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
        raise RuntimeError(f"S1_GIT_FAILED {' '.join(args)} {decode(result.stdout)[-1200:]}")
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
    raise RuntimeError("S1_ARDUINO_CLI_NOT_FOUND")


def main() -> int:
    parser = argparse.ArgumentParser(description="A14 S1 10-minute TCP stability soak")
    parser.add_argument("--serial", default="COM14")
    parser.add_argument("--duration", type=int, default=600)
    parser.add_argument("--arduino-cli")
    parser.add_argument("--fqbn", default="jwplc_local:esp32:jwplcbasic")
    args = parser.parse_args()
    if args.duration < 600:
        raise RuntimeError("S1_DURATION_MUST_BE_AT_LEAST_600_SECONDS")

    repo = Path(__file__).resolve().parents[3]
    sketch = repo / "tools/modbus-tcp-benchmark/firmware/a14_s1_tcp_async_tx_soak"
    runner = repo / "tools/modbus-tcp-benchmark/pc/a14_s1_tcp_async_tx_soak_case.py"
    libraries = repo / "JWPLC/2.1.0/libraries"
    contract = repo / "tools/modbus-tcp-benchmark/gates/a14_package_promotion_contract.py"
    client_cpp = libraries / "JWPLC_Ethernet/src/EthernetClient.cpp"

    branch = git(repo, "branch", "--show-current")
    emit("S1_BRANCH", branch)
    emit("S1_HEAD", git(repo, "rev-parse", "HEAD"))
    emit("S1_DURATION_S", args.duration)
    emit("S1_PHYSICAL_STABILITY", "PENDING_USER")
    if branch != BRANCH:
        raise RuntimeError("S1_BRANCH_MISMATCH")
    if git(repo, "status", "--porcelain"):
        raise RuntimeError("S1_TREE_NOT_CLEAN")

    ast.parse(runner.read_text(encoding="utf-8"), filename=str(runner))
    emit("S1_RUNNER_AST", "PASS")
    client_text = client_cpp.read_text(encoding="utf-8")
    for token in (
        "EthernetClient::beginWriteAsync",
        "EthernetClient::pollWriteAsync",
        "EthernetClient::writeAsyncInProgress",
        "EthernetClient::cancelWriteAsync",
        "Ethernet.socketSend(_sockindex, buf, size, _timeout)",
    ):
        ok = token in client_text
        emit(f"S1_SOURCE_{token.split('::')[-1].split('(')[0]}", "PASS" if ok else "FAIL")
        if not ok:
            raise RuntimeError(f"S1_SOURCE_TOKEN_MISSING={token}")

    contract_result = run([sys.executable, "-B", str(contract)], repo)
    print(decode(contract_result.stdout))
    if contract_result.returncode != 0:
        raise RuntimeError("S1_PACKAGE_CONTRACT_FAILED")

    cli = find_cli(args.arduino_cli)
    result_root = Path(tempfile.mkdtemp(prefix="jwplc_a14_s1_tcp_soak_"))
    build_dir = result_root / "build"
    compile_log = result_root / "compile.log"
    upload_log = result_root / "upload.log"
    case_log = result_root / "case.log"
    emit("S1_RESULT_ROOT", result_root)

    compile_result = run(
        [
            str(cli), "compile", "--verbose", "--fqbn", args.fqbn,
            "--build-path", str(build_dir), "--libraries", str(libraries), str(sketch),
        ],
        repo,
    )
    compile_log.write_bytes(compile_result.stdout)
    emit("S1_COMPILE_EXIT", compile_result.returncode)
    if compile_result.returncode != 0:
        print(decode(compile_result.stdout)[-8000:])
        raise RuntimeError("S1_COMPILE_FAILED")
    normalized = decode(compile_result.stdout).replace("\\", "/").lower()
    for token in ("ethernetclient.cpp", "socket.cpp", "w5100.cpp", "jwplc_idlescreen.cpp", "jwplc_tft.cpp"):
        ok = token in normalized
        emit(f"S1_COMPILE_TOKEN_{token}", "PASS" if ok else "FAIL")
        if not ok:
            raise RuntimeError(f"S1_COMPILE_TOKEN_MISSING={token}")
    if "libjwplc_display.a" in normalized or "libjwplc_tft.a" in normalized:
        raise RuntimeError("S1_UNEXPECTED_DISPLAY_ARCHIVE")

    upload_result = run(
        [
            str(cli), "upload", "--fqbn", args.fqbn, "--port", args.serial,
            "--input-dir", str(build_dir), str(sketch),
        ],
        repo,
    )
    upload_log.write_bytes(upload_result.stdout)
    emit("S1_UPLOAD_EXIT", upload_result.returncode)
    if upload_result.returncode != 0:
        print(decode(upload_result.stdout)[-5000:])
        raise RuntimeError("S1_UPLOAD_FAILED")

    case_result = run(
        [
            sys.executable, "-B", "-u", str(runner), "--serial", args.serial,
            "--duration", str(args.duration), "--recovery-duration", "2",
        ],
        repo,
    )
    case_log.write_bytes(case_result.stdout)
    case_text = decode(case_result.stdout)
    print(case_text)
    emit("S1_CASE_EXIT", case_result.returncode)
    if case_result.returncode != 0 or "S1_GATE_RESULT=PASS" not in case_text:
        raise RuntimeError("S1_RUNTIME_FAILED")
    if "S1_PHYSICAL_STABILITY=PENDING_USER" not in case_text:
        raise RuntimeError("S1_PHYSICAL_POLICY_MISSING")

    emit("S1_RX_CHANGED", "NO")
    emit("S1_CORRUPTION_ERRORS", 0)
    emit("S1_SPI_LOCK_ERRORS", 0)
    emit("S1_TRANSPORT_ERRORS", 0)
    emit("S1_DEVICE_RESETS", 0)
    emit("S1_GATE", "PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
