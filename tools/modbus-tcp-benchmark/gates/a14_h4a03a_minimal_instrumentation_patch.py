from __future__ import annotations

import argparse
from pathlib import Path


def replace_once(text: str, old: str, new: str, label: str) -> str:
    count = text.count(old)
    if count != 1:
        raise RuntimeError(f"{label}_MATCH_COUNT={count}")
    return text.replace(old, new, 1)


def find_function_span(text: str, signature: str, label: str) -> tuple[int, int]:
    """Return [start,end) for a concrete C/C++ function definition.

    The signature may be a prefix for a multiline declaration. Forward
    declarations are skipped by requiring '(' ... ')' followed by '{' before ';'.
    """
    search_from = 0
    while True:
        start = text.find(signature, search_from)
        if start < 0:
            raise RuntimeError(f"{label}_NOT_FOUND")

        if "(" in signature:
            open_paren = start + signature.find("(")
        else:
            open_paren = text.find("(", start + len(signature))
        if open_paren < 0:
            raise RuntimeError(f"{label}_OPEN_PAREN_NOT_FOUND")

        depth = 0
        close_paren = -1
        i = open_paren
        while i < len(text):
            c = text[i]
            if c == "(":
                depth += 1
            elif c == ")":
                depth -= 1
                if depth == 0:
                    close_paren = i
                    break
            i += 1
        if close_paren < 0:
            raise RuntimeError(f"{label}_CLOSE_PAREN_NOT_FOUND")

        brace = text.find("{", close_paren + 1)
        semi = text.find(";", close_paren + 1)
        if brace >= 0 and (semi < 0 or brace < semi):
            depth = 0
            i = brace
            while i < len(text):
                c = text[i]
                if c == "{":
                    depth += 1
                elif c == "}":
                    depth -= 1
                    if depth == 0:
                        end = i + 1
                        if end < len(text) and text[end] == "\r":
                            end += 1
                        if end < len(text) and text[end] == "\n":
                            end += 1
                        return start, end
                i += 1
            raise RuntimeError(f"{label}_UNBALANCED_BRACES")

        search_from = close_paren + 1


def replace_function(text: str, signature: str, replacement: str, label: str) -> str:
    start, end = find_function_span(text, signature, label)
    return text[:start] + replacement.rstrip() + "\n\n" + text[end:]


def assert_count(text: str, needle: str, expected: int, label: str) -> None:
    actual = text.count(needle)
    print(f"{label}={actual}")
    if actual != expected:
        raise RuntimeError(f"{label}_EXPECTED_{expected}_ACTUAL_{actual}")


def assert_at_least(text: str, needle: str, minimum: int, label: str) -> None:
    actual = text.count(needle)
    print(f"{label}={actual}")
    if actual < minimum:
        raise RuntimeError(f"{label}_MIN_{minimum}_ACTUAL_{actual}")


