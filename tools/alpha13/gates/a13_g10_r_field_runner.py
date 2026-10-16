#!/usr/bin/env python3
"""G10-R campo: R1/R2/R4 con consentimiento y captura, NUNCA hace flash.

--dry-run / --selftest: sin COM, red ni hardware.
--live: necesita evidencia EXTERNA de build+upload verificada, COM4/COM14
CH340, protocolo G10-R en la placa y confirmacion interactiva.
Los logs UART/PC se conservan incluso con REVIEW_STOP. Un PASS de replay
NO se declara adopcion ni certificacion industrial. No ejecutar en maquinas.
"""
from __future__ import annotations

import argparse
from datetime import datetime
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import queue
import re
import socket
import struct
import tempfile
import sys
import threading
import time
import uuid

ROOT=Path(__file__).resolve().parents[3]
PORTS=("COM4","COM14")
PROTOCOL="G10R_PROTOCOL:R1R2R4_R3SNAPSHOT_V1"
FQBN="jwplc_hbench:esp32:jwplcbasic"
CH340=(0x1a86,0x7523)
BAUD=115200
R1_SECONDS=605
R4_SECONDS=1805
R2_FAULT_SECONDS=12
R2_RECOVERY_SECONDS=96
AUTH="G10R AUTORIZO CAPTURA COM4 COM14 Y MANIOBRAS RJ45"
READY_RE=re.compile(r"^G10R_READY:IP=(\d{1,3}(?:\.\d{1,3}){3}) TCP=1 UDP=1 RTU=1 BOOT=([0-9a-f]+) LINK=([012]) VER=R1R2R4_R3SNAPSHOT_V1$")
HB_RE=re.compile(r"^G10R_HEARTBEAT:([0-9a-f]{24}) (.*)$")
FIELDS={"HB":"hb","BOOT":"boot","IP":"ip","LINK":"link","ETH":"eth",
        "TCP":"tcp","SD_OK":"sd_ok","SD_FAIL":"sd_fail","RTC":"rtc",
        "FRAM":"fram","RTU_OK":"rtu_ok","RTU_FAIL":"rtu_fail"}


def require(ok,reason):
    if not ok:raise RuntimeError(reason)


def inspect_evidence(path:Path)->dict:
    """El manifiesto es una PRECONDICIÓN documental, NO attestation remota."""
    d=json.loads(path.read_text(encoding="utf-8-sig"))
    require(d.get("schema")=="G10-R-AUTHORIZED-UPLOAD-V1",
            "UPLOAD_EVIDENCE_SCHEMA_MISSING")
    require(d.get("fqbn")==FQBN,"UPLOAD_EVIDENCE_WRONG_FQBN")
    require(d.get("ports")==["COM4","COM14"],
            "UPLOAD_EVIDENCE_WRONG_PORTS")
    require(d.get("uploaded") == {"COM4":"PASS_CONFIRMED_UPLOAD",
                                 "COM14":"PASS_CONFIRMED_UPLOAD"},
            "UPLOAD_EVIDENCE_NO_CONFIRMED_BOTH")
    sha=d.get("elf_sha256",{})
    require(isinstance(sha,dict) and set(sha)==set(PORTS) and all(
        isinstance(sha[p],str) and re.fullmatch("[a-f0-9]{64}",sha[p])
        for p in PORTS),"UPLOAD_EVIDENCE_ELF_HASH_MISSING")
    require(d.get("gpio4_reset")=="DISABLED"
            and d.get("product_modified") is False,
            "UPLOAD_EVIDENCE_UNSAFE")
    require(d.get("upload")=="PASS_BOTH_EXPLICIT" and
            d.get("consent")=="EXPLICIT_AFTER_SOURCE_FIRST_BUILD",
            "UPLOAD_EVIDENCE_NO_EXPLICIT_CONSENT")
    require(d.get("g10_i_source_sha")==
            "a954cabd9c6e37876b59995a41ff0a449308ccd1",
            "UPLOAD_EVIDENCE_DIFFERENT_CANDIDATE")
    require(re.fullmatch(r"[a-f0-9]{40}",d.get("source_head","")) is not None,
            "UPLOAD_EVIDENCE_HEAD_REQUIRED")
    require(d.get("r3_lab_snapshot")==
            "READ_ONLY_INSTRUMENTED_NOT_FAULT_INJECTED",
            "UPLOAD_EVIDENCE_R3_LAB_MISSING")
    meta=d.get("port_metadata",{})
    require(isinstance(meta,dict) and set(meta)==set(PORTS) and all(
        meta[p].get("vid")==CH340[0] and meta[p].get("pid")==CH340[1]
        for p in PORTS),"UPLOAD_EVIDENCE_USB_IDENTITY_MISSING")
    for port in PORTS:
        log=path.parent/("UPLOAD_"+port+".log")
        require(log.is_file(),"UPLOAD_EVIDENCE_LOG_MISSING_"+port)
        require(hashlib.sha256(log.read_bytes()).hexdigest()==
                d.get("upload_log_"+port+"_sha256"),
                "UPLOAD_EVIDENCE_LOG_HASH_DRIFT_"+port)
    # Reconciliación documental: no es prueba criptográfica de la memoria.
    return d


