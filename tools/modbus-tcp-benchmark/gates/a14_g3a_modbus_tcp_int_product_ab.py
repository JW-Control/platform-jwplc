#!/usr/bin/env python3
from __future__ import annotations
import argparse, ast, statistics, sys, tempfile, time
from pathlib import Path
import a14_h4a04p1_tcp_rx_bottleneck_profile as common

BRANCH="v2.1.0-alpha.14/feature/modbus-tcp"
ORDER=("POLLING","INT_GUIDED","INT_GUIDED","POLLING")
DURATION=30.0

def one(text,key):
    p=key+"="
    v=[x[len(p):].strip() for x in text.splitlines() if x.startswith(p)]
    if len(v)!=1: raise RuntimeError(f"G3A_KEY_COUNT {key}={len(v)}")
    return v[0]

def num(text,key): return float(one(text,key))
def med(v): return float(statistics.median(v))
def spread(v):
    m=med(v)
    return (max(v)-min(v))*100.0/m if m>0 else 0.0
def delta(v,r): return (v/r-1.0)*100.0 if r>0 else 0.0
def reduction(v,r): return (1.0-v/r)*100.0 if r>0 else 0.0

def compile_variant(cli,fqbn,repo,sketch,libs,build,variant,log):
    iv="1" if variant=="INT_GUIDED" else "0"
    flags="-DJWPLC_ETHERNET_ENABLE_PROFILE_HOOKS=1 "+f"-DJWPLC_MODBUS_TCP_INT_GUIDED_RX={iv}"
    cmd=[str(cli),"compile","--verbose","--fqbn",fqbn,"--build-path",str(build),
         "--libraries",str(libs),"--build-property",f"compiler.cpp.extra_flags={flags}",str(sketch)]
    rc=common.run_logged(cmd,log,repo)
    common.emit(f"G3A_{variant}_COMPILE_EXIT",rc)
    if rc!=0:
        print(common.decode(log.read_bytes())[-8000:])
        raise RuntimeError(f"G3A_{variant}_COMPILE_FAILED")
    t=common.decode(log.read_bytes()).replace("\\","/").lower()
    checks={
        "PROFILE_HOOKS":"-djwplc_ethernet_enable_profile_hooks=1" in t,
        "INT_FLAG":f"-djwplc_modbus_tcp_int_guided_rx={iv}" in t,
        "MODBUS_SOURCE":"jwplc_modbustcp.cpp" in t,
        "SOCKET_SOURCE":"socket.cpp" in t,
    }
    for k,ok in checks.items(): common.emit(f"G3A_{variant}_COMPILE_{k}","PASS" if ok else "FAIL")
    bad=[k for k,ok in checks.items() if not ok]
    if bad: raise RuntimeError("G3A_COMPILE_CONTRACT_FAIL="+",".join(bad))

