import argparse
import socket
import sys
import threading
import time
import serial

def parse_args():
    p=argparse.ArgumentParser()
    p.add_argument("--host",required=True)
    p.add_argument("--port",type=int,default=5004)
    p.add_argument("--serial",required=True)
    p.add_argument("--baud",type=int,default=115200)
    p.add_argument("--timeout-s",type=float,default=10.0)
    return p.parse_args()

def main():
    a=parse_args()
    lines=[]
    end=threading.Event()
    stop=threading.Event()
    ser=serial.Serial(a.serial,a.baud,timeout=0.1)

    def reader():
        while not stop.is_set():
            raw=ser.readline()
            if not raw: continue
            line=raw.decode("utf-8",errors="replace").strip()
            if not line: continue
            lines.append(line)
            print(line,flush=True)
            if line=="NB1_D2R_RESULT=END": end.set()

    t=threading.Thread(target=reader,daemon=True); t.start()
    deadline=time.monotonic()+a.timeout_s
    ready=False
    while time.monotonic()<deadline:
        if any(x.startswith("NB1_D2R_READY=YES") for x in lines):
            ready=True; break
        time.sleep(0.05)
    if not ready:
        print("NB1_D2R_READY_TIMEOUT=YES")
        stop.set(); t.join(timeout=1); ser.close(); return 2

    s=socket.socket(socket.AF_INET,socket.SOCK_STREAM)
    s.settimeout(2.0)
    s.connect((a.host,a.port))
    print("CLIENT_CONNECTED=YES")
    s.sendall(b"R")

    rx=0
    while time.monotonic()<deadline and not end.is_set():
        try:
            data=s.recv(4096)
            if data: rx+=len(data)
        except socket.timeout:
            pass
        except OSError:
            break

    if not end.wait(timeout=2.0):
        print("NB1_D2R_RESULT_TIMEOUT=YES")
        s.close(); stop.set(); t.join(timeout=1); ser.close(); return 3

    print(f"CLIENT_RX_BYTES={rx}")
    s.close(); stop.set(); t.join(timeout=1); ser.close()
    print("NB1_D2R_CLIENT_PASS=YES")
    return 0

if __name__=="__main__":
    sys.exit(main())
