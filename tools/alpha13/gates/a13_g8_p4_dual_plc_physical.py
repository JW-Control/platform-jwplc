#!/usr/bin/env python3
"""G8-P4: validación física aislada, dos JWPLC Basic, Master COM4 / Slave COM14.

Compila fuentes productivas, confirma explícitamente overwrite de AMBAS placas,
sube Slave y Master por separado, lee ambas consolas sin DTR/RTS,
ejecuta 12 FC03 y comprueba rxBytes RX=156 (Master), RX=96 (Slave).
No modifica repositorio/firmware de producto, Git, FRAM ni relés por sketch.
El firmware PREEXISTENTE de ambas placas NO se restaura automáticamente.
"""
from __future__ import annotations

import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import re
import subprocess
import sys
import tempfile
import time
import uuid
from datetime import datetime

ROOT = Path(__file__).resolve().parents[3]
GATE = ROOT/"tools/alpha13/gates/a13_g8_p1_p2_candidate.py"
FQBN = "jwplc_local:esp32:jwplcbasic"
BRANCH = "v2.1.0-alpha.13/feature/cleanup-robustness"
PRODUCT_COMMIT = "fb71d65eb2c842b3f46e6949760e693f3b73d95e"
CLI_SHA = "1017de89179c3167e6b8a38ca6cc4091fa69a3cf97aafa7810f01423003f5571"
ARCHIVE_SHA = "f239100fbb6770a3a6f336179fbfa3a4afd27cc16e12c6f239cf05f28f7a1416"
CORE_SHA = "a1a985f64c22838a1987d4280b8c1dc431a42d5789c6692cbfa36dfb6252246d"
TFT_SHA = "4c606ba66e29c1337d42450a29fd09fe76c6988f72aa51d3882b81db59f82ecc"
FRAM_SHA = "df390f3379da13c6d4503a9fb83bc098f2944590fe3ad083348e95bfdcaf7299"
MASTER_PORT = "COM4"
SLAVE_PORT = "COM14"
NUM_READS = 12
MASTER_RX = NUM_READS*13
SLAVE_RX = NUM_READS*8
CONFIRM = "G8 FLASH COM14 SLAVE COM4 MASTER"
LIBS = ROOT/"JWPLC/2.1.0/libraries"
RTU = LIBS/"JWPLC_ModbusRTU"
RS = LIBS/"JWPLC_RS485"
ARCHIVE = RTU/"src/esp32/libJWPLC_ModbusRTU.a"
SKETCHES = {
    "SLAVE": ROOT/"tools/alpha13/sketches/g8_p4_slave",
    "MASTER": ROOT/"tools/alpha13/sketches/g8_p4_master",
}
RUN = {}
RES = None
TEMP = None


def say(k,v):
    print("G8_P4_"+str(k)+"="+str(v),flush=True)


def require(x,message):
    if not x:
        raise RuntimeError(message)


def sha(path):
    d=hashlib.sha256()
    with Path(path).open("rb") as f:
        for chunk in iter(lambda:f.read(1024*1024),b""):d.update(chunk)
    return d.hexdigest()


def git(*args):
    p=subprocess.run(["git","-c","core.quotepath=false",*args],
                     cwd=ROOT,capture_output=True)
    require(p.returncode==0,"GIT_COMMAND_FAILED_"+repr(args)+":"+
            p.stderr.decode("utf-8","replace")[-300:])
    return p.stdout.decode("utf-8","replace").strip()


def save():
    if RES:
        (RES/"MANIFEST.json").write_text(
            json.dumps(RUN,indent=2,ensure_ascii=False)+"\n",encoding="utf-8")
        (RES/"SUMMARY.log").write_text(
            "\n".join("G8_P4_"+k.upper()+"="+str(v)
                      for k,v in RUN.items() if k!="ports")+"\n",
            encoding="utf-8")


def phase(k,state):
    RUN.setdefault("phases",{})[k]=state
    say("PHASE",k+"_"+state)
    save()


def command(args,label,cwd=ROOT):
    argv=[str(x) for x in args]
    p=subprocess.run(argv,cwd=cwd,stdout=subprocess.PIPE,
                     stderr=subprocess.STDOUT,encoding="utf-8",errors="replace")
    out=p.stdout or ""
    (RES/(label+".log")).write_text(
        "COMMAND="+repr(argv)+"\nEXIT="+str(p.returncode)+"\n"+out,
        encoding="utf-8")
    say(label+"_EXIT",p.returncode)
    if p.returncode:
        for line in [x for x in out.splitlines()
                     if re.search(r"error:|fatal error|undefined reference|No such file|cannot",x,re.I)][-6:]:
            say("DIAGNOSTIC",line[:280])
    require(p.returncode==0,label+"_FAILED_SEE_LOG")
    return out


