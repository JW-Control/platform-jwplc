#!/usr/bin/env python3
"""Alpha13 G10-H (Windows): banco COM4 principal, COM14 slave opcional.

Compone overlay G10-I a plataforma TEMPORAL, compila con Arduino CLI local,
comprueba procedencia y pide consentimiento explícito antes de cada upload.
No modifica el package productivo ni resetea GPIO4. Nunca hace rollback
automático del firmware anterior. Sin COM14: PASS_LIMITED sin RTU cruzado.
"""
from __future__ import annotations

import argparse
from datetime import datetime
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import re
import shutil
import socket
import struct
import subprocess
import sys
import tempfile
import time
import uuid

ROOT=Path(__file__).resolve().parents[3]
G10_I_SHA="a954cabd9c6e37876b59995a41ff0a449308ccd1"
BRANCH="codex/alpha13-g10-h-physical-com4-com14"
FQBN="jwplc_local:esp32:jwplcbasic"
MASTER="COM4"
SLAVE="COM14"
SERIAL_BAUD=115200
CH340=(0x1A86,0x7523)
SECONDS=40
OUT=None
STATE={"phase":"INITIAL","physical_status":"NOT_RUN","flash_com4":"NOT_RUN",
       "flash_com14":"NOT_RUN","gpio4_reset":"DISABLED",
       "product_modified":False,"prior_firmware_restored":False}

def emit(k,value):
    print("G10_H_"+str(k)+"="+str(value),flush=True)

def require(ok,reason):
    if not ok:raise RuntimeError(reason)

def digest(path):
    obj=hashlib.sha256()
    with Path(path).open("rb") as h:
        for chunk in iter(lambda:h.read(1024*1024),b""):obj.update(chunk)
    return obj.hexdigest()

def git(*args):
    p=subprocess.run(["git",*args],cwd=ROOT,capture_output=True,
                     text=True,errors="replace")
    require(p.returncode==0,"GIT_FAILED_"+str(args))
    return p.stdout.strip()

def save():
    if OUT is None:return
    (OUT/"MANIFEST.json").write_text(
        json.dumps(STATE,ensure_ascii=False,indent=2)+"\n",encoding="utf-8")
    (OUT/"SUMMARY.log").write_text(
        "".join("G10_H_"+k.upper()+"="+str(v)+"\n" for k,v in STATE.items()
                if not isinstance(v,(dict,list))),encoding="utf-8")

def phase(name):
    STATE["phase"]=name
    emit("PHASE",name)
    save()

def command(argv,label,cwd=None):
    cmd=[str(x) for x in argv]
    p=subprocess.run(cmd,cwd=cwd or ROOT,capture_output=True,text=True,
                     errors="replace",timeout=300)
    content=(p.stdout or "")+"\n"+(p.stderr or "")
    (OUT/(label+".log")).write_text(
        "COMMAND="+repr(cmd)+"\nEXIT="+str(p.returncode)+"\n"+content,
        encoding="utf-8")
    emit(label+"_EXIT",p.returncode)
    if p.returncode:
        for row in content.splitlines():
            if re.search(r"error:|fatal error|undefined reference|failed",row,re.I):
                emit("BUILD_DIAGNOSTIC",row[:250])
    require(p.returncode==0,label+"_FAILED_CHECK_LOG")
    return content

def verify_port(port):
    from serial.tools import list_ports
    matches=[x for x in list_ports.comports() if x.device.upper()==port]
    require(len(matches)==1,"PORT_ABSENT_OR_AMBIGUOUS_"+port)
    info=matches[0]
    require((info.vid,info.pid)==CH340,"UNEXPECTED_USB_DEVICE_"+port)
    return {"port":port,"vid":info.vid,"pid":info.pid,
            "description":info.description}

