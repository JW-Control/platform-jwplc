#!/usr/bin/env python3
"""Alpha13 G10-P1G-C2-S2B2B: outbound connect lifecycle, REAL C++ overlay.

Native with W5500 mocks, no integrated Arduino build or source adoption.
BUS_BUSY is not a connection failure. Explicit deferred-start state is
experimental and must be verified under legacy connect()/DNS before adoption.
"""
from __future__ import annotations
import argparse
from datetime import datetime
import hashlib
import importlib.util
import json
from pathlib import Path
import subprocess
import sys
import uuid

ROOT=Path(__file__).resolve().parents[3]
BRANCH="v2.1.0-alpha.13/feature/cleanup-robustness"
PRIOR_HEAD="e66021828971d82fa5b78e3319aaa44e977ef150"
PRIOR_RUN="g10_p1g_c2_s2b2a_20261013_121841_efdbfb69"
SRC_BASE=ROOT/"tools/alpha13/candidates/g10_p1g_c"
TEST=SRC_BASE/"native_c2_s2b2b_outbound_connect_test.cpp"
C1_CPP=SRC_BASE/"JWPLC_W5500_SafeAccess.cpp"
SIGS=[
  "int EthernetClient::beginConnectAsync(IPAddress ip, uint16_t port)",
  "int EthernetClient::pollConnectAsync()",
  "bool EthernetClient::connectAsyncInProgress()",
  "void EthernetClient::cancelConnectAsync()",
]
EXPECTED=[
  "C2_S2B2B_OUTBOUND_CONNECT_POLL=PASS",
  "C2_S2B2B_SPI_BUSY_START_AND_POLL=PASS",
  "C2_S2B2B_RETARGET_AND_STALE_ALIAS=PASS",
  "C2_S2B2B_CANCEL_BUSY_RETRY_OWNER=PASS",
  "C2_S2B2B_RESET_AND_INVALID_IP=PASS",
  "C2_S2B2B_NATIVE_RESULT=PASS_OUTBOUND_SLICE_ADOPTION_BLOCKED",
]
NEGATIVES=[
  ("REMOVE_BEGIN_GUARD",SIGS[0],
   "jwplc_w5500_candidate::BusScope bus(20);",
   "/* no begin guard */","CANCEL_WRITE_OUTSIDE_GUARD"),
  ("REMOVE_NEW_TOKEN",SIGS[0],
   "_jwplcSocketToken = jwplc_w5500_candidate::currentSocketTokenLocked(_sockindex);",
   "/* no token captured */","BEGIN_CONNECT_MUST_BE_PENDING"),
  ("NO_DEFER_ON_BUSY",SIGS[0],
   "_jwplcDeferredConnect = true;",
   "_jwplcDeferredConnect = false;","BUS_BUSY_DEFERRED_START_LOST"),
  ("REMOVE_CANCEL_OWNER",SIGS[3],
   "!jwplc_w5500_candidate::matchesLocked(_jwplcSocketToken)",
   "false","STALE_CANCEL_CLOSED_NEW_OWNER"),
  ("POLL_BUSY_FALSE_DISCONNECT",SIGS[1],
   "if (!bus) return 0;",
   "if (!bus) return -1;","BUS_BUSY_POLL_MISREPORTED_DISCONNECTED"),
]
def say(k,v):print("G10_P1G_C2_S2B2B_"+str(k)+"="+str(v),flush=True)
def need(b,msg):
    if not b:raise RuntimeError(msg)
def sha(path):
    h=hashlib.sha256()
    with Path(path).open("rb") as f:
        for chunk in iter(lambda:f.read(1024*1024),b""):h.update(chunk)
    return h.hexdigest()
def load(rel,name):
    spec=importlib.util.spec_from_file_location(name,ROOT/rel)
    need(spec is not None and spec.loader is not None,"MODULE_LOAD_FAILED_"+name)
    module=importlib.util.module_from_spec(spec)
    sys.modules[name]=module
    spec.loader.exec_module(module)
    return module
def git(*args):
    p=subprocess.run(["git","-c","core.quotepath=false",*args],
       cwd=ROOT,text=True,capture_output=True,errors="replace",timeout=25)
    need(p.returncode==0,"GIT_FAILED_"+repr(args))
    return p.stdout.strip()
