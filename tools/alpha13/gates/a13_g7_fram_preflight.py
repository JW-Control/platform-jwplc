#!/usr/bin/env python3
"""G7 FRAM preflight NO-DESTRUCTIVO; no upload, escrituras FRAM ni commits.

Compila ejemplos existentes con la librería precompilada normal y verifica
la procedencia de la CLI/board/FRAM, sin sustituir producto ni archivos .a.
"""
from __future__ import annotations
import argparse
from datetime import datetime
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import sys
import tempfile
import uuid

ROOT=Path(__file__).resolve().parents[3]
BRANCH="v2.1.0-alpha.13/feature/cleanup-robustness"
FQBN="jwplc_local:esp32:jwplcbasic"
LIBS=ROOT/"JWPLC/2.1.0/libraries"
FRAM=LIBS/"JW_FRAM"
ARCHIVE=FRAM/"src/esp32/libJW_FRAM.a"
SKETCHES=[
    LIBS/"JWPLC_GlobalPeripherals/examples/04.FRAM_Basic",
    FRAM/"examples/Block_Config",
]
RUN_ID=datetime.now().strftime("%Y%m%d_%H%M%S")+"_"+uuid.uuid4().hex[:8]
OUT=ROOT/"tools/alpha13/results"/("g7_fram_preflight_"+RUN_ID)

def emit(key,val): print(f"{key}={val}",flush=True)

def require(flag,why):
    if not flag: raise RuntimeError(why)

def sha(path):
    h=hashlib.sha256()
    with Path(path).open("rb") as stream:
        for block in iter(lambda:stream.read(1<<20),b""):
            h.update(block)
    return h.hexdigest()

def git(*args):
    done=subprocess.run(["git",*args],cwd=ROOT,
                        stdout=subprocess.PIPE,stderr=subprocess.PIPE)
    require(done.returncode==0,
            "GIT_ERROR_"+done.stderr.decode(errors="replace")[-250:])
    return done.stdout.decode("utf-8","replace").strip()