def preflight(rtu):
    phase("N0_PREFLIGHT")
    require(os.name=="nt","WINDOWS_REQUIRED")
    require(Path.cwd().resolve()==ROOT,"EXECUTE_FROM_REPO_ROOT")
    head=git("rev-parse","HEAD")
    require(git("rev-parse",f"refs/remotes/origin/{BRANCH}")==head,
            "REMOTE_HEAD_NOT_SAME_STOP")
    require(subprocess.run(["git","merge-base","--is-ancestor",G10_I_SHA,head],
                           cwd=ROOT).returncode==0,
            "G10_I_CANDIDATE_NOT_ANCESTOR")
    require(not git("diff","--name-only") and
            not git("diff","--cached","--name-only") and
            not git("ls-files","--unmerged"),
            "TRACKED_WORKTREE_NOT_CLEAN")
    cli=shutil.which("arduino-cli")
    if not cli:
        for guess in (Path.home()/"AppData/Local/Programs/Arduino CLI/arduino-cli.exe",
                      Path.home()/"AppData/Local/Arduino15/arduino-cli.exe"):
            if guess.is_file():cli=str(guess);break
    require(cli is not None,"ARDUINO_CLI_NOT_ON_PATH")
    version=command([cli,"version"],"ARDUINO_CLI_VERSION")
    require(re.search(r"\bVersion:\s*1\.5\.1\b",version),
            "EXPECTED_ARDUINO_CLI_1_5_1")
    command([cli,"core","list"],"ARDUINO_INSTALLED_CORES")
    import serial
    STATE["pyserial_version"]=getattr(serial,"VERSION","unknown")
    ports={MASTER:verify_port(MASTER)}
    if rtu: ports[SLAVE]=verify_port(SLAVE)
    STATE.update({"head":head,"g10_i_source_sha":G10_I_SHA,
                  "ports":ports,"rtu_enabled":bool(rtu),
                  "cli_version":"1.5.1"})
    emit("PREFLIGHT","PASS_"+("DUAL" if rtu else "SINGLE"))
    save()
    return cli

def isolated_platform(tmp,cli):
    phase("STAGE_COMPOSED_PLATFORM")
    source=ROOT/"tools/alpha13/gates/a13_g10_integrated.py"
    spec=importlib.util.spec_from_file_location("g10_h_import_integrator",source)
    require(spec is not None and spec.loader is not None,"INTEGRATOR_MISSING")
    m=importlib.util.module_from_spec(spec)
    sys.modules[spec.name]=m
    spec.loader.exec_module(m)
    check_head=m.preflight(True)
    require(check_head==STATE["head"],"HEAD_CHANGED_DURING_STAGE")
    files=m.compose()   # No N1 rebuild: exact candidate already has N1/N2/N3 PASS.
    hashes={rel:{"sha256":hashlib.sha256(src.encode("utf-8")).hexdigest()}
            for rel,src in files.items()}
    stage=tmp/"staged-package"
    m.stage_platform(stage,files,hashes)
    users=tmp/"sketchbook"
    target=users/"hardware/jwplc_local/esp32"
    target.parent.mkdir(parents=True)
    shutil.copytree(stage,target)
    for relative in ("boards.txt","platform.txt",
                     "precompiled/core/JWPLCBASIC/core.a"):
        original=ROOT/"JWPLC/2.1.0"/relative
        require(digest(original)==digest(target/relative),
                "PLATFORM_FROZEN_DRIFT_"+relative)
    freeze=m.load(m.MODULES["frozen"],"g10h_frozen")
    require(freeze.check_frozen()==5,"FROZEN_ARCHIVE_HASH_DRIFT")
    library=target/"libraries/JWPLC_Ethernet/src"
    ethernet=(library/"JWPLC_Ethernet.cpp").read_text(encoding="utf-8-sig")
    require("jwplc_w5500_candidate::enter(timeoutMs)" in ethernet and
            "jwplc_w5500_candidate::leave();" in ethernet,
            "B08_RUNTIME_WRAPPERS_ABSENT")
    require("EthernetUDP::jwplcServiceDeferredClose()" in ethernet,
            "B03_AUTOLOAD_TICKET_ABSENT")
    config_dir=tmp/"arduino-config"
    config_dir.mkdir()
    command([cli,"config","init","--dest-dir",config_dir],
            "CONFIG_INIT")
    config=config_dir/"arduino-cli.yaml"
    require(config.is_file(),"CONFIG_FILE_NOT_CREATED")
    command([cli,"--config-file",config,"config","set",
             "directories.user",users],"CONFIG_ISOLATE_SKETCHBOOK")
    STATE["overlay_sources"]={rel:info["sha256"] for rel,info in hashes.items()}
    STATE["staged_core_sha256"]=digest(target/"precompiled/core/JWPLCBASIC/core.a")
    STATE["staged_platform"] = "TEMP_ISOLATED_NO_PRODUCT_MUTATION"
    emit("STAGE","PASS_"+str(len(files))+"_OVERLAY_SOURCES")
    save()
    return config,target

