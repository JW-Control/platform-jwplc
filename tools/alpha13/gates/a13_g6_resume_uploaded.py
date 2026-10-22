#!/usr/bin/env python3
"""A13-G6 reanudar desde un upload comprobado, SIN recompilar ni reflashear.

El operador ya efectuó:
- Arduino CLI 1.5.1 oficial, 3/3 compilaciones overlay, upload PASS.
- Diagnóstico TX: dos datagramas W5500 1472+576, concatenación exacta.
Esta reanudación comprueba todos los artefactos, realiza el resto del test
físico con el mismo firmware/token, luego adopta/regresa/cierra automáticamente
sólo si todas las comprobaciones pasan. Sin prompts AUTORIZO.
"""
from __future__ import annotations

from datetime import datetime
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile
import uuid

ROOT=Path(__file__).resolve().parents[3]
RUN_ID="20261011_150515_c7ab9c7e"
EXPECTED_APP="03ea7fbefab1269c3f6de6a51efa39e26d7c50ff3e8cdfd8b3f955a3dc490581"
EXPECTED_CLI="1017de89179c3167e6b8a38ca6cc4091fa69a3cf97aafa7810f01423003f5571"
CLI_VERSION="Version: 1.5.1"
ORIGINAL_HEAD="94204295060b90e0b784514edd67318aee109b13"
ORIGINAL=ROOT/"tools/alpha13/results"/("g6_integrated_"+RUN_ID)
SCRIPT=ROOT/"tools/alpha13/gates/a13_g6_integrated.py"
HOST=ROOT/"tools/alpha13/gates/a13_g6_udp_pc.py"
RESUME_TAG=datetime.now().strftime("%Y%m%d_%H%M%S")+"_"+uuid.uuid4().hex[:8]

def require(ok,message):
    if not ok:raise RuntimeError(message)

def sha(file):
    h=hashlib.sha256()
    with Path(file).open("rb") as f:
        for part in iter(lambda:f.read(1<<20),b""):h.update(part)
    return h.hexdigest()

def log(key,value):
    print(f"G6_RESUME_{key}={value}",flush=True)

def load_module(name,path):
    spec=importlib.util.spec_from_file_location(name,path)
    require(spec is not None and spec.loader is not None,"MODULE_NOT_LOADABLE_"+name)
    obj=importlib.util.module_from_spec(spec)
    sys.modules[name]=obj
    spec.loader.exec_module(obj)
    return obj

def fixture_cases(host):
    ip="192.168.0.31"
    expected=host.EXPECTED_TX
    addr=(ip,host.DUT_PORT)
    other=("192.168.0.32",host.DUT_PORT)
    packets=[
        ([(expected,addr)],True,"ONE_DATAGRAM"),
        ([(expected[:1472],addr),(expected[1472:],addr)],True,"MTU_SPLIT"),
        ([(expected[:1472],addr),(expected[1472:],other)],False,"WRONG_SOURCE_IP"),
        ([(expected[:1472],addr),(expected[1472:],(ip,9999))],False,"WRONG_SOURCE_PORT"),
        ([(expected[1472:],addr),(expected[:1472],addr)],False,"WRONG_ORDER"),
        ([(expected[:1472],addr)],False,"MISSING_FRAGMENT"),
        ([(expected[:1024],addr),(expected[1024:],addr)],False,"UNEXPECTED_SHAPE"),
        ([(expected[:1471]+b"x",addr),(expected[1472:],addr)],False,"CORRUPT_CONTENT"),
    ]
    for pair,should_pass,label in packets:
        _,src,shape,data=host.tx_contract(pair,ip)
        outcome=src and shape and data
        require(outcome is should_pass,"UDP_PC_FIXTURE_"+label)
        log("UDP_FIXTURE_"+label,"PASS")
    log("UDP_FIXTURES","8_OF_8_PASS")

