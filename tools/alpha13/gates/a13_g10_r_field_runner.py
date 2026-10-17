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
import subprocess
import tempfile
import sys
import threading
import time
import uuid

ROOT=Path(__file__).resolve().parents[3]
PORTS=("COM4","COM14")
PROTOCOL="G10R_PROTOCOL:R1R2R3R4_FAULT_LAB_V2"
FQBN="jwplc_hbench:esp32:jwplcbasic"
CH340=(0x1a86,0x7523)
BAUD=115200
R1_SECONDS=605
R4_SECONDS=1805
R2_FAULT_SECONDS=12
R2_RECOVERY_SECONDS=96
AUTH="G10R AUTORIZO CAPTURA COM4 COM14 Y MANIOBRAS RJ45"
# Estado intencionalmente fail-closed: R3 no cuenta con inyector físico
# source-backed aprobado ni orquestación dedicada. R1/R2/R4 no lo sustituyen.
R3_PHYSICAL_INJECTION_READY=False
# Las cinco sondas pertenecen a UNA misma campaña H, no a uploads distintos.
R3_PROBE_ORDER=("UDP","DNS","TCP","DHCP_CANCEL","DHCP_FINISH")
READY_RE=re.compile(r"^G10R_READY:IP=(\d{1,3}(?:\.\d{1,3}){3}) TCP=1 UDP=1 RTU=1 BOOT=([0-9A-Fa-f]+) LINK=([012]) VER=R1R2R3R4_FAULT_LAB_V2$")
HB_RE=re.compile(r"^G10R_HEARTBEAT:([0-9a-f]{24}) (.*)$")
FIELDS={"HB":"hb","BOOT":"boot","IP":"ip","LINK":"link","ETH":"eth",
        "TCP":"tcp","SD_OK":"sd_ok","SD_FAIL":"sd_fail","RTC":"rtc",
        "DHCP_STATE":"dhcp_state","DHCP_ERR":"dhcp_err",
        "DHCP_TICKS":"dhcp_ticks","DHCP_PEND":"dhcp_pend",
        "DHCP_RENEW_OK":"dhcp_renew_ok","DHCP_RENEW_FAIL":"dhcp_renew_fail",
        "DHCP_REBIND_OK":"dhcp_rebind_ok","DHCP_REBIND_FAIL":"dhcp_rebind_fail",
        "IO_READY":"io_ready","IO_AGE_MS":"io_age_ms",
        "RTC_AGE_MS":"rtc_age_ms","TFT_READY":"tft_ready",
        "FRAM":"fram","RTU_OK":"rtu_ok","RTU_FAIL":"rtu_fail"}


def require(ok,reason):
    if not ok:raise RuntimeError(reason)


def current_source_head()->str:
    """Identidad local de las fuentes, sin interacción de red."""
    result=subprocess.run(["git","rev-parse","HEAD"],cwd=ROOT,
                          capture_output=True,text=True,timeout=12)
    require(result.returncode==0,"G10R_LOCAL_GIT_HEAD_NOT_AVAILABLE")
    head=result.stdout.strip()
    require(re.fullmatch("[a-f0-9]{40}",head) is not None,
            "G10R_LOCAL_GIT_HEAD_INVALID")
    require(subprocess.run(["git","diff","--quiet","--"],cwd=ROOT,
                           timeout=12).returncode==0,
            "G10R_TRACKED_WORKTREE_DIRTY")
    require(subprocess.run(["git","diff","--cached","--quiet","--"],cwd=ROOT,
                           timeout=12).returncode==0,
            "G10R_STAGED_WORKTREE_DIRTY")
    return head


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
    require(d["source_head"]==current_source_head(),
            "UPLOAD_EVIDENCE_NOT_FROM_CURRENT_SOURCE_HEAD")
    require(d.get("r3_lab_snapshot")==
            "READ_ONLY_INSTRUMENTED_NOT_FAULT_INJECTED",
            "UPLOAD_EVIDENCE_R3_LAB_MISSING")
    sk=d.get("sketch_sha256",{})
    expected_sources={
        "COM4":ROOT/"tools/alpha13/sketches/g10_r_integrated/g10_r_integrated.ino",
        "COM14":ROOT/"tools/alpha13/sketches/g8_p4_slave/g8_p4_slave.ino",
    }
    require(isinstance(sk,dict) and set(sk)==set(PORTS) and all(
        isinstance(sk[p],str) and sk[p]==hashlib.sha256(
            expected_sources[p].read_bytes()).hexdigest()
        for p in PORTS),"UPLOAD_EVIDENCE_CURRENT_SKETCH_SHA_DRIFT")
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
        if k=="BOOT":d[field]=v.lower()
        elif k=="IP":d[field]=v
        else:
            if not v.isdecimal():return None
            d[field]=int(v)
    if d["link"] not in (0,1,2):return None
    if d["eth"] not in (0,1) or d["tcp"] not in (0,1) or d["rtc"] not in (0,1):
        return None
    if not re.fullmatch(r"[A-Fa-f0-9]{1,8}",d["boot"]):return None
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



