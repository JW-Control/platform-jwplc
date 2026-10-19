#!/usr/bin/env python3
"""Enrutador estricto del N0 legado G10-R: F1 aprobada o contrato original.

Esta herramienta NO concede permiso genérico para editar JWPLC/.
En el PR H0-F1 permite solo las rutas declaradas y el diff exacto del
w5100.cpp; cualquier otro caso ejecuta el contrato original sin cambios.
"""
from __future__ import annotations
import argparse
import os
from pathlib import Path
import subprocess
import sys

ROOT=Path(__file__).resolve().parents[3]
BASE="a9c96273274170195498ff282decf93f0388a450"
F1_HEAD="codex/alpha13-h0-f1-ss-pin-default-20261019"
F1_BASE="v2.1.0-alpha.13/feature/cleanup-robustness"
POSTMERGE_HEAD="docs/alpha13-h0-f1-postmerge-com4-20261019"
POSTMERGE_BASE="f2c6821566fa62df744c3af8d4d70a440994e64f"
POSTMERGE_CHANGES=frozenset((
    "tools/alpha13/gates/a13_g10_r_h0_f1_scope_bridge.py",
    "docs/v2.1.0-alpha.13/ALPHA13_STATUS.md",
    "docs/v2.1.0-alpha.13/A13_H_COM4_F1_PHYSICAL_PROTOCOL_20261019.md",
))
PRODUCT="JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/utility/w5100.cpp"
ALLOW=frozenset((
    ".github/workflows/alpha13-g10-r-preflight.yml",
    ".github/workflows/alpha13-h0-e1e2.yml",
    ".github/workflows/alpha13-h0-f1-package.yml",
    PRODUCT,
    "docs/v2.1.0-alpha.13/A13_H0_F1_PACKAGE_SS_PIN_20261019.md",
    "docs/v2.1.0-alpha.13/ALPHA13_STATUS.md",
    "tools/alpha13/gates/a13_g10_r_h0_f1_scope_bridge.py",
    "tools/alpha13/gates/a13_h0_e1e2_verify.py",
    "tools/alpha13/gates/a13_h0_f1_arduino.py",
    "tools/alpha13/gates/a13_h0_f1_test.py",
))
F1_INSERT='''// JWPLC F1: el CS real del W5500 procede exclusivamente del package.
// SS del variant es static const uint8_t y no participa en #if defined(...).
#if !defined(PIN_SPI_SS_ETHERNET_LIB) && __has_include("jwplc_spi_bus.h")
#include "jwplc_spi_bus.h"
#define PIN_SPI_SS_ETHERNET_LIB JWPLC_ETH_CS
#endif

'''
F1_GUARD='''#if defined(CONFIG_IDF_TARGET_ESP32) && (SS_PIN_DEFAULT >= 6) && (SS_PIN_DEFAULT <= 11)
#error "SS_PIN_DEFAULT apunta a GPIO6-11, reservado para flash SPI en ESP32"
#endif

'''

def run(*cmd):
    p=subprocess.run(cmd,cwd=ROOT,capture_output=True,text=True,errors="replace",timeout=45)
    if p.returncode:raise RuntimeError("COMMAND_FAILED:"+str(cmd)+p.stderr[-900:])
    return p.stdout

def inspect_paths(changed):
    errors=[]
    if PRODUCT not in changed:errors.append("F1_PRODUCT_NOT_CHANGED")
    for path in sorted(changed-ALLOW):errors.append("UNEXPECTED_PATH:"+path)
    for path in changed:
        if "alpha15" in path.lower() or path.endswith(".a") or "bootloader.bin" in path.lower():
            errors.append("FORBIDDEN_PATH:"+path)
    return errors

def exact_patch(original,actual):
    anchor="// If variant.h or other headers specifically define the\n"
    end="// W5100 controller instance\n"
    if original.count(anchor)!=1 or original.count(end)!=1:
        return False
    expected=original.replace(anchor,F1_INSERT+anchor,1)
    expected=expected.replace(end,F1_GUARD+end,1)
    return expected==actual

