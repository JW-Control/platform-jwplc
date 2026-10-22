#!/usr/bin/env python3
"""G8 P1/P2: candidato aislado RTU/RS485 y archive RTU fuente->objeto.

No cambia producto. Sin COM, upload, FRAM, Git commit, merge ni PR.
Gate PASS significa CANDIDATE_BUILD_PASS, NO ADOPTION/PUBLICATION.
"""
from __future__ import annotations
import argparse
from datetime import datetime
import hashlib
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
RTU=LIBS/"JWPLC_ModbusRTU"
RS=LIBS/"JWPLC_RS485"
ARCHIVE=RTU/"src/esp32/libJWPLC_ModbusRTU.a"
FRAM=LIBS/"JW_FRAM/src/esp32/libJW_FRAM.a"
CORE=ROOT/"JWPLC/2.1.0/precompiled/core/JWPLCBASIC/core.a"
TFT=LIBS/"JWPLC_TFT/src/esp32/libJWPLC_TFT.a"
CAND_RTU=ROOT/"tools/alpha13/candidates/g8/JWPLC_ModbusRTU.cpp"
CAND_RS=ROOT/"tools/alpha13/candidates/g8/JWPLC_RS485.cpp"
SOURCE_TEST=ROOT/"tools/alpha13/gates/a13_g8_source_selftest.py"
P0=ROOT/"tools/alpha13/results/g8_p0_20261011_185223_e0c4e0cc/MANIFEST.json"
EXPECTED={
    CORE:"a1a985f64c22838a1987d4280b8c1dc431a42d5789c6692cbfa36dfb6252246d",
    TFT:"4c606ba66e29c1337d42450a29fd09fe76c6988f72aa51d3882b81db59f82ecc",
    FRAM:"df390f3379da13c6d4503a9fb83bc098f2944590fe3ad083348e95bfdcaf7299",
}
CLI_SHA="1017de89179c3167e6b8a38ca6cc4091fa69a3cf97aafa7810f01423003f5571"
PRODUCT=[
 RTU/"src/JWPLC_ModbusRTU.cpp",RTU/"src/JWPLC_ModbusRTU.h",
 RS/"src/JWPLC_RS485.cpp",RS/"src/JWPLC_RS485.h",
 RTU/"library.properties",RS/"library.properties",ARCHIVE,CORE,TFT,FRAM
]
SKETCHES=[
 ("SLAVE",RTU/"examples/01.ModbusRTU_Slave_Holding"),
 ("MASTER",RTU/"examples/02.ModbusRTU_Master_Read"),
 ("RS485_STATUS",RS/"examples/03.RS485_Status"),
]
M={}
RES=None
TMP=None

def say(name,value):
    print("G8_"+name+"="+str(value),flush=True)

def need(ok,reason):
    if not ok:raise RuntimeError(reason)

def sha(path):
    h=hashlib.sha256()
    with Path(path).open("rb") as f:
        for chunk in iter(lambda:f.read(1<<20),b""):h.update(chunk)
    return h.hexdigest()

def git(*args):
    p=subprocess.run(["git","-c","core.quotepath=false",*args],cwd=ROOT,capture_output=True)
    need(p.returncode==0,"GIT_COMMAND_FAILED_"+p.stderr.decode("utf-8","replace")[-350:])
    return p.stdout.decode("utf-8","replace").strip()

def git_paths(*args):
    p=subprocess.run(["git","-c","core.quotepath=false",*args,"-z"],cwd=ROOT,
                     stdout=subprocess.PIPE,stderr=subprocess.PIPE)
    need(p.returncode==0,"GIT_PATHS_FAILED")
    return set(filter(None,p.stdout.decode("utf-8","replace").split("\0")))

def save():
    RES.mkdir(parents=True,exist_ok=True)
    (RES/"MANIFEST.json").write_text(
        json.dumps(M,indent=2,ensure_ascii=False)+"\n",encoding="utf-8")
    lines=["G8_RUN_ID="+M["run_id"],"G8_STATUS="+M["status"]]
    lines += ["G8_PHASE_"+k+"="+v for k,v in M["phases"].items()]
    lines += [ "G8_"+k.upper()+"="+str(M[k])
               for k in ("reason","entry_head","archive_sha256","source_object_sha256")
               if k in M]
    (RES/"SUMMARY.log").write_text("\n".join(lines)+"\n",encoding="utf-8")

def stage(label,state):
    M["phases"][label]=state
    say("PHASE",label+"_"+state)
    save()

