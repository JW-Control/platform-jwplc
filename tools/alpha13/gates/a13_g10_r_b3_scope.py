#!/usr/bin/env python3
"""PRE-H B3: alcance exacto e inmovilidad del producto, fail-closed.

No abre red/COM/USB ni escribe firmware. Se invoca en CI con checkout completo.
Este contrato reemplaza la whitelist histórica desactualizada, manteniendo el
principio de rutas explícitas y el control separado de GPIO4/archives/Alpha15.
"""
from __future__ import annotations
import argparse
import ast
from pathlib import Path
import subprocess

BASE = "d01f6b2730bc0eaf2aee0fd9b1cb8eb6dafe10c7"
ALLOWED = frozenset([".github/workflows/alpha13-g10-r-preflight.yml",".github/workflows/alpha13-g10-r-source-build.yml","docs/v2.1.0-alpha.13/A13_ALPHA15_COMPAT_READONLY_PRETRANSFER_20261016.md","docs/v2.1.0-alpha.13/A13_G10_R_CAMPANA_INTEGRADA_Y_TRANSFERENCIA_ALPHA15_20261016.md","docs/v2.1.0-alpha.13/A13_G10_R_PLAN_RECUPERACION_Y_RESISTENCIA_20261016.md","docs/v2.1.0-alpha.13/ALPHA13_STATUS.md","tools/alpha13/candidates/g10_r/native_g10_r_lab_snapshot_test.cpp","tools/alpha13/gates/a13_g10_r_build_candidate.py","tools/alpha13/gates/a13_g10_r_field_runner.py","tools/alpha13/gates/a13_g10_r_instrumentation.py","tools/alpha13/gates/a13_g10_r_owner_epoch.py","tools/alpha13/gates/a13_g10_r_preflight.py","tools/alpha13/gates/a13_g10_r_r3_native.py","tools/alpha13/gates/a13_g10_r_r3_probe_contract.py","tools/alpha13/gates/a13_g10_r_r3_protocol_native.py","tools/alpha13/gates/a13_g10_r_replay.py","tools/alpha13/gates/a13_g10_r_trace.py","tools/alpha13/gates/a13_g10_r_trace_host.py","tools/alpha13/gates/a13_g10_r_trace_validate.py","tools/alpha13/sketches/g10_r_integrated/g10_r_integrated.ino",".github/workflows/alpha13-preh-b2-offline.yml",".github/workflows/alpha13-preh-tcp124-offline.yml",".github/workflows/alpha13-preh-tcp124-source-build.yml",".github/workflows/alpha13-preh-b3-integrated.yml","docs/v2.1.0-alpha.13/A13_G10_R_PREH_B2_DHCP_FAIRNESS_20261017.md","docs/v2.1.0-alpha.13/A13_G10_R_PREH_TCP124_REVIEW_20261016.md","docs/v2.1.0-alpha.13/A13_G10_R_PREH_B3_CIERRE_20261017.md","tools/alpha13/gates/a13_g10_r_b2_contract.py","tools/alpha13/gates/a13_g10_r_b2_dhcp_stage.py","tools/alpha13/gates/a13_g10_r_b3_scope.py", "tools/alpha13/gates/a13_g10_r_b3_matrix.py", "tools/alpha13/gates/a13_g10_r_b4_bench.py", ".github/workflows/alpha13-preh-b4-safety.yml", "docs/v2.1.0-alpha.13/A13_G10_R_PREH_B4_BANCO_RESPALDOS_SEGURIDAD_20261017.md","tools/alpha13/gates/a13_g10_r_tcp124_collector.py","tools/alpha13/gates/a13_g10_r_tcp124_contract.py","tools/alpha13/gates/a13_g10_r_tcp124_host.py","tools/alpha13/gates/a13_g10_r_tcp124_join.py","tools/alpha13/gates/a13_g10_r_tcp124_provenance.py","tools/alpha13/gates/a13_g10_r_tcp124_router.py","tools/alpha13/gates/a13_g10_r_tcp124_stage.py","tools/alpha13/gates/a13_g10_r_tcp124_uart_contract.py"])
GUARDS = {
    "tools/alpha13/gates/a13_g10_r_build_candidate.py":
        "R3_COMPLETE_PHYSICAL_CAMPAIGN_READY = False",
    "tools/alpha13/gates/a13_g10_r_field_runner.py":
        "R3_PHYSICAL_INJECTION_READY=False",
}

def scope_check(changed: list[str], files: dict[str,str]) -> list[str]:
    reasons=[]
    for p in changed:
        if p not in ALLOWED:
            reasons.append("UNEXPECTED_PATH:"+p)
        if p.startswith(("JWPLC/","tools/alpha15/")) or "bootloader.bin" in p:
            reasons.append("PROTECTED_PATH:"+p)
    if not changed:
        reasons.append("EMPTY_COMPARISON")
    for p, needle in GUARDS.items():
        if needle not in files.get(p, ""):
            reasons.append("PHYSICAL_GUARD_MISSING:"+p)
    return reasons

def diff_paths()->list[str]:
    process=subprocess.run(["git","diff","--name-only",BASE,"HEAD"],
       capture_output=True,text=True,check=True)
    return [x for x in process.stdout.splitlines() if x]

def check()->int:
    sources={p:Path(p).read_text(encoding="utf-8") for p in GUARDS}
    changed=diff_paths()
    violations=scope_check(changed,sources)
    if violations:
        for issue in violations: print("G10_R_PREH_B3_REVIEW_STOP="+issue)
        return 2
    print("G10_R_PREH_B3_SCOPE=PASS_EXACT_ALLOWLIST")
    print("G10_R_PREH_B3_PRODUCT_GPIO4_ARCHIVES_ALPHA15=UNCHANGED")
    print("G10_R_PREH_B3_PHYSICAL_GUARDS=FALSE_CONFIRMED")
    print("G10_R_PREH_B3_CHANGED_PATHS="+str(len(changed)))
    return 0

def selftest()->int:
    ok={p:v for p,v in GUARDS.items()}
    assert not scope_check(["tools/alpha13/gates/a13_g10_r_b2_contract.py"],ok)
    cases=[
        (["JWPLC/2.1.0/boards.txt"],ok),
        (["JWPLC/2.1.0/precompiled/libraries/x.a"],ok),
        (["tools/alpha15/anything.py"],ok),
        (["tools/alpha13/results/log.json"],ok),
        (["docs/v2.1.0-alpha.13/unknown.md"],ok),
        ([".github/workflows/unreviewed.yml"],ok),
        ([],ok),
        (["tools/alpha13/gates/a13_g10_r_b2_contract.py"],{}),
    ]
    for i,(paths,files) in enumerate(cases):
        assert scope_check(paths,files),f"NEGATIVE_{i}"
    for p in GUARDS: ast.parse(Path(p).read_text(encoding="utf-8"))
    print("G10_R_PREH_B3_SCOPE_NEGATIVES=8_REJECTED")
    print("G10_R_PREH_B3_NO_HARDWARE=PASS")
    return 0

if __name__=="__main__":
    parser=argparse.ArgumentParser()
    group=parser.add_mutually_exclusive_group(required=True)
    group.add_argument("--selftest",action="store_true")
    group.add_argument("--check",action="store_true")
    args=parser.parse_args()
    raise SystemExit(selftest() if args.selftest else check())