def parse_heartbeat(line:str,token:str,phase:str,t:float):
    m=HB_RE.fullmatch(line)
    if not m or m.group(1)!=token:return None
    parts=m.group(2).split()
    values={}
    for part in parts:
        k,sep,v=part.partition("=")
        if not sep or k in values:return None
        values[k]=v
    required=set(FIELDS)|{"UDP_RX","UDP_TX","BTN_EVENTS"}
    if set(values)!=required:return None
    d={"phase":phase,"t":t,"kind":"heartbeat"}
    for k,v in values.items():
        field=FIELDS.get(k)
        if field is None:continue
        if k in ("BOOT","IP"):d[field]=v
        else:
            if not v.isdecimal():return None
            d[field]=int(v)
    if d["link"] not in (0,1,2):return None
    if d["eth"] not in (0,1) or d["tcp"] not in (0,1) or d["rtc"] not in (0,1):
        return None
    if not re.fullmatch(r"[a-f0-9]{4,8}",d["boot"]):return None
    if not re.fullmatch(r"\d{1,3}(?:\.\d{1,3}){3}",d["ip"]):
        return None
    return d



def parse_slave_result(line:str,token:str):
    prefix="G8_P4_SLAVE_RESULT:"+token+" "
    if not line.startswith(prefix):return None
    values={}
    for part in line[len(prefix):].split():
        name,sep,value=part.partition("=")
        if not sep or name in values or not value.isdecimal():
            return None
        values[name]=int(value)
    expected={"RXBYTES","RXFRAMES","TXFRAMES","REQUESTS","CRC","READY"}
    if set(values)!=expected:return None
    return values


def recv_exact(s, count:int)->bytes:
    buf=bytearray()
    while len(buf)<count:
        fragment=s.recv(count-len(buf))
        if not fragment:raise OSError("TCP_EOF")
        buf.extend(fragment)
    return bytes(buf)


def tcp_fc03(ip:str,tid:int)->bool:
    packet=struct.pack(">HHHBBHH",tid&0xffff,0,6,1,3,0,4)
    with socket.create_connection((ip,502),timeout=1.2) as conn:
        conn.settimeout(1.2)
        conn.sendall(packet)
        header=recv_exact(conn,7)
        rid,pid,length,unit=struct.unpack(">HHHB",header)
        require((rid,pid,length,unit)==(tid&0xffff,0,11,1),
                "BAD_MBAP")
        pdu=recv_exact(conn,length-1)
        require(pdu==b"\x03\x08\x12\x34\x23\x45\x34\x56\x45\x67",
                "BAD_FC03")
    return True


def udp_echo(ip:str,token:str,seq:int)->bool:
    data=("G10R."+token+"."+str(seq)).encode("ascii")
    with socket.socket(socket.AF_INET,socket.SOCK_DGRAM) as sock:
        sock.settimeout(1.2)
        sock.sendto(data,(ip,15020))
        payload,src=sock.recvfrom(256)
        require(src[0]==ip and payload==data,"BAD_UDP_ECHO")
    return True


