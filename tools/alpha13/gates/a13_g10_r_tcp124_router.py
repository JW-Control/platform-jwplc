#!/usr/bin/env python3
"""G10-R TCP124: preparación y ensayo de carga PC dirigido, NUNCA upload.

--selftest solo valida plan y safety; --live requiere autorización específica
posterior a PRE-H y NO se invoca desde CI. No certifica W5500/leases.
"""
from __future__ import annotations
import argparse
import ipaddress
import json
import socket
import struct
import threading
import time
from pathlib import Path

COUNTS=(1,2,4)
AUTH="G10R AUTORIZO PRUEBA TCP EN ROUTER SIN FLASH"
def plan(ip, count, seconds):
    addr=ipaddress.ip_address(ip)
    if addr.version!=4 or not addr.is_private or addr.is_loopback or addr.is_link_local or addr.is_multicast:
        raise ValueError("TARGET_MUST_BE_PRIVATE_LAN_IPV4")
    if count not in COUNTS or not 1<=seconds<=15:
        raise ValueError("INVALID_BOUND")
    return {"target":str(addr),"port":502,"clients":count,"hold_seconds":seconds,
            "server_policy":"SINGLE_ACTIVE_MODBUS",
            "expected_parallel_fc03":"NOT_ASSUMED",
            "w5500_lease":"NOT_ATTESTED","physical":"AUTHORIZATION_REQUIRED"}

def recv_exact(sock,n):
    out=b""
    while len(out)<n:
        data=sock.recv(n-len(out))
        if not data:raise OSError("TRUNCATED_FC03")
        out+=data
    return out

def run_target(target,count,hold):
    start=threading.Barrier(count+1,timeout=8)
    records=[None]*count
    def client(i):
        row={"client":i,"connect_ok":False,"fc03_ok":False,"close_confirmed":False}
        try:
            with socket.create_connection((target,502),timeout=2) as s:
                s.settimeout(2)
                row["opened_ns"]=time.monotonic_ns()
                row["connect_ok"]=True
                start.wait()
                tid=i+1
                # FC03, holding registers 0..3; protocolo actual del G10-R.
                s.sendall(struct.pack("!HHHBBHH",tid,0,6,1,3,0,4))
                try:
                    head=recv_exact(s,7)
                    rid,pid,length,unit=struct.unpack("!HHHB",head)
                    if (rid,pid,length,unit)==(tid,0,11,1):
                        row["fc03_ok"]=recv_exact(s,10)==bytes.fromhex("03081234234534564567")
                except (OSError,TimeoutError):
                    row["fc03_timeout_or_error"]=True
                # Mantener conexión abierta para observación de concurrencia TCP.
                time.sleep(hold)
                row["closed_ns"]=time.monotonic_ns()
            row["close_confirmed"]=True # PC solamente, no W5500
        except Exception as e:
            row["error"]=type(e).__name__
            try:start.abort()
            except Exception:pass
        finally:records[i]=row
    tasks=[threading.Thread(target=client,args=(i,),daemon=True) for i in range(count)]
    for t in tasks:t.start()
    try:start.wait()
    except threading.BrokenBarrierError:pass
    for t in tasks:t.join(timeout=hold+6)
    if any(t.is_alive() for t in tasks):
        raise RuntimeError("WORKER_STUCK_ABORT")
    intervals=[(x["opened_ns"],x["closed_ns"]) for x in records or []
               if x and "opened_ns" in x and "closed_ns" in x]
    peak=max((sum(a<=t<b for a,b in intervals) for t in
              {a for a,b in intervals}),default=0)
    # Resultado observacional, sin convertir timeout FC03 a defecto del servidor single-client.
    return {"status":"REVIEW_REQUIRED_PHYSICAL_EVIDENCE","clients":records,
            "host_tcp_active_peak":peak,"requested":count,"w5500_lease":"UNKNOWN",
            "simultaneous_modbus_service":"NOT_CLAIMED"}

def selftest():
    for n in COUNTS:
        p=plan("192.168.0.31",n,2)
        assert p["clients"]==n and p["expected_parallel_fc03"]=="NOT_ASSUMED"
    for bad in ("127.0.0.1","8.8.8.8","224.1.1.1"):
        try:plan(bad,1,2)
        except ValueError:pass
        else:raise AssertionError("UNSAFE_IP_ACCEPTED")
    print("G10_R_TCP124_ROUTER_PLAN=PASS_OFFLINE_NO_NETWORK")
    print("G10_R_TCP124_ROUTER_PHYSICAL=NOT_RUN")

if __name__=="__main__":
    a=argparse.ArgumentParser()
    g=a.add_mutually_exclusive_group(required=True)
    g.add_argument("--selftest",action="store_true")
    g.add_argument("--live",action="store_true")
    a.add_argument("--target")
    a.add_argument("--clients",type=int,choices=COUNTS)
    a.add_argument("--hold",type=int,default=2)
    args=a.parse_args()
    if args.selftest:selftest()
    else:
        raise SystemExit("G10R_PREH_BLOCKED_ROUTER_PHYSICAL_NOT_AUTHORIZED")