def main() -> int:
    parser = argparse.ArgumentParser(
        description="A14 H4A0.3A: remove intrusive hot-path diagnostics from proven FAST candidate"
    )
    parser.add_argument("--ethernet-root", required=True)
    parser.add_argument("--instrumented-sketch", required=True)
    args = parser.parse_args()

    eth_root = Path(args.ethernet_root).resolve()
    sketch_path = Path(args.instrumented_sketch).resolve()
    w5100_cpp = eth_root / "src" / "utility" / "w5100.cpp"

    for path in (sketch_path, w5100_cpp):
        if not path.is_file():
            raise RuntimeError(f"H4A03A_FILE_NOT_FOUND={path}")

    ino = sketch_path.read_text(encoding="utf-8")
    cpp = w5100_cpp.read_text(encoding="utf-8")

    # Precondition: this patch is valid only after the exact H4A0.2 FAST chain.
    preconditions = (
        (ino, "static constexpr uint8_t UDP_RX_MAX_PACKETS_PER_HOLD = 2;", 1, "H4A03A_PRE_BATCH2"),
        (ino, "static constexpr uint8_t ETH_INT_PIN = 15;", 1, "H4A03A_PRE_INT"),
        (ino, "udpSocket.jwplcDiagReadPacketFastDeferred(", 1, "H4A03A_PRE_FUSED_DEFERRED"),
        (ino, "udpSocket.jwplcDiagCommitRxFast()", 1, "H4A03A_PRE_COMMIT2"),
        (ino, "P3J-R1: do not clear RECV before draining", 1, "H4A03A_PRE_R1"),
        (ino, "ETH14_RAW_IDLE=PASS", 1, "H4A03A_PRE_SERIAL_IDLE"),
    )
    for text, needle, expected, label in preconditions:
        assert_count(text, needle, expected, label)

    assert_at_least(cpp, "++jwplcDiagReadCallsCounter;", 1, "H4A03A_PRE_W5100_READ_COUNTER")
    assert_at_least(ino, "const uint32_t fastStartUs = micros();", 1, "H4A03A_PRE_FAST_TIMER")
    assert_at_least(ino, "const uint32_t udpRxHoldStartUs =", 1, "H4A03A_PRE_HOLD_TIMER")
    assert_at_least(ino, "const uint32_t tcpSpiHoldStartUs = micros();", 1, "H4A03A_PRE_TCP_TIMER")
    assert_count(ino, "    updateLoopTiming();\n", 1, "H4A03A_PRE_LOOP_PROFILING_CALL")

    # 1) W5100 global read counters: keep dormant diagnostic API so the already
    #    patched isolated library compiles, but remove increments from every read.
    cpp = replace_once(
        cpp,
        "\t++jwplcDiagReadCallsCounter;\n\tjwplcDiagReadBytesCounter += len;\n",
        "",
        "H4A03A_REMOVE_W5100_READ_COUNTER_HOT_PATH",
    )

    # 2) INT ISR: retain only the readiness flag required by the algorithm.
    minimal_isr = """static void IRAM_ATTR onEthernetInterrupt()
{
    ethIntPending = true;
}"""
    ino = replace_function(
        ino,
        "static void IRAM_ATTR onEthernetInterrupt(",
        minimal_isr,
        "H4A03A_ISR_FUNCTION",
    )

    # 3) TCP service still runs while UDP_RX is measured. Preserve shared SPI
    #    ownership and lock-error accounting, remove per-loop micros()/hold stats.
    minimal_tcp = """static void serviceTcp()
{
    if (!jwplcSPI_acquire(50))
    {
        ++tcpSpiLockErrors;
        return;
    }

    jwplcSPI_deselectAll();
    serviceTcpUnlocked();
    jwplcSPI_release();
}"""
    ino = replace_function(
        ino,
        "static void serviceTcp(",
        minimal_tcp,
        "H4A03A_SERVICE_TCP_FUNCTION",
    )

    # 4) Fast UDP branch: only effective byte/packet/error accounting remains.
    service_start, service_end = find_function_span(
        ino,
        "static void serviceUdpUnlocked(",
        "H4A03A_SERVICE_UDP_UNLOCKED_FUNCTION",
    )
    service_text = ino[service_start:service_end]

    branch_start = service_text.find("    if (mode == MODE_UDP_RX)\n    {\n")
    if branch_start < 0:
        raise RuntimeError("H4A03A_FAST_BRANCH_START_NOT_FOUND")

    marker = "    const bool udpRxModeAtEntry =\n"
    legacy_start = service_text.find(marker, branch_start)
    if legacy_start < 0:
        raise RuntimeError("H4A03A_LEGACY_PATH_ANCHOR_NOT_FOUND")

    minimal_fast_branch = """    if (mode == MODE_UDP_RX)
    {
        const int fastRead =
            udpSocket.jwplcDiagReadPacketFastDeferred(
                udpBuffer,
                sizeof(udpBuffer));

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
        return;
    }

"""
    service_text = (
        service_text[:branch_start]
        + minimal_fast_branch
        + service_text[legacy_start:]
    )
    ino = ino[:service_start] + service_text + ino[service_end:]

    # 5) Preserve BATCH2 + INT gate + COMMIT2 + R1 exactly as algorithmic work,
    #    but remove diagnostic counters/timers from serviceUdp().
    minimal_udp = """static void serviceUdp()
{
    if (
        mode == MODE_UDP_RX &&
        ethIntConfigured)
    {
        const bool pinLow =
            digitalRead(ETH_INT_PIN) == LOW;

        if (!ethIntPending && !pinLow)
        {
            return;
        }

        ethIntPending = false;
    }

    if (!jwplcSPI_acquire(50))
    {
        ++udpSpiLockErrors;
        return;
    }

    jwplcSPI_deselectAll();

    // H4A0.3A keeps the proven BATCH2 boundary but removes hold profiling.
    static constexpr uint8_t UDP_RX_MAX_PACKETS_PER_HOLD = 2;
    uint8_t udpRxPacketsThisHold = 0;

    do
    {
        const uint32_t udpRxOperationsBeforeCall =
            rxOperations;

        serviceUdpUnlocked();

        if (
            mode != MODE_UDP_RX ||
            rxOperations == udpRxOperationsBeforeCall)
        {
            break;
        }

        ++udpRxPacketsThisHold;
    }
    while (
        udpRxPacketsThisHold <
        UDP_RX_MAX_PACKETS_PER_HOLD);

    if (mode == MODE_UDP_RX)
    {
        const bool commitOk =
            udpSocket.jwplcDiagCommitRxFast();

        if (!commitOk)
        {
            ++transportErrors;
        }

        // P3J-R1: clear RECV only after RX_RD + Sock_RECV commit, then
        // sample RSR once to cover an overlapping level-triggered event.
        if (
            commitOk &&
            ethIntConfigured &&
            ethIntUdpSocket < MAX_SOCK_NUM)
        {
            W5100.writeSnIR(
                ethIntUdpSocket,
                SnIR::RECV);

            uint16_t postCommitRsr = 0;
            (void)W5100.readSnRX_RSRStable(
                ethIntUdpSocket,
                postCommitRsr);

            if (
                postCommitRsr > 0 ||
                digitalRead(ETH_INT_PIN) == LOW)
            {
                ethIntPending = true;
            }
        }
    }

    jwplcSPI_release();

    if (
        ethIntConfigured &&
        digitalRead(ETH_INT_PIN) == LOW)
    {
        ethIntPending = true;
    }
}"""
    ino = replace_function(
        ino,
        "static void serviceUdp(",
        minimal_udp,
        "H4A03A_SERVICE_UDP_FUNCTION",
    )

    # 6) PERFORMANCE must not simultaneously profile loop latency.
    ino = replace_once(
        ino,
        "    updateLoopTiming();\n\n",
        "",
        "H4A03A_REMOVE_LOOP_PROFILING_CALL",
    )

    sketch_path.write_text(ino, encoding="utf-8", newline="\n")
    w5100_cpp.write_text(cpp, encoding="utf-8", newline="\n")

    # Postconditions: algorithm survives, intrusive hot-path instrumentation does not.
    final_ino = sketch_path.read_text(encoding="utf-8")
    final_cpp = w5100_cpp.read_text(encoding="utf-8")

    required = (
        (final_ino, "static constexpr uint8_t UDP_RX_MAX_PACKETS_PER_HOLD = 2;", 1, "H4A03A_POST_BATCH2"),
        (final_ino, "static constexpr uint8_t ETH_INT_PIN = 15;", 1, "H4A03A_POST_INT"),
        (final_ino, "udpSocket.jwplcDiagReadPacketFastDeferred(", 1, "H4A03A_POST_FUSED_DEFERRED"),
        (final_ino, "udpSocket.jwplcDiagCommitRxFast()", 1, "H4A03A_POST_COMMIT2"),
        (final_ino, "uint16_t postCommitRsr = 0;", 1, "H4A03A_POST_R1_RSR"),
        (final_ino, "ETH14_RAW_IDLE=PASS", 1, "H4A03A_POST_SERIAL_IDLE"),
        (final_ino, "++rxOperations;", 3, "H4A03A_POST_RX_PACKET_COUNTER_TOTAL"),
        (final_ino, "++udpSpiLockErrors;", 1, "H4A03A_POST_UDP_SPI_LOCK_COUNTER"),
        (final_ino, "++tcpSpiLockErrors;", 3, "H4A03A_POST_TCP_SPI_LOCK_COUNTER_TOTAL"),
    )
    for text, needle, expected, label in required:
        assert_count(text, needle, expected, label)

    forbidden = (
        (final_cpp, "++jwplcDiagReadCallsCounter;", "H4A03A_FORBID_W5100_CALL_COUNTER"),
        (final_cpp, "jwplcDiagReadBytesCounter += len;", "H4A03A_FORBID_W5100_BYTE_COUNTER"),
        (final_ino, "const uint32_t fastStartUs = micros();", "H4A03A_FORBID_FAST_TIMER"),
        (final_ino, "const uint32_t udpRxHoldStartUs =", "H4A03A_FORBID_UDP_HOLD_TIMER"),
        (final_ino, "const uint32_t tcpSpiHoldStartUs = micros();", "H4A03A_FORBID_TCP_HOLD_TIMER"),
        (final_ino, "    updateLoopTiming();", "H4A03A_FORBID_LOOP_PROFILING"),
        (final_ino, "++ethIntIsrCount;", "H4A03A_FORBID_ISR_COUNTER"),
        (final_ino, "++udpRxIntSkipCount;", "H4A03A_FORBID_INT_SKIP_COUNTER"),
        (final_ino, "++udpRxIntWakeCount;", "H4A03A_FORBID_INT_WAKE_COUNTER"),
        (final_ino, "++udpRxIntLowFallbackCount;", "H4A03A_FORBID_INT_FALLBACK_COUNTER"),
    )
    for text, needle, label in forbidden:
        assert_count(text, needle, 0, label)

    print("H4A03A_FAST_COMPONENTS=BATCH2+INT+FUSED+COMMIT2+R1+R2")
    print("H4A03A_PERFORMANCE_COUNTERS=RX_BYTES+RX_PACKETS+TRANSPORT_ERRORS+SPI_LOCK_ERRORS")
    print("H4A03A_LOOP_LATENCY_PROFILING=OFF")
    print("H4A03A_W5100_READ_DIAGNOSTICS=OFF")
    print("H4A03A_HOLD_TIMING=OFF")
    print("H4A03A_DETAILED_INT_DIAGNOSTICS=OFF")
    print("H4A03A_PRODUCT_SOURCE_MUTATION=NO")
    print("A14_H4A03A_MINIMAL_INSTRUMENTATION_PATCH=PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
