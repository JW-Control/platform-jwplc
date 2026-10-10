#!/usr/bin/env python3
"""G5 PC: serial token + PC TCP server 5008 + cliente entrante JWPLC 5018."""
from __future__ import annotations
import argparse
import ipaddress
import re
import socket
import sys
import threading
import time
import serial

PORT_OUTBOUND=5008
PORT_DUT_SERVER=5018
PAYLOAD_BYTES=5000
SERVER_BYTES=64
RX_BYTES=128

def host_ip_for(peer):
    with socket.socket(socket.AF_INET,socket.SOCK_DGRAM) as s:
        s.connect((peer,PORT_DUT_SERVER))
        return s.getsockname()[0]

def main():
    p=argparse.ArgumentParser()
    p.add_argument("--port",required=True)
    p.add_argument("--token",required=True)
    p.add_argument("--timeout",type=float,default=24)
    args=p.parse_args()
    expected=bytes(i&255 for i in range(PAYLOAD_BYTES))
    reply=bytes((i*7+3)&255 for i in range(RX_BYTES))
    server_expected=bytes((i*11+5)&255 for i in range(SERVER_BYTES))
    listener=None
    inbound=None
    replies=[]
    result={}
    received=[]
    done=threading.Event()
    stop=threading.Event()
    try:
        with serial.Serial(args.port,115200,timeout=.25) as ser:
            ready_pattern=re.compile(r"^G5_READY="+re.escape(args.token)+
                                     r" IP=([0-9.]+)$")
            deadline=time.monotonic()+args.timeout
            while time.monotonic()<deadline:
                line=ser.readline().decode("utf-8","replace").strip()
                if line: print(line,flush=True)
                m=ready_pattern.match(line)
                if m:
                    device=str(ipaddress.ip_address(m.group(1)))
                    break
            else:
                print("G5_HOST_ERROR=FRESH_READY_TIMEOUT",flush=True)
                return 2
            host=host_ip_for(device)
            print("G5_DUT_IP="+device,flush=True)
            print("G5_PC_IP="+host,flush=True)
            listener=socket.socket(socket.AF_INET,socket.SOCK_STREAM)
            listener.setsockopt(socket.SOL_SOCKET,socket.SO_REUSEADDR,1)
            listener.bind((host,PORT_OUTBOUND))
            listener.listen(1)
            listener.settimeout(10.0)
            def receiver():
                try:
                    conn,_=listener.accept()
                    with conn:
                        conn.settimeout(8.0)
                        while len(received)<PAYLOAD_BYTES:
                            data=conn.recv(min(4096,PAYLOAD_BYTES-len(received)))
                            if not data: break
                            received.extend(data)
                        if bytes(received)==expected:
                            conn.sendall(reply)
                        else:
                            result["error"]="WRONG_OUTBOUND_PAYLOAD"
                except Exception as exc:
                    result["error"]=str(exc)
                finally:
                    done.set()
            thread=threading.Thread(target=receiver,daemon=True)
            thread.start()
            inbound=socket.socket(socket.AF_INET,socket.SOCK_STREAM)
            inbound.settimeout(5.0)
            connected=False
            for _ in range(10):
                try:
                    inbound.connect((device,PORT_DUT_SERVER))
                    connected=True
                    break
                except (OSError,TimeoutError):
                    time.sleep(.2)
            if not connected:
                print("G5_HOST_ERROR=DUT_SERVER_CONNECT_FAIL",flush=True)
                return 3

            ser.write(("G5_RUN:"+args.token+" "+host+"\n").encode("ascii"))
            ser.flush()
            marker="G5_DONE="+args.token
            serial_lines=[]
            limit=time.monotonic()+18
            while time.monotonic()<limit:
                line=ser.readline().decode("utf-8","replace").strip()
                if line:
                    print(line,flush=True)
                    serial_lines.append(line)
                if line==marker: break
            else:
                print("G5_HOST_ERROR=SERIAL_DONE_TIMEOUT",flush=True)
                return 4

            inbound.settimeout(4.0)
            inbound_bytes=b""
            while len(inbound_bytes)<SERVER_BYTES:
                data=inbound.recv(SERVER_BYTES-len(inbound_bytes))
                if not data: break
                inbound_bytes+=data
            if not done.wait(3.0):
                print("G5_HOST_ERROR=OUTBOUND_SERVER_NOT_DONE",flush=True)
                return 5
            assert not result.get("error"), result.get("error")
            def exactly(key,value):
                hits=[x.split("=",1)[1] for x in serial_lines
                      if x.startswith(key+"=")]
                return hits==[value]
            checks={
                "SERIAL_PASS":exactly("G5_RESULT","PASS"),
                "SERIAL_ERRORS_ZERO":exactly("G5_ERRORS","0"),
                "SERIAL_TX_COMPLETE":exactly("G5_TX_BYTES",str(PAYLOAD_BYTES)),
                "SERIAL_RX_COMPLETE":exactly("G5_RX_BYTES",str(RX_BYTES)),
                "SERIAL_SERVER_COMPLETE":exactly("G5_SERVER_BYTES",str(SERVER_BYTES)),
                "PC_RECEIVED_EXACT_5000":bytes(received)==expected,
                "PC_RECEIVED_SERVER_64":inbound_bytes==server_expected,
            }
            for key,ok in checks.items():
                print("G5_"+key+"="+("PASS" if ok else "FAIL"),flush=True)
            if not all(checks.values()):return 6
            print("G5_PC_PHYSICAL=PASS",flush=True)
            return 0
    except Exception as exc:
        print("G5_HOST_ERROR="+repr(exc),flush=True)
        return 7
    finally:
        if inbound is not None: inbound.close()
        if listener is not None: listener.close()

if __name__=="__main__":sys.exit(main())
