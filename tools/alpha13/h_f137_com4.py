#!/usr/bin/env python3
"""F137: identidad/backup solo lectura COM4 (NO upload, COM14, erase ni restore).

flash_id/read_flash pueden reiniciar COM4 al entrar/salir del bootloader.
Toda evidencia fisica se guarda SOLO en la PC, no en el repositorio.
"""
from __future__ import annotations

import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import re
import subprocess
import sys
import time

ROOT = Path(__file__).resolve().parents[2]
BRANCH = "codex/alpha13-precompile-benchmark-docs-20261020"
ARCHIVES = {
    "JWPLC_Ethernet": "f190bbbbd69861941807a290be653de422e11a898d1a9583d289cc573d38a0f4",
    "JWPLC_ModbusTCP": "d5456307c5e8a4d2c4fcd3252464b96859faaa4df3ece3ca23e3c720f3b25a1d",
}
FLASH_SIZE = 0x400000
PORTS = {"COM4": "MASTER", "COM14": "SLAVE"}
CONSENT = "AUTORIZO LECTURA Y RESPALDO SOLO COM4"
VID_PID = (0x1A86, 0x7523)


def require(ok: bool, reason: str) -> None:
    if not ok:
        raise RuntimeError("H_F137_REVIEW_STOP_" + reason)


def git(*args: str) -> str:
    p = subprocess.run(["git", *args], cwd=ROOT, capture_output=True,
                       text=True, encoding="utf-8", errors="replace", timeout=25)
    require(p.returncode == 0, "GIT_" + args[0])
    return p.stdout.strip()


