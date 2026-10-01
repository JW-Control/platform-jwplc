#!/usr/bin/env python3
import argparse
import socket
import time
import serial
import a14_g3a_modbus_tcp_int_case as g3a

def emit(k,v):
    print(f"{k}={v}")

def main():
    p=argparse.ArgumentParser()
    p.add_argument("--serial",default="COM14")
    p.add_argument("--duration",type=float,default=15.0)
    p.add_argument("--variant",choices=("POLLING","ADAPTIVE"),required=True)
    p.add_argument("--expected-hot-poll-us",type=int,default=0)
    a=p.parse_args()
    expected="YES" if a.variant=="ADAPTIVE" else "NO"
    ser=serial.Serial()
    ser.port=a.serial; ser.baudrate=115200; ser.timeout=0.05; ser.write_timeout=1.0
    ser.dtr=False; ser.rts=False
    sock=None
    try:
        ser.open(); time.sleep(0.3)
        ready=g3a.wait_ready(ser); host=ready["ETH_IP"]
        if ready.get("INT_GUIDED_RX_BUILD")!=expected:
            raise RuntimeError("D2_IDLE_INT_VARIANT_MISMATCH")
        if int(ready.get("INT_HOT_POLL_US_BUILD","-1"))!=a.expected_hot_poll_us:
            raise RuntimeError("D2_IDLE_HOT_POLL_VARIANT_MISMATCH")
        sock=socket.create_connection((host,502),timeout=3.0)
        sock.settimeout(1.0)
        sock.setsockopt(socket.IPPROTO_TCP,socket.TCP_NODELAY,1)
        g3a.warmup_transaction(sock)
        g3a.wait_connected(ser)
        g3a.reset_stats(ser)
        t0=time.perf_counter(); time.sleep(a.duration); elapsed=time.perf_counter()-t0
        s=g3a.snapshot(ser)
        rx=g3a.intval(s,"RX_FRAMES"); tx=g3a.intval(s,"TX_FRAMES")
        ok=g3a.intval(s,"REQUESTS_OK"); payload=g3a.intval(s,"TCP_PROF_PAYLOAD_BYTES")
        status=g3a.intval(s,"TCP_PROF_SOCKET_STATUS_CALLS")
        avail=g3a.intval(s,"TCP_PROF_AVAILABLE_CALLS")
        zero=g3a.intval(s,"TCP_PROF_AVAILABLE_ZERO_CALLS")
        nonzero=g3a.intval(s,"TCP_PROF_AVAILABLE_NONZERO_CALLS")
        errors=(g3a.intval(s,"PROTOCOL_ERRORS")+g3a.intval(s,"FRAME_TIMEOUTS")+g3a.intval(s,"BUS_LOCK_TIMEOUTS"))
        clean=(rx==0 and tx==0 and ok==0 and payload==0 and errors==0 and zero+nonzero==avail and s.get("CLIENT_CONNECTED")=="YES")
        emit("D2_IDLE_VARIANT",a.variant)
        emit("D2_IDLE_DURATION_S",f"{elapsed:.3f}")
        emit("D2_IDLE_STATUS_CALLS",status)
        emit("D2_IDLE_AVAILABLE_CALLS",avail)
        emit("D2_IDLE_AVAILABLE_ZERO",zero)
        emit("D2_IDLE_LOOP_AVG_US",g3a.intval(s,"LOOP_GAP_AVG_US"))
        emit("D2_IDLE_LOOP_MAX_US",g3a.intval(s,"LOOP_GAP_MAX_US"))
        emit("D2_IDLE_FUNCTIONAL_PASS","YES" if clean else "NO")
        return 0 if clean else 2
    finally:
        if sock is not None:
            try: sock.close()
            except OSError: pass
        if ser.is_open: ser.close()

if __name__=="__main__":
    raise SystemExit(main())
