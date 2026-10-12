#!/usr/bin/env python3
"""G8-D0: diagnostico de SOLO LECTURA del indice Git en Windows.

No modifica archivos de producto, index, atributos, hooks ni configuracion.
No ejecuta compilacion, adopcion, rollback, COM, upload ni Git add/commit/push.
"""
from __future__ import annotations
import hashlib
import json
from pathlib import Path
import subprocess
from datetime import datetime
from uuid import uuid4

ROOT=Path(__file__).resolve().parents[3]
BRANCH="v2.1.0-alpha.13/feature/cleanup-robustness"
TARGETS=[
    "JWPLC/2.1.0/libraries/JWPLC_ModbusRTU/src/JWPLC_ModbusRTU.cpp",
    "JWPLC/2.1.0/libraries/JWPLC_RS485/src/JWPLC_RS485.cpp",
    "JWPLC/2.1.0/libraries/JWPLC_ModbusRTU/src/esp32/libJWPLC_ModbusRTU.a",
]


def log(k,v):
    print("G8_D0_"+k+"="+str(v),flush=True)


def git(*args):
    p=subprocess.run(["git","-c","core.quotepath=false",*args],
        cwd=ROOT,stdout=subprocess.PIPE,stderr=subprocess.PIPE)
    return {
        "exit":p.returncode,
        "stdout_utf8":p.stdout.decode("utf-8","replace"),
        "stderr_utf8":p.stderr.decode("utf-8","replace"),
        "stdout_hex_prefix":p.stdout[:180].hex(),
        "stdout_bytes":len(p.stdout),
    }


def split_nul(result):
    return [x for x in result["stdout_utf8"].split("\x00") if x]


def sha(path):
    d=hashlib.sha256()
    with open(path,"rb") as f:
        for chunk in iter(lambda:f.read(1048576),b""):
            d.update(chunk)
    return d.hexdigest()


