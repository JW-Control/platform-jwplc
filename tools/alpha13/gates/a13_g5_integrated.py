#!/usr/bin/env python3
"""Alpha13-G5: source-first Ethernet TCP, red real, regresiones y rollback.

Nunca hace commit, push ni release. Adopta fuentes productivas SOLO cuando
la prueba física y la verificación de la librería temporal han pasado.
"""
from __future__ import annotations
import argparse
from datetime import datetime
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile
import uuid

ROOT=Path(__file__).resolve().parents[3]
BRANCH="v2.1.0-alpha.13/feature/cleanup-robustness"
DOCROOT="docs/v2.1.0-alpha.13"
LIBS=ROOT/"JWPLC/2.1.0/libraries"
ETH=LIBS/"JWPLC_Ethernet"
FQBN="jwplc_local:esp32:jwplcbasic"
SOURCE_MAP={
 "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/EthernetClient.cpp":
 "tools/alpha13/candidates/g5/EthernetClient.cpp",
 "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/EthernetServer.cpp":
 "tools/alpha13/candidates/g5/EthernetServer.cpp",
}
CORE=ROOT/"JWPLC/2.1.0/precompiled/core/JWPLCBASIC/core.a"
CORE_SHA="a1a985f64c22838a1987d4280b8c1dc431a42d5789c6692cbfa36dfb6252246d"
TFT=LIBS/"JWPLC_TFT/src/esp32/libJWPLC_TFT.a"
TFT_SHA="4c606ba66e29c1337d42450a29fd09fe76c6988f72aa51d3882b81db59f82ecc"
RUN_ID=datetime.now().strftime("%Y%m%d_%H%M%S")+"_"+uuid.uuid4().hex[:8]
OUT=ROOT/"tools/alpha13/results"/("g5_integrated_"+RUN_ID)
TMP=Path(tempfile.gettempdir())/("jwplc_a13_g5_"+RUN_ID)
BACKUP=TMP/"product_backups"
STATE={"hito":"A13-G5","run_id":RUN_ID,"status":"REVIEW","phase":{},
       "product_changed":False,"commit_executed":False,
       "upload_executed":False,"release_published":False}
ADOPTED=False

class Stop(Exception):
    def __init__(self,msg,category="PRECONDITION"):
        super().__init__(msg)
        self.category=category

def require(condition,msg,category="PRECONDITION"):
    if not condition:raise Stop(msg,category)

def emit(key,val):print(f"{key}={val}",flush=True)
def sha(p):
    h=hashlib.sha256()
    with Path(p).open("rb") as f:
        for part in iter(lambda:f.read(1024*1024),b""):h.update(part)
    return h.hexdigest()
def git(*args):
    p=subprocess.run(["git",*args],cwd=ROOT,stdout=subprocess.PIPE,stderr=subprocess.PIPE)
    if p.returncode:raise Stop("GIT_ERROR:"+repr(args)+":"+p.stderr.decode(errors="replace")[-450:])
    return p.stdout.decode("utf-8","replace").strip()
def save():
    OUT.mkdir(parents=True,exist_ok=True)
    (OUT/"MANIFEST.json").write_text(json.dumps(STATE,indent=2,ensure_ascii=False)+"\n",encoding="utf-8")
    lines=["GATE=A13-G5","RUN_ID="+RUN_ID,"STATUS="+STATE["status"]]
    lines += [f"{k}={v}" for k,v in STATE["phase"].items()]
    for field in ("reason","category","port","token","app_sha256","physical","regressions","product_sha256","rollback"):
        if field in STATE:lines.append(f"{field.upper()}={STATE[field]}")
    lines+=["GIT_COMMIT=NO","PUSH=NO","RELEASE_MERGE=NO"]
    (OUT/"SUMMARY.log").write_text("\n".join(lines)+"\n",encoding="utf-8")
def phase(k,value):
    STATE["phase"][k]=value
    emit("PHASE",k+"_"+value)
    save()

