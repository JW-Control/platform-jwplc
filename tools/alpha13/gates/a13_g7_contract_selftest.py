#!/usr/bin/env python3
"""F118 prevention: G7 source/harness contract verifier (read-only, no hardware)."""
from __future__ import annotations
import ast
from pathlib import Path
import re
import sys

ROOT=Path(__file__).resolve().parents[3]
GATES=ROOT/"tools/alpha13/gates"
CAND=ROOT/"tools/alpha13/candidates/g7"

def must(value,label):
    if not value:
        raise RuntimeError("G7_CONTRACT_"+label)
    print("G7_CONTRACT_"+label+"=PASS",flush=True)

def main():
    paths=[GATES/"a13_g7_integrated.py",
           GATES/"a13_g7_fram_serial.py",
           GATES/"a13_g7_contract_selftest.py"]
    for p in paths:
        ast.parse(p.read_text(encoding="utf-8"),filename=str(p))
    must(True,"PYTHON_AST_ALL")
    runner=paths[0].read_text(encoding="utf-8")
    serial=paths[1].read_text(encoding="utf-8")
    cpp=(CAND/"JW_FRAM.cpp").read_text(encoding="utf-8")
    header=(CAND/"JW_FRAM.h").read_text(encoding="utf-8")
    fw=(ROOT/"tools/alpha13/firmware/a13_g7_fram_probe/a13_g7_fram_probe.ino").read_text(encoding="utf-8")
    must(fw.count("__G7_TOKEN__")==1,"UNIQUE_SERIAL_TOKEN")
    must("kBase = 0x1F00" in fw and "kWindow = 128" in fw and
         "g_nv.putBytes(\"original\"" in fw and
         "g_nv.putBool(\"pending\",true)" in fw and
         "restorePending()" in fw and
         "G7_FRAM_RESTORE_PASS" in fw,
         "FRAM_PERSISTENT_BACKUP_AND_RESTORE")
    for key in ["G7_FRAM_NULL_AND_BOUNDS_ERRORS",
                "G7_FRAM_VALID_BLOCK_ERRORS",
                "G7_FRAM_CORRUPT_BLOCK_ERRORS",
                "G7_FRAM_VERSION_ERRORS",
                "G7_FRAM_STALE_RECOVERY",
                "G7_FRAM_RESTORE_PASS",
                "G7_FRAM_TOTAL_ERRORS",
                "G7_FRAM_DONE"]:
        must(key in fw and key in serial,"SERIAL_PROTOCOL_"+key)
    must("values == nullptr" in cpp and cpp.count("values == nullptr")==2,
         "SOURCE_NULL_GUARDS")
    must("uint8_t *payload = static_cast<uint8_t *>(malloc(sizeof(T)))" in header and
         header.index("if (checksum != header.checksum)",header.index("bool JW_FRAM::readBlock")) <
         header.index("memcpy(&value, payload, sizeof(T))",header.index("bool JW_FRAM::readBlock")),
         "BLOCK_PUBLISH_ONLY_AFTER_CHECKSUM")
    must('"candidate_sha256"]={p:sha(ROOT/c) for p,c in PRODUCT.items()}' in runner and
         "set(MANIFEST[\"candidate_sha256\"])==set(PRODUCT)" in runner,
         "MANIFEST_SCHEMA_PRODUCT_KEYS")
    must("ARCHIVE_MEMBER_SET_WRONG" in runner and
         "SOURCE_OBJECT_ARCHIVE_PARITY_FAILED" in runner and
         "WRONG_BUNDLED_BUSIO" in runner and
         "precompiled=full" in runner,
         "ARCHIVE_SOURCE_MEMBER_LINK")
    must('input("JWPLC en banco, FRAM disponible y sin actuadores (BANCO_FRAM): ")' in runner and
         '"AUTORIZO_COMMIT_G7"' not in runner,
         "BENCH_SAFETY_ONLY_CONFIRMATION")
    # Avoid ValueError from .index(): absence must be a labeled failure.
    # Match ACTUAL runner function arguments, not invented milestones.
    must("def main():" in runner,"MAIN_ENTRYPOINT_PRESENT")
    stages=runner.split("def main():",1)[1]
    sequence=(
        ("SOURCE_ARCHIVE", "build_archive(cli,source)"),
        ("PHYSICAL", "upload_physical(cli,pre,args.serial_port)"),
        ("PRODUCT_ADOPTION", "adopt(archive)"),
        ("PRODUCT_REGRESSIONS", 'regression(cli,FRAM,"PRODUCT_REGRESSIONS")'),
        ("FINAL_AUDIT", "audit(archive)"),
        ("AUTO_GIT_CLOSURE", "auto_git_close()"),
    )
    positions=[]
    for name,needle in sequence:
        at=stages.find(needle)
        must(at>=0,"SEQUENCE_STAGE_"+name+"_PRESENT")
        positions.append(at)
    must(positions==sorted(positions) and len(set(positions))==len(positions),
         "SAFE_EXECUTION_SEQUENCE")
    must('git("add","--",*sorted(paths))' in runner and
         '"push","origin","HEAD:refs/heads/"+BRANCH' in runner and
         'git("reset"' not in runner and 'git("clean"' not in runner,
         "EXACT_PATH_GIT_NONFORCE_NO_DESTRUCTIVE")
    print("G7_CONTRACT_SELFTEST=PASS",flush=True)

if __name__=="__main__":
    try:
        main()
    except Exception as exc:
        print("G7_CONTRACT_SELFTEST=FAIL",flush=True)
        print("G7_CONTRACT_REASON="+repr(exc),flush=True)
        sys.exit(2)
