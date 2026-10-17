#!/usr/bin/env python3
"""G10-R R3: contrato OFFLINE de sonda UDP bajo mutex W5500 real.

La sonda sólo es una SUBFASE de la campaña H; no cubre DNS/DHCP/TCP,
ni reemplaza R1/R2/R4, ni autoriza upload, COM o RJ45.
"""
from __future__ import annotations

import argparse
import copy
import json
import re

TOKEN=re.compile(r"^[0-9a-f]{24}$")
PREFIX="G10R_R3_UDP:"
SUCCESS=["ARMED","CLOSE_DURING_HOLD","AWAIT_LIFECYCLE_REVIEW"]
KNOWN=set(SUCCESS)|{
    "MUTEX_NOT_ACQUIRED","PREPARE_FAILED","TASK_CREATION_FAILED",
    "CLOSE_RACE_REVIEW","INJECTION_INCOMPLETE_REVIEW","HOLD_TIMING_REVIEW",
}


def evaluate(lines:list[str],trace:dict,token:str)->dict:
    errors=[]
    if not isinstance(token,str) or not TOKEN.fullmatch(token):
        return {"status":"REVIEW_STOP","errors":["INVALID_SESSION_TOKEN"]}
    states=[]
    lease=[]
    holds=[]
    for line in lines:
        if not isinstance(line,str):continue
        if line.startswith("G10R_R3_HOLD:"):
            m=re.fullmatch("G10R_R3_HOLD:"+token+r" US=(\d+)",line)
            if m is None:errors.append("INVALID_OR_FOREIGN_HOLD_DURATION")
            else:holds.append(int(m.group(1)))
            continue
        if line.startswith("G10R_R3_LEASE:"):
            m=re.fullmatch("G10R_R3_LEASE:"+token+
                r" SID=(\d+) EPOCH=(\d+) ALLOC=(\d+) OWNER=(\d+)",line)
            if m is None:
                errors.append("INVALID_OR_FOREIGN_R3_LEASE")
            else:
                values=tuple(map(int,m.groups()))
                if not (0<=values[0]<8 and all(x>0 for x in values[1:])):
                    errors.append("BAD_R3_LEASE_IDENTITY")
                lease.append(values)
            continue
        if not line.startswith(PREFIX):continue
        marker=PREFIX+token+" STATE="
        if not line.startswith(marker):
            errors.append("FOREIGN_SESSION_R3_MESSAGE")
            continue
        state=line[len(marker):]
        if state not in KNOWN:
            errors.append("INVALID_PROBE_STATE")
        states.append(state)
    if states!=SUCCESS:
        errors.append("R3_UDP_PHASE_INCOMPLETE_OR_OUT_OF_ORDER")
    if len(lease)!=1:
        errors.append("R3_EXACT_LEASE_NOT_RECORDED")
    if len(holds)!=1 or not 60000<=holds[0]<=120000:
        errors.append("R3_REAL_MUTEX_HOLD_DURATION_OUT_OF_RANGE")
    if not isinstance(trace,dict):
        return {"status":"REVIEW_STOP","errors":errors+["TRACE_MISSING"]}
    if trace.get("status")!="OBSERVATIONAL_COMPLETE_NOT_PHYSICAL_PASS":
        errors.append("TRACE_GAP_OR_UNVERIFIED_STATUS")
    if trace.get("dropped")!=0 or trace.get("issues"):
        errors.append("TRACE_LOSS_OR_GAP")
    events=trace.get("events")
    if not isinstance(events,list):
        return {"status":"REVIEW_STOP","errors":errors+["TRACE_EVENTS_MISSING"]}
    queues={}
    drains={}
    for ev in events:
        if not isinstance(ev,dict):
            errors.append("MALFORMED_TRACE_EVENT")
            continue
        k=ev.get("kind")
        if k not in (6,7):continue
        key=(ev.get("socket"),ev.get("epoch"),
             ev.get("allocation"),ev.get("owner"))
        if any(type(x) is not int or x<=0 for x in key[1:]) or (
            type(key[0]) is not int or not 0<=key[0]<8):
            errors.append("BAD_LEASE_IDENTITY")
            continue
        seq=ev.get("seq")
        if type(seq) is not int or seq<1:
            errors.append("INVALID_SEQUENCE")
            continue
        if k==6:queues.setdefault(key,[]).append(seq)
        else:drains.setdefault(key,[]).append(seq)
    # En un gate concurrente también se cierran sockets TCP/DNS/DHCP:
    # solo el lease exacto anunciado por la sonda se atribuye a R3-UDP.
    target=lease[0] if len(lease)==1 else None
    matches=0
    if target is not None:
        starts=queues.get(target,[])
        finishes=drains.get(target,[])
        matches=sum(any(start<end for start in starts) for end in finishes)
        if matches!=1:
            errors.append("PROBE_LEASE_MISSING_OR_DOUBLE_DRAIN")
    else:
        errors.append("PROBE_LEASE_UNRESOLVED")
    return {"status":"R3_UDP_SOURCE_CONTRACT_ONLY_NOT_PHYSICAL_PASS"
            if not errors else "REVIEW_STOP",
            "errors":list(dict.fromkeys(errors)),
            "matches":matches,"states":states,
            "coverage":{"udp_stop":matches==1,"dns":False,
                        "dhcp":False,"tcp_1_2_4_clients":False},
            "r3_physical_campaign":"NOT_QUALIFIED"}


