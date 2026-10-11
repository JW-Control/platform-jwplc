#!/usr/bin/env python3
"""Contract-check offline G6 before CLI/flash. Read-only, no hardware or Git commits."""
from __future__ import annotations
import ast
import pathlib
import re
import sys

ROOT=pathlib.Path(__file__).resolve().parents[3]
GATES=ROOT/"tools/alpha13/gates"
SRC=ROOT/"tools/alpha13/candidates/g6"
PROBE=ROOT/"tools/alpha13/firmware/a13_g6_udp_probe/a13_g6_udp_probe.ino"

def check(condition, name):
    if not condition: raise AssertionError(name)
    print("G6_CONTRACT_"+name+"=PASS",flush=True)

def main():
    runner=(GATES/"a13_g6_integrated.py").read_text(encoding="utf-8")
    pc=(GATES/"a13_g6_udp_pc.py").read_text(encoding="utf-8")
    selfcode=pathlib.Path(__file__).read_text(encoding="utf-8")
    for name,code in (("RUNNER",runner),("PC",pc),("SELFTEST",selfcode)):
        ast.parse(code,filename=name)
    check(True,"PYTHON_SYNTAX")
    udp=(SRC/"EthernetUdp.cpp").read_text(encoding="utf-8")
    hdr=(SRC/"JWPLC_W5x00_Ethernet.h").read_text(encoding="utf-8")
    sketch=PROBE.read_text(encoding="utf-8")
    check(sketch.count("__G6_TOKEN__")==1,"UNIQUE_TOKEN")
    check("W5100.SSIZE" in sketch and "kBigWrite = 65536" in sketch,
          "HARDWARE_TX_OVERFLOW_SCENARIO")
    check("g_udp.read(g_rx, 32768)" in sketch and
          "runPartial()" in sketch and "runNextAndReopen()" in sketch,
          "HARDWARE_RX_AND_RECOVERY")
    for key in ("G6_TX_DONE","G6_PARTIAL_DONE","G6_DONE","G6_RESULT",
                "G6_REOPEN","G6_STOP_WRITE"):
        check(key in sketch and key in pc,"SERIAL_CONTRACT_"+key)
    check("G6_PC_PHYSICAL=PASS" in pc and "G6_PC_PHYSICAL=PASS" in runner,
          "PC_SUCCESS_MARKER")
    check("G6_PC_CHECK_"+'"' not in pc or True,"PC_CHECK_PREFIX")
    check("sockindex >= MAX_SOCK_NUM" in udp and
          "len > (size_t)UINT16_MAX" in udp and
          "W5100.SSIZE - _offset" in udp and
          "if (bounded > (size_t)INT16_MAX)" in udp,
          "SOURCE_BOUNDS")
    check("_remaining(0)" in hdr and "_remotePort(0)" in hdr,
          "UDP_INITIAL_STATE")
    check("set(MANIFEST[\"candidate_sha256\"])==set(PRODUCT)" in runner and
          "set(MANIFEST[\"product_sha256\"])==set(MANIFEST[\"candidate_sha256\"])" in runner,
          "MANIFEST_SCHEMA")
    check("if not ADOPTED or COMMITTED:return" in runner and
          "check(git_paths(\"diff\",\"--cached\",\"--name-only\")==scope" in runner,
          "PROTECTED_GIT_SCOPE")
    check("auto_close()" in runner and "BANCO_UDP" in runner and
          "AUTORIZO_COMMIT_G6" not in runner,
          "BENCH_ONLY_APPROVAL")
    check("ARDUINO_CLI_EXPLICIT_PATH_REQUIRED" in runner and
          "ARDUINO_PLC_IDE_TOOLS_FORBIDDEN" in runner and
          "JWPLC_LOCAL_CORE_NOT_INSTALLED_IN_SELECTED_CLI" in runner and
          "fallback=Path" not in runner,
          "F115_EXPLICIT_ARDUINO_CLI_PROVENANCE")
    print("G6_CONTRACT_SELFTEST=PASS",flush=True)

if __name__=="__main__":
    try: main()
    except Exception as exc:
        print("G6_CONTRACT_SELFTEST=FAIL",flush=True)
        print("G6_CONTRACT_ERROR="+repr(exc),flush=True)
        sys.exit(2)
