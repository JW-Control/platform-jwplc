#!/usr/bin/env python3
"""Alpha13 G9-P0: inventario reproducible READ-ONLY del cleanup canonico.

A13-017 documentación activa; A13-012 NUL real en JW_SD.cpp;
A13-014 variables/ramas invariables en GlobalPeripherals.
NO altera fuentes, .a, build config, index Git, hardware, serial o uploads.
Resultados únicamente bajo tools/alpha13/results (ignorado por Git).
"""
from __future__ import annotations

import hashlib
import json
from pathlib import Path
import re
import subprocess
import sys
from datetime import datetime
from uuid import uuid4

ROOT=Path(__file__).resolve().parents[3]
BRANCH="v2.1.0-alpha.13/feature/cleanup-robustness"
G8_COMMIT="fb71d65eb2c842b3f46e6949760e693f3b73d95e"
SD="JWPLC/2.1.0/libraries/JW_SD/src/JW_SD.cpp"
SD_PROPERTIES="JWPLC/2.1.0/libraries/JW_SD/library.properties"
SD_ARCHIVE="JWPLC/2.1.0/libraries/JW_SD/src/esp32/libJW_SD.a"
GP="JWPLC/2.1.0/libraries/JWPLC_GlobalPeripherals/src/JWPLC_GlobalPeripherals.cpp"
GP_PROPERTIES="JWPLC/2.1.0/libraries/JWPLC_GlobalPeripherals/library.properties"
README="README.md"
MODBUS_TCP_H="JWPLC/2.1.0/libraries/JWPLC_ModbusTCP/src/JWPLC_ModbusTCP.h"
SD_H="JWPLC/2.1.0/libraries/JW_SD/src/JW_SD.h"
SOURCES=[SD,SD_PROPERTIES,SD_ARCHIVE,GP,GP_PROPERTIES,README,MODBUS_TCP_H,SD_H]


def emit(k,v):
    print("G9_P0_"+k+"="+str(v),flush=True)


def required(ok,reason):
    if not ok: raise RuntimeError(reason)


def git(*args):
    p=subprocess.run(["git","-c","core.quotepath=false",*args],
                     cwd=ROOT,stdout=subprocess.PIPE,stderr=subprocess.PIPE)
    required(p.returncode==0,"GIT_FAILED:"+repr(args)+":"+
             p.stderr.decode("utf-8","replace")[-200:])
    return p.stdout


def sha256_bytes(blob):
    return hashlib.sha256(blob).hexdigest()


def snapshot():
    required((ROOT/".git").exists(),"NOT_REPOSITORY_ROOT")
    required(Path(git("rev-parse","--show-toplevel").decode("utf-8").strip()).resolve()
             ==ROOT,"WRONG_ROOT")
    required(git("branch","--show-current").decode("utf-8").strip()==BRANCH,
             "WRONG_BRANCH")
    head=git("rev-parse","HEAD").decode("ascii").strip()
    remote=git("rev-parse","refs/remotes/origin/"+BRANCH).decode("ascii").strip()
    required(head==remote,"LOCAL_HEAD_ORIGIN_NOT_EQUAL")
    required(subprocess.run(["git","merge-base","--is-ancestor",G8_COMMIT,head],
                           cwd=ROOT,stdout=subprocess.DEVNULL,
                           stderr=subprocess.DEVNULL).returncode==0,
             "G8_PRODUCT_COMMIT_NOT_IN_HISTORY")
    required(not git("diff","--name-only","-z") and
             not git("diff","--cached","--name-only","-z"),
             "PRODUCT_OR_INDEX_TRACKED_DIRTY")
    tracked=set(x.decode("utf-8","surrogateescape")
                for x in git("ls-files","-z").split(b"\x00") if x)
    required(all(p in tracked for p in SOURCES),"BASELINE_FILE_UNTRACKED")
    result={
        "schema":"A13-G9-P0-READONLY-1",
        "branch":BRANCH,"head":head,"remote_head":remote,
        "g8_product_commit_ancestor":True,"tracked_worktree_clean":True,
        "files":{},"cleanup_inventory":{},"modified_product":False,
        "git_commit_or_push":False,"upload":False,"serial":False,
        "status":"BASELINE_CAPTURED_READ_ONLY",
    }
    raw={}
    for name in SOURCES:
        data=(ROOT/name).read_bytes()
        raw[name]=data
        result["files"][name]={"bytes":len(data),"sha256":sha256_bytes(data),
                               "nul_count":data.count(b"\x00")}
    sd=raw[SD]
    null_positions=[m.start() for m in re.finditer(b"\x00",sd)]
    contextual=[{"offset":pos,"surrounding_bytes_repr":repr(
        sd[max(0,pos-32):min(len(sd),pos+32)])}
        for pos in null_positions[:8]]
    embedded_literal=b"'"+b"\x00"+b"'"
    data_sd={
        "embedded_nul_count":len(null_positions),
        "embedded_nul_offsets":null_positions,
        "nul_in_quoted_character_comparison":embedded_literal in sd,
        "nul_contexts":contextual,
        "precompiled_full":b"precompiled=full" in raw[SD_PROPERTIES],
        "archive_present":len(raw[SD_ARCHIVE])>1000,
        "archive_sha256":result["files"][SD_ARCHIVE]["sha256"],
        "classification":(
            "CONFIRMED_NEEDS_SOURCE_AND_ARCHIVE_COHERENCE"
            if null_positions and embedded_literal in sd else
            "NO_EMBEDDED_NUL_CURRENT" if not null_positions
            else "REVIEW_UNEXPECTED_NUL_PATTERN"),
    }
    result["cleanup_inventory"]["A13-012"]=data_sd
    gp=raw[GP].decode("utf-8","replace")
    rtc_refs=[{"line":i,"text":line.strip()} for i,line in enumerate(
        gp.splitlines(),1) if re.search(r"\bg_rtcEnabled\b",line)]
    refresh_refs=[{"line":i,"text":line.strip()} for i,line in enumerate(
        gp.splitlines(),1) if re.search(r"\bg_buttonRefreshThrottleMs\b",line)]
    data_gp={
        "rtc_references":rtc_refs,"throttle_references":refresh_refs,
        "rtc_static_bool_true":bool(re.search(
            r"\bstatic\s+bool\s+g_rtcEnabled\s*=\s*true\s*;",gp)),
        "throttle_mutable_25ms":bool(re.search(
            r"\bstatic\s+uint32_t\s+g_buttonRefreshThrottleMs\s*=\s*25\s*;",gp)),
        "library_precompiled_full":b"precompiled=full" in raw[GP_PROPERTIES],
        "classification":"NEEDS_EQUIVALENCE_REVIEW",
    }
    result["cleanup_inventory"]["A13-014"]=data_gp
    docs={
        "readme_latest_alpha11_claim":
            "Alpha11 permanece como última versión completamente cerrada" in
            raw[README].decode("utf-8","replace"),
        "readme_alpha12_published":
            "v2.1.0-alpha.12" in raw[README].decode("utf-8","replace"),
        "modbustcp_a14_2_future_comment":
            "A14.2" in raw[MODBUS_TCP_H].decode("utf-8","replace"),
        "jw_sd_g2b_future_comment":
            "G2b: el runtime atendera el manager automaticamente." in
            raw[SD_H].decode("utf-8","replace"),
        "classification":"TEXT_CONTRACT_REVIEW_REQUIRED",
    }
    result["cleanup_inventory"]["A13-017"]=docs
    return result


