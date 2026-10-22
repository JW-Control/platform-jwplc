#!/usr/bin/env python3
"""Alpha13 G10-P1G-C2-S2A: fuente real socketBegin + lease experimental.

Solo overlay para asignación, NO integración con EthernetClient/Server/UDP,
NO adopción, NO flash, NO compilación Arduino. Usa método C++ productivo
extraído y prototipo W5500 C1 compilados con mocks. Falla cerrado.
"""
from __future__ import annotations

import argparse
from datetime import datetime
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import subprocess
import sys
import uuid

ROOT = Path(__file__).resolve().parents[3]
BRANCH = "v2.1.0-alpha.13/feature/cleanup-robustness"
C2_S1_HEAD = "edbb0082f4277b1ab651da370328788ed7206d23"
C2_S1_RUN = "g10_p1g_c2_s1_20261013_101730_2d1e241a"
SOURCE_REL = "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/socket.cpp"
CANDIDATE = ROOT / "tools/alpha13/candidates/g10_p1g_c"
C1_H = CANDIDATE / "JWPLC_W5500_SafeAccess.h"
C1_CPP = CANDIDATE / "JWPLC_W5500_SafeAccess.cpp"
TEST = CANDIDATE / "native_c2_s2a_socket_allocation_test.cpp"
SIG = "uint8_t EthernetClass::socketBegin(uint8_t protocol, uint16_t port)"
EXPECTED = [
    "C2_S2A_REAL_SOCKET_BEGIN_MUTEX_LEASE=PASS",
    "C2_S2A_SAME_INDEX_AND_RESET_STALE=PASS",
    "C2_S2A_BUSY_SKIP_NO_HARDWARE=PASS",
    "C2_S2A_NATIVE_RESULT=PASS_LIMITED_SOCKET_ALLOCATION_ADOPTION_BLOCKED",
]
NEGATIVES = [
    ("REMOVE_SOCKET_BEGIN_GUARD",
     "jwplc_w5500_candidate::BusScope bus(20);\n\tif (!bus) return MAX_SOCK_NUM;",
     "/* removed guard */", "SPI_TRANSACTION_WITHOUT_LOCK"),
    ("REMOVE_SOCKET_LEASE",
     "(void)jwplc_w5500_candidate::allocateSocketLockedAuto(s);",
     "/* removed lease */", "LEASE_NOT_ALLOCATED"),
    ("KEEP_LEGACY_LISTENER_MAP",
     "EthernetServer::server_port[s] = 0;",
     "/* stale listener mapping */", "SERVER_MAP_NOT_INVALIDATED"),
    ("SKIP_HARDWARE_OPEN",
     "W5100.execCmdSn(s, Sock_OPEN);",
     "/* no actual hardware OPEN */", "SOCKET_OPEN_MISSING"),
]
def say(k, v): print("G10_P1G_C2_S2A_" + k + "=" + str(v), flush=True)
def need(ok, why):
    if not ok: raise RuntimeError(why)
def sha(path):
    h=hashlib.sha256()
    with Path(path).open("rb") as f:
        for block in iter(lambda:f.read(1024*1024),b""):
            h.update(block)
    return h.hexdigest()
def git(*args):
    p=subprocess.run(["git","-c","core.quotepath=false",*args],
                     cwd=ROOT,capture_output=True,text=True,errors="replace",timeout=25)
    need(p.returncode==0,"GIT_FAILED_"+repr(args))
    return p.stdout.strip()
def module(path,name):
    spec=importlib.util.spec_from_file_location(name,ROOT/path)
    need(spec is not None and spec.loader is not None,"MODULE_UNAVAILABLE_"+name)
    obj=importlib.util.module_from_spec(spec)
    sys.modules[name]=obj
    spec.loader.exec_module(obj)
    return obj
def preflight(ci):
    need(Path.cwd().resolve()==ROOT and
         Path(git("rev-parse","--show-toplevel")).resolve()==ROOT,
         "REPOSITORY_ROOT_REQUIRED")
    head=git("rev-parse","HEAD")
    need(subprocess.run(["git","merge-base","--is-ancestor",C2_S1_HEAD,head],
         cwd=ROOT,capture_output=True).returncode==0,"C2_S1_HEAD_NOT_ANCESTOR")
    need(not git("diff","--name-only") and not git("diff","--cached","--name-only"),
         "GIT_TRACKED_DIRTY")
    if not ci:
        need(git("branch","--show-current")==BRANCH,"WRONG_BRANCH")
        need(head==git("rev-parse","refs/remotes/origin/"+BRANCH),
             "LOCAL_REMOTE_HEAD_MISMATCH")
        p=ROOT/"tools/alpha13/results"/C2_S1_RUN/"MANIFEST.json"
        need(p.is_file(),"LOCAL_C2_S1_MANIFEST_MISSING")
        obj=json.loads(p.read_text(encoding="utf-8-sig"))
        need(obj.get("schema")=="A13-G10-P1G-C2-S1-REAL-SOURCE-SLICE-1" and
             obj.get("run_id")==C2_S1_RUN and
             obj.get("head")==C2_S1_HEAD and
             obj.get("status")==
               "REAL_SOURCE_MUTEX_SLICE_NATIVE_PASS_INCOMPLETE_ADOPTION_BLOCKED",
             "LOCAL_C2_S1_MANIFEST_NOT_QUALIFIED")
    gate=module("tools/alpha13/gates/a13_g10_p1g_a_mutex_callpaths.py",
                "p1g_c2_s2a_frozen")
    need(gate.check_frozen()==5,"PRECOMPILED_FROZEN_CHANGED")
    gate.check(gate.sources())
    return head
