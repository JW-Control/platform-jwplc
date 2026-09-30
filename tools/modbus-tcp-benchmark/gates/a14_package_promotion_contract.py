#!/usr/bin/env python3
from __future__ import annotations

from pathlib import Path


def require(condition: bool, label: str) -> None:
    print(f"{label}={'PASS' if condition else 'FAIL'}")
    if not condition:
        raise RuntimeError(label)


def main() -> int:
    repo = Path(__file__).resolve().parents[3]

    display_props = (
        repo
        / "JWPLC"
        / "2.1.0"
        / "libraries"
        / "JWPLC_Display"
        / "library.properties"
    )
    tft_props = (
        repo
        / "JWPLC"
        / "2.1.0"
        / "libraries"
        / "JWPLC_TFT"
        / "library.properties"
    )
    spi_props = (
        repo
        / "JWPLC"
        / "2.1.0"
        / "libraries"
        / "SPI"
        / "library.properties"
    )
    spi_h = (
        repo
        / "JWPLC"
        / "2.1.0"
        / "libraries"
        / "SPI"
        / "src"
        / "SPI.h"
    )
    spi_cpp = (
        repo
        / "JWPLC"
        / "2.1.0"
        / "libraries"
        / "SPI"
        / "src"
        / "SPI.cpp"
    )
    idle_cpp = (
        repo
        / "JWPLC"
        / "2.1.0"
        / "libraries"
        / "JWPLC_Display"
        / "src"
        / "JWPLC_IdleScreen.cpp"
    )
    eth_header = (
        repo
        / "JWPLC"
        / "2.1.0"
        / "libraries"
        / "JWPLC_Ethernet"
        / "src"
        / "JWPLC_W5x00_Ethernet.h"
    )
    udp_cpp = (
        repo
        / "JWPLC"
        / "2.1.0"
        / "libraries"
        / "JWPLC_Ethernet"
        / "src"
        / "EthernetUdp.cpp"
    )
    socket_cpp = (
        repo
        / "JWPLC"
        / "2.1.0"
        / "libraries"
        / "JWPLC_Ethernet"
        / "src"
        / "socket.cpp"
    )
    w5100_h = (
        repo
        / "JWPLC"
        / "2.1.0"
        / "libraries"
        / "JWPLC_Ethernet"
        / "src"
        / "utility"
        / "w5100.h"
    )
    w5100_cpp = (
        repo
        / "JWPLC"
        / "2.1.0"
        / "libraries"
        / "JWPLC_Ethernet"
        / "src"
        / "utility"
        / "w5100.cpp"
    )

    for path in (
        display_props,
        tft_props,
        spi_props,
        spi_h,
        spi_cpp,
        idle_cpp,
        eth_header,
        udp_cpp,
        socket_cpp,
        w5100_h,
        w5100_cpp,
    ):
        require(path.is_file(), f"PACKAGE_FILE_{path.name}")

    display = display_props.read_text(encoding="utf-8")
    tft = tft_props.read_text(encoding="utf-8")
    spi_props_text = spi_props.read_text(encoding="utf-8")
    spi_header = spi_h.read_text(encoding="utf-8")
    spi_impl = spi_cpp.read_text(encoding="utf-8")
    idle = idle_cpp.read_text(encoding="utf-8")
    header = eth_header.read_text(encoding="utf-8")
    udp = udp_cpp.read_text(encoding="utf-8")
    socket = socket_cpp.read_text(encoding="utf-8")
    w5100 = w5100_h.read_text(encoding="utf-8")
    w5100_impl = w5100_cpp.read_text(encoding="utf-8")

    for name, props in (
        ("DISPLAY", display),
        ("TFT", tft),
        ("SPI", spi_props_text),
    ):
        require(
            "precompiled=full" not in props,
            f"{name}_PRECOMPILED_FULL_DISABLED",
        )
        require(
            "dot_a_linkage=true" not in props,
            f"{name}_DOT_A_LINKAGE_DISABLED",
        )

    require(
        "static constexpr int TITLE_BOX_X =" in idle
        and "static constexpr int TITLE_BOX_Y =" in idle
        and "static constexpr int TITLE_BOX_W =" in idle
        and "static constexpr int TITLE_BOX_H =" in idle
        and "static constexpr uint16_t C_TITLE_BG =" in idle
        and "static constexpr uint16_t C_TITLE_TEXT =" in idle
        and "TITLE_BOX_X," in idle
        and "TITLE_BOX_Y," in idle
        and "TITLE_BOX_W," in idle
        and "TITLE_BOX_H," in idle
        and "C_TITLE_BG);" in idle
        and "tft->setTextColor(C_TITLE_TEXT, C_TITLE_BG);" in idle
        and "tft->print(g_title);" in idle,
        "DISPLAY_TFT_TITLE_BACKGROUND_MARKER",
    )

    require(
        "SIR_W5500" in w5100
        and "SIMR_W5500" in w5100
        and "SnIMR" in w5100,
        "ETH_W5500_INT_REGISTERS",
    )

    require(
        "#define JWPLC_W5500_RX_DIRECT_TRANSFER_BYTES 0" in w5100,
        "ETH_W5500_DIRECT_RX_DEFAULT_OFF",
    )
    require(
        "SPI.transferBytes(nullptr, buf, len);" in w5100_impl
        and "JWPLC_W5500_RX_DIRECT_TRANSFER_BYTES" in w5100_impl,
        "ETH_W5500_DIRECT_RX_CANDIDATE_PRESENT",
    )

    require(
        "#define JWPLC_W5500_RX_FIFO_REUSE 1" in w5100,
        "ETH_W5500_FIFO_REUSE_DEFAULT_ON",
    )
    require(
        "SPI.jwplcReadBytesReuseFifo(buf, len);" in w5100_impl
        and "JWPLC_W5500_RX_FIFO_REUSE" in w5100_impl,
        "ETH_W5500_FIFO_REUSE_CANDIDATE_PRESENT",
    )
    require(
        "void jwplcReadBytesReuseFifo(uint8_t *out, uint32_t size);" in spi_header
        and "SPIClass::jwplcReadBytesReuseFifo" in spi_impl
        and "jwplcSpiReadBytesReuseFifoNL" in spi_impl,
        "SPI_JWPLC_FIFO_REUSE_HELPER_PRESENT",
    )
    require(
        "#define JWPLC_SPI_PROFILE_FIFO_REUSE_CHUNKS 0" in spi_header,
        "SPI_JWPLC_CHUNK_PROFILE_DEFAULT_OFF",
    )
    require(
        "JWPLCSpiFifoReuseChunkProfile" in spi_header
        and "jwplcResetReadBytesReuseFifoProfile" in spi_header
        and "jwplcGetReadBytesReuseFifoProfile" in spi_header
        and "jwplcFifoReuseChunkProfile" in spi_impl,
        "SPI_JWPLC_CHUNK_PROFILE_PRESENT",
    )

    require(
        "socketRecvUDPFastDeferred" in header
        and "socketCommitUDPFast" in header
        and "jwplcReadPacketFastDeferred" in header
        and "jwplcCommitRxFast" in header,
        "ETH_FAST_UDP_DECLARATIONS",
    )
    require(
        "EthernetClass::socketRecvUDPFastDeferred" in socket
        and "EthernetClass::socketCommitUDPFast" in socket,
        "ETH_FAST_UDP_BACKEND",
    )
    require(
        "EthernetUDP::jwplcReadPacketFastDeferred" in udp
        and "EthernetUDP::jwplcCommitRxFast" in udp,
        "ETH_FAST_UDP_ADDITIVE_API",
    )

    require(
        "socketRecvTCPFastDeferred" in header
        and "socketCommitTCPFast" in header
        and "jwplcReadTcpFastDeferred" in header
        and "jwplcCommitRxFast" in header,
        "ETH_FAST_TCP_RX_DECLARATIONS",
    )
    require(
        "EthernetClass::socketRecvTCPFastDeferred" in socket
        and "EthernetClass::socketCommitTCPFast" in socket,
        "ETH_FAST_TCP_RX_BACKEND",
    )

    client_cpp = (
        repo
        / "JWPLC"
        / "2.1.0"
        / "libraries"
        / "JWPLC_Ethernet"
        / "src"
        / "EthernetClient.cpp"
    ).read_text(encoding="utf-8")
    require(
        "EthernetClient::jwplcReadTcpFastDeferred" in client_cpp
        and "EthernetClient::jwplcCommitRxFast" in client_cpp,
        "ETH_FAST_TCP_RX_ADDITIVE_API",
    )

    package_text = "\n".join((header, udp, socket, client_cpp))
    require(
        "jwplcDiag" not in package_text,
        "ETH_NO_DIAGNOSTIC_API_PROMOTED",
    )

    # The Arduino-compatible legacy API must remain present.
    require(
        "virtual int parsePacket();" in header
        and "virtual int read(uint8_t *buf, size_t len);" in header,
        "ETH_LEGACY_UDP_API_PRESERVED",
    )

    print("PACKAGE_DEVELOPMENT_MODE=SOURCE_FIRST")
    print("PACKAGE_FAST_UDP_POLICY=ADDITIVE_INTERNAL_LEGACY_PRESERVED")
    print("A14_PACKAGE_PROMOTION_CONTRACT=PASS")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception as exc:
        print(f"A14_PACKAGE_PROMOTION_CONTRACT_EXCEPTION={exc}")
        raise SystemExit(1)
