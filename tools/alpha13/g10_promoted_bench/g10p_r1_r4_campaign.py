#!/usr/bin/env python3
"""G10-PH0 sobre package promovido: R1/R2/R3-externo/R4 sin upload, MAC ni COM.

No certifica BUS_BUSY, leases internos, DNS ni memoria sin la instrumentación adecuada.
El operador debe capturar UART de ambos PLC para acreditar liveness y periféricos.
"""
from __future__ import annotations

import argparse
import datetime as dt
import ipaddress
import json
from pathlib import Path
import socket
import struct
import subprocess
import sys
import tempfile
import time

from g10p_modbus_tcp_readonly import EXPECT, address, read

ROOT = Path(__file__).resolve().parents[3]
PACKAGE_HEAD = "7c1974c659be4aa88c877a849db6efc32673da60"
BRANCH = "codex/alpha13-g10-promote-functional-20261019"
R1_SECONDS = 605
R2_CYCLES = 3
R2_LINK_DOWN_SECONDS = 12
R2_RECOVERY_SECONDS = 96
R3_CHURN_SECONDS = 90
R4_SECONDS = 1805
UDP_PORT = 15020
# La placa sólo dispone de packet[96]; no enviar datagramas grandes.
UDP_MSG = b"G10PH0-R1R4-ECHO"

def stop(reason: str) -> None:
    raise RuntimeError("G10P_R_REVIEW_STOP_" + reason)

def git(*args: str) -> str:
    p = subprocess.run(["git", "-C", str(ROOT), *args],
                       capture_output=True, text=True, errors="replace", timeout=20)
    if p.returncode:
        stop("GIT_" + "_".join(args[:2]))
    return p.stdout.strip()

def preflight() -> None:
    if git("branch", "--show-current") != BRANCH:
        stop("WRONG_BRANCH")
    if git("status", "--porcelain", "--untracked-files=no"):
        stop("DIRTY_TRACKED")
    git("merge-base", "--is-ancestor", PACKAGE_HEAD, "HEAD")
    # El código actualmente flasheado no puede diferir por una edición de herramientas.
    protected = (
        "JWPLC/",
        "tools/alpha13/g10_promoted_bench/G10P_COM4_Master/",
        "tools/alpha13/g10_promoted_bench/G10P_COM14_Slave/",
        "tools/alpha13/g10_promoted_bench/g10p_dual_build.py",
    )
    if git("diff", "--name-only", PACKAGE_HEAD, "HEAD", "--", *protected):
        stop("INSTALLED_FIRMWARE_SOURCE_DRIFT")
    print("G10P_R_N0=PASS_PACKAGE_FIRMWARE_UNCHANGED_NO_UPLOAD", flush=True)

def ips_valid(com4: str, com14: str) -> dict[str, str]:
    a, b = address(com4), address(com14)
    if a == b:
        stop("DUPLICATE_IP")
    if ipaddress.ip_address(a) not in ipaddress.ip_network("192.168.0.0/16") or (
        ipaddress.ip_address(b) not in ipaddress.ip_network("192.168.0.0/16")
    ):
        stop("ROUTER_EXPECTED_192_168_LAN")
    return {"COM4_MASTER": a, "COM14_SLAVE": b}

def tcp_read(role: str, ip: str, seq: int) -> float:
    start = time.monotonic()
    registers = read(ip, (0xA000 + (seq % 4096)) & 65535)
    if registers != EXPECT[role]:
        stop("FC03_CONTENT_" + role)
    return (time.monotonic() - start) * 1000.0

def udp_echo(ip: str) -> float:
    start = time.monotonic()
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as s:
        s.settimeout(3.0)
        s.sendto(UDP_MSG, (ip, UDP_PORT))
        data, addr = s.recvfrom(128)
    if data != UDP_MSG or addr[0] != ip or addr[1] != UDP_PORT:
        stop("UDP_ECHO_CONTENT_OR_PEER")
    return (time.monotonic() - start) * 1000.0

def roundtrip(ips: dict[str, str], seq: int, *, udp: bool = True) -> dict:
    r = {
        "com4_ms": round(tcp_read("COM4_MASTER", ips["COM4_MASTER"], seq), 2),
        "com14_ms": round(tcp_read("COM14_SLAVE", ips["COM14_SLAVE"], seq + 1), 2),
    }
    if udp:
        r["udp_ms"] = round(udp_echo(ips["COM4_MASTER"]), 2)
    return r

