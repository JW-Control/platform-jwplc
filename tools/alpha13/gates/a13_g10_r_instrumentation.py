#!/usr/bin/env python3
"""G10-R R3: instrumentación SOLO para plataforma TEMPORAL del banco.

Amplía el overlay G10-I, no el repositorio JWPLC/2.1.0. Únicamente
captura (bajo mutex W5500) epoch, ownerCookie, allocation, valid y
ticket de cierre diferido. NO libera sockets ni simula cierre y NO
acredita por sí sola la secuencia completa DNS/DHCP bajo SPI_BUSY.
"""
from __future__ import annotations

import argparse
import importlib.util
from pathlib import Path
import sys

ROOT=Path(__file__).resolve().parents[3]
HEADER="JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/JWPLC_W5500_SafeAccess.h"
CPP="JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/JWPLC_W5500_SafeAccess.cpp"
CLOSING="} // namespace jwplc_w5500_candidate"

HEADER_DIAGNOSTIC="""
// G10-R SOLO EN FIRMWARE DIAGNOSTICO: interfaz de inspeccion read-only.
// Esta API NO forma parte del producto ni del contrato Arduino comercial.
struct LabSocketSnapshot {
    uint8_t socketIndex = 255;
    uint32_t epoch = 0;
    uint32_t allocation = 0;
    uint32_t ownerCookie = 0;
    uint32_t pendingCloseAllocation = 0;
    bool valid = false;
};
enum class LabCaptureOutcome : uint8_t {
    Captured = 1,
    BusBusy = 2,
    InvalidOutput = 3
};
// Captura del mismo instante bajo mutex W5500. BUS_BUSY no significa CLOSED.
LabCaptureOutcome labCaptureSockets(LabSocketSnapshot* out,
                                    uint8_t capacity, uint32_t timeoutMs);
"""
CPP_DIAGNOSTIC="""
// G10-R: inspectores de solo lectura ejecutados bajo BusScope.
// NO escribir slots, no cancelar tickets ni afirmar estado CLOSED por BUS_BUSY.
LabCaptureOutcome labCaptureSockets(LabSocketSnapshot* out,
                                    uint8_t capacity, uint32_t timeoutMs) {
    if (out == nullptr || capacity < kSockets)
        return LabCaptureOutcome::InvalidOutput;
    BusScope bus(timeoutMs);
    if (!bus) return LabCaptureOutcome::BusBusy;
    for (uint8_t index = 0; index < kSockets; ++index) {
        const Slot& slot = g_slots[index];
        out[index].socketIndex = index;
        out[index].epoch = slot.epoch;
        out[index].allocation = slot.allocation;
        out[index].ownerCookie = slot.owner;
        out[index].pendingCloseAllocation =
            g_pendingClose[index].load(std::memory_order_acquire);
        out[index].valid = slot.valid;
    }
    return LabCaptureOutcome::Captured;
}
"""


def compose_lab(base: dict[str,str]) -> dict[str,str]:
    """No toca disco. Devuelve exactamente las mismas 14 rutas ya compuestas."""
    if len(base)!=14 or HEADER not in base or CPP not in base:
        raise RuntimeError("G10_R_R3_EXPECTED_G10I_14_SOURCE_OVERLAY")
    result=base.copy()
    h=result[HEADER]
    c=result[CPP]
    for label,source in (("HEADER",h),("CPP",c)):
        if source.count(CLOSING)!=1:
            raise RuntimeError("G10_R_R3_NAMESPACE_DRIFT_"+label)
        if "labCaptureSockets(" in source:
            raise RuntimeError("G10_R_R3_DUPLICATE_LAB_SYMBOL_"+label)
    # Procedencia SOURCE-BACKED: snapshot toma el mismo lease que usa el
    # driver, no valores calculados por el sketch ni datos de un mock.
    must_h=("struct SocketToken final", "bool matchesLocked(",
            "bool requestDeferredClose(")
    must_c=("Slot g_slots[kSockets]", "g_pendingClose[kSockets]",
            "bool enter(uint32_t timeoutMs)", "void leave()",
            "uint8_t serviceDeferredClose(")
    for needle in must_h:
        if needle not in h: raise RuntimeError("G10_R_R3_HEADER_SOURCE_DRIFT_"+needle)
    for needle in must_c:
        if needle not in c: raise RuntimeError("G10_R_R3_CPP_SOURCE_DRIFT_"+needle)
    result[HEADER]=h.replace(CLOSING,HEADER_DIAGNOSTIC+"\n"+CLOSING,1)
    result[CPP]=c.replace(CLOSING,CPP_DIAGNOSTIC+"\n"+CLOSING,1)
    if len(result)!=14:
        raise RuntimeError("G10_R_R3_SOURCE_COUNT_CHANGED")
    return result


