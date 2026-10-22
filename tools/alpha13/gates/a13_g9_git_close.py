#!/usr/bin/env python3
"""Alpha13 G9 P4: commit y push solo tres archivos productivos aprobados.

Requiere G9-P0, candidato P2/P3 y adopcion P4 PASS locales exactos.
Sin COM/upload, FRAM/microSD, Git reset/rebase/merge, PR ni release.
--verify-only valida todo sin escribir Git; BAT ejecuta --confirm A13_G9_COMMIT.
"""
from __future__ import annotations

import argparse
from datetime import datetime
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import uuid

ROOT=Path(__file__).resolve().parents[3]
BRANCH="v2.1.0-alpha.13/feature/cleanup-robustness"
BASE="54332580e560ddc0986027f782c7dfcf456c0bd0"
P0_ID="g9_p0_20261011_234245_6727f0b0"
CAND_ID="g9_p2p3_20261012_000449_23842b4f"
ADOPT_ID="g9_adopt_20261012_101458_1e395a6e"
ARCHIVE_SHA="58e86fdf31c9bf94204fcbcf7462656fd9f4d9a7d73bff35bd660dbff538b89e"
OLD_ARCHIVE="1b9619ba37295782ade1edc6f06439a38e7e534fc08f5b05757c9af7a645acc0"
SD_CPP="JWPLC/2.1.0/libraries/JW_SD/src/JW_SD.cpp"
SD_A="JWPLC/2.1.0/libraries/JW_SD/src/esp32/libJW_SD.a"
GP_CPP="JWPLC/2.1.0/libraries/JWPLC_GlobalPeripherals/src/JWPLC_GlobalPeripherals.cpp"
TARGETS={SD_CPP,SD_A,GP_CPP}
CAND_DIR=ROOT/"tools/alpha13/results"/CAND_ID
SOURCE={
 SD_CPP:CAND_DIR/"candidate_sources/JW_SD.cpp",
 SD_A:CAND_DIR/"candidate_library/JW_SD/src/esp32/libJW_SD.a",
 GP_CPP:CAND_DIR/"candidate_sources/JWPLC_GlobalPeripherals.cpp",
}
ALLOWED_REMOTE_DELTA={
 "tools/alpha13/gates/a13_g9_git_close.py",
 "tools/alpha13/gates/run_a13_g9_git_close.bat",
 "tools/alpha13/gates/a13_g9_git_close_selftest.py",
 ".github/workflows/alpha13-g9-git-close.yml",
 "docs/v2.1.0-alpha.13/ALPHA13_STATUS.md",
 "docs/v2.1.0-alpha.13/A13_G9_PRODUCT_ADOPTION_PASS_20261012.md",
}
REGRESSIONS=["SD_BASIC","GLOBAL_SD","GLOBAL_RTC","GLOBAL_BUTTONS"]
MESSAGE="fix(alpha13-g9): adoptar A13-012 SD NUL y A13-014 GlobalPeripherals"

def say(k,v):
    print("G9_GIT_"+str(k)+"="+str(v),flush=True)

def need(ok,reason):
    if not ok:raise RuntimeError(reason)

def sha(file):
    h=hashlib.sha256()
    with Path(file).open("rb") as f:
        for data in iter(lambda:f.read(1024*1024),b""):h.update(data)
    return h.hexdigest()

def call(*args,check=True):
    p=subprocess.run(["git","-c","core.quotepath=false",*args],
                     cwd=ROOT,stdout=subprocess.PIPE,stderr=subprocess.PIPE)
    if check and p.returncode:
        raise RuntimeError("GIT_FAILED_"+repr(args[:2])+":"+
                           p.stderr.decode("utf-8","replace")[-500:])
    return p

def gt(*args):
    return call(*args).stdout.decode("utf-8","replace").strip()

def gp(*args):
    argv=list(args)
    argv.insert(argv.index("--") if "--" in argv else len(argv),"-z")
    return set(filter(None,call(*argv).stdout.decode(
        "utf-8","surrogateescape").split("\0")))

def remote():
    lines=gt("ls-remote","--heads","origin","refs/heads/"+BRANCH).splitlines()
    need(len(lines)==1,"REMOTE_HEAD_NOT_UNIQUE")
    return lines[0].split()[0]

def manifest_map(d,label):
    need(isinstance(d,dict) and
         all(isinstance(k,str) and isinstance(v,str) for k,v in d.items()),
         "MANIFEST_MAP_INVALID_"+label)
    out={}
    for k,v in d.items():
        key=k.replace("\\","/")
        need(key not in out and not key.startswith("/") and
             ".." not in key.split("/"),"MANIFEST_UNSAFE_KEY_"+label)
        out[key]=v
    return out

