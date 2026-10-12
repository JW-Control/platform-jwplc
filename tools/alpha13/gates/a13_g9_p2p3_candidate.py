#!/usr/bin/env python3
"""G9-P2/P3 offline: SD source NUL→escape con archive, GlobalPeripherals RTC/25 ms.

Solamente crea candidatos y binarios temporales. NO modifica producto,
no hace upload/COM ni escribe FRAM/SD, no stage, commit, push ni release.
G9 no se cierra con este runner: requiere adopción controlada posterior.
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
FQBN="jwplc_local:esp32:jwplcbasic"
LIBS=ROOT/"JWPLC/2.1.0/libraries"
SD=LIBS/"JW_SD"
GP=LIBS/"JWPLC_GlobalPeripherals"
SD_SOURCE=SD/"src/JW_SD.cpp"
GP_SOURCE=GP/"src/JWPLC_GlobalPeripherals.cpp"
ARCHIVE=SD/"src/esp32/libJW_SD.a"
CORE=ROOT/"JWPLC/2.1.0/precompiled/core/JWPLCBASIC/core.a"
TFT=LIBS/"JWPLC_TFT/src/esp32/libJWPLC_TFT.a"
FRAM=LIBS/"JW_FRAM/src/esp32/libJW_FRAM.a"
RTU=LIBS/"JWPLC_ModbusRTU/src/esp32/libJWPLC_ModbusRTU.a"
EXPECTED={
    ARCHIVE:"1b9619ba37295782ade1edc6f06439a38e7e534fc08f5b05757c9af7a645acc0",
    CORE:"a1a985f64c22838a1987d4280b8c1dc431a42d5789c6692cbfa36dfb6252246d",
    TFT:"4c606ba66e29c1337d42450a29fd09fe76c6988f72aa51d3882b81db59f82ecc",
    FRAM:"df390f3379da13c6d4503a9fb83bc098f2944590fe3ad083348e95bfdcaf7299",
    RTU:"f239100fbb6770a3a6f336179fbfa3a4afd27cc16e12c6f239cf05f28f7a1416",
}
CLI_SHA="1017de89179c3167e6b8a38ca6cc4091fa69a3cf97aafa7810f01423003f5571"
P0=ROOT/"tools/alpha13/results/g9_p0_20261011_234245_6727f0b0/MANIFEST.json"
SKETCHES=[
 ("SD_BASIC",SD/"examples/BasicReadWrite"),
 ("GLOBAL_SD",GP/"examples/05.microSD_Basic"),
 ("GLOBAL_RTC",GP/"examples/03.RTC_Basic"),
 ("GLOBAL_BUTTONS",GP/"examples/02.Buttons_Basic"),
]
M={}
RES=None
TMP=None


def say(k,v):
    print("G9_CANDIDATE_"+str(k)+"="+str(v),flush=True)


def require(ok,reason):
    if not ok: raise RuntimeError(reason)


def digest(file):
    h=hashlib.sha256()
    with Path(file).open("rb") as f:
        for piece in iter(lambda:f.read(1024*1024),b""):h.update(piece)
    return h.hexdigest()


def git(*args):
    p=subprocess.run(["git","-c","core.quotepath=false",*args],cwd=ROOT,
                     capture_output=True)
    require(p.returncode==0,"GIT_FAILED_"+repr(args)+":"+
            p.stderr.decode("utf-8","replace")[-250:])
    return p.stdout.decode("utf-8","replace").strip()


def git_paths(*args):
    return set(filter(None,subprocess.run(
        ["git","-c","core.quotepath=false",*args,"-z"],cwd=ROOT,
        capture_output=True,check=True).stdout.decode("utf-8","surrogateescape").split("\0")))


def save():
    (RES/"MANIFEST.json").write_text(
        json.dumps(M,ensure_ascii=False,indent=2)+"\n",encoding="utf-8")
    (RES/"SUMMARY.log").write_text(
        "\n".join("G9_CANDIDATE_"+str(k).upper()+"="+str(v)
                  for k,v in M.items() if not isinstance(v,dict))+"\n",
        encoding="utf-8")


def phase(name,value):
    M.setdefault("phases",{})[name]=value
    say("PHASE",name+"_"+value)
    save()


def run(args,label,cwd=ROOT):
    argv=[str(v) for v in args]
    p=subprocess.run(argv,cwd=cwd,stdout=subprocess.PIPE,
                     stderr=subprocess.STDOUT,errors="replace",
                     encoding="utf-8")
    output=p.stdout or ""
    (RES/(label+".log")).write_text(
        "COMMAND="+repr(argv)+"\nEXIT="+str(p.returncode)+"\n"+output,
        encoding="utf-8")
    say(label+"_EXIT",p.returncode)
    if p.returncode:
        matches=[l for l in output.splitlines()
                 if re.search(r"fatal error|error:|undefined reference|cannot|no such file",l,re.I)]
        for line in matches[-8:]:say("COMPILER_ERROR",line[:300])
    require(p.returncode==0,label+"_FAILED_SEE_LOG")
    return output


def load_contract():
    path=ROOT/"tools/alpha13/gates/a13_g9_candidate_contract.py"
    spec=importlib.util.spec_from_file_location("g9_contract",path)
    require(spec is not None and spec.loader is not None,"SOURCE_CONTRACT_IMPORT_FAILED")
    mod=importlib.util.module_from_spec(spec)
    sys.modules[spec.name]=mod
    spec.loader.exec_module(mod)
    return mod


def preflight(cli):
    phase("PREFLIGHT","RUNNING")
    require(Path.cwd().resolve()==ROOT and
            Path(git("rev-parse","--show-toplevel")).resolve()==ROOT,
            "RUN_AT_REPOSITORY_ROOT")
    require(git("branch","--show-current")==BRANCH,"WRONG_BRANCH")
    require(not git_paths("diff","--name-only") and
            not git_paths("diff","--cached","--name-only"),
            "GIT_TRACKED_FILES_DIRTY")
    head=git("rev-parse","HEAD")
    require(head==git("rev-parse","refs/remotes/origin/"+BRANCH),
            "LOCAL_REMOTE_HEAD_NOT_EQUAL")
    require(cli.is_absolute() and cli.is_file() and
            cli.name.lower()=="arduino-cli.exe" and digest(cli)==CLI_SHA,
            "PINNED_ARDUINO_CLI_151_NOT_FOUND")
    require("Version: 1.5.1" in run([cli,"version"],"CLI_VERSION"),
            "ARDUINO_CLI_VERSION_NOT_151")
    cores=run([cli,"core","list"],"CLI_CORES")
    require(re.search(r"(?m)^\s*jwplc_local:esp32\s+2\.1\.0-dev(?:\s|$)",cores),
            "LOCAL_JWPLC_CORE_NOT_PRESENT")
    require(P0.is_file(),"G9_P0_LOCAL_MANIFEST_MISSING")
    baseline=json.loads(P0.read_text(encoding="utf-8-sig"))
    require(baseline.get("schema")=="A13-G9-P0-READONLY-1" and
            baseline.get("run_id")=="g9_p0_20261011_234245_6727f0b0" and
            baseline.get("status")=="BASELINE_CAPTURED_READ_ONLY" and
            baseline.get("head")=="93cbd1e7e04ac4f5aedac95bf35fa29453733c38",
            "G9_P0_NOT_CANONICAL")
    require(subprocess.run(["git","merge-base","--is-ancestor",
                            baseline["head"],head],cwd=ROOT).returncode==0,
            "G9_P0_HEAD_NOT_IN_HISTORY")
    for path,expected in EXPECTED.items():
        require(path.is_file() and digest(path)==expected,
                "CLOSED_ARCHIVE_CHANGED_"+path.name)
    for path in [SD_SOURCE,GP_SOURCE,SD/"src/JW_SD.h",
                 GP/"src/JWPLC_GlobalPeripherals.h"]:
        require(path.is_file(),"LIBRARY_FILE_MISSING_"+str(path))
    require(re.search(r"(?m)^precompiled=full\s*$",
                      (SD/"library.properties").read_text(encoding="utf-8")),
            "SD_NOT_PRECOMPILED_FULL")
    require("precompiled=" not in
            (GP/"library.properties").read_text(encoding="utf-8"),
            "GLOBALPERIPHERALS_UNEXPECTED_PRECOMPILED")
    files=baseline.get("files",{})
    for target in [SD_SOURCE,GP_SOURCE,ARCHIVE]:
        name=target.relative_to(ROOT).as_posix()
        require(name in files and digest(target)==files[name]["sha256"],
                "G9_P0_TO_CANDIDATE_SOURCE_BASELINE_DIFF_"+target.name)
    M["entry_head"]=head
    M["baseline_run_id"]=baseline["run_id"]
    M["before_sha256"]={p.relative_to(ROOT).as_posix():digest(p)
                        for p in [SD_SOURCE,GP_SOURCE,ARCHIVE]}
    M["closed_archives"]={p.relative_to(ROOT).as_posix():v
                          for p,v in EXPECTED.items()}
    phase("PREFLIGHT","PASS")


def create_candidates(c):
    phase("SOURCE_CONTRACT","RUNNING")
    proof=run([sys.executable,"-B",
         ROOT/"tools/alpha13/gates/a13_g9_candidate_contract.py"],
         "SOURCE_CONTRACT")
    require("G9_CANDIDATE_CONTRACT=PASS_READ_ONLY" in proof,
            "SOURCE_CONTRACT_NOT_PASS")
    out=RES/"candidate_sources"
    out.mkdir()
    sdnew=c.make_sd_candidate(SD_SOURCE.read_bytes())
    gpnew=c.make_global_candidate(GP_SOURCE.read_bytes())
    (out/"JW_SD.cpp").write_bytes(sdnew)
    (out/"JWPLC_GlobalPeripherals.cpp").write_bytes(gpnew)
    M["candidate_source_sha256"]={
        SD_SOURCE.relative_to(ROOT).as_posix():digest(out/"JW_SD.cpp"),
        GP_SOURCE.relative_to(ROOT).as_posix():digest(out/"JWPLC_GlobalPeripherals.cpp"),
    }
    M["candidate_sources_directory"]=str(out)
    phase("SOURCE_CONTRACT","PASS")
    return out


def make_source_overlays(candidates):
    phase("SOURCE_OVERLAY","RUNNING")
    sd=TMP/"source/JW_SD"
    gp=TMP/"source/JWPLC_GlobalPeripherals"
    shutil.copytree(SD,sd)
    shutil.copytree(GP,gp)
    shutil.copy2(candidates/"JW_SD.cpp",sd/"src/JW_SD.cpp")
    shutil.copy2(candidates/"JWPLC_GlobalPeripherals.cpp",
                 gp/"src/JWPLC_GlobalPeripherals.cpp")
    # Forzar compilación desde fuente; ni old ni new .a se permiten en overlay.
    shutil.rmtree(sd/"src/esp32")
    props=(sd/"library.properties").read_text(encoding="utf-8")
    props="\n".join(x for x in props.splitlines()
                    if not x.startswith("precompiled="))+"\n"
    (sd/"library.properties").write_text(props,encoding="utf-8")
    require(not list(sd.rglob("*.a")) and not list(gp.rglob("*.a")),
            "SOURCE_OVERLAYS_STILL_HAVE_ARCHIVE")
    phase("SOURCE_OVERLAY","PASS")
    return sd,gp


def verify_selection(log,name,library,role):
    matches=re.findall(r"(?m)^Using library "+re.escape(name)+
                       r" at version .+ in folder: (.+)$",log)
    require(len(matches)==1 and
            Path(matches[0].strip()).resolve()==library.resolve(),
            "WRONG_LIBRARY_SELECTED_"+role+"_"+name)


def compile_sketch(cli,role,sketch,sd,gp):
    build=TMP/("build_"+role.lower())
    flags=[cli,"compile","--fqbn",FQBN,"-j","0","-v","--clean",
           "--build-path",build,
           "--library",sd,"--library",gp,"--libraries",LIBS,sketch]
    output=run(flags,"COMPILE_"+role)
    verify_selection(output,"JW_SD",sd,role)
    verify_selection(output,"JWPLC_GlobalPeripherals",gp,role)
    require(re.search(r"[/\\]precompiled[/\\]core[/\\]JWPLCBASIC[/\\]core\.a",
                      output) is not None,"CORE_ARCHIVE_LINK_MISSING_"+role)
    return output,build


def find_archiver(log):
    bins=re.findall(
        r"""["']([^"'\r\n]*xtensa-esp32-elf-g\+\+(?:\.exe)?)["']""",log,re.I)
    for compiler in bins:
        for name in ("xtensa-esp32-elf-gcc-ar.exe",
                     "xtensa-esp32-elf-gcc-ar"):
            p=Path(compiler).parent/name
            if p.is_file():return p
    raise RuntimeError("ESP32_ARCHIVER_NOT_FOUND_IN_COMPILER_COMMAND")


def build_archive(cli,sd,gp):
    phase("SOURCE_BUILD","RUNNING")
    log,build=compile_sketch(cli,"SOURCE_SD",
                             SKETCHES[0][1],sd,gp)
    objs=list(build.rglob("JW_SD.cpp.o"))
    globals_=list(build.rglob("JWPLC_GlobalPeripherals.cpp.o"))
    require(len(objs)==1 and len(globals_)==1,
            "SOURCE_OBJECTS_NOT_UNIQUE_SD_GLOBAL")
    src_object=objs[0]
    M["source_object_sha256"]=digest(src_object)
    M["global_source_object_sha256"]=digest(globals_[0])
    archiver=find_archiver(log)
    M["archiver"]=str(archiver)
    M["archiver_sha256"]=digest(archiver)
    phase("SOURCE_BUILD","PASS")

    phase("ARCHIVE_REBUILD","RUNNING")
    candidate=RES/"candidate_library/JW_SD"
    shutil.copytree(SD,candidate)
    shutil.copy2(RES/"candidate_sources/JW_SD.cpp",
                 candidate/"src/JW_SD.cpp")
    archive=candidate/"src/esp32/libJW_SD.a"
    archive.unlink()
    run([archiver,"crs",archive,src_object],"ARCHIVE_CREATE")
    members=run([archiver,"t",archive],"ARCHIVE_MEMBERS").splitlines()
    require([x.strip() for x in members if x.strip()]==["JW_SD.cpp.o"],
            "SD_ARCHIVE_MEMBERS_NOT_SINGLE_SOURCE_OBJECT")
    extracted=TMP/"extracted_archive"
    extracted.mkdir()
    run([archiver,"x",archive],"ARCHIVE_EXTRACT",cwd=extracted)
    member=extracted/"JW_SD.cpp.o"
    require(member.is_file() and digest(member)==digest(src_object),
            "SD_ARCHIVE_MEMBER_OBJECT_BYTES_DIFFER")
    M["archive_member_sha256"]=digest(member)
    M["candidate_archive_sha256"]=digest(archive)
    M["candidate_archive_path"]=str(archive)
    say("ARCHIVE_MEMBER_BYTE_PARITY","PASS")
    say("NEW_SD_ARCHIVE_SHA256",digest(archive))
    phase("ARCHIVE_REBUILD","PASS")
    return candidate,archive


def check_archive_link(log,archive,label):
    matches=[line.replace("\\\\","\\") for line in log.splitlines()
             if "-lJW_SD" in line and ".elf" in line]
    require(len(matches)==1,"JW_SD_LINK_LINE_NOT_UNIQUE_"+label)
    line=matches[0]
    require(re.search(r"""(?<!\S)["']?-lJW_SD(?=\s|["']|$)""",line),
            "JW_SD_LINK_FLAG_MISSING_"+label)
    dirs=re.findall(r"""(?<!\S)["']?-L["']?([A-Za-z]:[\\/][^\s"']+)""",line)
    require(archive.parent.resolve() in
            [Path(s).resolve() for s in dirs],
            "JW_SD_ARCHIVE_LINK_PATH_WRONG_"+label)
    require(digest(archive)==M["candidate_archive_sha256"],
            "JW_SD_ARCHIVE_HASH_CHANGED_"+label)
    say("ARCHIVE_LINK_"+label,"PASS_EXACT_PATH_FLAG_SHA")


