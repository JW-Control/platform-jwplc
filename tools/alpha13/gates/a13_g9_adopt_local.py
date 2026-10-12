#!/usr/bin/env python3
"""Alpha13-G9: adopcion local estricta de SD.cpp, SD.a y GlobalPeripherals.cpp.

Backup y rollback SHA. Sin COM, upload, FRAM/SD fisico, Git stage/commit/push.
PASS deja SOLO los tres productos modificados localmente para commit posterior.
"""
from __future__ import annotations
import argparse
from datetime import datetime
import hashlib
import importlib.util
import json
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile
import uuid

ROOT=Path(__file__).resolve().parents[3]
BRANCH="v2.1.0-alpha.13/feature/cleanup-robustness"
P0_ID="g9_p0_20261011_234245_6727f0b0"
CAND_ID="g9_p2p3_20261012_000449_23842b4f"
NEW_ARCHIVE="58e86fdf31c9bf94204fcbcf7462656fd9f4d9a7d73bff35bd660dbff538b89e"
OLD_ARCHIVE="1b9619ba37295782ade1edc6f06439a38e7e534fc08f5b05757c9af7a645acc0"
CLI_SHA="1017de89179c3167e6b8a38ca6cc4091fa69a3cf97aafa7810f01423003f5571"
CI_COMMIT="712a3510fee3764aa4384ac784c9fd112ba0154e"
FQBN="jwplc_local:esp32:jwplcbasic"
LIBS=ROOT/"JWPLC/2.1.0/libraries"
SD=LIBS/"JW_SD"
GP=LIBS/"JWPLC_GlobalPeripherals"
SD_CPP="JWPLC/2.1.0/libraries/JW_SD/src/JW_SD.cpp"
SD_A="JWPLC/2.1.0/libraries/JW_SD/src/esp32/libJW_SD.a"
GP_CPP="JWPLC/2.1.0/libraries/JWPLC_GlobalPeripherals/src/JWPLC_GlobalPeripherals.cpp"
FILES={SD_CPP,SD_A,GP_CPP}
CAND_DIR=ROOT/"tools/alpha13/results"/CAND_ID
CANDIDATES={
 SD_CPP:CAND_DIR/"candidate_sources/JW_SD.cpp",
 SD_A:CAND_DIR/"candidate_library/JW_SD/src/esp32/libJW_SD.a",
 GP_CPP:CAND_DIR/"candidate_sources/JWPLC_GlobalPeripherals.cpp",
}
SKETCHES=[
 ("SD_BASIC",SD/"examples/BasicReadWrite"),
 ("GLOBAL_SD",GP/"examples/05.microSD_Basic"),
 ("GLOBAL_RTC",GP/"examples/03.RTC_Basic"),
 ("GLOBAL_BUTTONS",GP/"examples/02.Buttons_Basic"),
]
M={}
OUT=None
TEMP=None


def emit(k,v):
    print("G9_ADOPT_"+k+"="+str(v),flush=True)


def require(v,reason):
    if not v: raise RuntimeError(reason)


def sha(p):
    h=hashlib.sha256()
    with Path(p).open("rb") as f:
        for data in iter(lambda:f.read(1024*1024),b""):h.update(data)
    return h.hexdigest()


def git_exec(*args,check=True):
    p=subprocess.run(["git","-c","core.quotepath=false",*args],cwd=ROOT,
                     capture_output=True)
    if check:
        require(p.returncode==0,"GIT_FAILED_"+repr(args)+":"+
                p.stderr.decode("utf-8","replace")[-250:])
    return p


def git(*args):
    return git_exec(*args).stdout.decode("utf-8","replace").strip()


def git_paths(*args):
    argv=list(args)
    argv.insert(argv.index("--") if "--" in argv else len(argv),"-z")
    return set(filter(None,git_exec(*argv).stdout.decode(
        "utf-8","surrogateescape").split("\0")))


