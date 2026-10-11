#!/usr/bin/env python3
"""Verifica un único G7 FRAM físico vía puerto serie (sin flash)."""
from __future__ import annotations
from contextlib import closing
import argparse
import re
import sys
import time
import serial

def main():
    parser=argparse.ArgumentParser()
    parser.add_argument("--serial-port",required=True)
    parser.add_argument("--token",required=True)
    args=parser.parse_args()
    if not re.fullmatch(r"g7_[0-9]{8}_[0-9]{6}_[0-9a-f]{8}",args.token):
        print("G7_SERIAL_ERROR=TOKEN_INVALID",flush=True)
        return 2
    expected={
        "G7_FRAM_STALE_RECOVERY":"1",
        "G7_FRAM_BACKUP_PASS":"1",
        "G7_FRAM_NULL_AND_BOUNDS_ERRORS":"0",
        "G7_FRAM_VALID_BLOCK_ERRORS":"0",
        "G7_FRAM_CORRUPT_BLOCK_ERRORS":"0",
        "G7_FRAM_VERSION_ERRORS":"0",
        "G7_FRAM_RESTORE_PASS":"1",
        "G7_FRAM_TOTAL_ERRORS":"0",
        "G7_FRAM_RESULT":"PASS",
        "G7_FRAM_DONE":args.token,
    }
    received={}
    try:
        ser=serial.Serial(port=None,baudrate=115200,timeout=.3,
                          rtscts=False,dsrdtr=False,xonxoff=False)
        # Mismo procedimiento que ya obtuvo 29 READY tras reconectar
        # el JWPLC. Evitar pulsar DTR/RTS al abrir el puerto CH340.
        ser.dtr=False
        ser.rts=False
        ser.port=args.serial_port
        print("G7_SERIAL_DTR_RTS=FALSE_FALSE",flush=True)
        with closing(ser):
            ser.open()
            deadline=time.monotonic()+45
            ready="G7_FRAM_READY="+args.token
            while time.monotonic()<deadline:
                line=ser.readline().decode("utf-8","replace").strip()
                if line:
                    print("G7_SERIAL_"+line,flush=True)
                if line==ready: break
            else:
                print("G7_SERIAL_ERROR=READY_TIMEOUT_NO_RUN_SENT",flush=True)
                return 3
            print("G7_SERIAL_READY_MATCH=PASS",flush=True)
            print("G7_SERIAL_COMMAND=G7_FRAM_RUN_SENT_ONCE",flush=True)
            ser.write(("G7_FRAM_RUN:"+args.token+"\n").encode("ascii"))
            ser.flush()
            deadline=time.monotonic()+40
            while time.monotonic()<deadline:
                line=ser.readline().decode("utf-8","replace").strip()
                if line:
                    print("G7_SERIAL_"+line,flush=True)
                if "=" in line:
                    key,value=line.split("=",1)
                    if key in expected:
                        received.setdefault(key,[]).append(value)
                if line=="G7_FRAM_DONE="+args.token:
                    break
            else:
                print("G7_SERIAL_ERROR=DONE_TIMEOUT_RESTORE_MAY_BE_PENDING",flush=True)
                return 4
        for key,value in expected.items():
            ok=received.get(key)==[value]
            print("G7_CHECK_"+key+"="+("PASS" if ok else "FAIL"),flush=True)
            if not ok:
                print("G7_SERIAL_ERROR=INVARIANT_"+key,flush=True)
                return 5
        print("G7_FRAM_PHYSICAL=PASS_BACKUP_RESTORE_CHECKSUM_NULL_BOUNDS",flush=True)
        return 0
    except Exception as exc:
        print("G7_SERIAL_ERROR="+repr(exc),flush=True)
        return 6
if __name__=="__main__":
    sys.exit(main())
