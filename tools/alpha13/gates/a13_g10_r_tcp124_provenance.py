#!/usr/bin/env python3
"""PRE-H: correlación conservadora de sesiones PC y observaciones W5500.

Offline, exclusivamente fixtures; nunca transforma un socket host en lease
físico por coincidencia de ID. No utiliza red, USB ni GPIO.
"""
from __future__ import annotations
import argparse
import copy
import json

KINDS={"Allocate","QueueRequest","DrainClose","Recycle","Invalidate"}
def evaluate(data):
    errors=[]
    if not isinstance(data,dict) or data.get("source")!="G10R_UART_LAB" or data.get("hardware") is not True:
        return {"status":"REVIEW_STOP","errors":["SOURCE_NOT_ATTESTED"]}
    sessions=data.get("sessions")
    events=data.get("events")
    if not isinstance(sessions,list) or not isinstance(events,list) or not sessions:
        return {"status":"REVIEW_STOP","errors":["MISSING_SESSIONS_OR_EVENTS"]}
    if data.get("trace_dropped",1)!=0 or data.get("trace_overrun",True) is not False:
        errors.append("TRACE_INCOMPLETE")
    identifiers=set()
    seen_sequences=set()
    history={}
    for event in events:
        if not isinstance(event,dict) or event.get("kind") not in KINDS:
            errors.append("UNKNOWN_EVENT");continue
        key=tuple(event.get(k) for k in ("socket","epoch","allocation","owner"))
        seq=event.get("seq")
        if (len(key)!=4 or type(key[0]) is not int or not 0<=key[0]<8
            or any(type(v) is not int or v<=0 for v in key[1:])
            or type(seq) is not int or seq<=0 or seq in seen_sequences):
            errors.append("INVALID_EVENT_PROVENANCE");continue
        seen_sequences.add(seq)
        history.setdefault(key,[]).append((seq,event["kind"]))
    for s in sessions:
        if not isinstance(s,dict):
            errors.append("MALFORMED_SESSION");continue
        sid=s.get("session")
        key=tuple(s.get(k) for k in ("socket","epoch","allocation","owner"))
        if not isinstance(sid,str) or not sid or sid in identifiers:
            errors.append("DUPLICATE_OR_INVALID_SESSION")
        identifiers.add(sid)
        if not s.get("uart_binding_verified",False):
            errors.append("SESSION_NOT_BOUND_TO_UART")
        if s.get("pc_transaction_id") is None or not s.get("fc03_ok",False):
            errors.append("TRANSACTION_NOT_CONFIRMED")
        rows=history.get(key,[])
        queues=[seq for seq,kind in rows if kind=="QueueRequest"]
        drains=[seq for seq,kind in rows if kind=="DrainClose"]
        if not rows or len(queues)!=1 or len(drains)!=1 or queues[0]>=drains[0]:
            errors.append("CLOSE_NOT_PROVEN_BY_LEASE")
        if any(kind=="Recycle" and seq<drains[0] for seq,kind in rows) if drains else False:
            errors.append("RECYCLED_BEFORE_CLOSE")
    return {"status":"REVIEW_STOP" if errors else "PASS_OFFLINE_EVIDENCE_CONTRACT_ONLY",
            "errors":sorted(set(errors)),"sessions":len(sessions),
            "physical_pass":"NOT_ATTESTED_BY_THIS_EVALUATOR"}

def fixture():
    return {"source":"G10R_UART_LAB","hardware":True,"trace_dropped":0,"trace_overrun":False,
        "sessions":[{"session":"test-1","socket":2,"epoch":3,"allocation":5,"owner":9,
                     "uart_binding_verified":True,"pc_transaction_id":47,"fc03_ok":True}],
        "events":[{"seq":1,"kind":"Allocate","socket":2,"epoch":3,"allocation":5,"owner":9},
                  {"seq":2,"kind":"QueueRequest","socket":2,"epoch":3,"allocation":5,"owner":9},
                  {"seq":3,"kind":"DrainClose","socket":2,"epoch":3,"allocation":5,"owner":9}]}

def selftest():
    positive=fixture()
    assert evaluate(positive)["status"]=="PASS_OFFLINE_EVIDENCE_CONTRACT_ONLY"
    cases=[]
    def case(label,fn):
        p=copy.deepcopy(positive);fn(p);cases.append((label,p))
    case("missing_uart",lambda p:p["sessions"][0].update(uart_binding_verified=False))
    case("wrong_owner",lambda p:p["sessions"][0].update(owner=15))
    case("wrong_epoch",lambda p:p["sessions"][0].update(epoch=4))
    case("missing_close",lambda p:p["events"].pop())
    case("double_drain",lambda p:p["events"].append(dict(p["events"][-1],seq=4)))
    case("missing_queue",lambda p:p["events"].pop(1))
    case("trace_drop",lambda p:p.update(trace_dropped=1))
    case("not_hardware",lambda p:p.update(hardware=False))
    case("wrong_source",lambda p:p.update(source="PC_ONLY"))
    case("duplicate_event_seq",lambda p:p["events"][-1].update(seq=2))
    case("fc03_failed",lambda p:p["sessions"][0].update(fc03_ok=False))
    for name,p in cases:
        assert evaluate(p)["status"]=="REVIEW_STOP",name
    print("G10_R_TCP124_PROVENANCE_CONTRACT=PASS_SYNTHETIC")
    print("G10_R_TCP124_PROVENANCE_NEGATIVES=11_REJECTED")
    print("G10_R_TCP124_HARDWARE=NOT_RUN")

if __name__=="__main__":
    p=argparse.ArgumentParser()
    p.add_argument("--selftest",action="store_true",required=True)
    p.parse_args()
    selftest()
