#!/usr/bin/env python3
"""G6 PC UDP físico con token: 2048 TX, 256 parcial, 16 siguiente."""
from __future__ import annotations
import argparse
import ipaddress
import re
import socket
import sys
import time
import serial

DUT_PORT = 5020
PC_PORT = 5021
EXPECTED_TX = bytes((i & 255) for i in range(2048))
EXPECTED_PARTIAL = bytes((i * 3 + 7) & 255 for i in range(256))
EXPECTED_NEXT = bytes((i * 5 + 1) & 255 for i in range(16))

def local_ip(peer: str) -> str:
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as s:
        s.connect((peer, DUT_PORT))
        return s.getsockname()[0]

def get_line(ser, token: str, expect: str, limit: float, output: list[str]) -> bool:
    deadline=time.monotonic()+limit
    while time.monotonic()<deadline:
        line=ser.readline().decode("utf-8","replace").strip()
        if line:
            print(line,flush=True)
            output.append(line)
        if line==expect:
            return True
    return False

def main():
    parser=argparse.ArgumentParser()
    parser.add_argument("--serial-port",required=True)
    parser.add_argument("--token",required=True)
    args=parser.parse_args()
    all_lines=[]
    try:
        with serial.Serial(args.serial_port,115200,timeout=.3) as ser:
            pattern=re.compile(r"^G6_READY="+re.escape(args.token)+r" IP=([0-9.]+)$")
            deadline=time.monotonic()+35
            dut=None
            while time.monotonic()<deadline:
                line=ser.readline().decode("utf-8","replace").strip()
                if line:
                    print(line,flush=True)
                    all_lines.append(line)
                m=pattern.match(line)
                if m:
                    dut=str(ipaddress.ip_address(m.group(1)))
                    break
            if dut is None:
                print("G6_PC_ERROR=READY_TIMEOUT",flush=True)
                return 2
            pc=local_ip(dut)
            print("G6_DUT_IP="+dut,flush=True)
            print("G6_PC_IP="+pc,flush=True)
            with socket.socket(socket.AF_INET,socket.SOCK_DGRAM) as sock:
                sock.settimeout(8.0)
                sock.bind((pc,PC_PORT))
                ser.write(("G6_TX:"+args.token+" "+pc+"\n").encode("ascii"))
                data,address=sock.recvfrom(8192)
                if address[0]!=dut or data!=EXPECTED_TX:
                    print("G6_PC_ERROR=TX_PAYLOAD_MISMATCH",flush=True)
                    return 3
                print("G6_PC_TX_2048_EXACT=PASS",flush=True)
                if not get_line(ser,args.token,"G6_TX_DONE="+args.token,15,all_lines):
                    print("G6_PC_ERROR=TX_SERIAL_TIMEOUT",flush=True)
                    return 4
                sock.sendto(EXPECTED_PARTIAL,(dut,DUT_PORT))
                ser.write(("G6_PARTIAL:"+args.token+"\n").encode("ascii"))
                if not get_line(ser,args.token,"G6_PARTIAL_DONE="+args.token,10,all_lines):
                    print("G6_PC_ERROR=PARTIAL_TIMEOUT",flush=True)
                    return 5
                sock.sendto(EXPECTED_NEXT,(dut,DUT_PORT))
                ser.write(("G6_NEXT:"+args.token+"\n").encode("ascii"))
                if not get_line(ser,args.token,"G6_DONE="+args.token,10,all_lines):
                    print("G6_PC_ERROR=NEXT_TIMEOUT",flush=True)
                    return 6

            expected={
                "G6_TX_ACCEPTED_BYTES":"2048",
                "G6_TX_EXPECTED_BYTES":"2048",
                "G6_TX_END_PACKET":"1",
                "G6_TX_ERRORS":"0",
                "G6_PARTIAL_PACKET":"256",
                "G6_PARTIAL_READ":"8",
                "G6_PARTIAL_REMAINING":"248",
                "G6_PARTIAL_DRAIN_PARSE":"0",
                "G6_PARTIAL_ERRORS":"0",
                "G6_NEXT_PACKET":"16",
                "G6_NEXT_READ":"16",
                "G6_STOP_AVAILABLE":"0",
                "G6_STOP_WRITE":"0",
                "G6_STOP_BEGIN_PACKET":"0",
                "G6_REOPEN":"1",
                "G6_FINAL_ERRORS":"0",
                "G6_RESULT":"PASS",
                "G6_DONE":args.token,
            }
            for key,value in expected.items():
                vals=[x.split("=",1)[1] for x in all_lines if x.startswith(key+"=")]
                ok=vals==[value]
                print("G6_PC_CHECK_"+key+"="+("PASS" if ok else "FAIL"),flush=True)
                if not ok:
                    print("G6_PC_ERROR=SERIAL_ASSERT_"+key,flush=True)
                    return 7
            print("G6_PC_PHYSICAL=PASS",flush=True)
            return 0
    except Exception as exc:
        print("G6_PC_ERROR="+repr(exc),flush=True)
        return 8
if __name__=="__main__":
    sys.exit(main())
