from __future__ import annotations

import argparse
from pathlib import Path


def replace_once(text: str, old: str, new: str, label: str) -> str:
    count = text.count(old)
    if count != 1:
        raise RuntimeError(f"{label}_MATCH_COUNT={count}")
    return text.replace(old, new, 1)


def main() -> int:
    parser = argparse.ArgumentParser(
        description="A14 H4A0.4-P1: coarse TCP RX stage profiler"
    )
    parser.add_argument("--sketch", required=True)
    args = parser.parse_args()

    path = Path(args.sketch).resolve()
    if not path.is_file():
        raise RuntimeError(f"H4A04P1_PROFILE_SKETCH_NOT_FOUND={path}")

    text = path.read_text(encoding="utf-8")

    globals_anchor = """static uint32_t tcpSpiHoldMaxUs = 0;
static size_t udpLastShortWriteBytes = 0;
"""
    globals_replacement = """static uint32_t tcpSpiHoldMaxUs = 0;

// H4A0.4-P1 coarse TCP RX profiling. Diagnostic-copy only.
static uint32_t tcpProfServiceCalls = 0;
static uint64_t tcpProfLockWaitTotalUs = 0;
static uint32_t tcpProfLockWaitMaxUs = 0;
static uint32_t tcpProfAcceptCalls = 0;
static uint64_t tcpProfAcceptTotalUs = 0;
static uint32_t tcpProfConnectedCalls = 0;
static uint64_t tcpProfConnectedTotalUs = 0;
static uint32_t tcpProfAvailableCalls = 0;
static uint64_t tcpProfAvailableTotalUs = 0;
static uint32_t tcpProfReadCalls = 0;
static uint64_t tcpProfReadTotalUs = 0;
static uint64_t tcpProfReadBytes = 0;
static uint64_t tcpProfUnlockedTotalUs = 0;
static uint32_t tcpProfUnlockedMaxUs = 0;

static size_t udpLastShortWriteBytes = 0;
"""
    text = replace_once(
        text,
        globals_anchor,
        globals_replacement,
        "H4A04P1_PROFILE_GLOBALS",
    )

    reset_anchor = """    tcpSpiHoldTotalUs = 0;
    tcpSpiHoldMaxUs = 0;
    udpLastShortWriteBytes = 0;
"""
    reset_replacement = """    tcpSpiHoldTotalUs = 0;
    tcpSpiHoldMaxUs = 0;

    tcpProfServiceCalls = 0;
    tcpProfLockWaitTotalUs = 0;
    tcpProfLockWaitMaxUs = 0;
    tcpProfAcceptCalls = 0;
    tcpProfAcceptTotalUs = 0;
    tcpProfConnectedCalls = 0;
    tcpProfConnectedTotalUs = 0;
    tcpProfAvailableCalls = 0;
    tcpProfAvailableTotalUs = 0;
    tcpProfReadCalls = 0;
    tcpProfReadTotalUs = 0;
    tcpProfReadBytes = 0;
    tcpProfUnlockedTotalUs = 0;
    tcpProfUnlockedMaxUs = 0;

    udpLastShortWriteBytes = 0;
"""
    text = replace_once(
        text,
        reset_anchor,
        reset_replacement,
        "H4A04P1_PROFILE_RESET",
    )

    snapshot_anchor = """    Serial.print("TCP_SPI_HOLD_MAX_US=");
    Serial.println(tcpSpiHoldMaxUs);

    Serial.print("UDP_LAST_SHORT_WRITE_BYTES=");
"""
    snapshot_replacement = """    Serial.print("TCP_SPI_HOLD_MAX_US=");
    Serial.println(tcpSpiHoldMaxUs);

    Serial.print("TCP_PROF_SERVICE_CALLS=");
    Serial.println(tcpProfServiceCalls);
    Serial.print("TCP_PROF_LOCK_WAIT_TOTAL_US=");
    Serial.println((unsigned long long)tcpProfLockWaitTotalUs);
    Serial.print("TCP_PROF_LOCK_WAIT_MAX_US=");
    Serial.println(tcpProfLockWaitMaxUs);
    Serial.print("TCP_PROF_ACCEPT_CALLS=");
    Serial.println(tcpProfAcceptCalls);
    Serial.print("TCP_PROF_ACCEPT_TOTAL_US=");
    Serial.println((unsigned long long)tcpProfAcceptTotalUs);
    Serial.print("TCP_PROF_CONNECTED_CALLS=");
    Serial.println(tcpProfConnectedCalls);
    Serial.print("TCP_PROF_CONNECTED_TOTAL_US=");
    Serial.println((unsigned long long)tcpProfConnectedTotalUs);
    Serial.print("TCP_PROF_AVAILABLE_CALLS=");
    Serial.println(tcpProfAvailableCalls);
    Serial.print("TCP_PROF_AVAILABLE_TOTAL_US=");
    Serial.println((unsigned long long)tcpProfAvailableTotalUs);
    Serial.print("TCP_PROF_READ_CALLS=");
    Serial.println(tcpProfReadCalls);
    Serial.print("TCP_PROF_READ_TOTAL_US=");
    Serial.println((unsigned long long)tcpProfReadTotalUs);
    Serial.print("TCP_PROF_READ_BYTES=");
    Serial.println((unsigned long long)tcpProfReadBytes);
    Serial.print("TCP_PROF_UNLOCKED_TOTAL_US=");
    Serial.println((unsigned long long)tcpProfUnlockedTotalUs);
    Serial.print("TCP_PROF_UNLOCKED_MAX_US=");
    Serial.println(tcpProfUnlockedMaxUs);

    Serial.print("UDP_LAST_SHORT_WRITE_BYTES=");
"""
    text = replace_once(
        text,
        snapshot_anchor,
        snapshot_replacement,
        "H4A04P1_PROFILE_SNAPSHOT",
    )

    accept_anchor = """static void serviceTcpUnlocked()
{
    acceptTcpClient();

    if (
        !tcpClient ||
        !tcpClient.connected()
    )
"""
    accept_replacement = """static void serviceTcpUnlocked()
{
    const uint32_t tcpProfAcceptStartUs = micros();
    acceptTcpClient();
    const uint32_t tcpProfAcceptUs =
        (uint32_t)(micros() - tcpProfAcceptStartUs);
    ++tcpProfAcceptCalls;
    tcpProfAcceptTotalUs += tcpProfAcceptUs;

    const uint32_t tcpProfConnectedStartUs = micros();
    const bool tcpProfConnected =
        tcpClient &&
        tcpClient.connected();
    const uint32_t tcpProfConnectedUs =
        (uint32_t)(micros() - tcpProfConnectedStartUs);
    ++tcpProfConnectedCalls;
    tcpProfConnectedTotalUs += tcpProfConnectedUs;

    if (
        !tcpProfConnected
    )
"""
    text = replace_once(
        text,
        accept_anchor,
        accept_replacement,
        "H4A04P1_PROFILE_ACCEPT_CONNECTED",
    )

    available_anchor = """            const int availableBytes =
                tcpClient.available();

            if (availableBytes <= 0)
"""
    available_replacement = """            const uint32_t tcpProfAvailableStartUs =
                micros();
            const int availableBytes =
                tcpClient.available();
            const uint32_t tcpProfAvailableUs =
                (uint32_t)(
                    micros() -
                    tcpProfAvailableStartUs);
            ++tcpProfAvailableCalls;
            tcpProfAvailableTotalUs +=
                tcpProfAvailableUs;

            if (availableBytes <= 0)
"""
    text = replace_once(
        text,
        available_anchor,
        available_replacement,
        "H4A04P1_PROFILE_AVAILABLE",
    )

    read_anchor = """            const int got =
                tcpClient.read(
                    tcpBuffer,
                    chunk);

            if (got <= 0)
"""
    read_replacement = """            const uint32_t tcpProfReadStartUs =
                micros();
            const int got =
                tcpClient.read(
                    tcpBuffer,
                    chunk);
            const uint32_t tcpProfReadUs =
                (uint32_t)(
                    micros() -
                    tcpProfReadStartUs);
            ++tcpProfReadCalls;
            tcpProfReadTotalUs += tcpProfReadUs;

            if (got <= 0)
"""
    text = replace_once(
        text,
        read_anchor,
        read_replacement,
        "H4A04P1_PROFILE_READ",
    )

    bytes_anchor = """            rxBytes +=
                (uint32_t)got;

            ++rxOperations;
"""
    bytes_replacement = """            rxBytes +=
                (uint32_t)got;
            tcpProfReadBytes +=
                (uint32_t)got;

            ++rxOperations;
"""
    text = replace_once(
        text,
        bytes_anchor,
        bytes_replacement,
        "H4A04P1_PROFILE_READ_BYTES",
    )

    service_old = """static void serviceTcp()
{
    // Raw EthernetClient/EthernetServer access must respect shared SPI ownership.
    if (!jwplcSPI_acquire(50))
    {
        ++tcpSpiLockErrors;
        return;
    }

    const uint32_t tcpSpiHoldStartUs = micros();
    jwplcSPI_deselectAll();
    serviceTcpUnlocked();
    const uint32_t tcpSpiHoldUs = (uint32_t)(micros() - tcpSpiHoldStartUs);
    ++tcpSpiHoldCount;
    tcpSpiHoldTotalUs += tcpSpiHoldUs;

    if (tcpSpiHoldUs > tcpSpiHoldMaxUs)
    {
        tcpSpiHoldMaxUs = tcpSpiHoldUs;
    }

    jwplcSPI_release();
}
"""

    service_new = """static void serviceTcp()
{
    // Raw EthernetClient/EthernetServer access must respect shared SPI ownership.
    ++tcpProfServiceCalls;
    const uint32_t tcpProfLockStartUs = micros();

    if (!jwplcSPI_acquire(50))
    {
        const uint32_t tcpProfLockWaitUs =
            (uint32_t)(micros() - tcpProfLockStartUs);
        tcpProfLockWaitTotalUs += tcpProfLockWaitUs;
        if (tcpProfLockWaitUs > tcpProfLockWaitMaxUs)
        {
            tcpProfLockWaitMaxUs = tcpProfLockWaitUs;
        }

        ++tcpSpiLockErrors;
        return;
    }

    const uint32_t tcpProfLockWaitUs =
        (uint32_t)(micros() - tcpProfLockStartUs);
    tcpProfLockWaitTotalUs += tcpProfLockWaitUs;
    if (tcpProfLockWaitUs > tcpProfLockWaitMaxUs)
    {
        tcpProfLockWaitMaxUs = tcpProfLockWaitUs;
    }

    const uint32_t tcpSpiHoldStartUs = micros();
    jwplcSPI_deselectAll();

    const uint32_t tcpProfUnlockedStartUs = micros();
    serviceTcpUnlocked();
    const uint32_t tcpProfUnlockedUs =
        (uint32_t)(micros() - tcpProfUnlockedStartUs);

    tcpProfUnlockedTotalUs += tcpProfUnlockedUs;
    if (tcpProfUnlockedUs > tcpProfUnlockedMaxUs)
    {
        tcpProfUnlockedMaxUs = tcpProfUnlockedUs;
    }

    const uint32_t tcpSpiHoldUs =
        (uint32_t)(micros() - tcpSpiHoldStartUs);
    ++tcpSpiHoldCount;
    tcpSpiHoldTotalUs += tcpSpiHoldUs;

    if (tcpSpiHoldUs > tcpSpiHoldMaxUs)
    {
        tcpSpiHoldMaxUs = tcpSpiHoldUs;
    }

    jwplcSPI_release();
}
"""
    text = replace_once(
        text,
        service_old,
        service_new,
        "H4A04P1_PROFILE_SERVICE_TCP",
    )

    path.write_text(text, encoding="utf-8", newline="\n")
    verify = path.read_text(encoding="utf-8")

    required = {
        "SERVICE": "TCP_PROF_SERVICE_CALLS=",
        "LOCK": "TCP_PROF_LOCK_WAIT_TOTAL_US=",
        "ACCEPT": "TCP_PROF_ACCEPT_TOTAL_US=",
        "CONNECTED": "TCP_PROF_CONNECTED_TOTAL_US=",
        "AVAILABLE": "TCP_PROF_AVAILABLE_TOTAL_US=",
        "READ": "TCP_PROF_READ_TOTAL_US=",
        "READ_BYTES": "TCP_PROF_READ_BYTES=",
        "UNLOCKED": "TCP_PROF_UNLOCKED_TOTAL_US=",
    }

    for label, needle in required.items():
        count = verify.count(needle)
        print(f"H4A04P1_PROFILE_CONTRACT_{label}_COUNT={count}")
        if count != 1:
            raise RuntimeError(
                f"H4A04P1_PROFILE_CONTRACT_{label}_INVALID={count}"
            )

    if verify.count("TCP_RX_MAX_CHUNKS_PER_LOCK = 8") != 1:
        raise RuntimeError("H4A04P1_TCP_RX_CHUNK_POLICY_DRIFT")

    print("H4A04P1_TCP_RX_MAX_CHUNKS_PER_LOCK=8")
    print("H4A04P1_PROFILE_SCOPE=RAW_SKETCH_COARSE_ONLY")
    print("H4A04P1_ETHERNET_LIBRARY_MUTATION=NO")
    print("H4A04P1_PRODUCT_SOURCE_MUTATION=NO")
    print("A14_H4A04P1_TCP_COARSE_PROFILE_PATCH=PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