def load_gate():
    sp=importlib.util.spec_from_file_location("g8_candidate_contract",GATE)
    require(sp is not None and sp.loader is not None,"G8_CANDIDATE_IMPORT_BAD")
    gate=importlib.util.module_from_spec(sp)
    sys.modules[sp.name]=gate
    sp.loader.exec_module(gate)
    gate.M.clear()
    gate.M["archive_sha256"]=ARCHIVE_SHA
    return gate


def preflight(cli):
    phase("PREFLIGHT","RUNNING")
    require(Path.cwd().resolve()==ROOT,"RUN_AT_REPOSITORY_ROOT")
    require(cli.is_file() and cli.name.lower()=="arduino-cli.exe" and
            sha(cli)==CLI_SHA,"PINNED_ARDUINO_CLI_INVALID")
    require(git("branch","--show-current")==BRANCH,"WRONG_BRANCH")
    head=git("rev-parse","HEAD")
    require(head==git("rev-parse","refs/remotes/origin/"+BRANCH),
            "GIT_LOCAL_REMOTE_MISMATCH")
    require(subprocess.run(["git","merge-base","--is-ancestor",PRODUCT_COMMIT,head],
                           cwd=ROOT).returncode==0,"G8_PRODUCT_COMMIT_NOT_ANCESTOR")
    require(not git("diff","--name-only") and not git("diff","--cached","--name-only"),
            "GIT_HAS_TRACKED_CHANGES")
    for path,value in {
        ARCHIVE:ARCHIVE_SHA,
        ROOT/"JWPLC/2.1.0/precompiled/core/JWPLCBASIC/core.a":CORE_SHA,
        LIBS/"JWPLC_TFT/src/esp32/libJWPLC_TFT.a":TFT_SHA,
        LIBS/"JW_FRAM/src/esp32/libJW_FRAM.a":FRAM_SHA,
    }.items():
        require(path.is_file() and sha(path)==value,
                "PACKAGE_ARCHIVE_HASH_MISMATCH_"+path.name)
    require(sha(RTU/"src/JWPLC_ModbusRTU.cpp") ==
            sha(ROOT/"tools/alpha13/candidates/g8/JWPLC_ModbusRTU.cpp"),
            "RTU_PRODUCT_NOT_SAME_AS_COMMITTED_CANDIDATE")
    require(sha(RS/"src/JWPLC_RS485.cpp") ==
            sha(ROOT/"tools/alpha13/candidates/g8/JWPLC_RS485.cpp"),
            "RS_PRODUCT_NOT_SAME_AS_COMMITTED_CANDIDATE")
    for name,sketch in SKETCHES.items():
        require((sketch/(sketch.name+".ino")).is_file(),
                "SKETCH_MISSING_"+name)
    try:
        import serial
        from serial.tools import list_ports
        RUN["pyserial_version"]=getattr(serial,"VERSION","unknown")
    except ImportError as exc:
        raise RuntimeError("PYSERIAL_REQUIRED_ALREADY_USED_BY_PRIOR_GATES") from exc
    allports=list(list_ports.comports())
    byname={}
    for x in allports:
        byname.setdefault(x.device.upper(),[]).append(x)
    say("PORTS_VISIBLE",",".join(p.device for p in allports) or "NONE")
    for role,port in (("MASTER",MASTER_PORT),("SLAVE",SLAVE_PORT)):
        matches=byname.get(port,[])
        require(len(matches)==1,"PORT_NOT_PRESENT_OR_AMBIGUOUS_"+role+"_"+port)
        x=matches[0]
        meta={"device":x.device,"description":x.description,
              "vid":x.vid,"pid":x.pid,"serial_number":x.serial_number,
              "hwid":x.hwid}
        RUN.setdefault("ports",{})[role]=meta
        say(role+"_PORT",x.device)
        say(role+"_PORT_USB",f"VID={x.vid} PID={x.pid} DESCRIPTION={x.description}")
        require(x.vid==0x1A86 and x.pid==0x7523,
                "PORT_NOT_CH340C_"+role+"_VERIFY_BEFORE_ANY_UPLOAD")
    RUN["entry_head"]=head
    RUN["archive_sha256"]=ARCHIVE_SHA
    phase("PREFLIGHT","PASS")