def patch(repro,original):
    body=repro.extract(original,SIG)
    need(body.count("SPI.beginTransaction(SPI_ETHERNET_SETTINGS);")==1,
         "SOURCE_SOCKET_BEGIN_SPI_ANCHOR_DRIFT")
    need(body.count("EthernetServer::server_port[s] = 0;")==1,
         "SOURCE_SOCKET_BEGIN_LISTENER_ANCHOR_DRIFT")
    need(body.count("W5100.execCmdSn(s, Sock_OPEN);")==1,
         "SOURCE_SOCKET_BEGIN_OPEN_ANCHOR_DRIFT")
    begin=SIG+"\n{"
    need(body.startswith(begin),"SOURCE_SOCKET_BEGIN_SIGNATURE_DRIFT")
    body=body.replace(begin,begin+"\n\tjwplc_w5500_candidate::BusScope bus(20);"
        "\n\tif (!bus) return MAX_SOCK_NUM;",1)
    trailer="\tSPI.endTransaction();\n\treturn s;\n}"
    need(body.count(trailer)==1,"SOURCE_RETURN_AFTER_OPEN_DRIFT")
    body=body.replace(trailer,
        "\tSPI.endTransaction();\n"
        "\t// Solo lease candidato: API Client/Server aun no transporta el token.\n"
        "\t(void)jwplc_w5500_candidate::allocateSocketLockedAuto(s);\n"
        "\treturn s;\n}",1)
    need(original.count(repro.extract(original,SIG))==1,
         "SOURCE_FUNCTION_NOT_UNIQUE")
    return ('#include "JWPLC_W5500_SafeAccess.h"\n'+
            original.replace(repro.extract(original,SIG),body,1)),body
def run_native(out,label,body,c1):
    run=out/label
    run.mkdir(parents=True,exist_ok=False)
    inc=run/"include"
    (inc/"freertos").mkdir(parents=True)
    (inc/"freertos"/"task.h").write_text(c1.TASK_H,encoding="utf-8")
    (inc/"jwplc_spi_bus.h").write_text(c1.SPI_H,encoding="utf-8")
    (run/"C2_S2A_PATCHED_SOCKET_BEGIN.inc").write_text(
        "// Extracted byte-for-byte from S2A overlay\n"+body+"\n",
        encoding="utf-8")
    host,kind=c1.compiler()
    exe=run/("c2_s2a.exe" if os.name=="nt" else "c2_s2a")
    if kind=="msvc":
        cmd=[host,"/nologo","/EHsc","/std:c++17","/W4",
             "/I"+str(inc),"/I"+str(run),"/I"+str(CANDIDATE),
             str(C1_CPP),str(TEST),"/Fe:"+str(exe)]
    elif kind=="msvc-vcvars":
        args=["cl.exe","/nologo","/EHsc","/std:c++17","/W4",
              "/I"+str(inc),"/I"+str(run),"/I"+str(CANDIDATE),
              str(C1_CPP),str(TEST),"/Fe:"+str(exe)]
        bat=run/"vcvars_compiler.bat"
        bat.write_text('@echo off\ncall "'+host+'" >nul\n'
                       'if errorlevel 1 exit /b 91\n'+
                       subprocess.list2cmdline(args)+'\n'
                       'exit /b %ERRORLEVEL%\n',encoding="utf-8")
        cmd=["cmd.exe","/d","/c",str(bat)]
    else:
        cmd=[host,"-std=c++17","-O0","-Wall","-Wextra",
             "-I"+str(inc),"-I"+str(run),"-I"+str(CANDIDATE),
             str(C1_CPP),str(TEST),"-o",str(exe)]
    env=os.environ.copy()
    if os.name=="nt" and kind=="gcc":
        env["PATH"]=str(Path(host).resolve().parent)+os.pathsep+env.get("PATH","")
    cp=subprocess.run(cmd,cwd=run,capture_output=True,text=True,errors="replace",
                      env=env,timeout=120)
    (run/"compile.log").write_text(
        "COMMAND="+repr(cmd)+"\nEXIT="+str(cp.returncode)+"\n"+
        cp.stdout+"\n"+cp.stderr,encoding="utf-8")
    if cp.returncode!=0:
        for line in (cp.stderr+"\n"+cp.stdout).splitlines()[-24:]:
            if line.strip():say("COMPILER_DIAG_"+label,line[:360])
    need(cp.returncode==0 and exe.is_file(),"NATIVE_COMPILE_FAILED_"+label)
    proc=subprocess.run([str(exe)],cwd=run,capture_output=True,text=True,
                        errors="replace",env=env,timeout=15)
    text=proc.stdout+"\n"+proc.stderr
    (run/"native.log").write_text(
        "EXIT="+str(proc.returncode)+"\n"+text,encoding="utf-8")
    return proc.returncode,text
