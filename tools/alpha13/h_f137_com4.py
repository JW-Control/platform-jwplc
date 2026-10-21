#!/usr/bin/env python3
"""Alpha13 F137: preflight de COM4, sin backup, flash ni abrir puertos.

COM4/COM14 son placas de desarrollo con firmware previo prescindible segun
la excepcion aprobada el 17/10/2026. La identidad fisica debe confirmarla
el operador; el inventario PnP no demuestra a que placa pertenece un COM.
No incluye operaciones de escritura ni lectura de la flash.
"""
from __future__ import annotations

import argparse
import hashlib
from pathlib import Path
import re
import subprocess
import sys
from types import SimpleNamespace

ROOT = Path(__file__).resolve().parents[2]
BRANCH = "codex/alpha13-precompile-benchmark-docs-20261020"
ARCHIVES = {
    "JWPLC_Ethernet": "f190bbbbd69861941807a290be653de422e11a898d1a9583d289cc573d38a0f4",
    "JWPLC_ModbusTCP": "d5456307c5e8a4d2c4fcd3252464b96859faaa4df3ece3ca23e3c720f3b25a1d",
}
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
    print("H_F137_BACKUP=NOT_REQUIRED_DEV_BOARD_WAIVER")
    print("H_F137_UPLOAD=NOT_AUTHORIZED_BY_THIS_STEP")


def fingerprint(port) -> str:
    identity = "|".join(str(x or "") for x in
                        (port.hwid, port.location, port.serial_number))
    require(bool(identity.strip("|")), "USB_FINGERPRINT_MISSING")
    return hashlib.sha256(identity.encode("utf-8")).hexdigest()


def validate_ports(devices) -> str:
    ports = {p.device.upper(): p for p in devices}
    master = ports.get("COM4")
    require(master is not None, "COM4_MISSING")
    require((master.vid, master.pid) == VID_PID, "COM4_USB_VID_PID")
    master_fingerprint = fingerprint(master)
    slave = ports.get("COM14")
    if slave is not None:
        require((slave.vid, slave.pid) == VID_PID, "COM14_USB_VID_PID")
        require(fingerprint(slave) != master_fingerprint,
                "TWO_DIFFERENT_USB_IDENTITIES_REQUIRED")
        return "COM4_AND_COM14_CH340_DISTINCT"
    return "COM4_CH340_COM14_NOT_PRESENT_NOT_REQUIRED"


def inventory() -> None:
    try:
        from serial.tools import list_ports
    except ImportError as exc:
        raise RuntimeError("H_F137_REVIEW_STOP_PYSERIAL_NOT_INSTALLED") from exc
    result = validate_ports(list_ports.comports())
    print("H_F137_USB=PASS_" + result)
    print("H_F137_ROLES=OPERATOR_PHYSICAL_CONFIRMATION_REQUIRED")
    print("H_F137_COM14=NOT_OPENED")
    print("H_F137_BACKUP=NOT_REQUIRED_DEV_BOARD_WAIVER")
    print("H_F137_UPLOAD=NOT_RUN")


def selftest() -> None:
    def sample(name, hwid, vid=VID_PID[0], pid=VID_PID[1]):
        return SimpleNamespace(device=name, hwid=hwid, location="USB_" + hwid,
                               serial_number=None, vid=vid, pid=pid)
    require(validate_ports([sample("COM4", "MASTER")]) ==
            "COM4_CH340_COM14_NOT_PRESENT_NOT_REQUIRED",
            "TEST_MASTER_ONLY")
    require(validate_ports([sample("COM4", "MASTER"),
                            sample("COM14", "SLAVE")]) ==
            "COM4_AND_COM14_CH340_DISTINCT", "TEST_TWO_IDENTITIES")
    negatives = (
        [sample("COM14", "SLAVE")],
        [sample("COM4", "MASTER", vid=0)],
        [sample("COM4", "MASTER"), sample("COM14", "MASTER")],
        [sample("COM4", "MASTER"), sample("COM14", "SLAVE", pid=0)],
    )
    for case in negatives:
        try:
            validate_ports(case)
        except RuntimeError:
            continue
        raise RuntimeError("H_F137_REVIEW_STOP_TEST_FALSE_PASS")
    print("H_F137_SELFTEST=PASS_NO_COM_OPEN_NO_BACKUP_NO_UPLOAD")
    print("H_F137_NEGATIVES=4_REJECTED")


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__)
    actions = ap.add_mutually_exclusive_group(required=True)
    actions.add_argument("--selftest", action="store_true")
    actions.add_argument("--check", action="store_true")
    actions.add_argument("--inventory", action="store_true")
    ap.add_argument("--expected-head")
    args = ap.parse_args()
    if args.selftest:
        selftest()
        return
    require(bool(args.expected_head and re.fullmatch(
        r"[0-9a-f]{40}", args.expected_head)), "EXPECTED_HEAD_REQUIRED")
    check(args.expected_head)
    if args.inventory:
        inventory()


if __name__ == "__main__":
    try:
        main()
    except (RuntimeError, OSError, ValueError, subprocess.TimeoutExpired) as exc:
        print(str(exc) if isinstance(exc, RuntimeError)
              else "H_F137_REVIEW_STOP_LOCAL_IO", file=sys.stderr)
        sys.exit(2)