def preflight(ci):
    head=git("rev-parse","HEAD")
    need(Path.cwd().resolve()==ROOT and
         Path(git("rev-parse","--show-toplevel")).resolve()==ROOT,
         "NOT_REPO_ROOT")
    need(subprocess.run(["git","merge-base","--is-ancestor",PRIOR_HEAD,head],
         cwd=ROOT,capture_output=True).returncode==0,"S2B2A_NOT_ANCESTOR")
    need(not git("diff","--name-only") and
         not git("diff","--cached","--name-only"),"TRACKED_DIRTY")
    if not ci:
        need(git("branch","--show-current")==BRANCH,"WRONG_BRANCH")
        need(head==git("rev-parse","refs/remotes/origin/"+BRANCH),
             "REMOTE_MISMATCH")
        p=ROOT/"tools/alpha13/results"/PRIOR_RUN/"MANIFEST.json"
        need(p.is_file(),"S2B2A_LOCAL_PASS_MANIFEST_MISSING")
        obj=json.loads(p.read_text(encoding="utf-8-sig"))
        need(obj.get("schema")=="A13-G10-P1G-C2-S2B2A-STOP-1" and
             obj.get("run_id")==PRIOR_RUN and obj.get("head")==PRIOR_HEAD and
             obj.get("status")==
             "COOPERATIVE_STOP_REAL_METHODS_PASS_INCOMPLETE_ADOPTION_BLOCKED",
             "S2B2A_LOCAL_MANIFEST_INVALID")
    gate=load("tools/alpha13/gates/a13_g10_p1g_a_mutex_callpaths.py",
              "s2b2b_frozen_check")
    need(gate.check_frozen()==5,"FROZEN_ARCHIVES_DRIFT")
    gate.check(gate.sources())
    return head

