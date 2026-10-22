#!/usr/bin/env python3
"""G10-R R3: lector UART de eventos históricos, OFFLINE para pruebas.

Cada hueco/dropped/overrun se registra como INSTRUMENTATION_GAP y no permite
calificar R3 físico. Nunca infiere CLOSE a partir de una mera snapshot.
No abre COM, sockets ni reconfigura placas.
"""
from __future__ import annotations

import argparse
import json


def parse_event(line:str,token:str):
    prefix="G10R_EVT:"+token+" "
    if not line.startswith(prefix):return None
    values={}
    for item in line[len(prefix):].split():
        name,sep,value=item.partition("=")
        if not sep or name in values or not value.isdecimal():return None
        values[name]=int(value)
    if set(values)!={"SEQ","KIND","SID","EPOCH","ALLOC","OWNER"}:
        return None
    if values["SEQ"]<1 or not 1<=values["KIND"]<=9 or not 0<=values["SID"]<8:
        return None
    return {"seq":values["SEQ"],"kind":values["KIND"],"socket":values["SID"],
            "epoch":values["EPOCH"],"allocation":values["ALLOC"],
            "owner":values["OWNER"]}


class TraceCollector:
    def __init__(self):
        self.token=""
        self.phase=""
        self.cursor=0
        self.batch=None
        self.waiting=False
        self.request_at=0.0
        self.events=[]
        self.issues=[]
        self.busy_count=0
        self.dropped=0
        self.samples=0

    def begin(self,token:str,phase:str):
        self.finish()
        self.token=token
        self.phase=phase

    def finish(self):
        if self.batch is not None or self.waiting:
            self.issues.append("TRACE_INCOMPLETE_SESSION_END")
        self.batch=None
        self.waiting=False

    def request(self,now:float):
        if not self.token:return None
        if self.waiting:
            if now-self.request_at<3.0:return None
            self.issues.append("TRACE_UART_REPLY_TIMEOUT")
            self.batch=None
            self.waiting=False
        self.waiting=True
        self.request_at=now
        return "G10R_EVENTS:"+self.token+":AFTER="+str(self.cursor)

    def receive(self,line:str,at:float):
        if not self.token:return
        header="G10R_EVT_HEAD:"+self.token+" "
        if line.startswith(header):
            fields={}
            for piece in line[len(header):].split():
                k,sep,v=piece.partition("=")
                if not sep or k in fields or not v.isdecimal():
                    self.issues.append("TRACE_HEAD_INVALID")
                    return
                fields[k]=int(v)
            if set(fields)!={"CODE","COUNT","NEWEST","DROPPED"}:
                self.issues.append("TRACE_HEAD_FIELDS_MISSING")
                return
            if self.batch is not None:self.issues.append("TRACE_OVERLAPPING_BATCH")
            self.batch={"phase":self.phase,"at":at,"fields":fields,"events":[]}
            return
        if line=="G10R_EVT_END:"+self.token:
            b=self.batch
            self.batch=None
            self.waiting=False
            if b is None:
                self.issues.append("TRACE_END_WITHOUT_HEADER")
                return
            f,items=b["fields"],b["events"]
            self.samples+=1
            if f["DROPPED"]>self.dropped:
                self.issues.append("TRACE_RING_DROPPED_EVENTS")
            if f["DROPPED"]<self.dropped:
                self.issues.append("TRACE_DROP_COUNTER_REGRESSION")
            self.dropped=f["DROPPED"]
            if f["CODE"]==3:
                self.busy_count+=1
                if items or f["COUNT"]!=0:self.issues.append("TRACE_BUSY_WITH_EVENTS")
                return
            if f["CODE"]==2:
                self.issues.append("TRACE_HISTORY_OVERRUN")
            if f["CODE"] not in (1,2):
                self.issues.append("TRACE_READ_INVALID")
                return
            if len(items)!=f["COUNT"] or len(items)>8:
                self.issues.append("TRACE_EVENT_COUNT_MISMATCH")
                return
            previous=self.cursor
            for item in items:
                if item["seq"]!=previous+1:
                    self.issues.append("TRACE_SEQUENCE_GAP")
                previous=item["seq"]
            if previous>f["NEWEST"]:
                self.issues.append("TRACE_CURSOR_AHEAD_OF_NEWEST")
            if items:
                self.cursor=items[-1]["seq"]
                self.events.extend({**item,"phase":b["phase"],"t":b["at"]}
                                   for item in items)
            return
        if line.startswith("G10R_EVT:"+self.token+" "):
            if self.batch is None:
                self.issues.append("TRACE_EVENT_WITHOUT_HEAD")
                return
            val=parse_event(line,self.token)
            if val is None:self.issues.append("TRACE_EVENT_INVALID")
            else:self.batch["events"].append(val)

    def report(self):
        return {"status":"OBSERVATIONAL_COMPLETE_NOT_PHYSICAL_PASS"
                if not self.issues else "INSTRUMENTATION_GAP_REVIEW_STOP",
                "cursor":self.cursor,"dropped":self.dropped,
                "samples":self.samples,"busy_count":self.busy_count,
                "issues":list(self.issues),"events":list(self.events)}


