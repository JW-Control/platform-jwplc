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

    old = """        if (c == 'R' || c == 'r')
        {
            resetCounters();

            Serial.println(
                "ETH14_RAW_RESET=PASS");
        }
        else if (c == 'S' || c == 's')
"""

    new = """        if (c == 'R' || c == 'r')
        {
            resetCounters();

            Serial.println(
                "ETH14_RAW_RESET=PASS");
        }
        else if (c == 'I' || c == 'i')
        {
            mode = MODE_IDLE;
            resetCounters();

            Serial.println(
                "ETH14_RAW_IDLE=PASS");
        }
        else if (c == 'S' || c == 's')
"""

    text = replace_once(
        text,
        old,
        new,
        "P3J_R2_SERIAL_IDLE",
    )

    path.write_text(text, encoding="utf-8", newline="\n")

    verify = path.read_text(encoding="utf-8")
    if verify.count("ETH14_RAW_IDLE=PASS") != 1:
        raise RuntimeError("P3J_R2_IDLE_ACK_POSTCONDITION_FAILED")

    if verify.count("mode = MODE_IDLE;") < 2:
        raise RuntimeError("P3J_R2_IDLE_MODE_POSTCONDITION_FAILED")

    print("P3J_R2_SERIAL_IDLE_COMMAND=I")
    print("P3J_R2_IDLE_RESETS_COUNTERS=YES")
    print("P3J_R2_PRODUCT_SOURCE_MUTATION=NO")
    print("P3J_R2_SERIAL_IDLE_PATCH=PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