def build(cli,config,platform,tmp,rtu):
    phase("BUILD_ONCE")
    sketches={"COM4":ROOT/"tools/alpha13/sketches/g10_h_integrated"}
    if rtu:sketches["COM14"]=ROOT/"tools/alpha13/sketches/g8_p4_slave"
    builds={}
    for role,sketch in sketches.items():
        builddir=tmp/("build-"+role)
        properties=command([cli,"--config-file",config,"compile",
              "--fqbn",FQBN,"--show-properties",sketch],
              "PROPERTIES_"+role)
        expected="runtime.platform.path="+str(platform)
        require(any(line.strip().replace("\\","/")==expected.replace("\\","/")
                    for line in properties.splitlines()),
                "WRONG_FULL_ALPHA13_PLATFORM_"+role)
        logs=command([cli,"--config-file",config,"compile",
              "--fqbn",FQBN,"--warnings","default",
              "--build-path",builddir,"--clean","-v",sketch],
              "COMPILE_"+role)
        for lib in ("JWPLC_Ethernet","JWPLC_ModbusTCP"):
            if role=="COM4":
                expectedlib=platform/"libraries"/lib
                require(any("Using library "+lib+" " in line and
                            str(expectedlib).lower() in line.lower()
                            for line in logs.splitlines()),
                        "WRONG_SELECTED_LIBRARY_"+lib)
        if role=="COM4":
            objects=list(builddir.rglob("JWPLC_W5500_SafeAccess.cpp.o"))
            require(len(objects)==1,"SAFEACCESS_OBJECT_NOT_UNIQUE")
            dep=objects[0].with_suffix(".d")
            if not dep.is_file():dep=Path(str(objects[0])+".d")
            require(dep.is_file(),"SAFEACCESS_DEPENDENCY_NOT_FOUND")
            raw=dep.read_text(encoding="utf-8",errors="replace").replace("\\\\","/")
            require("JWPLC_W5500_SafeAccess.cpp" in raw,
                    "SAFEACCESS_DEPENDENCY_WRONG")
            STATE["safeaccess_object_sha256"]=digest(objects[0])
        elfs=list(builddir.glob("*.elf"))
        require(len(elfs)==1,"ELF_NOT_UNIQUE_"+role)
        STATE["elf_"+role+"_sha256"]=digest(elfs[0])
        builds[role]=builddir
        emit("BUILD_"+role,"PASS_SOURCE_FIRST")
    save()
    return builds

def authorize(rtu):
    phase("FLASH_AUTHORIZATION")
    print("\n=== G10-H: AVISO ANTES DE MODIFICAR PLACAS ===",flush=True)
    print("Banco aislado SIN actuadores ni cargas en reles Q.")
    print("COM4: reemplazara firmware con diagnostico candidato G10-I.")
    if rtu:print("COM14: reemplazara firmware con Slave G8-P4 ID2.")
    print("No hay rollback ni restauracion automatica del firmware anterior.")
    print("No se activa GPIO4 ni se escriben FRAM o microSD.")
    print("Cierra Serial Monitor, IDE y cualquier otro proceso que use los COM.")
    token="G10H AUTORIZO FLASH COM4"+(" COM14" if rtu else "")
    try:confirm=input("\nEscribe exactamente '"+token+"': ").strip()
    except EOFError:confirm=""
    require(confirm==token,"UPLOAD_NOT_AUTHORIZED")
    STATE["consent"]="EXPLICIT_AFTER_BUILD"
    save()

