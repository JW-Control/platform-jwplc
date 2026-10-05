from __future__ import annotations

import argparse
import hashlib
from pathlib import Path
import shutil


RAW_REL = Path("tools/modbus-tcp-benchmark/firmware/eth14_raw_transport_server/eth14_raw_transport_server.ino")
ETH_REL = Path("JWPLC/2.1.0/libraries/JWPLC_Ethernet")
W5100_CPP_REL = ETH_REL / "src/utility/w5100.cpp"
W5100_H_REL = ETH_REL / "src/utility/w5100.h"

EXPECTED = {
    RAW_REL: "9A0C6CF27E44A61DC97686FB1A769EBBBB34D036CDF8D0CA0EEAB597E699AB6B",
    W5100_CPP_REL: "9F94AAC1BB25966C18EDFD6A5C5D5908A9DBD4CF11B6E9BC099E10B28CEF272F",
    W5100_H_REL: "9A833532C44E0CFCD66429A764E8BDBF62A8871838B0355565BC0A63043455FC",
}


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest().upper()


def replace_once(text: str, old: str, new: str, label: str) -> str:
    count = text.count(old)
    if count != 1:
        raise RuntimeError(f"{label}_MATCH_COUNT={count}")
    return text.replace(old, new, 1)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--repo-root", required=True)
    parser.add_argument("--work-root", required=True)
    args = parser.parse_args()

    repo = Path(args.repo_root).resolve()
    work = Path(args.work_root).resolve()

    for rel, expected in EXPECTED.items():
        path = repo / rel
        actual = sha256(path)
        print(f"SOURCE_SHA256={rel.as_posix()}={actual}")
        if actual != expected:
            raise RuntimeError(f"P3_SOURCE_HASH_MISMATCH={rel.as_posix()}")

    if work.exists():
        shutil.rmtree(work)
    work.mkdir(parents=True)

    diag_lib_root = work / "libraries"
    diag_eth = diag_lib_root / "JWPLC_Ethernet"
    shutil.copytree(repo / ETH_REL, diag_eth)

    diag_sketch_dir = work / "sketch" / "eth14_raw_transport_server"
    shutil.copytree((repo / RAW_REL).parent, diag_sketch_dir)

    h_path = diag_eth / "src/utility/w5100.h"
    cpp_path = diag_eth / "src/utility/w5100.cpp"
    ino_path = diag_sketch_dir / "eth14_raw_transport_server.ino"

    h = h_path.read_text(encoding="utf-8")
    h = replace_once(
        h,
        "  static uint8_t getChip(void) { return chip; }\n",
        "  static uint8_t getChip(void) { return chip; }\n"
        "  static void jwplcDiagResetReadCounters(void);\n"
        "  static uint32_t jwplcDiagReadCalls(void);\n"
        "  static uint64_t jwplcDiagReadBytes(void);\n",
        "P3_W5100_H_DIAG_DECL",
    )
    h_path.write_text(h, encoding="utf-8", newline="\n")

    cpp = cpp_path.read_text(encoding="utf-8")
    cpp = replace_once(
        cpp,
        "W5100Class W5100;\n",
        "W5100Class W5100;\n\n"
        "static uint32_t jwplcDiagReadCallsCounter = 0;\n"
        "static uint64_t jwplcDiagReadBytesCounter = 0;\n",
        "P3_W5100_CPP_DIAG_GLOBALS",
    )
    cpp = replace_once(
        cpp,
        "uint16_t W5100Class::read(uint16_t addr, uint8_t *buf, uint16_t len)\n{\n\tuint8_t cmd[4];",
        "uint16_t W5100Class::read(uint16_t addr, uint8_t *buf, uint16_t len)\n{\n"
        "\t++jwplcDiagReadCallsCounter;\n"
        "\tjwplcDiagReadBytesCounter += len;\n"
        "\tuint8_t cmd[4];",
        "P3_W5100_CPP_READ_COUNTER",
    )
    cpp = replace_once(
        cpp,
        "bool W5100Class::readSnTX_FSRStable(\n",
        "void W5100Class::jwplcDiagResetReadCounters(void)\n"
        "{\n"
        "\tjwplcDiagReadCallsCounter = 0;\n"
        "\tjwplcDiagReadBytesCounter = 0;\n"
        "}\n\n"
        "uint32_t W5100Class::jwplcDiagReadCalls(void)\n"
        "{\n"
        "\treturn jwplcDiagReadCallsCounter;\n"
        "}\n\n"
        "uint64_t W5100Class::jwplcDiagReadBytes(void)\n"
        "{\n"
        "\treturn jwplcDiagReadBytesCounter;\n"
        "}\n\n"
        "bool W5100Class::readSnTX_FSRStable(\n",
        "P3_W5100_CPP_DIAG_METHODS",
    )
    cpp_path.write_text(cpp, encoding="utf-8", newline="\n")

    ino = ino_path.read_text(encoding="utf-8")
    ino = replace_once(
        ino,
        "#include <JWPLC_Ethernet.h>\n",
        "#include <JWPLC_Ethernet.h>\n#include <utility/w5100.h>\n",
        "P3_RAW_INCLUDE",
    )

    globals_block = """static size_t udpLastShortWriteBytes = 0;

static uint32_t udpRxPackets = 0;
static uint32_t udpRxParseCalls = 0;
static uint64_t udpRxParseTotalUs = 0;
static uint32_t udpRxParseMaxUs = 0;
static uint32_t udpRxPacketParseCalls = 0;
static uint64_t udpRxPacketParseTotalUs = 0;
static uint32_t udpRxPacketParseMaxUs = 0;
static uint32_t udpRxReadCalls = 0;
static uint64_t udpRxReadTotalUs = 0;
static uint32_t udpRxReadMaxUs = 0;
static uint64_t udpRxSpiReadCalls = 0;
static uint64_t udpRxSpiReadBytes = 0;
static uint32_t udpRxServiceHoldCount = 0;
static uint64_t udpRxServiceHoldTotalUs = 0;
static uint32_t udpRxServiceHoldMaxUs = 0;
static uint32_t udpRxActiveHoldCount = 0;
static uint64_t udpRxActiveHoldTotalUs = 0;
static uint32_t udpRxActiveHoldMaxUs = 0;
static uint64_t udpRxActiveHoldBytes = 0;"""
    ino = replace_once(
        ino,
        "static size_t udpLastShortWriteBytes = 0;",
        globals_block,
        "P3_RAW_GLOBALS",
    )

    reset_block = """    udpLastShortWriteBytes = 0;

    udpRxPackets = 0;
    udpRxParseCalls = 0;
    udpRxParseTotalUs = 0;
    udpRxParseMaxUs = 0;
    udpRxPacketParseCalls = 0;
    udpRxPacketParseTotalUs = 0;
    udpRxPacketParseMaxUs = 0;
    udpRxReadCalls = 0;
    udpRxReadTotalUs = 0;
    udpRxReadMaxUs = 0;
    udpRxSpiReadCalls = 0;
    udpRxSpiReadBytes = 0;
    udpRxServiceHoldCount = 0;
    udpRxServiceHoldTotalUs = 0;
    udpRxServiceHoldMaxUs = 0;
    udpRxActiveHoldCount = 0;
    udpRxActiveHoldTotalUs = 0;
    udpRxActiveHoldMaxUs = 0;
    udpRxActiveHoldBytes = 0;
    W5100.jwplcDiagResetReadCounters();"""
    ino = replace_once(
        ino,
        "    udpLastShortWriteBytes = 0;",
        reset_block,
        "P3_RAW_RESET",
    )

    calc_anchor = """    if (loopGapSamples > 0)
    {
        loopGapAvgUs =
            (uint32_t)(
                loopGapSumUs /
                loopGapSamples);
    }
"""
    calc_block = calc_anchor + """
    const uint32_t udpRxParseAvgUs =
        udpRxParseCalls > 0
            ? (uint32_t)(udpRxParseTotalUs / udpRxParseCalls)
            : 0;
    const uint32_t udpRxPacketParseAvgUs =
        udpRxPacketParseCalls > 0
            ? (uint32_t)(udpRxPacketParseTotalUs / udpRxPacketParseCalls)
            : 0;
    const uint32_t udpRxReadAvgUs =
        udpRxReadCalls > 0
            ? (uint32_t)(udpRxReadTotalUs / udpRxReadCalls)
            : 0;
    const uint32_t udpRxServiceHoldAvgUs =
        udpRxServiceHoldCount > 0
            ? (uint32_t)(udpRxServiceHoldTotalUs / udpRxServiceHoldCount)
            : 0;
    const uint32_t udpRxActiveHoldAvgUs =
        udpRxActiveHoldCount > 0
            ? (uint32_t)(udpRxActiveHoldTotalUs / udpRxActiveHoldCount)
            : 0;
    const uint64_t udpRxSpiReadsPerPacketX1000 =
        udpRxPackets > 0
            ? (udpRxSpiReadCalls * 1000ULL) / udpRxPackets
            : 0;
    const uint64_t udpRxSpiReadBytesPerPacketX1000 =
        udpRxPackets > 0
            ? (udpRxSpiReadBytes * 1000ULL) / udpRxPackets
            : 0;
    const uint64_t udpRxBytesPerActiveHoldX1000 =
        udpRxActiveHoldCount > 0
            ? (udpRxActiveHoldBytes * 1000ULL) / udpRxActiveHoldCount
            : 0;
    const uint32_t udpRxEmptyHoldCount =
        udpRxServiceHoldCount >= udpRxActiveHoldCount
            ? udpRxServiceHoldCount - udpRxActiveHoldCount
            : 0;
"""
    ino = replace_once(
        ino,
        calc_anchor,
        calc_block,
        "P3_RAW_SNAPSHOT_CALCS",
    )

    print_anchor = """    Serial.print("TCP_CONNECTED=");
"""
    print_block = """    Serial.print("UDP_RX_PACKETS=");
    Serial.println(udpRxPackets);
    Serial.print("UDP_RX_PARSE_CALLS=");
    Serial.println(udpRxParseCalls);
    Serial.print("UDP_RX_PARSE_US_AVG=");
    Serial.println(udpRxParseAvgUs);
    Serial.print("UDP_RX_PARSE_US_MAX=");
    Serial.println(udpRxParseMaxUs);
    Serial.print("UDP_RX_PACKET_PARSE_CALLS=");
    Serial.println(udpRxPacketParseCalls);
    Serial.print("UDP_RX_PACKET_PARSE_US_AVG=");
    Serial.println(udpRxPacketParseAvgUs);
    Serial.print("UDP_RX_PACKET_PARSE_US_MAX=");
    Serial.println(udpRxPacketParseMaxUs);
    Serial.print("UDP_RX_READ_CALLS=");
    Serial.println(udpRxReadCalls);
    Serial.print("UDP_RX_READ_US_AVG=");
    Serial.println(udpRxReadAvgUs);
    Serial.print("UDP_RX_READ_US_MAX=");
    Serial.println(udpRxReadMaxUs);
    Serial.print("UDP_RX_SPI_READ_CALLS=");
    Serial.println((unsigned long long)udpRxSpiReadCalls);
    Serial.print("UDP_RX_SPI_READ_BYTES=");
    Serial.println((unsigned long long)udpRxSpiReadBytes);
    Serial.print("UDP_RX_SPI_READS_PER_PACKET_X1000=");
    Serial.println((unsigned long long)udpRxSpiReadsPerPacketX1000);
    Serial.print("UDP_RX_SPI_READ_BYTES_PER_PACKET_X1000=");
    Serial.println((unsigned long long)udpRxSpiReadBytesPerPacketX1000);
    Serial.print("UDP_RX_SERVICE_HOLD_COUNT=");
    Serial.println(udpRxServiceHoldCount);
    Serial.print("UDP_RX_SERVICE_HOLD_US_AVG=");
    Serial.println(udpRxServiceHoldAvgUs);
    Serial.print("UDP_RX_SERVICE_HOLD_US_MAX=");
    Serial.println(udpRxServiceHoldMaxUs);
    Serial.print("UDP_RX_ACTIVE_HOLD_COUNT=");
    Serial.println(udpRxActiveHoldCount);
    Serial.print("UDP_RX_ACTIVE_HOLD_US_AVG=");
    Serial.println(udpRxActiveHoldAvgUs);
    Serial.print("UDP_RX_ACTIVE_HOLD_US_MAX=");
    Serial.println(udpRxActiveHoldMaxUs);
    Serial.print("UDP_RX_EMPTY_HOLD_COUNT=");
    Serial.println(udpRxEmptyHoldCount);
    Serial.print("UDP_RX_BYTES_PER_ACTIVE_HOLD_X1000=");
    Serial.println((unsigned long long)udpRxBytesPerActiveHoldX1000);

""" + print_anchor
    ino = replace_once(
        ino,
        print_anchor,
        print_block,
        "P3_RAW_SNAPSHOT_PRINT",
    )

    parse_anchor = """    const int packetSize =
        udpSocket.parsePacket();
"""
    parse_block = """    const bool udpRxModeAtEntry =
        mode == MODE_UDP_RX;
    const uint32_t udpRxSpiReadCallsAtEntry =
        W5100.jwplcDiagReadCalls();
    const uint64_t udpRxSpiReadBytesAtEntry =
        W5100.jwplcDiagReadBytes();
    const uint32_t udpRxParseStartUs = micros();

    const int packetSize =
        udpSocket.parsePacket();

    const uint32_t udpRxParseUs =
        (uint32_t)(micros() - udpRxParseStartUs);

    if (udpRxModeAtEntry)
    {
        ++udpRxParseCalls;
        udpRxParseTotalUs += udpRxParseUs;

        if (udpRxParseUs > udpRxParseMaxUs)
        {
            udpRxParseMaxUs = udpRxParseUs;
        }
    }
"""
    ino = replace_once(
        ino,
        parse_anchor,
        parse_block,
        "P3_RAW_PARSE_TIMER",
    )

    read_init_anchor = """        int totalRead = 0;
"""
    read_init_block = """        int totalRead = 0;
        uint32_t packetReadCalls = 0;
        uint64_t packetReadTotalUs = 0;
        uint32_t packetReadMaxUs = 0;
"""
    ino = replace_once(
        ino,
        read_init_anchor,
        read_init_block,
        "P3_RAW_READ_LOCALS",
    )

    read_anchor = """            const int got =
                udpSocket.read(
                    udpBuffer + totalRead,
                    sizeof(udpBuffer) -
                        totalRead);
"""
    read_block = """            const uint32_t packetReadStartUs = micros();

            const int got =
                udpSocket.read(
                    udpBuffer + totalRead,
                    sizeof(udpBuffer) -
                        totalRead);

            const uint32_t packetReadUs =
                (uint32_t)(micros() - packetReadStartUs);

            ++packetReadCalls;
            packetReadTotalUs += packetReadUs;

            if (packetReadUs > packetReadMaxUs)
            {
                packetReadMaxUs = packetReadUs;
            }
"""
    ino = replace_once(
        ino,
        read_anchor,
        read_block,
        "P3_RAW_READ_TIMER",
    )

    payload_anchor = """        if (mode == MODE_UDP_RX)
        {
            rxBytes += totalRead;
            ++rxOperations;
        }
"""
    payload_block = """        if (mode == MODE_UDP_RX)
        {
            rxBytes += totalRead;
            ++rxOperations;

            ++udpRxPackets;
            ++udpRxPacketParseCalls;
            udpRxPacketParseTotalUs += udpRxParseUs;

            if (udpRxParseUs > udpRxPacketParseMaxUs)
            {
                udpRxPacketParseMaxUs = udpRxParseUs;
            }

            udpRxReadCalls += packetReadCalls;
            udpRxReadTotalUs += packetReadTotalUs;

            if (packetReadMaxUs > udpRxReadMaxUs)
            {
                udpRxReadMaxUs = packetReadMaxUs;
            }

            const uint32_t udpRxSpiReadCallsAfter =
                W5100.jwplcDiagReadCalls();
            const uint64_t udpRxSpiReadBytesAfter =
                W5100.jwplcDiagReadBytes();

            udpRxSpiReadCalls +=
                (uint32_t)(
                    udpRxSpiReadCallsAfter -
                    udpRxSpiReadCallsAtEntry);

            udpRxSpiReadBytes +=
                (uint64_t)(
                    udpRxSpiReadBytesAfter -
                    udpRxSpiReadBytesAtEntry);
        }
"""
    ino = replace_once(
        ino,
        payload_anchor,
        payload_block,
        "P3_RAW_PACKET_METRICS",
    )

    hold_anchor = """    jwplcSPI_deselectAll();
    serviceUdpUnlocked();
    jwplcSPI_release();
"""
    hold_block = """    const bool udpRxHoldAtEntry =
        mode == MODE_UDP_RX;
    const uint32_t udpRxOperationsBefore =
        rxOperations;
    const uint64_t udpRxBytesBefore =
        rxBytes;
    const uint32_t udpRxHoldStartUs =
        micros();

    jwplcSPI_deselectAll();
    serviceUdpUnlocked();

    const uint32_t udpRxHoldUs =
        (uint32_t)(micros() - udpRxHoldStartUs);

    if (udpRxHoldAtEntry)
    {
        ++udpRxServiceHoldCount;
        udpRxServiceHoldTotalUs += udpRxHoldUs;

        if (udpRxHoldUs > udpRxServiceHoldMaxUs)
        {
            udpRxServiceHoldMaxUs = udpRxHoldUs;
        }

        if (rxOperations > udpRxOperationsBefore)
        {
            ++udpRxActiveHoldCount;
            udpRxActiveHoldTotalUs += udpRxHoldUs;
            udpRxActiveHoldBytes +=
                rxBytes - udpRxBytesBefore;

            if (udpRxHoldUs > udpRxActiveHoldMaxUs)
            {
                udpRxActiveHoldMaxUs = udpRxHoldUs;
            }
        }
    }

    jwplcSPI_release();
"""
    ino = replace_once(
        ino,
        hold_anchor,
        hold_block,
        "P3_RAW_HOLD_TIMER",
    )

    ino_path.write_text(ino, encoding="utf-8", newline="\n")

    print(f"DIAG_LIBRARY_ROOT={diag_lib_root}")
    print(f"DIAG_ETHERNET_ROOT={diag_eth}")
    print(f"DIAG_SKETCH_DIR={diag_sketch_dir}")
    print(f"DIAG_W5100_H_SHA256={sha256(h_path)}")
    print(f"DIAG_W5100_CPP_SHA256={sha256(cpp_path)}")
    print(f"DIAG_RAW_SHA256={sha256(ino_path)}")
    print("A14_P3_DIAGNOSTIC_PATCH=PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