def compile_pair(cli):
    phase("BUILD","RUNNING")
    gate=load_gate()
    for role in ("SLAVE","MASTER"):
        sketch=SKETCHES[role]
        build=TEMP/("build_"+role.lower())
        out=command([cli,"compile","--fqbn",FQBN,"-j","0","-v","--clean",
                     "--build-path",build,"--libraries",LIBS,sketch],
                     "COMPILE_"+role)
        gate.assert_selected(out,role,"JWPLC_RS485",RS)
        gate.assert_selected(out,role,"JWPLC_ModbusRTU",RTU)
        require(len(list(build.rglob("JWPLC_RS485.cpp.o")))==1,
                "RS485_SOURCE_OBJECT_NOT_REBUILT_"+role)
        require(not list(build.rglob("JWPLC_ModbusRTU.cpp.o")),
                "RTU_COMPILED_AS_CPP_NOT_PRECOMPILED_"+role)
        gate.link_check(out,RTU,role,ARCHIVE)
        RUN.setdefault("build_paths",{})[role]=str(build)
        say("PRODUCT_BUILD_"+role,"PASS_PRECOMPILED_RTU_RS485_SOURCE")
    require(sha(ARCHIVE)==ARCHIVE_SHA,"ARCHIVE_CHANGED_AFTER_BUILD")
    phase("BUILD","PASS")


def consent_to_flash():
    phase("FLASH_AUTHORIZATION","RUNNING")
    print("\nIMPORTANTE: Se sobreescribirá el firmware actual de AMBOS JWPLC Basic.\n"
          "NO usar si controlan maquinaria o si necesitas conservar firmware previo.\n"
          "Antes de confirmar: desconecta actuadores/cargas de las salidas,\n"
          "alimenta ambos equipos por USB en un banco y conecta RS485 P con P,\n"
          "N con N (P=A, N=B en JWPLC), sin conectar 24V a RS-485.\n"
          "MASTER USB=COM4, SLAVE USB=COM14. La copia de firmware anterior\n"
          "NO se recuperará automáticamente tras esta prueba.\n",flush=True)
    try:
        answer=input("Escribe exactamente '"+CONFIRM+"' para aceptar upload de ambas placas: ")
    except EOFError:
        answer=""
    require(answer.strip()==CONFIRM,"UPLOAD_NOT_AUTHORIZED_NO_HARDWARE_CHANGE")
    RUN["flash_authorized"]=True
    phase("FLASH_AUTHORIZATION","PASS")


def upload_pair(cli):
    phase("UPLOAD","RUNNING")
    # Primero Slave. El Master nunca se flashea si falla la carga del Slave.
    for role,port in (("SLAVE",SLAVE_PORT),("MASTER",MASTER_PORT)):
        say("UPLOAD_TARGET",role+"_"+port)
        command([cli,"upload","--fqbn",FQBN,"--port",port,
                 "--input-dir",RUN["build_paths"][role]],
                "UPLOAD_"+role)
        RUN.setdefault("uploads",{})[role]="PASS"
        save()
    phase("UPLOAD","PASS")


def open_port(port):
    import serial
    dev=serial.Serial(port=None,baudrate=115200,timeout=0.08,
                      write_timeout=1.0,rtscts=False,dsrdtr=False,
                      xonxoff=False)
    dev.dtr=False
    dev.rts=False
    dev.port=port
    dev.open()
    return dev


def get_lines(dev,buffer):
    data=dev.read(dev.in_waiting or 1)
    if not data:
        return []
    buffer.extend(data)
    require(len(buffer)<8192,"SERIAL_LINE_BUFFER_EXCEEDED")
    lines=[]
    while b"\n" in buffer:
        i=buffer.index(b"\n")
        raw=bytes(buffer[:i]).strip(b"\r")
        del buffer[:i+1]
        lines.append(raw.decode("utf-8","backslashreplace").strip())
    return lines


def parse_report(line,role,token):
    prefix="G8_P4_"+role+"_RESULT:"+token+" "
    if not line.startswith(prefix):
        return None
    values={}
    for field in line[len(prefix):].split():
        require("=" in field,"REPORT_FIELD_MALFORMED_"+role)
        k,v=field.split("=",1)
        require(k not in values and v.isdecimal(),
                "REPORT_VALUE_INVALID_"+role+"_"+k)
        values[k]=int(v)
    required=(
        {"OK","FAIL","RXBYTES","RXFRAMES","TXFRAMES","TIMEOUTS","CRC","READY"}
        if role=="MASTER" else
        {"RXBYTES","RXFRAMES","TXFRAMES","REQUESTS","CRC","READY"}
    )
    require(set(values)==required,"REPORT_FIELDS_NOT_EXACT_"+role)
    return values


