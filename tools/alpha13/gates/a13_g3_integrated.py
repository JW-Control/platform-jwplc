#!/usr/bin/env python3
"""A13-G3 integrated TCA RMW/shadow qualification. Candidate is local-only.

The gate protects baseline source/archive, adopts four source candidates,
rebuilds the official precompiled core using existing tooling, verifies normal
Arduino linking, compiles a fresh physical probe, asks permission for load-free
hardware exercise, checks unique serial provenance, and leaves exact product
changes UNCOMMITTED only on PASS. All failures restore source/core backups.
"""
from __future__ import annotations
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile
import time
import uuid
from datetime import datetime

ROOT = Path(__file__).resolve().parents[3]
BRANCH = "v2.1.0-alpha.13/feature/cleanup-robustness"
BASE = "c33aff2bad3c3a94e988ae030ceb99ad241c9eef"
FQBN = "jwplc_local:esp32:jwplcbasic"
CORE_REL = "JWPLC/2.1.0/precompiled/core/JWPLCBASIC/core.a"
CORE_SHA = "6f328eeb796091070c8d852a2c0e90f71047d8d2b5786fe3cd5079b2fb6ff983"
SOURCE_MAP = {
    "JWPLC/2.1.0/cores/jwcontrol/peripherals/src/jwplc_i2c_bridge.cpp":
       ("tools/alpha13/candidates/g3/jwplc_i2c_bridge.cpp", "6bc35a5770442cee65dd1d9ddd91915383e99fcc"),
    "JWPLC/2.1.0/cores/jwcontrol/peripherals/include/jwplc_i2c_bridge.h":
       ("tools/alpha13/candidates/g3/jwplc_i2c_bridge.h", "1d0b1937ff4b6f327affb500d09a9f4e1a60760a"),
    "JWPLC/2.1.0/cores/jwcontrol/peripherals/src/peripheral-tca6424a.c":
       ("tools/alpha13/candidates/g3/peripheral-tca6424a.c", "c580169d53362f4cbf85278e67acc603e204dcea"),
    "JWPLC/2.1.0/cores/jwcontrol/jwplc_peripherals.cpp":
       ("tools/alpha13/candidates/g3/jwplc_peripherals.cpp", "c3d53566d274e95b7dda111327140b5393f7db35"),
}
ID = datetime.now().strftime("%Y%m%d_%H%M%S") + "_" + uuid.uuid4().hex[:8]
RUN = ROOT / "tools/alpha13/results" / ("g3_integrated_" + ID)
TEMP = Path(tempfile.gettempdir()) / ("jwplc_a13_g3_" + ID)
BACKUP = TEMP / "originals"
RES = {"hito": "A13-G3", "run_id": ID, "status": "REVIEW", "phases": {},
       "product_committed": False, "release_published": False,
       "backup": str(BACKUP), "output": str(RUN)}
PHASE = "INIT"
ADOPTED = False

class Stop(RuntimeError):
    def __init__(self, msg, kind="PRECONDITION"):
        super().__init__(msg)
        self.kind = kind

def need(ok, msg, kind="PRECONDITION"):
    if not ok:
        raise Stop(msg, kind)

def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()

def git(*args, check=True):
    p = subprocess.run(["git", *args], cwd=ROOT, stdout=subprocess.PIPE,
                       stderr=subprocess.PIPE)
    if check and p.returncode:
        raise Stop("GIT_" + "_".join(args) + "=" + p.stderr.decode("utf-8", "replace")[-400:])
    return p

def emit(key, val):
    print(f"{key}={val}", flush=True)

