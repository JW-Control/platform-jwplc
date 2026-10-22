#!/usr/bin/env python3
"""Alpha13 G10-R N0: contrato de procedencia READ-ONLY.

No hace uploads, no abre COM/USB, no accede a IP, no cambia firmware ni
ejecuta acciones físicas. El selftest crea datos SINTÉTICOS, no evidencia H.
"""
from __future__ import annotations

import argparse
import copy
import hashlib
import json
from pathlib import Path
import re
import sys

G10H_HEAD = "c776ab92388e8765e933bf50cab8215e7c18325b"
G10I_SOURCE = "a954cabd9c6e37876b59995a41ff0a449308ccd1"
CORE_SHA = "a1a985f64c22838a1987d4280b8c1dc431a42d5789c6692cbfa36dfb6252246d"
W5500_OBJECT_SHA = "817d04ea874eae60e85447f65fa3c0c4c483bd4e6465959c91a792300ed52833"
COM4_ELF_SHA = "cc2636518f4bac2823ecfb924e8c07a91a0e8c2a88431cc0542d924291ad61db"
COM14_ELF_SHA = "82b2b4d85e3113ed555699d5a15f53610c75568198a630044a3a3921c9d70890"
FQBN = "jwplc_hbench:esp32:jwplcbasic"
SOURCE_SAFEACCESS = "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/JWPLC_W5500_SafeAccess.cpp"
SHA = re.compile(r"[0-9a-f]{64}\Z")


def check_manifest(m: dict) -> list[str]:
    """Requiere un H real aprobado antes de siquiera planificar G10-R físico."""
    issues: list[str] = []
    if not isinstance(m, dict):
        return ["MANIFEST_OBJECT_REQUIRED"]

    exact = {
        "phase": "DONE",
        "physical_status": "PASS_DUAL_ETHERNET_SPI_RTU",
        "exit_code": 0,
        "head": G10H_HEAD,
        "g10_i_source_sha": G10I_SOURCE,
        "bench_fqbn": FQBN,
        "bench_vendor": "jwplc_hbench",
        "staged_platform": "TEMP_ISOLATED_NO_PRODUCT_MUTATION",
        "staged_core_sha256": CORE_SHA,
        "safeaccess_object_sha256": W5500_OBJECT_SHA,
        "elf_COM4_sha256": COM4_ELF_SHA,
        "elf_COM14_sha256": COM14_ELF_SHA,
        "flash_com4": "PASS_CONFIRMED_UPLOAD",
        "flash_com14": "PASS_CONFIRMED_UPLOAD",
        "gpio4_reset": "DISABLED",
        "product_modified": False,
        "prior_firmware_restored": False,
        "rtu_enabled": True,
        "cli_version": "1.5.1",
        "consent": "EXPLICIT_AFTER_BUILD",
    }
    for name, expected in exact.items():
        observed = m.get(name)
        if type(observed) is not type(expected) or observed != expected:
            issues.append("FIELD_" + name.upper() + "_MISMATCH")

    overlay = m.get("overlay_sources")
    if not isinstance(overlay, dict) or len(overlay) != 14:
        issues.append("OVERLAY_14_SOURCES_REQUIRED")
    else:
        if SOURCE_SAFEACCESS not in overlay:
            issues.append("SAFEACCESS_SOURCE_MISSING")
        for name, sha in overlay.items():
            if not isinstance(name, str) or not name.startswith("JWPLC/2.1.0/") or not isinstance(sha, str) or not SHA.fullmatch(sha):
                issues.append("OVERLAY_PATH_OR_SHA_INVALID")
                break

    ports = m.get("ports")
    if not isinstance(ports, dict) or set(ports) != {"COM4", "COM14"}:
        issues.append("DUAL_PORTS_REQUIRED")
    else:
        for port in ("COM4", "COM14"):
            p = ports[port]
            if not isinstance(p, dict) or p.get("port") != port or p.get("vid") != 6790 or p.get("pid") != 29987:
                issues.append("USB_IDENTITY_" + port + "_MISMATCH")

    stats = m.get("measurements")
    if not isinstance(stats, dict):
        return issues + ["MEASUREMENTS_REQUIRED"]

    for name, minimum in (("tcp_ok", 8), ("udp_ok", 8), ("heartbeat_lines", 12)):
        val = stats.get(name)
        if type(val) is not int or val < minimum:
            issues.append(name.upper() + "_BELOW_MINIMUM")
    for name in ("tcp_fail", "udp_fail"):
        val = stats.get(name)
        if type(val) is not int or val < 0:
            issues.append(name.upper() + "_COUNT_INVALID")
    hb = stats.get("last_hb")
    if not isinstance(hb, dict):
        issues.append("LAST_HEARTBEAT_MISSING")
    else:
        for name, minimum in (("HB", 12), ("SD_OK", 5), ("FRAM", 5),
                              ("UDP_RX", 8), ("UDP_TX", 8),
                              ("RTU_OK", 8), ("BTN_EVENTS", 2)):
            val = hb.get(name)
            if type(val) is not int or val < minimum:
                issues.append("HEARTBEAT_" + name + "_BELOW_MINIMUM")
        for name, expected in (("ETH", 1), ("TCP", 1), ("RTC", 1),
                               ("SD_FAIL", 0), ("RTU_FAIL", 0)):
            if type(hb.get(name)) is not int or hb[name] != expected:
                issues.append("HEARTBEAT_" + name + "_MISMATCH")
    slave = stats.get("slave_result")
    if not isinstance(slave, dict):
        issues.append("SLAVE_RESULT_MISSING")
    else:
        for name in ("REQUESTS", "RXFRAMES", "TXFRAMES"):
            if type(slave.get(name)) is not int or slave[name] < 8:
                issues.append("SLAVE_" + name + "_BELOW_MINIMUM")
        for name, val in (("CRC", 0), ("READY", 1)):
            if type(slave.get(name)) is not int or slave[name] != val:
                issues.append("SLAVE_" + name + "_MISMATCH")
    return issues


