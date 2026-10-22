#!/usr/bin/env python3
"""G7C offline: candidate/source -> object -> .a -> linker -> regressions.

No USB, no upload, no FRAM write, no product adoption, no Git commit.
Reuses the tested G7 archive/linker machinery without its physical stage.
"""
from __future__ import annotations
import argparse
from datetime import datetime
import hashlib
import importlib.util
import json
from pathlib import Path
import re
import subprocess
import sys
import tempfile
import uuid

ROOT=Path(__file__).resolve().parents[3]
BRANCH="v2.1.0-alpha.13/feature/cleanup-robustness"
PRODUCT={
  "JWPLC/2.1.0/libraries/JW_FRAM/src/JW_FRAM.cpp":
  "tools/alpha13/candidates/g7c/JW_FRAM.cpp",
  "JWPLC/2.1.0/libraries/JW_FRAM/src/JW_FRAM.h":
  "tools/alpha13/candidates/g7c/JW_FRAM.h",
}
ARCHIVE="JWPLC/2.1.0/libraries/JW_FRAM/src/esp32/libJW_FRAM.a"
SOURCE_TEST=ROOT/"tools/alpha13/gates/a13_g7c_source_selftest.py"
G7=ROOT/"tools/alpha13/gates/a13_g7_integrated.py"

def say(name,value):print("G7C_"+name+"="+str(value),flush=True)
def require(ok,why):
    if not ok:raise RuntimeError(why)
def sha(p):
    h=hashlib.sha256()
    with Path(p).open("rb") as r:
        for part in iter(lambda:r.read(1<<20),b""):h.update(part)
    return h.hexdigest()
def git(*args):
    p=subprocess.run(["git",*args],cwd=ROOT,capture_output=True,text=True,
                     encoding="utf-8",errors="replace")
    require(p.returncode==0,"GIT_"+p.stderr[-350:])
    return p.stdout.strip()
def git_paths(*args):
    p=subprocess.run(["git",*args,"-z"],cwd=ROOT,
                     stdout=subprocess.PIPE,stderr=subprocess.PIPE)
    require(p.returncode==0,"GIT_STATUS_ERROR")
    return set(filter(None,p.stdout.decode("utf-8","replace").split("\0")))
def import_gate():
    spec=importlib.util.spec_from_file_location("g7c_g7_archive_engine",G7)
    require(spec is not None and spec.loader is not None,"G7_ENGINE_MISSING")
    g=importlib.util.module_from_spec(spec)
    sys.modules[spec.name]=g
    spec.loader.exec_module(g)
    return g

def preflight(g,cli):
    g.stage("PREFLIGHT","RUNNING")
    require(cli.is_absolute() and cli.is_file() and
            cli.name.lower()=="arduino-cli.exe",
            "OFFICIAL_ABSOLUTE_ARDUINO_CLI_REQUIRED")
    require("arduino plc ide tools" not in str(cli).lower(),
            "WRONG_ARDUINO_PLC_IDE_TOOLS")
    require(git("branch","--show-current")==BRANCH,"WRONG_BRANCH")
    require(Path(git("rev-parse","--show-toplevel")).resolve()==ROOT,
            "WRONG_GIT_ROOT")
    require(not git_paths("diff","--name-only"),"WORKTREE_DIRTY")
    require(not git_paths("diff","--cached","--name-only"),"STAGED_FILES")
    require(git("rev-parse","HEAD")==
            git("rev-parse","refs/remotes/origin/"+BRANCH),
            "LOCAL_REMOTE_HEAD_NOT_EQUAL")
    require(sha(cli)=="1017de89179c3167e6b8a38ca6cc4091fa69a3cf97aafa7810f01423003f5571",
            "ARDUINO_CLI_SHA_NOT_G7_PINNED")
    require(g.CORE.is_file() and g.TFT.is_file() and
            sha(g.CORE)==g.CORE_SHA and sha(g.TFT)==g.TFT_SHA,
            "CORE_OR_TFT_BASELINE_CHANGED")
    props=(g.FRAM/"library.properties").read_text(encoding="utf-8")
    require(re.search(r"(?m)^precompiled=full\s*$",props) is not None,
            "FRAM_LIBRARY_POLICY_CHANGED")
    original=[*PRODUCT,ARCHIVE]
    for path in [*PRODUCT,*PRODUCT.values(),ARCHIVE]:
        require((ROOT/path).is_file(),"MISSING_SOURCE_"+path)
    g.MANIFEST["entry_head"]=git("rev-parse","HEAD")
    g.MANIFEST["cli_path"]=str(cli)
    g.MANIFEST["cli_sha256"]=sha(cli)
    g.MANIFEST["before_sha256"]={p:sha(ROOT/p) for p in original}
    g.MANIFEST["candidate_sha256"]={p:sha(ROOT/c) for p,c in PRODUCT.items()}
    g.MANIFEST["tracked_dirty_at_start"]=0
    g.MANIFEST["product_files"]=original
    version=g.call([cli,"version"],"CLI_VERSION")
    require("Version: 1.5.1" in version,"CLI_VERSION_NOT_151")
    cores=g.call([cli,"core","list"],"CLI_CORES")
    require(re.search(r"(?m)^\s*jwplc_local:esp32\s+2\.1\.0-dev(?:\s|$)",
                      cores)is not None,"JWPLC_LOCAL_CORE_MISSING")
    say("HEAD",g.MANIFEST["entry_head"])
    say("CLI_SHA256",g.MANIFEST["cli_sha256"])
    g.stage("PREFLIGHT","PASS")

