#!/usr/bin/env python3
"""A13 PRE-H B2: hook observacional de DHCP cooperativo, sólo staging.

Observa return de maintainAsync() y pending real del backend tras liberar SPI.
No cambia DHCP, no fuerza renovación ni añade API productiva.
"""
from __future__ import annotations
import argparse

PATH="JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/JWPLC_Ethernet.cpp"
INCLUDE='#include "JWPLC_Ethernet.h"'
ANCHOR='''        const int maintainResult = Ethernet.maintainAsync();
        const bool maintainPending = Ethernet.dhcpMaintenanceInProgress();
        releaseBus();'''
REPLACEMENT='''        const int maintainResult = Ethernet.maintainAsync();
        const bool maintainPending = Ethernet.dhcpMaintenanceInProgress();
        releaseBus();

        // G10-R LAB: tras soltar bus SPI; observación sin cambios de flujo.
        if (g10rLabDhcpMaintenance != nullptr)
        {
            g10rLabDhcpMaintenance(maintainResult, maintainPending);
        }'''
DECL='extern "C" void g10rLabDhcpMaintenance(int, bool) __attribute__((weak));'

def compose(files):
    if len(files)!=14 or PATH not in files:
        raise RuntimeError("G10R_B2_REQUIRES_14_SOURCES")
    s=files[PATH]
    if s.count(INCLUDE)!=1 or s.count(ANCHOR)!=1 or "g10rLabDhcpMaintenance" in s:
        raise RuntimeError("G10R_B2_DHCP_ANCHOR_DRIFT")
    s=s.replace(INCLUDE,INCLUDE+"\n"+DECL,1)
    s=s.replace(ANCHOR,REPLACEMENT,1)
    out=dict(files);out[PATH]=s
    assert all(out[p]==files[p] for p in files if p!=PATH)
    return out

def selftest():
    assert "maintainAsync()" in ANCHOR and "releaseBus();" in ANCHOR
    assert REPLACEMENT.index("releaseBus();")<REPLACEMENT.index("g10rLabDhcpMaintenance(")
    assert "weak" in DECL and "Serial" not in REPLACEMENT
    print("G10R_B2_DHCP_HOOK=PASS_CONTRACT_NO_HARDWARE")

if __name__=="__main__":
    p=argparse.ArgumentParser()
    p.add_argument("--selftest",action="store_true",required=True)
    p.parse_args();selftest()