def index_marks():
    entries=git_exec("ls-files","-v","-z","--",*sorted(FILES)).stdout
    marks={}
    for item in entries.split(b"\0"):
        if not item:continue
        require(len(item)>=3 and item[1:2]==b" ","INDEX_BAD_RECORD")
        name=item[2:].decode("utf-8","surrogateescape")
        require(name not in marks,"INDEX_DUPLICATE_PATH")
        marks[name]=item[0:1].decode("ascii","replace")
    emit("INDEX_FLAGS",";".join(k+":"+marks[k] for k in sorted(marks)))
    require(set(marks)==FILES and all(v=="H" for v in marks.values()) and
            git_paths("ls-files","--full-name","--",*sorted(FILES))==FILES and
            git_paths("ls-tree","-r","--name-only","HEAD","--",*sorted(FILES))==FILES,
            "INDEX_NOT_TRACKING_EXACT_THREE_NORMAL_FILES")
    return marks


def save():
    if OUT is None:return
    (OUT/"MANIFEST.json").write_text(
        json.dumps(M,ensure_ascii=False,indent=2)+"\n",encoding="utf-8")
    (OUT/"SUMMARY.log").write_text(
        "\n".join("G9_ADOPT_"+k.upper()+"="+str(v) for k,v in M.items()
                  if not isinstance(v,dict))+"\n",encoding="utf-8")


def phase(name,status):
    M.setdefault("phases",{})[name]=status
    emit("PHASE",name+"_"+status)
    save()


def cli_run(args,label):
    argv=[str(s) for s in args]
    p=subprocess.run(argv,cwd=ROOT,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,
                     encoding="utf-8",errors="replace")
    text=p.stdout or ""
    (OUT/(label+".log")).write_text(
        "COMMAND="+repr(argv)+"\nEXIT="+str(p.returncode)+"\n"+text,
        encoding="utf-8")
    emit(label+"_EXIT",p.returncode)
    if p.returncode:
        for line in [x for x in text.splitlines()
                     if re.search(r"error:|fatal error|undefined reference|cannot|No such file",x,re.I)][-7:]:
            emit("COMPILER_DIAGNOSTIC",line[:260])
    require(p.returncode==0,label+"_FAILED_SEE_LOG")
    return text


def load_contract():
    path=ROOT/"tools/alpha13/gates/a13_g9_candidate_contract.py"
    spec=importlib.util.spec_from_file_location("g9_adopt_contract",path)
    require(spec is not None and spec.loader is not None,"G9_CONTRACT_FILE_MISSING")
    c=importlib.util.module_from_spec(spec)
    sys.modules[spec.name]=c
    spec.loader.exec_module(c)
    return c


def git_scope(label):
    changed=git_paths("diff","--name-only")
    staged=git_paths("diff","--cached","--name-only")
    marks=index_marks()
    actual={}
    for name in sorted(FILES):
        p=subprocess.run(["git","hash-object","--path",name,name],
                         cwd=ROOT,capture_output=True,text=True)
        require(p.returncode==0,"HASH_OBJECT_FAILED_"+name)
        actual[name]=(p.stdout.strip()!=git("rev-parse","HEAD:"+name))
    emit(label+"_DIFF_PATHS",";".join(sorted(changed)) or "NONE")
    emit(label+"_EXPECTED_PATHS",";".join(sorted(FILES)))
    emit(label+"_BYTES_CHANGED",";".join(k+":"+str(actual[k]) for k in sorted(actual)))
    M["git_scope_"+label.lower()]={
       "observed":sorted(changed),"expected":sorted(FILES),"staged":sorted(staged),
       "index_flags":marks,"head_differs_from_worktree":actual}
    save()
    require(not staged and changed==FILES and all(actual.values()),
            label+"_EXACT_THREE_DIRTY_PATHS_FAILED")


def require_ancestor(older,newer,label):
    require(git_exec("merge-base","--is-ancestor",older,newer,
                     check=False).returncode==0,label+"_NOT_ANCESTOR")


