#!/usr/bin/env python3
"""PRE-H B2: evaluator de observabilidad DHCP y equidad periférica.

Solo telemetría. Sin placa no demuestra renovación/rebind ni TFT refrescado.
Cualquier inconsistencia es REVIEW_STOP; ausencia de renew/rebind se registra
NOT_OBSERVED, no se inventa un PASS físico.
"""
from __future__ import annotations
import argparse
import copy
import json

DHCP=("dhcp_ticks","dhcp_pend","dhcp_renew_ok","dhcp_renew_fail",
      "dhcp_rebind_ok","dhcp_rebind_fail")
COUNTERS=("sd_ok","sd_fail","fram","rtu_ok","rtu_fail","udp_rx","udp_tx")
TFT=("tft_draw","tft_fail","tft_max_us","tft_spi_wait_max_us")
PHASES=("R1","R2","R3","R4")
def evaluate(data):
    issues=[]
    events=data.get("events") if isinstance(data,dict) else None
    if not isinstance(events,list):
        return {"status":"REVIEW_STOP","issues":["NO_EVENT_LIST"]}
    hbs=[e for e in events if isinstance(e,dict) and e.get("kind")=="heartbeat"]
    if not hbs:
        return {"status":"REVIEW_STOP","issues":["NO_HEARTBEATS"]}
    boots={e.get("boot") for e in hbs}
    if len(boots)!=1:issues.append("BOOT_CHANGED")
    for phase in {e.get("phase") for e in hbs}:
        group=sorted((e for e in hbs if e.get("phase")==phase),
                     key=lambda e:e.get("t",-1))
        if len(group)<3:issues.append("INSUFFICIENT_"+str(phase))
        previous=None
        for e in group:
            required=DHCP+COUNTERS+TFT+("io_ready","io_age_ms","rtc_age_ms",
                    "tft_ready","dhcp_state","dhcp_err","rtc","ip","link","eth","t")
            if any(k not in e for k in required):
                issues.append("MISSING_TELEMETRY");continue
            nums=DHCP+COUNTERS+TFT+("io_ready","io_age_ms","rtc_age_ms",
                              "tft_ready","dhcp_state","dhcp_err","rtc","link","eth")
            if any(type(e[k]) is not int or e[k]<0 for k in nums):
                issues.append("INVALID_COUNTER");continue
            if e["io_ready"]!=1 or e["rtc"]!=1 or e["tft_ready"]!=1:
                issues.append("PERIPHERAL_NOT_READY")
            if e["io_age_ms"]>3000 or e["rtc_age_ms"]>5000:
                issues.append("IO_OR_RTC_SCAN_STALLED")
            if not 0<=e["dhcp_state"]<=6:
                issues.append("INVALID_DHCP_STATE")
            if previous is not None:
                if e["t"]<=previous["t"] or e["t"]-previous["t"]>6:
                    issues.append("HEARTBEAT_GAP")
                if any(e[k]<previous[k] for k in COUNTERS):
                    issues.append("PHASE_COUNTER_REGRESSION")
            previous=e
        if len(group)>2 and all(
            isinstance(e.get("fram"),int) and isinstance(e.get("sd_ok"),int)
            for e in group):
            if group[-1]["fram"]<=group[0]["fram"] or group[-1]["sd_ok"]<=group[0]["sd_ok"]:
                issues.append("SD_OR_FRAM_NO_PROGRESS")
        if len(group)>2 and all(type(e.get("tft_draw")) is int for e in group):
            if group[-1]["tft_draw"]<=group[0]["tft_draw"]:
                issues.append("TFT_REFRESH_NO_PROGRESS")
    ordered=sorted(hbs,key=lambda e:e.get("t",-1))
    for a,b in zip(ordered,ordered[1:]):
        if all(k in a and k in b for k in DHCP+TFT):
            if any(b[k]<a[k] for k in DHCP):issues.append("GLOBAL_DHCP_COUNTER_REGRESSION")
            if any(b[k]<a[k] for k in TFT):issues.append("GLOBAL_TFT_COUNTER_REGRESSION")
    r2=[e for e in hbs if e.get("phase")=="R2"]
    if r2:
        if not any(e.get("link")==2 for e in r2):
            issues.append("R2_LINK_DOWN_NOT_OBSERVED")
        if not any(e.get("link")==1 and e.get("eth")==1 and
                   e.get("dhcp_state")==5 and e.get("ip")!="0.0.0.0" for e in r2):
            issues.append("R2_RECOVERY_NOT_OBSERVED")
    last=ordered[-1]
    renewal={
      "renew_success":last.get("dhcp_renew_ok",0)>ordered[0].get("dhcp_renew_ok",0),
      "rebind_success":last.get("dhcp_rebind_ok",0)>ordered[0].get("dhcp_rebind_ok",0),
      "pending_seen":last.get("dhcp_pend",0)>ordered[0].get("dhcp_pend",0)
    }
    return {
      "status":"REVIEW_STOP" if issues else "PASS_OFFLINE_TELEMETRY_CONTRACT_ONLY",
      "issues":sorted(set(issues)),
      "dhcp_lease_maintenance":renewal,
      "dhcp_renew_rebind_physical":"NOT_PROVEN_BY_OFFLINE_EVALUATOR",
      "tft_refresh_progress":"OBSERVED_BY_H3E1_RELEASE_BUS_COUNTS",
      "tca_io_progress":"OBSERVED_BY_RUNTIME_SCAN_AGE_NOT_BUS_TRANSACTION",
      "physical_campaign":"NOT_ATTESTED"
    }

