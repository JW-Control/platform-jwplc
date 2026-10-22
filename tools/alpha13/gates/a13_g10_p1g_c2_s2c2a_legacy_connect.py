#!/usr/bin/env python3
"""A13 G10-P1G-C2-S2C2A: host pre-DNS owner safety and legacy IP timeout.

Source-backed host native overlay. DNS itself still unprotected (real
EthernetUDP/DNS methods NOT compiled). Adoption/Arduino/hardware blocked.
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
PREV_HEAD="07e72ce9498208ed573dc80741b8fe6c5ef1ef57"
PREV_RUN="g10_p1g_c2_s2c1_20261013_133216_bf7c8acb"
SRC="JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/EthernetClient.cpp"
BASE=ROOT/"tools/alpha13/candidates/g10_p1g_c"
TEST=BASE/"native_c2_s2c2a_legacy_connect_test.cpp"
C1=BASE/"JWPLC_W5500_SafeAccess.cpp"
IP="int EthernetClient::connect(IPAddress ip, uint16_t port)"
HOST="int EthernetClient::connect(const char * host, uint16_t port)"
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
 "C2_S2C2A_IP_BLOCKING_SUCCESS=PASS",
 "C2_S2C2A_IP_BUSY_TIMEOUT_NO_PHANTOM=PASS",
 "C2_S2C2A_HOST_DNS_AND_OLD_OWNER=PASS",
 "C2_S2C2A_HOST_BUSY_NO_DESTRUCTIVE_DNS=PASS",
 "C2_S2C2A_STALE_HOST_AND_DNS_FAILURE=PASS",
 "C2_S2C2A_NATIVE_RESULT=PASS_IP_HOST_PRE_DNS_SLICE_ADOPTION_BLOCKED",
]
NEGATIVES=[
 ("HOST_NO_GUARD",HOST,
  "jwplc_w5500_candidate::BusScope bus(20);",
  "bool bus = true; // no ownership guard",
  "HOST_PRE_DNS_CLEANUP_OR_DNS_MISSING"),
 ("HOST_NO_OWNER_VALIDATION",HOST,
  "!jwplc_w5500_candidate::matchesLocked(_jwplcSocketToken)",
  "false","HOST_STALE_PRE_DNS_TOUCHED_WRONG_OWNER"),
 ("HOST_BUSY_DROPS_HANDLE",HOST,
  "if (!bus) return 0; // preserve active socket under contention",
  "if (!bus) { _sockindex=MAX_SOCK_NUM; return 0; }",
  "HOST_BUSY_DROPPED_HANDLE_OR_RAN_DNS"),
 ("HOST_DNS_ACQUIRES_LONG_LOCK",HOST,
  "dns.begin(Ethernet.dnsServerIP());",
  "{ jwplc_w5500_candidate::BusScope dnsGuard(20); dns.begin(Ethernet.dnsServerIP()); }",
  "DNS_LOOKUP_HOLDING_SPI_MUTEX"),
 ("IP_TIMEOUT_NO_CANCEL",IP,
  "cancelConnectAsync();",
  "/* no deferred connect cleanup */",
  "TIMED_OUT_DEFERRED_CONNECT_LEAKED"),
]
def say(k,v):print("G10_P1G_C2_S2C2A_"+str(k)+"="+str(v),flush=True)
def need(value,why):
    if not value: raise RuntimeError(why)
def sha(path):
    h=hashlib.sha256()
    with Path(path).open("rb") as f:
        for chunk in iter(lambda:f.read(1024*1024),b""):h.update(chunk)
    return h.hexdigest()
def load(path,name):
    spec=importlib.util.spec_from_file_location(name,ROOT/path)
    need(spec and spec.loader,"IMPORT_FAILED_"+name)
    o=importlib.util.module_from_spec(spec)
    sys.modules[name]=o
    spec.loader.exec_module(o)
    return o
def git(*args):
    p=subprocess.run(["git","-c","core.quotepath=false",*args],cwd=ROOT,
        capture_output=True,text=True,errors="replace",timeout=30)
    need(p.returncode==0,"GIT_FAILED_"+repr(args))
    return p.stdout.strip()
def preflight(ci):
    head=git("rev-parse","HEAD")
    need(Path.cwd().resolve()==ROOT and
         Path(git("rev-parse","--show-toplevel")).resolve()==ROOT,"NOT_REPO_ROOT")
    need(subprocess.run(["git","merge-base","--is-ancestor",PREV_HEAD,head],
         cwd=ROOT,capture_output=True).returncode==0,"PREVIOUS_HEAD_NOT_ANCESTOR")
    need(not git("diff","--name-only") and
         not git("diff","--cached","--name-only"),"GIT_TRACKED_DIRTY")
    if not ci:
        need(git("branch","--show-current")==BRANCH,"WRONG_BRANCH")
        need(head==git("rev-parse","refs/remotes/origin/"+BRANCH),
             "LOCAL_REMOTE_HEAD_MISMATCH")
        path=ROOT/"tools/alpha13/results"/PREV_RUN/"MANIFEST.json"
        need(path.is_file(),"LOCAL_S2C1_PASS_MANIFEST_MISSING")
        m=json.loads(path.read_text(encoding="utf-8-sig"))
        need(m.get("schema")=="A13-G10-P1G-C2-S2C1-TX-CANCEL-1" and
             m.get("run_id")==PREV_RUN and
             m.get("head")==PREV_HEAD and
             m.get("status")=="TX_CANCEL_REAL_CPP_NATIVE_PASS_INCOMPLETE_ADOPTION_BLOCKED",
             "LOCAL_S2C1_MANIFEST_MISMATCH")
    frozen=load("tools/alpha13/gates/a13_g10_p1g_a_mutex_callpaths.py",
                "s2c2a_frozen")
    need(frozen.check_frozen()==5,"FROZEN_ARCHIVES_CHANGED")
    frozen.check(frozen.sources())
    return head

def patch_legacy(repro,source):
    ip=repro.extract(source,IP)
    host=repro.extract(source,HOST)
    need("int state = beginConnectAsync(ip, port);" in ip and
         "cancelConnectAsync();" in ip,"IP_LEGACY_DRIFT")
    need("DNSClient dns;" in host and "dns.getHostByName(host, remote_addr)" in host and
         host.count("Ethernet.socketDisconnect(_sockindex);")==1,
         "HOST_LEGACY_DRIFT")
    # IP blocking follows the real source body (no semantic change): its
    # timeout calls cancelConnectAsync, which must clear DEFERRED request.
    # This test does not prove that an ALLOCATED socket under BUS_BUSY is
    # automatically released without future service.
    modified_ip=ip
    needle="""\tif (_sockindex < MAX_SOCK_NUM) {
\t\tif (Ethernet.socketStatus(_sockindex) != SnSR::CLOSED) {
\t\t\tEthernet.socketDisconnect(_sockindex); // TODO: should we call stop()?
\t\t}
\t\t_sockindex = MAX_SOCK_NUM;
\t}"""
    need(host.count(needle)==1,"HOST_OLD_SESSION_ANCHOR_DRIFT")
    modified_host=host.replace(needle, """\tif (_sockindex < MAX_SOCK_NUM) {
\t\tjwplc_w5500_candidate::BusScope bus(20);
\t\tif (!bus) return 0; // preserve active socket under contention
\t\tif (!jwplc_w5500_candidate::matchesLocked(_jwplcSocketToken)) {
\t\t\t_sockindex = MAX_SOCK_NUM; // stale copied handle
\t\t} else {
\t\t\tif (Ethernet.socketStatus(_sockindex) != SnSR::CLOSED)
\t\t\t\tEthernet.socketDisconnect(_sockindex);
\t\t\t(void)jwplc_w5500_candidate::invalidateSocketLocked(_jwplcSocketToken);
\t\t\t_sockindex = MAX_SOCK_NUM;
\t\t}
\t}""",1)
    need(modified_host!=host,"HOST_PATCH_FAILED")
    output=source.replace(host,modified_host,1)
    return output,{IP:modified_ip,HOST:modified_host}

def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--ci-standalone",action="store_true")
    args=ap.parse_args()
    rid="g10_p1g_c2_s2c2a_"+datetime.now().strftime("%Y%m%d_%H%M%S")+"_"+uuid.uuid4().hex[:8]
    out=ROOT/"tools/alpha13/results"/rid
    out.mkdir(parents=True,exist_ok=False)
    manifest={"schema":"A13-G10-P1G-C2-S2C2A-LEGACY-CONNECT-1",
              "run_id":rid,"status":"REVIEW_STOP",
              "product_changed":False,"arduino_compiled":False,
              "hardware_touched":False,"candidate_adopted":False,"gpio4_enabled":False}
    try:
        head=preflight(args.ci_standalone)
        repro=load("tools/alpha13/gates/a13_g10_p1a_a13_006_repro.py",
                   "s2c2a_extract")
        prior=load("tools/alpha13/gates/a13_g10_p1g_c2_s2c1_tx_cancel.py",
                   "s2c2a_tx")
        s2b1=load("tools/alpha13/gates/a13_g10_p1g_c2_s2b1_client_server_lease.py",
                   "s2c2a_handles")
        s2b2a=load("tools/alpha13/gates/a13_g10_p1g_c2_s2b2a_cooperative_stop.py",
                    "s2c2a_stop")
        s2b2b=load("tools/alpha13/gates/a13_g10_p1g_c2_s2b2b_outbound_connect.py",
                    "s2c2a_outbound")
        c1=load("tools/alpha13/gates/a13_g10_p1g_c1_native_prototype.py",
                "s2c2a_cxx")
        originals={k:(ROOT/path).read_text(encoding="utf-8-sig")
                   for k,path in s2b1.FILES.items()}
        ov=s2b1.patch(repro,originals)
        ov["CLIENT"],_=s2b2a.patched_methods(repro,ov["CLIENT"])
        ov["HEADER"]=ov["HEADER"].replace(
            "jwplc_w5500_candidate::SocketToken _jwplcSocketToken{};",
            """jwplc_w5500_candidate::SocketToken _jwplcSocketToken{};