def preflight(cli,c):
    phase("PREFLIGHT","RUNNING")
    require(Path.cwd().resolve()==ROOT and
            Path(git("rev-parse","--show-toplevel")).resolve()==ROOT,
            "ROOT_NOT_REPOSITORY")
    require(git("branch","--show-current")==BRANCH,"WRONG_BRANCH")
    require(not git_paths("diff","--name-only") and
            not git_paths("diff","--cached","--name-only") and
            not git_exec("ls-files","--unmerged").stdout,"TRACKED_WORKTREE_NOT_CLEAN")
    head=git("rev-parse","HEAD")
    require(head==git("rev-parse","refs/remotes/origin/"+BRANCH),
            "REMOTE_TRACKING_NOT_SYNCHRONIZED")
    require_ancestor(CI_COMMIT,head,"CI_CONTRACT")
    index_marks()
    require(cli.is_absolute() and cli.is_file() and
            cli.name.lower()=="arduino-cli.exe" and sha(cli)==CLI_SHA,
            "OFFICIAL_ARDUINO_CLI_SHA_MISMATCH")
    require("Version: 1.5.1" in cli_run([cli,"version"],"CLI_VERSION"),
            "CLI_NOT_151")
    cores=cli_run([cli,"core","list"],"CLI_CORES")
    require(re.search(r"(?m)^\s*jwplc_local:esp32\s+2\.1\.0-dev(?:\s|$)",cores),
            "JWPLC_LOCAL_CORE_MISSING")
    p0_path=ROOT/"tools/alpha13/results"/P0_ID/"MANIFEST.json"
    oldpath=CAND_DIR/"MANIFEST.json"
    require(p0_path.is_file() and oldpath.is_file(),"PREVIOUS_MANIFEST_MISSING")
    p0=json.loads(p0_path.read_text(encoding="utf-8-sig"))
    old=json.loads(oldpath.read_text(encoding="utf-8-sig"))
    require(p0.get("schema")=="A13-G9-P0-READONLY-1" and
            p0.get("run_id")==P0_ID and
            p0.get("status")=="BASELINE_CAPTURED_READ_ONLY",
            "P0_RUN_NOT_QUALIFIED")
    require(old.get("schema")=="A13-G9-P2P3-CANDIDATE-1" and
            old.get("run_id")==CAND_ID and old.get("baseline_run_id")==P0_ID and
            old.get("status")=="CANDIDATE_BUILD_PASS_PRODUCT_UNCHANGED",
            "CANDIDATE_RUN_NOT_QUALIFIED")
    stages=["PREFLIGHT","SOURCE_CONTRACT","SOURCE_OVERLAY","SOURCE_BUILD",
            "ARCHIVE_REBUILD","CANDIDATE_REGRESSIONS","FINAL_AUDIT"]
    require(all(old.get("phases",{}).get(x)=="PASS" for x in stages) and
            old.get("candidate_regressions")==[name for name,_ in SKETCHES],
            "CANDIDATE_PHASES_OR_REGRESSIONS_NOT_PASS")
    require(all(old.get(x) is False for x in
            ["product_modified","hardware_access","sd_fram_access",
             "git_commit","git_push","physical_upload"]),
            "CANDIDATE_SIDE_EFFECTS_UNEXPECTED")
    require_ancestor(p0.get("head","invalid"),old.get("entry_head","invalid"),
                     "P0_HEAD")
    require_ancestor(old.get("entry_head","invalid"),head,"CANDIDATE_HEAD")
    oldhash=old.get("before_sha256",{})
    require(set(oldhash)==FILES,"BASELINE_PATHS_NOT_EXACT_THREE")
    require(old.get("candidate_archive_sha256")==NEW_ARCHIVE and
            old.get("source_object_sha256") and
            old.get("archive_member_sha256")==old.get("source_object_sha256") and
            old.get("global_source_object_sha256"),
            "SOURCE_OBJECT_ARCHIVE_PROVENANCE_MISSING")
    require(set(old.get("candidate_source_sha256",{}))=={SD_CPP,GP_CPP},
            "CANDIDATE_SOURCE_MANIFEST_KEYS_DIFFER")
    require(Path(old.get("candidate_archive_path","")).resolve()==CANDIDATES[SD_A].resolve() and
            Path(old.get("candidate_sources_directory","")).resolve()==
            (CAND_DIR/"candidate_sources").resolve(),"CANDIDATE_ARTIFACT_LOCATION_CHANGED")
    for name in sorted(FILES):
        orig=ROOT/name
        require(orig.is_file() and sha(orig)==oldhash[name] and
                sha(orig)==p0.get("files",{}).get(name,{}).get("sha256"),
                "SOURCE_BASELINE_MISMATCH_"+name)
        candidate=CANDIDATES[name]
        wanted=(NEW_ARCHIVE if name==SD_A
                else old["candidate_source_sha256"][name])
        require(candidate.is_file() and sha(candidate)==wanted and
                sha(candidate)!=sha(orig),"CANDIDATE_SHA_OR_DELTA_INVALID_"+name)
    require(oldhash[SD_A]==OLD_ARCHIVE,"ORIGINAL_SD_ARCHIVE_SHA_MISMATCH")
    for name,expected in old.get("closed_archives",{}).items():
        require(sha(ROOT/name)==expected,
                "CLOSED_ARCHIVE_SHA_MISMATCH_"+name)
    require(c.make_sd_candidate((ROOT/SD_CPP).read_bytes())==CANDIDATES[SD_CPP].read_bytes(),
            "SD_EXACT_BYTE_CONTRACT_NOT_REPRODUCED")
    require(c.make_global_candidate((ROOT/GP_CPP).read_bytes())==CANDIDATES[GP_CPP].read_bytes(),
            "GLOBAL_EXACT_BYTE_CONTRACT_NOT_REPRODUCED")
    M["entry_head"]=head
    M["p0_run"]=P0_ID
    M["before_sha256"]=oldhash
    M["candidate_sha256"]={name:sha(CANDIDATES[name]) for name in FILES}
    M["source_object_sha256"]=old["source_object_sha256"]
    M["global_source_object_sha256"]=old["global_source_object_sha256"]
    M["archive_member_sha256"]=old["archive_member_sha256"]
    M["archive_sha256"]=NEW_ARCHIVE
    M["cli_sha256"]=sha(cli)
    phase("PREFLIGHT","PASS")