def patch(repro,original):
    bodies={sig:repro.extract(original,sig) for sig in SIGS}
    a=bodies[SIGS[0]]
    need(all(x in a for x in (
        "cancelWriteAsync();","Ethernet.socketBegin(SnMR::TCP, 0);",
        "Ethernet.socketConnect(_sockindex, rawIPAddress(ip), port);",
        "return pollConnectAsync();")),
        "BEGIN_CONNECT_SOURCE_DRIFT")
    a=a.replace(SIGS[0]+"\n{",SIGS[0]+"""
{
\tjwplc_w5500_candidate::BusScope bus(20);
\tif (!bus) {
\t\t_jwplcDeferredConnect = true;
\t\t_jwplcDeferredIp = ip;
\t\t_jwplcDeferredPort = port;
\t\treturn 0; // SPI_BUSY, not failure
\t}
\t_jwplcDeferredConnect = false;
\t// No touching an old socket whose allocation changed.
\tif (_sockindex < MAX_SOCK_NUM &&
\t    !jwplc_w5500_candidate::matchesLocked(_jwplcSocketToken)) {
\t\t_sockindex = MAX_SOCK_NUM;
\t\t_jwplcSocketToken = {};
\t}""",1)
    need(a.count("_sockindex = MAX_SOCK_NUM;\n\t}")==1,
         "REPLACE_OLD_SESSION_ANCHOR_DRIFT")
    a=a.replace("_sockindex = MAX_SOCK_NUM;\n\t}",
        "(void)jwplc_w5500_candidate::invalidateSocketLocked(_jwplcSocketToken);"
        "\n\t\t_sockindex = MAX_SOCK_NUM;\n\t}",1)
    need(a.count("if (_sockindex >= MAX_SOCK_NUM) return -1;")==1,
         "NEW_SOCKET_ANCHOR_CHANGED")
    a=a.replace("if (_sockindex >= MAX_SOCK_NUM) return -1;",
        """if (_sockindex >= MAX_SOCK_NUM) return -1;
\t_jwplcSocketToken = jwplc_w5500_candidate::currentSocketTokenLocked(_sockindex);
\tif (!bool(_jwplcSocketToken)) {
\t\t_sockindex = MAX_SOCK_NUM;
\t\treturn -1; // Never issue CONNECT with an unowned socket.
\t}""",1)
    bodies[SIGS[0]]=a

    b=bodies[SIGS[1]]
    need("uint8_t stat = Ethernet.socketStatus(_sockindex);" in b and
         b.count("_sockindex = MAX_SOCK_NUM;")==1,"POLL_SOURCE_DRIFT")
    b=b.replace(SIGS[1]+"\n{",SIGS[1]+"""
{
\tif (_jwplcDeferredConnect)
\t\treturn beginConnectAsync(_jwplcDeferredIp,_jwplcDeferredPort);
\tif (_sockindex >= MAX_SOCK_NUM) return -1;
\tjwplc_w5500_candidate::BusScope bus(20);
\tif (!bus) return 0;
\tif (!jwplc_w5500_candidate::matchesLocked(_jwplcSocketToken)) {
\t\t_sockindex = MAX_SOCK_NUM;
\t\treturn -1;
\t}""",1)
    b=b.replace("""if (stat == SnSR::CLOSED) {
\t\t_sockindex = MAX_SOCK_NUM;""",
      """if (stat == SnSR::CLOSED) {
\t\t(void)jwplc_w5500_candidate::invalidateSocketLocked(_jwplcSocketToken);
\t\t_sockindex = MAX_SOCK_NUM;""",1)
    bodies[SIGS[1]]=b

    c=bodies[SIGS[2]]
    need("uint8_t stat = Ethernet.socketStatus(_sockindex);" in c,
         "IN_PROGRESS_SOURCE_DRIFT")
    c=c.replace(SIGS[2]+"\n{",SIGS[2]+"""
{
\tif (_jwplcDeferredConnect) return true;
\tif (_sockindex >= MAX_SOCK_NUM) return false;
\tjwplc_w5500_candidate::BusScope bus(20);
\tif (!bus) return true; // Unknown under contention, not disconnected.
\tif (!jwplc_w5500_candidate::matchesLocked(_jwplcSocketToken)) return false;""",1)
    bodies[SIGS[2]]=c

    d=bodies[SIGS[3]]
    need(d.count("Ethernet.socketDisconnect(_sockindex);")==1 and
         d.count("Ethernet.socketClose(_sockindex);")==1,
         "CANCEL_SOURCE_DRIFT")
    d=d.replace(SIGS[3]+"\n{",SIGS[3]+"""
{
\tif (_sockindex >= MAX_SOCK_NUM) {
\t\t_jwplcDeferredConnect = false;
\t\treturn;
\t}
\tjwplc_w5500_candidate::BusScope bus(20);
\tif (!bus) return; // Keep identity; retry required.
\t_jwplcDeferredConnect = false;
\tif (!jwplc_w5500_candidate::matchesLocked(_jwplcSocketToken)) {
\t\t_sockindex = MAX_SOCK_NUM;
\t\treturn;
\t}""",1)
    d=d.replace("""\t_sockindex = MAX_SOCK_NUM;
}""",
      """\t(void)jwplc_w5500_candidate::invalidateSocketLocked(_jwplcSocketToken);
\t_sockindex = MAX_SOCK_NUM;
}""",1)
    bodies[SIGS[3]]=d

    out=original
    for sig in SIGS:
        before=repro.extract(original,sig)
        after=bodies[sig]
        need(after!=before and out.count(before)==1,
             "PATCH_MISSING_OR_AMBIGUOUS_"+sig)
        out=out.replace(before,after,1)
    return out,bodies

