#!/usr/bin/env python3
"""G6 read-only product diagnostic: replay TX on the ALREADY FLASHED probe.

No Arduino CLI, compile, upload, source adoption, commits or cleanup.
Requires existing G6 manifest with successful upload and no product mutation.
Writes only ignored evidence logs to that run's local results directory.
"""
from __future__ import annotations
import argparse
import hashlib
import ipaddress
import json
from pathlib import Path
import re
import socket
import sys
import time
import serial

ROOT = Path(__file__).resolve().parents[3]
RESULTS = ROOT / "tools/alpha13/results"
PC_PORT = 5021
DUT_PORT = 5020
EXPECTED = bytes(i & 255 for i in range(2048))

def show(key, value):
    print(f"G6_DIAG_{key}={value}", flush=True)

def get_run(run_id):
    if run_id:
        dirs = [RESULTS / f"g6_integrated_{run_id}"]
    else:
        dirs = sorted(RESULTS.glob("g6_integrated_*"),
                      key=lambda p: p.stat().st_mtime, reverse=True)
    for folder in dirs:
        manifest_path = folder / "MANIFEST.json"
        if not manifest_path.is_file():
            continue
        try:
            m = json.loads(manifest_path.read_text(encoding="utf-8-sig"))
        except (ValueError, OSError):
            continue
        if (m.get("schema") != "A13-G6-INTEGRATED-1" or
            m.get("physical_upload") is not True or
            m.get("product_changed") is not False or
            m.get("git_product_committed") is not False or
            m.get("status") != "REVIEW_STOP" or
            m.get("reason") != "G6_PHYSICAL_PC_FAILED"):
            continue
        token = m.get("token")
        if (not isinstance(token, str) or not
            re.fullmatch(r"[0-9]{8}_[0-9]{6}_[0-9a-f]{8}_[0-9a-f]{32}", token)):
            continue
        if folder.name != "g6_integrated_" + m.get("run_id",""):
            continue
        return folder, m
    raise RuntimeError("NO_ELIGIBLE_G6_UPLOADED_MANIFEST")

def local_ip_for(remote):
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as s:
        s.connect((remote,DUT_PORT))
        return s.getsockname()[0]

def read_until(ser, pattern, timeout, received):
    until=time.monotonic()+timeout
    while time.monotonic()<until:
        line=ser.readline().decode("utf-8","replace").strip()
        if line:
            show("SERIAL",line)
            received.append(line)
        if pattern(line):
            return line
    return None

def run_diag(args):
    folder,m=get_run(args.run_id)
    port=m.get("port")
    if not isinstance(port,str) or port.upper()!="COM4":
        raise RuntimeError("UNEXPECTED_PORT_IN_MANIFEST")
    show("RUN_ID",m["run_id"])
    show("APP_SHA256",m.get("app_sha256","MISSING"))
    show("PORT",port)
    token=m["token"]
    marker=re.compile(r"^G6_READY="+re.escape(token)+r" IP=([0-9.]+)$")
    serial_lines=[]
    info={
        "run_id":m["run_id"],"token":token,
        "product_mutation":False,"upload":False,"git_changes":False,
        "comparison":{"expected_size":len(EXPECTED),
                      "expected_sha256":hashlib.sha256(EXPECTED).hexdigest()}
    }
    try:
        with serial.Serial(port,115200,timeout=0.3) as ser:
            line=read_until(ser,lambda s:marker.match(s) is not None,
                            args.ready_timeout,serial_lines)
            if not line:
                raise RuntimeError("READY_TOKEN_NOT_SEEN_OR_DEVICE_NOT_RUNNING_G6")
            dut=str(ipaddress.IPv4Address(marker.match(line).group(1)))
            pc=local_ip_for(dut)
            show("DUT_IP",dut)
            show("PC_IP",pc)
            info.update(dut_ip=dut,pc_ip=pc)
            with socket.socket(socket.AF_INET,socket.SOCK_DGRAM) as sock:
                sock.bind((pc,PC_PORT))
                sock.settimeout(args.packet_timeout)
                command=f"G6_TX:{token} {pc}\n"
                ser.write(command.encode("ascii"))
                ser.flush()
                show("TX_COMMAND_SENT","YES")
                try:
                    data,addr=sock.recvfrom(8192)
                    info["comparison"].update({
                        "received_size":len(data),"remote_ip":addr[0],
                        "remote_port":addr[1],
                        "received_sha256":hashlib.sha256(data).hexdigest(),
                        "matches_exactly":data==EXPECTED,
                        "source_ip_matches":addr[0]==dut,
                        "length_matches":len(data)==len(EXPECTED),
                    })
                    mismatch=next((i for i,(a,b) in enumerate(zip(data,EXPECTED))
                                   if a!=b),None)
                    if mismatch is None and len(data)!=len(EXPECTED):
                        mismatch=min(len(data),len(EXPECTED))
                    info["comparison"]["first_mismatch_index"]=mismatch
                    info["comparison"]["rx_first_32_hex"]=data[:32].hex()
                    info["comparison"]["expected_first_32_hex"]=EXPECTED[:32].hex()
                    info["comparison"]["rx_last_32_hex"]=data[-32:].hex()
                    show("DATAGRAM_RECEIVED","YES")
                    show("RECEIVED_BYTES",len(data))
                    show("EXPECTED_BYTES",len(EXPECTED))
                    show("REMOTE_IP",addr[0])
                    show("REMOTE_PORT",addr[1])
                    show("SOURCE_IP_MATCH",addr[0]==dut)
                    show("LENGTH_MATCH",len(data)==len(EXPECTED))
                    show("CONTENT_MATCH",data==EXPECTED)
                    show("FIRST_MISMATCH_INDEX",mismatch if mismatch is not None else "NONE")
                    show("RECEIVED_SHA256",info["comparison"]["received_sha256"])
                    show("RX_FIRST_32_HEX",data[:32].hex())
                except socket.timeout:
                    info["comparison"]["packet_timeout"]=True
                    show("DATAGRAM_RECEIVED","NO_TIMEOUT")
                done=read_until(ser,lambda s:s=="G6_TX_DONE="+token,
                                args.serial_timeout,serial_lines)
                info["serial_done"]=bool(done)
                info["serial_lines"]=serial_lines
                show("SERIAL_DONE",bool(done))
    finally:
        evidence=folder/"G6_TX_DIAGNOSTIC.json"
        evidence.write_text(json.dumps(info,indent=2,ensure_ascii=False)+"\n",encoding="utf-8")
        show("EVIDENCE",evidence)
        show("GIT_OR_FIRMWARE_MUTATED","NO")
    show("STATUS","DIAGNOSTIC_COLLECTED_NOT_A_G6_PASS")
    return 0

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--run-id",default="")
    parser.add_argument("--ready-timeout",type=float,default=35)
    parser.add_argument("--packet-timeout",type=float,default=8)
    parser.add_argument("--serial-timeout",type=float,default=12)
    args=parser.parse_args()
    try: return run_diag(args)
    except Exception as exc:
        show("STATUS","REVIEW")
        show("ERROR",repr(exc))
        return 2

if __name__=="__main__":sys.exit(main())