def check_reports(master,slave):
    require(master["OK"]==NUM_READS and master["FAIL"]==0,
            "MASTER_READ_VALUES_NOT_12_OF_12")
    require(master["RXBYTES"]==MASTER_RX,
            "MASTER_RX_BYTES_MISMATCH_EXPECTED_156")
    require(master["RXFRAMES"]>=NUM_READS and master["TXFRAMES"]>=NUM_READS,
            "MASTER_FRAME_COUNTERS_LOW")
    require(master["TIMEOUTS"]==0 and master["CRC"]==0 and master["READY"]==1,
            "MASTER_TIMEOUT_CRC_OR_UART_NOT_READY")
    require(slave["RXBYTES"]==SLAVE_RX,
            "SLAVE_RX_BYTES_MISMATCH_EXPECTED_96")
    require(slave["RXFRAMES"]>=NUM_READS and slave["TXFRAMES"]>=NUM_READS
            and slave["REQUESTS"]>=NUM_READS,"SLAVE_FRAME_COUNTERS_LOW")
    require(slave["CRC"]==0 and slave["READY"]==1,
            "SLAVE_CRC_OR_UART_NOT_READY")
    return True


def monitor():
    phase("PHYSICAL","RUNNING")
    import serial
    devices={}
    logs={}
    buffers={}
    markers={}
    token=RUN["run_id"].split("_")[-1] + uuid.uuid4().hex[:16]
    require(bool(re.fullmatch(r"[0-9a-f]{24}",token)),"INVALID_SESSION_TOKEN")
    RUN["token"]=token
    try:
        for role,port in (("SLAVE",SLAVE_PORT),("MASTER",MASTER_PORT)):
            devices[role]=open_port(port)
            logs[role]=(RES/("SERIAL_"+role+".log")).open("w",encoding="utf-8")
            buffers[role]=bytearray()
        say("SERIAL_DTR_RTS","FALSE_FALSE_BOTH")
        deadline=time.monotonic()+45
        while time.monotonic()<deadline and len(markers)<2:
            for role,dev in devices.items():
                for line in get_lines(dev,buffers[role]):
                    if not line: continue
                    logs[role].write(line+"\n")
                    logs[role].flush()
                    if line.startswith("G8_P4_FAIL:"):
                        raise RuntimeError("FIRMWARE_START_"+role+"_"+line[:120])
                    mark="G8_P4_READY:"+role+":"
                    if line.startswith(mark):
                        expected=(
                            "G8_P4_READY:SLAVE:ID2:115200:8N1" if role=="SLAVE"
                            else "G8_P4_READY:MASTER:115200:8N1")
                        require(line==expected,"SERIAL_ROLE_OR_CONFIG_WRONG_"+role)
                        if role not in markers:
                            markers[role]=line
                            say("READY_"+role,"PASS_"+dev.port)
            time.sleep(0.005)
        require(len(markers)==2,"DUAL_READY_TIMEOUT_OR_WRONG_PORT")
        phase("DUAL_READY","PASS")
        RUN["ready_markers"]=markers
        # Nota: el Slave atiende RTU incluso antes de recibir el START.
        devices["MASTER"].write(("G8_P4_START:"+token+"\n").encode("ascii"))
        devices["MASTER"].flush()
        say("START","SENT_TO_COM4_ONCE")
        master=None
        masterstarted=False
        deadline=time.monotonic()+40
        while time.monotonic()<deadline and master is None:
            for role,dev in devices.items():
                for line in get_lines(dev,buffers[role]):
                    if not line: continue
                    logs[role].write(line+"\n")
                    logs[role].flush()
                    if line.startswith("G8_P4_FAIL:"):
                        raise RuntimeError("FIRMWARE_"+role+"_"+line[:120])
                    if role=="MASTER":
                        if line=="G8_P4_MASTER_STARTED:"+token:
                            masterstarted=True
                            say("MASTER_STARTED","PASS_TOKEN_MATCH")
                        val=parse_report(line,"MASTER",token)
                        if val is not None:
                            master=val
                            say("MASTER_REPORT",json.dumps(master,sort_keys=True))
            time.sleep(0.005)
        require(masterstarted,"MASTER_TOKEN_NOT_ACKNOWLEDGED")
        require(master is not None,"MASTER_REPORT_TIMEOUT")
        devices["SLAVE"].write(("G8_P4_REPORT:"+token+"\n").encode("ascii"))
        devices["SLAVE"].flush()
        say("SLAVE_REPORT_REQUEST","SENT_TO_COM14_ONCE")
        slave=None
        deadline=time.monotonic()+12
        while time.monotonic()<deadline and slave is None:
            for role,dev in devices.items():
                for line in get_lines(dev,buffers[role]):
                    if not line: continue
                    logs[role].write(line+"\n")
                    logs[role].flush()
                    if line.startswith("G8_P4_FAIL:"):
                        raise RuntimeError("FIRMWARE_"+role+"_"+line[:120])
                    if role=="SLAVE":
                        val=parse_report(line,"SLAVE",token)
                        if val is not None:
                            slave=val
                            say("SLAVE_REPORT",json.dumps(slave,sort_keys=True))
            time.sleep(0.005)
        require(slave is not None,"SLAVE_REPORT_TIMEOUT")
        RUN["master"]=master
        RUN["slave"]=slave
        phase("PHYSICAL","PASS")
        check_reports(master,slave)
        say("MASTER_RX_BYTES","PASS_156")
        say("SLAVE_RX_BYTES","PASS_96")
        say("MODBUS_FC03","PASS_12_OF_12")
        RUN["serial_validation"]="PASS"
    finally:
        for d in devices.values():
            try:d.close()
            except Exception:pass
        for l in logs.values():
            try:l.close()
            except Exception:pass


