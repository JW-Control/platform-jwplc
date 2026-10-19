#!/usr/bin/env python3
"""Alpha13 G10-P0: auditoría READ-ONLY del compositor funcional frente a F1.

No escribe en JWPLC/, no compila ni flashea. Genera manifiesto SHA256 en stdout
y opcionalmente en una ruta fuera del checkout, para adjuntar a CI.
"""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[3]
F1_BASE = "2b5802720df552e01a0b4f9d4fe1a7bd2de16daf"
PACKAGE = "JWPLC/2.1.0/libraries/"
ETH = PACKAGE + "JWPLC_Ethernet/src/"
MODBUS = PACKAGE + "JWPLC_ModbusTCP/src/"
DESTINATIONS = {
    ETH + "JWPLC_W5x00_Ethernet.h",
    ETH + "EthernetClient.cpp",
    ETH + "EthernetServer.cpp",
    ETH + "EthernetUdp.cpp",
    ETH + "socket.cpp",
    ETH + "JWPLC_Ethernet.cpp",
    ETH + "Dns.cpp",
    ETH + "Dhcp.cpp",
    ETH + "JWPLC_W5500_SafeAccess.h",
    ETH + "JWPLC_W5500_SafeAccess.cpp",
    MODBUS + "JWPLC_ModbusTCP.cpp",
    MODBUS + "JWPLC_ModbusTCP_Client.cpp",
    MODBUS + "JWPLC_ModbusTCP.h",
    MODBUS + "JWPLC_ModbusTCP_Client.h",
}
F1_SOURCE = ETH + "utility/w5100.cpp"
NEW_FILES = {
    ETH + "JWPLC_W5500_SafeAccess.h",
    ETH + "JWPLC_W5500_SafeAccess.cpp",
}
UNCHANGED_BY_DESIGN = {ETH + "Dns.cpp", ETH + "Dhcp.cpp"}


def need(ok: bool, msg: str) -> None:
    if not ok:
        raise RuntimeError("G10_P0_REVIEW_STOP_" + msg)


def git(*args: str) -> bytes:
    r = subprocess.run(["git", "-C", str(ROOT), *args], capture_output=True)
    need(r.returncode == 0, "GIT_COMMAND_" + "_".join(args[:2]) + "_" + r.stderr.decode(errors="replace")[-300:])
    return r.stdout


def sha(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def build_manifest() -> dict:
    git("merge-base", "--is-ancestor", F1_BASE, "HEAD")
    head = git("rev-parse", "HEAD").decode().strip()
    changed_product = git("diff", "--name-only", F1_BASE, "HEAD", "--", "JWPLC/").decode().splitlines()
    need(not changed_product, "PRODUCT_DIFF_NOT_EMPTY_" + repr(changed_product))
    need(not git("status", "--porcelain", "--", "JWPLC/").strip(), "PRODUCT_WORKTREE_DIRTY")
    need((ROOT / F1_SOURCE).is_file(), "F1_SOURCE_MISSING")
    need((ROOT / F1_SOURCE).read_bytes() == git("show", F1_BASE + ":" + F1_SOURCE), "F1_SOURCE_BYTES_CHANGED")

    # compose() lee módulos y fuentes del checkout; no llama a stage_platform()
    # ni a write_evidence(), por lo que no escribe al árbol de producto.
    from a13_g10_integrated import compose

    outputs = compose()
    need(isinstance(outputs, dict), "COMPOSITOR_RETURN_NOT_DICT")
    need(set(outputs) == DESTINATIONS,
         "DESTINATION_DRIFT_MISSING_" + repr(sorted(DESTINATIONS - set(outputs)))
         + "_EXTRA_" + repr(sorted(set(outputs) - DESTINATIONS)))

    results = []
    classes = {"ADDED": 0, "MODIFIED": 0, "UNCHANGED": 0}
    for path in sorted(outputs):
        value = outputs[path]
        need(isinstance(value, str), "OUTPUT_NOT_TEXT_" + path)
        generated = value.encode("utf-8")
        source = ROOT / path
        if path in NEW_FILES:
            need(not source.exists(), "NEW_SAFEACCESS_ALREADY_IN_PRODUCT_" + path)
            candidate = ROOT / "tools/alpha13/candidates/g10_p1g_c" / source.name
            need(candidate.is_file(), "SAFEACCESS_CANDIDATE_MISSING_" + path)
            need(candidate.read_text(encoding="utf-8-sig") == value,
                 "SAFEACCESS_SOURCE_NOT_IDENTICAL_" + path)
            source_hash = sha(candidate.read_bytes())
            status = "ADDED"
        else:
            need(source.is_file(), "PRODUCT_PATH_MISSING_" + path)
            # Los bytes originales se comparan con los del SHA F1 ya validado.
            original = source.read_bytes()
            need(original == git("show", F1_BASE + ":" + path),
                 "PRODUCT_PATH_CHANGED_SINCE_F1_" + path)
            status = "UNCHANGED" if original == generated else "MODIFIED"
            source_hash = sha(original)
        need(path not in UNCHANGED_BY_DESIGN or status == "UNCHANGED",
             "DNS_DHCP_SOURCE_WAS_MODIFIED_" + path)
        classes[status] += 1
        results.append({
            "path": path,
            "status": status,
            "source_sha256": source_hash,
            "composed_sha256": sha(generated),
            "composed_bytes": len(generated),
        })
    need(F1_SOURCE not in outputs, "F1_W5100_OVERWRITTEN")
    return {
        "result": "PASS_G10_P0_READONLY_EXACT_COMPOSE",
        "f1_product_base": F1_BASE,
        "candidate_head": head,
        "destinations": len(results),
        "existing": len(results) - classes["ADDED"],
        "new": classes["ADDED"],
        "classification": classes,
        "f1_w5100_sha256": sha((ROOT / F1_SOURCE).read_bytes()),
        "dns_dhcp_source_intact": True,
        "f1_utility_w5100_not_in_overlay": True,
        "files": results,
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", type=Path, default=None,
                        help="JSON fuera del repositorio; por defecto solo stdout")
    args = parser.parse_args()
    if args.output is not None:
        output = args.output.resolve()
        need(not output.is_relative_to(ROOT), "OUTPUT_INSIDE_REPOSITORY")
        need(output.parent.is_dir(), "OUTPUT_PARENT_MISSING")
        need(not output.exists(), "OUTPUT_ALREADY_EXISTS")
    result = build_manifest()
    encoded = json.dumps(result, ensure_ascii=False, indent=2, sort_keys=True) + "\n"
    if args.output:
        args.output.write_text(encoded, encoding="utf-8")
    print("G10_P0=" + result["result"])
    print("G10_P0_HEAD=" + result["candidate_head"])
    print("G10_P0_FILES=" + str(result["destinations"]))
    print("G10_P0_MODIFIED=" + str(result["classification"]["MODIFIED"]))
    print("G10_P0_ADDED=" + str(result["classification"]["ADDED"]))
    print("G10_P0_UNCHANGED=" + str(result["classification"]["UNCHANGED"]))
    print("G10_P0_F1_W5100=UNCHANGED_BYTE_FOR_BYTE")
    print("G10_P0_NO_FIRMWARE_WRITE=YES")
    if args.output:
        print("G10_P0_MANIFEST=" + str(args.output))
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (RuntimeError, OSError, UnicodeError) as ex:
        print(str(ex), file=sys.stderr)
        sys.exit(2)
