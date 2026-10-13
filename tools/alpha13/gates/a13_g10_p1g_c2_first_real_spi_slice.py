#!/usr/bin/env python3
"""G10-P1G-C2-S1: overlay real de 8 metodos mutex SPI y C++ nativo.

Slice INCOMPLETO intencionadamente: Client.connected/socketStatus y wrappers
runtime/Modbus, no protege todas las APIs ni sockets/lease/reset del producto.
Nunca modificar las fuentes productivas; no adoptar aunque native PASS.
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

ROOT=Path(__file__).resolve().parents[3]
BRANCH="v2.1.0-alpha.13/feature/cleanup-robustness"
PREV_RUN="g10_p1g_c1_20261013_092038_8703b3da"
PREV_HEAD="7750917312bfb46cf2b7a1c719abc1b7f8fa5fe4"
ETH="JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/"
MB="JWPLC/2.1.0/libraries/JWPLC_ModbusTCP/src/"
TARGETS={
    "RUNTIME":ETH+"JWPLC_Ethernet.cpp",
    "MB_SERVER":MB+"JWPLC_ModbusTCP.cpp",
    "MB_CLIENT":MB+"JWPLC_ModbusTCP_Client.cpp",
    "RAW_CLIENT":ETH+"EthernetClient.cpp",
    "SOCKET":ETH+"socket.cpp",
}
SIGS={
    "RUNTIME":[
        "bool JWPLC_EthernetClass::acquireBus(uint32_t timeoutMs)",
        "void JWPLC_EthernetClass::releaseBus()"],
    "MB_SERVER":[
        "bool JWPLC_ModbusTCPClass::acquireBus(uint32_t timeoutMs)",
        "void JWPLC_ModbusTCPClass::releaseBus()"],
    "MB_CLIENT":[
        "bool JWPLC_ModbusTCPClientClass::acquireBus(uint32_t timeoutMs)",
        "void JWPLC_ModbusTCPClientClass::releaseBus()"],
    "RAW_CLIENT":["uint8_t EthernetClient::connected()"],
    "SOCKET":["uint8_t EthernetClass::socketStatus(uint8_t s)"],
}
CBASE=ROOT/"tools/alpha13/candidates/g10_p1g_c"
TEST=CBASE/"native_c2_spi_slice_test.cpp"
CSOURCE=CBASE/"JWPLC_W5500_SafeAccess.cpp"
HEADER=CBASE/"JWPLC_W5500_SafeAccess.h"
EXPECT=[
    "C2_REAL_SOURCE_DIRECT_AND_NESTED_SPI=PASS",
    "C2_SPI_BUSY_NO_W5500_ACCESS=PASS_WITH_SEMANTIC_GAP",
    "C2_NATIVE_RESULT=PASS_LIMITED_SLICE_ADOPTION_BLOCKED",
]
NEGATIVE_MUTATIONS={
    "NO_RAW_CLIENT_SCOPE":("RAW_CLIENT",
        "jwplc_w5500_candidate::BusScope bus(20);\n\tif (!bus) return 0;",
        "/* no raw client lock */","AVAILABLE_OUTSIDE_MUTEX"),
    "NO_SOCKET_SCOPE":("SOCKET",
        "jwplc_w5500_candidate::BusScope bus(20);\n\tif (!bus) return SnSR::CLOSED;",
        "/* no socket status lock */","SOCKET_STATUS_OUTSIDE_MUTEX"),
    "MB_CLIENT_RAW_SPI_LOCK":("MB_CLIENT",
        "jwplc_w5500_candidate::enter(timeoutMs)",
        "jwplcSPI_acquire(timeoutMs)","NESTED_CLIENT_DEADLOCK_OR_FALSE_DISCONNECT"),
    "MB_SERVER_RAW_RELEASE":("MB_SERVER",
        "jwplc_w5500_candidate::leave();",
        "jwplcSPI_release();","MUTEX_RELEASE_BY_NONOWNER"),
}
def say(k,v):print("G10_P1G_C2_S1_"+str(k)+"="+str(v),flush=True)
def need(ok,msg):
    if not ok:raise RuntimeError(msg)
def sha(path):
    h=hashlib.sha256()
    with Path(path).open("rb") as f:
        for block in iter(lambda:f.read(1024*1024),b""):h.update(block)
    return h.hexdigest()
def git(*args):
    x=subprocess.run(["git","-c","core.quotepath=false",*args],
                     cwd=ROOT,capture_output=True,text=True,
                     errors="replace",timeout=25)
    need(x.returncode==0,"GIT_"+repr(args))
    return x.stdout.strip()
def load_module(path,name):
    spec=importlib.util.spec_from_file_location(name,ROOT/path)
    need(spec is not None and spec.loader is not None,
         "MODULE_NOT_FOUND_"+name)
    m=importlib.util.module_from_spec(spec)
    sys.modules[name]=m
    spec.loader.exec_module(m)
    return m

def patch_functions(repro,orig):
    result={}
    for key,rel in TARGETS.items():
        old=orig[key]
        now=old
        for sig in SIGS[key]:
            body=repro.extract(now,sig)
            modified=body
            if sig.endswith("::acquireBus(uint32_t timeoutMs)"):
                need(body.count("jwplcSPI_acquire(timeoutMs)")==1,
                     "ACQUIRE_ANCHOR_DRIFT_"+key)
                modified=body.replace("jwplcSPI_acquire(timeoutMs)",
                     "jwplc_w5500_candidate::enter(timeoutMs)",1)
                if key in ("MB_CLIENT","MB_SERVER"):
                    need(modified.count("    jwplcSPI_deselectAll();\n")==1,
                         "DESELECT_ANCHOR_DRIFT_"+key)
                    modified=modified.replace("    jwplcSPI_deselectAll();\n",
                         "    // enter() deselects on first acquire only.\n",1)
            elif sig.endswith("::releaseBus()"):
                need(body.count("jwplcSPI_release();")==1,
                     "RELEASE_ANCHOR_DRIFT_"+key)
                modified=body.replace("jwplcSPI_release();",
                     "jwplc_w5500_candidate::leave();",1)
            elif sig=="uint8_t EthernetClient::connected()":
                need(body.startswith(sig+"\n{"),"CONNECTED_BODY_DRIFT")
                modified=body.replace(sig+"\n{",
                     sig+"\n{\n\tjwplc_w5500_candidate::BusScope bus(20);"
                     "\n\tif (!bus) return 0;",1)
            elif sig=="uint8_t EthernetClass::socketStatus(uint8_t s)":
                need(body.startswith(sig+"\n{"),"STATUS_BODY_DRIFT")
                modified=body.replace(sig+"\n{",
                     sig+"\n{\n\tjwplc_w5500_candidate::BusScope bus(20);"
                     "\n\tif (!bus) return SnSR::CLOSED;",1)
            else:
                raise RuntimeError("UNRECOGNIZED_SIGNATURE_"+sig)
            need(modified!=body and now.count(body)==1,
                 "PATCH_NOT_UNIQUE_"+sig)
            now=now.replace(body,modified,1)
        now='#include "JWPLC_W5500_SafeAccess.h"\n'+now
        need(now!=old,"OVERLAY_UNCHANGED_"+key)
        result[key]=now
    need(len(result)==5,"OVERLAY_TARGETS_NOT_FIVE")
    return result

def methods_include(repro,overlay):
    # 8 bodies exactos extraidos del overlay.
    funcs=[repro.extract(overlay[k],sig)
           for k in TARGETS for sig in SIGS[k]]
    need(len(funcs)==8,"EXTRACTED_METHODS_NOT_EIGHT")
    return ("// Generated by a13_g10_p1g_c2_first_real_spi_slice.py\n"
            "// Actual method bodies from source patch overlay, not mock methods.\n"
            +"\n\n".join(funcs)+"\n")

def native_build(out,tag,include_text,c1,host,kind):
    folder=out/tag
    folder.mkdir(parents=True,exist_ok=False)
    inc=folder/"include"
    (inc/"freertos").mkdir(parents=True)
    task_h=load_module("tools/alpha13/gates/a13_g10_p1g_c1_native_prototype.py",
                       "p1g_c2_c1_stubs_"+tag.replace("-","_"))
    (inc/"freertos"/"task.h").write_text(task_h.TASK_H,encoding="utf-8")
    (inc/"jwplc_spi_bus.h").write_text(task_h.SPI_H,encoding="utf-8")
    (folder/"C2_REAL_PATCHED_METHODS.inc").write_text(include_text,encoding="utf-8")
    exe=folder/("c2_slice.exe" if os.name=="nt" else "c2_slice")
    if kind=="msvc":
        argv=[host,"/nologo","/EHsc","/std:c++17","/W4",
              "/I"+str(inc),"/I"+str(folder),"/I"+str(CBASE),
              str(CSOURCE),str(TEST),"/Fe:"+str(exe)]
    elif kind=="msvc-vcvars":
        bat=folder/"compile_vcvars.bat"
        cl_args=["cl.exe","/nologo","/EHsc","/std:c++17","/W4",
                 "/I"+str(inc),"/I"+str(folder),"/I"+str(CBASE),
                 str(CSOURCE),str(TEST),"/Fe:"+str(exe)]
        import subprocess as sp
        bat.write_text('@echo off\ncall "'+host+'" >nul\n'
                       'if errorlevel 1 exit /b 91\n'
                       +sp.list2cmdline(cl_args)+'\n'
                       'exit /b %ERRORLEVEL%\n',encoding="utf-8")
        argv=["cmd.exe","/d","/c",str(bat)]
    else:
        argv=[host,"-std=c++17","-O0","-Wall","-Wextra",
              "-I"+str(inc),"-I"+str(folder),"-I"+str(CBASE),
              str(CSOURCE),str(TEST),"-o",str(exe)]
    env=os.environ.copy()
    if os.name=="nt" and kind=="gcc":
        env["PATH"]=str(Path(host).resolve().parent)+os.pathsep+env.get("PATH","")
    comp=subprocess.run(argv,cwd=folder,capture_output=True,text=True,
                        errors="replace",timeout=120,env=env)
    (folder/"compiler.log").write_text(
        "COMMAND="+repr(argv)+"\nEXIT="+str(comp.returncode)+"\n"+
        comp.stdout+"\n"+comp.stderr,encoding="utf-8")
    if comp.returncode!=0:
        for x in (comp.stderr+"\n"+comp.stdout).splitlines()[-25:]:
            if x.strip():say("DIAG_"+tag,x[:350])
    need(comp.returncode==0 and exe.is_file(),
         "COMPILE_FAILED_"+tag+"_SEE_LOG")
    run=subprocess.run([str(exe)],cwd=folder,capture_output=True,
                       text=True,errors="replace",timeout=15,env=env)
    log=run.stdout+"\n"+run.stderr
    (folder/"native.log").write_text(
        "EXIT="+str(run.returncode)+"\n"+log,encoding="utf-8")
    return run.returncode,log

def preflight(ci):
    need(Path.cwd().resolve()==ROOT and
         Path(git("rev-parse","--show-toplevel")).resolve()==ROOT,
         "REPOSITORY_ROOT_REQUIRED")
    head=git("rev-parse","HEAD")
    need(subprocess.run(["git","merge-base","--is-ancestor",PREV_HEAD,head],
         cwd=ROOT,capture_output=True).returncode==0,
         "C1_HEAD_NOT_ANCESTOR")
    need(not git("diff","--name-only") and not git("diff","--cached","--name-only"),
         "TRACKED_DIRTY")
    if not ci:
        need(git("branch","--show-current")==BRANCH,"WRONG_BRANCH")
        need(head==git("rev-parse","refs/remotes/origin/"+BRANCH),
             "LOCAL_REMOTE_MISMATCH")
        p=ROOT/"tools/alpha13/results"/PREV_RUN/"MANIFEST.json"
        need(p.is_file(),"LOCAL_C1_PASS_MANIFEST_MISSING")
        m=json.loads(p.read_text(encoding="utf-8-sig"))
        need(m.get("schema")=="A13-G10-P1G-C1-NATIVE-PROTOTYPE-1" and
             m.get("run_id")==PREV_RUN and m.get("head")==PREV_HEAD and
             m.get("status")==
             "ISOLATED_BUS_LEASE_PROTOTYPE_NATIVE_PASS_ADOPTION_BLOCKED",
             "LOCAL_C1_MANIFEST_INVALID")
    gate=load_module("tools/alpha13/gates/a13_g10_p1g_a_mutex_callpaths.py",
                     "p1g_c2_frozen_gate")
    need(gate.check_frozen()==5,"FROZEN_ARCHIVE_SHA_MISMATCH")
    gate.check(gate.sources())
    return head

def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--ci-standalone",action="store_true")
    args=ap.parse_args()
    runid="g10_p1g_c2_s1_"+datetime.now().strftime("%Y%m%d_%H%M%S")+"_"+uuid.uuid4().hex[:8]
    out=ROOT/"tools/alpha13/results"/runid
    out.mkdir(parents=True,exist_ok=False)
    m={"schema":"A13-G10-P1G-C2-S1-REAL-SOURCE-SLICE-1",
       "run_id":runid,"status":"REVIEW_STOP","product_mutation":False,
       "gpio4_enabled":False,"hardware_access":False,"firmware_compiled":False,
       "candidate_adoption":False}
    try:
        head=preflight(args.ci_standalone)
        repro=load_module("tools/alpha13/gates/a13_g10_p1a_a13_006_repro.py",
                          "p1g_c2_source_extract")
        c1=load_module("tools/alpha13/gates/a13_g10_p1g_c1_native_prototype.py",
                       "p1g_c2_compiler")
        orig={k:(ROOT/p).read_text(encoding="utf-8-sig")
              for k,p in TARGETS.items()}
        overlay=patch_functions(repro,orig)
        paths={}
        for key,path in TARGETS.items():
            p=out/"overlay"/path
            p.parent.mkdir(parents=True,exist_ok=True)
            p.write_text(overlay[key],encoding="utf-8")
            paths[path]={"original_sha256":sha(ROOT/path),
                         "overlay_sha256":sha(p)}
        methods=methods_include(repro,overlay)
        host,kind=c1.compiler()
        rc,log=native_build(out,"positive",methods,c1,host,kind)
        need(rc==0 and all(expected in log for expected in EXPECT),
             "POSITIVE_REAL_METHODS_FAILED")
        say("POSITIVE_NATIVE","PASS_REAL_EIGHT_METHODS")
        for label,(key,old,new,expected_error) in NEGATIVE_MUTATIONS.items():
            need(overlay[key].count(old)==1,
                 "NEGATIVE_ANCHOR_NOT_UNIQUE_"+label)
            alt=dict(overlay)
            alt[key]=overlay[key].replace(old,new,1)
            bad=methods_include(repro,alt)
            neg_rc,neglog=native_build(out,"negative_"+label,bad,c1,host,kind)
            if neg_rc==0 or expected_error not in neglog:
                say("NEGATIVE_DEBUG_"+label,
                    "EXIT_"+str(neg_rc)+"_"+neglog.strip().replace("\\n"," | ")[:400])
            need(neg_rc!=0 and expected_error in neglog,
                 "NEGATIVE_NOT_REJECTED_"+label)
            say("NEGATIVE_"+label,"PASS_REJECTED")
        m.update({"status":"REAL_SOURCE_MUTEX_SLICE_NATIVE_PASS_INCOMPLETE_ADOPTION_BLOCKED",
                  "head":head,"compiler":host,"methods_extracted":8,
                  "overlay_files":paths,"negatives":len(NEGATIVE_MUTATIONS),
                  "c1_core_sha256":sha(CSOURCE),
                  "test_sha256":sha(TEST),
                  "unprotected_remaining":"OTHER_TCP_UDP_SOCKET_DHCP_DNS",
                  "unproven":"OVERALL_MUTEX_COVERAGE_LEASE_RESET_FAIRNESS",
                  "known_semantic_gap":"RAW_CONNECTED_BUS_BUSY_RETURNS_FALSE",
                  "compile_arduino":"NOT_RUN","product_sources_modified":False})
        say("HEAD",head)
        say("METHODS_REAL_PATCHED",8)
        say("OVERLAY_FILES",len(paths))
        say("NEGATIVES",len(NEGATIVE_MUTATIONS))
        say("FROZEN_ARCHIVES",5)
        say("PRODUCT_CHANGED","NO")
        say("STATUS",m["status"])
        say("RESULTS",out)
        return 0
    except Exception as e:
        m["reason"]=repr(e)
        say("STATUS","REVIEW_STOP")
        say("REASON",repr(e))
        say("RESULTS",out)
        return 2
    finally:
        (out/"MANIFEST.json").write_text(
            json.dumps(m,ensure_ascii=False,indent=2)+"\n",encoding="utf-8")
if __name__=="__main__":
    try:sys.exit(main())
    except Exception as e:
        say("FATAL",repr(e))
        sys.exit(2)