def marks():
    want=TARGETS
    raw=call("ls-files","-v","-z","--",*sorted(want)).stdout
    result={}
    for b in raw.split(b"\0"):
        if not b:continue
        need(len(b)>=3 and b[1:2]==b" ","INDEX_MALFORMED")
        k=b[2:].decode("utf-8","surrogateescape")
        need(k not in result,"INDEX_DUPLICATE")
        result[k]=b[:1].decode("ascii","replace")
    say("INDEX_FLAGS",";".join(k+":"+result[k] for k in sorted(result)))
    need(set(result)==want and all(c=="H" for c in result.values()),
         "INDEX_FLAGS_OR_PATHS_WRONG")
    need(gp("ls-files","--full-name","--",*sorted(want))==want and
         gp("ls-tree","-r","--name-only","HEAD","--",*sorted(want))==want,
         "INDEX_TRACKED_OR_HEAD_PATHS_WRONG")
    return result

def run(args):
    rid="g9_git_"+datetime.now().strftime("%Y%m%d_%H%M%S")+"_"+uuid.uuid4().hex[:8]
    results=ROOT/"tools/alpha13/results"/rid
    results.mkdir(parents=True,exist_ok=False)
    m={"schema":"A13-G9-GIT-CLOSE-1","run_id":rid,
       "candidate_run":CAND_ID,"adoption_run":ADOPT_ID,
       "status":"REVIEW_STOP_BEFORE_COMMIT","product_commit":None,
       "git_push":False,"upload":False,"serial_access":False,
       "sd_fram_access":False,"release_merge_pr":False}
    staged=False
    committed=False
    try:
        need(Path.cwd().resolve()==ROOT and
             Path(gt("rev-parse","--show-toplevel")).resolve()==ROOT,
             "REPO_ROOT_WRONG")
        need(gt("branch","--show-current")==BRANCH,"BRANCH_WRONG")
        need(not call("ls-files","--unmerged").stdout,"UNMERGED_FILES")
        need(not gp("diff","--cached","--name-only"),"STAGED_FILES_ALREADY_PRESENT")
        observed=gp("diff","--name-only")
        say("UNSTAGED_PATHS",";".join(sorted(observed)) or "NONE")
        need(observed==TARGETS,"EXACT_THREE_PRODUCT_FILES_NOT_DIRTY")
        head=gt("rev-parse","HEAD")
        m["entry_head"]=head
        need(call("merge-base","--is-ancestor",BASE,head,check=False).returncode==0,
             "ADOPTION_BASE_NOT_ANCESTOR")
        changes=gp("diff","--name-only",BASE,head)
        need(changes<=ALLOWED_REMOTE_DELTA and
             "tools/alpha13/gates/a13_g9_git_close.py" in changes and
             "tools/alpha13/gates/run_a13_g9_git_close.bat" in changes,
             "UNEXPECTED_REMOTE_PRODUCT_OR_DOCUMENT_CHANGES")
        need(head==remote(),"REMOTE_HEAD_NOT_EQUAL_LOCAL")
        marks()
        p0_path=ROOT/"tools/alpha13/results"/P0_ID/"MANIFEST.json"
        cand_path=CAND_DIR/"MANIFEST.json"
        adopt_path=ROOT/"tools/alpha13/results"/ADOPT_ID/"MANIFEST.json"
        need(all(p.is_file() for p in [p0_path,cand_path,adopt_path]),
             "PRIOR_PROVENANCE_MANIFESTS_MISSING")
        p0=json.loads(p0_path.read_text(encoding="utf-8-sig"))
        cand=json.loads(cand_path.read_text(encoding="utf-8-sig"))
        adopt=json.loads(adopt_path.read_text(encoding="utf-8-sig"))
        need(p0.get("schema")=="A13-G9-P0-READONLY-1" and
             p0.get("run_id")==P0_ID and
             p0.get("status")=="BASELINE_CAPTURED_READ_ONLY",
             "P0_NOT_QUALIFIED")
        need(cand.get("schema")=="A13-G9-P2P3-CANDIDATE-1" and
             cand.get("run_id")==CAND_ID and
             cand.get("baseline_run_id")==P0_ID and
             cand.get("status")=="CANDIDATE_BUILD_PASS_PRODUCT_UNCHANGED" and
             all(cand.get("phases",{}).get(x)=="PASS" for x in [
                 "SOURCE_BUILD","ARCHIVE_REBUILD","CANDIDATE_REGRESSIONS","FINAL_AUDIT"]) and
             cand.get("candidate_regressions")==REGRESSIONS and
             cand.get("source_object_sha256") and
             cand.get("archive_member_sha256")==cand.get("source_object_sha256") and
             cand.get("candidate_archive_sha256")==ARCHIVE_SHA,
             "CANDIDATE_ARCHIVE_AND_TESTS_NOT_PROVEN")
        need(adopt.get("schema")=="A13-G9-PRODUCT-ADOPTION-1" and
             adopt.get("run_id")==ADOPT_ID and
             adopt.get("source_run")==CAND_ID and
             adopt.get("status")=="PRODUCT_ADOPTED_REGRESSIONS_PASS_PENDING_COMMIT" and
             adopt.get("product_adopted") is True and
             adopt.get("entry_head")==BASE and
             adopt.get("archive_sha256")==ARCHIVE_SHA and
             adopt.get("source_object_sha256")==cand["source_object_sha256"] and
             adopt.get("archive_member_sha256")==cand["archive_member_sha256"] and
             adopt.get("global_source_object_sha256")==cand.get("global_source_object_sha256") and
             adopt.get("product_regressions")==REGRESSIONS and
             all(adopt.get("phases",{}).get(x)=="PASS" for x in [
                 "PREFLIGHT","BACKUP","PRODUCT_ADOPTION","PRODUCT_REGRESSIONS","FINAL_AUDIT"]) and
             all(adopt.get(x) is False for x in [
                 "upload","serial_access","sd_fram_access","git_commit","git_push"]),
             "ADOPTION_NOT_PROVEN")
        need(p0.get("head") and cand.get("entry_head") and
             call("merge-base","--is-ancestor",p0["head"],cand["entry_head"],check=False).returncode==0 and
             call("merge-base","--is-ancestor",cand["entry_head"],BASE,check=False).returncode==0,
             "P0_CANDIDATE_ADOPTION_HEAD_CHAIN_INVALID")
        old=manifest_map(cand.get("before_sha256"),"CAND_BEFORE")
        old_adopt=manifest_map(adopt.get("before_sha256"),"ADOPT_BEFORE")
        now_adopt=manifest_map(adopt.get("product_sha256"),"ADOPT_PRODUCT")
        proposed=manifest_map(adopt.get("candidate_sha256"),"ADOPT_CANDIDATE")
        src=manifest_map(cand.get("candidate_source_sha256"),"CAND_SOURCES")
        need(set(old)==TARGETS and old==old_adopt and
             set(now_adopt)==TARGETS and now_adopt==proposed and
             set(src)=={SD_CPP,GP_CPP},
             "MANIFEST_EXACT_THREE_PATH_SET_MISMATCH")
        need(old[SD_A]==OLD_ARCHIVE and
             now_adopt[SD_A]==ARCHIVE_SHA,"SD_ARCHIVE_MANIFEST_HASH_MISMATCH")
        for k in sorted(TARGETS):
            f=ROOT/k
            artifact=SOURCE[k]
            expected=(ARCHIVE_SHA if k==SD_A else src[k])
            need(artifact.is_file() and sha(f)==sha(artifact)==
                 expected==now_adopt[k] and
                 sha(f)==proposed[k],"CANDIDATE_PRODUCT_BYTES_INVALID_"+k)
            need(p0.get("files",{}).get(k,{}).get("sha256")==old[k],
                 "P0_PRODUCT_SHA_MISMATCH_"+k)
        scope=adopt.get("git_scope_final",{})
        need(set(scope.get("observed",[]))==TARGETS and
             set(scope.get("expected",[]))==TARGETS and
             scope.get("staged")==[] and
             all(scope.get("head_differs_from_worktree",{}).get(k) is True
                 for k in TARGETS),"ADOPTION_FINAL_GIT_SCOPE_NOT_PROVEN")
        backup=Path(adopt.get("backup_root",""))
        need(backup.is_dir(),"ADOPTION_BACKUP_DIRECTORY_MISSING")
        for k in sorted(TARGETS):
            b=backup/k
            need(b.is_file() and sha(b)==old[k],"ADOPTION_BACKUP_SHA_BAD_"+k)
            need(gt("rev-parse","HEAD:"+k)==
                 gt("hash-object","--path",k,str(b)),
                 "BACKUP_DIFFERS_FROM_GIT_HEAD_"+k)
            need(gt("hash-object","--path",k,k)!=gt("rev-parse","HEAD:"+k),
                 "LOCAL_NOT_CHANGED_VS_HEAD_"+k)
        m["product_sha256"]=now_adopt
        m["backups_verified"]=True
        m["archive_sha256"]=ARCHIVE_SHA
        say("PREFLIGHT","PASS")
        if args.verify_only:
            m["status"]="VERIFY_ONLY_PASS_NO_GIT_MUTATION"
            say("STATUS",m["status"])
            say("RESULTS",results)
            return 0

        call("add","--",*sorted(TARGETS))
        staged=True
        need(gp("diff","--cached","--name-only")==TARGETS and
             not gp("diff","--name-only"),"STAGED_OR_UNSTAGED_SCOPE_BAD")
        need(call("diff","--cached","--check",check=False).returncode==0,
             "STAGED_DIFF_CHECK_FAILED")
        for k in sorted(TARGETS):
            need(gt("rev-parse",":"+k)==gt("hash-object","--path",k,k),
                 "STAGED_BLOB_NOT_PRODUCT_"+k)
        need(hashlib.sha256(call("show",":"+SD_A).stdout).hexdigest()==ARCHIVE_SHA,
             "STAGED_SD_ARCHIVE_BYTES_NOT_CANDIDATE")
        (results/"STAGED_DIFF_STAT.log").write_bytes(call("diff","--cached","--stat").stdout)
        say("STAGED_AUDIT","PASS")
        need(remote()==head,"REMOTE_MOVED_BEFORE_COMMIT")
        call("commit","--only","-m",MESSAGE,"--",*sorted(TARGETS))
        committed=True
        staged=False
        new=gt("rev-parse","HEAD")
        need(gt("rev-parse","HEAD^")==head and
             gp("diff-tree","--no-commit-id","--name-only","-r",new)==TARGETS,
             "COMMIT_PARENT_OR_THREE_FILES_BAD")
        need(hashlib.sha256(call("show","HEAD:"+SD_A).stdout).hexdigest()==ARCHIVE_SHA,
             "COMMITTED_SD_ARCHIVE_HASH_WRONG")
        for k in [SD_CPP,GP_CPP]:
            need(gt("rev-parse","HEAD:"+k)==gt("hash-object","--path",k,k),
                 "COMMITTED_SOURCE_BLOB_BAD_"+k)
        need(not gp("diff","--name-only") and
             not gp("diff","--cached","--name-only"),"WORKTREE_NOT_CLEAN_AFTER_COMMIT")
        m["product_commit"]=new
        say("PRODUCT_COMMIT",new)
        call("push","origin","HEAD:refs/heads/"+BRANCH)
        need(remote()==new,"REMOTE_PUSH_HEAD_DIFFERS")
        m["git_push"]=True
        m["remote_head"]=new
        m["status"]="PRODUCT_COMMIT_PUSH_PASS_G9_RECONCILIATION_PENDING"
        say("STATUS",m["status"])
        say("COMMIT_PUSH","PASS")
        say("RESULTS",results)
        return 0
    except Exception as e:
        m["status"]="REVIEW_LOCAL_COMMIT_PUSH_PENDING" if committed else "REVIEW_STOP_BEFORE_COMMIT"
        m["reason"]=repr(e)
        if staged and not committed:
            try:
                call("restore","--staged","--",*sorted(TARGETS))
                m["index_restored"]=True
                say("STAGED_INDEX_RESTORE","PASS")
            except Exception as recovery:
                m["index_restored"]=False
                m["index_restore_error"]=repr(recovery)
                say("STAGED_INDEX_RESTORE","REVIEW")
        say("STATUS",m["status"])
        say("REASON",repr(e))
        say("RESULTS",results)
        return 2
    finally:
        (results/"MANIFEST.json").write_text(
            json.dumps(m,ensure_ascii=False,indent=2)+"\n",encoding="utf-8")
        (results/"SUMMARY.log").write_text(
            "\n".join("G9_GIT_"+k.upper()+"="+str(v) for k,v in m.items()
                      if not isinstance(v,dict))+"\n",encoding="utf-8")

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument("--verify-only",action="store_true")
    p.add_argument("--confirm")
    args=p.parse_args()
    need(args.verify_only or args.confirm=="A13_G9_COMMIT",
         "EXPLICIT_COMMIT_AUTHORIZATION_REQUIRED")
    need(not (args.verify_only and args.confirm is not None),
         "MUTUALLY_EXCLUSIVE_MODES")
    return run(args)

if __name__=="__main__":
    try:sys.exit(main())
    except Exception as e:
        say("FATAL",repr(e))
        sys.exit(2)
