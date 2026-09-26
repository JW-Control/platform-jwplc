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
    parser.add_argument("--instrumented-sketch", required=True)
    args = parser.parse_args()

    path = Path(args.instrumented_sketch).resolve()
    if not path.is_file():
        raise RuntimeError(f"P3J_R1_SKETCH_NOT_FOUND={path}")

    text = path.read_text(encoding="utf-8")

    clear_before = """    // Clear RECV only after all shared-SPI chip selects are deselected.
    // A datagram received while this hold is active can assert RECV again.
    if (
        mode == MODE_UDP_RX &&
        ethIntConfigured &&
        ethIntUdpSocket < MAX_SOCK_NUM)
    {
        W5100.writeSnIR(
            ethIntUdpSocket,
            SnIR::RECV);
    }

    static constexpr uint8_t UDP_RX_MAX_PACKETS_PER_HOLD = 2;
"""

    clear_deferred = """    // P3J-R1: do not clear RECV before draining a full 2KB batch.
    // With two 1024-byte W5500 UDP records, the socket can be full here.
    // Defer interrupt clear until after RX_RD + Sock_RECV releases space.
    static constexpr uint8_t UDP_RX_MAX_PACKETS_PER_HOLD = 2;
"""

    text = replace_once(
        text,
        clear_before,
        clear_deferred,
        "P3J_R1_DEFER_RECV_CLEAR",
    )

    commit_old = """    if (
        mode == MODE_UDP_RX &&
        !udpSocket.jwplcDiagCommitRxFast())
    {
        ++transportErrors;
    }

    const uint32_t udpRxHoldUs =
"""

    commit_new = """    if (mode == MODE_UDP_RX)
    {
        const bool commitOk =
            udpSocket.jwplcDiagCommitRxFast();

        if (!commitOk)
        {
            ++transportErrors;
        }

        // Clear the latched RECV event only after the hardware RX pointer
        // has been committed. Then sample RSR once to cover any RECV event
        // that overlapped while the bit was already asserted.
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

    const uint32_t udpRxHoldUs =
"""

    text = replace_once(
        text,
        commit_old,
        commit_new,
        "P3J_R1_POST_COMMIT_REARM",
    )

    path.write_text(text, encoding="utf-8", newline="\n")

    verify = path.read_text(encoding="utf-8")

    if verify.count("P3J-R1: do not clear RECV before draining") != 1:
        raise RuntimeError("P3J_R1_DEFER_CLEAR_POSTCONDITION_FAILED")

    if verify.count("postCommitRsr") != 3:
        raise RuntimeError("P3J_R1_RSR_POSTCONDITION_FAILED")

    print("P3J_R1_CLEAR_RECV_TIMING=AFTER_COMMIT")
    print("P3J_R1_POST_COMMIT_RSR_CHECKS_PER_HOLD=1")
    print("P3J_R1_CONTINUOUS_RSR_POLLING=NO")
    print("P3J_R1_INT_REARM_ON_RSR_OR_PIN_LOW=YES")
    print("P3J_R1_PRODUCT_SOURCE_MUTATION=NO")
    print("P3J_R1_POSTCOMMIT_REARM_PATCH=PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
