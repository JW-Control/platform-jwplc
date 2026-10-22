#!/usr/bin/env python3
"""G10-R R3: auditor OFFLINE de secuencia real de LabEvent del overlay.

Detecta close con dueño ajeno, epoch/allocation reciclados, falta de
DrainClose, huecos de secuencia y cobertura ausente. Nunca emite PASS físico.
"""
from __future__ import annotations

import argparse
import copy
import json
from pathlib import Path

ALLOCATION=1
RECYCLE=2
BUSY=3
IMMEDIATE=4
STALE=5
PUBLISH=6
DRAIN=7
IGNORED=8
INVALIDATE=9
VALID_KINDS=set(range(1,10))
COVERAGE={ALLOCATION,RECYCLE,BUSY,IMMEDIATE,STALE,PUBLISH,DRAIN,IGNORED}


def evaluate(doc:dict)->dict:
    failures=[]
    if not isinstance(doc,dict) or doc.get("schema")!="G10-R-LIFECYCLE-REPLAY-V1":
        return {"status":"REVIEW_STOP","failures":["INVALID_SCHEMA"]}
    if doc.get("origin") not in ("HOST_SYNTHETIC_ONLY",
                                 "UART_SOURCE_CAPTURE_UNVERIFIED"):
        failures.append("INVALID_PROVENANCE")
    events=doc.get("events")
    if not isinstance(events,list) or not events or len(events)>100000:
        return {"status":"REVIEW_STOP","failures":["NO_EVENTS_OR_OVERSIZE"]}
    if doc.get("instrumentation_gaps") or doc.get("dropped_events",0)!=0:
        failures.append("INSTRUMENTATION_GAP")
    active={}
    queued=set()
    kinds=set()
    cursor=0
    for i,e in enumerate(events):
        if not isinstance(e,dict):
            failures.append("MALFORMED_EVENT");continue
        seq,kind,sid=e.get("seq"),e.get("kind"),e.get("socket")
        epoch,allocation,owner=(e.get("epoch"),e.get("allocation"),e.get("owner"))
        if (type(seq) is not int or type(kind) is not int or type(sid) is not int
            or type(epoch) is not int or type(allocation) is not int or
            type(owner) is not int or seq<=0 or kind not in VALID_KINDS
            or not 0<=sid<8 or epoch<0 or allocation<0 or owner<0):
            failures.append("INVALID_EVENT_FIELDS");continue
        if seq!=cursor+1:
            failures.append("NONCONTIGUOUS_EVENT_SEQUENCE")
        cursor=seq
        kinds.add(kind)
        lease=(epoch,allocation,owner)
        live=active.get(sid)
        if kind==ALLOCATION:
            if not all(lease):
                failures.append("ALLOC_ZERO_IDENTITY")
            if live is not None:
                failures.append("ALLOC_WITHOUT_RECYCLE")
            active[sid]=lease
        elif kind==RECYCLE:
            if live!=lease:
                failures.append("RECYCLE_OTHER_OWNER")
            active.pop(sid,None)
        elif kind==BUSY:
            # SPI_BUSY no demuestra cierre ni propiedad actual.
            pass
        elif kind==IMMEDIATE:
            if live!=lease:
                failures.append("IMMEDIATE_CLOSED_OTHER_OWNER")
            active.pop(sid,None) if live==lease else None
        elif kind==STALE:
            if live==lease:
                failures.append("STALE_REPORTED_FOR_CURRENT")
        elif kind==PUBLISH:
            # requestDeferredClose solo prueba publicacion, no que el
            # productor conserve el lease: un token obsoleto puede publicarse.
            if live==lease:queued.add((sid,lease))
        elif kind==DRAIN:
            if live!=lease:
                failures.append("DRAIN_CLOSED_OTHER_OWNER")
            if (sid,lease) not in queued:
                failures.append("DRAIN_NO_SOURCE_TICKET")
            active.pop(sid,None) if live==lease else None
            queued.discard((sid,lease))
        elif kind==IGNORED:
            if live==lease:
                failures.append("STALE_TICKET_POINTS_TO_CURRENT")
        elif kind==INVALIDATE:
            if live!=lease:
                failures.append("INVALIDATE_OTHER_OWNER")
            active.pop(sid,None) if live==lease else None
    uncovered=sorted(COVERAGE-kinds)
    return {
        "status":("REVIEW_STOP" if failures else
                  "PASS_OFFLINE_CONSISTENCY_ONLY"),
        "failures":list(dict.fromkeys(failures)),
        "missing_fault_injection_coverage":uncovered,
        "physical_status":"NOT_ATTESTED",
        "event_count":len(events),
    }


def sample()->dict:
    def e(seq,kind,alloc,owner=11):
        return {"seq":seq,"kind":kind,"socket":2,"epoch":1,
                "allocation":alloc,"owner":owner}
    return {"schema":"G10-R-LIFECYCLE-REPLAY-V1",
            "origin":"HOST_SYNTHETIC_ONLY",
            "dropped_events":0,"instrumentation_gaps":[],
            "events":[
                e(1,1,5),e(2,3,5),e(3,6,5),e(4,7,5),
                e(5,1,6,12),e(6,6,6,12),e(7,2,6,12),
                e(8,1,7,13),e(9,8,6,13),e(10,5,6,12),
                e(11,4,7,13)
            ]}


def selftest():
    m=sample()
    assert evaluate(m)["status"]=="PASS_OFFLINE_CONSISTENCY_ONLY"
    tests=[
        ("missing_sequence",lambda x:x["events"][3].update(seq=55)),
        ("drain_wrong_owner",lambda x:x["events"][3].update(owner=77)),
        ("drain_not_queued",lambda x:x["events"][2].update(kind=3)),
        ("recycle_wrong_owner",lambda x:x["events"][6].update(owner=80)),
        ("stale_closes_current",lambda x:x["events"][9].update(kind=4,allocation=7,owner=13)),
        ("bad_socket",lambda x:x["events"][0].update(socket=8)),
        ("unrecorded_drop",lambda x:x.update(dropped_events=1)),
        ("gap_recorded",lambda x:x.update(instrumentation_gaps=["UART_TIMEOUT"])),
        ("missing_alloc",lambda x:x["events"][0].update(kind=3)),
        ("double_close",lambda x:x["events"].append({**x["events"][-1],"seq":12})),
    ]
    for name,mutation in tests:
        d=copy.deepcopy(m)
        mutation(d)
        assert evaluate(d)["status"]=="REVIEW_STOP",name
    print("G10_R_R3_SEMANTIC_NEGATIVES=10_PASS_REJECTED")
    print("G10_R_R3_SEMANTICS=PASS_OFFLINE_ONLY_PHYSICAL_NOT_ATTESTED")


def main():
    p=argparse.ArgumentParser(description=__doc__)
    g=p.add_mutually_exclusive_group(required=True)
    g.add_argument("--selftest",action="store_true")
    g.add_argument("--evaluate",type=Path)
    args=p.parse_args()
    if args.selftest:selftest();return 0
    try:
        doc=json.loads(args.evaluate.read_text(encoding="utf-8-sig"))
        result=evaluate(doc)
        print(json.dumps(result,indent=2,ensure_ascii=False))
        return 0 if result["status"]=="PASS_OFFLINE_CONSISTENCY_ONLY" else 2
    except (OSError,ValueError) as exc:
        print("G10_R_R3_SEMANTIC_REVIEW_STOP="+str(type(exc).__name__))
        return 2

if __name__=="__main__":
    raise SystemExit(main())
