#!/usr/bin/env python3
"""Alpha13 F137: eco UDP de COM4, solo LAN local y sin acceder a USB/flash.

Puerto UDP 15020 usado por el sketch G10P_COM4_Master. Los paquetes son
menores de 96 bytes. No ejecuta escritura Modbus, no desconecta RJ45.
"""
from __future__ import annotations

import argparse
import socket
import sys
import time

from g10p_modbus_tcp_readonly import address

UDP_PORT = 15020
PAYLOAD = b"F137-UDP-ECHO-READONLY"


def stop(reason: str) -> None:
    raise RuntimeError("F137_UDP_REVIEW_STOP_" + reason)


def validate_reply(ip: str, payload: bytes, data: bytes, peer: tuple) -> None:
    if data != payload:
        stop("WRONG_ECHO_CONTENT")
    if len(peer) < 2 or peer[0] != ip or peer[1] != UDP_PORT:
        stop("UNEXPECTED_UDP_PEER")


def echo(ip: str) -> float:
    start = time.monotonic()
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sock:
        sock.settimeout(3.0)
        sent = sock.sendto(PAYLOAD, (ip, UDP_PORT))
        if sent != len(PAYLOAD):
            stop("PARTIAL_SEND")
        reply, peer = sock.recvfrom(128)
        validate_reply(ip, PAYLOAD, reply, peer)
    return (time.monotonic() - start) * 1000


def selftest() -> None:
    assert len(PAYLOAD) < 96
    ip = address("192.168.0.159")
    validate_reply(ip, PAYLOAD, PAYLOAD, (ip, UDP_PORT))
    failures = (
        (b"wrong", (ip, UDP_PORT)),
        (PAYLOAD, ("192.168.0.31", UDP_PORT)),
        (PAYLOAD, (ip, UDP_PORT + 1)),
        (PAYLOAD, (ip,)),
    )
    for data, peer in failures:
        try:
            validate_reply(ip, PAYLOAD, data, peer)
        except RuntimeError:
            pass
        else:
            raise AssertionError("FALSE_PASS_UDP_NEGATIVE")
    print("F137_UDP_SELFTEST=PASS_4_NEGATIVES_NO_NETWORK_NO_USB")


def main() -> None:
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--selftest", action="store_true")
    p.add_argument("--com4-ip")
    p.add_argument("--samples", type=int, default=3)
    args = p.parse_args()
    if args.selftest:
        selftest()
        return
    if not args.com4_ip:
        stop("COM4_IP_REQUIRED")
    if not 1 <= args.samples <= 5:
        stop("SAMPLES_OUTSIDE_1_TO_5")
    ip = address(args.com4_ip)  # solo IPv4 privada; no remoto publico
    results = []
    for n in range(args.samples):
        results.append(echo(ip))
        if n + 1 != args.samples:
            time.sleep(0.4)
    print(f"F137_UDP_COM4=PASS_{args.samples}_OF_{args.samples}_ECHO")
    print("F137_UDP_LATENCY_MS=" + ",".join(f"{ms:.2f}" for ms in results))
    print("F137_UDP_USB=NOT_OPENED")
    print("F137_UDP_MODBUS_WRITE=NOT_RUN")


if __name__ == "__main__":
    try:
        main()
    except (OSError, RuntimeError, ValueError, AssertionError) as ex:
        print(str(ex) if isinstance(ex, RuntimeError)
              else "F137_UDP_REVIEW_STOP_NETWORK_OR_TEST", file=sys.stderr)
        sys.exit(2)