def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--ci-standalone",action="store_true")
    args=ap.parse_args()
    runid=("g10_p1g_c2_s2b2b_"+datetime.now().strftime("%Y%m%d_%H%M%S")
           +"_"+uuid.uuid4().hex[:8])
    out=ROOT/"tools/alpha13/results"/runid
    out.mkdir(parents=True,exist_ok=False)
    m={"schema":"A13-G10-P1G-C2-S2B2B-OUTBOUND-1",
       "run_id":runid,"status":"REVIEW_STOP",
       "product_mutations":False,"hardware_access":False,
       "arduino_build":False,"gpio4_enabled":False,"adoption":False}
    try:
        h=preflight(args.ci_standalone)
        repro=load("tools/alpha13/gates/a13_g10_p1a_a13_006_repro.py",
                   "s2b2b_extract")
        s2b1=load("tools/alpha13/gates/a13_g10_p1g_c2_s2b1_client_server_lease.py",
                   "s2b2b_prior_handle")
        s2b2a=load("tools/alpha13/gates/a13_g10_p1g_c2_s2b2a_cooperative_stop.py",
                   "s2b2b_prior_stop")
        c1=load("tools/alpha13/gates/a13_g10_p1g_c1_native_prototype.py",
                 "s2b2b_native_cxx")
        originals={k:(ROOT/path).read_text(encoding="utf-8-sig")
                   for k,path in s2b1.FILES.items()}
        ov=s2b1.patch(repro,originals)
        ov["CLIENT"],_ = s2b2a.patched_methods(repro,ov["CLIENT"])
        ov["HEADER"]=ov["HEADER"].replace(
            "jwplc_w5500_candidate::SocketToken _jwplcSocketToken{};",
            """jwplc_w5500_candidate::SocketToken _jwplcSocketToken{};
\tbool _jwplcDeferredConnect = false;
\tIPAddress _jwplcDeferredIp;
\tuint16_t _jwplcDeferredPort = 0;""",1)
        need(ov["HEADER"].count("_jwplcDeferredConnect = false;")==1,
             "DEFERRED_FIELDS_NOT_ADDED")
        ov["CLIENT"],bodies=patch(repro,ov["CLIENT"])
        paths={}
        for key,path in s2b1.FILES.items():
            dest=out/"overlay"/path
            dest.parent.mkdir(parents=True,exist_ok=True)
            dest.write_text(ov[key],encoding="utf-8")
            paths[path]=sha(dest)
        # Reuse S2B2A native C++ compiler/stubs, with the outbound harness.
        s2b2a.TEST=TEST
        def build(tag,table):
            inc="\n\n".join(table[sig] for sig in SIGS)+"\n"
            return s2b2a.native(out,tag,inc,c1)
        # Host harness includes C2_S2B2A_REAL_PATCHED_METHODS.inc by design.
        rc,log=build("positive",bodies)
        need(rc==0 and all(t in log for t in EXPECTED),
             "POSITIVE_OUTBOUND_NATIVE_FAILED")
        say("POSITIVE_NATIVE","PASS_REAL_OUTBOUND_LIFECYCLE")
        for label,sig,old,new,expected in NEGATIVES:
            need(bodies[sig].count(old)==1,
                 "NEGATIVE_ANCHOR_"+label)
            alt=dict(bodies)
            alt[sig]=bodies[sig].replace(old,new,1)
            rc,log=build("negative_"+label,alt)
            if rc==0 or expected not in log:
                say("NEGATIVE_DIAG_"+label,
                    "EXIT_"+str(rc)+"_"+log.strip().replace("\n"," | ")[:450])
            need(rc!=0 and expected in log,"NEGATIVE_NOT_REJECTED_"+label)
            say("NEGATIVE_"+label,"PASS_REJECTED")
        m.update({
           "status":"OUTBOUND_CONNECT_REAL_CPP_NATIVE_PASS_INCOMPLETE_ADOPTION_BLOCKED",
           "head":h,"cpp_method_count":len(SIGS),"overlay_files":paths,
           "native_negative_count":len(NEGATIVES),
           "harness_sha256":sha(TEST),"safe_access_sha256":sha(C1_CPP),
           "gaps":[
             "connect(IPAddress) legacy timeout cancellation may leave deferred request under SPI_BUSY",
             "connect(const char*) DNS and legacy old-session cleanup not protected",
             "cancelWriteAsync may close socket under valid lease; requires separate write gate",
             "SPI_BUSY during cancelConnectAsync leaves request for explicit retry",
             "current raw connected()/read()/write() still not lease-safe",
             "mocked EthernetClass socketBegin; source-backed separately S2A",
             "not a full Arduino compile, no SD/FRAM/TFT fairness or GPIO4 reset test"],
           "product_modified":False
        })
        say("HEAD",h)
        say("REAL_CPP_METHODS",len(SIGS))
        say("OVERLAY_FILES",len(paths))
        say("NEGATIVES",len(NEGATIVES))
        say("FROZEN_ARCHIVES",5)
        say("PRODUCT_CHANGED","NO")
        say("STATUS",m["status"])
        say("RESULTS",out)
        return 0
    except Exception as err:
        m["reason"]=repr(err)
        say("STATUS","REVIEW_STOP")
        say("REASON",repr(err))
        say("RESULTS",out)
        return 2
    finally:
        (out/"MANIFEST.json").write_text(
             json.dumps(m,indent=2,ensure_ascii=False)+"\n",
             encoding="utf-8")
if __name__=="__main__":
    try:sys.exit(main())
    except Exception as e:
        say("FATAL",repr(e))
        sys.exit(2)