def run_window(phase: str, seconds: int, ips: dict[str, str],
               report: dict, udp: bool = True) -> None:
    t0 = time.monotonic()
    seq = 0
    report["samples"] = []
    while time.monotonic() - t0 < seconds:
        try:
            sample = roundtrip(ips, seq, udp=udp)
        except (OSError, TimeoutError, ValueError, struct.error) as error:
            stop(phase + "_TRAFFIC_" + type(error).__name__)
        sample["elapsed_s"] = round(time.monotonic() - t0, 1)
        report["samples"].append(sample)
        seq += 1
        if seq % 30 == 0:
            print(f"G10P_{phase}_PROGRESS={seq}_SAMPLES_{int(time.monotonic()-t0)}S",
                  flush=True)
        time.sleep(max(0.0, min(1.0, seconds - (time.monotonic()-t0))))
    if seq < 2:
        stop(phase + "_INSUFFICIENT_SAMPLES")
    report["duration_s"] = round(time.monotonic() - t0, 2)
    print(f"G10P_{phase}_NETWORK=PASS_{seq}_SAMPLES", flush=True)

def operator_confirm(prompt: str) -> None:
    answer = input(prompt + " Escribe CONFIRMO: ").strip().upper()
    if answer != "CONFIRMO":
        stop("OPERATOR_EVENT_NOT_CONFIRMED")

def r2(ips: dict[str, str], report: dict) -> None:
    report["cycles"] = []
    for cycle in range(1, R2_CYCLES + 1):
        roundtrip(ips, cycle)
        print(f"G10P_R2_CYCLE={cycle}: desconecta SOLO RJ45 de COM4, NO USB/RTU.",
              flush=True)
        operator_confirm("RJ45 COM4 desconectado y monitor UART en marcha.")
        down_start = time.monotonic()
        off_samples = 0
        while time.monotonic() - down_start < R2_LINK_DOWN_SECONDS:
            try:
                tcp_read("COM4_MASTER", ips["COM4_MASTER"], cycle)
            except (OSError, TimeoutError, ConnectionError):
                off_samples += 1
            else:
                stop("R2_TARGET_STILL_TCP_REACHABLE_CYCLE_" + str(cycle))
            # COM14 debe responder mientras COM4 no está en red.
            tcp_read("COM14_SLAVE", ips["COM14_SLAVE"], cycle)
            time.sleep(0.5)
        if off_samples < 2:
            stop("R2_NO_LINK_DOWN_EVIDENCE_" + str(cycle))
        print(f"G10P_R2_CYCLE={cycle}_TCP_DOWN=OBSERVED_{off_samples}_FAILS",
              flush=True)
        operator_confirm("Reconecta RJ45 COM4; observa UART, PHY y DHCP.")
        up_start = time.monotonic()
        recovered = False
        attempts = 0
        while time.monotonic() - up_start < R2_RECOVERY_SECONDS:
            attempts += 1
            try:
                roundtrip(ips, cycle + attempts)
                recovered = True
                break
            except (OSError, TimeoutError, ValueError, struct.error):
                time.sleep(1.0)
        if not recovered:
            stop("R2_RECOVERY_NOT_PROVEN_SAME_IP_CYCLE_" + str(cycle))
        # La red sola no certifica PHY/DHCP: debe cotejarse UART.
        record = {"cycle": cycle, "tcp_off_samples": off_samples,
                  "recovery_s": round(time.monotonic() - up_start, 2),
                  "operator_uart_confirmation": "PENDING"}
        report["cycles"].append(record)
        print(f"G10P_R2_CYCLE={cycle}_NETWORK_RECOVERY=PASS",
              flush=True)
    print("G10P_R2_NETWORK=PASS_3_CYCLES_UART_PHY_DHCP_PENDING", flush=True)