def fixture():
    events=[]
    for phase_index,phase in enumerate(("R1","R2","R4")):
        for idx in range(4):
            e={"kind":"heartbeat","phase":phase,"t":phase_index*20+idx+1,
               "boot":"abcdef12","ip":"192.168.1.50","link":1,"eth":1,"tcp":1,
               "dhcp_state":5,"dhcp_err":0,"io_ready":1,"io_age_ms":60,
               "rtc_age_ms":300,"tft_ready":1,"rtc":1,
               "sd_ok":idx+1,"sd_fail":0,"fram":idx+1,
               "rtu_ok":idx+1,"rtu_fail":0,"udp_rx":idx+1,"udp_tx":idx+1,
               "dhcp_ticks":phase_index*10+idx+1,"dhcp_pend":0,
               "tft_draw":phase_index*20+idx+1,"tft_fail":0,
               "tft_max_us":1500,"tft_spi_wait_max_us":100,
               "dhcp_renew_ok":0,"dhcp_renew_fail":0,
               "dhcp_rebind_ok":0,"dhcp_rebind_fail":0}
            if phase=="R2" and idx==1:e.update(link=2,eth=0,dhcp_state=3,ip="0.0.0.0")
            events.append(e)
    return {"events":events}

def selftest():
    p=fixture()
    assert evaluate(p)["status"]=="PASS_OFFLINE_TELEMETRY_CONTRACT_ONLY"
    cases=[]
    def mutated(name,fn):
        q=copy.deepcopy(p);fn(q);cases.append((name,q))
    mutated("no_heartbeats",lambda q:q["events"].clear())
    mutated("bad_boot",lambda q:q["events"][-1].update(boot="deaddead"))
    mutated("stale_io",lambda q:q["events"][2].update(io_age_ms=99999))
    mutated("rtc_stalled",lambda q:q["events"][2].update(rtc_age_ms=99999))
    mutated("tft_not_ready",lambda q:q["events"][2].update(tft_ready=0))
    mutated("tft_stall",lambda q:[e.update(tft_draw=1) for e in q["events"][:4]])
    mutated("tft_regression",lambda q:q["events"][-1].update(tft_draw=0))
    mutated("missing_dhcp",lambda q:q["events"][2].pop("dhcp_state"))
    mutated("dhcp_regression",lambda q:q["events"][-1].update(dhcp_ticks=0))
    mutated("fram_stalled",lambda q:[e.update(fram=0) for e in q["events"][:4]])
    mutated("missing_recovery",lambda q:[e.update(link=2,eth=0) for e in q["events"] if e["phase"]=="R2"])
    mutated("no_link_down",lambda q:[e.update(link=1) for e in q["events"]])
    mutated("heartbeat_gap",lambda q:q["events"][2].update(t=17))
    for name,q in cases:
        assert evaluate(q)["status"]=="REVIEW_STOP",name
    print("G10R_B2_DHCP_PERIPHERAL_CONTRACT=PASS_SYNTHETIC")
    print(f"G10R_B2_NEGATIVE_CASES={len(cases)}_REJECTED")
    print("G10R_B2_DHCP_RENEW_REBIND=NOT_PHYSICALLY_PROVEN")
    print("G10R_B2_TFT_REFRESH=H3E1_CONTRACT_ONLY_NO_HARDWARE")
if __name__=="__main__":
    p=argparse.ArgumentParser()
    g=p.add_mutually_exclusive_group(required=True)
    g.add_argument("--selftest",action="store_true")
    g.add_argument("--manifest")
    args=p.parse_args()
    if args.selftest:selftest()
    else:
        with open(args.manifest,encoding="utf-8") as f:payload=json.load(f)
        r=evaluate(payload);print(json.dumps(r,indent=2))
        raise SystemExit(0 if r["status"]=="PASS_OFFLINE_TELEMETRY_CONTRACT_ONLY" else 2)
