from __future__ import annotations
import argparse
import sys
import time
from pathlib import Path

THIS_DIR = Path(__file__).resolve().parent
sys.path.insert(0, str(THIS_DIR))

import a14_perf_fc03_qualification_sweep as q
import a14_p5b_master_slave_qualification as p5b
import a14_p5rtu_tcp_budget_frontier as budget

FIFO_CASES = [(120,b"[\n"),(32,b"]\n"),(16,b"{\n"),(8,b"}\n"),(1,b"?\n")]

def iv(v, k): return p5b.int_value(v, k, -1)
def sv(v, k): return v.get(k, "").strip()

def snap(master, slave):
    return q.request_snapshot(master, echo=False), p5b.request_slave_snapshot(slave, 5.0)

def configure_base(master, slave):
    budget.send_command_wait(master,b"X\n",p5b.MASTER_STOP_ACK)
    for port in (slave, master):
        budget.send_command_wait(port,b"9\n","RTU_BAUD_REQUESTED=500000")
        budget.send_command_wait(port,b"4\n","RTU_FRAME_GAP_US=100")
    budget.send_command_wait(master,b"U\n","RTU_RATE_MODE=UNPACED")
    time.sleep(0.1)
    ms, ss = snap(master, slave)
    ok = (
        iv(ms,"RTU_BAUD_EFFECTIVE")==500000 and iv(ss,"RTU_BAUD_EFFECTIVE")==500000
        and sv(ms,"RTU_CLOCK_PROFILE")=="APB_FORCED" and sv(ss,"RTU_CLOCK_PROFILE")=="APB_FORCED"
        and iv(ms,"RTU_FRAME_GAP_US")==100 and iv(ss,"RTU_FRAME_GAP_US")==100
        and sv(ms,"RTU_MOTOR")=="ASYNC" and sv(ss,"RTU_MOTOR")=="ASYNC"
        and sv(ms,"RTU_TX_MODE")=="QUEUED" and sv(ss,"RTU_TX_MODE")=="QUEUED"
    )
    print(f"RTUH3A_BASE_PROFILE_PASS={'YES' if ok else 'NO'}")
    if not ok: raise RuntimeError("RTUH3A_BASE_PROFILE_INVALID")

def configure_fifo(master, slave, fifo, command):
    ack=f"RTU_RX_FIFO_FULL={fifo}"
    budget.send_command_wait(slave,command,ack)
    budget.send_command_wait(master,command,ack)
    time.sleep(0.05)
    ms, ss = snap(master, slave)
    ok=iv(ms,"RTU_RX_FIFO_FULL")==fifo and iv(ss,"RTU_RX_FIFO_FULL")==fifo
    print(f"RTUH3A_FIFO_CONFIG FIFO_BYTES={fifo} MASTER={iv(ms,'RTU_RX_FIFO_FULL')} SLAVE={iv(ss,'RTU_RX_FIFO_FULL')} PASS={'YES' if ok else 'NO'}")
    if not ok: raise RuntimeError(f"RTUH3A_FIFO_CONFIG_FAILED_{fifo}")

def run_case(master, slave, host, port, duration, fifo, tcp_on):
    mode="TCP500" if tcp_on else "OFF"
    print(f"RTUH3A_CASE_BEGIN FIFO_BYTES={fifo} TCP={mode} BAUD=500000 GAP_US=100 CLOCK=APB_FORCED")
    budget.send_command_wait(master,b"X\n",p5b.MASTER_STOP_ACK)
    time.sleep(0.1)
    q.wait_server_disconnected(master,timeout_s=20.0)
    q.reset_stats(master)
    p5b.reset_slave_stats(slave,3.0)
    budget.send_command_wait(master,b"G\n",p5b.MASTER_START_ACK)

    tcp=None
    if tcp_on:
        tcp=budget.run_paced_fc03(host,port,duration,500.0,125)
    else:
        time.sleep(duration)

    budget.send_command_wait(master,b"X\n",p5b.MASTER_STOP_ACK)
    time.sleep(0.1)
    ms, ss=snap(master,slave)

    d=iv(ms,"RTU_TRAFFIC_DURATION_MS")
    started=iv(ms,"RTU_REQUESTS_STARTED"); rejected=iv(ms,"RTU_REQUESTS_REJECTED")
    completed=iv(ms,"RTU_REQUESTS_COMPLETED"); success=iv(ms,"RTU_REQUESTS_SUCCESS")
    failed=iv(ms,"RTU_REQUESTS_FAILED"); verify=iv(ms,"RTU_VERIFY_FAILS")
    timeouts=iv(ms,"RTU_MASTER_TIMEOUTS"); mcrc=iv(ms,"RTU_CRC_ERRORS"); scrc=iv(ss,"RTU_CRC_ERRORS")
    hz=completed/(d/1000.0) if d>0 else 0.0

    profile=(
        iv(ms,"RTU_BAUD_EFFECTIVE")==500000 and iv(ss,"RTU_BAUD_EFFECTIVE")==500000
        and sv(ms,"RTU_CLOCK_PROFILE")=="APB_FORCED" and sv(ss,"RTU_CLOCK_PROFILE")=="APB_FORCED"
        and iv(ms,"RTU_FRAME_GAP_US")==100 and iv(ss,"RTU_FRAME_GAP_US")==100
        and iv(ms,"RTU_RX_FIFO_FULL")==fifo and iv(ss,"RTU_RX_FIFO_FULL")==fifo
        and sv(ms,"RTU_MOTOR")=="ASYNC" and sv(ss,"RTU_MOTOR")=="ASYNC"
        and sv(ms,"RTU_TX_MODE")=="QUEUED" and sv(ss,"RTU_TX_MODE")=="QUEUED"
    )
    rtu_clean=(
        started==completed==success and rejected==0 and failed==0 and verify==0
        and timeouts==0 and mcrc==0 and scrc==0
        and iv(ss,"RTU_RX_FRAMES")==completed and iv(ss,"RTU_TX_FRAMES")==completed
        and iv(ss,"RTU_REQUESTS_OK")==completed
    )
    tcp_req=tcp_avg=tcp_p95=tcp_p99=0.0
    tcp_clean=True
    if tcp_on:
        tcp_req=float(tcp["achieved_req_s"]); tcp_avg=float(tcp["avg_us"])
        tcp_p95=float(tcp["p95_us"]); tcp_p99=float(tcp["p99_us"])
        tcp_clean=(
            float(tcp["target_pct"])>=99.0 and tcp["timeouts"]==0
            and tcp["transport_errors"]==0 and tcp["protocol_errors"]==0
            and iv(ms,"FRAME_TIMEOUTS")==0 and iv(ms,"BUS_LOCK_TIMEOUTS")==0
            and iv(ms,"PROTOCOL_ERRORS")==0 and iv(ms,"REQUESTS_OK")==int(tcp["ok"])
        )
    clean=(profile and rtu_clean and tcp_clean and iv(ms,"PERIPHERAL_FAILURE_COUNT")==0 and iv(ms,"SD_DATALOG_FAILED_COMMITS")==0)
    row={"fifo":fifo,"mode":mode,"hz":hz,"tcp_req":tcp_req,"tcp_avg":tcp_avg,"tcp_p95":tcp_p95,"tcp_p99":tcp_p99,
         "service_gap":iv(ms,"RTU_SERVICE_GAP_MAX_US"),"loop_gap":iv(ms,"LOOP_GAP_MAX_US"),"clean":clean}
    print(
        f"RTUH3A_CASE FIFO_BYTES={fifo} TCP={mode} RTU_HZ={hz:.3f} "
        f"TCP_REQ_S={tcp_req:.3f} TCP_AVG_US={tcp_avg:.1f} TCP_P95_US={tcp_p95:.1f} TCP_P99_US={tcp_p99:.1f} "
        f"RTU_SERVICE_GAP_MAX_US={row['service_gap']} LOOP_GAP_MAX_US={row['loop_gap']} "
        f"FAILED={failed} TIMEOUTS={timeouts} MASTER_CRC={mcrc} SLAVE_CRC={scrc} RUNTIME_CLEAN={'YES' if clean else 'NO'}"
    )
    return row