def invoke(args,label,cwd=ROOT):
    args=[str(a) for a in args]
    p=subprocess.run(args,cwd=cwd,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,
                     errors="backslashreplace",encoding="utf-8")
    output=p.stdout or ""
    (OUT/(label+".log")).write_text("ARGS="+repr(args)+"\nEXIT="+str(p.returncode)+"\n"+output,encoding="utf-8")
    emit(label+"_EXIT",p.returncode)
    if p.returncode:
        errors=list(dict.fromkeys(line.strip() for line in output.splitlines()
                if re.search(r"(?:fatal error:|[ ]error:|undefined reference|REVIEW|FAIL|error C[0-9]+)",line)))
        for line in errors[-8:]:emit("DIAGNOSTIC",line[-350:])
        if not errors:emit("DIAGNOSTIC_TAIL","\\n".join(output.splitlines()[-9:])[-800:])
        raise Stop(label+"_EXIT_"+str(p.returncode),"PRODUCT_OR_ENVIRONMENT")
    return output

def cli_find(explicit):
    if explicit:
        require(Path(explicit).is_file() or shutil.which(explicit),
                "CLI_NOT_FOUND","ENVIRONMENT")
        return explicit
    p=shutil.which("arduino-cli")
    if p:return p
    fallback=Path(r"C:\Program Files\Arduino PLC IDE Tools\arduino-cli.exe")
    require(fallback.is_file(),"ARDUINO_CLI_NOT_FOUND","ENVIRONMENT")
    return str(fallback)

def port_check(requested):
    from serial.tools import list_ports
    ports=list(list_ports.comports())
    choices=[x.device for x in ports]
    emit("PORTS_VISIBLE",",".join(choices) or "NONE")
    if requested:
        found=[x for x in ports if x.device.upper()==requested.upper()]
        require(len(found)==1,"SERIAL_PORT_NOT_PRESENT","ENVIRONMENT")
        return found[0].device
    options=[x for x in ports if x.device.upper()!="COM1"]
    require(len(options)==1,"AMBIGUOUS_SERIAL_PORTS","ENVIRONMENT")
    return options[0].device

def contract():
    client=(ROOT/list(SOURCE_MAP.values())[0]).read_text(encoding="utf-8")
    server=(ROOT/list(SOURCE_MAP.values())[1]).read_text(encoding="utf-8")
    require(client.count("size_t EthernetClient::write(const uint8_t *buf, size_t size)")==1,"CLIENT_WRITE_SIGNATURE","HARNESS")
    require(client.count("int EthernetClient::read(uint8_t *buf, size_t size)")==1,"CLIENT_READ_SIGNATURE","HARNESS")
    require(server.count("size_t EthernetServer::write(const uint8_t *buffer, size_t size)")==1,"SERVER_WRITE_SIGNATURE","HARNESS")
    segment=client.split("size_t EthernetClient::write(const uint8_t *buf, size_t size)",1)[1].split("int EthernetClient::beginWriteAsync(",1)[0]
    require("totalWritten += sent;" in segment and "totalWritten != size" in segment and
            "remainingMs" in segment and "setWriteError()" in segment,"TCP_TX_CHUNK_CONTRACT","HARNESS")
    segment=client.split("int EthernetClient::read(uint8_t *buf, size_t size)",1)[1].split("int EthernetClient::jwplcReadTcpFastDeferred(",1)[0]
    require("INT16_MAX" in segment and "boundedSize" in segment,"TCP_RX_CLAMP_CONTRACT","HARNESS")
    require("return anyClient ? minimumWritten : 0;" in server,"TCP_SERVER_WRITE_CONTRACT","HARNESS")
    for src in (client,server):
        require(re.search(r"\b(?:bool bool|int int|size_t size_t|void void)\b",src) is None,
                "DUPLICATE_TYPE_REJECTED","HARNESS")
    # Prueba de límites de la aritmética de diseño, antes de compilar/upload:
    for requested in (0,1,2048,2049,5000,65535,65536,100000):
        chunks=[]
        left=requested
        while left:
            count=min(left,2048)
            chunks.append(count)
            left-=count
        require(sum(chunks)==requested and all(0<x<=2048 for x in chunks),
                "TX_ARITHMETIC_MODEL_FAILED","HARNESS")
    for requested in (0,1,32767,32768,65535,100000):
        result=min(requested,32767)
        require(0<=result<=32767,"RX_ARITHMETIC_MODEL_FAILED","HARNESS")
    STATE["contract"]="PASS"

