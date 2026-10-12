#!/usr/bin/env python3
"""G9 P2/P3: contrato exacto A13-012 (NUL SD) y A13-014 (RTC/botonera).

No modifica fuentes de producto. Pruebas puras, sin Git, COM ni Arduino.
"""
from __future__ import annotations

import difflib
from pathlib import Path
import re
import sys

ROOT=Path(__file__).resolve().parents[3]
SD="JWPLC/2.1.0/libraries/JW_SD/src/JW_SD.cpp"
GLOBAL="JWPLC/2.1.0/libraries/JWPLC_GlobalPeripherals/src/JWPLC_GlobalPeripherals.cpp"
SD_OLD=b"'\x00'"
SD_NEW=b"'\\0'"
GLOBAL_REPLACEMENTS=(
    ("// Flags básicos para futura expansión.\n"
     "// Por ahora el RTC está habilitado por defecto y siempre se muestra en IDLE.\n"
     "static bool g_rtcEnabled = true;\n\n",
     "// RTC está habilitado cuando JWPLC_HAS_RTC=1; no existe flag interno configurable.\n\n"),
    ("static uint32_t g_buttonRefreshThrottleMs = 25;",
     "static constexpr uint32_t g_buttonRefreshThrottleMs = 25;"),
    ("    if (!g_rtcEnabled)\n"
     "    {\n"
     "        return false;\n"
     "    }\n\n", ""),
    ("if (!g_rtcEnabled || rtc == nullptr)", "if (rtc == nullptr)"),
)


def require(value,reason):
    if not value: raise RuntimeError(reason)


def make_sd_candidate(data:bytes)->bytes:
    require(data.count(b"\x00")==1,"SD_NUL_COUNT_NOT_EXACT_ONE")
    require(data.count(SD_OLD)==1,"SD_CHARACTER_NUL_PATTERN_NOT_EXACT")
    original_offset=data.index(SD_OLD)
    new=data[:original_offset]+SD_NEW+data[original_offset+len(SD_OLD):]
    require(b"\x00" not in new,"SD_NUL_NOT_REMOVED")
    require(len(new)==len(data)+1,"SD_ESCAPED_LITERAL_LENGTH_UNEXPECTED")
    require(new[original_offset:original_offset+len(SD_NEW)]==SD_NEW and
            new[:original_offset]+SD_OLD+new[original_offset+len(SD_NEW):]==data,
            "SD_NOT_REVERSIBLE_EXACT_ONE_LITERAL_AT_KNOWN_OFFSET")
    return new


def make_global_candidate(data:bytes)->bytes:
    src=data.decode("utf-8")
    require(src.count("g_rtcEnabled")==3,"GLOBAL_RTC_REFERENCES_CHANGED")
    require(src.count("g_buttonRefreshThrottleMs")==2,
            "GLOBAL_THROTTLE_REFERENCES_CHANGED")
    # Git puede entregar CRLF en Windows y LF en Linux. Preservar bytes
    # originales fuera de las cuatro ventanas de cambio, sin normalizar
    # o reescribir el archivo completo con finales de línea distintos.
    newline="\r\n" if "\r\n" in src else "\n"
    require(not ("\r\n" in src and src.count("\r\n")!=src.count("\n")),
            "GLOBAL_MIXED_NEWLINES_UNEXPECTED")
    new=src
    for original,replacement in GLOBAL_REPLACEMENTS:
        source_pattern=original.replace("\n",newline)
        target_pattern=replacement.replace("\n",newline)
        require(new.count(source_pattern)==1,
                "GLOBAL_REPLACEMENT_NOT_EXACT:"+repr(original[:40]))
        new=new.replace(source_pattern,target_pattern,1)
    require("g_rtcEnabled" not in new,"GLOBAL_RTC_FLAG_REMAINS")
    require("static constexpr uint32_t g_buttonRefreshThrottleMs = 25;" in new,
            "GLOBAL_THROTTLE_NOT_25_CONSTEXPR")
    require("if (rtc == nullptr)" in new,"GLOBAL_NULL_GUARD_REMOVED")
    require(new.count("jwplcSystemForceDisplayRefresh();")==src.count(
        "jwplcSystemForceDisplayRefresh();"),"GLOBAL_REFRESH_CALL_CHANGED")
    require(new.count("return JWPLC_RTC.begin();")==src.count(
        "return JWPLC_RTC.begin();"),"GLOBAL_RTC_BEGIN_CHANGED")
    require(new.count("#if !JWPLC_HAS_RTC")==src.count("#if !JWPLC_HAS_RTC"),
            "GLOBAL_RTC_BUILD_GUARDS_CHANGED")
    require(new.count("JWPLC_SD.serviceDataLogs();")==src.count(
        "JWPLC_SD.serviceDataLogs();"),"GLOBAL_SD_RUNTIME_CHANGED")
    # Los únicos cambios de líneas deberán provenir de estos cuatro bloques.
    unified=list(difflib.unified_diff(src.splitlines(),new.splitlines(),n=0))
    require(len(unified)>3 and len(unified)<45,
            "GLOBAL_DIFF_UNEXPECTED_SCOPE")
    return new.encode("utf-8")


def test():
    sd=(ROOT/SD).read_bytes()
    gp=(ROOT/GLOBAL).read_bytes()
    s2=make_sd_candidate(sd)
    g2=make_global_candidate(gp)
    require(s2!=sd and g2!=gp,"NO_REAL_CANDIDATE_DELTA")
    require(s2.count(b"\\0")==sd.count(b"\\0")+1,
            "SD_ESCAPED_NUL_NOT_TEXTUAL")
    require(b"precompiled=full" in (ROOT/"JWPLC/2.1.0/libraries/JW_SD/library.properties").read_bytes(),
            "SD_PRECOMPILED_POLICY_CHANGED")
    gp_prop=(ROOT/"JWPLC/2.1.0/libraries/JWPLC_GlobalPeripherals/library.properties").read_bytes()
    require(b"precompiled=" not in gp_prop,
            "GLOBALPERIPHERALS_UNEXPECTED_PRECOMPILED")
    print("G9_CANDIDATE_SD_NUL_EXACT_SINGLE_ESCAPE=PASS",flush=True)
    print("G9_CANDIDATE_GLOBAL_RTC_ENABLED_AND_25MS_PRESERVED=PASS",flush=True)
    print("G9_CANDIDATE_GLOBAL_RTCDATALOG_HOOKS_PRESERVED=PASS",flush=True)
    print("G9_CANDIDATE_PRODUCT_FILES_UNCHANGED=PASS",flush=True)
    print("G9_CANDIDATE_CONTRACT=PASS_READ_ONLY",flush=True)


if __name__=="__main__":
    sys.exit(test())
