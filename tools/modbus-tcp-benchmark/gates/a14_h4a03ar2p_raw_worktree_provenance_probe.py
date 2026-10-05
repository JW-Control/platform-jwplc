#!/usr/bin/env python3
from __future__ import annotations

import difflib
import hashlib
import re
import subprocess
from pathlib import Path

RAW = Path("tools/modbus-tcp-benchmark/firmware/eth14_raw_transport_server/eth14_raw_transport_server.ino")
P3K = Path("tools/modbus-tcp-benchmark/gates/a14_p3k_tcp_udp_same_session_parity.ps1")


def run(repo: Path, *args: str) -> tuple[int, bytes, bytes]:
    p = subprocess.run(
        ["git", "-C", str(repo), *args],
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
    )
    return p.returncode, p.stdout, p.stderr


def text(data: bytes) -> str:
    return data.decode("utf-8", errors="replace")


def sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest().upper()


def variants(data: bytes) -> dict[str, str]:
    lf = data.replace(b"\r\n", b"\n").replace(b"\r", b"\n")
    crlf = lf.replace(b"\n", b"\r\n")
    return {
        "RAW": sha256(data),
        "LF": sha256(lf),
        "CRLF": sha256(crlf),
    }


def git_stdout(repo: Path, *args: str) -> bytes:
    code, out, err = run(repo, *args)
    if code != 0:
        raise RuntimeError(
            f"GIT_FAILED {' '.join(args)} :: {text(out + err)[-1200:]}"
        )
    return out


def main() -> int:
    repo = Path(__file__).resolve().parents[3]
    raw_path = repo / RAW
    p3k_path = repo / P3K

    print("=" * 72)
    print(" A14 H4A0.3A-R2P - RAW WORKTREE PROVENANCE PROBE")
    print("=" * 72)

    branch = text(git_stdout(repo, "branch", "--show-current")).strip()
    head = text(git_stdout(repo, "rev-parse", "HEAD")).strip()
    print(f"BRANCH={branch}")
    print(f"HEAD={head}")
    print(f"RAW_PATH={RAW.as_posix()}")

    if not raw_path.is_file() or not p3k_path.is_file():
        raise RuntimeError("R2P_REQUIRED_FILE_MISSING")

    p3k_text = p3k_path.read_text(encoding="utf-8")
    m = re.search(
        r'\$expectedRawHash\s*=\s*"([0-9A-F]{64})"',
        p3k_text,
    )
    if not m:
        raise RuntimeError("R2P_EXPECTED_RAW_HASH_NOT_FOUND")
    expected = m.group(1)
    print(f"P3K_EXPECTED_RAW_SHA256={expected}")

    wt = raw_path.read_bytes()
    head_bytes = git_stdout(repo, "show", f"HEAD:{RAW.as_posix()}")

    wt_v = variants(wt)
    head_v = variants(head_bytes)

    print(f"WORKTREE_RAW_SHA256={wt_v['RAW']}")
    print(f"WORKTREE_LF_SHA256={wt_v['LF']}")
    print(f"WORKTREE_CRLF_SHA256={wt_v['CRLF']}")
    print(f"HEAD_BLOB_RAW_SHA256={head_v['RAW']}")
    print(f"HEAD_BLOB_LF_SHA256={head_v['LF']}")
    print(f"HEAD_BLOB_CRLF_SHA256={head_v['CRLF']}")
    print(f"WORKTREE_MATCHES_P3K_EXPECTED={wt_v['RAW'] == expected}")
    print(
        "HEAD_ANY_REPRESENTATION_MATCHES_P3K_EXPECTED="
        f"{expected in head_v.values()}"
    )

    ls_v = text(git_stdout(repo, "ls-files", "-v", "--", RAW.as_posix())).strip()
    ls_s = text(git_stdout(repo, "ls-files", "-s", "--", RAW.as_posix())).strip()
    ls_debug = text(
        git_stdout(repo, "ls-files", "--debug", "--", RAW.as_posix())
    ).strip()
    attrs = text(
        git_stdout(repo, "check-attr", "-a", "--", RAW.as_posix())
    ).strip()

    print(f"LS_FILES_V={ls_v}")
    print(f"LS_FILES_S={ls_s}")
    print("LS_FILES_DEBUG_BEGIN")
    print(ls_debug)
    print("LS_FILES_DEBUG_END")
    print("CHECK_ATTR_BEGIN")
    print(attrs if attrs else "(none)")
    print("CHECK_ATTR_END")

    diff = text(git_stdout(repo, "diff", "--", RAW.as_posix()))
    cached = text(git_stdout(repo, "diff", "--cached", "--", RAW.as_posix()))
    print(f"GIT_DIFF_VISIBLE={'YES' if diff.strip() else 'NO'}")
    print(f"GIT_DIFF_CACHED_VISIBLE={'YES' if cached.strip() else 'NO'}")

    # Compare semantic LF-normalized text so line-ending conversion cannot
    # masquerade as a source change.
    wt_lines = wt.replace(b"\r\n", b"\n").decode(
        "utf-8", errors="replace"
    ).splitlines()
    head_lines = head_bytes.replace(b"\r\n", b"\n").decode(
        "utf-8", errors="replace"
    ).splitlines()

    same_semantic = wt_lines == head_lines
    print(f"WORKTREE_EQUALS_HEAD_AFTER_EOL_NORMALIZATION={same_semantic}")

    if not same_semantic:
        udiff = list(
            difflib.unified_diff(
                head_lines,
                wt_lines,
                fromfile="HEAD",
                tofile="WORKTREE",
                lineterm="",
                n=3,
            )
        )
        print(f"SEMANTIC_DIFF_LINE_COUNT={len(udiff)}")
        print("SEMANTIC_DIFF_PREVIEW_BEGIN")
        for line in udiff[:120]:
            print(line)
        if len(udiff) > 120:
            print(f"...TRUNCATED_{len(udiff) - 120}_LINES")
        print("SEMANTIC_DIFF_PREVIEW_END")
    else:
        print("SEMANTIC_DIFF_LINE_COUNT=0")

    tag = ls_v[:1] if ls_v else ""
    hidden_candidate = (
        wt_v["RAW"] == expected
        and not same_semantic
        and not diff.strip()
    )

    print(f"LS_FILES_TAG={tag}")
    print(f"HIDDEN_WORKTREE_DIVERGENCE_CANDIDATE={hidden_candidate}")

    if hidden_candidate:
        print("R2P_INTERPRETATION=P3K_RAW_PRESERVED_OUTSIDE_VISIBLE_GIT_DIFF")
    elif wt_v["RAW"] == expected:
        print("R2P_INTERPRETATION=P3K_RAW_PRESENT_IN_CURRENT_WORKTREE")
    else:
        print("R2P_INTERPRETATION=P3K_RAW_NOT_PRESENT_IN_CURRENT_WORKTREE")

    print("PRODUCT_SOURCE_MUTATION=NO")
    print("HARNESS_FAILURE=NO")
    print("PRODUCT_FAILURE=NO_EVIDENCE")
    print("HARDWARE_FAILURE=NO_EVIDENCE")
    print("A14_H4A03AR2P_RAW_PROVENANCE_PROBE=PASS")
    print("NEXT=RETURN_TO_CHAT_DO_NOT_RUN_PHYSICAL_REPLAY")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except SystemExit:
        raise
    except Exception as exc:
        print(f"R2P_EXCEPTION={exc}")
        print("HARNESS_FAILURE=YES")
        print("PRODUCT_FAILURE=NO_EVIDENCE")
        print("HARDWARE_FAILURE=NO_EVIDENCE")
        raise SystemExit(1)