def digest(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            h.update(block)
    return h.hexdigest()


def check(head: str) -> None:
    require(git("rev-parse", "HEAD") == head, "HEAD_CHANGED")
    require(git("branch", "--show-current") == BRANCH, "WRONG_BRANCH")
    require(not git("status", "--porcelain", "--untracked-files=no"),
            "DIRTY_TRACKED_TREE")
    for lib, expected in ARCHIVES.items():
        base = ROOT / "JWPLC/2.1.0/libraries" / lib
        archive = base / "src/esp32" / ("lib" + lib + ".a")
        require(archive.is_file(), "ARCHIVE_MISSING_" + lib)
        require(digest(archive) == expected, "ARCHIVE_SHA_CHANGED_" + lib)
        props = (base / "library.properties").read_text(encoding="utf-8")
        require(bool(re.search(r"(?m)^precompiled=full\s*$", props)),
                "PRECOMPILED_FULL_MISSING_" + lib)
    print("H_F137_N0=PASS_HEAD_CLEAN_TWO_ARCHIVES")
    print("H_F137_UPLOAD=NOT_AUTHORIZED")


def inventory() -> dict:
    try:
        from serial.tools import list_ports
    except ImportError as ex:
        raise RuntimeError("H_F137_REVIEW_STOP_PYSERIAL_NOT_INSTALLED") from ex
    ports = {p.device.upper(): p for p in list_ports.comports()}
    result = {}
    for name, role in PORTS.items():
        p = ports.get(name)
        require(p is not None, "PORT_NOT_PRESENT_" + name)
        require((p.vid, p.pid) == VID_PID, "CH340_ID_MISMATCH_" + name)
        key = "|".join(str(x or "") for x in
                       (p.hwid, p.location, p.serial_number))
        require(bool(key.strip("|")), "USB_IDENTITY_MISSING_" + name)
        result[name] = {
            "role": role,
            "usb_sha256": hashlib.sha256(key.encode()).hexdigest(),
        }
    require(result["COM4"]["usb_sha256"] != result["COM14"]["usb_sha256"],
            "DISTINCT_USB_IDENTITY_UNPROVEN")
    print("H_F137_USB=PASS_TWO_DISTINCT_CH340_PORTS")
    print("H_F137_ROLES=OPERATOR_MUST_CONFIRM_PHYSICALLY")
    print("H_F137_COM14=NO_PORT_OPENED")
    return result


def flash_size_from(output: str) -> int:
    m = re.search(r"(?i)Detected flash size:\s*(\d+)\s*(KB|MB)", output)
    require(m is not None, "FLASH_SIZE_UNDETERMINED")
    return int(m.group(1)) * (1024 if m.group(2).upper() == "KB" else 1024**2)


def mac_sha_from(output: str) -> str:
    m = re.search(r"(?im)^\s*MAC:\s*((?:[0-9a-f]{2}:){5}[0-9a-f]{2})\b", output)
    require(m is not None, "MAC_UNDETERMINED")
    return hashlib.sha256(m.group(1).lower().encode()).hexdigest()


def esptool(*args: str, timeout: int = 900) -> str:
    p = subprocess.run([sys.executable, "-m", "esptool", "--chip", "esp32",
                        "--port", "COM4", "--baud", "115200", *args],
                       capture_output=True, text=True, errors="replace",
                       timeout=timeout)
    # NO mostrar stdout/stderr: pueden incluir identificadores de la placa.
    require(p.returncode == 0, "ESPTOOL_" + args[0] + "_EXIT_" + str(p.returncode))
    return (p.stdout or "") + "\n" + (p.stderr or "")


def backup(head: str, out: Path) -> None:
    check(head)
    usb = inventory()
    require(importlib.util.find_spec("esptool") is not None,
            "ESPTOOL_NOT_INSTALLED")
    out = out.expanduser().resolve()
    require(out != ROOT and ROOT not in out.parents, "OUTPUT_INSIDE_REPO")
    require(not out.exists(), "OUTPUT_ALREADY_EXISTS")
    print("AVISO: esptool puede reiniciar COM4. Banco aislado; Q sin cargas/tension.")
    print("Confirmar fisicamente COM4 Master, COM14 Slave, monitores serie cerrados.")
    print("No se hara upload, erase, restauracion ni acceso a COM14.")
    typed = input("Escribe '" + CONSENT + "': ").strip()
    require(typed == CONSENT, "BACKUP_CONSENT_NOT_GIVEN")
    out.mkdir(parents=True, exist_ok=False)
    image = out / "COM4_FULL_FLASH_0x400000.bin"
    try:
        probe = esptool("flash_id")
        size = flash_size_from(probe)
        require(size == FLASH_SIZE, "DETECTED_FLASH_NOT_4MB")
        mac_sha = mac_sha_from(probe)
        esptool("read_flash", "0x0", hex(size), str(image))
        require(image.is_file() and image.stat().st_size == size,
                "BACKUP_LENGTH_MISMATCH")
        sha = digest(image)
        manifest = {
            "schema": "JWPLC-A13-F137-COM4-FULL-READ-V1",
            "head": head,
            "role": "COM4_MASTER",
            "flash_size_bytes": size,
            "backup_file": image.name,
            "backup_sha256": sha,
            "mac_sha256_local_only": mac_sha,
            "usb_sha256_local_only": usb["COM4"]["usb_sha256"],
            "com14": "UNTOUCHED",
            "esptool_calls": ["flash_id", "read_flash"],
            "upload": "NOT_AUTHORIZED_NOT_EXECUTED",
            "restore": "NOT_EXECUTED",
            "timestamp_local": time.strftime("%Y-%m-%dT%H:%M:%S"),
        }
        (out / "MANIFEST_LOCAL.json").write_text(
            json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
        print("H_F137_COM4_BACKUP=PASS_FULL_FLASH_SHA256")
        print("H_F137_FLASH_SIZE_BYTES=" + str(size))
        print("H_F137_COM14=UNTOUCHED")
        print("H_F137_UPLOAD=NOT_RUN")
        print("Conservar binario y MANIFEST_LOCAL.json solo en PC del operador.")
    except BaseException:
        print("H_F137_BACKUP=REVIEW_STOP_PARTIAL_BACKUP_MAY_EXIST",
              file=sys.stderr)
        raise


def selftest() -> None:
    require(flash_size_from("Detected flash size: 4MB") == FLASH_SIZE,
            "SELFTEST_4MB")
    require(flash_size_from("Detected flash size: 2MB") != FLASH_SIZE,
            "SELFTEST_NEGATIVE")
    require(mac_sha_from("MAC: aa:bb:cc:dd:ee:ff") ==
            hashlib.sha256(b"aa:bb:cc:dd:ee:ff").hexdigest(), "SELFTEST_MAC")
    try:
        flash_size_from("Detected flash size: unknown")
    except RuntimeError:
        pass
    else:
        raise RuntimeError("H_F137_REVIEW_STOP_SELFTEST_FALSE_PASS")
    print("H_F137_SELFTEST=PASS_NO_COM_NO_FLASH_NO_UPLOAD")


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__)
    g = ap.add_mutually_exclusive_group(required=True)
    g.add_argument("--selftest", action="store_true")
    g.add_argument("--check", action="store_true")
    g.add_argument("--inventory", action="store_true")
    g.add_argument("--backup-com4", action="store_true")
    ap.add_argument("--expected-head")
    ap.add_argument("--output", type=Path)
    args = ap.parse_args()
    if args.selftest:
        selftest()
        return
    require(bool(args.expected_head and re.fullmatch(
        r"[0-9a-f]{40}", args.expected_head)), "EXPECTED_HEAD_REQUIRED")
    if args.check:
        check(args.expected_head)
    elif args.inventory:
        check(args.expected_head)
        inventory()
    else:
        require(args.output is not None, "BACKUP_OUTPUT_REQUIRED")
        backup(args.expected_head, args.output)


if __name__ == "__main__":
    try:
        main()
    except (RuntimeError, OSError, ValueError, subprocess.TimeoutExpired,
            EOFError, KeyboardInterrupt) as ex:
        print(str(ex) if isinstance(ex, RuntimeError)
              else "H_F137_REVIEW_STOP_LOCAL_OPERATOR_OR_IO", file=sys.stderr)
        sys.exit(2)
