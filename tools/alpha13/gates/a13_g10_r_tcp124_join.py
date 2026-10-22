#!/usr/bin/env python3
"""A13 G10-R: correlación offline PC FC03 -> UART TID/SID/lease -> lifecycle.

Sólo usa evidencias recibidas; no acepta banderas uart_binding_verified ni
declara PASS físico. Rechaza colisiones, TID ambiguos y trazas perdidas.
"""
from __future__ import annotations
import argparse
import copy
import json

def evaluate(manifest):
    problems=[]
    tcp=manifest.get("tcp_uart",{})
    trace=manifest.get("r3_lifecycle_trace",{})
    pc=manifest.get("pc_tcp_transactions")
    if not isinstance(tcp,dict) or tcp.get("status")!="OBSERVATIONAL_ONLY_NOT_PHYSICAL_PASS":
        problems.append("TCP_UART_INCOMPLETE")
    if not isinstance(trace,dict) or trace.get("status")!="OBSERVATIONAL_COMPLETE_NOT_PHYSICAL_PASS":
        problems.append("LIFECYCLE_INCOMPLETE")
    if not isinstance(pc,list) or not pc:
        problems.append("NO_PC_TRANSACTIONS");pc=[]
    bindings={}
    for batch in tcp.get("batches",[]) if isinstance(tcp,dict) else []:
        if batch.get("status")!="OBSERVATIONAL_COMPLETE_NOT_PHYSICAL_PASS":
            problems.append("TCP_BATCH_REVIEW_STOP");continue
        token=batch.get("token")
        for row in batch.get("rows",[]):
            if not isinstance(row,dict):problems.append("MALFORMED_UART_ROW");continue
            tid=row.get("TID");sid=row.get("SID");code=row.get("CAPTURE")
            lease=(sid,row.get("EPOCH"),row.get("ALLOC"),row.get("OWNER"))
            if not isinstance(token,str) or len(token)!=24 or type(tid) is not int:
                problems.append("MALFORMED_UART_BINDING");continue
            if code!=1 or type(sid) is not int or not 0<=sid<8 or any(
                type(x) is not int or x<=0 for x in lease[1:]):
                problems.append("UNVERIFIED_UART_LEASE");continue
            key=(token,tid)
            previous=bindings.get(key)
            if previous is not None and previous!=lease:
                problems.append("CONFLICTING_TID_LEASE")
            bindings[key]=lease
    events=trace.get("events",[]) if isinstance(trace,dict) else []
    groups={}
    for event in events:
        if not isinstance(event,dict):problems.append("INVALID_LIFECYCLE_EVENT");continue
        lease=tuple(event.get(k) for k in ("socket","epoch","allocation","owner"))
        if any(type(x) is not int for x in lease):
            problems.append("INVALID_LIFECYCLE_IDENTITY");continue
        groups.setdefault(lease,[]).append((event.get("seq"),event.get("kind")))
    matched=0
    checked=set()
    for transaction in pc:
        if not isinstance(transaction,dict):problems.append("INVALID_PC_EVENT");continue
        if transaction.get("tcp") is not True:
            continue # timeouts/esperas del servidor single-client no se convierten en PASS
        token=transaction.get("token");tid=transaction.get("tcp_tid")
        key=(token,tid)
        if key in checked:
            problems.append("DUPLICATE_PC_TRANSACTION");continue
        checked.add(key)
        lease=bindings.get(key)
        if lease is None:
            problems.append("SUCCESSFUL_PC_TID_NOT_BOUND_TO_UART");continue
        kinds=groups.get(lease,[])
        immediate=[seq for seq,k in kinds if k==4]
        queued=[seq for seq,k in kinds if k==6]
        drained=[seq for seq,k in kinds if k==7]
        direct=len(immediate)==1 and not queued and not drained
        deferred=(not immediate and len(queued)==1 and len(drained)==1 and
                  type(queued[0]) is int and type(drained[0]) is int and
                  queued[0]<drained[0])
        if not (direct or deferred):
            problems.append("W5500_CLOSE_NOT_VERIFIED");continue
        end=immediate[0] if direct else drained[0]
        if any(k==2 and (type(seq) is not int or seq<end) for seq,k in kinds):
            problems.append("RECYCLE_BEFORE_CLOSE");continue
        matched+=1
    if not checked:problems.append("NO_SUCCESSFUL_PC_FC03")
    return {"status":"REVIEW_STOP" if problems else "PASS_OFFLINE_SOURCE_EVIDENCE_CONTRACT",
            "issues":sorted(set(problems)),"matched":matched,
            "hardware_attestation":"NOT_PROVIDED_BY_THIS_TOOL"}