def synthetic_fixture() -> dict:
    """Fixture FICTICIO: se utiliza exclusivamente para pruebas negativas."""
    return {
        "phase": "DONE", "physical_status": "PASS_DUAL_ETHERNET_SPI_RTU",
        "exit_code": 0, "head": G10H_HEAD, "g10_i_source_sha": G10I_SOURCE,
        "bench_fqbn": FQBN, "bench_vendor": "jwplc_hbench",
        "staged_platform": "TEMP_ISOLATED_NO_PRODUCT_MUTATION",
        "staged_core_sha256": CORE_SHA, "safeaccess_object_sha256": W5500_OBJECT_SHA,
        "elf_COM4_sha256": COM4_ELF_SHA, "elf_COM14_sha256": COM14_ELF_SHA,
        "flash_com4": "PASS_CONFIRMED_UPLOAD", "flash_com14": "PASS_CONFIRMED_UPLOAD",
        "gpio4_reset": "DISABLED", "product_modified": False,
        "prior_firmware_restored": False, "rtu_enabled": True,
        "cli_version": "1.5.1", "consent": "EXPLICIT_AFTER_BUILD",
        "overlay_sources": {
            SOURCE_SAFEACCESS: "a" * 64,
            **{"JWPLC/2.1.0/libraries/mock/file%02d.cpp" % i: "b" * 64 for i in range(13)},
        },
        "ports": {
            "COM4": {"port": "COM4", "vid": 6790, "pid": 29987},
            "COM14": {"port": "COM14", "vid": 6790, "pid": 29987},
        },
        "measurements": {
            "tcp_ok": 10, "tcp_fail": 0, "udp_ok": 10, "udp_fail": 0,
            "heartbeat_lines": 12,
            "last_hb": {"HB": 12, "ETH": 1, "TCP": 1, "SD_OK": 5,
                        "SD_FAIL": 0, "RTC": 1, "FRAM": 5, "UDP_RX": 8,
                        "UDP_TX": 8, "RTU_OK": 8, "RTU_FAIL": 0,
                        "BTN_EVENTS": 2},
            "slave_result": {"REQUESTS": 8, "RXFRAMES": 8,
                             "TXFRAMES": 8, "CRC": 0, "READY": 1},
        },
    }


