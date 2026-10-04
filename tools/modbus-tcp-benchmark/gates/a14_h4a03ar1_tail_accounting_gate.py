#!/usr/bin/env python3
from __future__ import annotations

import argparse
import re
import socket
import statistics
import subprocess
import sys
import tempfile
import time
from pathlib import Path

BRANCH = "v2.1.0-alpha.14/feature/modbus-tcp"
TAILS_MS = (0, 200, 400)
ORDER_MS = (0, 200, 400, 400, 0, 200)


def kv(k: str, v: object) -> None:
    print(f"{k}={v}")


def proc(cmd: list[str], cwd: Path | None = None) -> subprocess.CompletedProcess[str]:
    return subprocess.run(cmd, cwd=str(cwd) if cwd else None, text=True,
                          stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                          encoding="utf-8", errors="replace")


def git(repo: Path, *args: str) -> str:
    p = proc(["git", "-C", str(repo), *args])
    if p.returncode:
        raise RuntimeError(f"GIT_FAILED {' '.join(args)}\n{p.stdout}")
    return p.stdout.strip()


def one(text: str, key: str) -> str:
    m = re.findall(rf"(?m)^{re.escape(key)}=(.*)\r?$", text)
    if len(m) != 1:
        raise RuntimeError(f"H4A03AR1_LOG_KEY_COUNT_{key}={len(m)}")
    return m[0].strip()


def num(text: str, key: str) -> float:
    return float(one(text, key))


def rawbench_module():
    pc = Path(__file__).resolve().parents[1] / "pc"
    sys.path.insert(0, str(pc))
    import eth14_raw_transport_benchmark as rawbench
    return rawbench


def serial_ack(dut, command: bytes, expected: bytes) -> None:
    dut.ser.reset_input_buffer(); dut.ser.write(command); dut.ser.flush()
    end = time.perf_counter() + 1.5; data = bytearray()
    while time.perf_counter() < end:
        chunk = dut.ser.read(max(1, dut.ser.in_waiting))
        if chunk:
            data.extend(chunk)
            if expected in data:
                return
    raise RuntimeError(f"H4A03AR1_SERIAL_ACK_MISSING={command!r}")


def ready(dut) -> str:
    end = time.perf_counter() + 30.0
    while time.perf_counter() < end:
        s = dut.snapshot(); ip = s.get("IP", "").strip()
        if s.get("RAW_SERVER_READY") == "YES" and s.get("ETH_READY") == "YES" and s.get("ETH_LINK") == "UP" and ip and ip != "0.0.0.0":
            kv("H4A03AR1_DUT_IP", ip); return ip
        time.sleep(0.25)
    raise RuntimeError("H4A03AR1_DUT_READY_TIMEOUT")


def quiescent(dut, rawbench) -> int:
    end = time.perf_counter() + 3.0; prev = None; stable = 0; latest = 0
    while time.perf_counter() < end:
        time.sleep(0.10); s = dut.snapshot()
        if s.get("MODE") != "IDLE": raise RuntimeError("H4A03AR1_IDLE_MODE_LOST")
        latest = rawbench.intval(s, "P3J_R2_IDLE_UDP_DISCARDED")
        stable = stable + 1 if prev is not None and latest == prev else 0; prev = latest
        if stable >= 3:
            return latest
    raise RuntimeError("H4A03AR1_QUIESCENCE_TIMEOUT")