def decode_socket_slot(line:str,token:str):
    """Snapshot source-backed; NO es un evento de CLOSE físico."""
    prefix="G10R_SOCK:"+token+" "
    if not line.startswith(prefix):return None
    fields={}
    for part in line[len(prefix):].split():
        key,sep,value=part.partition("=")
        if not sep or key in fields or not value.isdecimal():return None
        fields[key]=int(value)
    if set(fields)!={"SID","EPOCH","ALLOC","OWNER","PENDING","VALID"}:
        return None
    if not 0<=fields["SID"]<8 or fields["VALID"] not in (0,1):
        return None
    if fields["VALID"] and not (
        fields["EPOCH"]>0 and fields["ALLOC"]>0 and fields["OWNER"]>0):
        return None
    return {"socket":fields["SID"],"epoch":fields["EPOCH"],
            "allocation":fields["ALLOC"],"owner":fields["OWNER"],
            "pending":fields["PENDING"],"valid":fields["VALID"]}


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
        self.r3_snapshots=[]
        self.r3_udp_lines=[]  # Histórico compat: incluye ahora DNS/TCP con kind.
        self.r3_udp_sessions=[]
        self.tcp124_stress=[]
        self._r3_requested_mode=None
        self._r3_udp_started_at=None
        self._sock_batch=None
        spec=importlib.util.spec_from_file_location(
            "g10r_trace_collector",
            ROOT/"tools/alpha13/gates/a13_g10_r_trace_host.py")
        require(spec is not None and spec.loader is not None,
                "R3_TRACE_COLLECTOR_UNAVAILABLE")
        m=importlib.util.module_from_spec(spec)
        sys.modules[spec.name]=m
        spec.loader.exec_module(m)
        self.trace=m.TraceCollector()
        tcp_path=ROOT/"tools/alpha13/gates/a13_g10_r_tcp124_collector.py"
        tcp_spec=importlib.util.spec_from_file_location("g10r_tcp_collector",tcp_path)
        require(tcp_spec is not None and tcp_spec.loader is not None,
                "TCP124_COLLECTOR_MISSING")
        tcp_module=importlib.util.module_from_spec(tcp_spec)
        sys.modules[tcp_spec.name]=tcp_module
        tcp_spec.loader.exec_module(tcp_module)
        self.tcp_collector=tcp_module.TcpCollector()
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
                        self.boot=ready.group(2).lower()
                        self.protocol_seen=True
                    if self.phase and self.token:
                        hb=parse_heartbeat(line,self.token,self.phase,at)
                        if hb:
                            self.latest=hb
                            self.events.append(hb)
                        self.capture_socket_line(line,at)
                        self.capture_udp_probe_line(line,at)
                        self.trace.receive(line,at)
                        self.tcp_collector.receive(line,at)
                elif name=="COM14":
                    if line=="G8_P4_READY:SLAVE:ID2:115200:8N1":
                        self.slave_ready=True
                    if self.slave_report_token:
                        parsed=parse_slave_result(line,self.slave_report_token)
                        if parsed is not None:self.slave_result=parsed

    def capture_udp_probe_line(self,line,at):
        # UART COM4, solo de la sesión vigente; no atribuir datos ajenos.
        if not self.phase or not self.token:return
        if not line.startswith(("G10R_R3_UDP:","G10R_R3_DNS:",
                                "G10R_R3_TCP:","G10R_R3_DHCP_CANCEL:",
                                "G10R_R3_DHCP_FINISH:","G10R_R3_LEASE:",
                                "G10R_R3_HOLD:")):return
        kind=self._r3_requested_mode
        if line.startswith("G10R_R3_"):
            prefix=line.split(":",1)[0]
            if prefix in ("G10R_R3_UDP","G10R_R3_DNS",
                          "G10R_R3_TCP","G10R_R3_DHCP_CANCEL",
                          "G10R_R3_DHCP_FINISH"):
                kind=prefix[len("G10R_R3_"):]
        self.r3_udp_lines.append({
            "phase":self.phase,"token":self.token,"kind":kind,
            "t":at,"line":line})
        if (kind in ("UDP","DNS","TCP","DHCP_CANCEL","DHCP_FINISH") and
            line=="G10R_R3_"+kind+":"+self.token+" STATE=ARMED"):
            self._r3_udp_started_at=at

    def r3_udp_probe_request(self,kind="UDP"):
        """Un modo por sesión, solo en la fase R3; nunca durante R1/R2/R4."""
        require(kind in ("UDP","DNS","TCP","DHCP_CANCEL","DHCP_FINISH"),
                "R3_UNSUPPORTED_PROBE_KIND")
        with self.lock:
            require(self.phase=="R3" and self.token and
                    self._r3_udp_started_at is None and
                    self._r3_requested_mode is None,
                    "R3_NOT_IN_DEDICATED_PHASE_OR_ALREADY_TRIGGERED")
            self._r3_requested_mode=kind
            token=self.token
        self.send("G10R_R3_"+kind+":"+token)
        return token

    def capture_socket_line(self,line,at):
        # Invocado sólo desde port_reader bajo self.lock.
        if not self.phase or not self.token:return
        prefix="G10R_SOCK_BEGIN:"+self.token+" T_MS="
        if line.startswith(prefix):
            tick=line[len(prefix):]
            if not tick.isdecimal():return
            if self._sock_batch is not None:
                old=self._sock_batch
                old["status"]="INCOMPLETE_REVIEW_STOP_OVERWRITTEN"
                self.r3_snapshots.append(old)
            self._sock_batch={"phase":self.phase,"t":at,"tick_ms":int(tick),
                              "status":"CAPTURING","slots":[]}
            return
        if line=="G10R_SOCK_END:"+self.token:
            batch=self._sock_batch
            self._sock_batch=None
            if batch is None:return
            slots=batch["slots"]
            batch["status"]=("CAPTURED" if
                len(slots)==8 and {x["socket"] for x in slots}==set(range(8))
                else "INCOMPLETE_REVIEW_STOP")
            self.r3_snapshots.append(batch)
            return
        if line.startswith("G10R_SOCK_STATUS:"+self.token+" CODE="):
            code=line.rpartition("=")[2]
            if code.isdecimal():
                self.r3_snapshots.append({
                    "phase":self.phase,"t":at,
                    "status":"BUS_BUSY" if int(code)==2 else "REVIEW_STOP",
                    "code":int(code)})
            self._sock_batch=None
            return
        if self._sock_batch is not None:
            data=decode_socket_slot(line,self.token)
            if data is not None:self._sock_batch["slots"].append(data)

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
            self.trace.begin(token,phase)
            self.tcp_collector.begin(token)
            self._r3_udp_started_at=None
            self._r3_requested_mode=None
        self.send("G10R_START:"+token+":RTU=1")
        return token

    def end(self):
        if self.token:self.send("G10R_STOP:"+self.token)
        with self.lock:
            if self._sock_batch is not None:
                self._sock_batch["status"]="INCOMPLETE_REVIEW_STOP_SESSION_END"
                self.r3_snapshots.append(self._sock_batch)
                self._sock_batch=None
            self.trace.finish()
            self.tcp_collector.finish()
            if self.phase=="R3":
                self.r3_udp_sessions.append({
                    "token":self.token,
                    "kind":self._r3_requested_mode,
                    "lines":[item["line"] for item in self.r3_udp_lines
                             if item["phase"]=="R3" and
                             item["token"]==self.token],
                    "trace":self.trace.report()})
            self.phase=""
            self.token=""
            self.latest=None

    def traffic_worker(self,phase,interval,halt:threading.Event):
        seq=0
        prev_rtu=None
        next_snapshot_at=time.monotonic()
        next_tcp_at=time.monotonic()
        next_events_at=time.monotonic()
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
            if time.monotonic()>=next_events_at:
                with self.lock:
                    trace_cmd=self.trace.request(self.stamp())
                if trace_cmd:self.send(trace_cmd)
                next_events_at=time.monotonic()+2
            if time.monotonic()>=next_tcp_at:
                command=self.tcp_collector.request()
                if command:self.send(command)
                next_tcp_at=time.monotonic()+15
            if time.monotonic()>=next_snapshot_at:
                # Sólo observación; no se utiliza para validar R3 CLOSE.
                self.send("G10R_SOCKETS:"+token)
                next_snapshot_at=time.monotonic()+15
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
                      "token":token,"tcp":tcp_ok,"tcp_tid":seq&0xffff,
                      "udp":udp_ok,"rtu":rtu_ok})
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

    def r3_suite(self):
        """Un solo gate R3: cinco sondas TEMPORALES + tráfico continuo.
        No ejecutable hasta que N0/N1/N2 y seguridad de banco aprueben
        R3_PHYSICAL_INJECTION_READY (actualmente FALSE).
        """
        for mode in R3_PROBE_ORDER:
            token=self.start("R3")
            halt=threading.Event()
            task=threading.Thread(target=self.traffic_worker,
                                  args=("R3",1,halt),daemon=True)
            task.start()
            try:
                # El firmware envía START/READY. No inyectar inmediatamente
                # tras abrir UART; esperar al menos 1 heartbeat de la sesión.
                deadline=time.monotonic()+12
                ready=False
                while time.monotonic()<deadline and not self.stop.is_set():
                    with self.lock:
                        ready=(self.latest is not None and
                               self.latest.get("phase")=="R3")
                    if ready:break
                    self.stop.wait(.1)
                require(ready,"R3_HEARTBEAT_NOT_READY_"+mode)
                self.r3_udp_probe_request(mode)
                terminal="G10R_R3_"+mode+":"+token+(
                    " STATE=AWAIT_LIFECYCLE_REVIEW")
                until=time.monotonic()+15
                seen=False
                while time.monotonic()<until and not self.stop.is_set():
                    with self.lock:
                        lines=[x["line"] for x in self.r3_udp_lines
                               if x["token"]==token]
                        seen=(terminal in lines)
                        bad=any((" STATE="+state) in line for line in lines
                            for state in ("PREPARE_FAILED","MUTEX_NOT_ACQUIRED",
                              "TASK_CREATION_FAILED","CLOSE_RACE_REVIEW",
                              "HOLD_TIMING_REVIEW","INJECTION_INCOMPLETE_REVIEW"))
                    require(not bad,"R3_INJECTION_ABORT_"+mode)
                    if seen:break
                    self.stop.wait(.1)
                require(seen,"R3_MISSING_TERMINAL_"+mode)
                # Dar oportunidad al lector del ring de recuperar CLOSE/DRAIN.
                if self.stop.wait(4):
                    raise RuntimeError("R3_SERIAL_WORKER_ABORT_"+mode)
            finally:
                halt.set();task.join(timeout=5);self.end()
            session=self.r3_udp_sessions[-1]
            require(session["kind"]==mode,"R3_MODE_IDENTITY_MISMATCH")
            p=ROOT/"tools/alpha13/gates/a13_g10_r_r3_probe_contract.py"
            spec=importlib.util.spec_from_file_location("g10r3_contract_"+mode,p)
            require(spec is not None and spec.loader is not None,
                    "R3_PROBE_CONTRACT_MISSING")
            module=importlib.util.module_from_spec(spec)
            sys.modules[spec.name]=module
            spec.loader.exec_module(module)
            verdict=module.evaluate(session["lines"],session["trace"],
                                    session["token"],mode)
            require(verdict["status"]=="R3_UDP_SOURCE_CONTRACT_ONLY_NOT_PHYSICAL_PASS",
                    "R3_PROBE_EVIDENCE_REVIEW_STOP_"+mode)
        # Estrés 1/2/4 CLIENTES PC contra servidor single-active, en la MISMA
        # campaña autorizada. Hasta habilitar H este método no se ejecuta.
        router_path=ROOT/"tools/alpha13/gates/a13_g10_r_tcp124_router.py"
        router_spec=importlib.util.spec_from_file_location("g10r_tcp_router",router_path)
        require(router_spec is not None and router_spec.loader is not None,
                "TCP124_ROUTER_MODULE_MISSING")
        router=importlib.util.module_from_spec(router_spec)
        sys.modules[router_spec.name]=router
        router_spec.loader.exec_module(router)
        for count in (1,2,4):
            token=self.start("R3")
            try:
                deadline=time.monotonic()+12
                ip=None
                while time.monotonic()<deadline and not self.stop.is_set():
                    with self.lock:
                        if self.latest and self.latest.get("phase")=="R3":
                            ip=self.latest.get("ip")
                    if ip:break
                    self.stop.wait(.1)
                require(ip is not None,"TCP124_HEARTBEAT_IP_MISSING")
                router.plan(ip,count,2)
                report=router.run_target(ip,count,2,tid_base=1000+count*100)
                report.update({"token":token,"phase":"R3"})
                self.tcp124_stress.append(report)
                for observation in report.get("clients",[]):
                    if observation is None:continue
                    if "tid" in observation:
                        self.add({"kind":"traffic","phase":"R3","token":token,
                                  "t":self.stamp(),"tcp_tid":observation["tid"],
                                  "tcp":observation["fc03_ok"],"udp":False,
                                  "rtu":False})
                self.send("G10R_TCP_EVENTS:"+token)
                with self.lock:
                    trace_cmd=self.trace.request(self.stamp())
                if trace_cmd:self.send(trace_cmd)
                if self.stop.wait(4):raise RuntimeError("TCP124_SESSION_ABORT")
            finally:
                self.end()
        # Completar las 5 sondas NO equivale a validar DHCP renew/rebind,
        # 1/2/4 clientes ni fairness TFT/TCA: H permanece sujeto al plan.

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
      "R3":"FIVE_SOURCE_BACKED_PROBES_STAGED_LAB_ONLY",
      "R3_probe_order":list(R3_PROBE_ORDER),
      "R3_coverage_gaps":["DHCP_RENEW_REBIND_PHYSICAL",
                          "TCP_CONCURRENT_1_2_4",
                          "TFT_TCA_FAIRNESS_MEASUREMENTS"],
      "physical_campaign_enabled":R3_PHYSICAL_INJECTION_READY,
      "live": "FAIL_CLOSED_R3_MISSING"}