def fixture():
    t="a"*24
    lines=[f"G10R_R3_LEASE:{t} SID=3 EPOCH=1 ALLOC=25 OWNER=5"]
    lines += [f"{PREFIX}{t} STATE={state}" for state in SUCCESS]
    lines.insert(-1,f"G10R_R3_HOLD:{t} US=80000")
    trace={"status":"OBSERVATIONAL_COMPLETE_NOT_PHYSICAL_PASS",
           "dropped":0,"issues":[],"events":[
              {"seq":10,"kind":6,"socket":3,"epoch":1,
               "allocation":25,"owner":5},
              {"seq":11,"kind":7,"socket":3,"epoch":1,
               "allocation":25,"owner":5},
           ]}
    return t,lines,trace


def selftest():
    t,lines,trace=fixture()
    assert evaluate(lines,trace,t)["status"]=="R3_UDP_SOURCE_CONTRACT_ONLY_NOT_PHYSICAL_PASS"
    negatives=[
        ("missing_held_close",lambda l,r:l.pop(2)),
        ("wrong_order",lambda l,r:l.reverse()),
        ("lost_history",lambda l,r:r.update(dropped=1)),
        ("history_overrun",lambda l,r:r.update(issues=["TRACE_HISTORY_OVERRUN"])),
        ("owner_recycled",lambda l,r:r["events"][1].update(owner=99)),
        ("epoch_changed",lambda l,r:r["events"][1].update(epoch=3)),
        ("not_drained",lambda l,r:r["events"].pop()),
        ("not_queued",lambda l,r:r["events"].pop(0)),
        ("race_in_probe",lambda l,r:l.__setitem__(2,l[2].replace("CLOSE_DURING_HOLD",
                                                                "CLOSE_RACE_REVIEW"))),
        ("foreign_token",lambda l,r:l.__setitem__(0,l[0].replace("a"*24,"b"*24))),
        ("missing_end",lambda l,r:l.pop()),
        ("wrong_recorded_lease",lambda l,r:l.__setitem__(0,l[0].replace("ALLOC=25","ALLOC=77"))),
        ("missing_lease",lambda l,r:l.pop(0)),
        ("duplicate_lease",lambda l,r:l.insert(1,l[0])),
        ("missing_duration",lambda l,r:l.pop(-2)),
        ("overlong_mutex",lambda l,r:l.__setitem__(-2,l[-2].replace("80000","185000"))),
        ("double_drain",lambda l,r:r["events"].append({
            "seq":12,"kind":7,"socket":3,"epoch":1,"allocation":25,"owner":5})),
    ]
    for label, mutate in negatives:
        l=copy.deepcopy(lines);r=copy.deepcopy(trace)
        mutate(l,r)
        assert evaluate(l,r,t)["status"]=="REVIEW_STOP",label
    # Tráfico paralelo: el cierre de otro socket jamás se atribuye a la
    # sonda ni se convierte en "ambiguous" si el lease R3 es inequívoco.
    with_parallel=copy.deepcopy(trace)
    with_parallel["events"] += [
        {"seq":12,"kind":6,"socket":1,"epoch":1,
         "allocation":30,"owner":77},
        {"seq":13,"kind":7,"socket":1,"epoch":1,
         "allocation":30,"owner":77},
    ]
    assert evaluate(lines,with_parallel,t)["status"]==(
        "R3_UDP_SOURCE_CONTRACT_ONLY_NOT_PHYSICAL_PASS")
    print("G10_R_R3_UDP_PROBE_NEGATIVES=17_REJECTED")
    print("G10_R_R3_UDP_PROBE=PASS_OFFLINE_CONTRACT_ONLY")
    print("G10_R_R3_FULL_H=BLOCKED_NOT_RUN")


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument("--selftest",action="store_true",required=True)
    p.parse_args()
    selftest()
    return 0


if __name__=="__main__":
    raise SystemExit(main())
