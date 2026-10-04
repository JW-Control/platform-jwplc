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
    args = parser.parse_args()

    eth_root = Path(args.ethernet_root).resolve()
    w5100_h = eth_root / "src" / "utility" / "w5100.h"
    w5100_cpp = eth_root / "src" / "utility" / "w5100.cpp"
    socket_cpp = eth_root / "src" / "socket.cpp"

    for path in (w5100_h, w5100_cpp, socket_cpp):
        if not path.is_file():
            raise RuntimeError(f"P3I_FILE_NOT_FOUND={path}")

    h = w5100_h.read_text(encoding="utf-8")

    h = replace_once(
        h,
        "  static uint64_t jwplcDiagReadBytes(void);\n",
        "  static uint64_t jwplcDiagReadBytes(void);\n"
        "  // P3I diagnostic W5500-only RX burst: one CS assertion and one\n"
        "  // 3-byte W5500 command header for UDP pseudo-header + payload.\n"
        "  static int jwplcDiagReadRxRecordBurst(\n"
        "      uint16_t addr,\n"
        "      uint16_t available,\n"
        "      uint8_t *header,\n"
        "      uint8_t *buf,\n"
        "      uint16_t maxLen);\n",
        "P3I_W5100_DECL",
    )

    w5100_h.write_text(h, encoding="utf-8", newline="\n")

    cpp = w5100_cpp.read_text(encoding="utf-8")

    impl_anchor = """void W5100Class::jwplcDiagResetReadCounters(void)
{
"""

    impl = """int W5100Class::jwplcDiagReadRxRecordBurst(
\tuint16_t addr,
\tuint16_t available,
\tuint8_t *header,
\tuint8_t *buf,
\tuint16_t maxLen)
{
\tif (
\t\tchip != 55 ||
\t\theader == nullptr ||
\t\tbuf == nullptr ||
\t\tavailable < 8)
\t{
\t\treturn 0;
\t}

\tuint8_t cmd[3];
\tcmd[0] = addr >> 8;
\tcmd[1] = addr & 0xFF;

\t#if defined(ETHERNET_LARGE_BUFFERS) && MAX_SOCK_NUM <= 1
\tcmd[2] = 0x18;
\t#elif defined(ETHERNET_LARGE_BUFFERS) && MAX_SOCK_NUM <= 2
\tcmd[2] = ((addr >> 8) & 0x20) | 0x18;
\t#elif defined(ETHERNET_LARGE_BUFFERS) && MAX_SOCK_NUM <= 4
\tcmd[2] = ((addr >> 7) & 0x60) | 0x18;
\t#else
\tcmd[2] = ((addr >> 6) & 0xE0) | 0x18;
\t#endif

\tsetSS();
\tSPI.transfer(cmd, 3);

\tmemset(header, 0, 8);
\tSPI.transfer(header, 8);

\tconst uint16_t payloadLen =
\t\t((uint16_t)header[6] << 8) |
\t\t(uint16_t)header[7];

\tconst uint32_t recordLen =
\t\t8U + (uint32_t)payloadLen;

\tif (
\t\tpayloadLen > maxLen ||
\t\trecordLen > available)
\t{
\t\tresetSS();
\t\t++jwplcDiagReadCallsCounter;
\t\tjwplcDiagReadBytesCounter += 8;
\t\treturn 0;
\t}

\tif (payloadLen > 0) {
\t\tmemset(buf, 0, payloadLen);
\t\tSPI.transfer(buf, payloadLen);
\t}

\tresetSS();

\t++jwplcDiagReadCallsCounter;
\tjwplcDiagReadBytesCounter += recordLen;

\treturn (int)payloadLen;
}

""" + impl_anchor

    cpp = replace_once(
        cpp,
        impl_anchor,
        impl,
        "P3I_W5100_BURST_IMPL",
    )

    w5100_cpp.write_text(cpp, encoding="utf-8", newline="\n")

    sock = socket_cpp.read_text(encoding="utf-8")

    old_read = """\tconst uint16_t ptr = state[s].RX_RD;
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
"""

    new_read = """\tconst uint16_t ptr = state[s].RX_RD;
\tconst uint16_t srcMask =
\t\t(uint16_t)ptr & W5100.SMASK;
\tconst uint16_t srcPtr =
\t\tW5100.RBASE(s) + srcMask;

\tconst int burstPayloadLen =
\t\tW5100.jwplcDiagReadRxRecordBurst(
\t\t\tsrcPtr,
\t\t\tavailable,
\t\t\theader,
\t\t\tbuf,
\t\t\tlen);

\tif (burstPayloadLen <= 0)
\t{
\t\tSPI.endTransaction();
\t\treturn burstPayloadLen;
\t}

\tconst uint16_t payloadLen =
\t\t(uint16_t)burstPayloadLen;

\tconst uint32_t recordLen =
\t\t8U + (uint32_t)payloadLen;
"""

    sock = replace_once(
        sock,
        old_read,
        new_read,
        "P3I_SOCKET_SINGLE_CS",
    )

    socket_cpp.write_text(sock, encoding="utf-8", newline="\n")

    verify_h = w5100_h.read_text(encoding="utf-8")
    verify_cpp = w5100_cpp.read_text(encoding="utf-8")
    verify_sock = socket_cpp.read_text(encoding="utf-8")

    if verify_h.count("jwplcDiagReadRxRecordBurst") != 1:
        raise RuntimeError("P3I_DECL_POSTCONDITION_FAILED")

    if verify_cpp.count("W5100Class::jwplcDiagReadRxRecordBurst") != 1:
        raise RuntimeError("P3I_IMPL_POSTCONDITION_FAILED")

    if verify_sock.count("W5100.jwplcDiagReadRxRecordBurst") != 1:
        raise RuntimeError("P3I_CALL_POSTCONDITION_FAILED")

    print("P3I_SCOPE=UDP_RX_FUSED_ONLY")
    print("P3I_W5500_ONLY=YES")
    print("P3I_SINGLE_CS_HEADER_AND_PAYLOAD=YES")
    print("P3I_SPI_COMMAND_HEADERS_PER_PACKET_TARGET=1")
    print("P3I_PRODUCT_SOURCE_MUTATION=NO")
    print("P3I_UDP_RX_SINGLE_CS_BURST_PATCH=PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