def main():
    ap=argparse.ArgumentParser()
    ap.add_argument("--serial",default="COM14")
    ap.add_argument("--arduino-cli")
    ap.add_argument("--fqbn",default="jwplc_local:esp32:jwplcbasic")
    a=ap.parse_args()
    repo=Path(__file__).resolve().parents[3]
    sketch=repo/"tools/modbus-tcp-benchmark/firmware/a14_g3a_modbus_tcp_int_product"
    runner=repo/"tools/modbus-tcp-benchmark/pc/a14_g3a_modbus_tcp_int_case.py"
    libs=repo/"JWPLC/2.1.0/libraries"
    print("="*78); print(" ALPHA14 G3A - REAL MODBUS TCP INT PRODUCT A/B"); print("="*78)
    branch=common.git(repo,"branch","--show-current"); head=common.git(repo,"rev-parse","HEAD")
    common.emit("BRANCH",branch); common.emit("HEAD",head)
    common.emit("G3A_ORDER",",".join(ORDER)); common.emit("G3A_DURATION_S",DURATION)
    common.emit("G3A_TARGET","FC03_125_1000_REQ_S")
    common.emit("G3A_INT_DEFAULT_IN_PACKAGE","OFF"); common.emit("G3A_PRODUCT_DEFAULT_CHANGED","NO")
    if branch!=BRANCH: raise RuntimeError("G3A_BRANCH_MISMATCH")
    if common.git(repo,"diff","--name-only") or common.git(repo,"diff","--cached","--name-only"):
        raise RuntimeError("G3A_TRACKED_TREE_NOT_CLEAN")
    ast.parse(runner.read_text(encoding="utf-8"),filename=str(runner))
    common.emit("G3A_RUNNER_AST","PASS")
    cli=common.find_cli(a.arduino_cli); common.emit("ARDUINO_CLI",cli)
    root=Path(tempfile.mkdtemp(prefix="jwplc_a14_g3a_modbus_int_ab_"))
    common.emit("G3A_RESULT_ROOT",root)
    builds={"POLLING":root/"build_polling","INT_GUIDED":root/"build_int"}
    for v in builds:
        compile_variant(cli,a.fqbn,repo,sketch,libs,builds[v],v,root/f"compile_{v.lower()}.log")

    rows={"POLLING":[],"INT_GUIDED":[]}; counts={"POLLING":0,"INT_GUIDED":0}
    for idx,v in enumerate(ORDER,1):
        counts[v]+=1; rn=counts[v]
        print(); print("="*78); print(f" G3A CASE {idx}/4 {v} RUN {rn}/2"); print("="*78)
        ulog=root/f"{idx:02d}_{v.lower()}_upload.log"
        rc=common.run_logged([str(cli),"upload","--fqbn",a.fqbn,"--port",a.serial,
                              "--input-dir",str(builds[v]),str(sketch)],ulog,repo)
        common.emit(f"G3A_{v}_RUN{rn}_UPLOAD_EXIT",rc)
        if rc!=0:
            print(common.decode(ulog.read_bytes())[-5000:]); raise RuntimeError("G3A_UPLOAD_FAILED")
        time.sleep(3)
        proc=common.run([sys.executable,"-B",str(runner),"--serial",a.serial,
                         "--duration",str(DURATION),"--variant",v],repo)
        clog=root/f"{idx:02d}_{v.lower()}_run{rn}.log"; clog.write_bytes(proc.stdout)
        text=common.decode(proc.stdout); print(text)
        if proc.returncode!=0 or one(text,"G3A_FUNCTIONAL_PASS")!="YES":
            raise RuntimeError(f"G3A_{v}_RUN{rn}_FAILED")
        rows[v].append({k:num(text,key) for k,key in {
            "req":"G3A_ACHIEVED_REQ_S","p95":"G3A_P95_US","p99":"G3A_P99_US",
            "max":"G3A_MAX_US","loop":"G3A_LOOP_MAX_US","status":"G3A_STATUS_CALLS",
            "avail":"G3A_AVAILABLE_CALLS","zero":"G3A_AVAILABLE_ZERO"}.items()})

    summary=[]
    def out(k,v): line=f"{k}={v}"; summary.append(line); print(line)
    print(); print("="*78); print(" G3A SUMMARY"); print("="*78)
    s={}
    for v in ("POLLING","INT_GUIDED"):
        b=rows[v]; s[v]={k:med([r[k] for r in b]) for k in b[0]}
        s[v]["spread"]=spread([r["req"] for r in b])
        p=f"G3A_{v}"
        out(p+"_REQ_S_MEDIAN",f"{s[v]['req']:.3f}"); out(p+"_REQ_S_SPREAD_PCT",f"{s[v]['spread']:.3f}")
        out(p+"_P95_US_MEDIAN",f"{s[v]['p95']:.3f}"); out(p+"_P99_US_MEDIAN",f"{s[v]['p99']:.3f}")
        out(p+"_MAX_US_MEDIAN",f"{s[v]['max']:.3f}"); out(p+"_LOOP_MAX_US_MEDIAN",f"{s[v]['loop']:.0f}")
        out(p+"_STATUS_CALLS_MEDIAN",f"{s[v]['status']:.0f}"); out(p+"_AVAILABLE_CALLS_MEDIAN",f"{s[v]['avail']:.0f}")
        out(p+"_AVAILABLE_ZERO_MEDIAN",f"{s[v]['zero']:.0f}")

    p=s["POLLING"]; i=s["INT_GUIDED"]
    sr=reduction(i["status"],p["status"]); ar=reduction(i["avail"],p["avail"])
    rd=delta(i["req"],p["req"]); d95=delta(i["p95"],p["p95"]); d99=delta(i["p99"],p["p99"])
    out("G3A_INT_STATUS_CALL_REDUCTION_PCT",f"{sr:.3f}"); out("G3A_INT_AVAILABLE_CALL_REDUCTION_PCT",f"{ar:.3f}")
    out("G3A_INT_REQ_S_DELTA_PCT",f"{rd:.3f}"); out("G3A_INT_P95_DELTA_PCT",f"{d95:.3f}"); out("G3A_INT_P99_DELTA_PCT",f"{d99:.3f}")
    lim95=max(p["p95"]*1.20,p["p95"]+200); lim99=max(p["p99"]*1.30,p["p99"]+500)
    mech=sr>=50 and ar>=40; rate=p["req"]>=995 and i["req"]>=995
    g95=i["p95"]<=lim95; g99=i["p99"]<=lim99
    out("G3A_INT_MECHANISM_CONFIRMED","YES" if mech else "NO")
    out("G3A_RATE_GUARD","PASS" if rate else "FAIL"); out("G3A_P95_GUARD","PASS" if g95 else "FAIL"); out("G3A_P99_GUARD","PASS" if g99 else "FAIL")
    ready=mech and rate and g95 and g99
    out("G3A_PRODUCT_CANDIDATE_READY_FOR_H3ER","YES" if ready else "NO")
    out("G3A_PRODUCT_DEFAULT_CHANGED","NO"); out("G3A_NEXT","G3B_H3ER_INT_FULL_RUNTIME" if ready else "REVIEW_G3A")
    out("G3A_STATUS","PASS" if ready else "REVIEW")
    sp=root/"SUMMARY.log"; sp.write_text("\n".join(summary)+"\n",encoding="utf-8"); common.emit("G3A_SUMMARY_LOG",sp)
    return 0 if ready else 2

if __name__=="__main__": raise SystemExit(main())