def static_test() -> None:
    # Corpus minimo SINTETICO para forzar rechazo de guardas.
    h=("struct SocketToken final\nbool matchesLocked(\n"
       "bool requestDeferredClose(\n"+CLOSING)
    c=("Slot g_slots[kSockets]\ng_pendingClose[kSockets]\n"
       "bool enter(uint32_t timeoutMs)\nvoid leave()\n"
       "uint8_t serviceDeferredClose(\n"+CLOSING)
    fixture={HEADER:h,CPP:c}
    fixture.update({"dummy%02d"%i:"" for i in range(12)})
    good=compose_lab(fixture)
    assert "LabCaptureOutcome::BusBusy" in good[CPP]
    assert "g_pendingClose[index].load(std::memory_order_acquire)" in good[CPP]
    assert "BusScope bus(timeoutMs);" in good[CPP]
    tests=[
        ("WRONG_SOURCE_COUNT",lambda d:d.pop("dummy00")),
        ("TOKEN_MISSING",lambda d:d.__setitem__(HEADER,d[HEADER].replace("struct SocketToken final",""))),
        ("PENDING_MISSING",lambda d:d.__setitem__(CPP,d[CPP].replace("g_pendingClose[kSockets]",""))),
        ("GUARD_MISSING",lambda d:d.__setitem__(CPP,d[CPP].replace("bool enter(uint32_t timeoutMs)",""))),
        ("EXTRA_NAMESPACE",lambda d:d.__setitem__(CPP,d[CPP]+CLOSING)),
        ("EXISTING_DIAGNOSTIC",lambda d:d.__setitem__(HEADER,d[HEADER]+"labCaptureSockets(")),
    ]
    for name,mutate in tests:
        trial=fixture.copy()
        mutate(trial)
        try:compose_lab(trial)
        except RuntimeError:pass
        else:raise AssertionError("G10_R_R3_NEGATIVE_NOT_REJECTED_"+name)
    print("G10_R_R3_STAGE_SELFTEST=PASS_6_NEGATIVE_MUTANTS")
    print("G10_R_R3_SNAPSHOT_CONTRACT=READ_ONLY_SOURCE_BACKED")
    print("G10_R_R3_FIRMWARE_PHYSICAL=NOT_RUN")


def compose_candidate() -> dict[str,str]:
    path=ROOT/"tools/alpha13/gates/a13_g10_integrated.py"
    spec=importlib.util.spec_from_file_location("g10_r_integrator_r3",path)
    if spec is None or spec.loader is None:raise RuntimeError("INTEGRATOR_MISSING")
    module=importlib.util.module_from_spec(spec)
    sys.modules[spec.name]=module
    spec.loader.exec_module(module)
    files=module.compose()
    return compose_lab(files)


def main() -> int:
    p=argparse.ArgumentParser(description=__doc__)
    x=p.add_mutually_exclusive_group(required=True)
    x.add_argument("--selftest",action="store_true")
    x.add_argument("--source-check",action="store_true")
    args=p.parse_args()
    try:
        if args.selftest:static_test()
        else:
            result=compose_candidate()
            assert len(result)==14
            print("G10_R_R3_SOURCE_OVERLAY=PASS_14_INSTRUMENTED")
            print("G10_R_R3_PRODUCT_MODIFIED=NO")
        return 0
    except (RuntimeError,AssertionError) as e:
        print("G10_R_R3_REVIEW_STOP="+str(e))
        return 2


if __name__=="__main__":
    sys.exit(main())
