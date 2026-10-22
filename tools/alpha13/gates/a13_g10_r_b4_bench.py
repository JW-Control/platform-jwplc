#!/usr/bin/env python3
"""Alpha13 PRE-H B4: validación LOCAL de respaldo y banco; NO accede a hardware.

--template crea una ficha vacía que SIEMPRE falla --check hasta completar
verificaciones reales del operador. El resultado no autoriza upload/captura.
No hace lecturas de COM, red, flash ni mutaciones del repositorio.
"""
from __future__ import annotations
import argparse
import copy
import hashlib
import json
from pathlib import Path
import tempfile

SCHEMA="JWPLC-A13-G10R-BENCH-BACKUP-V1"
VID=0x1A86
PID=0x7523
ROLES={"COM4":"DIAGNOSTIC_MASTER","COM14":"MODBUS_RTU_SLAVE"}
CHECKS=(
    "plcs_bench_only_not_installed",
    "q_relays_no_loads_and_no_mains",
    "isolated_router_lan_no_industrial_network",
    "power_supplies_verified_and_safe_disconnect_ready",
    "rs485_bench_link_only_verified",
    "com4_rj45_cable_identified",
    "no_gpio4_reset_or_power_cycle_in_campaign",
    "prior_firmware_restore_procedure_prepared_not_executed",
    "operator_accepts_full_flash_backup_required",
    "serial_monitors_closed_before_upload",
)
SHA256LEN=64
MIN_FLASH=2*1024*1024
MAX_FLASH=32*1024*1024

def template()->dict:
    return {
        "schema":SCHEMA,
        "purpose":"BENCH_READINESS_NOT_AUTHORIZATION",
        "devices":{port:{
            "role":role,"usb_vid":VID,"usb_pid":PID,
            "usb_identity_fingerprint":"",
            "flash_size_detected_bytes":0,
            "backup_full_flash_path":"",
            "backup_sha256":"",
            "backup_read_verified":False,
            "restoration_command_reviewed":False,
        } for port,role in ROLES.items()},
        "operator_checks":{key:False for key in CHECKS},
        "operator_note":"Completar LOCALMENTE; no subir este JSON ni imágenes de flash a GitHub",
        "flash_consent":"NOT_GIVEN",
        "capture_and_rj45_consent":"NOT_GIVEN",
        "power_cycle_consent":"NOT_GIVEN",
    }

def sha256(path:Path)->str:
    h=hashlib.sha256()
    with path.open("rb") as f:
        for chunk in iter(lambda:f.read(1024*1024),b""):
            h.update(chunk)
    return h.hexdigest()

def validate(document:dict, *, verify_files:bool=True)->list[str]:
    issues=[]
    if not isinstance(document,dict) or document.get("schema")!=SCHEMA:
        return ["B4_SCHEMA_INVALID"]
    if document.get("purpose")!="BENCH_READINESS_NOT_AUTHORIZATION":
        issues.append("B4_PURPOSE_INVALID")
    if document.get("flash_consent")!="NOT_GIVEN" or document.get("capture_and_rj45_consent")!="NOT_GIVEN" or document.get("power_cycle_consent")!="NOT_GIVEN":
        issues.append("B4_CONSENT_MUST_BE_SEPARATE_LATER")
    checks=document.get("operator_checks")
    if not isinstance(checks,dict) or set(checks)!=set(CHECKS):
        issues.append("B4_CHECKLIST_INCOMPLETE")
    elif any(checks[key] is not True for key in CHECKS):
        issues.append("B4_PHYSICAL_BENCH_UNCONFIRMED")
    devices=document.get("devices")
    if not isinstance(devices,dict) or set(devices)!=set(ROLES):
        return issues+["B4_TWO_DISTINCT_PORT_ROLES_REQUIRED"]
    fingerprints=[]
    backup_paths=[]
    for port,role in ROLES.items():
        d=devices[port]
        if not isinstance(d,dict):
            issues.append("B4_DEVICE_INVALID_"+port);continue
        if d.get("role")!=role or type(d.get("usb_vid")) is not int or d["usb_vid"]!=VID or type(d.get("usb_pid")) is not int or d["usb_pid"]!=PID:
            issues.append("B4_ROLE_USB_ID_MISMATCH_"+port)
        fingerprint=d.get("usb_identity_fingerprint")
        if not isinstance(fingerprint,str) or len(fingerprint.strip())<8:
            issues.append("B4_USB_FINGERPRINT_MISSING_"+port)
        else:fingerprints.append(fingerprint.strip())
        size=d.get("flash_size_detected_bytes")
        if type(size) is not int or size<MIN_FLASH or size>MAX_FLASH or (size&(size-1))!=0:
            issues.append("B4_DETECTED_FLASH_SIZE_INVALID_"+port)
        filename=d.get("backup_full_flash_path")
        expected=d.get("backup_sha256")
        if not isinstance(filename,str) or not filename.strip():
            issues.append("B4_BACKUP_PATH_MISSING_"+port);continue
        path=Path(filename).expanduser().resolve()
        backup_paths.append(str(path))
        if not isinstance(expected,str) or len(expected)!=SHA256LEN or any(c not in "0123456789abcdef" for c in expected):
            issues.append("B4_BACKUP_SHA_INVALID_"+port)
        if d.get("backup_read_verified") is not True or d.get("restoration_command_reviewed") is not True:
            issues.append("B4_BACKUP_AND_RECOVERY_NOT_ATTESTED_"+port)
        if verify_files:
            if not path.is_file():
                issues.append("B4_BACKUP_NOT_FOUND_"+port)
            else:
                if type(size) is int and path.stat().st_size!=size:
                    issues.append("B4_FULL_FLASH_SIZE_MISMATCH_"+port)
                if isinstance(expected,str) and len(expected)==SHA256LEN and sha256(path)!=expected:
                    issues.append("B4_FULL_FLASH_HASH_MISMATCH_"+port)
    if len(fingerprints)!=2 or len(set(fingerprints))!=2:
        issues.append("B4_SAME_OR_UNKNOWN_USB_DEVICE")
    if len(backup_paths)!=2 or len(set(backup_paths))!=2:
        issues.append("B4_BACKUPS_NOT_DISTINCT")
    return sorted(set(issues))

