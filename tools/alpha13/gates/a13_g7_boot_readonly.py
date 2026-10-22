#!/usr/bin/env python3
"""G7: diagnóstico PASIVO post-upload ESP32. NO envía ni ejecuta G7_RUN.

Sólo:
- lee resultados locales ya existentes (MANIFEST + log serial anterior);
- abre COM4 a 115200 con DTR/RTS desactivados ANTES de abrirlo;
- lee 20 s de UART sin escribir ni intentar resets;
- crea evidencia .log en carpeta tools/alpha13/results/ (ignorada).
No compila, flashea, borra NVS/FRAM, modifica producto ni Git.
"""
from __future__ import annotations
import argparse
from datetime import datetime
import json
from pathlib import Path
import re
import sys
import time
import uuid

ROOT=Path(__file__).resolve().parents[3]
RUN="g7_20261011_162621_6ca7668e"
RESULTS=ROOT/"tools/alpha13/results"
CANDIDATES=sorted(RESULTS.glob("g7_fram_g7_*"),key=lambda p:p.stat().st_mtime,reverse=True)

def out(label,val):print(f"G7_DIAG_{label}={val}",flush=True)

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--serial-port",default="COM4")
    parser.add_argument("--seconds",type=int,default=20)
    args=parser.parse_args()
    if args.serial_port.upper()!="COM4" or not 5<=args.seconds<=30:
        out("ERROR","PORT_OR_SECONDS_OUT_OF_SCOPE")
        return 2
    path=next((p for p in CANDIDATES if
               (p/"MANIFEST.json").is_file() and
               (lambda m: m.get("physical_upload") is True and
                m.get("status")=="REVIEW_STOP" and
                m.get("reason")=="FRAM_PHYSICAL_SERIAL_FAILED" and
                m.get("product_adopted") is False and
                m.get("fram_restored") is False)(
                    json.loads((p/"MANIFEST.json").read_text(encoding="utf-8-sig"))
               )),None)
    if path is None:
        out("ERROR","NO_ELIGIBLE_G7_UPLOAD_REVIEW_MANIFEST")
        return 2
    manifest=json.loads((path/"MANIFEST.json").read_text(encoding="utf-8-sig"))
    token=manifest["run_id"]
    out("ORIGINAL_RUN",token)
    out("FIRMWARE_SHA256",manifest.get("app_sha256","UNKNOWN"))
    out("ORIGINAL_UPLOAD",manifest.get("physical_upload"))
    out("ORIGINAL_FRAM_RESTORED",manifest.get("fram_restored"))
    out("FRAM_TEST_COMMAND","NOT_SENT")
    log=path/"FRAM_PHYSICAL_SERIAL.log"
    if log.is_file():
        lines=log.read_text(encoding="utf-8-sig",errors="replace").splitlines()
        for line in lines:
            if "G7_SERIAL_" in line or "EXCEPTION" in line:
                out("ORIGINAL_SERIAL",line[:300])
        out("ORIGINAL_SERIAL_LINES",sum("G7_SERIAL_" in x for x in lines))
    import serial
    new=RESULTS/("g7_passive_uart_"+datetime.now().strftime("%Y%m%d_%H%M%S")+
                 "_"+uuid.uuid4().hex[:8]+".log")
    count=0; bytes_seen=0;ready=0;critical=[]
    ser=serial.Serial(port=None,baudrate=115200,timeout=.2,
                      rtscts=False,dsrdtr=False,xonxoff=False)
    # CH340 + ESP32: minimise handshake-line changes (without deliberate reset).
    ser.dtr=False
    ser.rts=False
    ser.port=args.serial_port
    out("PASSIVE_PORT",args.serial_port)
    out("PASSIVE_DTR_RTS","FALSE_FALSE")
    try:
        ser.open()
        until=time.monotonic()+args.seconds
        with new.open("w",encoding="utf-8") as dest:
            dest.write(f"RUN={token}\nPORT={args.serial_port}\nBAUD=115200\n")
            dest.write("NO_SEND_NO_RESET_NO_FLASH_NO_FRAM_ACCESS=YES\n")
            while time.monotonic()<until:
                data=ser.readline()
                if not data:continue
                bytes_seen+=len(data)
                decoded=data.decode("utf-8","replace").rstrip("\r\n")
                count+=1
                if decoded=="G7_FRAM_READY="+token:
                    ready+=1
                if any(tag in decoded.lower() for tag in
                       ["panic","guru meditation","abort","backtrace",
                        "rst:","boot:","reboot","exception","wdt","assert"]):
                    critical.append(decoded[:260])
                dest.write(f"[{time.monotonic():.3f}] {decoded}\n")
                if count<=70:out("UART",decoded[:240])
        out("UART_LINES",count)
        out("UART_BYTES",bytes_seen)
        out("READY_EXACT_COUNT",ready)
        out("BOOT_PANIC_MARKER_COUNT",len(critical))
        out("EVIDENCE",new)
        out("G7_FRAM_WRITES","NONE_FROM_THIS_DIAG")
        out("STATUS","PASSIVE_CAPTURE_COMPLETE_NOT_A_G7_PASS")
        return 0
    except Exception as exc:
        out("STATUS","REVIEW")
        out("ERROR",repr(exc))
        return 3
    finally:
        if ser.is_open:ser.close()

if __name__=="__main__":
    sys.exit(main())
