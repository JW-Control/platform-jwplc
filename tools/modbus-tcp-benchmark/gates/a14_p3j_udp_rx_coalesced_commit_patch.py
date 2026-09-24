from __future__ import annotations

import argparse
from pathlib import Path


def replace_once(text: str, old: str, new: str, label: str) -> str:
    count = text.count(old)
    if count != 1:
        raise RuntimeError(f"{label}_MATCH_COUNT={count}")
    return text.replace(old, new, 1)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--ethernet-root", required=True)
    parser.add_argument("--instrumented-sketch", required=True)
    args = parser.parse_args()

    eth_root = Path(args.ethernet_root).resolve()
    sketch = Path(args.instrumented_sketch).resolve()

    header = eth_root / "src" / "JWPLC_W5x00_Ethernet.h"
    socket_cpp = eth_root / "src" / "socket.cpp"
    udp_cpp = eth_root / "src" / "EthernetUdp.cpp"

    for path in (header, socket_cpp, udp_cpp, sketch):
        if not path.is_file():
            raise RuntimeError(f"P3J_FILE_NOT_FOUND={path}")

    h = header.read_text(encoding="utf-8")

    class_decl = (
        "\tstatic int socketRecvUDPFast(\n"
        "\t\tuint8_t s,\n"
        "\t\tuint8_t *header,\n"
        "\t\tuint8_t *buf,\n"
        "\t\tuint16_t len);\n"
    )

    class_decl_new = class_decl + (
        "\t// P3J diagnostic: defer RX_RD/Sock_RECV so batch2 can commit once.\n"
        "\tstatic int socketRecvUDPFastDeferred(\n"
        "\t\tuint8_t s,\n"
        "\t\tuint8_t *header,\n"
        "\t\tuint8_t *buf,\n"
        "\t\tuint16_t len);\n"
        "\tstatic bool socketCommitUDPFast(uint8_t s);\n"
    )

    h = replace_once(
        h,
        class_decl,
        class_decl_new,
        "P3J_ETHERNETCLASS_DECL",
    )

    udp_decl = (
        "\tint jwplcDiagReadPacketFast(uint8_t *buffer, size_t len);\n"
    )

    udp_decl_new = udp_decl + (
        "\tint jwplcDiagReadPacketFastDeferred(uint8_t *buffer, size_t len);\n"
        "\tbool jwplcDiagCommitRxFast();\n"
    )

    h = replace_once(
        h,
        udp_decl,
        udp_decl_new,
        "P3J_UDP_DECL",
    )

    udp_class_start = h.index("class EthernetUDP : public UDP")
    client_class_start = h.index("class EthernetClient : public Client")
    udp_class_text = h[udp_class_start:client_class_start]

    for symbol in (
        "jwplcDiagReadPacketFastDeferred",
        "jwplcDiagCommitRxFast",
    ):
        if udp_class_text.count(symbol) != 1:
            raise RuntimeError(
                f"P3J_UDP_DECL_POSTCONDITION_{symbol}=FAIL"
            )

    header.write_text(h, encoding="utf-8", newline="\n")

    cpp = socket_cpp.read_text(encoding="utf-8")

    anchor = """uint16_t EthernetClass::socketRecvAvailable(uint8_t s)
{
"""

    implementation = """int EthernetClass::socketRecvUDPFastDeferred(
\tuint8_t s,
\tuint8_t *header,
\tuint8_t *buf,
\tuint16_t len)
{
\tif (
\t\ts >= MAX_SOCK_NUM ||
\t\theader == nullptr ||
\t\tbuf == nullptr)
\t{
\t\treturn -1;
\t}

\tSPI.beginTransaction(SPI_ETHERNET_SETTINGS);

\tuint16_t available = state[s].RX_RSR;

\tif (available < 8) {
\t\tuint16_t rsr = 0;
\t\t(void)W5100.readSnRX_RSRStable(s, rsr);

\t\tavailable =
\t\t\trsr >= state[s].RX_inc
\t\t\t\t? (uint16_t)(rsr - state[s].RX_inc)
\t\t\t\t: 0;

\t\tstate[s].RX_RSR = available;
\t}

\tif (available < 8) {
\t\tSPI.endTransaction();
\t\treturn 0;
\t}

\tconst uint16_t ptr = state[s].RX_RD;
\tread_data(s, ptr, header, 8);

\tconst uint16_t payloadLen =
\t\t((uint16_t)header[6] << 8) |
\t\t(uint16_t)header[7];

\tconst uint32_t recordLen =
\t\t8U + (uint32_t)payloadLen;

\tif (
\t\tpayloadLen > len ||
\t\trecordLen > available ||
\t\trecordLen > W5100.SSIZE)
\t{
\t\tSPI.endTransaction();
\t\treturn 0;
\t}

\tif (payloadLen > 0) {
\t\tread_data(
\t\t\ts,
\t\t\t(uint16_t)(ptr + 8U),
\t\t\tbuf,
\t\t\tpayloadLen);
\t}

\tconst uint16_t nextPtr =
\t\t(uint16_t)(ptr + recordLen);

\tconst uint32_t pending =
\t\t(uint32_t)state[s].RX_inc +
\t\trecordLen;

\tif (pending > W5100.SSIZE) {
\t\tSPI.endTransaction();
\t\treturn -1;
\t}

\tstate[s].RX_RD = nextPtr;
\tstate[s].RX_RSR =
\t\t(uint16_t)(available - recordLen);
\tstate[s].RX_inc =
\t\t(uint16_t)pending;

\tSPI.endTransaction();

\treturn (int)payloadLen;
}

bool EthernetClass::socketCommitUDPFast(uint8_t s)
{
\tif (s >= MAX_SOCK_NUM) {
\t\treturn false;
\t}

\tSPI.beginTransaction(SPI_ETHERNET_SETTINGS);

\tif (state[s].RX_inc == 0) {
\t\tSPI.endTransaction();
\t\treturn true;
\t}

\tW5100.writeSnRX_RD(
\t\ts,
\t\tstate[s].RX_RD);

\tconst bool accepted =
\t\tW5100.execCmdSnChecked(
\t\t\ts,
\t\t\tSock_RECV,
\t\t\t1000);

\tif (accepted) {
\t\tstate[s].RX_inc = 0;
\t}

\tSPI.endTransaction();
\treturn accepted;
}

""" + anchor

    cpp = replace_once(
        cpp,
        anchor,
        implementation,
        "P3J_SOCKET_IMPL",
    )

    socket_cpp.write_text(cpp, encoding="utf-8", newline="\n")

    udp = udp_cpp.read_text(encoding="utf-8")

    udp_anchor = """int EthernetUDP::read()
{
"""

    udp_impl = """int EthernetUDP::jwplcDiagReadPacketFastDeferred(
\tuint8_t *buffer,
\tsize_t len)
{
\tif (
\t\tsockindex >= MAX_SOCK_NUM ||
\t\tbuffer == nullptr ||
\t\tlen == 0)
\t{
\t\treturn -1;
\t}

\tuint8_t header[8];

\tconst int got =
\t\tEthernet.socketRecvUDPFastDeferred(
\t\t\tsockindex,
\t\t\theader,
\t\t\tbuffer,
\t\t\t(uint16_t)len);

\tif (got > 0) {
\t\t_remoteIP = header;
\t\t_remotePort =
\t\t\t((uint16_t)header[4] << 8) |
\t\t\t(uint16_t)header[5];
\t\t_remaining = 0;
\t}

\treturn got;
}

bool EthernetUDP::jwplcDiagCommitRxFast()
{
\tif (sockindex >= MAX_SOCK_NUM) {
\t\treturn false;
\t}

\treturn Ethernet.socketCommitUDPFast(
\t\tsockindex);
}

""" + udp_anchor

    udp = replace_once(
        udp,
        udp_anchor,
        udp_impl,
        "P3J_UDP_IMPL",
    )

    udp_cpp.write_text(udp, encoding="utf-8", newline="\n")

    ino = sketch.read_text(encoding="utf-8")

    ino = replace_once(
        ino,
        "udpSocket.jwplcDiagReadPacketFast(\n",
        "udpSocket.jwplcDiagReadPacketFastDeferred(\n",
        "P3J_SKETCH_DEFERRED_READ",
    )

    batch_anchor = """    while (
        udpRxPacketsThisHold <
        UDP_RX_MAX_PACKETS_PER_HOLD);

    const uint32_t udpRxHoldUs =
"""

    batch_replacement = """    while (
        udpRxPacketsThisHold <
        UDP_RX_MAX_PACKETS_PER_HOLD);

    if (
        mode == MODE_UDP_RX &&
        !udpSocket.jwplcDiagCommitRxFast())
    {
        ++transportErrors;
    }

    const uint32_t udpRxHoldUs =
"""

    ino = replace_once(
        ino,
        batch_anchor,
        batch_replacement,
        "P3J_BATCH_COMMIT",
    )

    sketch.write_text(ino, encoding="utf-8", newline="\n")

    verify_h = header.read_text(encoding="utf-8")
    verify_cpp = socket_cpp.read_text(encoding="utf-8")
    verify_udp = udp_cpp.read_text(encoding="utf-8")
    verify_ino = sketch.read_text(encoding="utf-8")

    checks = {
        "DECL_DEFERRED": verify_h.count("socketRecvUDPFastDeferred") == 1,
        "DECL_COMMIT": verify_h.count("socketCommitUDPFast") == 1,
        "IMPL_DEFERRED": verify_cpp.count(
            "EthernetClass::socketRecvUDPFastDeferred"
        ) == 1,
        "IMPL_COMMIT": verify_cpp.count(
            "EthernetClass::socketCommitUDPFast"
        ) == 1,
        "UDP_DEFERRED": verify_udp.count(
            "EthernetUDP::jwplcDiagReadPacketFastDeferred"
        ) == 1,
        "UDP_COMMIT": verify_udp.count(
            "EthernetUDP::jwplcDiagCommitRxFast"
        ) == 1,
        "SKETCH_DEFERRED": verify_ino.count(
            "udpSocket.jwplcDiagReadPacketFastDeferred("
        ) == 1,
        "SKETCH_COMMIT": verify_ino.count(
            "udpSocket.jwplcDiagCommitRxFast()"
        ) == 1,
    }

    failed = [name for name, ok in checks.items() if not ok]
    if failed:
        raise RuntimeError(
            "P3J_POSTCONDITION_FAILED=" +
            ",".join(failed)
        )

    print("P3J_SCOPE=UDP_RX_FUSED_BATCH2_ONLY")
    print("P3J_MAX_PACKETS_PER_COMMIT=2")
    print("P3J_RX_RD_COMMIT_PER_HOLD_TARGET=1")
    print("P3J_SOCK_RECV_PER_HOLD_TARGET=1")
    print("P3J_PRODUCT_SOURCE_MUTATION=NO")
    print("P3J_UDP_RX_COALESCED_COMMIT_PATCH=PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
