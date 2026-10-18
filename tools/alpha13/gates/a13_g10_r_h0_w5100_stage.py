#!/usr/bin/env python3
"""H0-E: checkpoints en W5100.getLinkStatus solo en plataforma staged.

Amplia el overlay de 14 a 15 fuentes: utility/w5100.cpp es ORIGINAL de producto
leido read-only. La unica modificación se escribe en el clon temporal Arduino.
No agrega llamadas Serial/esperas/transferencias SPI a la ruta instrumentada.
"""
from __future__ import annotations

import argparse
from pathlib import Path

ROOT=Path(__file__).resolve().parents[3]
PATH="JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/utility/w5100.cpp"
INCLUDE='#include "w5100.h"'
DECL='''// H0-E SOLO plataforma temporal: callback ligero por etapa, opcional.
extern "C" void g10rH0LinkStage(uint8_t stage) __attribute__((weak));
static inline void h0W5100Stage(uint8_t stage) {
    if (g10rH0LinkStage) g10rH0LinkStage(stage);
}
'''
START='W5100Linkstatus W5100Class::getLinkStatus()\n{'
END='\nuint16_t W5100Class::write('
INIT='\tuint8_t phystatus;\n\n\tif (!init()) return UNKNOWN;'
CASE='\t  case 55:'
READ='''\t\tSPI.beginTransaction(SPI_ETHERNET_SETTINGS);
\t\tphystatus = readPHYCFGR_W5500();
\t\tSPI.endTransaction();'''
NEW_INIT='''\tuint8_t phystatus;
\th0W5100Stage(31); // entrar backend getLinkStatus
\th0W5100Stage(32); // antes de W5100.init
\tif (!init()) return UNKNOWN;
\th0W5100Stage(33); // init retorno correcto'''
NEW_READ='''\t\th0W5100Stage(35); // antes de SPI.beginTransaction
\t\tSPI.beginTransaction(SPI_ETHERNET_SETTINGS);
\t\th0W5100Stage(36); // beginTransaction retorno
\t\th0W5100Stage(37); // antes de readPHYCFGR_W5500
\t\tphystatus = readPHYCFGR_W5500();
\t\th0W5100Stage(38); // PHY leido, antes endTransaction
\t\tSPI.endTransaction();
\t\th0W5100Stage(39); // endTransaction retorno'''

def instrument(src: str) -> str:
    if src.count(INCLUDE)!=1 or DECL in src:
        raise RuntimeError("H0_E_W5100_HEADER_DRIFT")
    if src.count(START)!=1 or src.count(END)!=1:
        raise RuntimeError("H0_E_W5100_METHOD_DRIFT")
    left,tail=src.split(START,1)
    body,right=tail.split(END,1)
    # Exactamente una coincidencia de cada ancla DENTRO de getLinkStatus.
    for label,needle in (("INIT",INIT),("CASE",CASE),("READ",READ)):
        if body.count(needle)!=1:
            raise RuntimeError("H0_E_W5100_"+label+"_DRIFT")
    updated=(body.replace(INIT,NEW_INIT,1)
                 .replace(CASE,CASE+'\n\t\th0W5100Stage(34); // caso W5500',1)
                 .replace(READ,NEW_READ,1))
    return (left.replace(INCLUDE,INCLUDE+'\n'+DECL,1)
            +START+updated+END+right)

def compose(base: dict[str,str]) -> dict[str,str]:
    if len(base)!=14 or PATH in base:
        raise RuntimeError("H0_E_EXPECTED_14_SOURCES_NO_W5100")
    original=(ROOT/PATH).read_text(encoding="utf-8-sig")
    updated=instrument(original)
    result=dict(base)
    result[PATH]=updated
    if len(result)!=15 or any(result[k]!=v for k,v in base.items()):
        raise RuntimeError("H0_E_OVERLAY_DRIFT_OTHER_FILES")
    return result

def selftest() -> None:
    valid='''#include "w5100.h"
W5100Linkstatus W5100Class::getLinkStatus()
{
\tuint8_t phystatus;

\tif (!init()) return UNKNOWN;
\tswitch (chip) {
\t  case 55:
\t\tSPI.beginTransaction(SPI_ETHERNET_SETTINGS);
\t\tphystatus = readPHYCFGR_W5500();
\t\tSPI.endTransaction();
\t\treturn LINK_ON;
\t}
}
uint16_t W5100Class::write(uint16_t addr, const uint8_t *buf, uint16_t len)
{
    return len;
}
'''
    v=instrument(valid)
    assert "h0W5100Stage(31)" in v
    assert "h0W5100Stage(39)" in v
    assert v.count('phystatus = readPHYCFGR_W5500();')==1
    assert v.count("SPI.beginTransaction(SPI_ETHERNET_SETTINGS);")==1
    assert v.index("h0W5100Stage(37)")<v.index("readPHYCFGR_W5500();")
    assert v.index("SPI.endTransaction();")<v.index("h0W5100Stage(39)")
    assert "Serial." not in v
    for name,broken in (
        ("INIT",valid.replace("if (!init())","if (!tryInit())")),
        ("READ",valid.replace("readPHYCFGR_W5500()","readSomethingElse()")),
        ("CASE",valid.replace("case 55:","case 54:")),
        ("DUP",valid.replace(START,START+"\n"+START)),
    ):
        try: instrument(broken)
        except RuntimeError: pass
        else: raise AssertionError("H0_E_NEGATIVE_ACCEPTED_"+name)
    source=(ROOT/PATH).read_text(encoding="utf-8-sig")
    result=instrument(source)
    assert result.count("h0W5100Stage(")==9  # 1 declaracion inline + 8 llamadas
    assert source.count("SPI.beginTransaction(SPI_ETHERNET_SETTINGS);") == result.count("SPI.beginTransaction(SPI_ETHERNET_SETTINGS);")
    assert source.count("readPHYCFGR_W5500();")==result.count("readPHYCFGR_W5500();")
    print("G10R_H0_E_W5100_SOURCE_ANCHORS=PASS")
    print("G10R_H0_E_STAGES=31_32_33_34_35_36_37_38_39")
    print("G10R_H0_E_W5100_NO_EXTRA_SPI_OR_SERIAL=PASS")
    print("G10R_H0_E_NEGATIVES=4_REJECTED")

def main() -> int:
    ap=argparse.ArgumentParser()
    ap.add_argument("--selftest",required=True,action="store_true")
    ap.parse_args()
    selftest()
    return 0

if __name__=="__main__":raise SystemExit(main())