def run(cmd,label,cwd=ROOT):
    argv=[str(x) for x in cmd]
    p=subprocess.run(argv,cwd=cwd,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,
                     encoding="utf-8",errors="replace")
    out=p.stdout or ""
    (RES/(label+".log")).write_text(
        "COMMAND="+repr(argv)+"\nEXIT="+str(p.returncode)+"\n"+out,
        encoding="utf-8")
    say(label+"_EXIT",p.returncode)
    if p.returncode:
        for line in [x for x in out.splitlines()
                     if re.search(r"error:|fatal error:|undefined reference",x,re.I)][-5:]:
            say("DIAGNOSTIC",line[-240:])
    need(p.returncode==0,label+"_EXIT_NONZERO")
    return out

def preflight(cli):
    stage("PREFLIGHT","RUNNING")
    need(Path.cwd().resolve()==ROOT,"RUN_FROM_REPO_ROOT")
    need(git("branch","--show-current")==BRANCH,"WRONG_BRANCH")
    need(Path(git("rev-parse","--show-toplevel")).resolve()==ROOT,"GIT_ROOT_MISMATCH")
    need(not git_paths("diff","--name-only"),"WORKTREE_DIRTY")
    need(not git_paths("diff","--cached","--name-only"),"STAGED_FILES")
    head=git("rev-parse","HEAD")
    need(head==git("rev-parse","refs/remotes/origin/"+BRANCH),
         "LOCAL_REMOTE_HEAD_DIFFER")
    M["entry_head"]=head
    need(cli.is_absolute() and cli.is_file() and cli.name.lower()=="arduino-cli.exe",
         "CLI_NOT_ABSOLUTE_EXE")
    need(sha(cli)==CLI_SHA,"OFFICIAL_CLI_SHA_MISMATCH")
    need("Version: 1.5.1" in run([cli,"version"],"CLI_VERSION"),"CLI_VERSION")
    cores=run([cli,"core","list"],"CLI_CORES")
    need(re.search(r"(?m)^\s*jwplc_local:esp32\s+2\.1\.0-dev(?:\s|$)",cores),
         "JWPLC_CORE_NOT_INSTALLED")
    need(P0.is_file(),"G8_P0_MANIFEST_MISSING")
    old=json.loads(P0.read_text(encoding="utf-8-sig"))
    need(old.get("schema")=="A13-G8-P0-READONLY-1" and
         old.get("run_id")=="g8_p0_20261011_185223_e0c4e0cc" and
         old.get("status")=="P0_BASELINE_PASS_NO_PRODUCT_CHANGE" and
         old.get("normal_compilations")==["SLAVE","MASTER","RS485_STATUS"] and
         old.get("phases",{}).get("FINAL_AUDIT")=="PASS",
         "G8_P0_NOT_VALIDATED")
    for p in [*PRODUCT,CAND_RTU,CAND_RS,SOURCE_TEST]:
        need(p.is_file(),"SOURCE_MISSING_"+str(p))
    for path,value in EXPECTED.items():
        need(sha(path)==value,"CLOSED_ARCHIVE_BASELINE_MISMATCH_"+path.name)
    M["source_sha256"]={str(p.relative_to(ROOT)):sha(p) for p in [CAND_RTU,CAND_RS]}
    M["before_sha256"]={str(p.relative_to(ROOT)):sha(p) for p in PRODUCT}
    for p in (RTU,RS):
        prop=(p/"library.properties").read_text(encoding="utf-8")
        if p==RTU:
            need(re.search(r"(?m)^precompiled=full\s*$",prop)!=None,
                 "RTU_NOT_PRECOMPILED")
        else:
            need(re.search(r"(?m)^precompiled\s*=",prop)==None,
                 "RS485_UNEXPECTED_PRECOMPILED")
    stage("PREFLIGHT","PASS")

def source_test():
    stage("NATIVE_CONTRACT","RUNNING")
    output=run([sys.executable,"-B",SOURCE_TEST],"SOURCE_TEST")
    need("G8_SOURCE_CONTRACT=PASS" in output and
         "G8_HARDWARESERIAL_BOOL_CORE_CONTRACT=PASS" in output,
         "SOURCE_SELFTEST_NOT_PASS")
    M["native_windows"]="NOT_AVAILABLE_CI_INDEPENDENT"
    M["native_ci_run"]="VERIFY_GITHUB_ACTIONS_NATIVE_PRIOR_TO_ADOPTION"
    stage("NATIVE_CONTRACT","PASS")