class Recorder:
    def __init__(self,root:Path):
        self.root=root
        self.stop=threading.Event()
        self.lock=threading.RLock()
        self.events=[]
        self.phase=""
        self.token=""
        self.latest=None
        self.boot=None
        self.ready_ip=None
        self.protocol_seen=False
        self.slave_ready=False
        self.slave_report_token=""
        self.slave_result=None
        self.ports={}
        self.threads=[]
        self.origin=time.monotonic()
        self.raw_handles={}

    def stamp(self)->float:
        return time.monotonic()-self.origin

    def add(self,event):
        with self.lock:self.events.append(event)

    def port_reader(self,name,ch):
        raw=self.raw_handles[name]
        while not self.stop.is_set():
            try:
                data=ch.readline()
            except Exception:
                self.add({"kind":"fault","t":self.stamp(),"port":name,
                          "reason":"SERIAL_READ_ERROR"})
                self.stop.set()
                break
            if not data:continue
            line=data.decode("utf-8","replace").strip()
            at=self.stamp()
            with self.lock:
                raw.write(f"{at:.3f} {line}\n")
                raw.flush()
                if name=="COM4":
                    if line==PROTOCOL:self.protocol_seen=True
                    ready=READY_RE.fullmatch(line)
                    if ready:
                        self.ready_ip=ready.group(1)
                        self.boot=ready.group(2)
                        self.protocol_seen=True
                    if self.phase and self.token:
                        hb=parse_heartbeat(line,self.token,self.phase,at)
                        if hb:
                            self.latest=hb
                            self.events.append(hb)
                elif name=="COM14":
                    if line=="G8_P4_READY:SLAVE:ID2:115200:8N1":
                        self.slave_ready=True
                    if self.slave_report_token:
                        parsed=parse_slave_result(line,self.slave_report_token)
                        if parsed is not None:self.slave_result=parsed

    def open(self):
        from serial.tools import list_ports
        from serial import Serial
        found={}
        for name in PORTS:
            matches=[p for p in list_ports.comports() if p.device.upper()==name]
            require(len(matches)==1,"USB_PORT_MISSING_"+name)
            p=matches[0]
            require((p.vid,p.pid)==CH340,"USB_IDENTITY_NOT_CH340_"+name)
            found[name]={"vid":p.vid,"pid":p.pid,"description":p.description}
        for name in PORTS:
            ch=Serial(port=None,baudrate=BAUD,timeout=0.07,write_timeout=1,
                      rtscts=False,dsrdtr=False)
            ch.dtr=False;ch.rts=False;ch.port=name;ch.open()
            self.ports[name]=ch
            self.raw_handles[name]=(self.root/("SERIAL_"+name+".log")).open(
                "w",encoding="utf-8")
            worker=threading.Thread(target=self.port_reader,args=(name,ch),
                                    daemon=True)
            worker.start();self.threads.append(worker)
        return found

    def close(self):
        self.stop.set()
        for thr in self.threads:thr.join(timeout=2)
        for ch in self.ports.values():
            try:ch.close()
            except Exception:pass
        for stream in self.raw_handles.values():stream.close()

    def send(self,message):
        self.ports["COM4"].write((message+"\n").encode("ascii"))
        self.ports["COM4"].flush()

    def start(self,phase):
        with self.lock:
            self.phase=phase
            self.token=uuid.uuid4().hex[:24]
            self.latest=None
            token=self.token
        self.send("G10R_START:"+token+":RTU=1")
        return token

    def end(self):
        if self.token:self.send("G10R_STOP:"+self.token)
        with self.lock:
            self.phase=""
            self.token=""
            self.latest=None

    def traffic_worker(self,phase,interval,halt:threading.Event):
        seq=0
        prev_rtu=None
        while not halt.is_set() and not self.stop.is_set():
            with self.lock:
                latest=dict(self.latest) if self.latest else None
                token=self.token
                ip=(latest or {}).get("ip") or self.ready_ip
                rtu_value=(latest or {}).get("rtu_ok")
                rtu_fail=(latest or {}).get("rtu_fail")
            # La primera lectura aún no tiene heartbeat/RTU diferencial.
            # No crear una muestra "fallida" ficticia por falta de sesión.
            if latest is None or rtu_value is None:
                halt.wait(.2)
                continue
            if prev_rtu is None:
                prev_rtu=rtu_value
                halt.wait(min(.6,interval))
                continue
            seq+=1
            tcp_ok=udp_ok=False
            if ip and ip!="0.0.0.0":
                try:tcp_ok=tcp_fc03(ip,seq)
                except (OSError,RuntimeError):pass
                try:udp_ok=udp_echo(ip,token,seq)
                except (OSError,RuntimeError):pass
            # Evidencia RTU por incremento del contador real COM4.
            rtu_ok=bool(rtu_value is not None and prev_rtu is not None and
                        rtu_value>prev_rtu and rtu_fail==0)
            if rtu_value is not None:prev_rtu=rtu_value
            self.add({"kind":"traffic","phase":phase,"t":self.stamp(),
                      "tcp":tcp_ok,"udp":udp_ok,"rtu":rtu_ok})
            halt.wait(interval)

    def measure(self,phase,duration:int,interval:int):
        self.start(phase)
        halt=threading.Event()
        task=threading.Thread(target=self.traffic_worker,
                              args=(phase,interval,halt),daemon=True)
        task.start()
        try:
            if self.stop.wait(duration):
                raise RuntimeError("SERIAL_WORKER_ABORT_DURING_"+phase)
        finally:
            halt.set();task.join(timeout=5);self.end()

    def request_slave_result(self):
        token=uuid.uuid4().hex[:24]
        with self.lock:self.slave_report_token=token
        self.ports["COM14"].write(("G8_P4_REPORT:"+token+"\n").encode("ascii"))
        self.ports["COM14"].flush()
        deadline=time.monotonic()+6
        while time.monotonic()<deadline:
            with self.lock:
                result=dict(self.slave_result) if self.slave_result else None
            if result:return result
            if self.stop.wait(.1):break
        raise RuntimeError("SLAVE_RESULT_TIMEOUT_OR_SERIAL_LOST")

    def link_cycles(self):
        self.start("R2")
        halt=threading.Event()
        task=threading.Thread(target=self.traffic_worker,
                             args=("R2",2,halt),daemon=True)
        task.start()
        try:
            for cycle in range(1,4):
                print(f"G10_R_R2_CYCLE={cycle}: RETIRA manualmente solo RJ45 de COM4; ENTER tras retirarlo.")
                input("Confirmar RJ45 retirado: ")
                self.add({"kind":"link_down","phase":"R2","t":self.stamp()})
                if self.stop.wait(R2_FAULT_SECONDS):
                    raise RuntimeError("SERIAL_WORKER_ABORT_R2_FAULT")
                print("RECONÉCTALO al mismo switch de banco; ENTER tras reconectar.")
                input("Confirmar RJ45 reconectado: ")
                self.add({"kind":"link_up","phase":"R2","t":self.stamp()})
                if self.stop.wait(R2_RECOVERY_SECONDS):
                    raise RuntimeError("SERIAL_WORKER_ABORT_R2_RECOVERY")
        finally:
            halt.set();task.join(timeout=5);self.end()