def save():
    RUN.mkdir(parents=True, exist_ok=True)
    (RUN / "MANIFEST.json").write_text(json.dumps(RES, indent=2, ensure_ascii=False)+"\n", encoding="utf-8")
    summary = ["HITO=A13-G3", "STATUS="+RES["status"], "RUN_ID="+ID,
               "CURRENT_PHASE="+PHASE, "RESULTS="+str(RUN), "BACKUP="+str(BACKUP)]
    summary += [k+"="+str(v) for k,v in RES["phases"].items()]
    for key in ("reason", "category", "core_before", "core_after", "app_sha256", "serial_port"):
        if key in RES: summary.append(key.upper()+"="+str(RES[key]))
    summary += ["PRODUCT_COMMIT=NO", "PUSH=NO", "RELEASE_MERGE=NO"]
    (RUN / "SUMMARY.log").write_text("\n".join(summary)+"\n", encoding="utf-8")

def stage(name, state):
    global PHASE
    PHASE = name
    RES["phases"][name] = state
    emit("PHASE", name+"_"+state)
    save()

def command(argv, label, cwd=None):
    log = RUN / (label+".log")
    p = subprocess.run([str(x) for x in argv], cwd=cwd or ROOT,
                       stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                       encoding="utf-8", errors="backslashreplace")
    output = p.stdout or ""
    log.write_text("ARGS="+repr(argv)+"\nEXIT="+str(p.returncode)+"\n"+output, encoding="utf-8")
    emit(label+"_EXIT", p.returncode)
    if p.returncode:
        emit(label+"_TAIL", "\\n".join(output.splitlines()[-9:]))
        raise Stop(label+"_EXIT_"+str(p.returncode), "PRODUCT_OR_ENVIRONMENT")
    return output

def find_program(exe):
    if exe:
        p = Path(exe)
        need(p.is_file() or shutil.which(exe), "PROGRAM_NOT_FOUND:"+exe, "ENVIRONMENT")
        return exe
    usual = Path(r"C:\Program Files\Arduino PLC IDE Tools\arduino-cli.exe")
    if usual.is_file():
        return str(usual)
    result = shutil.which("arduino-cli")
    need(result is not None, "ARDUINO_CLI_MISSING", "ENVIRONMENT")
    return result

def proof_contract():
    candidate = {k:(ROOT / v[0]).read_text(encoding="utf-8")
                 for k,v in SOURCE_MAP.items()}
    bridge = candidate[next(k for k in candidate if k.endswith("jwplc_i2c_bridge.cpp"))]
    tca = candidate[next(k for k in candidate if k.endswith("peripheral-tca6424a.c"))]
    runtime = candidate[next(k for k in candidate if k.endswith("/jwplc_peripherals.cpp"))]
    header = candidate[next(k for k in candidate if k.endswith("jwplc_i2c_bridge.h"))]
    need("int jwplcI2C_transactionBegin(void);" in header, "NO_TRANSACTION_DECL")
    block = bridge.split("int jwplcI2C_updateBit(",1)[1]
    need("if (!jwplcI2CLock())" in block and
         block.find("jwplcI2CLock()") < block.find("jwplcI2C_readReg8(") <
         block.find("jwplcI2C_writeReg8(") < block.find("jwplcI2CUnlock()"),
         "RMW_LOCK_SCOPE_INVALID")
    for name in ("TCA6424A_init(", "TCA6424A_writePin(", "TCA6424A_writeBank("):
        segment=tca.split("bool "+name,1)[1].split("\n}",1)[0]
        need("jwplcI2C_transactionBegin()" in segment and
             "jwplcI2C_transactionEnd()" in segment, "SHADOW_LOCK_INVALID:"+name)
    for name in ("jwplcSystemSetOutputShadow(", "JWPLC_writeOutputs(", "jwplc_digitalWrite("):
        segment=runtime.split("void "+name,1)[1].split("\n}",1)[0]
        need("jwplcI2C_transactionBegin()" in segment and
             "jwplcI2C_transactionEnd()" in segment, "RUNTIME_LOCK_INVALID:"+name)
    need(tca.count("g_outputShadowValid[bank] = false;") >= 2, "FAILED_WRITE_INVALIDATION_MISSING")
    RES["source_contract"] = "PASS"

