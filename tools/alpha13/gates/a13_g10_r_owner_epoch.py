#!/usr/bin/env python3
"""G10-R R3: contrato OFFLINE de owner/epoch/close, NO telemetría física.

Un PASS verifica la coherencia del registro de eventos; no prueba que
el firmware haya emitido realmente los eventos. --selftest usa fixtures
SINTÉTICOS y mutaciones negativas. Sin COM, sockets ni upload.
"""
from __future__ import annotations

import argparse
import copy
import json
from pathlib import Path
import sys

SCHEMA = "G10-R-OWNER-EPOCH-V1"
OPERATIONS = {"ALLOC", "CLOSE_BUSY", "RETRY_CLOSE", "CLOSE_CONFIRMED",
              "RECYCLE", "CLOSE_STALE", "CANCEL", "SESSION_END"}


def evaluate(doc: dict) -> dict:
    failures: list[str] = []
    def bad(message: str):
        if message not in failures:
            failures.append(message)
    if not isinstance(doc, dict) or doc.get("schema") != SCHEMA:
        return {"status": "REVIEW_STOP", "failures": ["SCHEMA"]}
    if doc.get("origin") not in {"SYNTHETIC_TEST", "DIAGNOSTIC_CAPTURE_UNVERIFIED"}:
        bad("PROVENANCE_ORIGIN_UNSUPPORTED")
    events = doc.get("events")
    if not isinstance(events, list) or not events or len(events) > 100000:
        return {"status": "REVIEW_STOP", "failures": ["EVENT_COUNT"]}
    current: dict[int, tuple[str, int] | None] = {}
    highest: dict[int, int] = {}
    pending: set[tuple[int, str, int]] = set()
    closed: set[tuple[int, str, int]] = set()
    recycled: set[tuple[int, str, int]] = set()
    count = {k: 0 for k in OPERATIONS}
    prev_seq = -1
    for event in events:
        if not isinstance(event, dict):
            bad("MALFORMED_EVENT")
            continue
        op, sock = event.get("op"), event.get("socket")
        owner, epoch, seq = event.get("owner"), event.get("epoch"), event.get("seq")
        if (op not in OPERATIONS or type(sock) is not int or not 0 <= sock < 8
            or not isinstance(owner, str) or not owner or type(epoch) is not int
            or epoch < 1 or type(seq) is not int or seq <= prev_seq):
            bad("EVENT_CONTRACT")
            continue
        prev_seq = seq
        count[op] += 1
        token = (sock, owner, epoch)
        live = current.get(sock)
        if op == "ALLOC":
            if live is not None:
                bad("ALLOC_BEFORE_CLOSE_OR_RECYCLE")
            if epoch <= highest.get(sock, 0):
                bad("EPOCH_NOT_ADVANCED")
            highest[sock] = max(epoch, highest.get(sock, 0))
            current[sock] = (owner, epoch)
        elif op == "CLOSE_BUSY":
            if live != (owner, epoch):
                bad("BUSY_CLOSE_NOT_CURRENT_OWNER")
            if token in closed:
                bad("BUSY_CLOSE_ALREADY_CLOSED")
            pending.add(token)
        elif op == "RETRY_CLOSE":
            if live != (owner, epoch) or token not in pending:
                bad("RETRY_WITHOUT_CURRENT_PENDING_TOKEN")
        elif op == "CLOSE_CONFIRMED":
            if live != (owner, epoch):
                bad("CLOSE_OTHER_OWNER_OR_STALE")
            if token in closed:
                bad("DOUBLE_CLOSE")
            current[sock] = None
            pending.discard(token)
            closed.add(token)
        elif op == "RECYCLE":
            if live is None:
                bad("RECYCLE_WITHOUT_OWNER")
            elif live != (owner, epoch):
                bad("RECYCLE_NON_OWNER")
            else:
                recycled.add(token)
                pending.discard(token)
                current[sock] = None
        elif op == "CLOSE_STALE":
            if token not in recycled or live == (owner, epoch):
                bad("FALSE_STALE_OR_CURRENT_OWNER")
            if token in closed:
                bad("STALE_AFTER_CONFIRMED_CLOSE")
        elif op == "CANCEL":
            if live != (owner, epoch):
                bad("CANCEL_NON_OWNER")
            if token not in pending:
                bad("CANCEL_MUST_KEEP_PENDING_LEASE")
        elif op == "SESSION_END":
            if live != (owner, epoch):
                bad("SESSION_END_NOT_CURRENT")
            # Salir de protocolo lógico SIN llamar CLOSE no libera el socket.
            if token in pending:
                bad("ABANDONED_DEFERRED_CLOSE")
            else:
                bad("SESSION_END_WITHOUT_PHYSICAL_CLOSE")
    if pending:
        bad("DEFERRED_CLOSE_NEVER_RESOLVED")
    if any(value is not None for value in current.values()):
        bad("ACTIVE_LEASE_NOT_CLOSED_OR_RECYCLED")
    if count["ALLOC"] < 2 or count["CLOSE_BUSY"] < 1 or count["RETRY_CLOSE"] < 1 or count["RECYCLE"] < 1 or count["CLOSE_STALE"] < 1:
        bad("MISSING_LIFECYCLE_COVERAGE")
    return {"status": "PASS_OFFLINE_CONTRACT" if not failures else "REVIEW_STOP",
            "failures": failures, "counts": count,
            "physical_status": "NOT_ATTESTED_FROM_OFFLINE_EVENTS"}


