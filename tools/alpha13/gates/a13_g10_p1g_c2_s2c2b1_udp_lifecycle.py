#!/usr/bin/env python3
"""C2-S2C2B1: six SOURCE-BACKED UDP lifecycle methods, W5500 lease / SPI.

No DNS implementation compiled here; separate S2C2B2. Never modify package,
flash, or claim C2 adoption from host mocks. Fail closed on source drift.
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
PRIOR_RUN="g10_p1g_c2_s2c2a_20261013_134702_6bdf6565"
PRIOR_HEAD="116dd26a953e8838e4d14ce376ae4d8ff063bd32"
UDP_PATH="JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/EthernetUdp.cpp"
HEADER_PATH="JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/JWPLC_W5x00_Ethernet.h"
BASE=ROOT/"tools/alpha13/candidates/g10_p1g_c"
TEST=BASE/"native_c2_s2c2b1_udp_lifecycle_test.cpp"
C1_CPP=BASE/"JWPLC_W5500_SafeAccess.cpp"
SIGS=[
    "uint8_t EthernetUDP::begin(uint16_t port)",
    "void EthernetUDP::stop()",
    "int EthernetUDP::beginPacket(IPAddress ip, uint16_t port)",
    "int EthernetUDP::beginEndPacketAsync()",
    "int EthernetUDP::pollEndPacketAsync()",
    "void EthernetUDP::cancelEndPacketAsync()",
]
EXPECTED=[
    "C2_S2C2B1_UDP_BEGIN_SEND_POLL_STOP=PASS",
    "C2_S2C2B1_UDP_BUSY_NO_HANDLE_LOSS=PASS",
    "C2_S2C2B1_UDP_STALE_OWNER_REJECTED=PASS",
    "C2_S2C2B1_UDP_BUSY_SEND_DEFERRED=PASS",
    "C2_S2C2B1_UDP_PACKET_BUSY_AND_RESET_STALE=PASS",
    "C2_S2C2B1_NATIVE_RESULT=PASS_UDP_LIFECYCLE_SLICE_ADOPTION_BLOCKED",
]
NEGATIVES=[
    ("OPEN_NO_GUARD",SIGS[0],
      "jwplc_w5500_candidate::BusScope bus(20);",
      "bool bus=true; // no physical lock",
      "UDP_BEGIN_WITHOUT_GUARD"),
    ("STOP_NO_OWNER",SIGS[1],
      "jwplc_w5500_candidate::matchesLocked(_jwplcSocketToken)",
      "true","UDP_STALE_CLOSED_NEW_OWNER"),
    ("STOP_BUSY_DROPS_HANDLE",SIGS[1],
      "if (!bus) return; // never discard socket on SPI_BUSY",
      "if (!bus) { sockindex=MAX_SOCK_NUM; return; }",
      "UDP_BUSY_STOP_LOST_HANDLE"),
    ("SEND_BUSY_NOT_DEFERRED",SIGS[3],
      "_jwplcDeferredSendStart = true;",
      "_jwplcDeferredSendStart = false;",
      "UDP_BUSY_SEND_WAS_NOT_DEFERRED"),
    ("POLL_NO_LEASE",SIGS[4],
      "!jwplc_w5500_candidate::matchesLocked(_jwplcSocketToken)",
      "false",
      "UDP_POLL_WITHOUT_TOKEN"),
]
def say(k,v):print("G10_P1G_C2_S2C2B1_"+str(k)+"="+str(v),flush=True)
def need(ok,msg):
    if not ok:raise RuntimeError(msg)
def sha(path):
    h=hashlib.sha256()
    with Path(path).open("rb") as f:
        for chunk in iter(lambda:f.read(1024*1024),b""):h.update(chunk)
    return h.hexdigest()
def mod(path,name):
    spec=importlib.util.spec_from_file_location(name,ROOT/path)
    need(spec is not None and spec.loader is not None,"MODULE_NOT_FOUND_"+name)
    m=importlib.util.module_from_spec(spec)
    sys.modules[name]=m
    spec.loader.exec_module(m)
    return m
def git(*args):
    p=subprocess.run(["git","-c","core.quotepath=false",*args],
                     cwd=ROOT,capture_output=True,text=True,errors="replace",timeout=25)
    need(p.returncode==0,"GIT_FAILED_"+repr(args))
    return p.stdout.strip()
def preflight(ci):
    need(Path.cwd().resolve()==ROOT and
         Path(git("rev-parse","--show-toplevel")).resolve()==ROOT,
         "REPOSITORY_ROOT_REQUIRED")
    head=git("rev-parse","HEAD")
    need(subprocess.run(["git","merge-base","--is-ancestor",PRIOR_HEAD,head],
         cwd=ROOT,capture_output=True).returncode==0,"S2C2A_NOT_ANCESTOR")
    need(not git("diff","--name-only") and
         not git("diff","--cached","--name-only"),"TRACKED_DIRTY")
    if not ci:
        need(git("branch","--show-current")==BRANCH,"WRONG_BRANCH")
        need(head==git("rev-parse","refs/remotes/origin/"+BRANCH),
             "LOCAL_REMOTE_HEAD_MISMATCH")
        p=ROOT/"tools/alpha13/results"/PRIOR_RUN/"MANIFEST.json"
        need(p.is_file(),"S2C2A_LOCAL_PASS_MANIFEST_MISSING")
        m=json.loads(p.read_text(encoding="utf-8-sig"))
        need(m.get("schema")=="A13-G10-P1G-C2-S2C2A-LEGACY-CONNECT-1" and
             m.get("run_id")==PRIOR_RUN and m.get("head")==PRIOR_HEAD and
             m.get("status")==
               "LEGACY_CONNECT_IP_HOST_PREDNS_NATIVE_PASS_DNS_UNPROTECTED_ADOPTION_BLOCKED",
             "S2C2A_LOCAL_MANIFEST_NOT_QUALIFIED")
    frozen=mod("tools/alpha13/gates/a13_g10_p1g_a_mutex_callpaths.py",
               "s2c2b1_frozen")
    need(frozen.check_frozen()==5,"FROZEN_ARCHIVE_SHA_CHANGED")
    frozen.check(frozen.sources())
    return head

def patches(repro,source):
    old={sig:repro.extract(source,sig) for sig in SIGS}
    b=dict(old)
    sig=SIGS[0]
    origin=old[sig]
    look="if (sockindex < MAX_SOCK_NUM) Ethernet.socketClose(sockindex);"
    need(origin.count(look)==1 and
         origin.count("sockindex = Ethernet.socketBegin(SnMR::UDP, port);")==1,
         "UDP_BEGIN_SOURCE_DRIFT")
    b[sig]=origin.replace(sig+"\n{",sig+"""
{
\tjwplc_w5500_candidate::BusScope bus(20);
\tif (!bus) return 0; // BUS_BUSY: preserve all previous UDP fields.
\t_jwplcDeferredSendStart=false;""",1).replace(
        look, """if (sockindex < MAX_SOCK_NUM &&
\t    jwplc_w5500_candidate::matchesLocked(_jwplcSocketToken)) {
\t\tEthernet.socketClose(sockindex);
\t\t(void)jwplc_w5500_candidate::invalidateSocketLocked(_jwplcSocketToken);
\t}""",1).replace(
        "if (sockindex >= MAX_SOCK_NUM) return 0;",
        """if (sockindex >= MAX_SOCK_NUM) return 0;
\t_jwplcSocketToken=jwplc_w5500_candidate::currentSocketTokenLocked(sockindex);
\tif (!bool(_jwplcSocketToken)) { sockindex=MAX_SOCK_NUM; return 0; }""",1)

    sig=SIGS[1]
    origin=old[sig]
    need(origin.count("if (sockindex < MAX_SOCK_NUM) Ethernet.socketClose(sockindex);")==1,
         "UDP_STOP_SOURCE_DRIFT")
    b[sig]=origin.replace(sig+"\n{",sig+"""
{
\tjwplc_w5500_candidate::BusScope bus(20);
\tif (!bus) return; // never discard socket on SPI_BUSY""",1).replace(
        "if (sockindex < MAX_SOCK_NUM) Ethernet.socketClose(sockindex);",
        """if (sockindex < MAX_SOCK_NUM &&
\t    jwplc_w5500_candidate::matchesLocked(_jwplcSocketToken)) {
\t\tEthernet.socketClose(sockindex);
\t\t(void)jwplc_w5500_candidate::invalidateSocketLocked(_jwplcSocketToken);
\t}""",1).replace(
        "\t_sendPending = false;", "\t_jwplcDeferredSendStart = false;\n\t_sendPending = false;",1)

    sig=SIGS[2]
    origin=old[sig]
    need("Ethernet.socketStartUDP(sockindex, rawIPAddress(ip), port)" in origin,
         "UDP_BEGIN_PACKET_DRIFT")
    b[sig]=origin.replace(sig+"\n{",sig+"""
{
\tjwplc_w5500_candidate::BusScope bus(20);
\tif (!bus) return 0;
\tif (sockindex < MAX_SOCK_NUM &&
\t    !jwplc_w5500_candidate::matchesLocked(_jwplcSocketToken)) {
\t\tsockindex=MAX_SOCK_NUM;
\t\t_sendPending=false;
\t\t_jwplcDeferredSendStart=false;
\t\treturn 0;
\t}""",1)

    sig=SIGS[3]
    origin=old[sig]
    need("Ethernet.socketBeginSendUDP(sockindex)" in origin,
         "UDP_BEGIN_SEND_DRIFT")
    b[sig]=origin.replace(sig+"\n{",sig+"""
{
\tjwplc_w5500_candidate::BusScope bus(20);
\tif (!bus) {
\t\tif (sockindex >= MAX_SOCK_NUM) return -1;
\t\t_sendPending = true;
\t\t_jwplcDeferredSendStart = true;
\t\treturn 0;
\t}
\tif (sockindex < MAX_SOCK_NUM &&
\t    !jwplc_w5500_candidate::matchesLocked(_jwplcSocketToken)) {
\t\tsockindex=MAX_SOCK_NUM;
\t\t_sendPending=false;
\t\t_jwplcDeferredSendStart=false;
\t\treturn -1;
\t}""",1).replace(
        "\t_sendPending = (state == 0);",
        "\t_jwplcDeferredSendStart = false;\n\t_sendPending = (state == 0);",1)

    sig=SIGS[4]
    origin=old[sig]
    need("Ethernet.socketPollSendUDP(sockindex)" in origin,
         "UDP_POLL_SEND_DRIFT")
    b[sig]=origin.replace(sig+"\n{",sig+"""
{
\tjwplc_w5500_candidate::BusScope bus(20);
\tif (!bus) return 0; // SPI_BUSY, retain send state
\tif (sockindex < MAX_SOCK_NUM &&
\t    !jwplc_w5500_candidate::matchesLocked(_jwplcSocketToken)) {
\t\tsockindex=MAX_SOCK_NUM;
\t\t_sendPending=false;
\t\t_jwplcDeferredSendStart=false;
\t\treturn -1;
\t}
\tif (_jwplcDeferredSendStart) {
\t\tif (sockindex >= MAX_SOCK_NUM) return -1;
\t\tconst int startState=Ethernet.socketBeginSendUDP(sockindex);
\t\t_jwplcDeferredSendStart=false;
\t\t_sendPending=(startState==0);
\t\treturn startState;
\t}""",1)

    sig=SIGS[5]
    origin=old[sig]
    need(origin.count("_sendPending = false;")==1,
         "UDP_CANCEL_SOURCE_DRIFT")
    b[sig]=origin.replace("_sendPending = false;",
                          "_sendPending = false;\n\t_jwplcDeferredSendStart = false;",1)
    overlay=source
    for sig in SIGS:
        need(b[sig]!=old[sig] and overlay.count(old[sig])==1,
             "PATCH_NOT_UNIQUE_"+sig)
        overlay=overlay.replace(old[sig],b[sig],1)
    return overlay,b

def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--ci-standalone",action="store_true")
    args=ap.parse_args()
    rid="g10_p1g_c2_s2c2b1_"+datetime.now().strftime("%Y%m%d_%H%M%S")+"_"+uuid.uuid4().hex[:8]
    out=ROOT/"tools/alpha13/results"/rid
    out.mkdir(parents=True,exist_ok=False)
    man={"schema":"A13-G10-P1G-C2-S2C2B1-UDP-LIFECYCLE-1",
         "run_id":rid,"status":"REVIEW_STOP","product_changed":False,
         "arduino_built":False,"hardware_touched":False,
         "gpio4_enabled":False,"adopted":False}
    try:
        head=preflight(args.ci_standalone)
        repro=mod("tools/alpha13/gates/a13_g10_p1a_a13_006_repro.py",
                  "s2c2b1_extract")
        c1=mod("tools/alpha13/gates/a13_g10_p1g_c1_native_prototype.py",
               "s2c2b1_cxx")
        cpp=(ROOT/UDP_PATH).read_text(encoding="utf-8-sig")
        hdr=(ROOT/HEADER_PATH).read_text(encoding="utf-8-sig")
        modified,bodies=patches(repro,cpp)
        anchor="bool _sendPending = false; // JWPLC cooperative UDP SEND state"
        need(hdr.count(anchor)==1,"UDP_CLASS_HEADER_FIELD_DRIFT")
        new_header=hdr.replace(anchor,anchor+
          "\n\tbool _jwplcDeferredSendStart = false;"
          "\n\tjwplc_w5500_candidate::SocketToken _jwplcSocketToken{};",1)
        new_header='#include "JWPLC_W5500_SafeAccess.h"\n'+new_header
        mod_cpp='#include "JWPLC_W5500_SafeAccess.h"\n'+modified
        for path,txt in ((UDP_PATH,mod_cpp),(HEADER_PATH,new_header)):
            dest=out/"overlay"/path
            dest.parent.mkdir(parents=True,exist_ok=True)
            dest.write_text(txt,encoding="utf-8")
        old_gate=mod("tools/alpha13/gates/a13_g10_p1g_c2_s2b2a_cooperative_stop.py",
                     "s2c2b1_cxx_builder")
        old_gate.TEST=TEST
        def build(label,items):
            code="\n\n".join(items[sig] for sig in SIGS)+"\n"
            return old_gate.native(out,label,code,c1)
        rc,log=build("positive",bodies)
        if rc or not all(x in log for x in EXPECTED):
            say("POSITIVE_DIAGNOSTIC","EXIT_"+str(rc)+"_"+log.strip().replace("\n"," | ")[:1100])
        need(rc==0 and all(x in log for x in EXPECTED),
             "UDP_SOURCE_POSITIVE_NATIVE_FAILED")
        say("POSITIVE_NATIVE","PASS_SIX_REAL_UDP_METHODS")
        for label,sig,old,new,expected in NEGATIVES:
            need(bodies[sig].count(old)==1,
                 "NEGATIVE_ANCHOR_NOT_UNIQUE_"+label)
            mutant=dict(bodies);mutant[sig]=bodies[sig].replace(old,new,1)
            rc,log=build("negative_"+label,mutant)
            if rc==0 or expected not in log:
                say("NEGATIVE_DIAGNOSTIC_"+label,
                    "EXIT_"+str(rc)+"_"+log.strip().replace("\n"," | ")[:480])
            need(rc!=0 and expected in log,"NEGATIVE_NOT_REJECTED_"+label)
            say("NEGATIVE_"+label,"PASS_REJECTED")
        man.update({
          "head":head,
          "status":"UDP_REAL_CPP_LIFECYCLE_NATIVE_PASS_DNS_PENDING_ADOPTION_BLOCKED",
          "cpp_methods":len(SIGS),"overlay_files":2,
          "negative_count":len(NEGATIVES),
          "original_udp_sha256":sha(ROOT/UDP_PATH),
          "original_header_sha256":sha(ROOT/HEADER_PATH),
          "c1_safe_access_sha256":sha(C1_CPP),
          "test_sha256":sha(TEST),
          "known_gaps":[
              "Real DNS client bodies not compiled; DNS MOCK not included",
              "UDP write/read/parsePacket/fastRX/endPacket and multicast not protected",
              "Hardware socketBegin mocked here; source-backed gate S2A separate",
              "Cooperative UDP retry may require caller after perpetual SPI_BUSY",
              "No Arduino CLI/IDE build, RTOS scheduling or physical W5500 hardware",
          ]
        })
        say("HEAD",head)
        say("REAL_CPP_METHODS",len(SIGS))
        say("OVERLAY_FILES",2)
        say("NEGATIVES",len(NEGATIVES))
        say("FROZEN_ARCHIVES",5)
        say("PRODUCT_CHANGED","NO")
        say("STATUS",man["status"])
        say("RESULTS",out)
        return 0
    except Exception as e:
        man["reason"]=repr(e)
        say("STATUS","REVIEW_STOP")
        say("REASON",repr(e))
        say("RESULTS",out)
        return 2
    finally:
        (out/"MANIFEST.json").write_text(
            json.dumps(man,ensure_ascii=False,indent=2)+"\n",
            encoding="utf-8")
if __name__=="__main__":
    try:sys.exit(main())
    except Exception as e:
        say("FATAL",repr(e))
        sys.exit(2)