def plan()->dict:
    return {"mode":"PREPARATION","hardware":"NOT_ACCESSED",
      "flash":"NOT_IMPLEMENTED_NOT_AUTHORIZED","gpio4":"DISABLED",
      "requires":"EXTERNAL_CONFIRMED_UPLOAD_EVIDENCE_AND_INTERACTIVE_CONSENT",
      "phases":{"R1_seconds":R1_SECONDS,"R2_cycles":3,
                "R2_fault_seconds":R2_FAULT_SECONDS,
                "R4_seconds":R4_SECONDS},
      "R3":"SEPARATE_SOURCE_INSTRUMENTATION_NOT_QUALIFIED_BY_R1_R4"}


def selftest()->int:
    sample=("G10R_HEARTBEAT:"+"a"*24+
      " HB=10 BOOT=abc123 IP=192.0.2.4 LINK=2 ETH=1 TCP=1"
      " SD_OK=10 SD_FAIL=0 RTC=1 FRAM=10 UDP_RX=4 UDP_TX=4"
      " RTU_OK=8 RTU_FAIL=0 BTN_EVENTS=2")
    ev=parse_heartbeat(sample,"a"*24,"R2",12.4)
    require(ev and ev["link"]==2 and ev["rtu_ok"]==8,
            "TEST_HEARTBEAT_POSITIVE")
    neg=[
        ("wrong_token",sample.replace("a"*24,"b"*24)),
        ("missing_ip",sample.replace(" IP=192.0.2.4","")),
        ("duplicate_link",sample+" LINK=1"),
        ("invalid_link",sample.replace("LINK=2","LINK=3")),
        ("wrong_field",sample.replace("RTU_OK=8","RTU_XX=8")),
        ("wrong_counter",sample.replace("SD_OK=10","SD_OK=abc")),
    ]
    for label,line in neg:
        require(parse_heartbeat(line,"a"*24,"R2",12.4) is None,
                "NEGATIVE_NOT_REJECTED_"+label)
    expected_ready=("G10R_READY:IP=192.0.2.4 TCP=1 UDP=1 RTU=1"
                    " BOOT=abc123 LINK=1 VER=R1R2R4_R3SNAPSHOT_V1")
    require(READY_RE.fullmatch(expected_ready) is not None,
            "READY_PERIODIC_PROTOCOL_MISSING")
    require(READY_RE.fullmatch(expected_ready.replace("VER=R1R2R4_R3SNAPSHOT_V1",
                                                     "VER=LEGACY")) is None,
            "READY_STALE_FIRMWARE_ACCEPTED")
    with tempfile.TemporaryDirectory(prefix="g10r-evidence-synth-") as d:
        root=Path(d)
        evidence={
            "schema":"G10-R-AUTHORIZED-UPLOAD-V1",
            "fqbn":FQBN,"ports":["COM4","COM14"],
            "uploaded":{"COM4":"PASS_CONFIRMED_UPLOAD",
                        "COM14":"PASS_CONFIRMED_UPLOAD"},
            "elf_sha256":{"COM4":"a"*64,"COM14":"b"*64},
            "gpio4_reset":"DISABLED","product_modified":False,
            "upload":"PASS_BOTH_EXPLICIT",
            "consent":"EXPLICIT_AFTER_SOURCE_FIRST_BUILD",
            "g10_i_source_sha":"a954cabd9c6e37876b59995a41ff0a449308ccd1",
            "source_head":"c"*40,
            "r3_lab_snapshot":"READ_ONLY_INSTRUMENTED_NOT_FAULT_INJECTED",
            "port_metadata":{"COM4":{"vid":CH340[0],"pid":CH340[1]},
                             "COM14":{"vid":CH340[0],"pid":CH340[1]}}
        }
        for port in PORTS:
            log=root/("UPLOAD_"+port+".log")
            log.write_text("SYNTHETIC_TEST_NO_HARDWARE",encoding="utf-8")
            evidence["upload_log_"+port+"_sha256"]=hashlib.sha256(
                log.read_bytes()).hexdigest()
        doc=root/"MANIFEST.json"
        doc.write_text(json.dumps(evidence),encoding="utf-8")
        require(inspect_evidence(doc)["schema"]==evidence["schema"],
                "TEST_EVIDENCE_POSITIVE")
        evidence["uploaded"]["COM4"]="NOT_RUN"
        doc.write_text(json.dumps(evidence),encoding="utf-8")
        try:inspect_evidence(doc)
        except RuntimeError:pass
        else:raise AssertionError("TEST_EVIDENCE_PARTIAL_UPLOAD_ACCEPTED")
    slave=("G8_P4_SLAVE_RESULT:"+"a"*24+
           " RXBYTES=99 RXFRAMES=12 TXFRAMES=12 REQUESTS=12 CRC=0 READY=1")
    require(parse_slave_result(slave,"a"*24)["CRC"]==0,"SLAVE_PARSE_POSITIVE")
    require(parse_slave_result(slave.replace("CRC=0","CRC=broken"),"a"*24) is None,
            "SLAVE_PARSE_NEGATIVE_NOT_REJECTED")
    class _FakeCOM14:
        def __init__(self):self.sent=[]
        def write(self,data):self.sent.append(data)
        def flush(self):pass
    fake=_FakeCOM14()
    rec=Recorder(Path("."))
    rec.ports["COM14"]=fake
    rec.slave_result={"RXBYTES":99,"RXFRAMES":12,"TXFRAMES":12,
                      "REQUESTS":12,"CRC":0,"READY":1}
    require(rec.request_slave_result()["REQUESTS"]==12 and
            len(fake.sent)==1 and fake.sent[0].startswith(b"G8_P4_REPORT:") and
            fake.sent[0].endswith(b"\\n"),
            "SLAVE_REPORT_COMMAND_TERMINATOR_REQUIRED")
    require(plan()["hardware"]=="NOT_ACCESSED",
            "DRY_RUN_IMPLICIT_HARDWARE")
    print("G10_R_FIELD_SELFTEST=PASS_6_NEGATIVE_SERIAL_CASES")
    print("G10_R_FIELD_HARDWARE=NOT_ACCESSED")
    return 0


