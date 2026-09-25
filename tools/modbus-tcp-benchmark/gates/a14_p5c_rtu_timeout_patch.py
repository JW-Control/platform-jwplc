from __future__ import annotations

import argparse
from pathlib import Path


ANCHOR = (
    "static constexpr uint32_t "
    "RTU_TIMEOUT_MS = 15UL;"
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

    count = text.count(ANCHOR)

    if count != 1:
        raise RuntimeError(
            f"P5C_TIMEOUT_ANCHOR_COUNT={count}"
        )

    replacement = (
        "static constexpr uint32_t "
        f"RTU_TIMEOUT_MS = {args.timeout_ms}UL;"
    )

    text = text.replace(
        ANCHOR,
        replacement,
        1,
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