def case(serial: str, tail: float) -> int:
    rb = rawbench_module(); dut = rb.DutSerial(serial); sock = None; measured = {}
    try:
        dut.open(); host = ready(dut)
        serial_ack(dut, b"I", b"ETH14_RAW_IDLE=PASS"); quiescent(dut, rb)
        serial_ack(dut, b"R", b"ETH14_RAW_RESET=PASS")
        sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        sock.setsockopt(socket.SOL_SOCKET, socket.SO_SNDBUF, 4 * 1024 * 1024)
        target = (host, 5002); sock.sendto(b"URX", target); time.sleep(0.08)
        if dut.snapshot().get("MODE") != "UDP_RX": raise RuntimeError("H4A03AR1_ARM_FAILED")
        serial_ack(dut, b"R", b"ETH14_RAW_RESET=PASS"); time.sleep(0.02)
        z = dut.snapshot()
        if z.get("MODE") != "UDP_RX" or any(rb.intval(z, k) for k in ("RX_BYTES","RX_OPERATIONS","TRANSPORT_ERRORS","UDP_SPI_LOCK_ERRORS","TCP_SPI_LOCK_ERRORS")):
            raise RuntimeError("H4A03AR1_ZERO_ARM_FAILED")
        payload = bytearray(1016); sent = 0; seq = 0; start = time.perf_counter(); deadline = start + 5.0
        while time.perf_counter() < deadline:
            payload[:4] = seq.to_bytes(4, "big"); sent += sock.sendto(payload, target); seq = (seq + 1) & 0xFFFFFFFF
        flood_end = time.perf_counter(); elapsed = flood_end - start
        if tail: time.sleep(tail)
        request_tail = time.perf_counter() - flood_end; measured = dut.snapshot(); done_tail = time.perf_counter() - flood_end
        serial_ack(dut, b"I", b"ETH14_RAW_IDLE=PASS"); discarded = quiescent(dut, rb)
    finally:
        if sock: sock.close()
        dut.close()
    packets = rb.intval(measured, "RX_OPERATIONS"); bytes_ = rb.intval(measured, "RX_BYTES")
    errs = rb.intval(measured, "TRANSPORT_ERRORS"); ul = rb.intval(measured, "UDP_SPI_LOCK_ERRORS"); tl = rb.intval(measured, "TCP_SPI_LOCK_ERRORS")
    kv("H4A03AR1_TAIL_TARGET_S", f"{tail:.3f}"); kv("H4A03AR1_DURATION_ACTUAL_S", f"{elapsed:.6f}")
    kv("H4A03AR1_TAIL_BEFORE_SNAPSHOT_REQUEST_S", f"{request_tail:.6f}"); kv("H4A03AR1_TAIL_UNTIL_SNAPSHOT_DONE_S", f"{done_tail:.6f}")
    kv("H4A03AR1_DUT_RX_PACKETS", packets); kv("H4A03AR1_DUT_RX_BYTES", bytes_)
    kv("H4A03AR1_DUT_MBPS_SEND_WINDOW", f"{rb.mbps(bytes_, elapsed):.6f}")
    kv("H4A03AR1_DUT_MBPS_SEND_PLUS_NOMINAL_TAIL", f"{rb.mbps(bytes_, elapsed + tail):.6f}")
    kv("H4A03AR1_DUT_MBPS_CAPTURE_LOWER_BOUND", f"{rb.mbps(bytes_, elapsed + done_tail):.6f}")
    kv("H4A03AR1_DUT_MBPS_CAPTURE_UPPER_BOUND", f"{rb.mbps(bytes_, elapsed + request_tail):.6f}")
    kv("H4A03AR1_TRANSPORT_ERRORS", errs); kv("H4A03AR1_UDP_SPI_LOCK_ERRORS", ul); kv("H4A03AR1_TCP_SPI_LOCK_ERRORS", tl); kv("H4A03AR1_SPI_LOCK_ERRORS_TOTAL", ul + tl)
    kv("H4A03AR1_POSTRUN_DISCARDED", discarded); ok = sent > 0 and packets > 0 and bytes_ > 0 and errs == 0 and ul + tl == 0
    kv("H4A03AR1_FUNCTIONAL_PASS", "YES" if ok else "NO"); return 0 if ok else 2