def regress(cli,pre_sd,archive,source_gp):
    phase("CANDIDATE_REGRESSIONS","RUNNING")
    seen=[]
    for label,sketch in SKETCHES:
        log,build=compile_sketch(cli,"REGRESSION_"+label,sketch,pre_sd,source_gp)
        require(not list(build.rglob("JW_SD.cpp.o")),
                "PRECOMPILED_JW_SD_SOURCE_WAS_COMPILED_"+label)
        require(len(list(build.rglob("JWPLC_GlobalPeripherals.cpp.o")))==1,
                "GLOBALPERIPHERALS_CPP_NOT_SELECTED_"+label)
        check_archive_link(log,archive,label)
        seen.append(label)
        say("REGRESSION_"+label,"PASS")
    M["candidate_regressions"]=seen
    phase("CANDIDATE_REGRESSIONS","PASS")


def final_audit():
    phase("FINAL_AUDIT","RUNNING")
    require(M["candidate_regressions"]==[x[0] for x in SKETCHES],
            "REGRESSIONS_INCOMPLETE")
    require(M["archive_member_sha256"]==M["source_object_sha256"],
            "SOURCE_ARCHIVE_PARITY_CHANGED")
    for name,value in M["before_sha256"].items():
        require(digest(ROOT/name)==value,
                "PRODUCT_FILE_MUTATED_"+name)
    for path,expected in EXPECTED.items():
        require(digest(path)==expected,
                "OTHER_PRECOMPILED_ARCHIVE_MUTATED_"+path.name)
    for original,newval in M["candidate_source_sha256"].items():
        candidate=RES/"candidate_sources"/Path(original).name
        require(digest(candidate)==newval,
                "CANDIDATE_SOURCE_CHANGED_"+original)
    require(git("rev-parse","HEAD")==M["entry_head"] and
            not git_paths("diff","--name-only") and
            not git_paths("diff","--cached","--name-only"),
            "GIT_OR_WORKTREE_CHANGED")
    phase("FINAL_AUDIT","PASS")
    say("PRODUCT_CHANGE","NONE")
    say("HARDWARE_ACCESS","NONE")


