#!/usr/bin/env python3
"""Autoprueba read-only de contrato del gate dual JWPLC G8 P4.

Importa el runner REAL; no abre COM, no hace upload ni compilación.
Prueba parser de reportes, casos negativos, roles, token y sketches.
"""
from __future__ import annotations
import importlib.util
import re
from pathlib import Path
import sys


def need(ok,what):
    if not ok: raise RuntimeError(what)


def main():
    target=Path(__file__).resolve().parent/"a13_g8_p4_dual_plc_physical.py"
    spec=importlib.util.spec_from_file_location("g8_p4_gate",target)
    need(spec is not None and spec.loader is not None,"RUNNER_IMPORT_FAILED")
    gate=importlib.util.module_from_spec(spec)
    sys.modules[spec.name]=gate
    spec.loader.exec_module(gate)
    need(gate.MASTER_PORT=="COM4" and gate.SLAVE_PORT=="COM14",
         "PORTS_NOT_EXACT_MASTER_COM4_SLAVE_COM14")
    need(gate.NUM_READS==12 and gate.MASTER_RX==156 and gate.SLAVE_RX==96,
         "FC03_FRAME_COUNTS_INVALID")
    token="0123456789abcdef01234567"
    mline="G8_P4_MASTER_RESULT:"+token+" OK=12 FAIL=0 RXBYTES=156 RXFRAMES=12 TXFRAMES=12 TIMEOUTS=0 CRC=0 READY=1"
    sline="G8_P4_SLAVE_RESULT:"+token+" RXBYTES=96 RXFRAMES=12 TXFRAMES=12 REQUESTS=12 CRC=0 READY=1"
    master=gate.parse_report(mline,"MASTER",token)
    slave=gate.parse_report(sline,"SLAVE",token)
    need(gate.check_reports(master,slave),"GOOD_PAIR_REJECTED")
    need(gate.parse_report(sline,"MASTER",token) is None,"ROLE_SWAPPED_ACCEPTED")
    need(gate.parse_report(mline,"MASTER","f"*24) is None,"WRONG_TOKEN_ACCEPTED")
    print("G8_P4_SELFTEST_PASS_PAIR=PASS",flush=True)
    mutations=[
      ({**master,"OK":11},"MASTER_BAD_GOOD_COUNT"),
      ({**master,"RXBYTES":0},"MASTER_MISSING_RX_BYTES"),
      ({**master,"RXBYTES":312},"MASTER_DOUBLE_RX_BYTES"),
      ({**master,"FAIL":1},"MASTER_REQUEST_FAIL"),
      ({**master,"CRC":1},"MASTER_CRC_ERROR"),
      ({**master,"TIMEOUTS":1},"MASTER_TIMEOUT"),
      ({**master,"READY":0},"MASTER_UART_NOT_READY"),
    ]
    for changed,label in mutations:
        try: gate.check_reports(changed,slave)
        except RuntimeError: pass
        else: raise RuntimeError(label+"_ACCEPTED")
    changes=[
      ({**slave,"RXBYTES":192},"SLAVE_DOUBLE_RX_BYTES"),
      ({**slave,"RXBYTES":0},"SLAVE_ZERO_RX_BYTES"),
      ({**slave,"REQUESTS":10},"SLAVE_MISSING_REQUESTS"),
      ({**slave,"CRC":1},"SLAVE_CRC"),
      ({**slave,"READY":0},"SLAVE_UART_NOT_READY"),
    ]
    for changed,label in changes:
        try: gate.check_reports(master,changed)
        except RuntimeError: pass
        else: raise RuntimeError(label+"_ACCEPTED")
    for altered in [
        mline.replace(" RXBYTES=156"," RXBYTES=-1"),
        mline+" CRC=0",
        mline.replace(" OK=12"," OK=12 BAD=1"),
    ]:
        try: gate.parse_report(altered,"MASTER",token)
        except RuntimeError: pass
        else: raise RuntimeError("BAD_REPORT_ACCEPTED")
    print("G8_P4_SELFTEST_NEGATIVE_CASES=15_PASS",flush=True)

    master_path=gate.SKETCHES["MASTER"]/"g8_p4_master.ino"
    slave_path=gate.SKETCHES["SLAVE"]/"g8_p4_slave.ino"
    ma=master_path.read_text(encoding="utf-8")
    sl=slave_path.read_text(encoding="utf-8")
    need('JWPLC_ModbusRTU.requestReadHoldingRegisters(' in ma and
         "kTargetId, 0, 4, values, 1200" in ma and
         "kCycles = 12" in ma and "G8_P4_MASTER_RESULT:" in ma and
         "G8_P4_START:" in ma,"MASTER_SKETCH_CONTRACT_INVALID")
    need("JWPLC_ModbusRTU.setHoldingRegisters(holding, 4)" in sl and
         "G8_P4_SLAVE_RESULT:" in sl and "G8_P4_REPORT:" in sl and
         "holding[4] = {0x1234, 0x2345, 0x3456, 0x4567}" in sl,
         "SLAVE_SKETCH_CONTRACT_INVALID")
    need("digitalWrite(" not in ma+sl and "pinMode(" not in ma+sl and
         "Serial2." not in ma+sl,"FORBIDDEN_PINS_OR_UART_RECONFIG")
    need(re.search(r"G8 FLASH COM14 SLAVE COM4 MASTER",gate.CONFIRM),
         "UPLOAD_CONFIRMATION_MISSING")
    need(gate.PRODUCT_COMMIT=="fb71d65eb2c842b3f46e6949760e693f3b73d95e",
         "G8_PRODUCT_COMMIT_NOT_PINNED")
    print("G8_P4_SELFTEST_SKETCHES_AND_SAFETY=PASS",flush=True)
    print("G8_P4_SELFTEST=PASS_NO_HARDWARE_ACCESS",flush=True)


if __name__=="__main__":
    main()