def run_gate(args):
    global RES,TEMP
    rid="g8_p4_"+datetime.now().strftime("%Y%m%d_%H%M%S")+"_"+uuid.uuid4().hex[:8]
    RES=ROOT/"tools/alpha13/results"/rid
    TEMP=Path(tempfile.gettempdir())/("jwplc_a13_"+rid)
    require(not RES.exists() and not TEMP.exists(),"RUN_COLLISION")
    RES.mkdir(parents=True)
    TEMP.mkdir(parents=True)
    RUN.clear()
    RUN.update({
        "schema":"A13-G8-P4-DUAL-PLC-1","run_id":rid,
        "status":"REVIEW_STOP","phases":{},"master_port":MASTER_PORT,
        "slave_port":SLAVE_PORT,"flash_authorized":False,
        "uploads":{},"rs485_wiring_confirmed_by_operator":False,
        "source_product_modified":False,"git_commit":False,"git_push":False,
        "physical_fram_write_test":False,
        "firmware_restoration":"NOT_AUTOMATED",
        "expected_master_rxbytes":MASTER_RX,"expected_slave_rxbytes":SLAVE_RX,
    })
    try:
        cli=Path(args.arduino_cli).expanduser().resolve()
        preflight(cli)
        compile_pair(cli)
        if args.preflight_only:
            RUN["status"]="PREFLIGHT_BUILD_PASS_NO_UPLOAD"
            say("STATUS",RUN["status"])
            say("RUN_ID",rid)
            say("RESULTS",RES)
            return 0
        consent_to_flash()
        RUN["rs485_wiring_confirmed_by_operator"]=True
        upload_pair(cli)
        monitor()
        require(sha(ARCHIVE)==ARCHIVE_SHA,"ARCHIVE_CHANGED_DURING_GATE")
        require(not git("diff","--name-only") and not git("diff","--cached","--name-only"),
                "REPOSITORY_PRODUCT_OR_DOC_CHANGED")
        phase("FINAL_AUDIT","PASS")
        RUN["status"]="PHYSICAL_PASS_2JWPLC_12FC03_RX_EXACT"
        say("STATUS",RUN["status"])
        say("MASTER","COM4")
        say("SLAVE","COM14")
        say("RESULTS",RES)
        say("GIT_OR_PRODUCT_CHANGE","NONE")
        say("FIRMWARE_RESTORE","NOT_AUTOMATED")
        return 0
    except Exception as exc:
        RUN["status"]="REVIEW_STOP"
        RUN["reason"]=repr(exc)
        say("STATUS","REVIEW_STOP")
        say("REASON",repr(exc))
        say("RESULTS",RES)
        if RUN.get("uploads"):
            say("FIRMWARE_RESTORE","MANUAL_OR_SEPARATE_APPROVED_GATE_REQUIRED")
        return 2
    finally:
        save()


def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--arduino-cli",default=r"C:\Program Files\Arduino CLI\arduino-cli.exe")
    ap.add_argument("--preflight-only",action="store_true")
    args=ap.parse_args()
    return run_gate(args)


if __name__=="__main__":
    sys.exit(main())
