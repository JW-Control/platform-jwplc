import argparse
import socket
import sys
import threading
import time
import serial

def parse_args():
    p=argparse.ArgumentParser()
    p.add_argument("--host",required=True)
    p.add_argument("--port",type=int,default=5005)
    p.add_argument("--serial",required=True)
    p.add_argument("--baud",type=int,default=115200)
    p.add_argument("--timeout-s",type=float,default=12.0)
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
            if line=="NB1_D2S_RESULT=END": end.set()

    t=threading.Thread(target=reader,daemon=True)
    t.start()

    deadline=time.monotonic()+a.timeout_s
    while time.monotonic()<deadline:
        if any(x.startswith("NB1_D2S_READY=YES") for x in lines):
            break
        time.sleep(0.05)
    else:
        print("NB1_D2S_READY_TIMEOUT=YES")
        stop.set(); t.join(timeout=1); ser.close(); return 2

    s=socket.socket(socket.AF_INET,socket.SOCK_STREAM)
    s.settimeout(0.2)
    s.connect((a.host,a.port))
    print("CLIENT_CONNECTED=YES")
    s.sendall(b"F")

    total=0
    while time.monotonic()<deadline and not end.is_set():
        try:
            data=s.recv(16384)
            if data:
                total+=len(data)
            else:
                break
        except socket.timeout:
            pass
        except OSError:
            break

    if not end.wait(timeout=2.0):
        print("NB1_D2S_RESULT_TIMEOUT=YES")
        try: s.close()
        except OSError: pass
        stop.set(); t.join(timeout=1); ser.close(); return 3

    try: s.close()
    except OSError: pass
    time.sleep(0.1)
    stop.set(); t.join(timeout=1); ser.close()

    print(f"CLIENT_RX_BYTES={total}")
    print("NB1_D2S_CLIENT_PASS=YES")
    return 0

if __name__=="__main__":
    sys.exit(main())
