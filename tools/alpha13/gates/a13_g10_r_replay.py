#!/usr/bin/env python3
"""Evaluador OFFLINE G10-R R1/R2/R4: NO abre COM, socket ni firmware.

Acepta eventos JSON reales o sintéticos, valida su contrato y clasifica
métricas. Un PASS contractual NO ES prueba física certificada: requiere
correlación posterior de logs UART/PC y procedencia de ELF.
"""
from __future__ import annotations

import argparse
import copy
import json
import math
from pathlib import Path
import statistics
import sys

R1_MIN_S = 600.0
R4_MIN_S = 1800.0
HB_GAP_MAX_S = 3.0
RECOVERY_DEADLINE_S = 90.0
RECOVERY_SUCCESS_WINDOW_S = 30.0


def _fail(errors, why):
    if why not in errors:
        errors.append(why)


def evaluate(report: dict) -> dict:
    errors = []
    measures = {}
    if not isinstance(report, dict) or report.get("schema") != "G10-R-REPLAY-V1":
        return {"status": "REVIEW_STOP", "errors": ["REPLAY_SCHEMA"], "metrics": {}}
    if report.get("origin") not in ("SYNTHETIC_CI", "PC_UART_CAPTURE_UNVERIFIED"):
        _fail(errors, "REPLAY_ORIGIN")
    events = report.get("events")
    if not isinstance(events, list) or not events:
        return {"status": "REVIEW_STOP", "errors": ["REPLAY_EVENTS_EMPTY"], "metrics": {}}
    if len(events) > 500000:
        return {"status": "REVIEW_STOP", "errors": ["REPLAY_TOO_MANY_EVENTS"], "metrics": {}}
    timeline = {"R1": [], "R2": [], "R4": []}
    last_t = None
    for item in events:
        if not isinstance(item, dict):
            _fail(errors, "EVENT_NOT_OBJECT")
            continue
        t = item.get("t")
        p = item.get("phase")
        kind = item.get("kind")
        if type(t) not in (int, float) or not math.isfinite(t) or t < 0:
            _fail(errors, "EVENT_INVALID_TIMESTAMP")
            continue
        if last_t is not None and t < last_t:
            _fail(errors, "EVENT_TIME_NOT_MONOTONIC")
        last_t = t
        if p not in timeline or kind not in ("heartbeat", "traffic", "link_down", "link_up"):
            _fail(errors, "EVENT_INVALID_PHASE_OR_KIND")
            continue
        timeline[p].append(item)
        if kind == "traffic":
            if any(type(item.get(k)) is not bool for k in ("tcp", "udp", "rtu")):
                _fail(errors, "TRAFFIC_BOOLEAN_FIELDS_REQUIRED")
            if p in ("R1", "R4") and (item.get("tcp") is not True or
                                     item.get("udp") is not True or
                                     item.get("rtu") is not True):
                _fail(errors, "STEADY_TRAFFIC_FAILURE_" + p)
        if kind == "heartbeat":
            if not isinstance(item.get("boot"), str) or len(item["boot"]) < 4:
                _fail(errors, "BOOT_NONCE_REQUIRED")
            if type(item.get("eth")) is not int or item["eth"] not in (0, 1):
                _fail(errors, "ETH_FIELD_INVALID")
            if type(item.get("tcp")) is not int or item["tcp"] not in (0, 1):
                _fail(errors, "TCP_FIELD_INVALID")
            if not isinstance(item.get("ip"), str) or not item["ip"]:
                _fail(errors, "IP_FIELD_REQUIRED")
            for key in ("hb", "sd_ok", "sd_fail", "fram", "rtu_ok", "rtu_fail"):
                if type(item.get(key)) is not int or item[key] < 0:
                    _fail(errors, "COUNTER_INVALID_" + key.upper())
            if p in ("R1", "R4") and (item.get("eth") != 1 or
                                     item.get("tcp") != 1 or
                                     item.get("ip") == "0.0.0.0"):
                _fail(errors, "STEADY_ETHERNET_LOST_" + p)
            if item.get("sd_fail") != 0 or item.get("rtu_fail") != 0:
                _fail(errors, "PERIPHERAL_FAILURE_" + p)
    if errors:
        return {"status": "REVIEW_STOP", "errors": errors, "metrics": measures}

    all_hb = [e for e in events if e["kind"] == "heartbeat"]
    if not all_hb:
        _fail(errors, "HEARTBEATS_ABSENT")
    else:
        boots = {x["boot"] for x in all_hb}
        if len(boots) != 1:
            _fail(errors, "UNEXPECTED_BOOT_CHANGE")
        for phase in ("R1", "R2", "R4"):
            hbs = [x for x in timeline[phase] if x["kind"] == "heartbeat"]
            if phase in ("R1", "R4") and len(hbs) < 10:
                _fail(errors, "HEARTBEATS_INSUFFICIENT_" + phase)
            for a, b in zip(hbs, hbs[1:]):
                if b["t"] - a["t"] > HB_GAP_MAX_S:
                    _fail(errors, "HEARTBEAT_GAP_" + phase)
                if b["hb"] <= a["hb"]:
                    _fail(errors, "HEARTBEAT_NOT_INCREASING_" + phase)
                for counter in ("sd_ok", "fram", "rtu_ok"):
                    if b[counter] < a[counter]:
                        _fail(errors, "COUNTER_REGRESSION_" + counter.upper() + "_" + phase)
            if phase in ("R1", "R4") and hbs:
                if (hbs[-1]["sd_ok"] <= hbs[0]["sd_ok"] or
                    hbs[-1]["fram"] <= hbs[0]["fram"] or
                    hbs[-1]["rtu_ok"] <= hbs[0]["rtu_ok"]):
                    _fail(errors, "NO_PERIPHERAL_PROGRESS_" + phase)

    for phase, minimum in (("R1", R1_MIN_S), ("R4", R4_MIN_S)):
        arr = timeline[phase]
        if not arr:
            _fail(errors, "PHASE_NOT_PRESENT_" + phase)
            continue
        dur = max(x["t"] for x in arr) - min(x["t"] for x in arr)
        measures[phase + "_duration_s"] = round(dur, 3)
        if dur < minimum:
            _fail(errors, "PHASE_TOO_SHORT_" + phase)
        reqs = [x for x in arr if x["kind"] == "traffic"]
        if len(reqs) < int(minimum // 20):
            _fail(errors, "TRAFFIC_SAMPLES_INSUFFICIENT_" + phase)

    r2 = timeline["R2"]
    downs = [(index, e) for index, e in enumerate(r2) if e["kind"] == "link_down"]
    ups = [(index, e) for index, e in enumerate(r2) if e["kind"] == "link_up"]
    if len(downs) != 3 or len(ups) != 3:
        _fail(errors, "R2_REQUIRES_THREE_LINK_CYCLES")
    else:
        prev_end = -1
        recoveries = []
        for (a_index, down), (b_index, up) in zip(downs, ups):
            if not (prev_end < a_index < b_index):
                _fail(errors, "R2_LINK_EVENT_ORDER")
                continue
            prev_end = b_index
            dt = up["t"] - down["t"]
            if not 10 <= dt <= 15:
                _fail(errors, "R2_FAULT_WINDOW_DURATION")
            post = r2[b_index + 1: (downs[downs.index((a_index, down)) + 1][0]
                                    if downs.index((a_index, down)) + 1 < len(downs)
                                    else len(r2))]
            healthy = [e for e in post if e["kind"] == "heartbeat" and
                       e["eth"] == 1 and e["tcp"] == 1 and e["ip"] != "0.0.0.0"]
            if not healthy:
                _fail(errors, "R2_RECOVERY_NO_READY_HEARTBEAT")
                continue
            ready = healthy[0]["t"]
            recoveries.append(round(ready - up["t"], 3))
            if ready < up["t"] or ready - up["t"] > RECOVERY_DEADLINE_S:
                _fail(errors, "R2_RECOVERY_DEADLINE_EXCEEDED")
            # Exigir cinco pares correctos CONSECUTIVOS tras readyness,
            # dentro de la ventana de 30s, sin filtrar fallos intermedios.
            window = [e for e in post if e["kind"] == "traffic" and
                      ready <= e["t"] <= ready + RECOVERY_SUCCESS_WINDOW_S]
            run = 0
            for trial in window:
                run = run + 1 if trial["tcp"] and trial["udp"] and trial["rtu"] else 0
                if run >= 5:
                    break
            if run < 5:
                _fail(errors, "R2_POST_RECOVERY_FIVE_CONSECUTIVE_MISSING")
            if not any(e["kind"] == "traffic" and e["rtu"] for e in r2[a_index:b_index]):
                _fail(errors, "R2_RTU_DID_NOT_CONTINUE_WHILE_LINK_DOWN")
            if not any(e["kind"] == "traffic" and e["rtu"] for e in post):
                _fail(errors, "R2_RTU_DID_NOT_CONTINUE_AFTER_LINK_UP")
        measures["R2_recovery_seconds"] = recoveries

    measures["event_count"] = len(events)
    measures["heartbeat_count"] = len(all_hb)
    measures["traffic_count"] = sum(x["kind"] == "traffic" for x in events)
    # Nunca convertir un PASS de replay en certificado físico por este script.
    return {"status": "PASS_REPLAY_CONTRACT" if not errors else "REVIEW_STOP",
            "errors": errors, "metrics": measures,
            "physical_status": "NOT_ATTESTED_FROM_REPLAY"}


def _synthetic() -> dict:
    """Corpus inventado exclusivamente para validación de contrato del parser."""
    ev = []
    boot = "a1b2c3d4"
    hbcount = 0
    sd = fram = rtu = 0
    def hb(phase, t, eth=1, tcp=1):
        nonlocal hbcount, sd, fram, rtu
        hbcount += 1
        sd += 1
        fram += 1
        rtu += 1
        return {"phase": phase, "t": t, "kind": "heartbeat", "boot": boot,
                "hb": hbcount, "eth": eth, "tcp": tcp,
                "ip": "192.0.2.42" if eth else "0.0.0.0",
                "sd_ok": sd, "sd_fail": 0, "fram": fram,
                "rtu_ok": rtu, "rtu_fail": 0}
    def traffic(phase, t, tcp=True, udp=True, rtu=True):
        return {"phase": phase, "t": t, "kind": "traffic",
                "tcp": tcp, "udp": udp, "rtu": rtu}
    for t in range(0, 602, 2):
        ev.append(hb("R1", t))
        if t % 10 == 0:
            ev.append(traffic("R1", t + 0.1))
    start = 610
    for cycle in range(3):
        base = start + cycle * 65
        ev.append(hb("R2", base))
        ev.append({"phase": "R2", "t": base+1, "kind": "link_down"})
        for step in (2, 6, 10):
            ev.append(traffic("R2", base+step, tcp=False, udp=False, rtu=True))
            ev.append(hb("R2", base+step+0.1, eth=0, tcp=0))
        ev.append({"phase": "R2", "t": base+13, "kind": "link_up"})
        ev.append(hb("R2", base+14))
        for step in range(5):
            ev.append(traffic("R2", base+15+step*2))
            ev.append(hb("R2", base+15+step*2+0.1))
        for step in range(25, 65, 2):
            ev.append(hb("R2", base+step))
    base = 820
    for t in range(base, base+1802, 2):
        ev.append(hb("R4", t))
        if (t-base) % 10 == 0:
            ev.append(traffic("R4", t+0.1))
    ev.sort(key=lambda e:e["t"])
    return {"schema": "G10-R-REPLAY-V1", "origin": "SYNTHETIC_CI", "events": ev}


def selftest() -> int:
    base = _synthetic()
    result = evaluate(base)
    if result["status"] != "PASS_REPLAY_CONTRACT":
        raise AssertionError("BASELINE_SYNTHETIC_INVALID:" + str(result["errors"]))
    mutations = [
        ("short_r1", lambda m: m["events"].__setitem__(slice(None),
            [e for e in m["events"] if e["phase"] != "R1" or e["t"] <= 580])),
        ("tcp_failure", lambda m: next(e for e in m["events"] if e["phase"]=="R1" and e["kind"]=="traffic").__setitem__("tcp",False)),
        ("udp_failure", lambda m: next(e for e in m["events"] if e["phase"]=="R4" and e["kind"]=="traffic").__setitem__("udp",False)),
        ("rtu_failure", lambda m: next(e for e in m["events"] if e["phase"]=="R4" and e["kind"]=="traffic").__setitem__("rtu",False)),
        ("boot_change", lambda m: next(e for e in m["events"] if e["phase"]=="R4" and e["kind"]=="heartbeat").__setitem__("boot","deadbeef")),
        ("no_link_cycles", lambda m: m["events"].__setitem__(slice(None),[e for e in m["events"] if e["kind"]!="link_down"])),
        ("stuck_recovery", lambda m: [
            e.update({"eth":0, "tcp":0, "ip":"0.0.0.0"})
            for e in m["events"] if e["phase"]=="R2" and
               e["kind"]=="heartbeat" and 624 <= e["t"] < 675]),
        ("fault_long", lambda m: next(e for e in m["events"] if e["kind"]=="link_up").__setitem__("t",629.0)),
        ("counter_regression", lambda m: next(e for e in reversed(m["events"]) if e["phase"]=="R4" and e["kind"]=="heartbeat").__setitem__("sd_ok",0)),
        ("missing_heartbeat", lambda m: m["events"].__setitem__(slice(None),[e for e in m["events"] if e["kind"]!="heartbeat"])),
        ("bad_origin", lambda m: m.__setitem__("origin","REAL_CERTIFIED")),
        ("invalid_bool", lambda m: next(e for e in m["events"] if e["kind"]=="traffic").__setitem__("tcp",1)),
    ]
    for label, mutate in mutations:
        trial = copy.deepcopy(base)
        mutate(trial)
        outcome = evaluate(trial)
        if outcome["status"] != "REVIEW_STOP":
            raise AssertionError("NEGATIVE_NOT_REJECTED:" + label)
    print("G10_R_REPLAY_SELFTEST=PASS_SYNTHETIC_ONLY")
    print("G10_R_REPLAY_NEGATIVES=%d_PASS_REJECTED" % len(mutations))
    print("G10_R_R1_R2_R4_PHYSICAL=NOT_RUN")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    opts = parser.add_mutually_exclusive_group(required=True)
    opts.add_argument("--selftest", action="store_true")
    opts.add_argument("--evaluate", type=Path, help="JSON de eventos en disco (sin COM/sockets)")
    args = parser.parse_args()
    if args.selftest:
        return selftest()
    try:
        data = json.loads(args.evaluate.read_text(encoding="utf-8-sig"))
        result = evaluate(data)
        print(json.dumps(result,indent=2,ensure_ascii=False,sort_keys=True))
        return 0 if result["status"] == "PASS_REPLAY_CONTRACT" else 2
    except (OSError,ValueError,UnicodeError) as exc:
        print("G10_R_REPLAY=REVIEW_STOP_UNREADABLE_" + type(exc).__name__)
        return 2


if __name__ == "__main__":
    sys.exit(main())