def fixture() -> dict:
    return {"schema": SCHEMA, "origin": "SYNTHETIC_TEST", "events": [
        {"seq": 1, "op": "ALLOC", "socket": 2, "owner": "DNS", "epoch": 15},
        {"seq": 2, "op": "CLOSE_BUSY", "socket": 2, "owner": "DNS", "epoch": 15},
        {"seq": 3, "op": "CANCEL", "socket": 2, "owner": "DNS", "epoch": 15},
        {"seq": 4, "op": "RETRY_CLOSE", "socket": 2, "owner": "DNS", "epoch": 15},
        {"seq": 5, "op": "CLOSE_CONFIRMED", "socket": 2, "owner": "DNS", "epoch": 15},
        {"seq": 6, "op": "ALLOC", "socket": 2, "owner": "DHCP", "epoch": 16},
        {"seq": 7, "op": "CLOSE_BUSY", "socket": 2, "owner": "DHCP", "epoch": 16},
        {"seq": 8, "op": "RECYCLE", "socket": 2, "owner": "DHCP", "epoch": 16},
        {"seq": 9, "op": "ALLOC", "socket": 2, "owner": "TCP", "epoch": 17},
        {"seq": 10, "op": "CLOSE_STALE", "socket": 2, "owner": "DHCP", "epoch": 16},
        {"seq": 11, "op": "CLOSE_CONFIRMED", "socket": 2, "owner": "TCP", "epoch": 17},
    ]}


def selftest() -> None:
    base = fixture()
    assert evaluate(base)["status"] == "PASS_OFFLINE_CONTRACT"
    mutators = [
        ("missing-retry", lambda d: d["events"].pop(3)),
        ("forget-close", lambda d: d["events"].pop(4)),
        ("double-close", lambda d: d["events"].append(dict(d["events"][-1], seq=12))),
        ("close-new-owner", lambda d: d["events"][-1].update(owner="DHCP",epoch=16)),
        ("false-stale", lambda d: d["events"][9].update(owner="TCP",epoch=17)),
        ("recycled-epoch", lambda d: d["events"][8].update(epoch=16)),
        ("stolen-owner", lambda d: d["events"][6].update(owner="DNS")),
        ("premature-alloc", lambda d: d["events"].insert(3, {"seq":3,"op":"ALLOC","socket":2,"owner":"EXTRA","epoch":18})),
        ("absent-physical-close", lambda d: d["events"][4].update(op="SESSION_END")),
        ("pending-abandoned", lambda d: d["events"][3].update(op="SESSION_END")),
        ("bus-busy-closes", lambda d: d["events"][1].update(op="CLOSE_CONFIRMED")),
        ("no-lease-identity", lambda d: d["events"][3].update(epoch=0)),
        ("unknown-provenance", lambda d: d.update(origin="FIELD_VERIFIED")),
        ("socket-overflow", lambda d: d["events"][0].update(socket=8)),
    ]
    for name,mutate in mutators:
        d = copy.deepcopy(base)
        mutate(d)
        if evaluate(d)["status"] != "REVIEW_STOP":
            raise AssertionError("NEGATIVE_NOT_REJECTED_"+name)
    print("G10_R_R3_SYNTHETIC_NEGATIVES=%d_PASS_REJECTED" % len(mutators))
    print("G10_R_R3_CONTRACT=PASS_OFFLINE_ONLY")
    print("G10_R_R3_INSTRUMENTED_FIRMWARE=NOT_IMPLEMENTED")
    print("G10_R_R3_PHYSICAL=NOT_RUN")


def main() -> int:
    parser=argparse.ArgumentParser(description=__doc__)
    x=parser.add_mutually_exclusive_group(required=True)
    x.add_argument("--selftest",action="store_true")
    x.add_argument("--replay",type=Path)
    a=parser.parse_args()
    if a.selftest:
        selftest()
        return 0
    try:
        document=json.loads(a.replay.read_text(encoding="utf-8-sig"))
        result=evaluate(document)
        print(json.dumps(result,indent=2,sort_keys=True))
        return 0 if result["status"]=="PASS_OFFLINE_CONTRACT" else 2
    except (OSError,ValueError,UnicodeError) as e:
        print("G10_R_R3_REVIEW_STOP="+type(e).__name__)
        return 2

if __name__=="__main__":
    sys.exit(main())