def preflight():
    stage("PREFLIGHT", "RUNNING")
    need(git("branch","--show-current").stdout.decode().strip()==BRANCH, "WRONG_BRANCH")
    head=git("rev-parse","HEAD").stdout.decode().strip()
    need(git("merge-base","--is-ancestor",BASE,head,check=False).returncode==0,
         "G3_BASELINE_NOT_ANCESTOR")
    need(not git("diff","--name-only").stdout.strip() and
         not git("diff","--cached","--name-only").stdout.strip(), "TRACKED_WORKTREE_NOT_CLEAN")
    need(not git("ls-files","--others","--exclude-standard").stdout.strip(),
         "UNTRACKED_FILES_PRESENT_REVIEW")
    RES["head"]=head
    for dst,(candidate,blob) in SOURCE_MAP.items():
        need(git("rev-parse","HEAD:"+dst).stdout.decode().strip()==blob,
             "BASELINE_BLOB_CHANGED:"+dst)
        need((ROOT/candidate).is_file(), "CANDIDATE_MISSING:"+candidate)
        need((ROOT/dst).is_file(), "SOURCE_MISSING:"+dst)
    need(sha(ROOT/CORE_REL)==CORE_SHA, "BASE_CORE_SHA_CHANGED")
    local_board=ROOT/"JWPLC/2.1.0/boards.local.txt"
    RES["boards_local_before"]=sha(local_board) if local_board.is_file() else None
    need((ROOT/"tools/build-speed-benchmark/Build-JWPLCPrecompiledCore.ps1").is_file(),
         "BUILD_TOOLING_MISSING")
    need((ROOT/"tools/build-speed-benchmark/Verify-JWPLCPrecompiledCore.ps1").is_file(),
         "VERIFY_TOOLING_MISSING")
    need((ROOT/"tools/alpha13/firmware/a13_g3_concurrent_probe/a13_g3_concurrent_probe.ino").is_file(),
         "PHYSICAL_SKETCH_MISSING")
    proof_contract()
    RES["core_before"]=CORE_SHA
    RES["baseline_sha256"]={p:sha(ROOT/p) for p in SOURCE_MAP}
    RES["candidate_sha256"]={p:sha(ROOT/q[0]) for p,q in SOURCE_MAP.items()}
    stage("PREFLIGHT","PASS")

def adoption():
    global ADOPTED
    stage("ADOPTION","RUNNING")
    BACKUP.mkdir(parents=True,exist_ok=False)
    for path in list(SOURCE_MAP)+[CORE_REL]:
        out=BACKUP/path
        out.parent.mkdir(parents=True,exist_ok=True)
        shutil.copy2(ROOT/path,out)
    (RUN/"BACKUPS.txt").write_text("\n".join(str(BACKUP/path)+" SHA="+sha(BACKUP/path)
                                         for path in list(SOURCE_MAP)+[CORE_REL])+"\n",encoding="utf-8")
    ADOPTED=True
    for path,(candidate,blob) in SOURCE_MAP.items():
        shutil.copyfile(ROOT/candidate,ROOT/path)
        need(sha(ROOT/path)==sha(ROOT/candidate), "ADOPTION_COPY_MISMATCH:"+path)
    need(git("diff","--check",check=False).returncode==0,"DIFF_CHECK_FAILED")
    dirty=set(git("diff","--name-only").stdout.decode().splitlines())
    need(dirty==set(SOURCE_MAP), "ADOPTION_DIRTY_SCOPE_MISMATCH:"+repr(sorted(dirty)))
    stage("ADOPTION","PASS")

def rollback():
    if not ADOPTED: return
    for path in list(SOURCE_MAP)+[CORE_REL]:
        src=BACKUP/path
        if src.is_file():
            shutil.copy2(src,ROOT/path)
    restored=all(sha(ROOT/p)==sha(BACKUP/p) for p in list(SOURCE_MAP)+[CORE_REL])
    RES["rollback"]="PASS" if restored else "FAIL"
    emit("ROLLBACK",RES["rollback"])