def fixture():
    t="a"*24
    lease={"socket":2,"epoch":3,"allocation":5,"owner":9}
    return {"tcp_uart":{"status":"OBSERVATIONAL_ONLY_NOT_PHYSICAL_PASS","batches":[{
        "status":"OBSERVATIONAL_COMPLETE_NOT_PHYSICAL_PASS","token":t,"rows":[{
        "SEQ":1,"TID":47,"SID":2,"CAPTURE":1,"EPOCH":3,"ALLOC":5,"OWNER":9}]}]},
      "pc_tcp_transactions":[{"token":t,"tcp_tid":47,"tcp":True}],
      "r3_lifecycle_trace":{"status":"OBSERVATIONAL_COMPLETE_NOT_PHYSICAL_PASS",
        "events":[dict(lease,seq=1,kind=1),
                  dict(lease,seq=2,kind=6),dict(lease,seq=3,kind=7)]}}
def selftest():
    data=fixture()
    assert evaluate(data)["status"]=="PASS_OFFLINE_SOURCE_EVIDENCE_CONTRACT"
    cases=[]
    def case(name,fn):
        obj=copy.deepcopy(data);fn(obj);cases.append((name,obj))
    case("wrong_owner",lambda p:p["tcp_uart"]["batches"][0]["rows"][0].update(OWNER=22))
    case("wrong_tid",lambda p:p["pc_tcp_transactions"][0].update(tcp_tid=48))
    case("no_close",lambda p:p["r3_lifecycle_trace"]["events"].pop())
    case("missing_queue",lambda p:p["r3_lifecycle_trace"]["events"].pop(1))
    case("trace_gap",lambda p:p["r3_lifecycle_trace"].update(status="INSTRUMENTATION_GAP_REVIEW_STOP"))
    case("tcp_overrun",lambda p:p["tcp_uart"].update(status="REVIEW_STOP"))
    case("busy_lease",lambda p:p["tcp_uart"]["batches"][0]["rows"][0].update(CAPTURE=2))
    case("failed_fc03",lambda p:p["pc_tcp_transactions"][0].update(tcp=False))
    case("wrong_epoch",lambda p:p["r3_lifecycle_trace"]["events"][2].update(epoch=88))
    case("duplicate_pc",lambda p:p["pc_tcp_transactions"].append(copy.deepcopy(p["pc_tcp_transactions"][0])))
    for name,obj in cases:
        assert evaluate(obj)["status"]=="REVIEW_STOP",name
    direct=copy.deepcopy(data)
    direct["r3_lifecycle_trace"]["events"]=direct["r3_lifecycle_trace"]["events"][:1]+[
        dict(direct["r3_lifecycle_trace"]["events"][0],seq=2,kind=4)]
    assert evaluate(direct)["status"]=="PASS_OFFLINE_SOURCE_EVIDENCE_CONTRACT"
    print("G10R_TCP124_PC_UART_LIFECYCLE_JOIN=PASS_FIXTURE_ONLY")
    print(f"G10R_TCP124_JOIN_NEGATIVES={len(cases)}_REJECTED")
    print("G10R_TCP124_PHYSICAL=NOT_RUN")

if __name__=="__main__":
    p=argparse.ArgumentParser(description=__doc__)
    g=p.add_mutually_exclusive_group(required=True)
    g.add_argument("--selftest",action="store_true")
    g.add_argument("--manifest")
    args=p.parse_args()
    if args.selftest:selftest()
    else:
        with open(args.manifest,encoding="utf-8") as f:payload=json.load(f)
        result=evaluate(payload);print(json.dumps(result,indent=2))
        raise SystemExit(0 if result["status"]=="PASS_OFFLINE_SOURCE_EVIDENCE_CONTRACT" else 2)
