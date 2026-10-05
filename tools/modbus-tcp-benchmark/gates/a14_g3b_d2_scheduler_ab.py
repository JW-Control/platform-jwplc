#!/usr/bin/env python3
from __future__ import annotations
import argparse, ast, statistics, sys, tempfile, time
from pathlib import Path
import a14_h4a04p1_tcp_rx_bottleneck_profile as common

BRANCH="v2.1.0-alpha.14/feature/modbus-tcp"
HOT_US=1500
ORDER=("POLLING","ADAPTIVE","ADAPTIVE","POLLING")

def one(t,k):
    p=k+"="; v=[x[len(p):].strip() for x in t.splitlines() if x.startswith(p)]
    if len(v)!=1: raise RuntimeError(f"D2_KEY_COUNT {k}={len(v)}")
    return v[0]
def num(t,k): return float(one(t,k))
def med(v): return float(statistics.median(v))
def delta(v,r): return (v/r-1)*100 if r>0 else 0.0
def reduction(v,r): return (1-v/r)*100 if r>0 else 0.0

def compile_variant(cli,fqbn,repo,sketch,libs,build,variant,log):
    iv="1" if variant=="ADAPTIVE" else "0"
    hot=str(HOT_US if variant=="ADAPTIVE" else 0)
    flags=("-DJWPLC_ETHERNET_ENABLE_PROFILE_HOOKS=1 "
           f"-DJWPLC_MODBUS_TCP_INT_GUIDED_RX={iv} "
           f"-DJWPLC_MODBUS_TCP_INT_HOT_POLL_US={hot}")
    cmd=[str(cli),"compile","--verbose","--fqbn",fqbn,"--build-path",str(build),
         "--libraries",str(libs),"--build-property",f"compiler.cpp.extra_flags={flags}",str(sketch)]
    rc=common.run_logged(cmd,log,repo)
    common.emit(f"D2_{variant}_COMPILE_EXIT",rc)
    if rc!=0:
        print(common.decode(log.read_bytes())[-8000:]); raise RuntimeError("D2_COMPILE_FAILED")
    s=common.decode(log.read_bytes()).replace("\\","/").lower()
    checks={
      "PROFILE":"-djwplc_ethernet_enable_profile_hooks=1" in s,
      "INT":f"-djwplc_modbus_tcp_int_guided_rx={iv}" in s,
      "HOT":f"-djwplc_modbus_tcp_int_hot_poll_us={hot}" in s,
      "MODBUS":"jwplc_modbustcp.cpp" in s,
      "SOCKET":"socket.cpp" in s,
    }
    for k,ok in checks.items(): common.emit(f"D2_{variant}_COMPILE_{k}","PASS" if ok else "FAIL")
    if not all(checks.values()): raise RuntimeError("D2_COMPILE_CONTRACT_FAIL")

def upload(cli,fqbn,repo,sketch,build,port,log):
    rc=common.run_logged([str(cli),"upload","--fqbn",fqbn,"--port",port,
                          "--input-dir",str(build),str(sketch)],log,repo)
    if rc!=0:
        print(common.decode(log.read_bytes())[-5000:]); raise RuntimeError("D2_UPLOAD_FAILED")