def rebuild(cli,ps):
    stage("SOURCE_TO_CORE_ARCHIVE","RUNNING")
    output=command([ps,"-NoProfile","-ExecutionPolicy","Bypass","-File",
                    ROOT/"tools/build-speed-benchmark/Build-JWPLCPrecompiledCore.ps1",
                    "-ArduinoCli",cli,"-Targets","Basic","-OutputRoot",TEMP/"core_build"],
                   "BUILD_OFFICIAL_CORE")
    need("CORE_PRECOMPILED_BUILD=PASS" in output,"SOURCE_CORE_BUILD_CONTRACT_MISSING")
    need(sha(ROOT/CORE_REL)!=CORE_SHA,"CORE_UNCHANGED_AFTER_SOURCE_PATCH")
    RES["core_after"]=sha(ROOT/CORE_REL)
    RES["core_bytes"]=(ROOT/CORE_REL).stat().st_size
    stage("SOURCE_TO_CORE_ARCHIVE","PASS")
    stage("NORMAL_PRECOMPILED_LINK","RUNNING")
    v=command([ps,"-NoProfile","-ExecutionPolicy","Bypass","-File",
               ROOT/"tools/build-speed-benchmark/Verify-JWPLCPrecompiledCore.ps1",
               "-Target","Basic","-ArduinoCli",cli,"-OutputRoot",TEMP/"core_verify"],
              "VERIFY_OFFICIAL_CORE")
    need("CORE_PRECOMPILED_VERIFY_BASIC=PASS" in v, "NORMAL_LINK_CONTRACT_MISSING")
    board=ROOT/"JWPLC/2.1.0/boards.local.txt"
    after=sha(board) if board.is_file() else None
    need(after==RES["boards_local_before"],"BOARDS_LOCAL_NOT_RESTORED")
    stage("NORMAL_PRECOMPILED_LINK","PASS")

def build_physical(cli):
    stage("PHYSICAL_PROBE_BUILD","RUNNING")
    src=ROOT/"tools/alpha13/firmware/a13_g3_concurrent_probe/a13_g3_concurrent_probe.ino"
    dest=TEMP/"a13_g3_concurrent_probe"
    dest.mkdir(parents=True,exist_ok=True)
    text=src.read_text(encoding="utf-8")
    need(text.count("__G3_TOKEN__")==1,"PROBE_TOKEN_CONTRACT_CHANGED")
    token=ID+"_"+uuid.uuid4().hex
    text=text.replace("__G3_TOKEN__",token)
    (dest/(dest.name+".ino")).write_text(text,encoding="utf-8")
    path=TEMP/"g3_physical_build"
    out=command([cli,"compile","--fqbn",FQBN,"-j","0","-v","--clean",
                 "--build-path",path,"--libraries",ROOT/"JWPLC/2.1.0/libraries",dest],
                "COMPILE_G3_PROBE")
    need(re.search(r"Using core 'jwcontrol_precompiled_stub'",out)!=None,
         "NORMAL_STUB_NOT_SELECTED")
    need(re.search(r"precompiled[/\\]core[/\\]JWPLCBASIC[/\\]core\.a",out)!=None,
         "CORE_A_NOT_LINKED_IN_PROBE")
    bin_path=path/(dest.name+".ino.bin")
    need(bin_path.is_file(),"APP_BINARY_MISSING")
    RES["app_sha256"]=sha(bin_path)
    RES["token"]=token
    stage("PHYSICAL_PROBE_BUILD","PASS")
    return path,token

