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
        description="A14 H4A0.4-P1: freeze TCP RX accounting without changing mode or counters"
    )
    parser.add_argument("--sketch", required=True)
    args = parser.parse_args()

    path = Path(args.sketch).resolve()
    if not path.is_file():
        raise RuntimeError(f"H4A04P1_SKETCH_NOT_FOUND={path}")

    text = path.read_text(encoding="utf-8")

    text = replace_once(
        text,
        "static RawBenchMode mode = MODE_IDLE;\n",
        "static RawBenchMode mode = MODE_IDLE;\n"
        "static bool h4a04TcpRxFrozen = false;\n",
        "H4A04P1_FREEZE_GLOBAL",
    )

    text = replace_once(
        text,
        "    transportErrors = 0;\n\n",
        "    transportErrors = 0;\n"
        "    h4a04TcpRxFrozen = false;\n\n",
        "H4A04P1_FREEZE_RESET",
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
        else if (c == 'F' || c == 'f')
        {
            h4a04TcpRxFrozen = true;

            Serial.println(
                "ETH14_TCP_RX_FREEZE=PASS");
        }
        else if (c == 'S' || c == 's')
"""

    text = replace_once(
        text,
        serial_old,
        serial_new,
        "H4A04P1_FREEZE_SERIAL",
    )

    mode_anchor = """    if (mode == MODE_IDLE)
    {
"""
    mode_replacement = """    if (
        h4a04TcpRxFrozen &&
        mode == MODE_TCP_RX)
    {
        return;
    }

    if (mode == MODE_IDLE)
    {
"""

    text = replace_once(
        text,
        mode_anchor,
        mode_replacement,
        "H4A04P1_FREEZE_RX_GATE",
    )

    path.write_text(text, encoding="utf-8", newline="\n")
    verify = path.read_text(encoding="utf-8")

    checks = {
        "FLAG": verify.count("static bool h4a04TcpRxFrozen = false;") == 1,
        "ACK": verify.count("ETH14_TCP_RX_FREEZE=PASS") == 1,
        "RX_GATE": verify.count("h4a04TcpRxFrozen &&") == 1,
        "RESET": verify.count("h4a04TcpRxFrozen = false;") == 2,
    }

    for label, ok in checks.items():
        print(
            f"H4A04P1_FREEZE_CONTRACT_{label}="
            f"{'PASS' if ok else 'FAIL'}"
        )

    failed = [label for label, ok in checks.items() if not ok]
    if failed:
        raise RuntimeError(
            "H4A04P1_FREEZE_POSTCONDITION_FAILED="
            + ",".join(failed)
        )

    print("H4A04P1_FREEZE_COMMAND=F")
    print("H4A04P1_FREEZE_MODE_PRESERVED=TCP_RX")
    print("H4A04P1_FREEZE_COUNTERS_PRESERVED=YES")
    print("H4A04P1_PRODUCT_SOURCE_MUTATION=NO")
    print("A14_H4A04P1_TCP_FREEZE_PATCH=PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
