#!/usr/bin/env python3
"""G10-R TCP124: generador/observador HOST con loopback en selftest.

No habla con JWPLC ni crea evidencia de leases W5500; integrar con
captura UART/snapshots source-backed es un hito distinto. Sin --live.
"""
from __future__ import annotations
import argparse
import json
import socket
import struct
import threading
import time

TARGETS=(1,2,4)

def recv_exact(sock, n):
    data=b""
    while len(data)<n:
        chunk=sock.recv(n-len(data))
        if not chunk:
            raise RuntimeError("TRUNCATED_MODBUS_TCP_FRAME")
        data+=chunk
    return data

def observe_loopback(n):
    if n not in TARGETS:
        raise ValueError("UNSUPPORTED_CLIENT_COUNT")
    server=socket.socket()
    server.bind(("127.0.0.1",0))
    server.listen(n)
    port=server.getsockname()[1]
    rendezvous=threading.Barrier(n+1,timeout=5)
    release=threading.Event()
    errors=[]
    samples=[None]*n
    server_errors=[]
    def acceptor():
        conns=[]
        try:
            for _ in range(n):
                c,_=server.accept()
                c.settimeout(5)
                conns.append(c)
            rendezvous.wait()
            # Mantener todos los sockets abiertos hasta que los clientes
            # hayan demostrado FC03 y se registre su solapamiento real.
            for c in conns:
                packet=recv_exact(c,12)
                tid,protocol,length,unit,fc,addr,qty=struct.unpack("!HHHBBHH",packet)
                if (protocol,length,fc,qty)!=(0,6,3,1):
                    raise RuntimeError("INVALID_FC03_REQUEST")
                reply=struct.pack("!HHHBBBH",tid,0,5,unit,3,2,0x1234)
                c.sendall(reply)
            release.wait(timeout=5)
        except Exception as exc:
            server_errors.append(str(exc))
        finally:
            for c in conns:c.close()
            server.close()
    def client(i):
        start=None
        try:
            with socket.create_connection(("127.0.0.1",port),timeout=4) as c:
                c.settimeout(5)
                start=time.monotonic_ns()
                rendezvous.wait()
                c.sendall(struct.pack("!HHHBBHH",i+1,0,6,1,3,0,1))
                head=recv_exact(c,7)
                tid,protocol,length,unit=struct.unpack("!HHHB",head)
                body=recv_exact(c,length-1)
                good=(tid==i+1 and protocol==0 and body==b"\\x03\\x02\\x12\\x34")
                if not good:raise RuntimeError("INVALID_FC03_RESPONSE")
                release.wait(timeout=5)
                end=time.monotonic_ns()
                samples[i]={"client":i,"start_ns":start,"end_ns":end,"fc03_ok":True}
        except Exception as exc:
            errors.append({"client":i,"error":str(exc)})
    thread=threading.Thread(target=acceptor,daemon=True)
    thread.start()
    workers=[threading.Thread(target=client,args=(i,),daemon=True) for i in range(n)]
    for t in workers:t.start()
    for t in workers:t.join(timeout=8)
    release.set()
    thread.join(timeout=8)
    errors.extend({"error":e} for e in server_errors)
    if any(t.is_alive() for t in workers+[thread]) or any(s is None for s in samples):
        errors.append({"error":"INCOMPLETE_OR_STUCK_SESSION"})
    peak=0
    if not errors:
        points=sorted({x["start_ns"] for x in samples})
        peak=max(sum(x["start_ns"]<=p<x["end_ns"] for x in samples) for p in points)
        if peak!=n:errors.append({"error":"NOT_SIMULTANEOUS"})
    return {"status":"PASS_HOST_LOOPBACK_ONLY" if not errors else "REVIEW_STOP",
            "target":n,"active_peak":peak,"sessions":samples,"errors":errors,
            "hardware":"NOT_ACCESSED","w5500_lease":"UNVERIFIED",
            "physical_gate":"NOT_RUN"}

def selftest():
    for n in TARGETS:
        report=observe_loopback(n)
        assert report["status"]=="PASS_HOST_LOOPBACK_ONLY",report
        assert report["active_peak"]==n,report
    print("G10_R_TCP124_REAL_HOST_SOCKETS=PASS_LOOPBACK_1_2_4")
    print("G10_R_TCP124_W5500_SOURCE_LEASE=NOT_VERIFIED")
    print("G10_R_TCP124_PHYSICAL=NOT_RUN")

if __name__=="__main__":
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument("--selftest",action="store_true",required=True)
    p.parse_args()
    selftest()
