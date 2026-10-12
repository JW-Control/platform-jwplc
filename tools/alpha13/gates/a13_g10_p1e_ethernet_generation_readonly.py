#!/usr/bin/env python3
"""G10-P1E: inventario readonly generacion W5500, sin modificar producto.

Distingue reset fisico, W5100.init guardado, link-off y _ready.
P1E PASS significa procedencia y riesgos capturados, NO correccion A13-006.
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
P1D_BASE="efd168d41a49ca0585d7b0886b29624e39f36249"
P1C_RUN="g10_p1c_20261012_140152_a7211c08"
P1D_RUN="g10_p1d_20261012_141926_035d46be"
SRC={
    "RUNTIME":"JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/JWPLC_Ethernet.cpp",
    "RUNTIME_HEADER":"JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/JWPLC_Ethernet.h",
    "BACKEND":"JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/JWPLC_W5x00_Ethernet.cpp",
    "W5100":"JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/utility/w5100.cpp",
    "ETH_CLIENT":"JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/EthernetClient.cpp",
    "TCP_CLIENT":"JWPLC/2.1.0/libraries/JWPLC_ModbusTCP/src/JWPLC_ModbusTCP_Client.cpp",
    "TCP_SERVER":"JWPLC/2.1.0/libraries/JWPLC_ModbusTCP/src/JWPLC_ModbusTCP.cpp",
    "ETH_PROPERTIES":"JWPLC/2.1.0/libraries/JWPLC_Ethernet/library.properties",
    "TCP_PROPERTIES":"JWPLC/2.1.0/libraries/JWPLC_ModbusTCP/library.properties",
}
FROZEN={
 "JWPLC/2.1.0/libraries/JW_SD/src/esp32/libJW_SD.a":
 "58e86fdf31c9bf94204fcbcf7462656fd9f4d9a7d73bff35bd660dbff538b89e",
 "JWPLC/2.1.0/precompiled/core/JWPLCBASIC/core.a":
 "a1a985f64c22838a1987d4280b8c1dc431a42d5789c6692cbfa36dfb6252246d",
 "JWPLC/2.1.0/libraries/JWPLC_TFT/src/esp32/libJWPLC_TFT.a":
 "4c606ba66e29c1337d42450a29fd09fe76c6988f72aa51d3882b81db59f82ecc",
 "JWPLC/2.1.0/libraries/JW_FRAM/src/esp32/libJW_FRAM.a":
 "df390f3379da13c6d4503a9fb83bc098f2944590fe3ad083348e95bfdcaf7299",
 "JWPLC/2.1.0/libraries/JWPLC_ModbusRTU/src/esp32/libJWPLC_ModbusRTU.a":
 "f239100fbb6770a3a6f336179fbfa3a4afd27cc16e12c6f239cf05f28f7a1416",
}

def say(k,v):print("G10_P1E_"+str(k)+"="+str(v),flush=True)
def need(cond,why):
    if not cond: raise RuntimeError(why)
def sha(p):
    h=hashlib.sha256()
    with Path(p).open("rb") as f:
        for chunk in iter(lambda:f.read(1024*1024),b""):h.update(chunk)
    return h.hexdigest()
def git(*args):
    p=subprocess.run(["git","-c","core.quotepath=false",*args],cwd=ROOT,
                     capture_output=True)
    need(p.returncode==0,"GIT_CALL_FAILED_"+repr(args))
    return p.stdout.decode("utf-8","replace").strip()
def extract(source,signature):
    start=source.find(signature)
    need(start>=0 and source.find(signature,start+len(signature))<0,
         "MISSING_OR_DUPLICATE_FUNCTION_"+signature)
    op=source.find("{",start+len(signature))
    need(op!=-1,"FUNCTION_OPEN_MISSING_"+signature)
    depth=0
    for i in range(op,len(source)):
        if source[i]=="{":depth+=1
        if source[i]=="}":
            depth-=1
            if depth==0:return source[start:i+1]
    raise RuntimeError("UNBALANCED_FUNCTION_"+signature)
def patterns(source):
    rt=source["RUNTIME"]; low=source["W5100"]
    probe=extract(rt,"bool JWPLC_EthernetClass::probeHardware()")
    reset=extract(rt,"void JWPLC_EthernetClass::resetHardwareIfNeeded()")
    link=extract(rt,"void JWPLC_EthernetClass::service()")
    ready=extract(rt,"bool JWPLC_EthernetClass::isReady() const")
    init=extract(low,"uint8_t W5100Class::init(void)")
    eth_begin=source["BACKEND"]
    client=source["TCP_CLIENT"];server=source["TCP_SERVER"]
    flags={
        "probe_calls_physical_reset":"resetHardwareIfNeeded();" in probe,
        "physical_reset_optional_pin255":"if (_resetPin == 255)" in reset,
        "physical_reset_pulses_low_high":
            "digitalWrite(_resetPin, LOW);" in reset and
            "digitalWrite(_resetPin, HIGH);" in reset,
        "ready_is_plain_boolean":"return _ready;" in ready,
        "link_off_can_clear_ready":
            "link == LinkOFF" in link and "_ready = false;" in link,
        "backend_init_static_guard":
            "static bool initialized = false;" in init and
            "if (initialized) return 1;" in init,
        "backend_begin_calls_w5100_init":
            eth_begin.count("W5100.init()")==3,
        "client_drop_handle_if_not_ready":
            "_tcpClient = EthernetClient();" in
            extract(client,"void JWPLC_ModbusTCPClientClass::task()"),
        "server_drop_handle_if_not_ready":
            "_client = EthernetClient();" in
            extract(server,"void JWPLC_ModbusTCPClass::task()"),
        "runtime_generation_not_exposed":
            "generation" not in source["RUNTIME_HEADER"].lower() and
            "generation" not in source["RUNTIME"].lower(),
    }
    need(all(flags.values()),"SOURCE_PATTERN_DRIFT_REVIEW_NOT_FIX")
    return flags
def selftest():
    src={key:(ROOT/path).read_text(encoding="utf-8-sig") for key,path in SRC.items()}
    flags=patterns(src)
    need(len(flags)==10,"EXPECTED_TEN_PATTERNS")
    # Negative de contrato: sin reset pin pulsado, la foto ya no corresponde.
    test=dict(src)
    test["RUNTIME"]=test["RUNTIME"].replace("digitalWrite(_resetPin, LOW);",
                                           "/* missing */",1)
    try:
        patterns(test)
    except RuntimeError as error:
        need("SOURCE_PATTERN_DRIFT_REVIEW_NOT_FIX" in str(error),
             "NEGATIVE_WRONG_REJECTION")
    else:raise RuntimeError("NEGATIVE_RESET_REMOVAL_ACCEPTED")
    # No normalizar a backslash las keys: nombres Git siempre POSIX.
    need(all("\\" not in x and "/" in x for x in SRC.values()),
         "PATH_KEYS_NOT_PORTABLE")
    say("SELFTEST","PASS_READONLY_TEN_PATTERNS_AND_NEGATIVE")
    return 0
def manifest(run_id,schema,status):
    p=ROOT/"tools/alpha13/results"/run_id/"MANIFEST.json"
    need(p.is_file(),"PREVIOUS_LOCAL_MANIFEST_MISSING_"+run_id)
    doc=json.loads(p.read_text(encoding="utf-8-sig"))
    need(doc.get("run_id")==run_id and doc.get("schema")==schema and
         doc.get("status")==status,"LOCAL_MANIFEST_NOT_QUALIFIED_"+run_id)
    return doc
def main():
    args=argparse.ArgumentParser(description=__doc__)
    args.add_argument("--selftest",action="store_true")
    args.add_argument("--ci-standalone",action="store_true",
                      help="solo CI: no exige resultados locales del operador")
    opt=args.parse_args()
    if opt.selftest:return selftest()
    runid="g10_p1e_"+datetime.now().strftime("%Y%m%d_%H%M%S")+"_"+uuid.uuid4().hex[:8]
    out=ROOT/"tools/alpha13/results"/runid
    out.mkdir(parents=True,exist_ok=False)
    m={"schema":"A13-G10-P1E-READONLY-1","run_id":runid,"status":"REVIEW_STOP",
       "product_change":False,"hardware_access":False,
       "compile":False,"git_commit":False,"git_push":False}
    try:
        need(Path.cwd().resolve()==ROOT and
             Path(git("rev-parse","--show-toplevel")).resolve()==ROOT,
             "NOT_REPO_ROOT")
        need(git("branch","--show-current")==BRANCH,"WRONG_BRANCH")
        head=git("rev-parse","HEAD")
        need(head==git("rev-parse","refs/remotes/origin/"+BRANCH),
             "LOCAL_REMOTE_TRACKING_HEAD_DIFFERS")
        ancestor=subprocess.run(["git","merge-base","--is-ancestor",P1D_BASE,head],
                                cwd=ROOT,capture_output=True)
        need(ancestor.returncode==0,"P1D_COMMIT_NOT_ANCESTOR")
        need(not git("diff","--name-only") and
             not git("diff","--cached","--name-only"),"GIT_TRACKED_DIRTY")
        if not opt.ci_standalone:
            p1c=manifest(P1C_RUN,"A13-G10-P1C-ARDUINO-CANDIDATE-1",
                          "ARDUINO_BASELINE_AND_CANDIDATE_BUILDS_PASS_PRODUCT_UNCHANGED")
            p1d=manifest(P1D_RUN,"A13-G10-P1D-LIFECYCLE-1",
                          "SOURCE_CONTRACT_PASS_NATIVE_NOT_RUN_ADOPTION_BLOCKED")
            need(p1c.get("phases",{}).get("ARDUINO_BUILDS")=="PASS" and
                 p1c.get("phases",{}).get("FINAL_AUDIT")=="PASS",
                 "P1C_NOT_FOUR_BUILDS_PASS")
            need(not p1d.get("candidate_adoption") and
                 not p1d.get("product_modified"),"P1D_ADOPTION_DISALLOWED")
        reads={k:(ROOT/p).read_text(encoding="utf-8-sig") for k,p in SRC.items()}
        m["sources"]={p:{"sha256":sha(ROOT/p),"bytes":(ROOT/p).stat().st_size}
                       for p in SRC.values()}
        m["frozen_archives"]={}
        for path,want in FROZEN.items():
            need((ROOT/path).is_file() and sha(ROOT/path)==want,
                 "FROZEN_ARCHIVE_MISMATCH_"+path)
            m["frozen_archives"][path]=want
        found=patterns(reads)
        m["observations"]=found
        m["head"]=head
        m["provenance"]=("CI_SOURCE_HISTORY" if opt.ci_standalone
                         else "WINDOWS_P1C_P1D_MANIFEST_CHAIN")
        m["decision"]="A13-006_FIX_REVISION_REQUIRED_RESET_GENERATION"
        m["status"]="RESET_GENERATION_BASELINE_CAPTURED_ADOPTION_BLOCKED"
        say("HEAD",head)
        for key,value in found.items():
            say(key.upper(),value)
        say("ARCHIVES_FROZEN",len(FROZEN))
        say("STATUS",m["status"])
        say("RESULTS",out)
        return 0
    except Exception as exc:
        m["reason"]=repr(exc)
        say("STATUS","REVIEW_STOP")
        say("REASON",repr(exc))
        say("RESULTS",out)
        return 2
    finally:
        (out/"MANIFEST.json").write_text(json.dumps(m,indent=2,ensure_ascii=False)
                                         +"\n",encoding="utf-8")
if __name__=="__main__":
    try:sys.exit(main())
    except Exception as ex:
        say("FATAL",repr(ex));sys.exit(2)