def upload(cli,config,builds,rtu):
    phase("UPLOAD_AUTHORIZED")
    roles=([("COM14",SLAVE),("COM4",MASTER)] if rtu else [("COM4",MASTER)])
    for role,port in roles:
        require(verify_port(port)==STATE["ports"][port],
                "USB_IDENTITY_CHANGED_"+role)
        command([cli,"--config-file",config,"upload",
            "--fqbn",FQBN,"--port",port,"--input-dir",builds[role]],
            "UPLOAD_"+role)
        STATE["flash_"+role.lower()]="PASS_CONFIRMED_UPLOAD"
        save()

def serial_open(port):
    import serial
    channel=serial.Serial(port=None,baudrate=SERIAL_BAUD,timeout=0.06,
                          write_timeout=1,rtscts=False,dsrdtr=False)
    channel.dtr=False;channel.rts=False
    channel.port=port
    channel.open()
    return channel

def serial_lines(channel,buf):
    buf.extend(channel.read(channel.in_waiting or 1))
    require(len(buf)<32768,"SERIAL_RX_OVERFLOW")
    data=[]
    while b"\n" in buf:
        i=buf.index(b"\n")
        data.append(bytes(buf[:i]).decode("utf-8","replace").strip())
        del buf[:i+1]
    return data

def recv_exact(sock,n):
    parts=bytearray()
    while len(parts)<n:
        block=sock.recv(n-len(parts))
        if not block:raise OSError("EOF_TCP")
        parts.extend(block)
    return bytes(parts)

def check_fc03(ip,tid):
    req=struct.pack(">HHHBBHH",tid,0,6,1,3,0,4)
    with socket.create_connection((ip,502),timeout=1.2) as s:
        s.settimeout(1.2)
        s.sendall(req)
        header=recv_exact(s,7)
        rid,pid,length,unit=struct.unpack(">HHHB",header)
        require(rid==tid and pid==0 and unit==1 and length==11,
                "MODBUS_TCP_MBAP_BAD")
        pdu=recv_exact(s,length-1)
        require(len(pdu)==10 and pdu[:2]==b"\x03\x08" and
                struct.unpack(">4H",pdu[2:])==(0x1234,0x2345,0x3456,0x4567),
                "MODBUS_TCP_FC03_BAD")
    return True

def check_udp(ip,token,seq):
    payload=f"G10H.{token}.{seq:04d}".encode("ascii")
    with socket.socket(socket.AF_INET,socket.SOCK_DGRAM) as s:
        s.settimeout(1.2)
        s.sendto(payload,(ip,15020))
        answer,addr=s.recvfrom(256)
        require(addr[0]==ip and answer==payload,"UDP_ECHO_PAYLOAD_MISMATCH")
    return True

def decode_hb(line,token):
    prefix="G10H_HEARTBEAT:"+token+" "
    if not line.startswith(prefix):return None
    values={}
    for part in line[len(prefix):].split():
        key,sep,value=part.partition("=")
        require(sep and value.isdecimal() and key not in values,
                "HEARTBEAT_MALFORMED")
        values[key]=int(value)
    required={"HB","ETH","TCP","SD_OK","SD_FAIL","RTC","FRAM",
              "UDP_RX","UDP_TX","RTU_OK","RTU_FAIL","BTN_EVENTS"}
    require(set(values)==required,"HEARTBEAT_FIELDS_DRIFT")
    return values