def original_evidence(g):
    require(ORIGINAL.is_dir(),"ORIGINAL_RUN_DIR_NOT_FOUND")
    m=json.loads((ORIGINAL/"MANIFEST.json").read_text(encoding="utf-8-sig"))
    require(m.get("schema")=="A13-G6-INTEGRATED-1","SCHEMA_CHANGED")
    require(m.get("run_id")==RUN_ID and m.get("head")==ORIGINAL_HEAD,"ORIGINAL_RUN_ID_OR_HEAD")
    require(m.get("status")=="REVIEW_STOP" and m.get("reason")=="G6_PHYSICAL_PC_FAILED",
            "ORIGINAL_RUN_NOT_G6_NETWORK_REVIEW")
    require(m.get("physical_upload") is True and m.get("product_changed") is False,
            "ORIGINAL_PRODUCT_OR_UPLOAD_FLAGS")
    require(m.get("git_product_committed") is False and
            m.get("git_docs_committed") is False and m.get("git_pushed") is False,
            "ORIGINAL_GIT_ALREADY_CHANGED")
    require(m.get("port")=="COM4","ORIGINAL_PORT")
    require(m.get("app_sha256")==EXPECTED_APP,"ORIGINAL_APP_HASH_CHANGED")
    require(m.get("cli_sha256")==EXPECTED_CLI,"ORIGINAL_CLI_CHANGED")
    require(re.fullmatch(r"[0-9]{8}_[0-9]{6}_[0-9a-f]{8}_[0-9a-f]{32}",
                         m.get("token","")) is not None,"ORIGINAL_TOKEN_INVALID")
    expected_phases=("PREFLIGHT","SOURCE_OVERLAY","CANDIDATE_REGRESSIONS",
                     "PHYSICAL_BUILD","PHYSICAL_APPROVAL","PHYSICAL_UPLOAD")
    for phase in expected_phases:
        require(m.get("phase",{}).get(phase)=="PASS",
                "PREVIOUS_PHASE_NOT_PASS_"+phase)
    require(set(m.get("overlay_regressions",[]))==
            {"ETH_STATIC","ETH_SPI","MODBUS_CLIENT"},"PREVIOUS_OVERLAY_REGRESSIONS")
    require(set(m.get("candidate_sha256",{}))==set(g.PRODUCT),
            "ORIGINAL_MANIFEST_KEYS_WRONG")
    for product,candidate in g.PRODUCT.items():
        require(m["candidate_sha256"].get(product)==sha(g.ROOT/candidate),
                "CANDIDATE_MODIFIED_AFTER_UPLOAD_"+product)
        require(m["source_before_sha256"].get(product)==sha(g.ROOT/product),
                "PRODUCT_BASELINE_CHANGED_"+product)
    logs=[
        "F112_PORTABILITY_SELFTEST","G6_CONTRACT_SELFTEST",
        "CLI_VERSION","CLI_CORES","COMPILE_OVERLAY_ETH_STATIC",
        "COMPILE_OVERLAY_ETH_SPI","COMPILE_OVERLAY_MODBUS_CLIENT",
        "COMPILE_G6_UDP_PHYSICAL","UPLOAD_G6",
    ]
    for name in logs:
        p=ORIGINAL/(name+".log")
        require(p.is_file() and "\nEXIT=0\n" in
                p.read_text(encoding="utf-8-sig")[:1500],
                "ORIGINAL_LOG_MISSING_OR_FAIL_"+name)
    old_pc=ORIGINAL/"G6_PHYSICAL_PC.log"
    require(old_pc.is_file() and "\nEXIT=3\n" in
            old_pc.read_text(encoding="utf-8-sig")[:1500],
            "ORIGINAL_NETWORK_REVIEW_MISMATCH")
    diag=ORIGINAL/"G6_TX_DIAGNOSTIC.json"
    require(diag.is_file(),"G6_DIAGNOSIS_JSON_MISSING")
    d=json.loads(diag.read_text(encoding="utf-8-sig"))
    result=d.get("comparison",{})
    require(d.get("run_id")==RUN_ID and d.get("token")==m.get("token"),
            "DIAGNOSIS_TOKEN_MISMATCH")
    require(result.get("source_ip_matches") is True and
            result.get("concatenated_matches_expected") is True and
            result.get("concatenated_bytes")==2048 and
            [x.get("bytes") for x in result.get("datagrams",[])]==[1472,576],
            "DIAGNOSIS_SPLIT_NOT_CONFIRMED")
    require(d.get("serial_done") is True,"DIAGNOSIS_SERIAL_NOT_COMPLETE")
    log("ORIGINAL_EVIDENCE","PASS_3_COMPILES_UPLOAD_AND_DIAGNOSTIC")
    return m

