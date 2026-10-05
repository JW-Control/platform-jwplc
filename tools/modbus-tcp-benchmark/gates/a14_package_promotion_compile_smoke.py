#!/usr/bin/env python3
from __future__ import annotations

import argparse
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path


def emit(key: str, value: object) -> None:
    print(f"{key}={value}")


def decode(data: bytes) -> str:
    for enc in ("utf-8-sig", "utf-8", "cp1252"):
        try:
            return data.decode(enc)
        except UnicodeDecodeError:
            pass
    return data.decode("utf-8", errors="replace")


def find_cli(explicit: str | None) -> Path:
    candidates: list[Path] = []

    if explicit:
        candidates.append(Path(explicit))

    which = shutil.which("arduino-cli")
    if which:
        candidates.append(Path(which))

    candidates.extend(
        [
            Path(r"C:\Program Files\Arduino PLC IDE Tools\arduino-cli.exe"),
            Path.home() / "AppData" / "Local" / "Arduino15" / "arduino-cli.exe",
        ]
    )

    for candidate in candidates:
        if candidate.is_file():
            return candidate.resolve()

    raise RuntimeError("A14_PACKAGE_PROMOTION_ARDUINO_CLI_NOT_FOUND")


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Alpha14 package-first promotion compile smoke"
    )
    parser.add_argument("--arduino-cli")
    parser.add_argument(
        "--fqbn",
        default="jwplc_local:esp32:jwplcbasic",
    )
    args = parser.parse_args()

    repo = Path(__file__).resolve().parents[3]
    contract = (
        repo
        / "tools"
        / "modbus-tcp-benchmark"
        / "gates"
        / "a14_package_promotion_contract.py"
    )
    sketch = (
        repo
        / "tools"
        / "modbus-tcp-benchmark"
        / "firmware"
        / "a14_package_promotion_compile_probe"
    )
    libraries = repo / "JWPLC" / "2.1.0" / "libraries"

    cli = find_cli(args.arduino_cli)
    emit("ARDUINO_CLI", cli)
    emit("FQBN", args.fqbn)
    emit("PACKAGE_LIBRARIES", libraries)

    contract_run = subprocess.run(
        [sys.executable, "-B", str(contract)],
        cwd=str(repo),
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
    )
    contract_text = decode(contract_run.stdout)
    print(contract_text)

    if contract_run.returncode != 0:
        raise RuntimeError(
            "A14_PACKAGE_PROMOTION_SOURCE_CONTRACT_FAILED"
        )

    result_root = Path(
        tempfile.mkdtemp(prefix="jwplc_a14_package_promotion_")
    )
    build = result_root / "build"
    log = result_root / "compile_verbose.log"

    cmd = [
        str(cli),
        "compile",
        "--verbose",
        "--fqbn",
        args.fqbn,
        "--build-path",
        str(build),
        "--libraries",
        str(libraries),
        str(sketch),
    ]

    proc = subprocess.run(
        cmd,
        cwd=str(repo),
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
    )
    log.write_bytes(proc.stdout)
    text = decode(proc.stdout)

    emit("COMPILE_EXIT", proc.returncode)
    emit("COMPILE_LOG", log)

    if proc.returncode != 0:
        print(text[-8000:])
        raise RuntimeError("A14_PACKAGE_PROMOTION_COMPILE_FAILED")

    normalized = text.replace("\\", "/").lower()

    requirements = {
        "DISPLAY_SOURCE_COMPILED": "jwplc_idlescreen.cpp" in normalized,
        "TFT_SOURCE_COMPILED": "jwplc_tft.cpp" in normalized,
        "ETHERNET_UDP_SOURCE_COMPILED": "ethernetudp.cpp" in normalized,
        "SOCKET_SOURCE_COMPILED": "socket.cpp" in normalized,
        "DISPLAY_PRECOMPILED_NOT_USED":
            "libjwplc_display.a" not in normalized,
        "TFT_PRECOMPILED_NOT_USED":
            "libjwplc_tft.a" not in normalized,
    }

    for label, ok in requirements.items():
        emit(label, "PASS" if ok else "FAIL")

    failed = [
        label
        for label, ok in requirements.items()
        if not ok
    ]
    if failed:
        raise RuntimeError(
            "A14_PACKAGE_PROMOTION_COMPILE_CONTRACT_FAILED="
            + ",".join(failed)
        )

    emit("PACKAGE_SOURCE_FIRST", "PASS")
    emit("FAST_UDP_LINK_PROBE", "PASS")
    emit("DISPLAY_TFT_SOURCE_BUILD", "PASS")
    emit("A14_PACKAGE_PROMOTION_COMPILE_SMOKE", "PASS")
    emit("NEXT", "RETURN_TO_CHAT_BEFORE_TCP_PROFILING")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception as exc:
        emit("A14_PACKAGE_PROMOTION_EXCEPTION", str(exc))
        raise SystemExit(1)