def gate(serial: str) -> int:
    here = Path(__file__).resolve(); repo = here.parents[3]; gates = here.parent
    if git(repo, "branch", "--show-current") != BRANCH: raise RuntimeError("H4A03AR1_BRANCH_MISMATCH")
    head = git(repo, "rev-parse", "HEAD"); kv("HEAD", head)
    if git(repo, "diff", "--name-only") or git(repo, "diff", "--cached", "--name-only"): raise RuntimeError("H4A03AR1_TRACKED_TREE_NOT_CLEAN")

    base = gates / "a14_h4a03a_minimal_duration_sweep.ps1"
    text = base.read_text(encoding="utf-8")
    anchor = 'Write-Host "H4A03A_COMPILE=PASS"\n\n$results = @{'
    if text.count(anchor) != 1: raise RuntimeError("H4A03AR1_BUILD_ONLY_ANCHOR_INVALID")
    injected = ('Write-Host "H4A03A_COMPILE=PASS"\n'
                'Write-Host "H4A03AR1_BUILD_ONLY_FAST_BUILD=$fastBuild"\n'
                'Write-Host "H4A03AR1_BUILD_ONLY_FAST_SKETCH=$fastSketch"\n'
                'Write-Host "H4A03AR1_BUILD_ONLY=PASS"\n'
                'exit 0\n\n$results = @{')
    tmp_ps1 = gates / "a14_h4a03ar1_buildonly_tmp.ps1"; tmp_ps1.write_text(text.replace(anchor, injected), encoding="utf-8")
    try:
        p = proc(["powershell.exe", "-NoLogo", "-NoProfile", "-ExecutionPolicy", "Bypass", "-File", str(tmp_ps1)])
    finally:
        tmp_ps1.unlink(missing_ok=True)
    print(p.stdout)
    if p.returncode or one(p.stdout, "H4A03AR1_BUILD_ONLY") != "PASS": raise RuntimeError("H4A03AR1_BUILD_STAGE_FAILED")
    build = Path(one(p.stdout, "H4A03AR1_BUILD_ONLY_FAST_BUILD")); sketch = Path(one(p.stdout, "H4A03AR1_BUILD_ONLY_FAST_SKETCH")); cli = Path(r"C:\Program Files\Arduino PLC IDE Tools\arduino-cli.exe")
    if not build.is_dir() or not sketch.is_file() or not cli.is_file(): raise RuntimeError("H4A03AR1_BUILD_ARTIFACT_MISSING")

    buckets = {ms: {k: [] for k in ("raw","nom","lo","hi","bytes")} for ms in TAILS_MS}; counts = {ms: 0 for ms in TAILS_MS}
    temp = Path(tempfile.mkdtemp(prefix="jwplc_a14_h4a03ar1_results_")); kv("H4A03AR1_RESULT_ROOT", temp); kv("H4A03AR1_EXECUTION_ORDER_MS", ",".join(map(str, ORDER_MS)))
    for i, ms in enumerate(ORDER_MS, 1):
        counts[ms] += 1; rn = counts[ms]; print(f"=== H4A0.3A-R1 CASE {i}/6 TAIL={ms}ms RUN={rn}/2 ===")
        up = proc([str(cli), "upload", "--fqbn", "jwplc_local:esp32:jwplcbasic", "--port", serial, "--input-dir", str(build), str(sketch.parent)])
        if up.returncode: print(up.stdout); raise RuntimeError(f"H4A03AR1_UPLOAD_FAIL_T{ms}_R{rn}")
        time.sleep(3.0)
        c = proc([sys.executable, "-B", "-u", str(here), "--case", "--serial", serial, "--tail", f"{ms/1000:.3f}"])
        (temp / f"tail{ms}_run{rn}.log").write_text(c.stdout, encoding="utf-8"); print(c.stdout)
        if c.returncode or one(c.stdout, "H4A03AR1_FUNCTIONAL_PASS") != "YES": raise RuntimeError(f"H4A03AR1_CASE_FAIL_T{ms}_R{rn}")
        for k in ("H4A03AR1_TRANSPORT_ERRORS","H4A03AR1_UDP_SPI_LOCK_ERRORS","H4A03AR1_TCP_SPI_LOCK_ERRORS","H4A03AR1_SPI_LOCK_ERRORS_TOTAL"):
            if num(c.stdout, k) != 0: raise RuntimeError(f"H4A03AR1_NONZERO_{k}")
        for dst,key in (("raw","H4A03AR1_DUT_MBPS_SEND_WINDOW"),("nom","H4A03AR1_DUT_MBPS_SEND_PLUS_NOMINAL_TAIL"),("lo","H4A03AR1_DUT_MBPS_CAPTURE_LOWER_BOUND"),("hi","H4A03AR1_DUT_MBPS_CAPTURE_UPPER_BOUND"),("bytes","H4A03AR1_DUT_RX_BYTES")):
            buckets[ms][dst].append(num(c.stdout, key))

    med = {ms: {k: statistics.median(v) for k,v in buckets[ms].items()} for ms in TAILS_MS}
    raw_delta = (med[400]["raw"] / med[0]["raw"] - 1) * 100; nom_delta = (med[400]["nom"] / med[0]["nom"] - 1) * 100
    interp = "TAIL_ACCOUNTING_BIAS_CONFIRMED" if raw_delta >= 4 and abs(nom_delta) <= 1.5 else ("TAIL_ACCOUNTING_BIAS_PARTIAL" if abs(raw_delta) >= 2 and abs(nom_delta) < abs(raw_delta) else "TAIL_ACCOUNTING_INCONCLUSIVE")
    print("=== H4A0.3A-R1 SUMMARY ==="); kv("H4A03AR1_HISTORICAL_P3K_REPORTED_MBPS", "13.866349"); kv("H4A03AR1_HISTORICAL_METHOD_400MS_TAIL_SEND_ONLY_DENOMINATOR", "YES")
    for ms in TAILS_MS:
        for label,k in (("RAW","raw"),("NOMINAL_CORRECTED","nom"),("CAPTURE_LOWER","lo"),("CAPTURE_UPPER","hi")):
            kv(f"H4A03AR1_TAIL{ms}_{label}_MEDIAN_MBPS", f"{med[ms][k]:.6f}")
    kv("H4A03AR1_RAW_400MS_VS_0MS_PCT", f"{raw_delta:.2f}"); kv("H4A03AR1_NOMINAL_CORRECTED_400MS_VS_0MS_PCT", f"{nom_delta:.2f}"); kv("H4A03AR1_INTERPRETATION", interp); kv("H4A03AR1_INTERPRETATION_IS_GATE_VERDICT", "NO")
    if input("TFT COM14 estable y operativo durante H4A0.3A-R1? (S/N): ").strip().upper() != "S": raise RuntimeError("H4A03AR1_TFT_PHYSICAL_REVIEW")
    if git(repo, "diff", "--name-only") or git(repo, "diff", "--cached", "--name-only"): raise RuntimeError("H4A03AR1_REPOSITORY_MUTATED")
    summary = temp / "SUMMARY.log"; summary.write_text("\n".join(["A14_H4A03AR1_TAIL_ACCOUNTING=PASS", f"HEAD={head}", f"H4A03AR1_RAW_400MS_VS_0MS_PCT={raw_delta:.2f}", f"H4A03AR1_NOMINAL_CORRECTED_400MS_VS_0MS_PCT={nom_delta:.2f}", f"H4A03AR1_INTERPRETATION={interp}", "H4A03AR1_HISTORICAL_P3K_VALIDITY_VERDICT=DEFER_TO_EXACT_REPLAY", "HARNESS_FAILURE=NO", "PRODUCT_FAILURE=NO_EVIDENCE", "HARDWARE_FAILURE=NO_EVIDENCE", "NEXT=RETURN_TO_CHAT_INTERPRET_R1_THEN_EXACT_P3K_REPLAY"]) + "\n", encoding="utf-8")
    kv("H4A03AR1_SUMMARY_LOG", summary); kv("HARNESS_FAILURE", "NO"); kv("PRODUCT_FAILURE", "NO_EVIDENCE"); kv("HARDWARE_FAILURE", "NO_EVIDENCE"); kv("A14_H4A03AR1_TAIL_ACCOUNTING", "PASS"); kv("NEXT", "RETURN_TO_CHAT_INTERPRET_R1_THEN_EXACT_P3K_REPLAY"); return 0


def main() -> int:
    ap = argparse.ArgumentParser(); ap.add_argument("--serial", default="COM14"); ap.add_argument("--case", action="store_true", help=argparse.SUPPRESS); ap.add_argument("--tail", type=float, default=0.0, help=argparse.SUPPRESS); a = ap.parse_args()
    return case(a.serial, a.tail) if a.case else gate(a.serial)


if __name__ == "__main__":
    try: raise SystemExit(main())
    except SystemExit: raise
    except Exception as e:
        kv("HARNESS_FAILURE", "UNCLASSIFIED"); kv("PRODUCT_FAILURE", "UNCLASSIFIED"); kv("HARDWARE_FAILURE", "UNCLASSIFIED"); kv("H4A03AR1_FAILURE_REQUIRES_CLASSIFICATION", "YES"); kv("H4A03AR1_EXCEPTION", str(e)); raise SystemExit(1)
