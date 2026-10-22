#!/usr/bin/env python3
"""A13 G10-P1F: snapshot read-only de reset W5500 y ownership de sockets.

Un PASS demuestra patrones/procedencia y no demuestra que A13-006 fue resuelto.
No compila, no escribe producto, no usa hardware, ni hace git commit/push.
"""
from __future__ import annotations
import argparse
from datetime import datetime
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import uuid

ROOT=Path(__file__).resolve().parents[3]
BRANCH="v2.1.0-alpha.13/feature/cleanup-robustness"
P1E_HEAD="37366d10858864e6cdee9e9467191b858a939ee3"
P1E_RUN="g10_p1e_20261012_142935_e37850cd"
FILES={
"RUNTIME":"JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/JWPLC_Ethernet.cpp",
"RUNTIME_H":"JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/JWPLC_Ethernet.h",
"W5100":"JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/utility/w5100.cpp",
"BACKEND":"JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/JWPLC_W5x00_Ethernet.cpp",
"SERVER":"JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/EthernetServer.cpp",
"ETH_CLIENT":"JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/EthernetClient.cpp",
"MB_CLIENT":"JWPLC/2.1.0/libraries/JWPLC_ModbusTCP/src/JWPLC_ModbusTCP_Client.cpp",
"MB_SERVER":"JWPLC/2.1.0/libraries/JWPLC_ModbusTCP/src/JWPLC_ModbusTCP.cpp",
"ETH_PROP":"JWPLC/2.1.0/libraries/JWPLC_Ethernet/library.properties",
"MB_PROP":"JWPLC/2.1.0/libraries/JWPLC_ModbusTCP/library.properties",
}
FROZEN={
"JWPLC/2.1.0/libraries/JW_SD/src/esp32/libJW_SD.a":"58e86fdf31c9bf94204fcbcf7462656fd9f4d9a7d73bff35bd660dbff538b89e",
"JWPLC/2.1.0/precompiled/core/JWPLCBASIC/core.a":"a1a985f64c22838a1987d4280b8c1dc431a42d5789c6692cbfa36dfb6252246d",
"JWPLC/2.1.0/libraries/JWPLC_TFT/src/esp32/libJWPLC_TFT.a":"4c606ba66e29c1337d42450a29fd09fe76c6988f72aa51d3882b81db59f82ecc",
"JWPLC/2.1.0/libraries/JW_FRAM/src/esp32/libJW_FRAM.a":"df390f3379da13c6d4503a9fb83bc098f2944590fe3ad083348e95bfdcaf7299",
"JWPLC/2.1.0/libraries/JWPLC_ModbusRTU/src/esp32/libJWPLC_ModbusRTU.a":"f239100fbb6770a3a6f336179fbfa3a4afd27cc16e12c6f239cf05f28f7a1416",
}
def say(k,v):print("G10_P1F_"+str(k)+"="+str(v),flush=True)
def need(condition,message):
    if not condition:raise RuntimeError(message)
def sha(path):
    d=hashlib.sha256()
    with Path(path).open("rb") as f:
        for data in iter(lambda:f.read(1024*1024),b""):d.update(data)
    return d.hexdigest()
def git(*args):
    p=subprocess.run(["git","-c","core.quotepath=false",*args],cwd=ROOT,capture_output=True)
    need(p.returncode==0,"GIT_FAIL_"+repr(args))
    return p.stdout.decode("utf-8","replace").strip()
def method(src,beg,end):
    i=src.find(beg)
    need(i>=0 and src.find(beg,i+1)==-1,"METHOD_NOT_UNIQUE_"+beg)
    j=src.find(end,i+len(beg))
    need(j>i,"METHOD_END_NOT_FOUND_"+end)
    return src[i:j]
def inspect(s):
    probe=method(s["RUNTIME"],"bool JWPLC_EthernetClass::probeHardware()",
                 "void JWPLC_EthernetClass::service()")
    reset=method(s["RUNTIME"],"void JWPLC_EthernetClass::resetHardwareIfNeeded()",
                 "bool JWPLC_EthernetClass::prepareSPI()")
    ready=method(s["RUNTIME"],"bool JWPLC_EthernetClass::isReady() const",
                 "bool JWPLC_EthernetClass::isBusy() const")
    # Method names in the runtime vary; use only bounded probe/reset functions
    server=s["SERVER"];mbs=s["MB_SERVER"];mbc=s["MB_CLIENT"]
    flags={
      "reset_occurs_before_spi_lock":
        probe.index("resetHardwareIfNeeded();") <
        probe.index("if (!acquireBus(500))"),
      "reset_is_gpio_and_optional":
        "if (_resetPin == 255)" in reset and
        "digitalWrite(_resetPin, LOW);" in reset and
        "digitalWrite(_resetPin, HIGH);" in reset,
      "ready_is_boolean_not_epoch":"return _ready;" in ready,
      "init_uses_static_initialized_guard":
        "static bool initialized = false;" in s["W5100"] and
        "if (initialized) return 1;" in s["W5100"],
      "physical_probe_still_enters_backend":
        "Ethernet.begin(" in probe and "W5100.init()" in s["BACKEND"],
      "listener_server_port_is_static":
        "EthernetServer::server_port[MAX_SOCK_NUM]" in server,
      "listener_records_socket_index":
        "server_port[sockindex] = _port;" in server,
      "listener_queries_legacy_mapping":
        "server_port[i] == _port" in server and
        "EthernetServer::operator bool()" in server,
      "modbus_server_rebuilds_listener_on_ready":
        "ensureServerListening();" in mbs and
        "JWPLC_Ethernet.isReady()" in mbs,
      "tcp_client_still_drops_handle_on_not_ready":
        "_tcpClient = EthernetClient();" in
        method(mbc,"void JWPLC_ModbusTCPClientClass::task()",
                   "void JWPLC_ModbusTCPClientClass::poll()"),
      "tcp_server_still_drops_handle_on_not_ready":
        "_client = EthernetClient();" in
        method(mbs,"void JWPLC_ModbusTCPClass::task()",
                   "void JWPLC_ModbusTCPClass::poll()"),
      "no_generation_exposed_in_runtime":
        "generation" not in s["RUNTIME_H"].lower() and
        "generation" not in s["RUNTIME"].lower(),
    }
    need(all(flags.values()),"SOURCE_DRIFT_MANUAL_REVIEW_REQUIRED")
    return flags
