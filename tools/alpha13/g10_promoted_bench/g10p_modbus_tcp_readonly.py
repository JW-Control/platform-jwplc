#!/usr/bin/env python3
"""PH0: comprobación READ-ONLY Modbus TCP de dos JWPLC en router aislado.

El PLC COM4 es master RTU y TCP server. COM14 es slave RTU y TCP server.
Este script SOLO envía FC03 (lectura) a TCP/502; nunca writes/scan/upload.
No descubre IP ni abre COM4/COM14 serial. No publicar los IP o MAC de campo.
"""
from __future__ import annotations
import argparse
import ipaddress
import socket
import struct
import sys

LAN = [ipaddress.ip_network(x) for x in
       ("10.0.0.0/8", "172.16.0.0/12", "192.168.0.0/16")]
EXPECT = {"COM4_MASTER": (0x1234, 0x2345, 0x3456, 0x4567),
          "COM14_SLAVE": (0x5678, 0x6789, 0x789A, 0x89AB)}


def fail(message: str) -> None:
    raise RuntimeError("G10P_TCP_REVIEW_STOP_" + message)


def address(raw: str) -> str:
    try:
        ip = ipaddress.IPv4Address(raw)
    except ipaddress.AddressValueError as error:
        fail("NOT_AN_IPV4_" + str(error))
    if not any(ip in net for net in LAN):
        fail("NOT_PRIVATE_ROUTER_LAN_" + raw)
    return str(ip)


def frame(tid: int) -> bytes:
    # MBAP TID | proto=0 | length=6 | unit=1 | FC03 | start=0 | count=4
    return struct.pack(">HHHBBHH", tid, 0, 6, 1, 3, 0, 4)


def recv_exact(sock: socket.socket, length: int) -> bytes:
    b = bytearray()
    while len(b) < length:
        chunk = sock.recv(length - len(b))
        if not chunk:
            fail("TCP_PEER_CLOSED_EARLY")
        b.extend(chunk)
    return bytes(b)


def decode(tid: int, header: bytes, payload: bytes) -> tuple[int, ...]:
    rx_tid, proto, length, unit = struct.unpack(">HHHB", header)
    if (rx_tid, proto, unit) != (tid, 0, 1) or length != len(payload) + 1:
        fail("BAD_MBAP_RESPONSE")
    if len(payload) != 10 or payload[0] != 3 or payload[1] != 8:
        fail("NOT_FC03_FOUR_REGISTERS")
    return struct.unpack(">HHHH", payload[2:])


def read(ip: str, tid: int) -> tuple[int, ...]:
    with socket.create_connection((ip, 502), timeout=4.0) as s:
        s.settimeout(4.0)
        s.sendall(frame(tid))
        header = recv_exact(s, 7)
        length = struct.unpack(">H", header[4:6])[0]
        if length != 11:
            fail("UNEXPECTED_MBAP_LENGTH_" + str(length))
        payload = recv_exact(s, length - 1)
        return decode(tid, header, payload)


def selftest() -> None:
    assert frame(0x1234).hex() == "123400000006010300000004"
    payload = bytes.fromhex("03081234234534564567")
    header = struct.pack(">HHHB", 0x1234, 0, len(payload) + 1, 1)
    assert decode(0x1234, header, payload) == EXPECT["COM4_MASTER"]
    for ip in ("127.0.0.1", "8.8.8.8", "169.254.1.2", "224.0.0.1"):
        try:
            address(ip)
        except RuntimeError:
            pass
        else:
            fail("BAD_ADDRESS_ACCEPTED_" + ip)
    assert address("192.168.1.20") == "192.168.1.20"
    print("G10P_TCP_SYNTHETIC=PASS_FC03_ONLY_LAN_RESTRICTED")


def main() -> None:
    p = argparse.ArgumentParser()
    p.add_argument("--selftest", action="store_true")
    p.add_argument("--com4-ip")
    p.add_argument("--com14-ip")
    args = p.parse_args()
    if args.selftest:
        selftest()
        return
    if not args.com4_ip or not args.com14_ip:
        fail("BOTH_IPS_REQUIRED")
    ips = {"COM4_MASTER": address(args.com4_ip),
           "COM14_SLAVE": address(args.com14_ip)}
    if len(set(ips.values())) != 2:
        fail("DUPLICATE_IP")
    for index, (role, ip) in enumerate(ips.items(), 1):
        regs = read(ip, 0xA130 + index)
        if regs != EXPECT[role]:
            fail("REGISTER_MISMATCH_" + role + "_" + repr(regs))
        print("G10P_TCP_FC03_" + role + "=PASS_4_REGISTERS")
    print("G10P_TCP_DUAL_ROUTER=PASS_2_DIFFERENT_IPS")
    print("G10P_TCP_WRITE=NOT_RUN COM4_SERIAL=NOT_OPENED COM14_SERIAL=NOT_OPENED")


if __name__ == "__main__":
    try:
        main()
    except (OSError, RuntimeError, ValueError, struct.error) as exc:
        print(str(exc), file=sys.stderr)
        sys.exit(2)