def fixture_line(token:str,seq:int,kind:int=1):
    return (f"G10R_EVT:{token} SEQ={seq} KIND={kind} SID=2 EPOCH=1"
            f" ALLOC=17 OWNER=22")


def test():
    tok="a"*24
    c=TraceCollector();c.begin(tok,"R1")
    assert c.request(1.0)=="G10R_EVENTS:"+tok+":AFTER=0"
    assert c.request(2.0) is None
    c.receive("G10R_EVT_HEAD:"+tok+" CODE=1 COUNT=2 NEWEST=2 DROPPED=0",2.1)
    c.receive(fixture_line(tok,1),2.2)
    c.receive(fixture_line(tok,2,6),2.3)
    c.receive("G10R_EVT_END:"+tok,2.4)
    assert c.report()["status"]=="OBSERVATIONAL_COMPLETE_NOT_PHYSICAL_PASS"
    assert c.cursor==2 and len(c.events)==2
    assert c.request(4.0)=="G10R_EVENTS:"+tok+":AFTER=2"
    c.receive("G10R_EVT_HEAD:"+tok+" CODE=3 COUNT=0 NEWEST=2 DROPPED=0",4.1)
    c.receive("G10R_EVT_END:"+tok,4.2)
    assert c.busy_count==1 and not c.issues
    mutants=[
      ("missing_seq",["G10R_EVT_HEAD:"+tok+" CODE=1 COUNT=1 NEWEST=4 DROPPED=0",
                      fixture_line(tok,4),"G10R_EVT_END:"+tok]),
      ("overrun",["G10R_EVT_HEAD:"+tok+" CODE=2 COUNT=0 NEWEST=80 DROPPED=0",
                  "G10R_EVT_END:"+tok]),
      ("dropped",["G10R_EVT_HEAD:"+tok+" CODE=1 COUNT=0 NEWEST=2 DROPPED=1",
                  "G10R_EVT_END:"+tok]),
      ("missing_event",["G10R_EVT_HEAD:"+tok+" CODE=1 COUNT=1 NEWEST=3 DROPPED=0",
                        "G10R_EVT_END:"+tok]),
      ("bad_sid",["G10R_EVT_HEAD:"+tok+" CODE=1 COUNT=1 NEWEST=3 DROPPED=0",
                  fixture_line(tok,3).replace("SID=2","SID=8"),
                  "G10R_EVT_END:"+tok]),
      ("invalid_read",["G10R_EVT_HEAD:"+tok+" CODE=4 COUNT=0 NEWEST=2 DROPPED=0",
                       "G10R_EVT_END:"+tok]),
    ]
    for name,lines in mutants:
        x=TraceCollector();x.begin(tok,"R3");x.request(0.0)
        for i,line in enumerate(lines):x.receive(line,.1*i)
        assert x.report()["status"]=="INSTRUMENTATION_GAP_REVIEW_STOP",name
    x=TraceCollector();x.begin(tok,"R4");x.request(1.)
    x.request(4.1)
    assert "TRACE_UART_REPLY_TIMEOUT" in x.issues
    print("G10_R_R3_TRACE_HOST_NEGATIVES=7_PASS_REJECTED")
    print("G10_R_R3_TRACE_HOST=PASS_OFFLINE_ONLY")


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument("--selftest",action="store_true",required=True)
    args=p.parse_args()
    if args.selftest:test()
    return 0


if __name__=="__main__":
    raise SystemExit(main())