def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--arduino-cli",required=True)
    ap.add_argument("--serial-port",default="COM4")
    args=ap.parse_args()
    cli=Path(args.arduino_cli).expanduser()
    require(cli.is_absolute() and cli.is_file(),"OFFICIAL_CLI_PATH_REQUIRED")
    require("Arduino PLC IDE Tools" not in str(cli),"UNWANTED_PLC_IDE_TOOLS")
    require(sha(cli)==EXPECTED_CLI,"ARDUINO_CLI_1_5_1_BINARY_HASH_MISMATCH")
    require(args.serial_port.upper()=="COM4","RUN_SERIAL_PORT_MUST_REMAIN_COM4")

    g=load_module("a13_g6_integrated_for_resume",SCRIPT)
    host=load_module("a13_g6_udp_host_contract",HOST)
    fixture_cases(host)
    m=original_evidence(g)

    # A distinct result directory retains the failed run and reuses
    # only its verified evidence, NOT its build artifacts or old host.
    dest=g.ROOT/"tools/alpha13/results"/("g6_resume_"+RESUME_TAG)
    temp=Path(tempfile.gettempdir())/("jwplc_a13_g6_resume_"+RESUME_TAG)
    require(not dest.exists() and not temp.exists(),"RESUME_DESTINATION_ALREADY_EXISTS")
    dest.mkdir(parents=True)
    temp.mkdir(parents=True)
    g.RESULT=dest
    g.TEMP=temp
    g.BACKUP=temp/"backup"
    g.RUN_ID=RUN_ID
    g.ADOPTED=False
    g.COMMITTED=False
    g.MANIFEST.clear()
    g.MANIFEST.update(m)
    g.MANIFEST["status"]="RESUME_RUNNING"
    g.MANIFEST["resume_origin"]=str(ORIGINAL)
    g.MANIFEST["resume_origin_head"]=ORIGINAL_HEAD
    g.MANIFEST["resume_tx_evidence"]=str(ORIGINAL/"G6_TX_DIAGNOSTIC.json")
    g.MANIFEST["git_product_committed"]=False
    g.MANIFEST["git_docs_committed"]=False
    g.MANIFEST["git_pushed"]=False
    g.MANIFEST["product_changed"]=False
    initial_head=None
    try:
        g.preflight()
        initial_head=g.MANIFEST["head"]
        g.run([sys.executable,"-B",
               g.ROOT/"tools/alpha13/gates/a13_finalizer_portability_selftest.py"],
              "F112_PORTABILITY_SELFTEST")
        g.run([sys.executable,"-B",
               g.ROOT/"tools/alpha13/gates/a13_g6_contract_selftest.py"],
              "G6_CONTRACT_SELFTEST")
        version=g.run([cli,"version"],"CLI_VERSION")
        require(CLI_VERSION in version,"CLI_NOT_1_5_1")
        cores=g.run([cli,"core","list"],"CLI_CORES")
        require(re.search(r"(?m)^\s*jwplc_local:esp32\s",cores),
                "CLI_LOCAL_JWPLC_CORE_MISSING")
        port=g.serial_port(args.serial_port)
        require(port=="COM4","SERIAL_PORT_CHANGED")
        g.MANIFEST["port"]=port
        g.MANIFEST["cli_path"]=str(cli.resolve())
        g.MANIFEST["cli_sha256"]=sha(cli)
        g.phase("RESUME_PREFLIGHT","PASS")
        g.phase("PHYSICAL_NETWORK","RUNNING")
        output=g.run([sys.executable,"-B",HOST,
                      "--serial-port",port,"--token",m["token"]],
                     "G6_PHYSICAL_PC")
        require("G6_PC_PHYSICAL=PASS" in output,"FULL_UDP_PC_PASS_NOT_PRESENT")
        require("G6_PC_TX_DATAGRAM_SIZES=1472,576" in output or
                "G6_PC_TX_DATAGRAM_SIZES=2048" in output,
                "FULL_UDP_TX_BOUNDARY_NOT_CHECKED")
        g.MANIFEST["physical"]="PASS_UDP_2048_TX_256_PARTIAL_16_RECOVERED_REOPEN"
        g.phase("PHYSICAL_NETWORK","PASS")
        g.adopt()
        g.phase("PRODUCT_REGRESSIONS","RUNNING")
        g.regressions(str(cli),g.ETH,"PRODUCT")
        g.phase("PRODUCT_REGRESSIONS","PASS")
        g.final()
        g.auto_close() # uses original G6 gate's exact-path, non-force policy
        g.MANIFEST["status"]="CLOSED_PASS"
        g.emit("STATUS","CLOSED_PASS")
        g.emit("REUSED_FIRMWARE_APP_SHA256",EXPECTED_APP)
        g.emit("G6_RESUME_NO_NEW_UPLOAD","YES")
        g.persist()
        return 0
    except (Exception,KeyboardInterrupt) as exc:
        g.MANIFEST["reason"]="RESUME_"+str(exc)
        g.MANIFEST["category"]="HARNESS_OR_PRODUCT_REVIEW"
        # Never restore after a commit, even if exception was raised
        # between a Git commit and the g.COMMITTED flag update.
        head_now=None
        try:head_now=g.git("rev-parse","HEAD")
        except Exception:pass
        if initial_head is not None and head_now==initial_head:
            g.restore()
        elif g.ADOPTED:
            g.emit("ROLLBACK","SKIPPED_GIT_HEAD_ADVANCED")
        g.MANIFEST["status"]="REVIEW_STOP"
        g.emit("STATUS","REVIEW_STOP")
        g.emit("REASON",str(exc))
        g.emit("NO_RESET_CHECKOUT_CLEAN_OR_BLIND_RETRY","YES")
        g.persist()
        return 2

if __name__=="__main__":
    try:
        sys.exit(main())
    except Exception as exc:
        log("STATUS","REVIEW_STOP")
        log("REASON",repr(exc))
        sys.exit(2)