def regressions(cli):
    stage("REGRESSION_NORMAL_CONSUMERS","RUNNING")
    libs=ROOT/"JWPLC/2.1.0/libraries"
    cases=(
        ("IDLE_STATUS",libs/"JWPLC_Display/examples/01.Display_IDLE_Status"),
        ("HMI_FIELDS",libs/"JWPLC_Display/examples/02.Display_HMI_Fields"),
        ("LOGIC_RUNTIME_UI",libs/"JWPLC_LogicRuntime_UI/examples/JWPLC_LogicRuntime_UI_Home"),
    )
    passed=[]
    for name,sketch in cases:
        need(sketch.is_dir(),"REGRESSION_SKETCH_MISSING:"+name,"HARNESS")
        out=command([cli,"compile","--fqbn",FQBN,"-j","0","-v","--clean",
                     "--build-path",TEMP/("reg_"+name),
                     "--libraries",libs,sketch],"COMPILE_REG_"+name)
        need("Using core 'jwcontrol_precompiled_stub'" in out,
             "REG_STUB_NOT_USED:"+name,"HARNESS")
        need(re.search(r"precompiled[/\\]core[/\\]JWPLCBASIC[/\\]core\.a",out)!=None,
             "REG_ARCHIVE_NOT_LINKED:"+name,"HARNESS")
        passed.append(name)
    RES["regressions"]=passed
    stage("REGRESSION_NORMAL_CONSUMERS","PASS")

def choose_port(requested):
    from serial.tools import list_ports
    ports=list(list_ports.comports())
    full=[(p.device,p.vid,p.pid) for p in ports]
    RES["ports"]=full
    names=[p.device for p in ports if p.device.upper()!="COM1"]
    if requested:
        need(requested in [p.device for p in ports],
             "REQUESTED_PORT_NOT_VISIBLE:"+requested,"ENVIRONMENT")
        selected=requested
    else:
        need(len(names)==1,"SERIAL_PORT_AMBIGUOUS:"+repr(names),"ENVIRONMENT")
        selected=names[0]
    RES["serial_port"]=selected
    emit("SERIAL_PORT",selected)
    return selected

def physical(cli,build,token,serial_port):
    stage("PHYSICAL_SAFETY_CHECK","RUNNING")
    emit("WARNING","La prueba energiza Q0_0 y Q0_1 repetidamente. Desconectar TODAS las cargas.")
    answer=input("Confirmar equipo aislado de actuadores y cargas (escribir DESCONECTADAS): ").strip()
    need(answer=="DESCONECTADAS","PHYSICAL_TEST_NOT_AUTHORIZED","PRECONDITION")
    from serial import Serial,SerialException
    stage("PHYSICAL_UPLOAD","RUNNING")
    command([cli,"upload","--fqbn",FQBN,"--port",serial_port,"--input-dir",build],
            "UPLOAD_G3_PROBE")
    stage("PHYSICAL_UPLOAD","PASS")
    stage("PHYSICAL_SERIAL","RUNNING")
    captured=[]
    try:
        with Serial(serial_port,115200,timeout=.25) as s:
            ready=False
            limit=time.monotonic()+22
            while time.monotonic()<limit and not ready:
                line=s.readline().decode("utf-8","backslashreplace").strip()
                if line: captured.append(line)
                if line=="G3_READY="+token: ready=True
            need(ready,"G3_READY_TIMEOUT","ENVIRONMENT_OR_HARDWARE")
            s.write(("G3_RUN:"+token+"\n").encode("ascii"))
            done=False
            limit=time.monotonic()+55
            while time.monotonic()<limit and not done:
                line=s.readline().decode("utf-8","backslashreplace").strip()
                if line: captured.append(line)
                if line=="G3_DONE="+token: done=True
            need(done,"G3_DONE_TIMEOUT","HARDWARE_OR_PRODUCT")
    except (OSError,SerialException) as e:
        raise Stop("SERIAL_EXCEPTION:"+str(e),"ENVIRONMENT")
    finally:
        (RUN/"serial_g3.log").write_text("\n".join(captured)+"\n",encoding="utf-8")
    def key(name):
        matching=[x.split("=",1)[1] for x in captured if x.startswith(name+"=")]
        need(len(matching)==1,name+"_CONTRACT_"+str(len(matching)),"HARNESS")
        return matching[0]
    need(key("G3_RESULT")=="PASS", "G3_HARDWARE_CONCURRENT_REGRESSION","PRODUCT")
    need(key("G3_ERRORS")=="0","G3_HARDWARE_BIT_ERRORS","PRODUCT")
    need(key("G3_TRIALS")=="150","G3_TRIAL_COUNT_CHANGED","HARNESS")
    need(key("G3_FINAL_OUTPUT")=="0","OUTPUT_NOT_RESET_TO_ZERO","HARDWARE")
    RES["physical_serial"]="PASS"
    stage("PHYSICAL_SERIAL","PASS")