def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--ci-standalone",action="store_true")
    opts=ap.parse_args()
    runid=("g10_p1g_c2_s2a_"+datetime.now().strftime("%Y%m%d_%H%M%S")
           +"_"+uuid.uuid4().hex[:8])
    out=ROOT/"tools/alpha13/results"/runid
    out.mkdir(parents=True,exist_ok=False)
    manifest={"schema":"A13-G10-P1G-C2-S2A-SOCKET-BEGIN-LEASE-1",
              "run_id":runid,"status":"REVIEW_STOP",
              "product_modifications":False,"gpio4_enabled":False,
              "hardware_access":False,"arduino_build":False,
              "candidate_adopted":False}
    try:
        head=preflight(opts.ci_standalone)
        repro=module("tools/alpha13/gates/a13_g10_p1a_a13_006_repro.py",
                     "p1g_c2_s2a_repro")
        c1=module("tools/alpha13/gates/a13_g10_p1g_c1_native_prototype.py",
                  "p1g_c2_s2a_compiler")
        original=(ROOT/SOURCE_REL).read_text(encoding="utf-8-sig")
        overlay,body=patch(repro,original)
        target=out/"overlay"/SOURCE_REL
        target.parent.mkdir(parents=True,exist_ok=True)
        target.write_text(overlay,encoding="utf-8")
        result,log=run_native(out,"positive",body,c1)
        need(result==0 and all(x in log for x in EXPECTED),
             "POSITIVE_SOURCE_NATIVE_FAILED")
        say("POSITIVE_NATIVE","PASS_REAL_SOCKET_BEGIN_LEASE")
        for key,before,after,expected in NEGATIVES:
            need(body.count(before)==1,"NEGATIVE_ANCHOR_"+key)
            mod=body.replace(before,after,1)
            exit_code,log=run_native(out,"negative_"+key,mod,c1)
            if not(exit_code!=0 and expected in log):
                say("NEGATIVE_DIAG_"+key,
                    str(exit_code)+"_"+log.strip().replace("\n"," | ")[:400])
            need(exit_code!=0 and expected in log,
                 "NEGATIVE_NOT_REJECTED_"+key)
            say("NEGATIVE_"+key,"PASS_REJECTED")
        manifest.update({
            "head":head,
            "status":"REAL_SOCKET_BEGIN_LEASE_NATIVE_PASS_INCOMPLETE_ADOPTION_BLOCKED",
            "method_source":SOURCE_REL,"method":SIG,
            "method_count":1,
            "source_sha256":sha(ROOT/SOURCE_REL),
            "overlay_sha256":sha(target),
            "candidate_core_sha256":sha(C1_CPP),
            "native_test_sha256":sha(TEST),
            "negative_mutations":len(NEGATIVES),
            "known_gap":"Client Server UDP no transportan ni validan token",
            "pending":"socketClose StopAsync CancelStopAsync DHCP DNS RTU listeners",
            "product_modified":False})
        say("HEAD",head)
        say("REAL_METHODS",1)
        say("NEGATIVES",len(NEGATIVES))
        say("ARCHIVES_FROZEN",5)
        say("PRODUCT_CHANGED","NO")
        say("STATUS",manifest["status"])
        say("RESULTS",out)
        return 0
    except Exception as ex:
        manifest["reason"]=repr(ex)
        say("STATUS","REVIEW_STOP")
        say("REASON",repr(ex))
        say("RESULTS",out)
        return 2
    finally:
        (out/"MANIFEST.json").write_text(
            json.dumps(manifest,ensure_ascii=False,indent=2)+"\n",
            encoding="utf-8")
if __name__=="__main__":
    try:sys.exit(main())
    except Exception as ex:
        say("FATAL",repr(ex))
        sys.exit(2)
