from __future__ import annotations

import argparse
import re
from pathlib import Path


PATTERN = re.compile(
    r"static constexpr uint32_t "
    r"RTU_TIMEOUT_MS = (\d+)UL;"
)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--sketch", required=True)
    parser.add_argument("--timeout-ms", type=int, required=True)
    args = parser.parse_args()

    path = Path(args.sketch).resolve()

    if not path.is_file():
        raise RuntimeError(
            f"P5C_SKETCH_NOT_FOUND={path}"
        )

    if args.timeout_ms < 1 or args.timeout_ms > 1000:
        raise RuntimeError(
            f"P5C_TIMEOUT_INVALID={args.timeout_ms}"
        )

    text = path.read_text(encoding="utf-8")
    matches = list(PATTERN.finditer(text))

    if len(matches) != 1:
        raise RuntimeError(
            "P5C_TIMEOUT_ANCHOR_COUNT="
            f"{len(matches)}"
        )

    previous_timeout = int(
        matches[0].group(1)
    )

    replacement = (
        "static constexpr uint32_t "
        f"RTU_TIMEOUT_MS = {args.timeout_ms}UL;"
    )

    text, count = PATTERN.subn(
        replacement,
        text,
        count=1,
    )

    if count != 1:
        raise RuntimeError(
            "P5C_TIMEOUT_REPLACE_COUNT="
            f"{count}"
        )

    if text.count(replacement) != 1:
        raise RuntimeError(
            "P5C_TIMEOUT_POSTCONDITION_FAILED"
        )

    path.write_text(
        text,
        encoding="utf-8",
        newline="\n",
    )

    print(
        "P5C_PREVIOUS_TIMEOUT_MS="
        f"{previous_timeout}"
    )
    print(
        f"P5C_TIMEOUT_MS={args.timeout_ms}"
    )
    print(
        "P5C_PERIOD_MS=20"
    )
    print(
        "P5C_PRODUCT_SOURCE_MUTATION=NO"
    )
    print(
        "P5C_TIMEOUT_PATCH=PASS"
    )

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
