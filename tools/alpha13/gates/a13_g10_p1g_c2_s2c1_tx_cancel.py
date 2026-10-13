#!/usr/bin/env python3
"""G10-P1G-C2-S2C1: cancelWriteAsync real, combinado con 4 métodos outbound.

Overlay aislado; compilación host C++ con stubs; no adopción productiva,
ni compilación Arduino, ni prueba hardware. No cerrar A13-006.
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
PRIOR_RUN="g10_p1g_c2_s2b2b_20261013_124846_9fdc28d7"
PRIOR_HEAD="755a9b91b379685bd5404b2a448d9d59e32c888a"
SRC="JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/EthernetClient.cpp"
BASE=ROOT/"tools/alpha13/candidates/g10_p1g_c"
TEST=BASE/"native_c2_s2c1_tx_cancel_test.cpp"
C1=BASE/"JWPLC_W5500_SafeAccess.cpp"
SIGNATURE="void EthernetClient::cancelWriteAsync()"
EXPECTED=[
  "C2_S2B2B_OUTBOUND_CONNECT_POLL=PASS",
  "C2_S2B2B_SPI_BUSY_START_AND_POLL=PASS",
  "C2_S2B2B_RETARGET_AND_STALE_ALIAS=PASS",
  "C2_S2B2B_CANCEL_BUSY_RETRY_OWNER=PASS",
  "C2_S2B2B_RESET_AND_INVALID_IP=PASS",
  "C2_S2C1_CANCEL_AFTER_SEND_INVALIDATES_LEASE=PASS",
  "C2_S2C1_BUSY_TX_STATE_RETRY=PASS",
  "C2_S2C1_STALE_TX_COPY_AND_UNSENT=PASS",
  "C2_S2C1_TX_RETARGET_NESTED_GUARD=PASS",
  "C2_S2C1_NATIVE_RESULT=PASS_TX_CANCEL_OUTBOUND_SLICE_ADOPTION_BLOCKED",
]
NEGATIVES=[
 ("NO_TX_GUARD",
  "jwplc_w5500_candidate::BusScope bus(20);",
  "bool bus = true; // deliberate no mutex",
  "SOCKET_CLOSE_UNLOCKED"),
 ("NO_TX_OWNER",
  "!jwplc_w5500_candidate::matchesLocked(_jwplcSocketToken)",
  "false",
  "STALE_TX_CANCEL_CLOSED_NEW_OWNER"),
 ("BUSY_DISCARDS_TX",
  "if (!bus) return; // SPI_BUSY: preserve TX state/handle.",
  "if (!bus) { _writeAsyncState = 0; return; }",
  "BUS_BUSY_LOST_WRITE_STATE"),
 ("NO_TX_LEASE_INVALIDATION",
  "(void)jwplc_w5500_candidate::invalidateSocketLocked(_jwplcSocketToken);",
  "(void)0;",
  "LEASE_STILL_VALID_AFTER_CLOSE"),
 ("SKIP_TX_HARDWARE_CLOSE",
  "Ethernet.socketClose(_sockindex);",
  "(void)_sockindex;",
  "CLOSE_NOT_ISSUED_WHEN_SEND_PENDING"),
]
def say(k,v): print("G10_P1G_C2_S2C1_"+str(k)+"="+str(v),flush=True)
def need(ok,why):
    if not ok: raise RuntimeError(why)
def sha(path):
    h=hashlib.sha256()
    with Path(path).open("rb") as f:
        for buf in iter(lambda:f.read(1024*1024),b""):h.update(buf)
    return h.hexdigest()
def load(rel,name):
    spec=importlib.util.spec_from_file_location(name,ROOT/rel)
    need(spec is not None and spec.loader is not None,"MODULE_MISSING_"+name)
    v=importlib.util.module_from_spec(spec)
    sys.modules[name]=v
    spec.loader.exec_module(v)
    return v
def git(*args):
    run=subprocess.run(["git","-c","core.quotepath=false",*args],
        cwd=ROOT,capture_output=True,text=True,errors="replace",timeout=30)
    need(run.returncode==0,"GIT_FAILED_"+repr(args))
    return run.stdout.strip()
def preflight(ci):
    need(Path.cwd().resolve()==ROOT and
         Path(git("rev-parse","--show-toplevel")).resolve()==ROOT,
         "REPO_ROOT_REQUIRED")
    head=git("rev-parse","HEAD")
    need(subprocess.run(["git","merge-base","--is-ancestor",PRIOR_HEAD,head],
         cwd=ROOT,capture_output=True).returncode==0,"S2B2B_NOT_ANCESTOR")
    need(not git("diff","--name-only") and
         not git("diff","--cached","--name-only"),"GIT_TRACKED_DIRTY")
    if not ci:
        need(git("branch","--show-current")==BRANCH,"WRONG_BRANCH")
        need(head==git("rev-parse","refs/remotes/origin/"+BRANCH),
             "LOCAL_REMOTE_MISMATCH")
        p=ROOT/"tools/alpha13/results"/PRIOR_RUN/"MANIFEST.json"
        need(p.is_file(),"LOCAL_S2B2B_PASS_MANIFEST_MISSING")
        m=json.loads(p.read_text(encoding="utf-8-sig"))
        need(m.get("schema")=="A13-G10-P1G-C2-S2B2B-OUTBOUND-1" and
             m.get("run_id")==PRIOR_RUN and
             m.get("head")==PRIOR_HEAD and
             m.get("status")=="OUTBOUND_CONNECT_REAL_CPP_NATIVE_PASS_INCOMPLETE_ADOPTION_BLOCKED",
             "LOCAL_S2B2B_MANIFEST_NOT_QUALIFIED")
    frozen=load("tools/alpha13/gates/a13_g10_p1g_a_mutex_callpaths.py",
                "s2c1_frozen")
    need(frozen.check_frozen()==5,"FROZEN_PRECOMPILED_CHANGED")
    frozen.check(frozen.sources())
    return head
def patch_tx(repro,source):
    before=repro.extract(source,SIGNATURE)
    need(before.startswith(SIGNATURE+"\n{") and
         "const bool sendWasIssued = _writeAsyncState == 2;" in before and
         before.count("Ethernet.socketClose(_sockindex);")==1,
         "TX_CANCEL_SOURCE_DRIFT")
    after=before.replace(SIGNATURE+"\n{",SIGNATURE+"""
{
\t// Guard before touching any shared W5500 socket, including SEND pending.
\tjwplc_w5500_candidate::BusScope bus(20);
\tif (!bus) return; // SPI_BUSY: preserve TX state/handle.
\tif (_sockindex < MAX_SOCK_NUM &&
\t    !jwplc_w5500_candidate::matchesLocked(_jwplcSocketToken)) {
\t\t_sockindex=MAX_SOCK_NUM; // no hardware operation on recycled slot
\t\t_writeAsyncState=0;
\t\t_writeAsyncBuffer=nullptr;
\t\t_writeAsyncLength=0;
\t\t_writeAsyncStartedAtMs=0;
\t\treturn;
\t}""",1)
    after=after.replace("Ethernet.socketClose(_sockindex);",
      "Ethernet.socketClose(_sockindex);\n"
      "\t\t(void)jwplc_w5500_candidate::invalidateSocketLocked(_jwplcSocketToken);",1)
    need(after!=before and source.count(before)==1,"TX_PATCH_NOT_UNIQUE")
    return source.replace(before,after,1),after
def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--ci-standalone",action="store_true")
    args=ap.parse_args()
    runid="g10_p1g_c2_s2c1_"+datetime.now().strftime("%Y%m%d_%H%M%S")+"_"+uuid.uuid4().hex[:8]
    out=ROOT/"tools/alpha13/results"/runid
    out.mkdir(parents=True,exist_ok=False)
    manifest={
        "schema":"A13-G10-P1G-C2-S2C1-TX-CANCEL-1","run_id":runid,
        "status":"REVIEW_STOP","product_modified":False,
        "arduino_compiled":False,"hardware_tested":False,
        "gpio4_enabled":False,"adopted":False
    }
    try:
        head=preflight(args.ci_standalone)
        repro=load("tools/alpha13/gates/a13_g10_p1a_a13_006_repro.py",
                   "s2c1_extract")
        prior=load("tools/alpha13/gates/a13_g10_p1g_c2_s2b2b_outbound_connect.py",
                   "s2c1_outbound")
        s2b1=load("tools/alpha13/gates/a13_g10_p1g_c2_s2b1_client_server_lease.py",
                  "s2c1_earlier_handles")
        s2b2a=load("tools/alpha13/gates/a13_g10_p1g_c2_s2b2a_cooperative_stop.py",
                  "s2c1_stop")
        c1=load("tools/alpha13/gates/a13_g10_p1g_c1_native_prototype.py",
                "s2c1_host")
        originals={k:(ROOT/p).read_text(encoding="utf-8-sig")
                   for k,p in s2b1.FILES.items()}
        ov=s2b1.patch(repro,originals)
        ov["CLIENT"],_=s2b2a.patched_methods(repro,ov["CLIENT"])
        ov["HEADER"]=ov["HEADER"].replace(
          "jwplc_w5500_candidate::SocketToken _jwplcSocketToken{};",
          """jwplc_w5500_candidate::SocketToken _jwplcSocketToken{};
\tbool _jwplcDeferredConnect = false;
\tIPAddress _jwplcDeferredIp;
\tuint16_t _jwplcDeferredPort = 0;""",1)
        ov["CLIENT"],connect_methods=prior.patch(repro,ov["CLIENT"])
        ov["CLIENT"],tx_body=patch_tx(repro,ov["CLIENT"])
        cpp_methods=[connect_methods[s] for s in prior.SIGS]+[tx_body]
        targets={}
        for key,path in s2b1.FILES.items():
            dest=out/"overlay"/path
            dest.parent.mkdir(parents=True,exist_ok=True)
            dest.write_text(ov[key],encoding="utf-8")
            targets[path]=sha(dest)
        # Compiles source-extracted OUTBOUND and TX cancellation in same binary.
        s2b2a.TEST=TEST
        def build(label,tx):
            sources="\n\n".join(cpp_methods[:-1]+[tx])+"\n"
            return s2b2a.native(out,label,sources,c1)
        rc,log=build("positive",tx_body)
        need(rc==0 and all(x in log for x in EXPECTED),
             "POSITIVE_TX_CANCEL_SOURCE_TEST_FAILED")
        say("POSITIVE_NATIVE","PASS_REAL_TX_CANCEL_WITH_OUTBOUND")
        for label,before,after,error in NEGATIVES:
            need(tx_body.count(before)==1,"NEGATIVE_ANCHOR_NOT_UNIQUE_"+label)
            bad=tx_body.replace(before,after,1)
            rc,log=build("negative_"+label,bad)
            if rc==0 or error not in log:
                say("NEGATIVE_DEBUG_"+label,
                    "EXIT_"+str(rc)+"_"+log.strip().replace("\n"," | ")[:420])
            need(rc!=0 and error in log,"NEGATIVE_NOT_REJECTED_"+label)
            say("NEGATIVE_"+label,"PASS_REJECTED")
        manifest.update({
          "head":head,"status":"TX_CANCEL_REAL_CPP_NATIVE_PASS_INCOMPLETE_ADOPTION_BLOCKED",
          "real_cpp_methods":len(cpp_methods),"overlay_sources":targets,
          "negative_mutations":len(NEGATIVES),
          "source_sha256":sha(ROOT/SRC),
          "candidate_sha256":sha(C1),
          "test_sha256":sha(TEST),
          "limits":[
            "pollWriteAsync/write/read not yet proved with actual function bodies",
            "connect(IPAddress)/connect(host)/DNS legacy not yet protected",
            "cancelWriteAsync under SPI_BUSY keeps state and needs later retry",
            "socketBegin mocked here; separate S2A compiles source method",
            "UDP/DHCP/RTC/FRAM/SD/TFT actual firmware not compiled",
            "All product sources and GPIO4 remain unchanged",
          ]
        })
        say("HEAD",head)
        say("REAL_CPP_METHODS",len(cpp_methods))
        say("OVERLAY_FILES",len(targets))
        say("NEGATIVES",len(NEGATIVES))
        say("FROZEN_ARCHIVES",5)
        say("PRODUCT_CHANGED","NO")
        say("STATUS",manifest["status"])
        say("RESULTS",out)
        return 0
    except Exception as err:
        manifest["reason"]=repr(err)
        say("STATUS","REVIEW_STOP")
        say("REASON",repr(err))
        say("RESULTS",out)
        return 2
    finally:
        (out/"MANIFEST.json").write_text(
            json.dumps(manifest,ensure_ascii=False,indent=2)+"\n",
            encoding="utf-8")
if __name__=="__main__":
    try:sys.exit(main())
    except Exception as err:
        say("FATAL",repr(err))
        sys.exit(2)
