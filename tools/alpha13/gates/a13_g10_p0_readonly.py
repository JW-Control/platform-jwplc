#!/usr/bin/env python3
"""Alpha13 G10-P0: baseline read-only de riesgos Ethernet y verificacion package.

No modifica codigo, index Git, manifests de producto, CI, archives ni hardware.
Snapshot solo en tools/alpha13/results (ignorado por Git). No compila/flash.
"""
from __future__ import annotations
from datetime import datetime
import hashlib
import json
from pathlib import Path
import re
import subprocess
import sys
import uuid

ROOT=Path(__file__).resolve().parents[3]
BRANCH="v2.1.0-alpha.13/feature/cleanup-robustness"
G9_PRODUCT="63e0291a7b1480af846e955559ab175aa78e7300"
FILES={
"A13-006_CLIENT":"JWPLC/2.1.0/libraries/JWPLC_ModbusTCP/src/JWPLC_ModbusTCP_Client.cpp",
"A13-006_SERVER":"JWPLC/2.1.0/libraries/JWPLC_ModbusTCP/src/JWPLC_ModbusTCP.cpp",
"A13-007_DHCP":"JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/Dhcp.cpp",
"A13-008_UDP":"JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/EthernetUdp.cpp",
"A13-005_ETH":"JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/EthernetClient.cpp",
"A13-015_CI":".github/workflows/ci-jwplc-package-smoke.yml",
"A13-016_FREEZE":"tools/alpha12/gates/alpha12_precompiled_global_audit.ps1",
"A13-018_BENCH":"tools/build-speed-benchmark/Run-JWPLCBuildBenchmark.ps1",
"CORE_BOARD":"JWPLC/2.1.0/boards.txt",
"CORE_PLATFORM":"JWPLC/2.1.0/platform.txt",
"G5_CLOSE":"docs/v2.1.0-alpha.13/A13_G5_CLOSURE_20261011.md",
"G6_CLOSE":"docs/v2.1.0-alpha.13/A13_G6_CLOSURE_20261011.md",
"G7C_CLOSE":"docs/v2.1.0-alpha.13/A13_G7C_FRAM_CLOSURE_20261011.md",
"G9_PHYSICAL_CLOSE":"docs/v2.1.0-alpha.13/A13_G9_P5_PHYSICAL_CLOSURE_20261012.md",
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

def say(k,v):
    print("G10_P0_"+str(k)+"="+str(v),flush=True)

def need(ok,reason):
    if not ok:raise RuntimeError(reason)

def git(*args):
    cmd=["git","-c","core.quotepath=false",*args]
    p=subprocess.run(cmd,cwd=ROOT,capture_output=True)
    need(p.returncode==0,"GIT_FAILED_"+repr(args)+":"+
         p.stderr.decode("utf-8","replace")[-200:])
    return p.stdout.decode("utf-8","replace").strip()

def sha(path):
    h=hashlib.sha256()
    with Path(path).open("rb") as f:
        for b in iter(lambda:f.read(1024*1024),b""):h.update(b)
    return h.hexdigest()

def section(s,start,end):
    pos=s.find(start)
    need(pos!=-1,"CANONICAL_FUNCTION_MISSING_"+start)
    stop=s.find(end,pos+len(start))
    need(stop!=-1,"CANONICAL_FUNCTION_END_MISSING_"+end)
    return s[pos:stop]

def inspect(raw):
    client=section(raw["A13-006_CLIENT"],
         "void JWPLC_ModbusTCPClientClass::closeSession()",
         "void JWPLC_ModbusTCPClientClass::fail(")
    dhcp=section(raw["A13-007_DHCP"],
         "uint8_t DhcpClass::parseDHCPResponsePacket(",
         "return type;")
    udp=section(raw["A13-008_UDP"],
         "int EthernetUDP::endPacket()",
         "size_t EthernetUDP::write(")
    bench=section(raw["A13-018_BENCH"],"function Get-BinaryBytes","function ")
    flags={
       "A13-006_socket_drop_without_lock":
            ("_tcpClient = EthernetClient();" in client and
             "acquireBus(20)" in client),
       "A13-007_unchecked_fixed_dhcp_read":
            ("_dhcpUdpSocket.read((uint8_t*)&fixedMsg, sizeof(RIP_MSG_FIXED));"
             in dhcp),
       "A13-007_option_len_minus_four":
            ("opt_len - 4" in dhcp),
       "A13-008_udp_while_no_deadline":
            ("while (state == 0)" in udp and
             "millis()" not in udp and
             "deadline" not in udp.lower()),
       "A13-015_ci_mentions_ModbusTCP":
            "ModbusTCP" in raw["A13-015_CI"],
       "A13-015_ci_mentions_ModbusRTU_Master":
            ("ModbusRTU_Master" in raw["A13-015_CI"] or
             "RTU_Master" in raw["A13-015_CI"]),
       "A13-016_uses_git_epoch_freshness":
            ("Get-LatestSourceCommit" in raw["A13-016_FREEZE"] and
             "$sourceCommit" in raw["A13-016_FREEZE"]),
       "A13-018_binarybytes_sums_all_bin":
            ('-Filter "*.bin"' in bench and
             "Measure-Object -Property Length -Sum" in bench),
       "G9_closed_from_actual_physical_evidence":
            "G9_FINAL=CLOSED_PASS_WITH_EXPLICIT_COVERAGE_LIMITS" in
              raw["G9_PHYSICAL_CLOSE"],
    }
    need(flags["G9_closed_from_actual_physical_evidence"],
         "G9_FINAL_PHYSICAL_CLOSE_DOCUMENT_MISSING")
    need(all(flags[key] for key in [
        "A13-006_socket_drop_without_lock",
        "A13-007_unchecked_fixed_dhcp_read",
        "A13-007_option_len_minus_four",
        "A13-008_udp_while_no_deadline",
        "A13-016_uses_git_epoch_freshness",
        "A13-018_binarybytes_sums_all_bin",
    ]),"OPEN_ISSUE_SOURCE_PATTERN_CHANGED_REVIEW_MANUALLY")
    return flags

def test():
    example="void X() { bug(); } void Y() {}"
    need("bug();" in section(example,"void X()","void Y()"),
         "SELFTEST_SECTION")
    need(sha(Path(__file__))==sha(Path(__file__)),
         "SELFTEST_STABLE_DIGEST")
    say("SELFTEST","PASS_READ_ONLY")
    return 0

def main():
    if len(sys.argv)==2 and sys.argv[1]=="--selftest":return test()
    need(len(sys.argv)==1,"USE_NO_ARGS_OR_SELFTEST")
    runid="g10_p0_"+datetime.now().strftime("%Y%m%d_%H%M%S")+"_"+uuid.uuid4().hex[:8]
    output=ROOT/"tools/alpha13/results"/runid
    output.mkdir(parents=True,exist_ok=False)
    m={"schema":"A13-G10-P0-READONLY-1","run_id":runid,
       "status":"REVIEW_STOP","no_product_edit":True,
       "no_git_mutation":True,"no_hardware":True,
       "no_compile":True,"phase":"PREFLIGHT","sources":{}}
    try:
        need(Path(git("rev-parse","--show-toplevel")).resolve()==ROOT and
             Path.cwd().resolve()==ROOT,"NOT_REPOSITORY_ROOT")
        need(git("branch","--show-current")==BRANCH,"WRONG_BRANCH")
        head=git("rev-parse","HEAD")
        need(head==git("rev-parse","refs/remotes/origin/"+BRANCH),
             "LOCAL_ORIGIN_HEAD_NOT_EQUAL")
        need(subprocess.run(["git","merge-base","--is-ancestor",G9_PRODUCT,head],
                            cwd=ROOT,capture_output=True).returncode==0,
             "G9_PRODUCT_COMMIT_NOT_IN_HISTORY")
        need(not git("diff","--name-only") and
             not git("diff","--cached","--name-only"),"GIT_NOT_CLEAN")
        raw={}
        for key,rel in FILES.items():
            f=ROOT/rel
            need(f.is_file(),"SOURCE_MISSING_"+rel)
            data=f.read_bytes()
            raw[key]=data.decode("utf-8","replace")
            m["sources"][rel]={"sha256":hashlib.sha256(data).hexdigest(),
                               "bytes":len(data)}
        for rel,expected in FROZEN.items():
            f=ROOT/rel
            need(f.is_file() and sha(f)==expected,
                 "FROZEN_ARCHIVE_DIFFERENT_"+rel)
            m["sources"][rel]={"sha256":sha(f),"bytes":f.stat().st_size,
                               "expected_sha256":expected}
        m["head"]=head
        m["remote_head"]=head
        m["open_patterns"]=inspect(raw)
        m["canonical_review_decisions"]={
            "A13-006":"REVIEW_OPEN",
            "A13-007":"REVIEW_OPEN",
            "A13-008":"REVIEW_OPEN",
            "A13-015":"ADOPT_PENDING",
            "A13-016":"ADOPT_PENDING",
            "A13-018":"ADOPT_PENDING",
            "F120":"UNKNOWN_PENDING"}
        m["status"]="BASELINE_CAPTURED_REVIEW_FINDINGS_OPEN"
        m["phase"]="FINAL_READONLY_AUDIT_PASS"
        say("HEAD",head)
        say("A13_006_SOCKET_DROP_PATTERN",m["open_patterns"]["A13-006_socket_drop_without_lock"])
        say("A13_007_DHCP_PARTIAL_PATTERN",m["open_patterns"]["A13-007_unchecked_fixed_dhcp_read"])
        say("A13_008_UDP_UNBOUNDED_WAIT_PATTERN",m["open_patterns"]["A13-008_udp_while_no_deadline"])
        say("A13_015_TCP_SMOKE_PRESENT",m["open_patterns"]["A13-015_ci_mentions_ModbusTCP"])
        say("A13_015_RTU_MASTER_SMOKE_PRESENT",m["open_patterns"]["A13-015_ci_mentions_ModbusRTU_Master"])
        say("A13_016_GIT_EPOCH_POLICY_PRESENT",m["open_patterns"]["A13-016_uses_git_epoch_freshness"])
        say("A13_018_BIN_SUM_PRESENT",m["open_patterns"]["A13-018_binarybytes_sums_all_bin"])
        say("FROZEN_ARCHIVES",len(FROZEN))
        say("STATUS",m["status"])
        say("RESULTS",output)
        return 0
    except Exception as error:
        m["reason"]=repr(error)
        say("STATUS","REVIEW_STOP")
        say("REASON",repr(error))
        say("RESULTS",output)
        return 2
    finally:
        (output/"MANIFEST.json").write_text(
            json.dumps(m,ensure_ascii=False,indent=2)+"\n",encoding="utf-8")
        (output/"SUMMARY.log").write_text(
            "\n".join(["G10_P0_STATUS="+m["status"],
                       "G10_P0_NO_PRODUCT_EDIT=YES",
                       "G10_P0_NO_GIT_MUTATION=YES",
                       "G10_P0_NO_HARDWARE=YES"])+"\n",encoding="utf-8")

if __name__=="__main__":
    try:sys.exit(main())
    except Exception as error:
        say("FATAL",repr(error))
        sys.exit(2)
