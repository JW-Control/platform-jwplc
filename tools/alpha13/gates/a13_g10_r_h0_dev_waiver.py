#!/usr/bin/env python3
"""H0 desarrollo: renuncia local y explícita a BACKUP; solo dos PLC de banco.

No autoriza flash ni maneja dispositivos. Mantiene B4 completo como ruta normal.
El JSON local NO se añade al repositorio. La autorización de escritura se
solicita más tarde por consola, tras CI, build y verificación de identidad MAC.
"""
from __future__ import annotations
import argparse
import json
import re
from pathlib import Path

SCHEMA="JWPLC-A13-H0-DEV-OVERWRITE-NO-BACKUP-V1"
PORTS={"COM4":"MASTER_WITH_MICROSD","COM14":"SLAVE_WITHOUT_MICROSD"}
SIZE=4194304
ACK="ACEPTO SOBRESCRIBIR AMBOS JWPLC DE DESARROLLO SIN BACKUP"
MAC=re.compile(r"^[0-9a-f]{2}(?::[0-9a-f]{2}){5}$")

def template()->dict:
    return {
      "schema":SCHEMA,"scope":"ISOLATED_PERSONAL_DEVELOPMENT_BENCH_ONLY",
      "device_roles":{name:{"role":role,"flash_bytes":SIZE,"expected_mac":""}
                      for name,role in PORTS.items()},
      "physical_checks":{
          "devices_owned_for_development_only":False,
          "no_external_voltage_or_load_on_Q":False,
          "connected_by_usb_only_during_upload":False,
          "serial_monitors_closed":False,
          "no_requirement_to_restore_previous_firmware":False,
      },
      "accept_loss_exact_phrase":"",
      "backup_policy":"WAIVED_BY_OWNER_FOR_DEV_BENCH",
      "physical_campaign":"NOT_AUTHORIZED_BY_THIS_WAIVER"
    }

def validate(doc:dict)->list[str]:
    if not isinstance(doc,dict) or doc.get("schema")!=SCHEMA:
        return ["BAD_SCHEMA"]
    errors=[]
    if doc.get("scope")!="ISOLATED_PERSONAL_DEVELOPMENT_BENCH_ONLY":
        errors.append("WRONG_SCOPE")
    if doc.get("backup_policy")!="WAIVED_BY_OWNER_FOR_DEV_BENCH":
        errors.append("BACKUP_POLICY_CHANGED")
    if doc.get("physical_campaign")!="NOT_AUTHORIZED_BY_THIS_WAIVER":
        errors.append("NO_PERMISSION_FOR_R1_R4")
    if doc.get("accept_loss_exact_phrase")!=ACK:
        errors.append("LOSS_NOT_EXPLICITLY_ACCEPTED")
    checks=doc.get("physical_checks")
    required=set(template()["physical_checks"])
    if not isinstance(checks,dict) or set(checks)!=required or any(v is not True for v in checks.values()):
        errors.append("PHYSICAL_CHECKS_INCOMPLETE")
    devices=doc.get("device_roles")
    if not isinstance(devices,dict) or set(devices)!=set(PORTS):
        return errors+["PORTS_MUST_BE_COM4_AND_COM14"]
    seen=[]
    for port,role in PORTS.items():
        d=devices[port]
        if not isinstance(d,dict) or d.get("role")!=role or type(d.get("flash_bytes")) is not int or d["flash_bytes"]!=SIZE:
            errors.append("ROLE_OR_FLASH_MISMATCH_"+port)
            continue
        mac=d.get("expected_mac")
        if not isinstance(mac,str) or MAC.fullmatch(mac.lower()) is None:
            errors.append("MISSING_EXPECTED_MAC_"+port)
        else:seen.append(mac.lower())
    if len(seen)!=2 or len(set(seen))!=2:errors.append("MACS_NOT_DISTINCT")
    return sorted(set(errors))

def selftest()->None:
    d=template()
    assert validate(d)
    d["device_roles"]["COM4"]["expected_mac"]="01:02:03:04:05:06"
    d["device_roles"]["COM14"]["expected_mac"]="02:02:03:04:05:06"
    d["physical_checks"]={k:True for k in d["physical_checks"]}
    d["accept_loss_exact_phrase"]=ACK
    assert not validate(d),"VALID_FIXTURE_REJECTED"
    import copy
    bad=[
      lambda x:x["device_roles"]["COM14"].update(expected_mac="01:02:03:04:05:06"),
      lambda x:x["device_roles"]["COM4"].update(flash_bytes=8388608),
      lambda x:x["device_roles"]["COM14"].update(role="MASTER_WITH_MICROSD"),
      lambda x:x["physical_checks"].update(no_external_voltage_or_load_on_Q=False),
      lambda x:x.update(accept_loss_exact_phrase="YES"),
      lambda x:x.update(physical_campaign="AUTHORIZED"),
      lambda x:x.update(scope="PRODUCTION"),
      lambda x:x["device_roles"].pop("COM14"),
      lambda x:x.update(backup_policy="OPTIONAL"),
      lambda x:x["device_roles"]["COM4"].update(expected_mac=""),
    ]
    for i,mut in enumerate(bad):
        copy_d=copy.deepcopy(d);mut(copy_d)
        assert validate(copy_d),f"NEGATIVE_ACCEPTED_{i}"
    print("G10_R_H0_DEV_WAIVER=PASS_SYNTHETIC_NO_USB")
    print("G10_R_H0_DEV_WAIVER_NEGATIVES=10_REJECTED")
    print("G10_R_H0_UPLOAD=NOT_AUTHORIZED_BY_SELFTEST")
    print("G10_R_H0_R1_R4=BLOCKED")

def main()->int:
    p=argparse.ArgumentParser(description=__doc__)
    group=p.add_mutually_exclusive_group(required=True)
    group.add_argument("--selftest",action="store_true")
    group.add_argument("--template",type=Path)
    group.add_argument("--check",type=Path)
    a=p.parse_args()
    if a.selftest:selftest();return 0
    if a.template:
        if a.template.exists():raise SystemExit("H0_REFUSE_OVERWRITE_EXISTING_LOCAL_RECORD")
        a.template.parent.mkdir(parents=True,exist_ok=True)
        a.template.write_text(json.dumps(template(),indent=2,ensure_ascii=False)+"\n",encoding="utf-8")
        print("H0_DEV_WAIVER_TEMPLATE_WRITTEN_NOT_AUTHORIZED="+str(a.template))
        return 0
    try:errors=validate(json.loads(a.check.read_text(encoding="utf-8-sig")))
    except (OSError,ValueError,UnicodeError) as exc:
        errors=["WAIVER_READ_ERROR_"+type(exc).__name__]
    if errors:
        for issue in errors:print("H0_DEV_WAIVER_REVIEW_STOP="+issue)
        return 2
    print("H0_DEV_WAIVER=VALID_LOCAL_DECLARATION_NOT_UPLOAD_PERMISSION")
    print("H0_FLASH_AUTH=INTERACTIVE_CONFIRMATION_STILL_REQUIRED")
    return 0
if __name__=="__main__":raise SystemExit(main())
