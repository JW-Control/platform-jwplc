#!/usr/bin/env python3
"""Alpha13 G10: promoción funcional G10-I a una rama *aislada*, nunca al base.

Se ejecuta con --apply exclusivamente bajo CI de la rama de PR autorizada.
Todos los guards se evalúan antes de escribir. No modifica archivos de
laboratorio G10-R ni w5100.cpp (F1), ni bootloader, autoload o archivos .a.
"""
from __future__ import annotations
import argparse
import hashlib
from pathlib import Path
import subprocess
import sys

ROOT=Path(__file__).resolve().parents[3]
BASE="0344ff9cbe42e9efa65e7d4caabb60ce15464270"
F1_BASE="2b5802720df552e01a0b4f9d4fe1a7bd2de16daf"
BRANCH="codex/alpha13-g10-promote-functional-20261019"
PFX="JWPLC/2.1.0/libraries/"
ETH=PFX+"JWPLC_Ethernet/src/"
MB=PFX+"JWPLC_ModbusTCP/src/"
NEW={ETH+"JWPLC_W5500_SafeAccess.h",ETH+"JWPLC_W5500_SafeAccess.cpp"}
UNCHANGED={ETH+"Dns.cpp",ETH+"Dhcp.cpp"}
PROTECTED={
    ETH+"utility/w5100.cpp",
    "JWPLC/2.1.0/boards.txt",
    "JWPLC/2.1.0/platform.txt",
}
EXPECTED={
    ETH+"JWPLC_W5x00_Ethernet.h",
    ETH+"EthernetClient.cpp", ETH+"EthernetServer.cpp",
    ETH+"EthernetUdp.cpp", ETH+"socket.cpp", ETH+"JWPLC_Ethernet.cpp",
    *UNCHANGED, *NEW,
    MB+"JWPLC_ModbusTCP.cpp",MB+"JWPLC_ModbusTCP_Client.cpp",
    MB+"JWPLC_ModbusTCP.h",MB+"JWPLC_ModbusTCP_Client.h",
}
def stop(reason):
    raise RuntimeError("G10_PROMO_REVIEW_STOP_"+reason)

def git(*args):
    p=subprocess.run(["git","-C",str(ROOT),*args],capture_output=True)
    if p.returncode: stop("GIT_"+str(args[:2])+"_"+p.stderr.decode(errors="replace")[-240:])
    return p.stdout

def canonical(path):
    return path.read_bytes().replace(b"\r\n",b"\n")

def h(data):
    return hashlib.sha256(data).hexdigest()

def main():
    parser=argparse.ArgumentParser()
    parser.add_argument("--apply",action="store_true")
    args=parser.parse_args()
    git("merge-base","--is-ancestor",BASE,"HEAD")
    if git("status","--porcelain").strip(): stop("DIRTY_CHECKOUT")
    if git("branch","--show-current").decode().strip()!=BRANCH:
        stop("WRONG_BRANCH")
    if git("diff","--name-only",BASE,"HEAD","--","JWPLC/").strip():
        stop("PRODUCT_ALREADY_MODIFIED_FROM_BASE")
    from a13_g10_integrated import compose
    files=compose()
    if set(files)!=EXPECTED: stop("COMPOSE_DESTINATION_DRIFT")
    if PROTECTED.intersection(files):stop("PROTECTED_FILES_IN_COMPOSITOR")
    for rel in PROTECTED:
        path=ROOT/rel
        if path.is_file() and canonical(path)!=git("show",F1_BASE+":"+rel):
            stop("F1_OR_CONFIG_CHANGED_"+rel)
    # Cinco archives congelados: ninguno se toca ni se escribe.
    archives=list((ROOT/"JWPLC/2.1.0").rglob("*.a"))
    if len(archives)<5: stop("FROZEN_ARCHIVES_MISSING")
    frozen={str(f.relative_to(ROOT)):h(f.read_bytes()) for f in archives}
    actions=[]
    for rel,source in sorted(files.items()):
        if not isinstance(source,str):stop("COMPOSE_NON_TEXT_"+rel)
        target=ROOT/rel
        new=source.encode("utf-8")
        if rel in NEW:
            if target.exists():stop("NEW_ALREADY_EXISTS_"+rel)
            candidate=ROOT/"tools/alpha13/candidates/g10_p1g_c"/target.name
            if not candidate.is_file() or canonical(candidate).decode("utf-8-sig")!=source:
                stop("SAFEACCESS_PROVENANCE_DRIFT_"+rel)
            actions.append(("ADDED",rel,new))
        else:
            if not target.is_file():stop("TARGET_NOT_FOUND_"+rel)
            old=canonical(target)
            if old!=git("show",BASE+":"+rel):
                stop("TARGET_SOURCE_DRIFT_"+rel)
            if rel in UNCHANGED:
                if old!=new:stop("DNS_DHCP_NOT_IDENTICAL_"+rel)
                continue
            if old==new:stop("EXPECTED_MODIFIED_SOURCE_UNCHANGED_"+rel)
            actions.append(("MODIFIED",rel,new))
    if len(actions)!=12 or len([x for x in actions if x[0]=="ADDED"])!=2:
        stop("EXPECTED_10_MODIFIED_2_ADDED")
    print("G10_PROMO_SOURCE_FIRST=PASS",flush=True)
    print("G10_PROMO_FILES_MODIFIED=10",flush=True)
    print("G10_PROMO_FILES_ADDED=2",flush=True)
    print("G10_PROMO_F1_CS=BYTE_IDENTICAL_TO_F1_BASE",flush=True)
    print("G10_PROMO_ARCHIVES_FROZEN="+str(len(frozen)),flush=True)
    if not args.apply:
        print("G10_PROMO_DRY_RUN=PASS_NO_WRITES",flush=True)
        return
    for _,rel,data in actions:
        target=ROOT/rel
        target.parent.mkdir(parents=True,exist_ok=True)
        target.write_bytes(data)
    for path,digest in frozen.items():
        if h((ROOT/path).read_bytes())!=digest:stop("FROZEN_ARCHIVE_CHANGED")
    if canonical(ROOT/(ETH+"utility/w5100.cpp"))!=git("show",F1_BASE+":"+ETH+"utility/w5100.cpp"):
        stop("POST_F1_W5100_DRIFT")
    changed=set(git("status","--porcelain","--","JWPLC/").decode().splitlines())
    changed_paths={x[3:].replace("\\","/") for x in changed}
    if changed_paths!={x[1] for x in actions}: stop("POST_CHANGED_FILE_SET_MISMATCH")
    for _,rel,data in actions:
        if h((ROOT/rel).read_bytes())!=h(data):stop("POST_WRITE_SHA_DRIFT_"+rel)
    print("G10_PROMO_APPLIED=PASS_12_FUNCTIONAL_PATHS",flush=True)
    print("G10_PROMO_NO_PHYSICAL_TEST=YES",flush=True)
if __name__=="__main__":
    try:main()
    except (RuntimeError,OSError,UnicodeError) as exc:
        print(str(exc),file=sys.stderr)
        sys.exit(2)