def selftest() -> None:
    assert R1_SECONDS == 605 and R4_SECONDS == 1805
    assert (R2_CYCLES, R2_LINK_DOWN_SECONDS, R2_RECOVERY_SECONDS) == (3, 12, 96)
    assert len(UDP_MSG) < 96
    assert ips_valid("192.168.0.159", "192.168.0.31")["COM14_SLAVE"] == "192.168.0.31"
    for a,b in (("192.168.0.1","192.168.0.1"),
                ("8.8.8.8","192.168.0.31"),
                ("127.0.0.1","192.168.0.31")):
        try:
            ips_valid(a,b)
        except RuntimeError:
            pass
        else:
            stop("NEGATIVE_IP_ACCEPTED")
    # No se abren sockets durante un selftest; los flujos sólo lectura
    # reutilizan el decoder FC03 sintético validado y prueban su validez.
    from g10p_modbus_tcp_readonly import frame, decode
    f = frame(0x41)
    assert f.hex() == "004100000006010300000004"
    regs = EXPECT["COM4_MASTER"]
    payload = b"\x03\x08" + struct.pack(">4H", *regs)
    header = struct.pack(">HHHB", 0x41, 0, len(payload)+1, 1)
    assert decode(0x41, header, payload) == regs
    print("G10P_R_N0=PASS_PROTOCOL_605_3X12_96_1805_FC03_UDP_BOUNDS_IP_NEGATIVES")
    print("G10P_R_HARDWARE=NOT_TOUCHED")

def main() -> None:
    p = argparse.ArgumentParser(description=__doc__)
    mode = p.add_mutually_exclusive_group(required=True)
    mode.add_argument("--selftest", action="store_true")
    mode.add_argument("--check", action="store_true")
    mode.add_argument("--phase", choices=["R1","R2","R3","R4"])
    p.add_argument("--com4-ip")
    p.add_argument("--com14-ip")
    p.add_argument("--output", type=Path)
    args = p.parse_args()
    if args.selftest:
        selftest()
        return
    preflight()
    if args.check:
        print("G10P_R_PREFLIGHT=PASS_NO_NETWORK_NO_USB")
        return
    if not args.com4_ip or not args.com14_ip:
        stop("BOTH_IPS_REQUIRED")
    ips = ips_valid(args.com4_ip, args.com14_ip)
    if args.output is None:
        stop("OUTPUT_REQUIRED_OUTSIDE_GIT")
    out = args.output.resolve()
    if out.is_relative_to(ROOT):
        stop("OUTPUT_INSIDE_CHECKOUT")
    out.mkdir(parents=True, exist_ok=True)
    file = out / ("G10P_" + args.phase + "_RESULT.json")
    if file.exists():
        stop("EVIDENCE_ALREADY_EXISTS")
    report = {
        "phase": args.phase, "status": "INCOMPLETE", "firmware_head": PACKAGE_HEAD,
        "script_head": git("rev-parse","HEAD"), "utc": dt.datetime.now(dt.timezone.utc).isoformat(),
        "network": ips, "mac_checked": False, "uart_opened": False,
        "upload": "NOT_RUN", "writes": "NOT_RUN",
        "limits": "UART RTU/PHY/peripherals must be checked by operator",
    }
    try:
        if args.phase == "R1":
            run_window("R1", R1_SECONDS, ips, report)
            report["status"] = "NETWORK_PASS_UART_PERIPHERALS_PENDING"
        elif args.phase == "R2":
            r2(ips, report)
            report["status"] = "NETWORK_RECOVERY_PASS_UART_PHY_DHCP_PENDING"
        elif args.phase == "R3":
            run_window("R3_EXTERNAL", R3_CHURN_SECONDS, ips, report, udp=False)
            report["status"] = "EXTERNAL_SOCKET_CHURN_PASS_INTERNAL_LEASES_UNTESTED"
            print("G10P_R3_INTERNAL=NOT_TESTED_NO_FAULT_INJECTION", flush=True)
        else:
            run_window("R4", R4_SECONDS, ips, report)
            report["status"] = "NETWORK_ENDURANCE_PASS_MEM_SPI_UART_PENDING"
    except (KeyboardInterrupt, RuntimeError, OSError, ValueError) as error:
        report["status"] = "REVIEW_STOP"
        report["error_type"] = type(error).__name__
        print("G10P_" + args.phase + "_REVIEW_STOP=" + type(error).__name__,
              file=sys.stderr, flush=True)
        raise
    finally:
        file.write_text(json.dumps(report, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
        print("G10P_" + args.phase + "_LOCAL_EVIDENCE=" + str(file), flush=True)
        print("G10P_" + args.phase + "_MAC=NOT_READ SERIAL=NOT_OPENED FLASH=NOT_TOUCHED")

if __name__ == "__main__":
    try:
        main()
    except (RuntimeError, OSError, ValueError) as error:
        print(str(error), file=sys.stderr)
        sys.exit(2)
    except KeyboardInterrupt:
        print("G10P_R_REVIEW_STOP_OPERATOR_ABORT", file=sys.stderr)
        sys.exit(130)