def selftest() -> None:
    baseline = synthetic_fixture()
    assert check_manifest(baseline) == [], "SELFTEST_BASELINE_INVALID"
    mutations = [
        ("wrong-head", lambda m: m.__setitem__("head", "0" * 40)),
        ("wrong-fqbn", lambda m: m.__setitem__("bench_fqbn", "jwplc_local:esp32:jwplcbasic")),
        ("wrong-core", lambda m: m.__setitem__("staged_core_sha256", "0" * 64)),
        ("reset-enabled", lambda m: m.__setitem__("gpio4_reset", "ENABLED")),
        ("product-modified", lambda m: m.__setitem__("product_modified", True)),
        ("stale-source", lambda m: m["overlay_sources"].pop(SOURCE_SAFEACCESS)),
        ("no-tcp", lambda m: m["measurements"].__setitem__("tcp_ok", 0)),
        ("sd-failure", lambda m: m["measurements"]["last_hb"].__setitem__("SD_FAIL", 1)),
        ("rtu-failure", lambda m: m["measurements"]["last_hb"].__setitem__("RTU_FAIL", 1)),
        ("crc-failure", lambda m: m["measurements"]["slave_result"].__setitem__("CRC", 1)),
        ("missing-port", lambda m: m["ports"].pop("COM14")),
        ("no-button", lambda m: m["measurements"]["last_hb"].__setitem__("BTN_EVENTS", 0)),
        ("invalid-elf", lambda m: m.__setitem__("elf_COM4_sha256", "0" * 64)),
    ]
    for label, mutation in mutations:
        trial = copy.deepcopy(baseline)
        mutation(trial)
        if not check_manifest(trial):
            raise AssertionError("SELFTEST_NEGATIVE_NOT_REJECTED_" + label)
    print("G10_R_CONTRACT_SELFTEST=PASS_SYNTHETIC_ONLY")
    print("G10_R_NEGATIVE_MUTATIONS=%d_PASS_REJECTED" % len(mutations))
    print("G10_R_PHYSICAL_TEST=NOT_RUN")
    print("G10_R_PRODUCT_MODIFIED=NO")


def check_file(path: Path) -> int:
    data = path.read_bytes()
    m = json.loads(data.decode("utf-8-sig"))
    failures = check_manifest(m)
    print("G10_R_N0_MANIFEST_SHA256=" + hashlib.sha256(data).hexdigest())
    if failures:
        print("G10_R_N0=REVIEW_STOP")
        for error in failures:
            print("G10_R_REASON=" + error)
        return 2
    print("G10_R_N0=PASS_G10H_MANIFEST_PROVENANCE_READONLY")
    print("G10_R_NEXT=G10_R_PLAN_APPROVAL_REQUIRED")
    print("G10_R_RECOVERY_TRAFFIC=NOT_RUN")
    print("G10_R_PHYSICAL_TEST=NOT_RUN")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    options = parser.add_mutually_exclusive_group(required=True)
    options.add_argument("--selftest", action="store_true")
    options.add_argument("--check-g10h-manifest", type=Path)
    args = parser.parse_args()
    if args.selftest:
        selftest()
        return 0
    try:
        return check_file(args.check_g10h_manifest)
    except (OSError, UnicodeError, json.JSONDecodeError, ValueError) as exc:
        print("G10_R_N0=REVIEW_STOP")
        print("G10_R_REASON=MANIFEST_UNREADABLE_" + type(exc).__name__)
        return 2


if __name__ == "__main__":
    sys.exit(main())