def validate_product_unchanged(g):
    before=g.MANIFEST["before_sha256"]
    require(all(sha(ROOT/p)==value for p,value in before.items()),
            "PRODUCT_MUTATED_DURING_OFFLINE")
    require(not git_paths("diff","--name-only") and
            not git_paths("diff","--cached","--name-only"),
            "GIT_WORKTREE_CHANGED_DURING_OFFLINE")
    say("PRODUCT_UNCHANGED","PASS")
    say("GIT_COMMIT_OR_PUSH","NONE")

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--arduino-cli",required=True)
    parser.add_argument("--native-cxx",default="")
    args=parser.parse_args()
    g=import_gate()
    run="g7c_"+datetime.now().strftime("%Y%m%d_%H%M%S")+"_"+uuid.uuid4().hex[:8]
    g.RUN_ID=run
    g.RESULT=ROOT/"tools/alpha13/results"/("g7c_offline_"+run)
    g.TEMP=Path(tempfile.gettempdir())/("jwplc_a13_"+run)
    require(not g.RESULT.exists() and not g.TEMP.exists(),"RUN_ID_COLLISION")
    g.RESULT.mkdir(parents=True)
    g.TEMP.mkdir(parents=True)
    g.PRODUCT=PRODUCT.copy()
    g.MANIFEST.clear()
    g.MANIFEST.update({
        "schema":"A13-G7C-OFFLINE-1","run_id":run,"status":"REVIEW",
        "phase":{},"product_changed":False,"physical_upload":False,
        "fram_writes":False,"git_committed":False,"git_pushed":False,
    })
    try:
        cli=Path(args.arduino_cli).expanduser().resolve()
        preflight(g,cli)
        g.stage("SOURCE_CONTRACT","RUNNING")
        cmd=[sys.executable,"-B",SOURCE_TEST]
        if args.native_cxx:cmd += ["--native-cxx",args.native_cxx]
        test_result=g.call(cmd,"G7C_SOURCE_SELFTEST")
        require("G7C_SOURCE_CONTRACT=PASS" in test_result,
                "CONTRACT_TEST_MISSING")
        native_pass="G7C_NATIVE_CPP=PASS_ACTUAL_EXTRACTED_METHOD" in test_result
        say("NATIVE_CPP","PASS" if native_pass else "NOT_AVAILABLE_PENDING")
        g.MANIFEST["native_cpp"]=("PASS" if native_pass else
                                   "NOT_AVAILABLE_PENDING")
        g.stage("SOURCE_CONTRACT","PASS")
        src=g.prep_source()
        pre,archive=g.build_archive(cli,src)
        g.check_precompiled(cli,pre,"CANDIDATE_LINK")
        g.regression(cli,pre,"CANDIDATE_REGRESSIONS")
        g.stage("FINAL_AUDIT","RUNNING")
        require(sha(archive)==g.MANIFEST["archive_sha256"],
                "ARCHIVE_SHA_CHANGED")
        require(sha(pre/"src/JW_FRAM.h")==g.MANIFEST["candidate_sha256"][
          "JWPLC/2.1.0/libraries/JW_FRAM/src/JW_FRAM.h"],
          "ARCHIVE_HEADER_MISMATCH")
        require(g.MANIFEST.get("candidate_regressions")==
                ["GLOBAL_FRAM","CONFIG_BLOCK"],"REGRESSIONS_MANIFEST_INVALID")
        validate_product_unchanged(g)
        g.stage("FINAL_AUDIT","PASS")
        g.MANIFEST["status"]=("CANDIDATE_BUILD_PASS_NATIVE_PASS"
                              if native_pass else
                              "CANDIDATE_BUILD_PASS_NATIVE_PENDING")
        say("RUN_ID",run)
        say("SOURCE_OBJECT_SHA256",g.MANIFEST["source_object_sha256"])
        say("CANDIDATE_ARCHIVE_SHA256",g.MANIFEST["archive_sha256"])
        say("RESULTS",g.RESULT)
        say("STATUS",g.MANIFEST["status"])
        say("PHYSICAL_UPLOAD_OR_FRAM_ACCESS","NONE")
        g.save()
        return 0
    except (Exception,KeyboardInterrupt) as exc:
        g.MANIFEST["status"]="REVIEW_STOP"
        g.MANIFEST["reason"]=repr(exc)
        say("STATUS","REVIEW_STOP")
        say("REASON",repr(exc))
        try:
            if "before_sha256" in g.MANIFEST:validate_product_unchanged(g)
        except Exception as check:
            say("PRODUCT_AUDIT_ERROR",repr(check))
        g.save()
        return 2

if __name__=="__main__":
    try:sys.exit(main())
    except Exception as e:
        say("STATUS","REVIEW_STOP")
        say("ERROR",repr(e))
        sys.exit(2)