def preflight():
    phase("PREFLIGHT","RUNNING")
    require(git("branch","--show-current")==BRANCH,"WRONG_BRANCH")
    require(not git("diff","--name-only") and not git("diff","--cached","--name-only"),
            "TRACKED_WORKTREE_DIRTY")
    require(not git("ls-files","--others","--exclude-standard"),"UNTRACKED_WORKTREE_DIRTY")
    head=git("rev-parse","HEAD")
    STATE["head"]=head
    for p,candidate in SOURCE_MAP.items():
        require((ROOT/p).is_file() and (ROOT/candidate).is_file(),"SOURCE_PATH_MISSING_"+p)
    require(sha(CORE)==CORE_SHA and sha(TFT)==TFT_SHA,"G3_G4_ARCHIVE_CHANGED")
    properties=(ETH/"library.properties").read_text(encoding="utf-8")
    require("precompiled=full" not in properties,"ETHERNET_UNEXPECTED_PRECOMPILED_CONTRACT")
    require((ROOT/"tools/alpha13/gates/a13_finalizer_portability_selftest.py").is_file(),
            "F112_PORTABILITY_SELFTEST_MISSING","HARNESS")
    contract()
    STATE["original_sha256"]={p:sha(ROOT/p) for p in SOURCE_MAP}
    STATE["candidate_sha256"]={p:sha(ROOT/c) for p,c in SOURCE_MAP.items()}
    phase("PREFLIGHT","PASS")

def overlay():
    phase("SOURCE_OVERLAY","RUNNING")
    dest=TMP/"source"/"JWPLC_Ethernet"
    dest.parent.mkdir(parents=True,exist_ok=True)
    shutil.copytree(ETH,dest)
    for original,candidate in SOURCE_MAP.items():
        sub=Path(original).relative_to("JWPLC/2.1.0/libraries/JWPLC_Ethernet")
        shutil.copy2(ROOT/candidate,dest/sub)
    phase("SOURCE_OVERLAY","PASS")
    return dest

def compile_case(cli,sketch,label,selected):
    build=TMP/("build_"+label)
    output=invoke([cli,"compile","--fqbn",FQBN,"-j","0","-v","--clean",
                  "--build-path",build,"--library",selected,"--libraries",LIBS,sketch],
                 "COMPILE_"+label)
    found=re.findall(r"^Using library JWPLC_Ethernet at version .+ in folder: (.+)$",output,re.M)
    require(len(found)==1 and Path(found[0].strip()).resolve()==selected.resolve(),
            "WRONG_SOURCE_LIBRARY_"+label,"HARNESS")
    require("Using core 'jwcontrol_precompiled_stub'" in output and
            re.search(r"[\\/]precompiled[\\/]core[\\/]JWPLCBASIC[\\/]core\.a",output),
            "NORMAL_CORE_ARCHIVE_NOT_LINKED","HARNESS")
    for name in ("EthernetClient.cpp.o","EthernetServer.cpp.o"):
        count=len(list(build.rglob(name)))
        require(count==1,"ETHERNET_SOURCE_NOT_COMPILED_"+label+"_"+name,"HARNESS")
    return build