\tbool _jwplcDeferredConnect = false;
\tIPAddress _jwplcDeferredIp;
\tuint16_t _jwplcDeferredPort = 0;""",1)
        ov["CLIENT"],connection=s2b2b.patch(repro,ov["CLIENT"])
        ov["CLIENT"],tx=prior.patch_tx(repro,ov["CLIENT"])
        ov["CLIENT"],blocking=patch_legacy(repro,ov["CLIENT"])
        paths={}
        for key,path in s2b1.FILES.items():
            dest=out/"overlay"/path
            dest.parent.mkdir(parents=True,exist_ok=True)
            dest.write_text(ov[key],encoding="utf-8")
            paths[path]=sha(dest)
        all_methods=[connection[s] for s in s2b2b.SIGS]+[tx]+[blocking[IP],blocking[HOST]]
        s2b2a.TEST=TEST
        def run(label, bodies):
            return s2b2a.native(out,label,"\n\n".join(bodies)+"\n",c1)
        rc,log=run("positive",all_methods)
        if rc!=0 or not all(x in log for x in EXPECTED):
            say("POSITIVE_DIAG","EXIT_"+str(rc)+"_"+log.strip().replace("\n"," | ")[:1300])
        need(rc==0 and all(x in log for x in EXPECTED),"POSITIVE_LEGACY_SOURCE_FAILED")
        say("POSITIVE_NATIVE","PASS_SOURCE_IP_HOST_PRE_DNS")
        for label,sig,before,after,expected in NEGATIVES:
            target=blocking[sig]
            need(target.count(before)==1,"NEGATIVE_ANCHOR_"+label)
            mutant=dict(blocking);mutant[sig]=target.replace(before,after,1)
            bodies=[connection[s] for s in s2b2b.SIGS]+[tx]+[mutant[IP],mutant[HOST]]
            rc,log=run("negative_"+label,bodies)
            if rc==0 or expected not in log:
                say("NEGATIVE_DIAG_"+label,
                    "EXIT_"+str(rc)+"_"+log.strip().replace("\n"," | ")[:650])
            need(rc!=0 and expected in log,"NEGATIVE_NOT_REJECTED_"+label)
            say("NEGATIVE_"+label,"PASS_REJECTED")
        manifest.update({
           "head":head,
           "status":"LEGACY_CONNECT_IP_HOST_PREDNS_NATIVE_PASS_DNS_UNPROTECTED_ADOPTION_BLOCKED",
           "source_methods_compiled":7,"overlay_files":paths,
           "negative_mutations":len(NEGATIVES),
           "original_source_sha256":sha(ROOT/SRC),
           "harness_sha256":sha(TEST),
           "c1_sha256":sha(C1),
           "known_limitations":[
               "DNSClient getHostByName uses real EthernetUDP currently not W5500 guarded",
               "DNS host resolution tested only via mock, NOT actual DNS/UDP",
               "IP connect timeout while previously allocated socket SPI_BUSY can leave a handle requiring retry",
               "Other raw TCP read/write and W5500 socket layer not globally protected",
               "Arduino CLI full build, timing fairness SD/FRAM/TFT and physical GPIO4 not tested",
           ],
        })
        say("HEAD",head)
        say("REAL_CPP_METHODS",len(all_methods))
        say("OVERLAY_FILES",len(paths))
        say("NEGATIVES",len(NEGATIVES))
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
          json.dumps(manifest,indent=2,ensure_ascii=False)+"\n",
          encoding="utf-8")
if __name__=="__main__":
    try:sys.exit(main())
    except Exception as e:
        say("FATAL",repr(e))
        sys.exit(2)