def selftest()->int:
    sample=("G10R_HEARTBEAT:"+"a"*24+
      " HB=10 BOOT=abc123 IP=192.0.2.4 LINK=2 ETH=1 TCP=1 DHCP_STATE=5 DHCP_ERR=0"
      " DHCP_TICKS=12 DHCP_PEND=1 DHCP_RENEW_OK=0 DHCP_RENEW_FAIL=0"
      " DHCP_REBIND_OK=0 DHCP_REBIND_FAIL=0"
      " SD_OK=10 SD_FAIL=0 RTC=1 FRAM=10 UDP_RX=4 UDP_TX=4"
      " RTU_OK=8 RTU_FAIL=0 IO_READY=1 IO_AGE_MS=50 RTC_AGE_MS=70 TFT_READY=1 BTN_EVENTS=2")
    ev=parse_heartbeat(sample,"a"*24,"R2",12.4)
    require(ev and ev["link"]==2 and ev["rtu_ok"]==8,
            "TEST_HEARTBEAT_POSITIVE")
    upper=sample.replace("BOOT=abc123","BOOT=ABC123")
    parsed_upper=parse_heartbeat(upper,"a"*24,"R2",12.4)
    require(parsed_upper and parsed_upper["boot"]=="abc123",
            "HEARTBEAT_ARDUINO_HEX_UPPERCASE_REJECTED")
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
                    " BOOT=abc123 LINK=1 VER=R1R2R3R4_FAULT_LAB_V2")
    require(READY_RE.fullmatch(expected_ready) is not None,
            "READY_PERIODIC_PROTOCOL_MISSING")
    require(READY_RE.fullmatch(expected_ready.replace("BOOT=abc123","BOOT=ABC123")) is not None,
            "READY_ARDUINO_HEX_UPPERCASE_REJECTED")
    require(READY_RE.fullmatch(expected_ready.replace("VER=R1R2R3R4_FAULT_LAB_V2",
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
            "source_head":current_source_head(),
            "r3_lab_snapshot":"READ_ONLY_INSTRUMENTED_NOT_FAULT_INJECTED",
            "sketch_sha256":{
                "COM4":hashlib.sha256((ROOT/"tools/alpha13/sketches/g10_r_integrated/g10_r_integrated.ino").read_bytes()).hexdigest(),
                "COM14":hashlib.sha256((ROOT/"tools/alpha13/sketches/g8_p4_slave/g8_p4_slave.ino").read_bytes()).hexdigest(),
            },
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
        evidence["uploaded"]["COM4"]="PASS_CONFIRMED_UPLOAD"
        evidence["source_head"]="0"*40
        doc.write_text(json.dumps(evidence),encoding="utf-8")
        try:inspect_evidence(doc)
        except RuntimeError:pass
        else:raise AssertionError("TEST_EVIDENCE_DIFFERENT_HEAD_ACCEPTED")
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
            fake.sent[0].endswith(b"\n"),
            "SLAVE_REPORT_COMMAND_TERMINATOR_REQUIRED")
    rec.phase="R1"
    rec.token="a"*24
    rec.capture_socket_line("G10R_SOCK_BEGIN:"+"a"*24+" T_MS=123",1.0)
    for sid in range(8):
        rec.capture_socket_line("G10R_SOCK:"+"a"*24+
            f" SID={sid} EPOCH=1 ALLOC={sid+1} OWNER=55 PENDING=0 VALID=1",
            1.0+sid*.01)
    rec.capture_socket_line("G10R_SOCK_END:"+"a"*24,1.3)
    require(len(rec.r3_snapshots)==1 and
            rec.r3_snapshots[0]["status"]=="CAPTURED" and
            len(rec.r3_snapshots[0]["slots"])==8,
            "R3_FULL_SNAPSHOT_NOT_RECORDED")
    rec.capture_socket_line("G10R_SOCK_BEGIN:"+"a"*24+" T_MS=500",2.0)
    rec.capture_socket_line("G10R_SOCK:"+"a"*24+
        " SID=0 EPOCH=1 ALLOC=1 OWNER=55 PENDING=0 VALID=1",2.1)
    rec.capture_socket_line("G10R_SOCK_END:"+"a"*24,2.2)
    require(rec.r3_snapshots[-1]["status"]=="INCOMPLETE_REVIEW_STOP",
            "R3_PARTIAL_SNAPSHOT_FALSE_PASS")
    rec.capture_socket_line("G10R_SOCK_STATUS:"+"a"*24+" CODE=2",3.0)
    require(rec.r3_snapshots[-1]["status"]=="BUS_BUSY",
            "R3_BUSY_MISCLASSIFIED_AS_CLOSED")
    require(decode_socket_slot("G10R_SOCK:"+"a"*24+
        " SID=8 EPOCH=1 ALLOC=1 OWNER=5 PENDING=0 VALID=1","a"*24) is None,
        "R3_OUT_OF_RANGE_SOCKET_ACCEPTED")
    require(plan()["hardware"]=="NOT_ACCESSED",
            "DRY_RUN_IMPLICIT_HARDWARE")
    require(R3_PHYSICAL_INJECTION_READY is False,
            "SELFTEST_MUST_REJECT_INCOMPLETE_PHYSICAL_CAMPAIGN")
    require(R3_PROBE_ORDER==("UDP","DNS","TCP","DHCP_CANCEL","DHCP_FINISH"),
            "SELFTEST_R3_INTEGRATED_MODES_DRIFT")
    for mode in R3_PROBE_ORDER:
        recorder=Recorder(Path("."))
        recorder.phase="R3";recorder.token="a"*24
        recorder._r3_requested_mode=mode
        recorder.capture_udp_probe_line(
            "G10R_R3_"+mode+":"+"a"*24+" STATE=ARMED",1.0)
        recorder.capture_udp_probe_line(
            "G10R_R3_HOLD:"+"a"*24+" US=80000",1.1)
        require(len(recorder.r3_udp_lines)==2 and
                recorder.r3_udp_lines[0]["kind"]==mode and
                recorder._r3_udp_started_at==1.0,
                "SELFTEST_R3_MODE_UART_NOT_CORRELATED_"+mode)
    # Los comandos R3 no pueden usarse en R1 ni sin sesión.
    r3rec=Recorder(Path("."))
    r3rec.phase="R1";r3rec.token="a"*24
    try:r3rec.r3_udp_probe_request()
    except RuntimeError:pass
    else:raise AssertionError("R3_UDP_PROBE_ALLOWED_DURING_R1")
    r3rec.phase="R3"
    r3rec.capture_udp_probe_line("G10R_R3_UDP:"+"a"*24+" STATE=ARMED",1.0)
    r3rec.capture_udp_probe_line("G10R_R3_HOLD:"+"a"*24+" US=80000",1.1)
    require(len(r3rec.r3_udp_lines)==2 and
            r3rec._r3_udp_started_at==1.0,
            "R3_UDP_UART_NOT_CAPTURED")
    r3rec.capture_udp_probe_line("G10R_R3_UDP:"+"b"*24+" STATE=ARMED",1.2)
    require(len(r3rec.r3_udp_lines)==3,
            "R3_FOREIGN_UART_NOT_PRESERVED_FOR_REVIEW")
    print("G10_R_FIELD_SELFTEST=PASS_6_NEGATIVE_SERIAL_CASES")
    print("G10_R_FIELD_HARDWARE=NOT_ACCESSED")
    return 0


def live(evidence:Path)->int:
    require(R3_PHYSICAL_INJECTION_READY,
            "G10R_PHYSICAL_BLOCKED_R3_PHASE_MISSING_IN_INTEGRATED_CAMPAIGN")
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
        c.r3_suite()  # Un único hito con subfases source-backed y abortos.
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
        result["r3_socket_snapshots"]=list(c.r3_snapshots)
        result["tcp_uart"]=c.tcp_collector.report()
        result["tcp124_stress"]=list(c.tcp124_stress)
        result["pc_tcp_transactions"]=[ev for ev in c.events
            if ev.get("kind")=="traffic" and "tcp_tid" in ev]
        result["r3_udp_lines"]=list(c.r3_udp_lines)
        result["r3_udp_sessions"]=list(c.r3_udp_sessions)
        result["r3_udp_verdicts"]=[]
        for session in c.r3_udp_sessions:
            try:
                p=ROOT/"tools/alpha13/gates/a13_g10_r_r3_probe_contract.py"
                spec=importlib.util.spec_from_file_location(
                    "g10r3_udp_contract",p)
                require(spec is not None and spec.loader is not None,
                        "R3_UDP_CONTRACT_UNAVAILABLE")
                m=importlib.util.module_from_spec(spec)
                sys.modules[spec.name]=m
                spec.loader.exec_module(m)
                result["r3_udp_verdicts"].append(m.evaluate(
                    session["lines"],session["trace"],session["token"],
                    session.get("kind") or "UNKNOWN"))
            except Exception as exc:
                result["r3_udp_verdicts"].append({
                    "status":"REVIEW_STOP",
                    "errors":["UDP_CONTRACT_EXCEPTION_"+type(exc).__name__]})
        result["r3_lifecycle_trace"]=c.trace.report()
        # B2: equidad y DHCP, solo evidencias observadas en UART/host.
        try:
            b2_path=ROOT/"tools/alpha13/gates/a13_g10_r_b2_contract.py"
            b2_spec=importlib.util.spec_from_file_location("g10r_b2_contract",b2_path)
            require(b2_spec is not None and b2_spec.loader is not None,
                    "B2_CONTRACT_MISSING")
            b2=importlib.util.module_from_spec(b2_spec)
            sys.modules[b2_spec.name]=b2
            b2_spec.loader.exec_module(b2)
            result["b2_observability"]=b2.evaluate(result)
            if result["b2_observability"]["status"]=="REVIEW_STOP":
                result["physical_status"]="REVIEW_STOP"
                status=2
        except Exception as exc:
            result["b2_observability"]={"status":"REVIEW_STOP",
                "issues":["B2_EVALUATION_EXCEPTION_"+type(exc).__name__]}
            result["physical_status"]="REVIEW_STOP"
            status=2
        # Unir evidencia de PC/Modbus con los eventos UART y el mismo lease.
        try:
            join_path=ROOT/"tools/alpha13/gates/a13_g10_r_tcp124_join.py"
            join_spec=importlib.util.spec_from_file_location("g10r_tcp_join",join_path)
            require(join_spec is not None and join_spec.loader is not None,
                    "TCP124_JOIN_UNAVAILABLE")
            join_module=importlib.util.module_from_spec(join_spec)
            sys.modules[join_spec.name]=join_module
            join_spec.loader.exec_module(join_module)
            result["tcp124_join"]=join_module.evaluate(result)
            if result["tcp124_join"]["status"]!="PASS_OFFLINE_SOURCE_EVIDENCE_CONTRACT":
                result["physical_status"]="REVIEW_STOP"
                status=2
        except Exception as exc:
            result["tcp124_join"]={"status":"REVIEW_STOP",
                "issues":["TCP_JOIN_EXCEPTION_"+type(exc).__name__]}
            result["physical_status"]="REVIEW_STOP"
            status=2
        result["r3_status"]="OBSERVABILITY_ONLY_NOT_CLOSE_FAULT_INJECTION"
        if c.trace.issues:
            result["r3_status"]="INSTRUMENTATION_GAP_REVIEW_STOP_R3"
        # Auditor semantico: no convierte una traza consistente en R3 fisico PASS.
        if result["r3_lifecycle_trace"]["events"]:
            try:
                p=ROOT/"tools/alpha13/gates/a13_g10_r_trace_validate.py"
                spec=importlib.util.spec_from_file_location("g10r3_semantics",p)
                require(spec is not None and spec.loader is not None,
                        "R3_SEMANTICS_UNAVAILABLE")
                m=importlib.util.module_from_spec(spec)
                sys.modules[spec.name]=m
                spec.loader.exec_module(m)
                report=result["r3_lifecycle_trace"]
                result["r3_semantic_verdict"]=m.evaluate({
                    "schema":"G10-R-LIFECYCLE-REPLAY-V1",
                    "origin":"UART_SOURCE_CAPTURE_UNVERIFIED",
                    "events":report["events"],
                    "dropped_events":report["dropped"],
                    "instrumentation_gaps":report["issues"]})
                if result["r3_semantic_verdict"]["status"]=="REVIEW_STOP":
                    result["r3_status"]="SEMANTIC_REVIEW_STOP_R3"
            except Exception as exc:
                result["r3_status"]="SEMANTIC_EVALUATOR_REVIEW_STOP_R3"
                result["r3_semantic_error"]=type(exc).__name__
        else:
            result["r3_status"]="NO_LIFECYCLE_EVENTS_REVIEW_STOP_R3"
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
