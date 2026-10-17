#!/usr/bin/env python3
"""Alpha13 G10-R: trazas de lifecycle exclusivas del overlay de LABORATORIO.

El hook no cambia decisiones de cierre. Registro fijo con try-lock sin espera.
Si se descartan eventos o se sobrescribe un cursor, la lectura devuelve una
brecha que IMPIDE declarar R3 PASS. No hay Serial ni SPI dentro de hooks.
"""
from __future__ import annotations

import argparse
import importlib.util
from pathlib import Path
import sys

ROOT=Path(__file__).resolve().parents[3]
H="JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/JWPLC_W5500_SafeAccess.h"
C="JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/JWPLC_W5500_SafeAccess.cpp"
END="} // namespace jwplc_w5500_candidate"

HEADER = """
// SOLO LABORATORIO: traza limitada, sin escrituras ni SPI extra.
enum class LabEventKind : uint8_t {
  Allocate=1, Recycle=2, CloseBusy=3, CloseImmediate=4, CloseStale=5,
  QueueRequest=6, DrainClose=7, DrainStale=8, Invalidate=9
};
struct LabEvent {
  uint32_t seq=0, epoch=0, allocation=0, owner=0;
  uint8_t socket=255;
  LabEventKind kind=LabEventKind::Allocate;
};
enum class LabEventResult : uint8_t {
  Read=1, HistoryOverrun=2, Busy=3, Invalid=4
};
// Sin mutex SPI, capacity 1..8, cursor sequence monotonico.
// HistoryOverrun o dropped>0 son brechas de instrumentacion.
LabEventResult labReadEvents(uint32_t after, LabEvent* out, uint8_t capacity,
    uint8_t* count, uint32_t* newest, uint32_t* dropped);
"""
GLOBALS = """
// El spinlock solo es para el anillo diagnostico. NUNCA espera por otro task.
constexpr uint32_t kLabRingSize=64;
LabEvent g_labRing[kLabRingSize]={};
std::atomic_flag g_labRingBusy=ATOMIC_FLAG_INIT;
std::atomic<uint32_t> g_labDropped{0};
uint32_t g_labSeq=0;
void labEvent(LabEventKind kind,const SocketToken& token) {
  if (g_labRingBusy.test_and_set(std::memory_order_acquire)) {
    g_labDropped.fetch_add(1,std::memory_order_relaxed);
    return;
  }
  if (g_labSeq==UINT32_MAX) {
    g_labDropped.fetch_add(1,std::memory_order_relaxed);
    g_labRingBusy.clear(std::memory_order_release);
    return;
  }
  const uint32_t seq=++g_labSeq;
  LabEvent event{};
  event.seq=seq;
  event.epoch=token.chipEpoch;
  event.allocation=token.allocation;
  event.owner=token.ownerCookie;
  event.socket=token.socketIndex;
  event.kind=kind;
  g_labRing[(seq-1)%kLabRingSize]=event;
  g_labRingBusy.clear(std::memory_order_release);
}
"""
READ = """
// No espera por bus SPI ni permite usar esta lectura para cerrar sockets.
LabEventResult labReadEvents(uint32_t after, LabEvent* out, uint8_t capacity,
    uint8_t* count, uint32_t* newest, uint32_t* dropped) {
  if (out==nullptr || count==nullptr || newest==nullptr ||
      dropped==nullptr || capacity==0 || capacity>8)
    return LabEventResult::Invalid;
  if (g_labRingBusy.test_and_set(std::memory_order_acquire))
    return LabEventResult::Busy;
  *count=0;
  *newest=g_labSeq;
  *dropped=g_labDropped.load(std::memory_order_relaxed);
  if (after>*newest) {
    g_labRingBusy.clear(std::memory_order_release);
    return LabEventResult::Invalid;
  }
  const uint32_t oldest=(*newest>kLabRingSize)
    ? (*newest-kLabRingSize+1) : 1;
  const bool overrun=after<oldest-1;
  const uint32_t first=overrun?oldest:after+1;
  for (uint32_t seq=first;seq<=*newest && *count<capacity;++seq) {
    const LabEvent& event=g_labRing[(seq-1)%kLabRingSize];
    if (event.seq!=seq) {
      g_labRingBusy.clear(std::memory_order_release);
      return LabEventResult::Invalid;
    }
    out[(*count)++]=event;
    if (seq==UINT32_MAX) break;
  }
  g_labRingBusy.clear(std::memory_order_release);
  return overrun?LabEventResult::HistoryOverrun:LabEventResult::Read;
}
"""
RULES=[
    ("        if (slot.valid && slot.allocation == ticket) {",
     "        const bool matchPending=(slot.valid && slot.allocation == ticket);\\n"
     "        if (matchPending) {"),
    ("    ++g_allocation;\n    Slot& s = g_slots[socket];",
     "    ++g_allocation;\n    Slot& s = g_slots[socket];\n"
     "    if(s.valid) {\n"
     "      SocketToken recycled{};\n"
     "      recycled.chipEpoch=s.epoch; recycled.allocation=s.allocation;\n"
     "      recycled.ownerCookie=s.owner; recycled.socketIndex=socket;\n"
     "      labEvent(LabEventKind::Recycle,recycled);\n"
     "    }"),
    ("    t.socketIndex = socket;\n    return t;\n}",
     "    t.socketIndex = socket;\n"
     "    labEvent(LabEventKind::Allocate,t);\n    return t;\n}"),
    ("    clearSlot(g_slots[token.socketIndex]);\n    return true;\n}",
     "    clearSlot(g_slots[token.socketIndex]);\n"
     "    labEvent(LabEventKind::Invalidate,token);\n    return true;\n}"),
    ("    if (!bus) return CloseOutcome::BusBusy; // NO abandonar handle.",
     "    if (!bus) { labEvent(LabEventKind::CloseBusy,token); "
     "return CloseOutcome::BusBusy; } // NO abandonar handle."),
    ("    if (!matchesLocked(token)) return CloseOutcome::Stale;\n"
     "    if (close == nullptr) return CloseOutcome::Stale;",
     "    if (!matchesLocked(token)) { labEvent(LabEventKind::CloseStale,token); "
     "return CloseOutcome::Stale; }\n"
     "    if (close == nullptr) return CloseOutcome::Stale;"),
    ("    close(token.socketIndex, context);\n"
     "    clearSlot(g_slots[token.socketIndex]);\n"
     "    return CloseOutcome::Closed;",
     "    close(token.socketIndex, context);\n"
     "    clearSlot(g_slots[token.socketIndex]);\n"
     "    labEvent(LabEventKind::CloseImmediate,token);\n"
     "    return CloseOutcome::Closed;"),
    ("    // An older owner may publish late, but may never displace a newer ticket.\n"
     "    return true;",
     "    // Peticion observada, NO afirmacion de que el socket aun es del emisor.\n"
     "    labEvent(LabEventKind::QueueRequest,token);\n"
     "    // An older owner may publish late, but may never displace a newer ticket.\n"
     "    return true;"),
    ("            close(index, context);\n            clearSlot(slot);\n"
     "            ++drained;",
     "            SocketToken draining{};\n"
     "            draining.chipEpoch=slot.epoch; draining.allocation=slot.allocation;\n"
     "            draining.ownerCookie=slot.owner; draining.socketIndex=index;\n"
     "            close(index, context);\n            clearSlot(slot);\n"
     "            labEvent(LabEventKind::DrainClose,draining);\n"
     "            ++drained;"),
    ("        uint32_t expected = ticket;\n"
     "        (void)pending.compare_exchange_strong(expected, 0,",
     "        if (!matchPending) {\n"
     "          SocketToken stale{};\n"
     "          stale.chipEpoch=slot.epoch; stale.allocation=ticket;\n"
     "          stale.ownerCookie=slot.owner; stale.socketIndex=index;\n"
     "          labEvent(LabEventKind::DrainStale,stale);\n"
     "        }\n"
     "        uint32_t expected = ticket;\n"
     "        (void)pending.compare_exchange_strong(expected, 0,"),
]