def live(evidence:Path)->int:
    require(os.name=="nt","WINDOWS_ONLY")
    proven=inspect_evidence(evidence)
    # El archivo aportado se considera EVIDENCIA EXTERNA; NO confirma
    # por sí mismo el contenido actual de la flash del microcontrolador.
    print("AVISO: carga de G10-R ya confirmada por evidencia externa, "
          "sin rollback automático. Este script NO sube firmware.")
    print("Banco SIN actuadores o cargas en Q; COM4/COM14 conectados a RS485.")
    print("R2 retirará/reinsertará RJ45 solo con acción manual por operador.")
    require(input("Escribe exactamente '"+AUTH+"': ").strip()==AUTH,
            "PHYSICAL_CONSENT_REQUIRED")
    out=ROOT/"tools/alpha13/results"/("g10_r_capture_"+
        datetime.now().strftime("%Y%m%d_%H%M%S")+"_"+uuid.uuid4().hex[:8])
    out.mkdir(parents=True,exist_ok=False)
    c=Recorder(out)
    result={"schema":"G10-R-REPLAY-V1",
            "origin":"PC_UART_CAPTURE_UNVERIFIED","events":[],
            "upload_evidence_sha256":hashlib.sha256(evidence.read_bytes()).hexdigest(),
            "physical_status":"NOT_ATTESTED_UNTIL_REVIEW"}
    status=2
    try:
        result["current_ports"]=c.open()
        deadline=time.monotonic()+45
        ready=False
        while time.monotonic()<deadline:
            with c.lock:
                ready=c.ready_ip is not None and c.protocol_seen and c.slave_ready
            if ready:break
            time.sleep(.1)
        require(ready,"G10_R_EXPECTED_FIRMWARE_PROTOCOL_NOT_FOUND")
        c.measure("R1",R1_SECONDS,5)
        c.link_cycles()
        c.measure("R4",R4_SECONDS,5)
        result["slave_result"]=c.request_slave_result()
        require(result["slave_result"]["CRC"]==0 and
                result["slave_result"]["READY"]==1 and
                result["slave_result"]["REQUESTS"]>=8,
                "SLAVE_CRC_OR_REQUESTS_FAILED")
        # Correlación y validación offline. No declara PASS físico automático.
        spec=importlib.util.spec_from_file_location("g10r_replay",
                 ROOT/"tools/alpha13/gates/a13_g10_r_replay.py")
        replay=importlib.util.module_from_spec(spec)
        sys.modules[spec.name]=replay;spec.loader.exec_module(replay)
        result["events"]=sorted(c.events,key=lambda ev:ev["t"])
        verdict=replay.evaluate(result)
        result["replay_verdict"]=verdict
        result["physical_status"]="EVIDENCE_COLLECTED_REQUIRES_REVIEW"
        status=0 if verdict["status"]=="PASS_REPLAY_CONTRACT" else 2
    except Exception as exc:
        result["physical_status"]="REVIEW_STOP"
        result["reason"]=str(exc)
        status=2
    finally:
        c.close()
        result["events"]=sorted(c.events,key=lambda ev:ev["t"])
        result["exit_code"]=status
        (out/"MANIFEST.json").write_text(
            json.dumps(result,ensure_ascii=False,indent=2)+"\n",encoding="utf-8")
        print("G10_R_CAPTURE_DIR="+str(out))
        print("G10_R_CAPTURE_STATUS="+result["physical_status"])
    return status


def main()->int:
    p=argparse.ArgumentParser(description=__doc__)
    modes=p.add_mutually_exclusive_group(required=True)
    modes.add_argument("--selftest",action="store_true")
    modes.add_argument("--dry-run",action="store_true")
    modes.add_argument("--live",action="store_true")
    p.add_argument("--upload-evidence",type=Path)
    args=p.parse_args()
    try:
        if args.selftest:return selftest()
        if args.dry_run:
            print(json.dumps(plan(),ensure_ascii=False,indent=2))
            return 0
        require(args.upload_evidence is not None,
                "LIVE_REQUIRES_PREVIOUS_UPLOAD_EVIDENCE_NOT_GENERATED_BY_THIS_RUNNER")
        return live(args.upload_evidence)
    except (OSError,ValueError,RuntimeError,EOFError) as exc:
        print("G10_R_REVIEW_STOP="+str(exc))
        return 2


if __name__=="__main__":
    sys.exit(main())
