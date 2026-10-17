#!/usr/bin/env python3
"""H0: trazar solo la API linkStatus() con marcas binarias, sin Serial bajo SPI.

Overlay temporal de JWPLC_Ethernet.cpp; no afecta fuentes del producto, APIs,
autoload, SPI mutex ni firmware de otros equipos.
"""
from __future__ import annotations
import argparse

PATH="JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/JWPLC_Ethernet.cpp"
INCLUDE='#include "JWPLC_Ethernet.h"'
DECL='extern "C" void g10rH0LinkStage(uint8_t stage) __attribute__((weak));'
OLD="""EthernetLinkStatus JWPLC_EthernetClass::linkStatus()
{
#if !JWPLC_HAS_ETHERNET
    return Unknown;
#else
    if (!acquireBus(200))
    {
        setError(JWPLC_ETH_BUS_LOCK_TIMEOUT);
        return Unknown;
    }

    jwplcSPI_deselectAll();
    EthernetLinkStatus status = Ethernet.linkStatus();

    releaseBus();
    return status;
#endif
}"""
NEW="""EthernetLinkStatus JWPLC_EthernetClass::linkStatus()
{
#if !JWPLC_HAS_ETHERNET
    return Unknown;
#else
    // H0 solo staged: registro RTC liviano, no Serial ni espera adicional.
    if (g10rH0LinkStage) g10rH0LinkStage(10); // antes de mutex
    if (!acquireBus(200))
    {
        if (g10rH0LinkStage) g10rH0LinkStage(11); // mutex no adquirido
        setError(JWPLC_ETH_BUS_LOCK_TIMEOUT);
        return Unknown;
    }
    if (g10rH0LinkStage) g10rH0LinkStage(20); // mutex obtenido
    jwplcSPI_deselectAll();
    if (g10rH0LinkStage) g10rH0LinkStage(30); // antes de PHY
    EthernetLinkStatus status = Ethernet.linkStatus();
    if (g10rH0LinkStage) g10rH0LinkStage(40); // PHY devuelto
    releaseBus();
    if (g10rH0LinkStage) g10rH0LinkStage(50); // mutex liberado
    return status;
#endif
}"""

def compose(files:dict[str,str])->dict[str,str]:
    if len(files)!=14 or PATH not in files:
        raise RuntimeError("H0_EXPECTED_STAGED_14_SOURCES")
    source=files[PATH]
    if source.count(INCLUDE)!=1 or source.count(OLD)!=1 or DECL in source:
        raise RuntimeError("H0_ETH_LINK_SOURCE_DRIFT")
    result=dict(files)
    result[PATH]=source.replace(INCLUDE,INCLUDE+"\n"+DECL,1).replace(OLD,NEW,1)
    if any(result[p]!=files[p] for p in files if p!=PATH):
        raise RuntimeError("H0_UNEXPECTED_FILE_MODIFICATION")
    return result

def selftest()->None:
    sample={f"fake/{i}":"other source" for i in range(13)}
    sample[PATH]=INCLUDE+"\n"+OLD
    out=compose(sample)
    assert len(out)==14
    assert out[PATH].count("g10rH0LinkStage(")==7  # 1 decl + 6 etapas
    assert out[PATH].count("Ethernet.linkStatus();")==1
    assert "acquireBus(200)" in out[PATH]
    assert out[PATH].index("g10rH0LinkStage(20)")<out[PATH].index("Ethernet.linkStatus();")
    assert out[PATH].index("Ethernet.linkStatus();")<out[PATH].index("g10rH0LinkStage(50)")
    for p in sample:
        if p!=PATH:assert out[p]==sample[p]
    try:compose({**sample,PATH:INCLUDE+"\n"+OLD.replace("acquireBus(200)","acquireBus(100)")})
    except RuntimeError:pass
    else:raise AssertionError("H0_SOURCE_MUTATION_ACCEPTED")
    print("G10R_H0_LINK_STAGES=PASS_ONLY_STAGED_ETHERNET_CPP")
    print("G10R_H0_LINK_STAGES=NO_GPIO4_NO_SERIAL_WITHIN_SPI")

if __name__=="__main__":
    p=argparse.ArgumentParser()
    p.add_argument("--selftest",action="store_true",required=True)
    p.parse_args()
    selftest()