def test():
    src={k:(ROOT/v).read_text(encoding="utf-8-sig") for k,v in FILES.items()}
    flags=inspect(src)
    need(len(flags)==12,"EXPECTED_TWELVE_CHECKS")
    # Falsa creencia: begin/reset antes del lock es correcto. Exigir detección.
    bad=dict(src)
    bad["RUNTIME"]=src["RUNTIME"].replace(
         "    resetHardwareIfNeeded();\n\n    if (!acquireBus(500))",
         "    if (!acquireBus(500))\n\n    resetHardwareIfNeeded();",1)
    try:inspect(bad)
    except RuntimeError:pass
    else:raise RuntimeError("NEGATIVE_RESET_ORDER_ACCEPTED")
    bad2=dict(src)
    bad2["SERVER"]=src["SERVER"].replace(
         "server_port[sockindex] = _port;", "/* deleted entry */",1)
    try:inspect(bad2)
    except RuntimeError:pass
    else:raise RuntimeError("NEGATIVE_LISTENER_INDEX_ACCEPTED")
    say("SELFTEST","PASS_TWELVE_PATTERNS_TWO_NEGATIVES")
    return 0
def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--selftest",action="store_true")
    ap.add_argument("--ci-standalone",action="store_true")
    opt=ap.parse_args()
    if opt.selftest:return test()
    runid="g10_p1f_"+datetime.now().strftime("%Y%m%d_%H%M%S")+"_"+uuid.uuid4().hex[:8]
    out=ROOT/"tools/alpha13/results"/runid
    out.mkdir(parents=True,exist_ok=False)
    m={"schema":"A13-G10-P1F-READONLY-1","run_id":runid,
       "status":"REVIEW_STOP","product_mutation":False,
       "git_mutation":False,"hardware_access":False,"compile":False}
    try:
        need(Path.cwd().resolve()==ROOT and
             Path(git("rev-parse","--show-toplevel")).resolve()==ROOT,
             "REPOSITORY_ROOT_REQUIRED")
        need(git("branch","--show-current")==BRANCH,"WRONG_BRANCH")
        head=git("rev-parse","HEAD")
        need(head==git("rev-parse","refs/remotes/origin/"+BRANCH),
             "REMOTE_TRACKING_HEAD_DIFFERENT")
        need(subprocess.run(["git","merge-base","--is-ancestor",P1E_HEAD,head],
             cwd=ROOT,capture_output=True).returncode==0,
             "P1E_HEAD_NOT_ANCESTOR")
        need(not git("diff","--name-only") and
             not git("diff","--cached","--name-only"),"GIT_TRACKED_DIRTY")
        if not opt.ci_standalone:
            old=ROOT/"tools/alpha13/results"/P1E_RUN/"MANIFEST.json"
            need(old.is_file(),"P1E_LOCAL_EVIDENCE_MISSING")
            previous=json.loads(old.read_text(encoding="utf-8-sig"))
            need(previous.get("schema")=="A13-G10-P1E-READONLY-1" and
                 previous.get("run_id")==P1E_RUN and
                 previous.get("head")==P1E_HEAD and
                 previous.get("status")==
                 "RESET_GENERATION_BASELINE_CAPTURED_ADOPTION_BLOCKED",
                 "P1E_LOCAL_EVIDENCE_NOT_QUALIFIED")
            for key in ["RUNTIME","W5100","BACKEND","MB_CLIENT","MB_SERVER"]:
                rel=FILES[key]
                need(previous.get("sources",{}).get(rel,{}).get("sha256")==
                     sha(ROOT/rel),"P1E_SOURCE_CHANGED_"+rel)
        src={k:(ROOT/v).read_text(encoding="utf-8-sig") for k,v in FILES.items()}
        m["sources"]={v:{"sha256":sha(ROOT/v),"bytes":(ROOT/v).stat().st_size}
                      for v in FILES.values()}
        for path,want in FROZEN.items():
            need((ROOT/path).is_file() and sha(ROOT/path)==want,
                 "FROZEN_ARCHIVE_SHA_MISMATCH_"+path)
        flags=inspect(src)
        m["flags"]=flags
        m["head"]=head
        m["archives"]=len(FROZEN)
        m["design_decision"]="EPOCH_RESET_AND_LISTENER_CONTRACT_REQUIRED"
        m["status"]="SOCKET_EPOCH_BASELINE_CAPTURED_ADOPTION_BLOCKED"
        say("HEAD",head)
        for key,val in flags.items():say(key.upper(),val)
        say("ARCHIVES_FROZEN",len(FROZEN))
        say("STATUS",m["status"])
        say("RESULTS",out)
        return 0
    except Exception as error:
        m["reason"]=repr(error)
        say("STATUS","REVIEW_STOP")
        say("REASON",repr(error))
        say("RESULTS",out)
        return 2
    finally:
        (out/"MANIFEST.json").write_text(
            json.dumps(m,ensure_ascii=False,indent=2)+"\n",encoding="utf-8")
if __name__=="__main__":
    try:sys.exit(main())
    except Exception as error:
        say("FATAL",repr(error));sys.exit(2)
