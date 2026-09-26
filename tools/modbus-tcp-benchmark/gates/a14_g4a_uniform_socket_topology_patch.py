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
    parser.add_argument("--ethernet-root", required=True)
    parser.add_argument("--sockets", required=True, type=int, choices=(8, 4, 2))
    args = parser.parse_args()

    root = Path(args.ethernet_root).resolve()
    header = root / "src" / "JWPLC_W5x00_Ethernet.h"

    if not header.is_file():
        raise RuntimeError(f"G4A_HEADER_NOT_FOUND={header}")

    text = header.read_text(encoding="utf-8")

    original_socket_block = """#if defined(RAMEND) && defined(RAMSTART) && ((RAMEND - RAMSTART) <= 2048)
#define MAX_SOCK_NUM 4
#else
#define MAX_SOCK_NUM 8
#endif
"""

    replacement_socket_block = f"""#define MAX_SOCK_NUM {args.sockets}
"""

    text = replace_once(
        text,
        original_socket_block,
        replacement_socket_block,
        "G4A_MAX_SOCK_NUM_BLOCK",
    )

    commented_large = "//#define ETHERNET_LARGE_BUFFERS"
    active_large = "#define ETHERNET_LARGE_BUFFERS"

    if args.sockets == 8:
        if text.count(commented_large) != 1:
            raise RuntimeError(
                f"G4A_COMMENTED_LARGE_BUFFERS_COUNT={text.count(commented_large)}"
            )
        expected_kb = 2
        large_buffers = "NO"
    else:
        text = replace_once(
            text,
            commented_large,
            active_large,
            "G4A_ENABLE_LARGE_BUFFERS",
        )
        expected_kb = 4 if args.sockets == 4 else 8
        large_buffers = "YES"

    header.write_text(text, encoding="utf-8", newline="\n")

    print(f"G4A_MAX_SOCK_NUM={args.sockets}")
    print(f"G4A_ETHERNET_LARGE_BUFFERS={large_buffers}")
    print(f"G4A_EXPECTED_RX_KB_PER_SOCKET={expected_kb}")
    print(f"G4A_EXPECTED_TX_KB_PER_SOCKET={expected_kb}")
    print("G4A_UNIFORM_TOPOLOGY_PATCH=PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