def verify_library(log,name,folder,label):
    dirs=re.findall(r"(?m)^Using library "+re.escape(name)+
                    r" at version .+ in folder: (.+)$",log)
    require(len(dirs)==1 and Path(dirs[0].strip()).resolve()==folder.resolve(),
            "PACKAGE_LIBRARY_SELECTED_WRONG_"+label+"_"+name)


def verify_archive(log,label):
    lines=[x.replace("\\\\","\\") for x in log.splitlines()
           if "-lJW_SD" in x and ".elf" in x]
    require(len(lines)==1,"SD_ARCHIVE_LINK_LINE_NOT_EXACT_"+label)
    line=lines[0]
    require(re.search(r"""(?<!\S)["']?-lJW_SD(?=\s|["']|$)""",line),
            "SD_LINK_FLAG_MISSING_"+label)
    dirs=re.findall(r"""(?<!\S)["']?-L["']?([A-Za-z]:[\\/][^\s"']+)""",line)
    require((ROOT/SD_A).parent.resolve() in [Path(x).resolve() for x in dirs],
            "SD_LINK_ARCHIVE_LOCATION_WRONG_"+label)
    require(sha(ROOT/SD_A)==NEW_ARCHIVE,"SD_ARCHIVE_BYTES_CHANGED_DURING_LINK")
    emit("ARCHIVE_LINK_"+label,"PASS_EXACT_PATH_FLAG_SHA")


def regress(cli):
    phase("PRODUCT_REGRESSIONS","RUNNING")
    passed=[]
    for label,sketch in SKETCHES:
        build=TEMP/("product_"+label.lower())
        log=cli_run([cli,"compile","--fqbn",FQBN,"-j","0","-v","--clean",
                     "--build-path",build,"--libraries",LIBS,sketch],
                    "COMPILE_PRODUCT_"+label)
        verify_library(log,"JW_SD",SD,label)
        verify_library(log,"JWPLC_GlobalPeripherals",GP,label)
        require(not list(build.rglob("JW_SD.cpp.o")),
                "JW_SD_COMPILED_SOURCE_INSTEAD_OF_ARCHIVE_"+label)
        require(len(list(build.rglob("JWPLC_GlobalPeripherals.cpp.o")))==1,
                "GLOBAL_CPP_NOT_FROM_SOURCE_"+label)
        require(re.search(r"[/\\]precompiled[/\\]core[/\\]JWPLCBASIC[/\\]core\.a",
                          log),"CORE_PRECOMPILED_NOT_SELECTED_"+label)
        verify_archive(log,label)
        emit("REGRESSION_"+label,"PASS")
        passed.append(label)
    require(passed==[name for name,_ in SKETCHES],"REGRESSIONS_MISSING")
    M["product_regressions"]=passed
    phase("PRODUCT_REGRESSIONS","PASS")