def selftest():
    sample=b"_path[0] == '"+b"\x00"+b"';"
    required(sample.count(b"\x00")==1 and b"'\x00'" in sample,
             "SELFTEST_BINARY_NUL_MODEL")
    corrected=b"_path[0] == '\\0';"
    required(corrected.count(b"\x00")==0 and corrected!=sample,
             "SELFTEST_TEXT_ESCAPE_MODEL")
    required(sha256_bytes(b"x")!=sha256_bytes(b"y"),
             "SELFTEST_HASH_NOT_DISCRIMINATING")
    emit("SELFTEST","PASS_NO_REPO_MUTATION")
    return 0


def main():
    if len(sys.argv)==2 and sys.argv[1]=="--selftest":
        return selftest()
    required(len(sys.argv)==1,"USAGE_NO_ARGS_OR_SELFTEST")
    rid="g9_p0_"+datetime.now().strftime("%Y%m%d_%H%M%S")+"_"+uuid4().hex[:8]
    out=ROOT/"tools/alpha13/results"/rid
    out.mkdir(parents=True,exist_ok=False)
    m={"run_id":rid,"status":"REVIEW_STOP","no_product_mutation":True}
    try:
        m=snapshot()
        m["run_id"]=rid
        emit("HEAD",m["head"])
        emit("SOURCE_SD_NUL_COUNT",m["cleanup_inventory"]["A13-012"]["embedded_nul_count"])
        emit("SOURCE_SD_NUL_OFFSETS",m["cleanup_inventory"]["A13-012"]["embedded_nul_offsets"])
        emit("SD_ARCHIVE_SHA256",m["files"][SD_ARCHIVE]["sha256"])
        emit("GLOBAL_RTC_REFS",len(m["cleanup_inventory"]["A13-014"]["rtc_references"]))
        emit("GLOBAL_THROTTLE_REFS",len(m["cleanup_inventory"]["A13-014"]["throttle_references"]))
        emit("README_OUTDATED_ALPHA11",m["cleanup_inventory"]["A13-017"]["readme_latest_alpha11_claim"])
        emit("STATUS",m["status"])
        emit("RESULTS",out)
        return 0
    except Exception as exc:
        m["status"]="REVIEW_STOP"
        m["reason"]=repr(exc)
        emit("STATUS","REVIEW_STOP")
        emit("REASON",repr(exc))
        emit("RESULTS",out)
        return 2
    finally:
        (out/"MANIFEST.json").write_text(
            json.dumps(m,indent=2,ensure_ascii=False)+"\n",encoding="utf-8")
        (out/"SUMMARY.log").write_text("\n".join([
            "G9_P0_RUN_ID="+rid,
            "G9_P0_STATUS="+m["status"],
            "G9_P0_PRODUCT_CHANGED=NONE",
            "G9_P0_GIT_COMMIT_OR_PUSH=NONE",
            "G9_P0_SERIAL_UPLOAD=NONE",
        ])+"\n",encoding="utf-8")


if __name__=="__main__":
    raise SystemExit(main())