def run():
    rid="g8_d0_"+datetime.now().strftime("%Y%m%d_%H%M%S")+"_"+uuid4().hex[:8]
    folder=ROOT/"tools/alpha13/results"/rid
    folder.mkdir(parents=True,exist_ok=False)
    payload={
        "schema":"A13-G8-INDEX-DIAGNOSTIC-1",
        "run_id":rid,"status":"REVIEW_STOP",
        "product_modified_by_runner":False,
        "git_index_modified_by_runner":False,
        "upload":False,"serial":False,"fram_write":False,
        "expected_paths":TARGETS,"commands":{},
    }
    try:
        log("RUN_ID",rid)
        cmds={
            "git_version":("version",),
            "top":("rev-parse","--show-toplevel"),
            "branch":("branch","--show-current"),
            "head":("rev-parse","HEAD"),
            "origin_tracking":("rev-parse","refs/remotes/origin/"+BRANCH),
            "status":("status","--short","--untracked-files=no"),
            "staged":("diff","--cached","--name-only","-z"),
            "unstaged":("diff","--name-only","-z"),
            "index_v":("ls-files","-v","--",*TARGETS),
            "index_v_z":("ls-files","-v","-z","--",*TARGETS),
            "index_stage_z":("ls-files","-s","-z","--",*TARGETS),
            "index_paths_z":("ls-files","--full-name","-z","--",*TARGETS),
            "head_tree_z":("ls-tree","-r","--name-only","-z","HEAD","--",*TARGETS),
            "all_tracked_z":("ls-files","-z"),
        }
        for name,args in cmds.items():
            payload["commands"][name]=git(*args)
            log("COMMAND_"+name.upper()+"_EXIT",payload["commands"][name]["exit"])
        x=payload["commands"]
        raw_v=x["index_v"]["stdout_utf8"]
        parsed={}
        for row in raw_v.splitlines():
            if len(row)>=3 and row[1]==" ":
                parsed[row[2:]]=row[0]
        got_index=split_nul(x["index_paths_z"])
        got_tree=split_nul(x["head_tree_z"])
        full_list=split_nul(x["all_tracked_z"])
        payload["paths"]={}
        for i,target in enumerate(TARGETS,1):
            local=ROOT/target
            present=local.is_file()
            tree=git("rev-parse","--verify","HEAD:"+target)
            obj=git("hash-object","--",target) if present else None
            detail={
                "name":target,"exists":present,
                "tracked_exact_in_index":target in got_index,
                "tracked_exact_in_head_tree":target in got_tree,
                "flag_v":parsed.get(target,"NOT_VISIBLE_IN_PLAIN_V"),
                "index_path_casefold_matches":[s for s in full_list if s.casefold()==target.casefold()],
                "head_object":tree["stdout_utf8"].strip() if tree["exit"]==0 else None,
                "worktree_object":obj["stdout_utf8"].strip() if obj and obj["exit"]==0 else None,
                "file_sha256":sha(local) if present else None,
            }
            detail["worktree_equals_head"]=(
                detail["head_object"] is not None and
                detail["head_object"]==detail["worktree_object"])
            payload["paths"][target]=detail
            log("PATH_"+str(i)+"_INDEX",detail["tracked_exact_in_index"])
            log("PATH_"+str(i)+"_HEAD",detail["tracked_exact_in_head_tree"])
            log("PATH_"+str(i)+"_V_FLAG",detail["flag_v"])
            log("PATH_"+str(i)+"_HEAD_EQUALS_WORKTREE",detail["worktree_equals_head"])
            log("PATH_"+str(i)+"_CASEFOLD",repr(detail["index_path_casefold_matches"]))
        payload["index_v_raw"]=raw_v
        payload["index_v_z_raw"]=repr(x["index_v_z"]["stdout_utf8"])
        payload["index_v_z_hex"]=x["index_v_z"]["stdout_hex_prefix"]
        payload["index_tracked_paths"]=got_index
        payload["head_tree_paths"]=got_tree
        log("INDEX_V_RAW_REPR",repr(raw_v[:1500]))
        log("INDEX_V_Z_RAW_REPR",repr(x["index_v_z"]["stdout_utf8"][:1500]))
        log("INDEX_PATHS",repr(got_index))
        log("HEAD_TREE_PATHS",repr(got_tree))
        log("UNSTAGED_PATHS",repr(split_nul(x["unstaged"])))
        log("STAGED_PATHS",repr(split_nul(x["staged"])))
        payload["clean"]=(
            not split_nul(x["unstaged"]) and
            not split_nul(x["staged"]))
        payload["on_expected_branch"]=(
            x["branch"]["stdout_utf8"].strip()==BRANCH)
        payload["local_equals_tracking"]=(
            x["head"]["stdout_utf8"].strip()==
            x["origin_tracking"]["stdout_utf8"].strip())
        payload["all_expected_index"]=all(
            z["tracked_exact_in_index"] for z in payload["paths"].values())
        payload["all_expected_head"]=all(
            z["tracked_exact_in_head_tree"] for z in payload["paths"].values())
        payload["status"]="DIAGNOSTIC_COLLECTED_NO_MUTATION"
        log("STATUS",payload["status"])
        log("CLEAN",payload["clean"])
        log("INDEX_TRACKS_ALL",payload["all_expected_index"])
        log("RESULTS",folder)
        return 0
    except Exception as exc:
        payload["status"]="REVIEW_STOP"
        payload["error"]=repr(exc)
        log("STATUS","REVIEW_STOP")
        log("ERROR",repr(exc))
        log("RESULTS",folder)
        return 2
    finally:
        (folder/"MANIFEST.json").write_text(
            json.dumps(payload,indent=2,ensure_ascii=False)+"\n",
            encoding="utf-8")
        (folder/"SUMMARY.log").write_text(
            "\n".join([
                "G8_D0_RUN_ID="+rid,
                "G8_D0_STATUS="+payload["status"],
                "G8_D0_INDEX_V_RAW_REPR="+repr(payload.get("index_v_raw","")),
                "G8_D0_INDEX_PATHS="+repr(payload.get("index_tracked_paths",[])),
                "G8_D0_HEAD_TREE_PATHS="+repr(payload.get("head_tree_paths",[])),
                "G8_D0_CLEAN="+str(payload.get("clean","UNKNOWN")),
                "G8_D0_PRODUCT_CHANGE=NONE",
                "G8_D0_INDEX_CHANGE=NONE",
            ])+"\n",encoding="utf-8")


if __name__=="__main__":
    raise SystemExit(run())
