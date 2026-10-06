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
        description="A14 H4A0.3A-R3: add out-of-band freeze without resetting counters"
    )
    parser.add_argument("--instrumented-sketch", required=True)
    args = parser.parse_args()

    path = Path(args.instrumented_sketch).resolve()
    if not path.is_file():
        raise RuntimeError(f"H4A03AR3_SKETCH_NOT_FOUND={path}")

    text = path.read_text(encoding="utf-8")

    anchor = """        else if (c == 'I' || c == 'i')
        {
            mode = MODE_IDLE;
            p3jR2IdleUdpDiscarded = 0;
            resetCounters();

            Serial.println(
                "ETH14_RAW_IDLE=PASS");
        }
        else if (c == 'S' || c == 's')
"""

    replacement = """        else if (c == 'I' || c == 'i')
        {
            mode = MODE_IDLE;
            p3jR2IdleUdpDiscarded = 0;
            resetCounters();

            Serial.println(
                "ETH14_RAW_IDLE=PASS");
        }
        else if (c == 'F' || c == 'f')
        {
            // H4A0.3A-R3: stop productive UDP accounting without touching
            // counters. The following snapshot therefore cannot add RX bytes
            // merely because Serial printing takes time.
            mode = MODE_IDLE;

            Serial.println(
                "ETH14_RAW_FREEZE=PASS");
        }
        else if (c == 'S' || c == 's')
"""

    text = replace_once(
        text,
        anchor,
        replacement,
        "H4A03AR3_FREEZE_SERIAL_COMMAND",
    )

    path.write_text(text, encoding="utf-8", newline="\n")
    verify = path.read_text(encoding="utf-8")

    checks = {
        "FREEZE_ACK": verify.count("ETH14_RAW_FREEZE=PASS") == 1,
        "IDLE_ACK": verify.count("ETH14_RAW_IDLE=PASS") == 1,
        "FREEZE_COMMAND": verify.count("c == 'F' || c == 'f'") == 1,
        "RESET_COUNTERS_TOTAL": verify.count("resetCounters();") >= 3,
    }
    for name, ok in checks.items():
        print(f"H4A03AR3_FREEZE_CONTRACT_{name}={'PASS' if ok else 'FAIL'}")
    failed = [name for name, ok in checks.items() if not ok]
    if failed:
        raise RuntimeError(
            "H4A03AR3_FREEZE_POSTCONDITION_FAILED=" + ",".join(failed)
        )

    freeze_pos = verify.index("c == 'F' || c == 'f'")
    freeze_end = verify.index("else if (c == 'S' || c == 's')", freeze_pos)
    freeze_body = verify[freeze_pos:freeze_end]
    if "resetCounters();" in freeze_body:
        raise RuntimeError("H4A03AR3_FREEZE_MUST_NOT_RESET_COUNTERS")

    print("H4A03AR3_FREEZE_COMMAND=F")
    print("H4A03AR3_FREEZE_RESETS_COUNTERS=NO")
    print("H4A03AR3_FREEZE_MODE_AFTER=IDLE")
    print("H4A03AR3_PRODUCT_SOURCE_MUTATION=NO")
    print("A14_H4A03AR3_FREEZE_PATCH=PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