def main():
    p=argparse.ArgumentParser()
    p.add_argument("--serial",default="COM14")
    p.add_argument("--arduino-cli")
    p.add_argument("--fqbn",default="jwplc_local:esp32:jwplcbasic")
    a=p.parse_args()
    repo=Path(__file__).resolve().parents[3]
    sketch=repo/"tools/modbus-tcp-benchmark/firmware/a14_g3a_modbus_tcp_int_product"
    high=repo/"tools/modbus-tcp-benchmark/pc/a14_g3a_modbus_tcp_int_case.py"
    idle=repo/"tools/modbus-tcp-benchmark/pc/a14_g3b_d2_idle_case.py"
    libs=repo/"JWPLC/2.1.0/libraries"
    print("="*78); print(" ALPHA14 G3B-D2 - ADAPTIVE INT HOT-POLL"); print("="*78)
    branch=common.git(repo,"branch","--show-current"); head=common.git(repo,"rev-parse","HEAD")
    common.emit("BRANCH",branch); common.emit("HEAD",head)
    common.emit("D2_HOT_POLL_US",HOT_US); common.emit("D2_ORDER",",".join(ORDER))
    common.emit("D2_PRODUCT_DEFAULT_CHANGED","NO")
    if branch!=BRANCH: raise RuntimeError("D2_BRANCH_MISMATCH")
    if common.git(repo,"diff","--name-only") or common.git(repo,"diff","--cached","--name-only"):
        raise RuntimeError("D2_TREE_DIRTY")
    ast.parse(high.read_text(encoding="utf-8")); ast.parse(idle.read_text(encoding="utf-8"))
    cli=common.find_cli(a.arduino_cli)
    root=Path(tempfile.mkdtemp(prefix="jwplc_a14_g3b_d2_"))
    common.emit("D2_RESULT_ROOT",root)
    builds={"POLLING":root/"build_polling","ADAPTIVE":root/"build_adaptive"}
    for v in builds:
        compile_variant(cli,a.fqbn,repo,sketch,libs,builds[v],v,root/f"compile_{v.lower()}.log")

    rows={"POLLING":[],"ADAPTIVE":[]}; counts={"POLLING":0,"ADAPTIVE":0}
    for idx,v in enumerate(ORDER,1):
        counts[v]+=1; rn=counts[v]
        print(); print("="*78); print(f" D2 HIGH CASE {idx}/4 {v} RUN {rn}/2"); print("="*78)
        upload(cli,a.fqbn,repo,sketch,builds[v],a.serial,root/f"high_{idx}_upload.log")
        time.sleep(3)
        runner_variant="INT_GUIDED" if v=="ADAPTIVE" else "POLLING"
        proc=common.run([sys.executable,"-B",str(high),"--serial",a.serial,
                         "--duration","30","--variant",runner_variant],repo)
        text=common.decode(proc.stdout); print(text)
        (root/f"high_{idx}_{v.lower()}.log").write_bytes(proc.stdout)
        if proc.returncode!=0 or one(text,"G3A_FUNCTIONAL_PASS")!="YES":
            raise RuntimeError(f"D2_HIGH_{v}_FAIL")
        rows[v].append({
          "req":num(text,"G3A_ACHIEVED_REQ_S"),"p95":num(text,"G3A_P95_US"),
          "p99":num(text,"G3A_P99_US"),"avg":num(text,"G3A_LAT_AVG_US"),
          "status":num(text,"G3A_STATUS_CALLS"),"avail":num(text,"G3A_AVAILABLE_CALLS")})

    idle_rows={}
    for idx,v in enumerate(("POLLING","ADAPTIVE"),1):
        print(); print("="*78); print(f" D2 IDLE CASE {idx}/2 {v}"); print("="*78)
        upload(cli,a.fqbn,repo,sketch,builds[v],a.serial,root/f"idle_{idx}_upload.log")
        time.sleep(3)
        hot=HOT_US if v=="ADAPTIVE" else 0
        proc=common.run([sys.executable,"-B",str(idle),"--serial",a.serial,
                         "--duration","15","--variant",v,
                         "--expected-hot-poll-us",str(hot)],repo)
        text=common.decode(proc.stdout); print(text)
        (root/f"idle_{idx}_{v.lower()}.log").write_bytes(proc.stdout)
        if proc.returncode!=0 or one(text,"D2_IDLE_FUNCTIONAL_PASS")!="YES":
            raise RuntimeError(f"D2_IDLE_{v}_FAIL")
        idle_rows[v]={"status":num(text,"D2_IDLE_STATUS_CALLS"),
                      "avail":num(text,"D2_IDLE_AVAILABLE_CALLS"),
                      "loop":num(text,"D2_IDLE_LOOP_AVG_US")}

    out=[]
    def emit(k,v): line=f"{k}={v}"; out.append(line); print(line)
    print(); print("="*78); print(" G3B-D2 SUMMARY"); print("="*78)
    stats={}
    for v in ("POLLING","ADAPTIVE"):
        b=rows[v]; stats[v]={k:med([r[k] for r in b]) for k in b[0]}
        emit(f"D2_{v}_REQ_S_MEDIAN",f"{stats[v]['req']:.3f}")
        emit(f"D2_{v}_AVG_US_MEDIAN",f"{stats[v]['avg']:.3f}")
        emit(f"D2_{v}_P95_US_MEDIAN",f"{stats[v]['p95']:.3f}")
        emit(f"D2_{v}_P99_US_MEDIAN",f"{stats[v]['p99']:.3f}")
        emit(f"D2_{v}_STATUS_CALLS_MEDIAN",f"{stats[v]['status']:.0f}")
        emit(f"D2_{v}_AVAILABLE_CALLS_MEDIAN",f"{stats[v]['avail']:.0f}")
    poll=stats["POLLING"]; cand=stats["ADAPTIVE"]
    p95d=delta(cand["p95"],poll["p95"]); p99d=delta(cand["p99"],poll["p99"])
    avgr=delta(cand["avg"],poll["avg"])
    idle_sr=reduction(idle_rows["ADAPTIVE"]["status"],idle_rows["POLLING"]["status"])
    idle_ar=reduction(idle_rows["ADAPTIVE"]["avail"],idle_rows["POLLING"]["avail"])
    emit("D2_HIGH_AVG_DELTA_PCT",f"{avgr:.3f}")
    emit("D2_HIGH_P95_DELTA_PCT",f"{p95d:.3f}")
    emit("D2_HIGH_P99_DELTA_PCT",f"{p99d:.3f}")
    emit("D2_IDLE_STATUS_REDUCTION_PCT",f"{idle_sr:.3f}")
    emit("D2_IDLE_AVAILABLE_REDUCTION_PCT",f"{idle_ar:.3f}")
    emit("D2_IDLE_POLLING_LOOP_AVG_US",f"{idle_rows['POLLING']['loop']:.0f}")
    emit("D2_IDLE_ADAPTIVE_LOOP_AVG_US",f"{idle_rows['ADAPTIVE']['loop']:.0f}")
    high_ok=(poll["req"]>=999.0 and cand["req"]>=999.0 and p95d<=5.0 and p99d<=8.0)
    idle_ok=(idle_sr>=80.0 and idle_ar>=80.0)
    emit("D2_HIGH_RATE_LATENCY_RECOVERY","PASS" if high_ok else "FAIL")
    emit("D2_IDLE_BUS_EFFICIENCY","PASS" if idle_ok else "FAIL")
    ready=high_ok and idle_ok
    emit("D2_READY_FOR_H3ER","YES" if ready else "NO")
    emit("D2_PRODUCT_DEFAULT_CHANGED","NO")
    emit("D2_NEXT","G3B_D2_H3ER" if ready else "REVIEW_D2")
    emit("D2_STATUS","PASS" if ready else "REVIEW")
    sp=root/"SUMMARY.log"; sp.write_text("\n".join(out)+"\n",encoding="utf-8")
    common.emit("D2_SUMMARY_LOG",sp)
    return 0 if ready else 2

if __name__=="__main__":
    raise SystemExit(main())