def main():
    ap=argparse.ArgumentParser()
    ap.add_argument("--master-serial",default="COM14"); ap.add_argument("--slave-serial",default="COM4")
    ap.add_argument("--host",required=True); ap.add_argument("--port",type=int,default=502)
    ap.add_argument("--duration-per-case",type=float,default=60.0)
    a=ap.parse_args()
    if a.duration_per_case<30: raise ValueError("duration-per-case debe ser >= 30 s")

    master=p5b.open_serial_no_dtr(a.master_serial); slave=p5b.open_serial_no_dtr(a.slave_serial)
    rows=[]
    print("="*78); print(" A14 RTU-H3A - RX FIFO THRESHOLD SWEEP"); print("="*78)
    print("BAUD=500000\nFRAME_GAP_US=100\nCLOCK=APB_FORCED\nFIFO_BYTES=120,32,16,8,1\nTCP_CASES=500,OFF\nMOTOR=ASYNC\nTX_MODE=QUEUED")
    try:
        time.sleep(1.0); configure_base(master,slave)
        for fifo,cmd in FIFO_CASES:
            configure_fifo(master,slave,fifo,cmd)
            rows.append(run_case(master,slave,a.host,a.port,a.duration_per_case,fifo,True))
            rows.append(run_case(master,slave,a.host,a.port,a.duration_per_case,fifo,False))

        print("="*78); print(" RTU-H3A SUMMARY"); print("="*78)
        for r in rows:
            print(f"RTUH3A_SUMMARY FIFO_BYTES={r['fifo']} TCP={r['mode']} RTU_HZ={r['hz']:.3f} TCP_REQ_S={r['tcp_req']:.3f} TCP_AVG_US={r['tcp_avg']:.1f} TCP_P95_US={r['tcp_p95']:.1f} RTU_SERVICE_GAP_MAX_US={r['service_gap']} LOOP_GAP_MAX_US={r['loop_gap']} RUNTIME_CLEAN={'YES' if r['clean'] else 'NO'}")

        for mode in ("TCP500","OFF"):
            baseline=next(r for r in rows if r["fifo"]==120 and r["mode"]==mode)
            if not baseline["clean"]:
                print("A14_RTU_H3A=REVIEW_CONTROL_FAILURE"); return 2
            clean=[r for r in rows if r["mode"]==mode and r["clean"]]
            for r in [x for x in rows if x["mode"]==mode]:
                gain=(r["hz"]/baseline["hz"]-1.0)*100.0 if baseline["hz"]>0 else 0.0
                print(f"RTUH3A_DELTA FIFO_BYTES={r['fifo']} TCP={mode} RTU_GAIN_VS_FIFO120_PCT={gain:.3f}")
            fastest=max(clean,key=lambda x:x["hz"])
            print(f"RTUH3A_FASTEST_CLEAN_{mode}_FIFO_BYTES={fastest['fifo']}")
            print(f"RTUH3A_FASTEST_CLEAN_{mode}_RTU_HZ={fastest['hz']:.3f}")
        print("A14_RTU_H3A=PASS_CHARACTERIZED"); return 0
    finally:
        master.close(); slave.close()

if __name__=="__main__":
    raise SystemExit(main())