def stage_libraries():
    stage("SOURCE_OVERLAY","RUNNING")
    source_rtu=TMP/"source/JWPLC_ModbusRTU"
    source_rs=TMP/"source/JWPLC_RS485"
    for lib,source,candidate,name in [
      (RTU,source_rtu,CAND_RTU,"JWPLC_ModbusRTU"),
      (RS,source_rs,CAND_RS,"JWPLC_RS485")]:
        (source/"src").mkdir(parents=True)
        shutil.copy2(candidate,source/"src"/(name+".cpp"))
        shutil.copy2(lib/"src"/(name+".h"),source/"src"/(name+".h"))
        props=(lib/"library.properties").read_text(encoding="utf-8")
        if lib==RTU:
            props="\n".join(s for s in props.splitlines()
                            if not s.startswith("precompiled="))+"\n"
        (source/"library.properties").write_text(props,encoding="utf-8")
    stage("SOURCE_OVERLAY","PASS")
    return source_rtu,source_rs

def assert_selected(log,label,name,path):
    matches=re.findall(r"(?m)^Using library "+re.escape(name)+
                       r" at version .+ in folder: (.+)$",log)
    need(len(matches)==1 and Path(matches[0].strip()).resolve()==path.resolve(),
         "WRONG_LIBRARY_SELECTED_"+label+"_"+name)

def compile(cli,sketch,label,rtu_lib,rs_lib):
    build=TMP/("build_"+label)
    flags=[cli,"compile","--fqbn",FQBN,"-j","0","-v","--clean",
           "--build-path",build]
    if not label.endswith("RS485_STATUS"):
        flags+=["--library",rtu_lib]
    flags+=["--library",rs_lib,"--libraries",LIBS,sketch]
    output=run(flags,"COMPILE_"+label)
    assert_selected(output,label,"JWPLC_RS485",rs_lib)
    if not label.endswith("RS485_STATUS"):
        assert_selected(output,label,"JWPLC_ModbusRTU",rtu_lib)
    rs_objs=list(build.rglob("JWPLC_RS485.cpp.o"))
    need(len(rs_objs)==1,"RS485_SOURCE_OBJECT_NOT_UNIQUE_"+label)
    need(re.search(r"[/\\]precompiled[/\\]core[/\\]JWPLCBASIC[/\\]core\.a",
                   output)!=None,"CORE_ARCHIVE_NOT_SELECTED_"+label)
    return output,build,rs_objs[0]

def build_archive(cli,src_rtu,src_rs):
    stage("SOURCE_BUILD","RUNNING")
    verbose,build,rs_obj=compile(cli,SKETCHES[0][1],
                                "SOURCE_RTU",src_rtu,src_rs)
    objects=list(build.rglob("JWPLC_ModbusRTU.cpp.o"))
    need(len(objects)==1,"RTU_SOURCE_OBJECT_NOT_UNIQUE")
    obj=objects[0]
    M["source_object_sha256"]=sha(obj)
    M["rs485_source_object_sha256"]=sha(rs_obj)
    compiler_tools=re.findall(
      r"""["']([^"'\r\n]*xtensa-esp32-elf-g\+\+(?:\.exe)?)["']""",
      verbose,re.I)
    archiver=None
    for comp in compiler_tools:
        for name in ("xtensa-esp32-elf-gcc-ar.exe","xtensa-esp32-elf-gcc-ar"):
            possible=Path(comp).parent/name
            if possible.is_file():
                archiver=possible
                break
        if archiver:break
    need(archiver is not None,"ARCHIVER_NOT_FOUND_IN_COMPILER_COMMAND")
    stage("SOURCE_BUILD","PASS")
    stage("ARCHIVE_REBUILD","RUNNING")
    candidate=TMP/"candidate/JWPLC_ModbusRTU"
    (candidate/"src/esp32").mkdir(parents=True)
    shutil.copy2(RTU/"src/JWPLC_ModbusRTU.h",candidate/"src/JWPLC_ModbusRTU.h")
    shutil.copy2(RTU/"library.properties",candidate/"library.properties")
    archive=candidate/"src/esp32/libJWPLC_ModbusRTU.a"
    run([archiver,"crs",archive,obj],"ARCHIVE_CREATE")
    members=run([archiver,"t",archive],"ARCHIVE_MEMBERS").splitlines()
    need([x.strip() for x in members if x.strip()]==
         ["JWPLC_ModbusRTU.cpp.o"],"ARCHIVE_MEMBERS_INVALID")
    extracted=TMP/"extract"
    extracted.mkdir()
    run([archiver,"x",archive],"ARCHIVE_EXTRACT",cwd=extracted)
    actual=extracted/"JWPLC_ModbusRTU.cpp.o"
    need(actual.is_file() and sha(actual)==sha(obj),
         "ARCHIVE_OBJECT_PARITY_FAILED")
    M["archive_sha256"]=sha(archive)
    M["archive_member_sha256"]=sha(actual)
    say("ARCHIVE_MEMBER_BYTE_PARITY","PASS")
    say("NEW_RTU_ARCHIVE_SHA256",sha(archive))
    stage("ARCHIVE_REBUILD","PASS")
    return candidate,archive

