#!/usr/bin/env python3
"""TCP124 hook: SOLO overlay G10-R; producto y archivos congelados intactos.

Notifica TID+socket después de validar MBAP y UNIT en el código real del
servidor, antes del despacho FC03. El receptor diagnóstico no escribe Serial
en callback ni altera resultado del protocolo. No equivale a FC03 OK.
"""
from __future__ import annotations
import argparse

PATH="JWPLC/2.1.0/libraries/JWPLC_ModbusTCP/src/JWPLC_ModbusTCP.cpp"
CALL_ANCHOR="    _txBuffer[6] = requestUnitId;"
HOOK_DECL='extern "C" void g10rLabTcpRequest(uint16_t, uint8_t) __attribute__((weak));'
CALL='''    // Solo bench G10-R: MBAP y unit validados; no demuestra respuesta OK.
    if (g10rLabTcpRequest != nullptr)
        g10rLabTcpRequest(readU16BE(&_rxBuffer[0]),
                          _client.getSocketNumber());'''

def compose(files):
    if len(files)!=14 or PATH not in files:
        raise RuntimeError("G10R_TCP124_REQUIRES_14_COMPOSED_SOURCES")
    source=files[PATH]
    if source.count(CALL_ANCHOR)!=1 or "g10rLabTcpRequest" in source:
        raise RuntimeError("G10R_TCP124_HOOK_ANCHOR_DRIFT")
    if '#include "JWPLC_ModbusTCP.h"' not in source:
        raise RuntimeError("G10R_TCP124_SOURCE_HEADER_DRIFT")
    source=source.replace('#include "JWPLC_ModbusTCP.h"',
                          '#include "JWPLC_ModbusTCP.h"\n'+HOOK_DECL,1)
    source=source.replace(CALL_ANCHOR,CALL_ANCHOR+"\n"+CALL,1)
    out=dict(files);out[PATH]=source
    assert all(out[p]==files[p] for p in files if p!=PATH)
    return out

def selftest():
    assert "readU16BE" in CALL and "_client.getSocketNumber()" in CALL
    assert "weak" in HOOK_DECL and "Serial" not in CALL
    print("G10R_TCP124_HOOK_CONTRACT=PASS_OFFLINE_NOT_COMPILED")

if __name__=="__main__":
    p=argparse.ArgumentParser()
    p.add_argument("--selftest",action="store_true",required=True)
    p.parse_args();selftest()