def validate_result(measure,rtu):
    latest=measure.get("last_hb") or {}
    require(measure["heartbeat_lines"]>=12 and latest.get("HB",0)>=12,
            "HEARTBEAT_CADENCE_LOST")
    require(latest.get("ETH")==1 and latest.get("TCP")==1,
            "ETHERNET_OR_MODBUS_SERVER_NOT_READY")
    require(latest.get("SD_OK",0)>=5 and latest.get("SD_FAIL",1)==0,
            "SD_READONLY_FAIRNESS_FAILED")
    require(latest.get("RTC")==1 and latest.get("FRAM",0)>=5,
            "RTC_OR_FRAM_NOT_ACTIVE")
    require(measure["tcp_ok"]>=8 and measure["udp_ok"]>=8,
            "TCP_UDP_NOT_SUSTAINED")
    require(latest.get("UDP_RX",0)>=8 and latest.get("UDP_TX",0)>=8,
            "UDP_FIRMWARE_NOT_ECHOING")
    require(latest.get("BTN_EVENTS",0)>=2,"BUTTON_NOT_PRESSED_RELEASED")
    if rtu:
        require(latest.get("RTU_OK",0)>=8 and latest.get("RTU_FAIL",1)==0,
                "MODBUS_RTU_CROSS_TRAFFIC_FAILED")
        slave=measure.get("slave_result") or {}
        require(slave.get("REQUESTS",0)>=8 and slave.get("CRC",-1)==0 and
                slave.get("READY",0)==1,
                "SLAVE_COUNTERS_NOT_PROVEN")
    return "PASS_DUAL_ETHERNET_SPI_RTU" if rtu else "PASS_SINGLE_ETHERNET_SPI_RTU_NOT_TESTED"

def monitor(rtu):
    phase("PHYSICAL_TRAFFIC")
    token=uuid.uuid4().hex[:24]
    STATE["session_token"]=token
    master=serial_open(MASTER)
    slave=serial_open(SLAVE) if rtu else None
    ports={"COM4":master}
    if slave:ports["COM14"]=slave
    buffers={p:bytearray() for p in ports}
    logfile={p:(OUT/("SERIAL_"+p+".log")).open("w",encoding="utf-8") for p in ports}
    measure={"tcp_ok":0,"tcp_fail":0,"udp_ok":0,"udp_fail":0,
             "heartbeat_lines":0,"last_hb":None,"slave_result":None}
    try:
        ready_ip=None; slave_ready=not rtu
        deadline=time.monotonic()+70
        while time.monotonic()<deadline and (not ready_ip or not slave_ready):
            for port,ch in ports.items():
                for line in serial_lines(ch,buffers[port]):
                    logfile[port].write(line+"\n");logfile[port].flush()
                    if port=="COM4":
                        found=re.fullmatch(
                            r"G10H_READY:IP=(\d{1,3}(?:\.\d{1,3}){3}) TCP=1 UDP=1 RTU=1",
                            line)
                        if found and found.group(1)!="0.0.0.0":
                            ready_ip=found.group(1)
                    if port=="COM14" and line=="G8_P4_READY:SLAVE:ID2:115200:8N1":
                        slave_ready=True
            time.sleep(0.02)
        require(ready_ip is not None,"DHCP_OR_ETHERNET_READY_TIMEOUT")
        require(slave_ready,"SLAVE_COM14_NOT_READY")
        STATE["board_ip"]=ready_ip
        emit("READY","PASS_IP_"+ready_ip)
        msg=f"G10H_START:{token}:RTU={int(rtu)}\n"
        master.write(msg.encode("ascii"));master.flush()
        started=False
        latest=0.0
        end=time.monotonic()+SECONDS
        iteration=0
        print("Prueba en marcha: pulsa y suelta UNA tecla en COM4.",flush=True)
        while time.monotonic()<end:
            for port,ch in ports.items():
                for line in serial_lines(ch,buffers[port]):
                    logfile[port].write(line+"\n");logfile[port].flush()
                    if port=="COM4":
                        if line.startswith("G10H_STARTED:"+token):started=True
                        parsed=decode_hb(line,token)
                        if parsed is not None:
                            measure["heartbeat_lines"]+=1
                            measure["last_hb"]=parsed
            now=time.monotonic()
            if started and now-latest>=0.8:
                latest=now
                iteration+=1
                try:
                    check_fc03(ready_ip,iteration&0xffff)
                    measure["tcp_ok"]+=1
                except (OSError,RuntimeError):
                    measure["tcp_fail"]+=1
                try:
                    check_udp(ready_ip,token,iteration)
                    measure["udp_ok"]+=1
                except (OSError,RuntimeError):
                    measure["udp_fail"]+=1
                if iteration%6==0:
                    emit("PROGRESS",f"TCP={measure['tcp_ok']} UDP={measure['udp_ok']} HB={measure['heartbeat_lines']}")
            time.sleep(0.02)
        require(started,"START_SESSION_TOKEN_NOT_ACKNOWLEDGED")
        if slave:
            slave.write(("G8_P4_REPORT:"+token+"\n").encode("ascii"))
            slave.flush()
            for _ in range(110):
                for line in serial_lines(slave,buffers["COM14"]):
                    logfile["COM14"].write(line+"\n");logfile["COM14"].flush()
                    pref="G8_P4_SLAVE_RESULT:"+token+" "
                    if line.startswith(pref):
                        pairs={}
                        for part in line[len(pref):].split():
                            k,v=part.split("=",1)
                            pairs[k]=int(v)
                        measure["slave_result"]=pairs
                if measure["slave_result"]:break
                time.sleep(0.03)
        STATE["measurements"]=measure
        STATE["physical_status"]=validate_result(measure,rtu)
        emit("RESULT",STATE["physical_status"])
    finally:
        for f in logfile.values():f.close()
        for ch in ports.values():ch.close()
        save()