def finalize():
    stage("FINAL_AUDIT","RUNNING")
    dirty=set(git("diff","--name-only").stdout.decode().splitlines())
    desired=set(SOURCE_MAP)|{CORE_REL}
    need(dirty==desired,"FINAL_PRODUCT_SCOPE_UNEXPECTED:"+repr(sorted(dirty)))
    need(git("diff","--check",check=False).returncode==0,"FINAL_DIFF_CHECK_FAILED")
    need(not git("diff","--cached","--name-only").stdout.strip(),"STAGED_CHANGES_NOT_ALLOWED")
    for path,(candidate,_) in SOURCE_MAP.items():
        need(sha(ROOT/path)==sha(ROOT/candidate),"PRODUCT_SOURCE_CHANGED:"+path)
    need(sha(ROOT/CORE_REL)==RES["core_after"],"CORE_CHANGED_AFTER_VERIFY")
    stage("FINAL_AUDIT","PASS")
    RES["product_paths"]=sorted(desired)

def run():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--serial-port",default="")
    ap.add_argument("--arduino-cli",default=None)
    ap.add_argument("--skip-physical",action="store_true",
                    help="Compile/rebuild/link only; leaves candidate locally uncommitted.")
    args=ap.parse_args()
    RUN.mkdir(parents=True,exist_ok=False)
    TEMP.mkdir(parents=True,exist_ok=False)
    try:
        preflight()
        cli=find_program(args.arduino_cli)
        ps=shutil.which("powershell.exe") or shutil.which("powershell") or shutil.which("pwsh")
        need(ps,"POWERSHELL_NOT_FOUND","ENVIRONMENT")
        need(shutil.which("python") or sys.executable,"PYTHON_NOT_FOUND","ENVIRONMENT")
        if not args.skip_physical:
            try: import serial
            except ImportError: raise Stop("PYSERIAL_NOT_INSTALLED","ENVIRONMENT")
            port=choose_port(args.serial_port)
        adoption()
        rebuild(cli,ps)
        build,token=build_physical(cli)
        regressions(cli)
        if not args.skip_physical:
            physical(cli,build,token,port)
        else:
            RES["physical_status"]="NOT_EXECUTED"
        finalize()
        RES["status"]="PASS_PHYSICAL_AND_NORMAL_CORE" if not args.skip_physical else "PASS_STATIC_PHYSICAL_PENDING"
        RES["next_gate"]="G3_CLOSURE_AFTER_REVIEW"
        emit("STATUS",RES["status"])
        emit("PRODUCT_DIRTY_FILES",len(RES["product_paths"]))
        emit("CORE_NEW_SHA256",RES["core_after"])
        emit("NEXT_GATE",RES["next_gate"])
        save()
        return 0
    except (Stop,Exception) as e:
        RES["reason"]=str(e)
        RES["category"]=e.kind if isinstance(e,Stop) else "HARNESS"
        rollback()
        RES["status"]="REVIEW" if RES["category"] not in ("PRODUCT","HARDWARE") else "FAIL"
        emit("STATUS",RES["status"])
        emit("REASON",RES["reason"])
        emit("CATEGORY",RES["category"])
        save()
        return 2

if __name__=="__main__":
    sys.exit(run())
