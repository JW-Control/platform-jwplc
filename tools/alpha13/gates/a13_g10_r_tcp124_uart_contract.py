#!/usr/bin/env python3
"""A13 PRE-H: verificar procedencia UART cruda, nunca flags autodeclarados.

No infiere identidad TCP desde MBAP TID: requiere línea de vínculo emitida
por una futura instrumentación del servidor candidato y lifecycle real.
Este archivo no genera eventos ni habilita captura física.
"""
from __future__ import annotations
import argparse
import copy
import re

TOKEN=r"[0-9a-f]{24}"
BIND=re.compile(rf"G10R_TCP_LEASE:({TOKEN}) TID=(\d+) SID=(\d+) EPOCH=(\d+) ALLOC=(\d+) OWNER=(\d+)")
LIFE=re.compile(rf"G10R_EVT:({TOKEN}) SEQ=(\d+) KIND=(\d+) SID=(\d+) EPOCH=(\d+) ALLOC=(\d+) OWNER=(\d+)")
START=re.compile(rf"G10R_STARTED:({TOKEN}) RTU=[01]")
END=re.compile(rf"G10R_EVT_END:({TOKEN})")
TCP_HEAD=re.compile(rf"G10R_TCP_HEAD:({TOKEN}) COUNT=(\\d+) DROPPED=(\\d+)")
TCP_END=re.compile(rf"G10R_TCP_END:({TOKEN})")
def evaluate(lines,token,transaction):
    errors=[]
    if not isinstance(lines,list) or not all(isinstance(s,str) for s in lines):
        return {"status":"REVIEW_STOP","errors":["NON_RAW_UART"]}
    if not re.fullmatch(TOKEN,token) or type(transaction) is not int or not 0<=transaction<65536:
        return {"status":"REVIEW_STOP","errors":["INVALID_INPUT"]}
    started=0;ended=0;tcp_head=[];tcp_ended=0;leases=[];events=[];foreign=False
    for line in lines:
        if m:=START.fullmatch(line):
            if m[1]==token:started+=1
            else:foreign=True
        elif m:=BIND.fullmatch(line):
            if m[1]!=token:foreign=True
            elif int(m[3])==transaction:
                if int(m[5])!=1:errors.append("CAPTURE_NOT_VALID")
                leases.append(tuple(map(int,m.groups()[3:4]+m.groups()[5:])))
        elif m:=TCP_HEAD.fullmatch(line):
            if m[1]!=token:foreign=True
            else:tcp_head.append((int(m[2]),int(m[3])))
        elif m:=TCP_END.fullmatch(line):
            if m[1]!=token:foreign=True
            else:tcp_ended+=1
        elif m:=LIFE.fullmatch(line):
            if m[1]!=token:foreign=True
            else:events.append(tuple(map(int,m.groups()[1:])))
        elif m:=END.fullmatch(line):
            if m[1]==token:ended+=1
            else:foreign=True
        elif line.startswith(("G10R_TCP_OBS:","G10R_TCP_HEAD:","G10R_TCP_END:","G10R_EVT:","G10R_STARTED:","G10R_EVT_END:")):
            errors.append("MALFORMED_PROTOCOL_LINE")
    if foreign:errors.append("FOREIGN_SESSION")
    if started!=1 or ended!=1 or tcp_ended!=1 or len(tcp_head)!=1:
        errors.append("MISSING_SESSION_BOUNDARIES")
    if len(tcp_head)==1 and (tcp_head[0][1]!=0 or tcp_head[0][0]<len(leases)):
        errors.append("TCP_TRACE_INCOMPLETE")
    if len(leases)!=1:errors.append("NO_UNIQUE_TRANSACTION_TO_LEASE_BIND")
    if len(leases)==1:
        sid,epoch,allocation,owner=leases[0]
        if not 0<=sid<8 or min(epoch,allocation,owner)<=0:
            errors.append("INVALID_LEASE")
        rows=[(seq,kind) for seq,kind,s,e,a,o in events if
              (s,e,a,o)==(sid,epoch,allocation,owner)]
        queues=[seq for seq,k in rows if k==6]
        drains=[seq for seq,k in rows if k==7]
        if len(queues)!=1 or len(drains)!=1 or queues[0]>=drains[0]:
            errors.append("CLOSE_NOT_PROVEN")
        if any(kind==2 and (not drains or seq<drains[0]) for seq,kind in rows):
            errors.append("RECYCLE_BEFORE_DRAIN")
    sequences=[r[0] for r in events]
    if sequences!=sorted(set(sequences)) or (sequences and sequences!=list(range(sequences[0],sequences[-1]+1))):
        errors.append("EVENT_SEQUENCE_GAP_OR_DUPLICATE")
    return {"status":"PASS_UART_PROTOCOL_CONTRACT_ONLY" if not errors else "REVIEW_STOP",
            "errors":sorted(set(errors)),"physical":"NOT_ATTESTED"}

