#!/usr/bin/env python3
"""G10-P1G-C2-S2B2A: overlay C++ REAL de begin/poll/stop cooperativo.

Se ejecuta con stubs host, no es el build Arduino integrado. Una prueba PASS
aprueba SOLO el slice nativo, nunca el firmware, API TCP completa o GPIO4.
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
PRIOR="g10_p1g_c2_s2b1_20261013_120240_ccb0080a"
PRIOR_HEAD="9d59ff4d1471cc563d59c078903367d60d59ae40"
SRC="JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/EthernetClient.cpp"
BASE=ROOT/"tools/alpha13/candidates/g10_p1g_c"
TEST=BASE/"native_c2_s2b2a_cooperative_stop_test.cpp"
C1=BASE/"JWPLC_W5500_SafeAccess.cpp"
SIGS=[
    "int EthernetClient::beginStopAsync()",
    "int EthernetClient::pollStopAsync()",
    "void EthernetClient::stop()",
]
EXPECTED=[
    "C2_S2B2A_GRACEFUL_STOP=PASS",
    "C2_S2B2A_BUSY_PRESERVES_HANDLE_AND_RETRIES=PASS",
    "C2_S2B2A_STALE_OWNER_NO_CLOSE=PASS",
    "C2_S2B2A_LEGACY_STOP_BOUNDED_RETRYABLE=PASS",
    "C2_S2B2A_NATIVE_RESULT=PASS_STOP_SLICE_NO_PRODUCT_ADOPTION",
]
NEGATIVES=[
    ("NO_BEGIN_GUARD",
     "jwplc_w5500_candidate::BusScope bus(20);\n\tif (!bus) return 0;",
     "/* guard removed */", "STATUS_WITHOUT_GUARD"),
    ("NO_OWNER_VALIDATION",
     "if (_sockindex < MAX_SOCK_NUM &&\\n\\t    !jwplc_w5500_candidate::matchesLocked(_jwplcSocketToken))",
     "if (_sockindex < MAX_SOCK_NUM &&\\n\\t    false)",
     "STALE_BEGIN_DID_NOT_DETACH"),
    ("BUSY_DROPS_HANDLE",
     "int EthernetClient::beginStopAsync()\\n{\\n\\tjwplc_w5500_candidate::BusScope bus(20);\\n\\tif (!bus) return 0;",
     "int EthernetClient::beginStopAsync()\\n{\\n\\tjwplc_w5500_candidate::BusScope bus(20);\\n\\tif (!bus) { _sockindex=MAX_SOCK_NUM; return 0; }",
     "BUS_BUSY_START_MUTATED_HANDLE_OR_HARDWARE"),
    ("UNBOUNDED_STOP_LIFECYCLE",
     "if ((uint32_t)(millis() - waitStartedMs) >= _timeout) return;",
     "if ((uint32_t)(millis() - waitStartedMs) >= _timeout) { _sockindex=MAX_SOCK_NUM; return; }",
     "LEGACY_STOP_BUSY_DISCARDED_HANDLE"),
]
def say(k,v):print("G10_P1G_C2_S2B2A_"+str(k)+"="+str(v),flush=True)
def need(ok,msg):
    if not ok:raise RuntimeError(msg)
def sha(path):
    h=hashlib.sha256()
    with Path(path).open("rb") as f:
        for chunk in iter(lambda:f.read(1024*1024),b""):h.update(chunk)
    return h.hexdigest()
def load(path,name):
    spec=importlib.util.spec_from_file_location(name,ROOT/path)
    need(spec is not None and spec.loader is not None,
         "MODULE_NOT_AVAILABLE_"+name)
    m=importlib.util.module_from_spec(spec)
    sys.modules[name]=m
    spec.loader.exec_module(m)
    return m
def git(*args):
    p=subprocess.run(["git","-c","core.quotepath=false",*args],
                     cwd=ROOT,text=True,capture_output=True,
                     errors="replace",timeout=25)
    need(p.returncode==0,"GIT_FAILURE_"+repr(args))
    return p.stdout.strip()
def preflight(ci):
    need(Path(git("rev-parse","--show-toplevel")).resolve()==ROOT
         and Path.cwd().resolve()==ROOT, "NOT_ROOT")
    h=git("rev-parse","HEAD")
    need(subprocess.run(["git","merge-base","--is-ancestor",PRIOR_HEAD,h],
         cwd=ROOT,capture_output=True).returncode==0,"S2B1_HEAD_NOT_ANCESTOR")
    need(not git("diff","--name-only") and
         not git("diff","--cached","--name-only"),"GIT_TRACKED_DIRTY")
    if not ci:
        need(git("branch","--show-current")==BRANCH,"WRONG_BRANCH")
        need(h==git("rev-parse","refs/remotes/origin/"+BRANCH),
             "REMOTE_HEAD_DIFFERS")
        p=ROOT/"tools/alpha13/results"/PRIOR/"MANIFEST.json"
        need(p.is_file(),"LOCAL_S2B1_MANIFEST_NOT_FOUND")
        m=json.loads(p.read_text(encoding="utf-8-sig"))
        need(m.get("schema")=="A13-G10-P1G-C2-S2B1-CLIENT-SERVER-LEASE-1" and
             m.get("head")==PRIOR_HEAD and
             m.get("run_id")==PRIOR and
             m.get("status")=="CLIENT_SERVER_TOKEN_CANCEL_SOURCE_NATIVE_PASS_INCOMPLETE_ADOPTION_BLOCKED",
             "LOCAL_S2B1_NOT_QUALIFIED")
    frozen=load("tools/alpha13/gates/a13_g10_p1g_a_mutex_callpaths.py",
                "s2b2a_frozen")
    need(frozen.check_frozen()==5,"FROZEN_ARCHIVES_CHANGED")
    frozen.check(frozen.sources())
    return h

def patched_methods(repro,old):
    origin={s:repro.extract(old,s) for s in SIGS}
    a=origin[SIGS[0]]
    need(a.count("cancelFlushAsync();")==1 and
         a.count("cancelWriteAsync();")==1 and
         a.count("Ethernet.socketDisconnect(_sockindex);")==1,
         "BEGIN_STOP_SOURCE_DRIFT")
    # Protección del owner ANTES de cancelFlush/Write (pueden tocar W5500).
    a=a.replace("int EthernetClient::beginStopAsync()\n{",
        """int EthernetClient::beginStopAsync()
{
\tjwplc_w5500_candidate::BusScope bus(20);
\tif (!bus) return 0;
\tif (_sockindex < MAX_SOCK_NUM &&
\t    !jwplc_w5500_candidate::matchesLocked(_jwplcSocketToken)) {
\t\t_sockindex = MAX_SOCK_NUM;
\t\t_stopPending = false;
\t\t_stopStartedAtMs = 0;
\t\treturn 1; // antiguo propietario: no tocar W5500
\t}""",1)
    a=a.replace("""if (stat == SnSR::CLOSED) {
\t\t_sockindex = MAX_SOCK_NUM;""",
        """if (stat == SnSR::CLOSED) {
\t\t(void)jwplc_w5500_candidate::invalidateSocketLocked(_jwplcSocketToken);
\t\t_sockindex = MAX_SOCK_NUM;""",1)

    b=origin[SIGS[1]]
    need("if (!_stopPending)" in b and
         "Ethernet.socketClose(_sockindex);" in b,
         "POLL_STOP_SOURCE_DRIFT")
    b=b.replace("""if (!_stopPending) {
\t\treturn (_sockindex >= MAX_SOCK_NUM) ? 1 : -1;
\t}""", """if (!_stopPending) {
\t\t// Un begin previo pudo quedar pendiente exclusivamente por BUS_BUSY.
\t\treturn (_sockindex < MAX_SOCK_NUM) ? beginStopAsync() : 1;
\t}""",1)
    b=b.replace("""if (_sockindex >= MAX_SOCK_NUM) {
\t\t_stopPending = false;
\t\t_stopStartedAtMs = 0;
\t\treturn 1;
\t}""", """if (_sockindex >= MAX_SOCK_NUM) {
\t\t_stopPending = false;
\t\t_stopStartedAtMs = 0;
\t\treturn 1;
\t}
\tjwplc_w5500_candidate::BusScope bus(20);
\tif (!bus) return 0;
\tif (!jwplc_w5500_candidate::matchesLocked(_jwplcSocketToken)) {
\t\t_sockindex = MAX_SOCK_NUM;
\t\t_stopPending = false;
\t\t_stopStartedAtMs = 0;
\t\treturn 1;
\t}""",1)
    b=b.replace("""if (Ethernet.socketStatus(_sockindex) == SnSR::CLOSED) {
\t\t_sockindex = MAX_SOCK_NUM;""",
        """if (Ethernet.socketStatus(_sockindex) == SnSR::CLOSED) {
\t\t(void)jwplc_w5500_candidate::invalidateSocketLocked(_jwplcSocketToken);
\t\t_sockindex = MAX_SOCK_NUM;""",1)
    b=b.replace("""Ethernet.socketClose(_sockindex);
\t\t_sockindex = MAX_SOCK_NUM;""",
        """Ethernet.socketClose(_sockindex);
\t\t(void)jwplc_w5500_candidate::invalidateSocketLocked(_jwplcSocketToken);
\t\t_sockindex = MAX_SOCK_NUM;""",1)

    c=origin[SIGS[2]]
    need("while (state == 0)" in c and "delay(1);" in c,
         "STOP_SOURCE_DRIFT")
    c=c.replace("""int state = beginStopAsync();
\twhile (state == 0) {
\t\tdelay(1);
\t\tstate = pollStopAsync();
\t}""", """const uint32_t waitStartedMs = millis();
\tint state = beginStopAsync();
\twhile (state == 0) {
\t\t// Void legacy API: la contención no permite cerrar W5500.
\t\t// Bounded return: conservar handle y dejar reintento explícito.
\t\tif ((uint32_t)(millis() - waitStartedMs) >= _timeout) return;
\t\tdelay(1);
\t\tstate = pollStopAsync();
\t}""",1)
    modified=[a,b,c]
    for sig,oldbody,newbody in zip(SIGS,origin.values(),modified):
        need(oldbody!=newbody and old.count(oldbody)==1,
             "BODY_NOT_PATCHED_"+sig)
    result=old
    for oldbody,newbody in zip(origin.values(),modified):
        result=result.replace(oldbody,newbody,1)
    return result,modified

def native(out,name,text,c1):
    folder=out/name
    folder.mkdir(parents=True,exist_ok=False)
    inc=folder/"include"
    (inc/"freertos").mkdir(parents=True)
    (inc/"freertos"/"task.h").write_text(c1.TASK_H,encoding="utf-8")
    (inc/"jwplc_spi_bus.h").write_text(c1.SPI_H,encoding="utf-8")
    (folder/"C2_S2B2A_REAL_PATCHED_METHODS.inc").write_text(
        "// Methods from source-derived overlay, no modeled copies\n"
        +text,encoding="utf-8")
    host,kind=c1.compiler()
    exe=folder/("c2_s2b2a.exe" if os.name=="nt" else "c2_s2b2a")
    if kind=="msvc":
        cmd=[host,"/nologo","/EHsc","/std:c++17","/W4",
             "/I"+str(inc),"/I"+str(folder),"/I"+str(BASE),
             str(C1),str(TEST),"/Fe:"+str(exe)]
    elif kind=="msvc-vcvars":
        args=["cl.exe","/nologo","/EHsc","/std:c++17","/W4",
              "/I"+str(inc),"/I"+str(folder),"/I"+str(BASE),
              str(C1),str(TEST),"/Fe:"+str(exe)]
        bat=folder/"compile_msvc.bat"
        bat.write_text('@echo off\ncall "'+host+'" >nul\n'
                       'if errorlevel 1 exit /b 91\n'
                       +subprocess.list2cmdline(args)+'\n'
                       'exit /b %ERRORLEVEL%\n',encoding="utf-8")
        cmd=["cmd.exe","/d","/c",str(bat)]
    else:
        cmd=[host,"-std=c++17","-O0","-Wall","-Wextra",
             "-I"+str(inc),"-I"+str(folder),"-I"+str(BASE),
             str(C1),str(TEST),"-o",str(exe)]
    env=os.environ.copy()
    if os.name=="nt" and kind=="gcc":
        env["PATH"]=str(Path(host).resolve().parent)+os.pathsep+env.get("PATH","")
    comp=subprocess.run(cmd,cwd=folder,capture_output=True,text=True,
                        errors="replace",timeout=120,env=env)
    (folder/"compile.log").write_text(
        "CMD="+repr(cmd)+"\nEXIT="+str(comp.returncode)+"\n"+
        comp.stdout+"\n"+comp.stderr,encoding="utf-8")
    if comp.returncode!=0:
        for line in (comp.stderr+"\n"+comp.stdout).splitlines()[-25:]:
            if line.strip():say("COMPILE_DIAGNOSTIC_"+name,line[:350])
    need(comp.returncode==0 and exe.is_file(),"NATIVE_COMPILE_FAILED_"+name)
    run=subprocess.run([str(exe)],cwd=folder,capture_output=True,text=True,
                       errors="replace",timeout=15,env=env)
    log=run.stdout+"\n"+run.stderr
    (folder/"native.log").write_text(
        "EXIT="+str(run.returncode)+"\n"+log,encoding="utf-8")
    return run.returncode,log

def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--ci-standalone",action="store_true")
    args=ap.parse_args()
    runid="g10_p1g_c2_s2b2a_"+datetime.now().strftime("%Y%m%d_%H%M%S")+"_"+uuid.uuid4().hex[:8]
    out=ROOT/"tools/alpha13/results"/runid
    out.mkdir(parents=True,exist_ok=False)
    manifest={"schema":"A13-G10-P1G-C2-S2B2A-STOP-1",
              "run_id":runid,"status":"REVIEW_STOP",
              "firmware_changed":False,"arduino_built":False,
              "hardware_touched":False,"adopted":False}
    try:
        head=preflight(args.ci_standalone)
        repro=load("tools/alpha13/gates/a13_g10_p1a_a13_006_repro.py",
                   "s2b2a_repro")
        s2b1=load("tools/alpha13/gates/a13_g10_p1g_c2_s2b1_client_server_lease.py",
                   "s2b2a_prior_overlay")
        c1=load("tools/alpha13/gates/a13_g10_p1g_c1_native_prototype.py",
                "s2b2a_c1")
        orig={key:(ROOT/p).read_text(encoding="utf-8-sig")
              for key,p in s2b1.FILES.items()}
        prior=s2b1.patch(repro,orig)
        overlay,methods=patched_methods(repro,prior["CLIENT"])
        outputs=dict(prior)
        outputs["CLIENT"]=overlay
        shas={}
        for key,p in s2b1.FILES.items():
            dst=out/"overlay"/p
            dst.parent.mkdir(parents=True,exist_ok=True)
            dst.write_text(outputs[key],encoding="utf-8")
            shas[p]=sha(dst)
        joined="\n\n".join(methods)+"\n"
        rc,log=native(out,"positive",joined,c1)
        need(rc==0 and all(x in log for x in EXPECTED),
             "POSITIVE_STOP_REAL_CPP_FAILED")
        say("POSITIVE_NATIVE","PASS_REAL_COOPERATIVE_STOP_METHODS")
        for label,old,new,error in NEGATIVES:
            need(joined.count(old)==1,"NEGATIVE_ANCHOR_NOT_UNIQUE_"+label)
            mutated=joined.replace(old,new,1)
            status,log=native(out,"negative_"+label,mutated,c1)
            if status==0 or error not in log:
                say("NEGATIVE_DIAG_"+label,
                    str(status)+"_"+log.strip().replace("\n"," | ")[:380])
            need(status!=0 and error in log,"NEGATIVE_NOT_REJECTED_"+label)
            say("NEGATIVE_"+label,"PASS_REJECTED")
        manifest.update({
            "head":head,
            "status":"COOPERATIVE_STOP_REAL_METHODS_PASS_INCOMPLETE_ADOPTION_BLOCKED",
            "real_method_count":3,"overlay_paths":shas,
            "negative_count":len(NEGATIVES),
            "c1_core_sha256":sha(C1),"test_sha256":sha(TEST),
            "known_limitations":[
                "Outbound connect begin/poll/cancel not protected",
                "Data read/write, UDP, DNS, DHCP not protected",
                "Stop() void returns with socket live after sustained SPI_BUSY; retry obligation unresolved",
                "Client side object state and C1 token not integrated in Arduino full build",
                "GPIO4 physical reset / fairness not verified",
                "Only source-method unit compiled with host mocks; not full package"],
        })
        say("HEAD",head)
        say("REAL_CPP_METHODS",3)
        say("OVERLAY_FILES",len(shas))
        say("NEGATIVES",len(NEGATIVES))
        say("FROZEN_ARCHIVES",5)
        say("PRODUCT_CHANGED","NO")
        say("STATUS",manifest["status"])
        say("RESULTS",out)
        return 0
    except Exception as exc:
        manifest["reason"]=repr(exc)
        say("STATUS","REVIEW_STOP")
        say("REASON",repr(exc))
        say("RESULTS",out)
        return 2
    finally:
        (out/"MANIFEST.json").write_text(
            json.dumps(manifest,ensure_ascii=False,indent=2)+"\n",
            encoding="utf-8")
if __name__=="__main__":
    try:sys.exit(main())
    except Exception as exc:
        say("FATAL",repr(exc))
        sys.exit(2)
