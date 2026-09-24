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
    header = eth_root / "src" / "utility" / "w5100.h"

    if not header.is_file():
        raise RuntimeError(f"P3G_W5100_H_NOT_FOUND={header}")
    if not sketch.is_file():
        raise RuntimeError(f"P3G_SKETCH_NOT_FOUND={sketch}")

    h = header.read_text(encoding="utf-8")

    h = replace_once(
        h,
        "  __GP_REGISTER8 (IR,     0x0015);    // Interrupt\n"
        "  __GP_REGISTER8 (IMR,    0x0016);    // Interrupt Mask\n",
        "  __GP_REGISTER8 (IR,     0x0015);    // Interrupt\n"
        "  __GP_REGISTER8 (IMR,    0x0016);    // Interrupt Mask\n"
        "  __GP_REGISTER8 (SIR_W5500,  0x0017); // W5500 Socket Interrupt\n"
        "  __GP_REGISTER8 (SIMR_W5500, 0x0018); // W5500 Socket Interrupt Mask\n",
        "P3G_COMMON_INTERRUPT_REGS",
    )

    h = replace_once(
        h,
        "  __SOCKET_REGISTER16(SnRX_WR,    0x002A)        // RX Write Pointer (supported?)\n",
        "  __SOCKET_REGISTER16(SnRX_WR,    0x002A)        // RX Write Pointer (supported?)\n"
        "  __SOCKET_REGISTER8(SnIMR,       0x002C)        // W5500 Socket Interrupt Mask\n",
        "P3G_SOCKET_INTERRUPT_REG",
    )

    header.write_text(h, encoding="utf-8", newline="\n")

    ino = sketch.read_text(encoding="utf-8")

    ino = replace_once(
        ino,
        "static constexpr uint16_t UDP_PORT = 5002;\n",
        "static constexpr uint16_t UDP_PORT = 5002;\n"
        "static constexpr uint8_t ETH_INT_PIN = 15;\n",
        "P3G_INT_PIN",
    )

    ino = replace_once(
        ino,
        "static EthernetUDP udpSocket;\n",
        "class P3GEthernetUDP : public EthernetUDP\n"
        "{\n"
        "public:\n"
        "    uint8_t diagnosticSocketNumber() const\n"
        "    {\n"
        "        return sockindex;\n"
        "    }\n"
        "};\n\n"
        "static P3GEthernetUDP udpSocket;\n",
        "P3G_UDP_SUBCLASS",
    )

    ino = replace_once(
        ino,
        "static uint64_t udpRxActiveHoldBytes = 0;",
        "static uint64_t udpRxActiveHoldBytes = 0;\n"
        "static volatile bool ethIntPending = false;\n"
        "static volatile uint32_t ethIntIsrCount = 0;\n"
        "static bool ethIntConfigured = false;\n"
        "static uint8_t ethIntUdpSocket = MAX_SOCK_NUM;\n"
        "static uint32_t udpRxIntSkipCount = 0;\n"
        "static uint32_t udpRxIntWakeCount = 0;\n"
        "static uint32_t udpRxIntLowFallbackCount = 0;\n",
        "P3G_GLOBALS",
    )

    ino = replace_once(
        ino,
        "static uint32_t lastLoopUs = 0;\n",
        "static void IRAM_ATTR onEthernetInterrupt()\n"
        "{\n"
        "    ethIntPending = true;\n"
        "    ++ethIntIsrCount;\n"
        "}\n\n"
        "static uint32_t lastLoopUs = 0;\n",
        "P3G_ISR",
    )

    ino = replace_once(
        ino,
        "    udpRxActiveHoldBytes = 0;\n"
        "    W5100.jwplcDiagResetReadCounters();",
        "    udpRxActiveHoldBytes = 0;\n"
        "    udpRxIntSkipCount = 0;\n"
        "    udpRxIntWakeCount = 0;\n"
        "    udpRxIntLowFallbackCount = 0;\n"
        "    W5100.jwplcDiagResetReadCounters();",
        "P3G_RESET_COUNTERS",
    )

    ino = replace_once(
        ino,
        '    Serial.print("UDP_RX_BYTES_PER_ACTIVE_HOLD_X1000=");\n'
        "    Serial.println((unsigned long long)udpRxBytesPerActiveHoldX1000);\n",
        '    Serial.print("UDP_RX_BYTES_PER_ACTIVE_HOLD_X1000=");\n'
        "    Serial.println((unsigned long long)udpRxBytesPerActiveHoldX1000);\n"
        '    Serial.print("W5100_DIAG_READ_CALLS_TOTAL=");\n'
        "    Serial.println((uint32_t)W5100.jwplcDiagReadCalls());\n"
        '    Serial.print("W5100_DIAG_READ_BYTES_TOTAL=");\n'
        "    Serial.println((unsigned long long)W5100.jwplcDiagReadBytes());\n"
        '    Serial.print("ETH_INT_CONFIGURED=");\n'
        '    Serial.println(ethIntConfigured ? "YES" : "NO");\n'
        '    Serial.print("ETH_INT_PIN=");\n'
        "    Serial.println(ETH_INT_PIN);\n"
        '    Serial.print("ETH_INT_UDP_SOCKET=");\n'
        "    Serial.println(ethIntUdpSocket);\n"
        '    Serial.print("ETH_INT_ISR_COUNT=");\n'
        "    Serial.println((uint32_t)ethIntIsrCount);\n"
        '    Serial.print("UDP_RX_INT_SKIP_COUNT=");\n'
        "    Serial.println(udpRxIntSkipCount);\n"
        '    Serial.print("UDP_RX_INT_WAKE_COUNT=");\n'
        "    Serial.println(udpRxIntWakeCount);\n"
        '    Serial.print("UDP_RX_INT_LOW_FALLBACK_COUNT=");\n'
        "    Serial.println(udpRxIntLowFallbackCount);\n",
        "P3G_SNAPSHOT_PRINT",
    )

    configure_anchor = """    if (!udpSocket.begin(UDP_PORT))
    {
        return;
    }

    tcpServer.begin();
"""

    configure_block = """    if (!udpSocket.begin(UDP_PORT))
    {
        return;
    }

    ethIntUdpSocket =
        udpSocket.diagnosticSocketNumber();

    if (
        W5100.getChip() != 55 ||
        ethIntUdpSocket >= MAX_SOCK_NUM)
    {
        udpSocket.stop();
        return;
    }

    pinMode(ETH_INT_PIN, INPUT);
    ethIntPending = false;
    ethIntIsrCount = 0;

    W5100.writeSnIR(
        ethIntUdpSocket,
        SnIR::RECV);

    W5100.writeSnIMR(
        ethIntUdpSocket,
        SnIR::RECV);

    W5100.writeSIMR_W5500(
        (uint8_t)(1U << ethIntUdpSocket));

    attachInterrupt(
        digitalPinToInterrupt(ETH_INT_PIN),
        onEthernetInterrupt,
        FALLING);

    ethIntConfigured = true;

    tcpServer.begin();
"""

    ino = replace_once(
        ino,
        configure_anchor,
        configure_block,
        "P3G_CONFIGURE_INTERRUPT",
    )

    service_anchor = """static void serviceUdp()
{
    // ETH14 G1A protected A/B:
    // raw EthernetUDP access must respect the JWPLC
    // shared-SPI ownership policy.
    if (!jwplcSPI_acquire(50))
    {
        ++udpSpiLockErrors;
        return;
    }
"""

    service_block = """static void serviceUdp()
{
    // During UDP_RX, use W5500 INTn as a cheap readiness gate.
    // ISR only sets a flag; all SPI work remains in normal task context.
    if (
        mode == MODE_UDP_RX &&
        ethIntConfigured)
    {
        const bool pinLow =
            digitalRead(ETH_INT_PIN) == LOW;

        if (!ethIntPending && !pinLow)
        {
            ++udpRxIntSkipCount;
            return;
        }

        if (!ethIntPending && pinLow)
        {
            ++udpRxIntLowFallbackCount;
        }

        ethIntPending = false;
        ++udpRxIntWakeCount;
    }

    // ETH14 G1A protected A/B:
    // raw EthernetUDP access must respect the JWPLC
    // shared-SPI ownership policy.
    if (!jwplcSPI_acquire(50))
    {
        ++udpSpiLockErrors;
        return;
    }
"""

    ino = replace_once(
        ino,
        service_anchor,
        service_block,
        "P3G_INT_GATE",
    )

    release_anchor = """    jwplcSPI_release();
}

void setup()
"""

    release_block = """    if (
        ethIntConfigured &&
        ethIntUdpSocket < MAX_SOCK_NUM)
    {
        W5100.writeSnIR(
            ethIntUdpSocket,
            SnIR::RECV);
    }

    jwplcSPI_release();

    if (
        ethIntConfigured &&
        digitalRead(ETH_INT_PIN) == LOW)
    {
        ethIntPending = true;
    }
}

void setup()
"""

    ino = replace_once(
        ino,
        release_anchor,
        release_block,
        "P3G_CLEAR_RECV_INTERRUPT",
    )

    sketch.write_text(ino, encoding="utf-8", newline="\n")

    print("P3G_ETH_INT_PIN=15")
    print("P3G_W5500_RECV_INTERRUPT=ENABLED")
    print("P3G_ISR_SPI_ACCESS=NO")
    print("P3G_UDP_RX_INT_GUIDED_PATCH=PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
