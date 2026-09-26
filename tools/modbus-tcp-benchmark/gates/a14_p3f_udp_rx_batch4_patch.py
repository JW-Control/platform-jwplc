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
        raise RuntimeError(f"P3F_SKETCH_NOT_FOUND={path}")

    text = path.read_text(encoding="utf-8")

    old = """    jwplcSPI_deselectAll();
    serviceUdpUnlocked();

    const uint32_t udpRxHoldUs =
        (uint32_t)(micros() - udpRxHoldStartUs);
"""

    new = """    jwplcSPI_deselectAll();

    static constexpr uint8_t UDP_RX_MAX_PACKETS_PER_HOLD = 4;
    uint8_t udpRxPacketsThisHold = 0;

    do
    {
        const uint32_t udpRxOperationsBeforeCall =
            rxOperations;

        serviceUdpUnlocked();

        if (
            mode != MODE_UDP_RX ||
            rxOperations == udpRxOperationsBeforeCall
        )
        {
            break;
        }

        ++udpRxPacketsThisHold;
    }
    while (
        udpRxPacketsThisHold <
        UDP_RX_MAX_PACKETS_PER_HOLD);

    const uint32_t udpRxHoldUs =
        (uint32_t)(micros() - udpRxHoldStartUs);
"""

    text = replace_once(
        text,
        old,
        new,
        "P3F_BATCH4_SERVICE_UDP",
    )

    path.write_text(text, encoding="utf-8", newline="\n")

    print("P3F_UDP_RX_MAX_PACKETS_PER_HOLD=4")
    print("P3F_OTHER_ALGORITHM_CHANGE=NO")
    print("P3F_BATCH4_PATCH=PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