def selftest():
    assert not inspect_paths({PRODUCT})
    assert inspect_paths({"JWPLC/2.1.0/boards.txt",PRODUCT})
    assert inspect_paths({"tools/alpha15/bad.py",PRODUCT})
    assert inspect_paths({"precompiled/fake.a",PRODUCT})
    assert inspect_paths({".github/workflows/alpha13-g10-r-preflight.yml"})
    a="// If variant.h or other headers specifically define the\n// W5100 controller instance\n"
    b=a.replace("// If variant.h",F1_INSERT+"// If variant.h").replace(
        "// W5100 controller instance",F1_GUARD+"// W5100 controller instance")
    assert exact_patch(a,b)
    assert not exact_patch(a,b+"// cambio inesperado\n")
    assert not exact_patch(a,b.replace("JWPLC_ETH_CS","10"))
    assert POSTMERGE_CHANGES == set(POSTMERGE_CHANGES)
    assert "JWPLC/2.1.0/boards.txt" not in POSTMERGE_CHANGES
    print("H0_F1_G10_LEGACY_BRIDGE_NEGATIVES=PASS_7")
    
def check():
    head=os.environ.get("G10R_PR_HEAD_REF","")
    base=os.environ.get("G10R_PR_BASE_REF","")
    event=os.environ.get("G10R_EVENT_NAME","")
    # No aceptar por simple nombre de rama sin contexto de PR real.
    if event=="pull_request" and head==F1_HEAD and base==F1_BASE:
        changed=set(run("git","diff","--name-only",BASE,"HEAD").splitlines())
        errors=inspect_paths(changed)
        if errors:raise RuntimeError("H0_F1_SCOPE_REVIEW_STOP:"+",".join(errors))
        original=run("git","show",BASE+":"+PRODUCT)
        actual=(ROOT/PRODUCT).read_text(encoding="utf-8")
        assert exact_patch(original,actual),"H0_F1_PRODUCT_DIFF_NOT_EXACT"
        assert "JWPLC_ETH_CS   5" in (
            ROOT/"JWPLC/2.1.0/cores/jwcontrol/peripherals/include/jwplc_spi_bus.h"
        ).read_text(encoding="utf-8"),"H0_F1_CS_SOURCE_DRIFT"
        print("G10_R_H0_F1_EXACT_EXCEPTION=PASS_W5100_ONLY_NO_GUARD_WEAKENING")
        return
    if event=="pull_request" and head==POSTMERGE_HEAD and base==F1_BASE:
        run("git","merge-base","--is-ancestor",POSTMERGE_BASE,"HEAD")
        changed=set(run("git","diff","--name-only",POSTMERGE_BASE,"HEAD").splitlines())
        assert changed==POSTMERGE_CHANGES, (
            "H0_F1_POSTMERGE_SCOPE_REVIEW_STOP:"+repr(sorted(changed)))
        original=run("git","show",BASE+":"+PRODUCT)
        actual=(ROOT/PRODUCT).read_text(encoding="utf-8")
        assert exact_patch(original,actual),"H0_F1_POSTMERGE_PRODUCT_DIFF_DRIFT"
        print("G10_R_H0_F1_POSTMERGE_DOCS=PASS_PRODUCT_F1_UNCHANGED_EXACT_DOCS")
        return
    # Contrato G10-R original: funciona para todos los demás PR/ramas.
    from a13_g10_r_b3_scope import check as legacy_check
    result=legacy_check()
    if result:raise RuntimeError("G10_R_LEGACY_REVIEW_STOP:"+str(result))
    print("G10_R_LEGACY_SCOPE=PASS_UNCHANGED")

def main():
    parser=argparse.ArgumentParser()
    group=parser.add_mutually_exclusive_group(required=True)
    group.add_argument("--selftest",action="store_true")
    group.add_argument("--check",action="store_true")
    a=parser.parse_args()
    if a.selftest:selftest()
    else:check()
if __name__=="__main__":main()