def compose_trace(files: dict[str,str])->dict[str,str]:
    if len(files)!=14 or H not in files or C not in files:
        raise RuntimeError("R3_TRACE_EXPECTS_14_COMPOSED_SOURCES")
    h,c=files[H],files[C]
    if h.count(END)!=1 or c.count(END)!=1:
        raise RuntimeError("R3_TRACE_NAMESPACE_DRIFT")
    if "labReadEvents(" in h or "labReadEvents(" in c:
        raise RuntimeError("R3_TRACE_DUPLICATE")
    if c.count("Slot g_slots[kSockets] = {};")!=1 or (
        c.count("}\n\nbool ownedByThisTask()")!=1):
        raise RuntimeError("R3_TRACE_CORE_ANCHOR_DRIFT")
    c=c.replace("Slot g_slots[kSockets] = {};",
                "Slot g_slots[kSockets] = {};\n"+GLOBALS,1)
    for before,after in RULES:
        if c.count(before)!=1:
            raise RuntimeError("R3_TRACE_SOURCE_ANCHOR_DRIFT_"+before[:45])
        c=c.replace(before,after,1)
    c=c.replace("}\n\nbool ownedByThisTask()",
                "}\n\n"+READ+"\nbool ownedByThisTask()",1)
    next_files=files.copy()
    next_files[H]=h.replace(END,HEADER+"\n"+END,1)
    next_files[C]=c
    if any(files[k]!=next_files[k] for k in files if k not in (H,C)):
        raise RuntimeError("R3_TRACE_CHANGED_NON_SAFEACCESS_SOURCE")
    return next_files


def selftest():
    assert len(RULES)==10
    for label,pair in enumerate(RULES):
        assert pair[0] and pair[1] and pair[0]!=pair[1],label
    assert "g_labDropped.fetch_add" in GLOBALS
    assert "HistoryOverrun" in READ and "if (after>*newest)" in READ
    print("G10_R_R3_TRACE_SELFTEST=PASS_9_GUARDED_HOOKS")
    print("G10_R_R3_TRACE_HARDWARE=NOT_RUN")


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    opts=parser.add_mutually_exclusive_group(required=True)
    opts.add_argument("--selftest",action="store_true")
    opts.add_argument("--source-check",action="store_true")
    args=parser.parse_args()
    try:
        if args.selftest:selftest()
        else:
            p=ROOT/"tools/alpha13/gates/a13_g10_r_instrumentation.py"
            spec=importlib.util.spec_from_file_location("r3_lab",p)
            m=importlib.util.module_from_spec(spec)
            sys.modules[spec.name]=m;spec.loader.exec_module(m)
            composed=compose_trace(m.compose_candidate())
            assert len(composed)==14
            print("G10_R_R3_TRACE_SOURCE_COMPOSED=PASS_14")
        return 0
    except Exception as e:
        print("G10_R_R3_TRACE_REVIEW_STOP="+str(e))
        return 2

if __name__=="__main__":
    sys.exit(main())