def selftest():
    data=struct.pack(">HHHBBHH",123,0,6,1,3,0,4)
    require(len(data)==12 and
            struct.unpack(">HHHBBHH",data)==(123,0,6,1,3,0,4),
            "SELFTEST_MBAP")
    hb="G10H_HEARTBEAT:"+("a"*24)+" HB=1 ETH=1 TCP=1 SD_OK=1 SD_FAIL=0 RTC=1 FRAM=1 UDP_RX=0 UDP_TX=0 RTU_OK=0 RTU_FAIL=0 BTN_EVENTS=0"
    require(decode_hb(hb,"a"*24)["RTC"]==1,"SELFTEST_HEARTBEAT")
    print("G10_H_SELFTEST=PASS_NO_HARDWARE_NO_UPLOAD")
    return 0

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--rtu",action="store_true",
                        help="Incluye Slave COM14; sube sketch G8-P4 a ambas placas")
    parser.add_argument("--selftest",action="store_true",
                        help="Solo chequeo host; no toca COM, Git ni Arduino")
    args=parser.parse_args()
    if args.selftest:return selftest()
    global OUT
    OUT=ROOT/"tools/alpha13/results"/(
        "g10_h_"+datetime.now().strftime("%Y%m%d_%H%M%S")+
        "_"+uuid.uuid4().hex[:8])
    OUT.mkdir(parents=True,exist_ok=False)
    emit("RESULT_DIR",OUT)
    result=2
    try:
        cli=preflight(args.rtu)
        with tempfile.TemporaryDirectory(prefix="jwplc-g10h-") as scratch:
            tmp=Path(scratch)
            config,platform=isolated_platform(tmp,cli)
            builds=build(cli,config,platform,tmp,args.rtu)
            authorize(args.rtu)
            upload(cli,config,builds,args.rtu)
            monitor(args.rtu)
        phase("DONE")
        result=0
    except Exception as exc:
        STATE["reason"]=str(exc)
        STATE["physical_status"]="REVIEW_STOP"
        emit("REVIEW_STOP",str(exc))
        result=2
    finally:
        STATE["exit_code"]=result
        save()
        emit("SUMMARY_PATH",OUT/"SUMMARY.log")
        emit("MANIFEST_PATH",OUT/"MANIFEST.json")
    return result

if __name__=="__main__":
    sys.exit(main())
