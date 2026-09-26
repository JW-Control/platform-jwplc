from __future__ import annotations

import argparse
from pathlib import Path


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--instrumented-sketch", required=True)
    args = parser.parse_args()

    path = Path(args.instrumented_sketch).resolve()
    if not path.is_file():
        raise RuntimeError(f"P3C_SKETCH_NOT_FOUND={path}")

    text = path.read_text(encoding="utf-8")

    start_marker = "static void serviceUdp()\n{"
    end_marker = "\n}\n\nvoid setup()"

    start = text.find(start_marker)
    if start < 0:
        raise RuntimeError("P3C_SERVICE_UDP_START_NOT_FOUND")

    end = text.find(end_marker, start)
    if end < 0:
        raise RuntimeError("P3C_SERVICE_UDP_END_NOT_FOUND")

    if text.find(start_marker, start + len(start_marker)) >= 0:
        raise RuntimeError("P3C_SERVICE_UDP_MULTIPLE_DEFINITIONS")

    replacement = r'''static void serviceUdp()
{
    static constexpr uint8_t UDP_RX_MAX_ATTEMPTS_PER_LOOP = 2;

    for (
        uint8_t udpRxAttemptIndex = 0;
        udpRxAttemptIndex < UDP_RX_MAX_ATTEMPTS_PER_LOOP;
        ++udpRxAttemptIndex)
    {
        const bool udpRxModeBeforeAcquire =
            mode == MODE_UDP_RX;

        if (!jwplcSPI_acquire(50))
        {
            ++udpSpiLockErrors;
            return;
        }

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

        if (udpRxModeBeforeAcquire)
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

        if (
            !udpRxModeBeforeAcquire ||
            mode != MODE_UDP_RX ||
            rxOperations == udpRxOperationsBefore)
        {
            break;
        }
    }
}'''

    patched = text[:start] + replacement + text[end + 2:]
    path.write_text(patched, encoding="utf-8", newline="\n")

    print("P3C_UDP_RX_MAX_ATTEMPTS_PER_LOOP=2")
    print("P3C_SPI_REACQUIRE_BETWEEN_ATTEMPTS=YES")
    print("P3C_MAX_PACKETS_PER_LOCK=1")
    print("P3C_OTHER_ALGORITHM_CHANGE=NO")
    print("P3C_SPLIT2_REACQUIRE_PATCH=PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
