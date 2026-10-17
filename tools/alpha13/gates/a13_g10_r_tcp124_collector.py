#!/usr/bin/env python3
"""Collector TCP124 bench: UART source-backed; no serial/COM/network access."""
from __future__ import annotations
import argparse
import re

FIELDS=("SEQ","TID","SID","CAPTURE","EPOCH","ALLOC","OWNER")
class TcpCollector:
    def __init__(self):
        self.batches=[]
        self.pending=None
        self.token=""
    def begin(self,token):
        self.finish()
        self.token=token
    def request(self):
        return "G10R_TCP_EVENTS:"+self.token if self.token else None
    def receive(self,line,at):
        if not line.startswith("G10R_TCP_"):return
        head="G10R_TCP_HEAD:"+self.token+" "
        row="G10R_TCP_OBS:"+self.token+" "
        tail="G10R_TCP_END:"+self.token
        if line.startswith(head):
            m=re.fullmatch(r"COUNT=(\d+) DROPPED=(\d+) OVERFLOW=([01])",line[len(head):])
            if self.pending:
                self.pending["status"]="REVIEW_STOP_INTERRUPTED"
                self.batches.append(self.pending)
            self.pending={"token":self.token,"at":at,"lines":[line],
                          "expected":int(m[1]) if m else -1,
                          "dropped":int(m[2]) if m else -1,
                          "overflow":int(m[3]) if m else 1,"rows":[]}
            return
        if line==tail:
            if self.pending is None:
                self.batches.append({"status":"REVIEW_STOP_END_WITHOUT_HEAD","lines":[line]})
                return
            b=self.pending;self.pending=None;b["lines"].append(line)
            seq=[r["SEQ"] for r in b["rows"]]
            b["status"]=("OBSERVATIONAL_COMPLETE_NOT_PHYSICAL_PASS" if
                b["expected"]==len(seq) and b["dropped"]==0 and
                b["overflow"]==0 and seq==sorted(set(seq))
                else "REVIEW_STOP_INCOMPLETE")
            self.batches.append(b);return
        if line.startswith(row) and self.pending is not None:
            self.pending["lines"].append(line)
            text=line[len(row):]
            pairs=text.split()
            vals={}
            for part in pairs:
                key,sep,value=part.partition("=")
                if not sep or key in vals or not value.isdecimal():
                    vals={};break
                vals[key]=int(value)
            if set(vals)==set(FIELDS) and 0<=vals["SID"]<8 and 0<=vals["TID"]<65536:
                self.pending["rows"].append(vals)
            return
        self.batches.append({"status":"REVIEW_STOP_FOREIGN_OR_MALFORMED",
                             "lines":[line]})
    def finish(self):
        if self.pending:
            self.pending["status"]="REVIEW_STOP_SESSION_END"
            self.batches.append(self.pending);self.pending=None
        self.token=""
    def report(self):
        return {"batches":self.batches,"status":
                "REVIEW_STOP" if any(b["status"].startswith("REVIEW_STOP") for b in self.batches)
                else "OBSERVATIONAL_ONLY_NOT_PHYSICAL_PASS"}

def selftest():
    c=TcpCollector();t="a"*24;c.begin(t)
    c.receive(f"G10R_TCP_HEAD:{t} COUNT=1 DROPPED=0 OVERFLOW=0",0)
    c.receive(f"G10R_TCP_OBS:{t} SEQ=1 TID=47 SID=2 CAPTURE=1 EPOCH=3 ALLOC=5 OWNER=9",1)
    c.receive(f"G10R_TCP_END:{t}",2)
    assert c.report()["status"]=="OBSERVATIONAL_ONLY_NOT_PHYSICAL_PASS"
    assert c.batches[0]["rows"][0]["OWNER"]==9
    c.begin(t)
    c.receive(f"G10R_TCP_HEAD:{t} COUNT=1 DROPPED=0 OVERFLOW=1",3)
    c.receive(f"G10R_TCP_END:{t}",4)
    assert c.report()["status"]=="REVIEW_STOP"
    print("G10R_TCP_COLLECTOR=PASS_SYNTHETIC_NO_HARDWARE")
if __name__=="__main__":
    p=argparse.ArgumentParser()
    p.add_argument("--selftest",action="store_true",required=True)
    p.parse_args();selftest()
