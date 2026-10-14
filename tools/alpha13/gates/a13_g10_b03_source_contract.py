#!/usr/bin/env python3
"""G10-I B03 — detector conservador de cierre UDP abandonado bajo SPI_BUSY.

Este script NO corrige ni cualifica el firmware. Lee fuente real y ejecuta
negativos autocontenidos para evitar que CI reporte falsos cierres. Sin hardware.
"""
from __future__ import annotations
import argparse
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[3]
SOURCE = ROOT / "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src"

def body(source: str, signature: str) -> str:
    pos = source.find(signature)
    if pos < 0 or source.find(signature, pos + 1) >= 0:
        raise ValueError("B03_SIGNATURE_NOT_UNIQUE_" + signature)
    left = source.find("{", pos + len(signature))
    if left < 0:
        raise ValueError("B03_BODY_NOT_FOUND")
    depth = 0
    for i in range(left, len(source)):
        if source[i] == "{":
            depth += 1
        elif source[i] == "}":
            depth -= 1
            if depth == 0:
                return source[left:i+1]
    raise ValueError("B03_BODY_UNBALANCED")

def is_abandoned(source: str, signature: str, stop_expr: str,
                 end_expr: str) -> bool:
    value = body(source, signature)
    stop = value.find(stop_expr)
    finish = value.find(end_expr)
    return stop >= 0 and finish > stop

def negative_mutations() -> None:
    sample = "void X::finish() { udp.stop(); active = false; }"
    assert is_abandoned(sample, "void X::finish()", "udp.stop()", "active = false")
    assert not is_abandoned(sample.replace("active = false", "return;"),
                            "void X::finish()", "udp.stop()", "active = false")
    assert not is_abandoned(sample.replace("udp.stop()", "udp.serviceDeferredClose()"),
                            "void X::finish()", "udp.stop()", "active = false")
    print("G10_I_B03_NEGATIVES=PASS_2_REJECTED")

def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--require-fixed", action="store_true",
                        help="Retorna error si sigue presente el patrón de riesgo")
    args = parser.parse_args()
    negative_mutations()
    dns = (SOURCE / "Dns.cpp").read_text(encoding="utf-8-sig")
    dhcp = (SOURCE / "Dhcp.cpp").read_text(encoding="utf-8-sig")
    checks = (
        ("DNS_FINISH", dns, "void DNSClient::finishResolveAsync(int result)",
         "iUdp.stop()", "iAsyncActive = false"),
        ("DNS_CANCEL", dns, "void DNSClient::cancelResolveAsync()",
         "iUdp.stop()", "iAsyncActive = false"),
        ("DHCP_FINISH", dhcp, "void DhcpClass::finishAsync()",
         "_dhcpUdpSocket.stop()", "_asyncActive = false"),
    )
    hazards = []
    for label, source, signature, stop, finish in checks:
        risk = is_abandoned(source, signature, stop, finish)
        print(f"G10_I_B03_{label}=" +
              ("SOURCE_RISK_PRESENT" if risk else "REVIEW_REQUIRED"))
        if risk:
            hazards.append(label)
    # No afirmar solución solo porque el patrón cambió: posible deriva.
    if len(hazards) != len(checks):
        print("G10_I_B03=SOURCE_DRIFT_REVIEW_STOP")
        return 2
    print("G10_I_B03=KNOWN_RISK_CONFIRMED_NOT_FIXED")
    return 3 if args.require_fixed else 0

if __name__ == "__main__":
    raise SystemExit(main())
