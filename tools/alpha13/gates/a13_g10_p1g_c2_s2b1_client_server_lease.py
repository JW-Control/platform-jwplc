#!/usr/bin/env python3
"""Alpha13 G10-P1G-C2-S2B1: Client ctor + Server available/accept + cancelStopAsync.

Overlay SOURCE-BACKED. No afirma protección de todos los flujos de Client,
Server o UDP. No modifica código productivo ni reconfigura GPIO4.
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
PREV_HEAD="d40b7ae25e6dc5e4b54034766abaa13b11bd7dba"
PREV_RUN="g10_p1g_c2_s2a_20261013_113859_460838be"
PFX="JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/"
FILES={
    "HEADER":PFX+"JWPLC_W5x00_Ethernet.h",
    "CLIENT":PFX+"EthernetClient.cpp",
    "SERVER":PFX+"EthernetServer.cpp",
}
SRC_BASE=ROOT/"tools/alpha13/candidates/g10_p1g_c"
C1_CPP=SRC_BASE/"JWPLC_W5500_SafeAccess.cpp"
TEST=SRC_BASE/"native_c2_s2b_client_server_lease_test.cpp"
CTOR="EthernetClient(uint8_t s) : _sockindex(s), _timeout(1000)"
METHODS={
    "CLIENT":["void EthernetClient::cancelStopAsync()"],
    "SERVER":["void EthernetServer::begin()",
              "EthernetClient EthernetServer::available()",
              "EthernetClient EthernetServer::accept()"],
}
EXPECTED=[
    "C2_S2B1_SERVER_AVAILABLE_COPY_STALE=PASS",
    "C2_S2B1_ACCEPT_COPY_AND_SPI_BUSY=PASS",
    "C2_S2B1_RESET_EPOCH_REJECTED_CLOSE=PASS",
    "C2_S2B1_NATIVE_RESULT=PASS_LIMITED_CLIENT_SERVER_CLOSE_ADOPTION_BLOCKED",
]
MUTATIONS=[
    ("CTOR_NO_TOKEN","HEADER",
     "_jwplcSocketToken = jwplc_w5500_candidate::currentSocketTokenLocked(s);",
     "/* no ctor token */","AVAILABLE_CLIENT_NOT_RETURNED"),
    ("CANCEL_NO_LEASE_CHECK","CLIENT",
     "!jwplc_w5500_candidate::matchesLocked(_jwplcSocketToken)",
     "false","STALE_COPY_CLOSED_RECYCLED_SOCKET"),
    ("SERVER_AVAILABLE_UNLOCKED","SERVER",
     "jwplc_w5500_candidate::BusScope bus(20);\n\tif (!bus) return EthernetClient(MAX_SOCK_NUM);",
     "/* removed available scope */","STATUS_UNLOCKED"),
    ("BUS_BUSY_DROPS_HANDLE","CLIENT",
     "if (!bus) return; // Contención SPI: conservar handle y reintentar.",
     "if (!bus) { _sockindex = MAX_SOCK_NUM; return; }",
     "BUS_BUSY_DISCARDED_HANDLE_OR_CLOSED"),
]
def say(k,v):print("G10_P1G_C2_S2B1_"+str(k)+"="+str(v),flush=True)
def need(ok,msg):
    if not ok:raise RuntimeError(msg)
def sha(path):
    h=hashlib.sha256()
    with Path(path).open("rb") as file:
        for chunk in iter(lambda:file.read(1024*1024),b""):
            h.update(chunk)
    return h.hexdigest()
def module(path,name):
    spec=importlib.util.spec_from_file_location(name,ROOT/path)
    need(spec is not None and spec.loader is not None,"MODULE_NOT_FOUND_"+name)
    o=importlib.util.module_from_spec(spec)
    sys.modules[name]=o
    spec.loader.exec_module(o)
    return o
def git(*args):
    p=subprocess.run(["git","-c","core.quotepath=false",*args],
                     cwd=ROOT,capture_output=True,text=True,errors="replace",timeout=25)
    need(p.returncode==0,"GIT_FAILED_"+repr(args))
    return p.stdout.strip()
def preflight(ci):
    head=git("rev-parse","HEAD")
    need(Path(git("rev-parse","--show-toplevel")).resolve()==ROOT
         and Path.cwd().resolve()==ROOT,"WRONG_REPOSITORY_ROOT")
    need(subprocess.run(["git","merge-base","--is-ancestor",PREV_HEAD,head],
         cwd=ROOT,capture_output=True).returncode==0,"C2_S2A_NOT_ANCESTOR")
    need(not git("diff","--name-only") and not git("diff","--cached","--name-only"),
         "TRACKED_FILES_DIRTY")
    if not ci:
        need(git("branch","--show-current")==BRANCH,"WRONG_BRANCH")
        need(head==git("rev-parse","refs/remotes/origin/"+BRANCH),
             "HEAD_REMOTE_MISMATCH")
        p=ROOT/"tools/alpha13/results"/PREV_RUN/"MANIFEST.json"
        need(p.is_file(),"C2_S2A_LOCAL_MANIFEST_MISSING")
        m=json.loads(p.read_text(encoding="utf-8-sig"))
        need(m.get("schema")=="A13-G10-P1G-C2-S2A-SOCKET-BEGIN-LEASE-1" and
             m.get("run_id")==PREV_RUN and m.get("head")==PREV_HEAD and
             m.get("status")=="REAL_SOCKET_BEGIN_LEASE_NATIVE_PASS_INCOMPLETE_ADOPTION_BLOCKED",
             "C2_S2A_LOCAL_MANIFEST_NOT_QUALIFIED")
    check=module("tools/alpha13/gates/a13_g10_p1g_a_mutex_callpaths.py",
                 "p1g_c2_s2b1_frozen")
    need(check.check_frozen()==5,"FROZEN_PRECOMPILED_SHA_MISMATCH")
    check.check(check.sources())
    return head

def patch(repro,original):
    out=dict(original)
    header=out["HEADER"]
    need(header.count(CTOR)==1,"CLIENT_CTOR_NOT_UNIQUE")
    body=repro.extract(header,CTOR)
    need("_startMillis = 0;" in body and "_jwplcSocketToken" not in body,
         "CLIENT_CTOR_CHANGED")
    modified=body.replace("_startMillis = 0;",
        """_startMillis = 0;
        if (s < MAX_SOCK_NUM) {
            jwplc_w5500_candidate::BusScope bus(20);
            if (bus)
                _jwplcSocketToken = jwplc_w5500_candidate::currentSocketTokenLocked(s);
            if (!bool(_jwplcSocketToken)) _sockindex = MAX_SOCK_NUM;
        }""",1)
    need(header.count(body)==1,"CONSTRUCTOR_BODY_NOT_UNIQUE")
    header=header.replace(body,modified,1)
    field="uint8_t _sockindex; // MAX_SOCK_NUM means client not in use"
    need(header.count(field)==1,"CLIENT_SOCK_INDEX_ANCHOR_DRIFT")
    header=header.replace(field,field+
        "\n\tjwplc_w5500_candidate::SocketToken _jwplcSocketToken{};",1)
    header='#include "JWPLC_W5500_SafeAccess.h"\n'+header
    out["HEADER"]=header

    client=out["CLIENT"]
    sig="void EthernetClient::cancelStopAsync()"
    old=repro.extract(client,sig)
    need(old.count("Ethernet.socketClose(_sockindex);")==1
         and old.count("cancelFlushAsync();")==1,"CLIENT_CANCEL_SOURCE_DRIFT")
    new=old.replace(sig+"\n{",sig+"""
{
    jwplc_w5500_candidate::BusScope bus(20);
    if (!bus) return; // Contención SPI: conservar handle y reintentar.
    if (_sockindex < MAX_SOCK_NUM &&
        !jwplc_w5500_candidate::matchesLocked(_jwplcSocketToken)) {
        _sockindex = MAX_SOCK_NUM; // stale handle: no tocar hardware ajeno
        _stopPending = false;
        _stopStartedAtMs = 0;
        return;
    }""",1)
    need(new!=old,"CANCEL_SCOPE_NOT_INJECTED")
    new=new.replace("Ethernet.socketClose(_sockindex);",
                    "Ethernet.socketClose(_sockindex);\n"
                    "        (void)jwplc_w5500_candidate::invalidateSocketLocked(_jwplcSocketToken);",1)
    client='#include "JWPLC_W5500_SafeAccess.h"\n'+client.replace(old,new,1)
    out["CLIENT"]=client

    server=out["SERVER"]
    for sig in METHODS["SERVER"]:
        old=repro.extract(server,sig)
        prefix=sig+"\n{"
        need(old.startswith(prefix),"SERVER_SIGNATURE_CHANGED_"+sig)
        if "::begin()" in sig:
            repl=prefix+"""
    jwplc_w5500_candidate::BusScope bus(20);
    if (!bus) return;"""
        else:
            repl=prefix+"""
    jwplc_w5500_candidate::BusScope bus(20);
    if (!bus) return EthernetClient(MAX_SOCK_NUM);"""
        new=old.replace(prefix,repl,1)
        if "::begin()" in sig:
            needle="server_port[sockindex] = _port;"
            need(new.count(needle)==1,"SERVER_LISTENER_ANCHOR_CHANGED")
            new=new.replace(needle,needle+"""
            jwplc_w5500_candidate::mapListenerLocked(sockindex,_port,
                jwplc_w5500_candidate::currentSocketTokenLocked(sockindex));""",1)
        else:
            needle="server_port[i] == _port"
            need(new.count(needle)==1,"SERVER_FILTER_ANCHOR_DRIFT")
            new=new.replace(needle,needle+
                " && jwplc_w5500_candidate::listenerMatchesLocked(i,_port,"
                "jwplc_w5500_candidate::currentSocketTokenLocked(i))",1)
        server=server.replace(old,new,1)
    out["SERVER"]='#include "JWPLC_W5500_SafeAccess.h"\n'+server
    return out

def witness(repro,overlay):
    header=repro.extract(overlay["HEADER"],CTOR)
    methods=[repro.extract(overlay[k],sig)
             for k in ("CLIENT","SERVER") for sig in METHODS[k]]
    need(len(methods)==4,"EXPECTED_FOUR_REAL_METHODS")
    return ("// Original class method bodies from patched SOURCE overlay.\n"+
            "\n\n".join(methods)+"\n",header)

def compile_native(base,label,witnesses,c1):
    out=base/label
    out.mkdir(parents=True,exist_ok=False)
    inc=out/"include"
    (inc/"freertos").mkdir(parents=True)
    (inc/"freertos"/"task.h").write_text(c1.TASK_H,encoding="utf-8")
    (inc/"jwplc_spi_bus.h").write_text(c1.SPI_H,encoding="utf-8")
    real,ctor=witnesses
    (out/"C2_S2B1_REAL_PATCHED_METHODS.inc").write_text(real,encoding="utf-8")
    template=TEST.read_text(encoding="utf-8-sig")
    need(template.count("__REAL_CLIENT_CTOR__")==1,"CTOR_TEMPLATE_DRIFT")
    test=out/"native_c2_s2b_test.cpp"
    test.write_text(template.replace("__REAL_CLIENT_CTOR__",ctor),encoding="utf-8")
    host,kind=c1.compiler()
    exe=out/("native_c2_s2b.exe" if os.name=="nt" else "native_c2_s2b")
    if kind=="msvc":
        cmd=[host,"/nologo","/EHsc","/std:c++17","/W4",
             "/I"+str(inc),"/I"+str(out),"/I"+str(SRC_BASE),
             str(C1_CPP),str(test),"/Fe:"+str(exe)]
    elif kind=="msvc-vcvars":
        clcmd=["cl.exe","/nologo","/EHsc","/std:c++17","/W4",
               "/I"+str(inc),"/I"+str(out),"/I"+str(SRC_BASE),
               str(C1_CPP),str(test),"/Fe:"+str(exe)]
        bat=out/"compile_msvc.bat"
        bat.write_text('@echo off\ncall "'+host+'" >nul\n'
                       'if errorlevel 1 exit /b 91\n'
                       +subprocess.list2cmdline(clcmd)+'\n'
                       'exit /b %ERRORLEVEL%\n',encoding="utf-8")
        cmd=["cmd.exe","/d","/c",str(bat)]
    else:
        cmd=[host,"-std=c++17","-O0","-Wall","-Wextra",
             "-I"+str(inc),"-I"+str(out),"-I"+str(SRC_BASE),
             str(C1_CPP),str(test),"-o",str(exe)]
    env=os.environ.copy()
    if os.name=="nt" and kind=="gcc":
        env["PATH"]=str(Path(host).resolve().parent)+os.pathsep+env.get("PATH","")
    comp=subprocess.run(cmd,cwd=out,capture_output=True,text=True,
                        errors="replace",timeout=120,env=env)
    (out/"compile.log").write_text(
        "COMMAND="+repr(cmd)+"\nEXIT="+str(comp.returncode)+"\n"
        +comp.stdout+"\n"+comp.stderr,encoding="utf-8")
    if comp.returncode!=0:
        for line in (comp.stderr+"\n"+comp.stdout).splitlines()[-22:]:
            if line.strip():say("COMPILER_DIAG_"+label,line[:380])
    need(comp.returncode==0 and exe.is_file(),"HOST_COMPILE_FAILED_"+label)
    run=subprocess.run([str(exe)],cwd=out,capture_output=True,text=True,
                       errors="replace",timeout=15,env=env)
    log=run.stdout+"\n"+run.stderr
    (out/"native.log").write_text(
        "EXIT="+str(run.returncode)+"\n"+log,encoding="utf-8")
    return run.returncode,log

def main():
    arg=argparse.ArgumentParser(description=__doc__)
    arg.add_argument("--ci-standalone",action="store_true")
    options=arg.parse_args()
    run_id="g10_p1g_c2_s2b1_"+datetime.now().strftime("%Y%m%d_%H%M%S")+"_"+uuid.uuid4().hex[:8]
    out=ROOT/"tools/alpha13/results"/run_id
    out.mkdir(parents=True,exist_ok=False)
    manifest={"schema":"A13-G10-P1G-C2-S2B1-CLIENT-SERVER-LEASE-1",
              "run_id":run_id,"status":"REVIEW_STOP",
              "product_changed":False,"arduino_compiled":False,
              "hardware_test":False,"gpio4_activated":False,
              "candidate_adopted":False}
    try:
        head=preflight(options.ci_standalone)
        repro=module("tools/alpha13/gates/a13_g10_p1a_a13_006_repro.py",
                     "p1g_c2_s2b1_extract")
        c1=module("tools/alpha13/gates/a13_g10_p1g_c1_native_prototype.py",
                  "p1g_c2_s2b1_c1")
        orig={key:(ROOT/path).read_text(encoding="utf-8-sig")
              for key,path in FILES.items()}
        over=patch(repro,orig)
        overlay_shas={}
        for key,path in FILES.items():
            p=out/"overlay"/path
            p.parent.mkdir(parents=True,exist_ok=True)
            p.write_text(over[key],encoding="utf-8")
            overlay_shas[path]=sha(p)
        rc,log=compile_native(out,"positive",witness(repro,over),c1)
        need(rc==0 and all(x in log for x in EXPECTED),
             "POSITIVE_CLIENT_SERVER_NATIVE_FAILED")
        say("POSITIVE_NATIVE","PASS_REAL_CLIENT_SERVER_TOKEN_AND_CLOSE")
        for label,key,old,new,expected_failure in MUTATIONS:
            need(over[key].count(old)==1,"NEGATIVE_ANCHOR_NOT_UNIQUE_"+label)
            copy=dict(over)
            copy[key]=over[key].replace(old,new,1)
            rc,log=compile_native(out,"negative_"+label,witness(repro,copy),c1)
            if not(rc!=0 and expected_failure in log):
                say("NEGATIVE_DEBUG_"+label,
                    "EXIT_"+str(rc)+"_"+log.strip().replace("\n"," | ")[:350])
            need(rc!=0 and expected_failure in log,
                 "NEGATIVE_NOT_REJECTED_"+label)
            say("NEGATIVE_"+label,"PASS_REJECTED")
        manifest.update({
            "head":head,
            "status":"CLIENT_SERVER_TOKEN_CANCEL_SOURCE_NATIVE_PASS_INCOMPLETE_ADOPTION_BLOCKED",
            "real_method_definitions":4,"real_header_constructor":1,
            "overlay_files":overlay_shas,"negative_mutations":len(MUTATIONS),
            "c1_core_sha256":sha(C1_CPP),"test_sha256":sha(TEST),
            "known_gaps":[
                "Client outbound beginConnectAsync/pollConnectAsync no lease",
                "Client beginStopAsync/pollStopAsync no lease or BUS_BUSY handling",
                "Client read/write/connected/compare still index-only outside this slice",
                "Server listener binding generation not fully integrated with reset",
                "UDP/DHCP and physical reset GPIO4 not integrated",
                "Arduino full package build and shared SPI fairness not run"],
        })
        say("HEAD",head)
        say("REAL_METHODS",4)
        say("HEADER_CONSTRUCTOR",1)
        say("OVERLAY_FILES",3)
        say("NEGATIVES",len(MUTATIONS))
        say("FROZEN_ARCHIVES",5)
        say("PRODUCT_CHANGED","NO")
        say("STATUS",manifest["status"])
        say("RESULTS",out)
        return 0
    except Exception as e:
        manifest["reason"]=repr(e)
        say("STATUS","REVIEW_STOP")
        say("REASON",repr(e))
        say("RESULTS",out)
        return 2
    finally:
        (out/"MANIFEST.json").write_text(
            json.dumps(manifest,ensure_ascii=False,indent=2)+"\n",
            encoding="utf-8")
if __name__=="__main__":
    try:sys.exit(main())
    except Exception as e:
        say("FATAL",repr(e))
        sys.exit(2)
