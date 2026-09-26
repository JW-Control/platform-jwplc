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
        raise RuntimeError(f"P3J_R2_SKETCH_NOT_FOUND={path}")

    text = path.read_text(encoding="utf-8")

    text = replace_once(
        text,
        "static RawBenchMode mode = MODE_IDLE;\n",
        "static RawBenchMode mode = MODE_IDLE;\n"
        "static uint32_t p3jR2IdleUdpDiscarded = 0;\n",
        "P3J_R2_IDLE_COUNTER_GLOBAL",
    )

    text = replace_once(
        text,
        '    Serial.print("RX_BYTES=");\n',
        '    Serial.print("P3J_R2_IDLE_UDP_DISCARDED=");\n'
        "    Serial.println(p3jR2IdleUdpDiscarded);\n"
        '    Serial.print("RX_BYTES=");\n',
        "P3J_R2_IDLE_COUNTER_SNAPSHOT",
    )

    serial_old = """        if (c == 'R' || c == 'r')
        {
            resetCounters();

            Serial.println(
                "ETH14_RAW_RESET=PASS");
        }
        else if (c == 'S' || c == 's')
"""

    serial_new = """        if (c == 'R' || c == 'r')
        {
            resetCounters();

            Serial.println(
                "ETH14_RAW_RESET=PASS");
        }
        else if (c == 'I' || c == 'i')
        {
            mode = MODE_IDLE;
            p3jR2IdleUdpDiscarded = 0;
            resetCounters();

            Serial.println(
                "ETH14_RAW_IDLE=PASS");
        }
        else if (c == 'S' || c == 's')
"""

    text = replace_once(
        text,
        serial_old,
        serial_new,
        "P3J_R2_SERIAL_IDLE",
    )

    idle_anchor = """        if (
            packetEquals(
                udpBuffer,
                totalRead,
                "STOP")
        )
        {
            mode = MODE_IDLE;
            return;
        }

        if (mode == MODE_UDP_RX)
"""

    idle_replacement = """        if (
            packetEquals(
                udpBuffer,
                totalRead,
                "STOP")
        )
        {
            mode = MODE_IDLE;
            return;
        }

        if (mode == MODE_IDLE)
        {
            ++p3jR2IdleUdpDiscarded;
            return;
        }

        if (mode == MODE_UDP_RX)
"""

    text = replace_once(
        text,
        idle_anchor,
        idle_replacement,
        "P3J_R2_IDLE_DISCARD_COUNTER",
    )

    path.write_text(text, encoding="utf-8", newline="\n")

    verify = path.read_text(encoding="utf-8")

    checks = {
        "ACK": verify.count("ETH14_RAW_IDLE=PASS") == 1,
        "MODE": verify.count("mode = MODE_IDLE;") >= 2,
        "SNAPSHOT": verify.count("P3J_R2_IDLE_UDP_DISCARDED=") == 1,
        "DISCARD_PATH": verify.count("++p3jR2IdleUdpDiscarded;") == 1,
    }

    failed = [name for name, ok in checks.items() if not ok]
    if failed:
        raise RuntimeError(
            "P3J_R2_POSTCONDITION_FAILED=" +
            ",".join(failed)
        )

    print("P3J_R2_SERIAL_IDLE_COMMAND=I")
    print("P3J_R2_IDLE_RESETS_COUNTERS=YES")
    print("P3J_R2_IDLE_DISCARD_COUNTER=YES")
    print("P3J_R2_IDLE_QUIESCENCE_OBSERVABLE=YES")
    print("P3J_R2_PRODUCT_SOURCE_MUTATION=NO")
    print("P3J_R2_SERIAL_IDLE_PATCH=PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