def launch(args,label):
    proc=subprocess.run([str(x) for x in args],cwd=ROOT,stdout=subprocess.PIPE,
                        stderr=subprocess.STDOUT,encoding="utf-8",
                        errors="backslashreplace")
    output=proc.stdout or ""
    (OUT/(label+".log")).write_text(
        "EXIT="+str(proc.returncode)+"\n"+output,encoding="utf-8")
    emit(label+"_EXIT",proc.returncode)
    if proc.returncode:
        for line in [l for l in output.splitlines()
                     if re.search(r"fatal error:|error:|undefined reference",l)][-5:]:
            emit("DIAGNOSTIC",line[-300:])
    require(proc.returncode==0,label+"_FAILED")
    return output

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--arduino-cli",required=True)
    args=parser.parse_args()
    OUT.mkdir(parents=True,exist_ok=False)
    record={"run_id":RUN_ID,"hito":"A13-G7-PREFLIGHT","status":"REVIEW",
            "product_mutated":False,"firmware_uploaded":False,
            "git_committed":False}
    try:
        cli=Path(args.arduino_cli).expanduser()
        require(cli.is_absolute() and cli.is_file(),"CLI_ABSOLUTE_EXE_REQUIRED")
        cli=cli.resolve()
        require(cli.name.casefold() in ("arduino-cli.exe","arduino-cli"),
                "CLI_FILENAME_UNEXPECTED")
        require("arduino plc ide tools" not in str(cli).casefold(),
                "ARDUINO_PLC_IDE_TOOLS_FORBIDDEN")
        record["cli_exe"]=str(cli)
        record["cli_sha256"]=sha(cli)
        emit("CLI_EXE",cli)
        emit("CLI_SHA256",record["cli_sha256"])
        require(git("branch","--show-current")==BRANCH,"WRONG_BRANCH")
        require(Path(git("rev-parse","--show-toplevel")).resolve()==ROOT,
                "WRONG_ROOT")
        record["git_head"]=git("rev-parse","HEAD")
        require(not git("status","--porcelain=v1","--untracked-files=all"),
                "GIT_WORKTREE_NOT_CLEAN")
        emit("GIT_HEAD",record["git_head"])
        version=launch([cli,"version"],"CLI_VERSION")
        require("Version: 1.5.1" in version,"ARDUINO_CLI_1_5_1_REQUIRED")
        cores=launch([cli,"core","list"],"CLI_CORES")
        require(re.search(r"(?m)^\s*jwplc_local:esp32\s+2\.1\.0-dev(?:\s|$)",
                          cores)!=None,
                "JWPLC_LOCAL_CORE_2_1_0_DEV_MISSING")
        properties=(FRAM/"library.properties").read_text(encoding="utf-8")
        require(re.search(r"(?m)^precompiled=full\s*$",properties)!=None,
                "FRAM_PRECOMPILED_MODE_CHANGED")
        require(ARCHIVE.is_file(),"FRAM_ARCHIVE_MISSING")
        for path in (FRAM/"src/JW_FRAM.cpp",FRAM/"src/JW_FRAM.h"):
            require(path.is_file(),"FRAM_SOURCE_MISSING_"+str(path))
        record["fram_archive_sha256"]=sha(ARCHIVE)
        record["fram_archive_bytes"]=ARCHIVE.stat().st_size
        record["fram_cpp_sha256"]=sha(FRAM/"src/JW_FRAM.cpp")
        record["fram_header_sha256"]=sha(FRAM/"src/JW_FRAM.h")
        emit("FRAM_ARCHIVE_SHA256",record["fram_archive_sha256"])
        emit("FRAM_ARCHIVE_BYTES",record["fram_archive_bytes"])
        emit("FRAM_PRECOMPILED_POLICY","FULL")

        for idx,sketch in enumerate(SKETCHES,1):
            require(sketch.is_dir(),"FRAM_SKETCH_NOT_FOUND_"+sketch.name)
            label=f"FRAM_COMPILE_{idx}"
            with tempfile.TemporaryDirectory(prefix="jwplc_g7_fram_") as tmp:
                build=Path(tmp)/"build"
                verbose=launch([cli,"compile","--fqbn",FQBN,
                    "-j","0","-v","--clean","--build-path",build,
                    "--libraries",LIBS,sketch],label)
                selected=re.findall(
                    r"(?m)^Using library JW_FRAM at version .+ in folder: (.+)$",
                    verbose)
                require(len(selected)==1 and
                        Path(selected[0].strip()).resolve()==FRAM.resolve(),
                        "WRONG_FRAM_LIBRARY_SELECTED_"+label)
                # En precompiled=full no debe compilarse JW_FRAM.cpp
                # del package; debe enlazarse el archive original.
                require(not list(build.rglob("JW_FRAM.cpp.o")),
                        "JW_FRAM_SOURCE_INSTEAD_OF_PRECOMPILED_"+label)
                require("libJW_FRAM.a" in verbose,
                        "JW_FRAM_ARCHIVE_NOT_IN_LINK_"+label)
                emit(label,"PASS_PRECOMPILED_NORMAL_LINK")
        record["status"]="PASS_PRECOMPILED_BASELINE"
        emit("STATUS",record["status"])
        emit("G7_SOURCE_MUTATION","NO")
        emit("G7_FRAM_WRITE","NO")
        emit("NEXT","DESIGN_CANDIDATE_AND_ARCHIVE_REBUILD")
        return 0
    except Exception as exc:
        record["status"]="REVIEW"
        record["reason"]=str(exc)
        emit("STATUS","REVIEW")
        emit("REASON",str(exc))
        return 2
    finally:
        (OUT/"MANIFEST.json").write_text(
            json.dumps(record,indent=2,ensure_ascii=False)+"\n",encoding="utf-8")
        (OUT/"SUMMARY.log").write_text(
            "STATUS="+record["status"]+"\n"+
            "RUN_ID="+RUN_ID+"\n"+
            "FIRMWARE_UPLOAD=NO\nFRAM_WRITE=NO\nGIT_COMMIT=NO\n",
            encoding="utf-8")

if __name__=="__main__":sys.exit(main())
