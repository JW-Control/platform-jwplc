#!/usr/bin/env python3
"""PRE-H TCP 1/2/4: contrato offline; NO abre sockets, COM ni ejecuta upload."""
from __future__ import annotations
import argparse
import copy
import json

CAPACITY = 8
TARGETS = (1, 2, 4)
REQUIRED = {"session", "client", "socket", "epoch", "allocation", "owner",
            "start_ms", "end_ms", "fc03_ok", "close_confirmed"}

def evaluate(events, target, reserved=0):
    errors = []
    if target not in TARGETS or type(reserved) is not int or not 0 <= reserved <= CAPACITY:
        return {"status": "REVIEW_STOP", "errors": ["INVALID_PARAMETERS"]}
    if not isinstance(events, list) or len(events) != target:
        return {"status": "REVIEW_STOP", "errors": ["INCOMPLETE_CLIENT_SET"]}
    identities = set()
    sessions = set()
    windows = []
    for ev in events:
        if not isinstance(ev, dict) or not REQUIRED <= ev.keys():
            errors.append("INCOMPLETE_EVIDENCE")
            continue
        nums = ("socket", "epoch", "allocation", "owner", "start_ms", "end_ms")
        if any(type(ev[k]) is not int for k in nums):
            errors.append("INVALID_NUMERIC_EVIDENCE")
            continue
        if not (0 <= ev["socket"] < CAPACITY and ev["epoch"] > 0
                and ev["allocation"] > 0 and ev["owner"] > 0
                and 0 <= ev["start_ms"] < ev["end_ms"]):
            errors.append("INVALID_LEASE_OR_WINDOW")
        if type(ev["client"]) is not int or not 0 <= ev["client"] < target:
            errors.append("INVALID_CLIENT")
        ident = (ev["socket"], ev["epoch"], ev["allocation"], ev["owner"])
        if ident in identities:
            errors.append("DUPLICATE_LEASE")
        identities.add(ident)
        if ev["socket"] in sessions:
            errors.append("SOCKET_REUSED_WHILE_ACTIVE")
        sessions.add(ev["socket"])
        if ev["fc03_ok"] is not True or ev["close_confirmed"] is not True:
            errors.append("FC03_OR_CLOSE_NOT_CONFIRMED")
        windows.append((ev["start_ms"], ev["end_ms"]))
    if len(windows) == target:
        # Half-open intervals: end==start is NOT simultaneous.
        peak = max(sum(start <= t < end for start, end in windows)
                   for t in sorted({start for start, _ in windows}))
        if peak != target:
            errors.append("NO_FULL_SIMULTANEITY")
    else:
        peak = 0
    if reserved + target > CAPACITY:
        errors.append("W5500_SOCKET_CAPACITY_EXCEEDED")
    if len({e.get("client") for e in events if isinstance(e, dict)}) != target:
        errors.append("CLIENT_IDENTITY_MISSING")
    return {"status": "PASS_OFFLINE_CONTRACT_ONLY" if not errors else "REVIEW_STOP",
            "errors": sorted(set(errors)), "target": target, "peak": peak,
            "reserved": reserved, "physical": "NOT_RUN"}

def fixture(n):
    return [{"session": "fixture", "client": i, "socket": i,
             "epoch": 1, "allocation": i+1, "owner": i+100,
             "start_ms": i*10, "end_ms": 100, "fc03_ok": True,
             "close_confirmed": True} for i in range(n)]

def selftest():
    for n in TARGETS:
        assert evaluate(fixture(n), n, reserved=CAPACITY-n)["status"] == "PASS_OFFLINE_CONTRACT_ONLY"
    cases = []
    v = fixture(4); v[3]["start_ms"] = 100; v[3]["end_ms"] = 150
    cases.append(("SEQUENTIAL_NOT_CONCURRENT", v, 4, 0))
    v = fixture(4); v[3]["socket"] = v[2]["socket"]
    cases.append(("DOUBLE_SOCKET", v, 4, 0))
    v = fixture(4); v[2]["allocation"] = 0
    cases.append(("INVALID_ALLOCATION", v, 4, 0))
    v = fixture(4); v[1]["fc03_ok"] = False
    cases.append(("FAILED_FC03", v, 4, 0))
    v = fixture(4); v[1]["close_confirmed"] = False
    cases.append(("UNCONFIRMED_CLOSE", v, 4, 0))
    cases.append(("EXCESS_CAPACITY", fixture(4), 4, 5))
    cases.append(("INCOMPLETE_SET", fixture(2), 4, 0))
    v = fixture(4); v[3]["client"] = 2
    cases.append(("DUPLICATE_CLIENT", v, 4, 0))
    v = fixture(4); v[0].pop("owner")
    cases.append(("MISSING_OWNER", v, 4, 0))
    for label, ev, n, reserved in cases:
        assert evaluate(ev, n, reserved)["status"] == "REVIEW_STOP", label
    print("G10_R_TCP124_HOST_CONTRACT=PASS")
    print(f"G10_R_TCP124_NEGATIVES={len(cases)}_REJECTED")
    print("G10_R_TCP124_PHYSICAL=NOT_RUN")
    return 0

def main():
    ap = argparse.ArgumentParser(description=__doc__)
    group = ap.add_mutually_exclusive_group(required=True)
    group.add_argument("--selftest", action="store_true")
    group.add_argument("--evaluate", metavar="JSON_FILE")
    args = ap.parse_args()
    if args.selftest:
        return selftest()
    with open(args.evaluate, encoding="utf-8") as stream:
        payload = json.load(stream)
    result = evaluate(payload["events"], payload["target"], payload.get("reserved", 0))
    print(json.dumps(result, sort_keys=True))
    return 0 if result["status"] == "PASS_OFFLINE_CONTRACT_ONLY" else 2

if __name__ == "__main__":
    raise SystemExit(main())