def main():
    global OUT,TEMP
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument("--arduino-cli",default=r"C:\Program Files\Arduino CLI\arduino-cli.exe")
    args=p.parse_args()
    runid="g9_adopt_"+datetime.now().strftime("%Y%m%d_%H%M%S")+"_"+uuid.uuid4().hex[:8]
    OUT=ROOT/"tools/alpha13/results"/runid
    TEMP=Path(tempfile.gettempdir())/("jwplc_a13_"+runid)
    require(not OUT.exists() and not TEMP.exists(),"RUN_ID_COLLISION")
    OUT.mkdir(parents=True)
    TEMP.mkdir(parents=True)
    backup=TEMP/"originals"
    M.update({"schema":"A13-G9-PRODUCT-ADOPTION-1",
              "run_id":runid,"status":"REVIEW_STOP","phases":{},
              "source_run":CAND_ID,"product_adopted":False,
              "upload":False,"serial_access":False,"sd_fram_access":False,
              "git_commit":False,"git_push":False})
    backed=False
    try:
        c=load_contract()
        cli=Path(args.arduino_cli).expanduser().resolve()
        preflight(cli,c)
        phase("BACKUP","RUNNING")
        for name in sorted(FILES):
            target=backup/name
            target.parent.mkdir(parents=True,exist_ok=True)
            shutil.copy2(ROOT/name,target)
        backed=True
        require(all(sha(ROOT/name)==sha(backup/name) for name in FILES),
                "BACKUP_NOT_EXACT_BYTES")
        M["backup_root"]=str(backup)
        phase("BACKUP","PASS")

        phase("PRODUCT_ADOPTION","RUNNING")
        for name in sorted(FILES):shutil.copy2(CANDIDATES[name],ROOT/name)
        require(all(sha(ROOT/name)==M["candidate_sha256"][name] for name in FILES),
                "ADOPTED_SHA_DIFFERS_CANDIDATE")
        git_scope("ADOPTION")
        require(c.make_sd_candidate((backup/SD_CPP).read_bytes())==(ROOT/SD_CPP).read_bytes() and
                c.make_global_candidate((backup/GP_CPP).read_bytes())==(ROOT/GP_CPP).read_bytes(),
                "EXACT_SOURCE_DIFF_NOT_PRESERVED")
        M["product_adopted"]=True
        phase("PRODUCT_ADOPTION","PASS")
        regress(cli)
        phase("FINAL_AUDIT","RUNNING")
        git_scope("FINAL")
        require(git("rev-parse","HEAD")==M["entry_head"] and
                all(sha(ROOT/name)==M["candidate_sha256"][name] for name in FILES) and
                sha(ROOT/SD_A)==NEW_ARCHIVE,"FINAL_PRODUCT_OR_GIT_SHA_DIFF")
        M["product_sha256"]={name:sha(ROOT/name) for name in FILES}
        phase("FINAL_AUDIT","PASS")
        M["status"]="PRODUCT_ADOPTED_REGRESSIONS_PASS_PENDING_COMMIT"
        emit("STATUS",M["status"])
        emit("ARCHIVE_SHA256",NEW_ARCHIVE)
        emit("BACKUP",backup)
        emit("RESULTS",OUT)
        emit("GIT_COMMIT_OR_PUSH","NONE")
        emit("HARDWARE_ACCESS","NONE")
        save()
        return 0
    except Exception as exc:
        M["status"]="REVIEW_STOP_ROLLBACK"
        M["reason"]=repr(exc)
        emit("STATUS",M["status"])
        emit("REASON",repr(exc))
        if backed:
            try:
                for name in sorted(FILES):shutil.copy2(backup/name,ROOT/name)
                require(all(sha(ROOT/name)==sha(backup/name) for name in FILES),
                        "ROLLBACK_SHA_MISMATCH")
                require(not git_paths("diff","--name-only") and
                        not git_paths("diff","--cached","--name-only"),
                        "ROLLBACK_GIT_WORKTREE_NOT_CLEAN")
                M["rollback"]="PASS"
                M["product_adopted"]=False
                emit("ROLLBACK","PASS")
            except Exception as err:
                M["rollback"]="FAILED_"+repr(err)
                emit("ROLLBACK","REVIEW_REQUIRED")
        emit("RESULTS",OUT)
        save()
        return 2


if __name__=="__main__":
    try:sys.exit(main())
    except Exception as exc:
        emit("FATAL",repr(exc))
        sys.exit(2)