def main():
    global RES,TMP
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--arduino-cli",default=r"C:\Program Files\Arduino CLI\arduino-cli.exe")
    args=parser.parse_args()
    runid="g9_p2p3_"+datetime.now().strftime("%Y%m%d_%H%M%S")+"_"+uuid.uuid4().hex[:8]
    RES=ROOT/"tools/alpha13/results"/runid
    TMP=Path(tempfile.gettempdir())/("jwplc_a13_"+runid)
    require(not RES.exists() and not TMP.exists(),"RUN_ID_COLLISION")
    RES.mkdir(parents=True)
    TMP.mkdir(parents=True)
    M.update({"schema":"A13-G9-P2P3-CANDIDATE-1",
              "run_id":runid,"status":"REVIEW_STOP",
              "phases":{},"product_modified":False,"hardware_access":False,
              "sd_fram_access":False,"git_commit":False,"git_push":False,
              "physical_upload":False})
    try:
        cli=Path(args.arduino_cli).expanduser().resolve()
        c=load_contract()
        preflight(cli)
        candidate=create_candidates(c)
        source_sd,source_gp=make_source_overlays(candidate)
        pre_sd,archive=build_archive(cli,source_sd,source_gp)
        regress(cli,pre_sd,archive,source_gp)
        final_audit()
        M["status"]="CANDIDATE_BUILD_PASS_PRODUCT_UNCHANGED"
        say("STATUS",M["status"])
        say("RUN_ID",runid)
        say("RESULTS",RES)
        save()
        return 0
    except Exception as exc:
        M["status"]="REVIEW_STOP"
        M["reason"]=repr(exc)
        say("STATUS","REVIEW_STOP")
        say("REASON",repr(exc))
        say("RESULTS",RES)
        try: save()
        except Exception:pass
        return 2


if __name__=="__main__":
    sys.exit(main())