def link_check(output,selected,label,archive):
    matches=[s.replace("\\\\","\\") for s in output.splitlines()
             if "-lJWPLC_ModbusRTU" in s and ".elf" in s]
    need(len(matches)==1,"RTU_LINK_LINE_NOT_UNIQUE_"+label)
    line=matches[0]
    need(re.search(r"""(?<!\S)["']?-lJWPLC_ModbusRTU(?=\s|["']|$)""",line)!=None,
         "RTU_L_FLAG_MISSING_"+label)
    dirs=re.findall(r"""(?<!\S)["']?-L["']?([A-Za-z]:[\\/][^\s"']+)""",line)
    need(archive.parent.resolve() in [Path(s).resolve() for s in dirs],
         "RTU_LINK_PATH_WRONG_"+label)
    need(sha(archive)==M["archive_sha256"],"RTU_LINK_ARCHIVE_SHA_CHANGED_"+label)
    say("RTU_ARCHIVE_LINK_"+label,"PASS_EXACT_L_PATH_L_FLAG_SHA")

def regression(cli,pre,source_rs,archive):
    stage("CANDIDATE_REGRESSIONS","RUNNING")
    done=[]
    for label,sketch in SKETCHES:
        log,build,_=compile(cli,sketch,"CANDIDATE_"+label,pre,source_rs)
        need(not list(build.rglob("JWPLC_ModbusRTU.cpp.o")),
             "PRECOMPILED_RTU_CPP_REBUILT_"+label)
        if label!="RS485_STATUS":
            link_check(log,pre,label,archive)
        done.append(label)
        say("REGRESSION_"+label,"PASS")
    M["candidate_regressions"]=done
    stage("CANDIDATE_REGRESSIONS","PASS")

def audit():
    stage("FINAL_AUDIT","RUNNING")
    need(all(sha(ROOT/name)==value for name,value in M["before_sha256"].items()),
         "PRODUCT_HASH_CHANGED")
    need(git("rev-parse","HEAD")==M["entry_head"],"GIT_HEAD_CHANGED")
    need(not git_paths("diff","--name-only") and
         not git_paths("diff","--cached","--name-only"),"GIT_WORKTREE_CHANGED")
    need(M["candidate_regressions"]==["SLAVE","MASTER","RS485_STATUS"],
         "REGRESSIONS_INCOMPLETE")
    stage("FINAL_AUDIT","PASS")

def main():
    global RES,TMP
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--arduino-cli",default=r"C:\Program Files\Arduino CLI\arduino-cli.exe")
    args=parser.parse_args()
    runid="g8_p1p2_"+datetime.now().strftime("%Y%m%d_%H%M%S")+"_"+uuid.uuid4().hex[:8]
    RES=ROOT/"tools/alpha13/results"/runid
    TMP=Path(tempfile.gettempdir())/("jwplc_a13_"+runid)
    need(not RES.exists() and not TMP.exists(),"RUN_ID_COLLISION")
    RES.mkdir(parents=True)
    TMP.mkdir(parents=True)
    M.update({"schema":"A13-G8-P1P2-CANDIDATE-1","run_id":runid,
              "status":"REVIEW_STOP","phases":{},"product_changed":False,
              "hardware_access":False,"upload":False,"git_commit":False,
              "git_push":False,"result_path":str(RES)})
    try:
        cli=Path(args.arduino_cli).expanduser().resolve()
        preflight(cli)
        source_test()
        source_rtu,source_rs=stage_libraries()
        pre,archive=build_archive(cli,source_rtu,source_rs)
        regression(cli,pre,source_rs,archive)
        audit()
        M["status"]="CANDIDATE_BUILD_PASS_PRODUCT_UNCHANGED"
        M["candidate_archive_path"]=str(archive)
        say("STATUS",M["status"])
        say("RUN_ID",runid)
        say("RESULTS",RES)
        say("PRODUCT_CHANGE","NONE")
        say("PHYSICAL_ACCESS","NONE")
        save()
        return 0
    except Exception as exc:
        M["reason"]=repr(exc)
        say("STATUS","REVIEW_STOP")
        say("REASON",repr(exc))
        say("RESULTS",RES)
        try:save()
        except Exception:pass
        return 2

if __name__=="__main__":
    sys.exit(main())