def selftest()->None:
    with tempfile.TemporaryDirectory(prefix="jwplc-b4-synthetic-") as folder:
        p=template()
        assert validate(p), "EMPTY_TEMPLATE_ACCEPTED"
        for i,port in enumerate(ROLES):
            path=Path(folder)/("mock_"+port+".bin")
            path.write_bytes(bytes([i+1])*(2*1024*1024))
            p["devices"][port].update(
                usb_identity_fingerprint="MOCK_PNP_ID_"+port,
                flash_size_detected_bytes=2*1024*1024,
                backup_full_flash_path=str(path),
                backup_sha256=sha256(path),
                backup_read_verified=True,
                restoration_command_reviewed=True)
        p["operator_checks"]={key:True for key in CHECKS}
        assert not validate(p),"SYNTHETIC_BASELINE_FAILED"
        mutations=[
            ("missing_backup",lambda q:q["devices"]["COM4"].update(backup_full_flash_path="")),
            ("wrong_hash",lambda q:q["devices"]["COM4"].update(backup_sha256="0"*64)),
            ("wrong_flash_size",lambda q:q["devices"]["COM4"].update(flash_size_detected_bytes=8*1024*1024)),
            ("same_usb",lambda q:q["devices"]["COM14"].update(usb_identity_fingerprint="MOCK_PNP_ID_COM4")),
            ("same_backup",lambda q:q["devices"]["COM14"].update(backup_full_flash_path=q["devices"]["COM4"]["backup_full_flash_path"])),
            ("no_check",lambda q:q["operator_checks"].update(q_relays_no_loads_and_no_mains=False)),
            ("wrong_role",lambda q:q["devices"]["COM4"].update(role="MODBUS_RTU_SLAVE")),
            ("bad_vid",lambda q:q["devices"]["COM4"].update(usb_vid=0)),
            ("no_restore",lambda q:q["devices"]["COM14"].update(restoration_command_reviewed=False)),
            ("no_read_verification",lambda q:q["devices"]["COM4"].update(backup_read_verified=False)),
            ("wrong_consent",lambda q:q.update(flash_consent="GRANTED")),
            ("wrong_schema",lambda q:q.update(schema="UNKNOWN")),
            ("missing_port",lambda q:q["devices"].pop("COM14")),
            ("wrong_pid",lambda q:q["devices"]["COM14"].update(usb_pid=0)),
        ]
        for name,mutation in mutations:
            q=copy.deepcopy(p);mutation(q)
            assert validate(q), "NEGATIVE_NOT_REJECTED_"+name
        print("G10_R_B4_BENCH_BACKUP_CONTRACT=PASS_SYNTHETIC_ONLY")
        print(f"G10_R_B4_MUTATIONS={len(mutations)}_REJECTED")
        print("G10_R_B4_OPERATOR_BACKUPS=NOT_PRESENT_IN_CI")
        print("G10_R_B4_FLASH_AND_LIVE=NOT_AUTHORIZED")

def main()->int:
    ap=argparse.ArgumentParser(description=__doc__)
    group=ap.add_mutually_exclusive_group(required=True)
    group.add_argument("--selftest",action="store_true")
    group.add_argument("--template",type=Path,metavar="LOCAL_JSON")
    group.add_argument("--check",type=Path,metavar="LOCAL_JSON")
    a=ap.parse_args()
    if a.selftest:selftest();return 0
    if a.template:
        if a.template.exists():raise SystemExit("B4_WILL_NOT_OVERWRITE_EXISTING_CHECKLIST")
        a.template.parent.mkdir(parents=True,exist_ok=True)
        a.template.write_text(json.dumps(template(),indent=2,ensure_ascii=False)+"\n",encoding="utf-8")
        print("B4_TEMPLATE_CREATED_NOT_VALIDATED="+str(a.template))
        return 0
    try:
        doc=json.loads(a.check.read_text(encoding="utf-8-sig"))
        issues=validate(doc)
    except (OSError,UnicodeError,ValueError,TypeError) as exc:
        issues=["B4_UNREADABLE_"+type(exc).__name__]
    if issues:
        print("G10_R_B4_OPERATOR_PREFLIGHT=REVIEW_STOP")
        for issue in issues: print("B4_REASON="+issue)
        return 2
    print("G10_R_B4_OPERATOR_PREFLIGHT=READY_FOR_SEPARATE_AUTH_ONLY")
    print("G10_R_B4_BACKUP_VERIFIED=SIZE_AND_SHA_LOCAL_FILE")
    print("G10_R_B4_DEVICE_FLASH_CONTENT=NOT_ATTESTED_BY_THIS_SCRIPT")
    print("G10_R_B4_FLASH_CAPTURE=NOT_AUTHORIZED")
    return 0

if __name__=="__main__":
    raise SystemExit(main())