def preview_build(cli,selected):
    phase("CANDIDATE_BUILD","RUNNING")
    fixtures=[
      ("ETH_STATIC",LIBS/"JWPLC_Ethernet/examples/02.Ethernet_StaticIP_Basic"),
      ("ETH_SPI",LIBS/"JWPLC_Ethernet/examples/Ethernet_SPI_Coexistence"),
      ("MODBUS_CLIENT",LIBS/"JWPLC_ModbusTCP/examples/02.ModbusTCP_Client"),
    ]
    # Los archivos reales del repositorio son la fuente; no inventar sketches.
    for label,path in fixtures:
        require(path.is_dir(),"REGRESSION_FIXTURE_MISSING_"+label,"HARNESS")
        compile_case(cli,path,"OVERLAY_"+label,selected)
    STATE["preview_regressions"]=[x for x,_ in fixtures]
    phase("CANDIDATE_BUILD","PASS")

def physical(cli,serial_port,selected):
    phase("PHYSICAL_BUILD","RUNNING")
    sketch=TMP/"G5_TCP_Probe"
    sketch.mkdir()
    source=ROOT/"tools/alpha13/firmware/a13_g5_tcp_correctness_probe/a13_g5_tcp_correctness_probe.ino"
    text=source.read_text(encoding="utf-8")
    require(text.count("__G5_TOKEN__")==1,"G5_TOKEN_ANCHOR","HARNESS")
    token=RUN_ID+"_"+uuid.uuid4().hex
    (sketch/"G5_TCP_Probe.ino").write_text(text.replace("__G5_TOKEN__",token),encoding="utf-8")
    build=compile_case(cli,sketch,"G5_PHYSICAL",selected)
    apps=list(build.rglob("*.ino.bin"))
    require(len(apps)==1,"G5_BIN_COUNT","HARNESS")
    STATE["app_sha256"]=sha(apps[0])
    STATE["token"]=token
    phase("PHYSICAL_BUILD","PASS")
    emit("APP_SHA256",STATE["app_sha256"])
    phase("PHYSICAL_APPROVAL","RUNNING")
    emit("CAUTION","Prueba requiere LAN W5500/PC, puerto TCP 5008 y equipo en banco SIN cargas.")
    confirmation=input("JWPLC aislado de actuadores y PC en misma red (BANCO_TCP): ").strip()
    require(confirmation=="BANCO_TCP","OPERATOR_NOT_AUTHORIZED","PRECONDITION")
    phase("PHYSICAL_APPROVAL","PASS")
    phase("PHYSICAL_UPLOAD","RUNNING")
    invoke([cli,"upload","--fqbn",FQBN,"--port",serial_port,"--input-dir",build],"UPLOAD_G5")
    STATE["upload_executed"]=True
    phase("PHYSICAL_UPLOAD","PASS")
    phase("PHYSICAL_NETWORK","RUNNING")
    pc=ROOT/"tools/alpha13/gates/a13_g5_tcp_pc.py"
    output=invoke([sys.executable,"-B",pc,"--port",serial_port,"--token",token,"--timeout","45"],
                  "PHYSICAL_G5_PC")
    require("G5_PC_PHYSICAL=PASS" in output,"G5_TCP_PC_CONTRACT","HARNESS")
    STATE["physical"]="PASS_5000_TX_128_RX_64_SERVER"
    phase("PHYSICAL_NETWORK","PASS")

def adopt():
    global ADOPTED
    phase("PRODUCT_ADOPTION","RUNNING")
    BACKUP.mkdir(parents=True,exist_ok=False)
    for p in SOURCE_MAP:
        b=BACKUP/p
        b.parent.mkdir(parents=True,exist_ok=True)
        shutil.copy2(ROOT/p,b)
    ADOPTED=True
    for p,candidate in SOURCE_MAP.items():
        shutil.copy2(ROOT/candidate,ROOT/p)
    require(set(git("diff","--name-only").splitlines())==set(SOURCE_MAP),
            "ADOPTION_SCOPE_INVALID","HARNESS")
    require(not git("diff","--check"),"ADOPTION_DIFF_CHECK","HARNESS")
    STATE["product_changed"]=True
    phase("PRODUCT_ADOPTION","PASS")

