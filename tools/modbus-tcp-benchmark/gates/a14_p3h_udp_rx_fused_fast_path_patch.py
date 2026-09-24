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
            raise RuntimeError(f"P3H_FILE_NOT_FOUND={path}")

    h = header.read_text(encoding="utf-8")

    h = replace_once(
        h,
        "static uint16_t socketRecvAvailable(uint8_t s);\n",
        "static uint16_t socketRecvAvailable(uint8_t s);\n"
        "\t// P3H diagnostic fused UDP RX path. Reads one complete W5500 UDP\n"
        "\t// record (8-byte pseudo-header + payload) under one SPI transaction.\n"
        "\tstatic int socketRecvUDPFast(\n"
        "\t\tuint8_t s,\n"
        "\t\tuint8_t *header,\n"
        "\t\tuint8_t *buf,\n"
        "\t\tuint16_t len);\n",
        "P3H_ETHERNETCLASS_DECL",
    )

    h = replace_once(
        h,
        "\tvirtual int read(uint8_t *buf, size_t size);\n",
        "\tvirtual int read(uint8_t *buf, size_t size);\n"
        "\t// P3H diagnostic-only API in the isolated benchmark copy.\n"
        "\tint jwplcDiagReadPacketFast(uint8_t *buffer, size_t len);\n",
        "P3H_UDP_DECL",
    )

    header.write_text(h, encoding="utf-8", newline="\n")

    cpp = socket_cpp.read_text(encoding="utf-8")

    anchor = """uint16_t EthernetClass::socketRecvAvailable(uint8_t s)
{
"""

    implementation = """int EthernetClass::socketRecvUDPFast(
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
\t\tavailable = rsr - state[s].RX_inc;
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
\t\trecordLen > available)
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

\tstate[s].RX_RD = nextPtr;
\tstate[s].RX_RSR =
\t\t(uint16_t)(available - recordLen);

\t// Commit both any previously deferred bytes and this complete datagram.
\tstate[s].RX_inc = 0;
\tW5100.writeSnRX_RD(s, nextPtr);

\tconst bool recvAccepted =
\t\tW5100.execCmdSnChecked(
\t\t\ts,
\t\t\tSock_RECV,
\t\t\t1000);

\tSPI.endTransaction();

\treturn recvAccepted
\t\t? (int)payloadLen
\t\t: -1;
}

""" + anchor

    cpp = replace_once(
        cpp,
        anchor,
        implementation,
        "P3H_SOCKET_FAST_IMPL",
    )

    socket_cpp.write_text(cpp, encoding="utf-8", newline="\n")

    udp = udp_cpp.read_text(encoding="utf-8")

    udp_anchor = """int EthernetUDP::read()
{
"""

    udp_impl = """int EthernetUDP::jwplcDiagReadPacketFast(
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
\t\tEthernet.socketRecvUDPFast(
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

""" + udp_anchor

    udp = replace_once(
        udp,
        udp_anchor,
        udp_impl,
        "P3H_UDP_FAST_IMPL",
    )

    udp_cpp.write_text(udp, encoding="utf-8", newline="\n")

    ino = sketch.read_text(encoding="utf-8")

    service_anchor = """static void serviceUdpUnlocked()
{
"""

    service_block = """static void serviceUdpUnlocked()
{
    if (mode == MODE_UDP_RX)
    {
        const uint32_t spiReadCallsAtEntry =
            W5100.jwplcDiagReadCalls();
        const uint64_t spiReadBytesAtEntry =
            W5100.jwplcDiagReadBytes();

        const uint32_t fastStartUs = micros();

        const int fastRead =
            udpSocket.jwplcDiagReadPacketFast(
                udpBuffer,
                sizeof(udpBuffer));

        const uint32_t fastUs =
            (uint32_t)(micros() - fastStartUs);

        ++udpRxParseCalls;
        udpRxParseTotalUs += fastUs;

        if (fastUs > udpRxParseMaxUs)
        {
            udpRxParseMaxUs = fastUs;
        }

        if (fastRead < 0)
        {
            ++transportErrors;
            return;
        }

        if (fastRead == 0)
        {
            return;
        }

        rxBytes += (uint32_t)fastRead;
        ++rxOperations;
        ++udpRxPackets;
        ++udpRxPacketParseCalls;
        udpRxPacketParseTotalUs += fastUs;

        if (fastUs > udpRxPacketParseMaxUs)
        {
            udpRxPacketParseMaxUs = fastUs;
        }

        const uint32_t spiReadCallsAfter =
            W5100.jwplcDiagReadCalls();
        const uint64_t spiReadBytesAfter =
            W5100.jwplcDiagReadBytes();

        udpRxSpiReadCalls +=
            (uint32_t)(
                spiReadCallsAfter -
                spiReadCallsAtEntry);

        udpRxSpiReadBytes +=
            (uint64_t)(
                spiReadBytesAfter -
                spiReadBytesAtEntry);

        return;
    }

"""

    ino = replace_once(
        ino,
        service_anchor,
        service_block,
        "P3H_RAW_FAST_BRANCH",
    )

    sketch.write_text(ino, encoding="utf-8", newline="\n")

    print("P3H_FAST_PATH_SCOPE=UDP_RX_ONLY")
    print("P3H_FAST_PATH_HEADER_BYTES=8")
    print("P3H_FAST_PATH_SPI_TRANSACTION_PER_PACKET=1")
    print("P3H_LEGACY_UDP_API_REPLACED=NO")
    print("P3H_PRODUCT_SOURCE_MUTATION=NO")
    print("P3H_UDP_RX_FUSED_FAST_PATH_PATCH=PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