def fixture():
    t="a"*24
    l=[f"G10R_STARTED:{t} RTU=1",
       f"G10R_TCP_HEAD:{t} COUNT=1 DROPPED=0",
       f"G10R_TCP_OBS:{t} SEQ=1 TID=47 SID=2 CAPTURE=1 EPOCH=3 ALLOC=5 OWNER=9",
       f"G10R_TCP_END:{t}",
       f"G10R_EVT:{t} SEQ=1 KIND=1 SID=2 EPOCH=3 ALLOC=5 OWNER=9",
       f"G10R_EVT:{t} SEQ=2 KIND=6 SID=2 EPOCH=3 ALLOC=5 OWNER=9",
       f"G10R_EVT:{t} SEQ=3 KIND=7 SID=2 EPOCH=3 ALLOC=5 OWNER=9",
       f"G10R_EVT_END:{t}"]
    return t,l
def selftest():
    token,lines=fixture()
    assert evaluate(lines,token,47)["status"]=="PASS_UART_PROTOCOL_CONTRACT_ONLY"
    cases=[]
    for name,fn in (
        ("missing_bind",lambda x:x.pop(2)),
        ("double_bind",lambda x:x.insert(3,x[2])),
        ("wrong_tid",lambda x:x.__setitem__(2,x[2].replace("TID=47","TID=48"))),
        ("wrong_owner",lambda x:x.__setitem__(2,x[2].replace("OWNER=9","OWNER=10"))),
        ("missing_close",lambda x:x.pop(-2)),
        ("missing_queue",lambda x:x.pop(-3)),
        ("gap",lambda x:x.__setitem__(-2,x[-2].replace("SEQ=3","SEQ=9"))),
        ("duplicate_seq",lambda x:x.__setitem__(-2,x[-2].replace("SEQ=3","SEQ=2"))),
        ("bad_sid",lambda x:x.__setitem__(2,x[2].replace("SID=2","SID=8"))),
        ("no_start",lambda x:x.pop(0)),
        ("no_end",lambda x:x.pop()),
        ("foreign_token",lambda x:x.__setitem__(2,x[2].replace(token,"b"*24))),
        ("busy_capture",lambda x:x.__setitem__(2,x[2].replace("CAPTURE=1","CAPTURE=2"))),
        ("trace_drop",lambda x:x.__setitem__(1,x[1].replace("DROPPED=0","DROPPED=1"))),
    ):
        v=copy.deepcopy(lines);fn(v);cases.append((name,v))
    for name,v in cases:
        assert evaluate(v,token,47)["status"]=="REVIEW_STOP",name
    print("G10R_TCP124_UART_CONTRACT=PASS_FIXTURE_ONLY")
    print(f"G10R_TCP124_UART_MUTANTS={len(cases)}_REJECTED")
    print("G10R_TCP124_UART_SOURCE_EMITTER=NOT_IMPLEMENTED")
if __name__=="__main__":
    p=argparse.ArgumentParser()
    p.add_argument("--selftest",action="store_true",required=True)
    p.parse_args();selftest()