def regress(cli):
    phase("PRODUCT_REGRESSIONS","RUNNING")
    cases=[
      ("ETH_STATIC",LIBS/"JWPLC_Ethernet/examples/02.Ethernet_StaticIP_Basic"),
      ("ETH_SPI",LIBS/"JWPLC_Ethernet/examples/Ethernet_SPI_Coexistence"),
      ("MODBUS_CLIENT",LIBS/"JWPLC_ModbusTCP/examples/02.ModbusTCP_Client"),
    ]
    for label,path in cases:
        compile_case(cli,path,"PRODUCT_"+label,ETH)
    STATE["regressions"]=[x for x,_ in cases]
    phase("PRODUCT_REGRESSIONS","PASS")

def final():
    phase("FINAL_AUDIT","RUNNING")
    require(set(git("diff","--name-only").splitlines())==set(SOURCE_MAP),
            "FINAL_DIRTY_SCOPE_INVALID","HARNESS")
    require(not git("diff","--cached","--name-only"),"STAGED_FILES_NOT_ALLOWED","HARNESS")
    require(not git("diff","--check"),"FINAL_DIFF_CHECK","HARNESS")
    for p,c in SOURCE_MAP.items():
        require(sha(ROOT/p)==sha(ROOT/c),"PRODUCT_CANDIDATE_CHANGED_"+p,"HARNESS")
    require(sha(CORE)==CORE_SHA and sha(TFT)==TFT_SHA,
            "CORE_OR_TFT_ARCHIVE_CHANGED","HARNESS")
    STATE["product_sha256"]={p:sha(ROOT/p) for p in SOURCE_MAP}
    phase("FINAL_AUDIT","PASS")

def restore():
    if not ADOPTED:return
    for p in SOURCE_MAP:
        original=BACKUP/p
        if original.is_file():shutil.copy2(original,ROOT/p)
    good=all(sha(ROOT/p)==sha(BACKUP/p) for p in SOURCE_MAP)
    STATE["rollback"]="PASS" if good else "FAIL"
    emit("ROLLBACK",STATE["rollback"])

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--serial-port",default="")
    parser.add_argument("--arduino-cli",default="")
    parser.add_argument("--skip-physical",action="store_true")
    args=parser.parse_args()
    OUT.mkdir(parents=True,exist_ok=False)
    TMP.mkdir(parents=True,exist_ok=False)
    try:
        preflight()
        invoke([sys.executable,"-B",ROOT/"tools/alpha13/gates/a13_finalizer_portability_selftest.py"],
               "F112_PORTABILITY_SELFTEST")
        cli=cli_find(args.arduino_cli)
        invoke([cli,"version"],"CLI_VERSION")
        if not args.skip_physical:
            import serial
            port=port_check(args.serial_port)
            STATE["port"]=port
        selected=overlay()
        preview_build(cli,selected)
        if args.skip_physical:
            STATE["status"]="PASS_COMPILE_PHYSICAL_PENDING"
            emit("STATUS",STATE["status"])
            save()
            return 0
        physical(cli,port,selected)
        adopt()
        regress(cli)
        final()
        STATE["status"]="PASS_PHYSICAL_NORMAL_SOURCE"
        emit("STATUS",STATE["status"])
        emit("PRODUCT_DIRTY_FILES",len(SOURCE_MAP))
        emit("NEXT_GATE","G5_CLOSURE_AFTER_REVIEW")
        save()
        return 0
    except (Stop,Exception,KeyboardInterrupt) as e:
        STATE["reason"]=str(e)
        STATE["category"]=e.category if isinstance(e,Stop) else (
            "USER_ABORT" if isinstance(e,KeyboardInterrupt) else "HARNESS")
        restore()
        STATE["status"]="REVIEW"
        emit("STATUS",STATE["status"])
        emit("REASON",STATE["reason"])
        emit("CATEGORY",STATE["category"])
        save()
        return 2

if __name__=="__main__":sys.exit(main())
