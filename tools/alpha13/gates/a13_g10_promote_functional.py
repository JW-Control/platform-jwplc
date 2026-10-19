#!/usr/bin/env python3
"""Alpha13 G10-P1: promover SOLO los 12 archivos funcionales G10-I.

Se ejecuta exclusivamente en rama aislada de promoción, una sola vez.
Aplica el compositor validado G10-P0 a las fuentes de producto y guarda
manifiesto SHA-256 en español. No altera F1, ni autoload, core, .a, ni pines.
"""
from __future__ import annotations

import hashlib
import json
from pathlib import Path
import subprocess
import sys
from a13_g10_integrated import compose
from a13_g10_p0_source_inventory import build_manifest, F1_BASE, DESTINATIONS, NEW_FILES, UNCHANGED_BY_DESIGN, F1_SOURCE, ROOT

PROMOTION_BRANCH = "codex/alpha13-g10-functional-promotion-20261019"
MANIFEST = Path("docs/v2.1.0-alpha.13/A13_G10_P1_MANIFIESTO_PROMOCION_20261019.json")


def need(ok, name):
    if not ok:
        raise RuntimeError("G10_P1_REVIEW_STOP_" + name)


def run(*args):
    p = subprocess.run(["git", "-C", str(ROOT), *args], capture_output=True, text=True, errors="replace")
    need(p.returncode == 0, "GIT_" + "_".join(args[:2]) + "_" + p.stderr[-300:])
    return p.stdout.strip()


def norm_bytes(path):
    return path.read_bytes().replace(b"\r\n", b"\n")


def digest(data):
    return hashlib.sha256(data).hexdigest()


def main():
    need(Path.cwd().resolve() == ROOT, "CWD_NOT_REPO")
    need(run("branch", "--show-current") == PROMOTION_BRANCH, "WRONG_BRANCH")
    need(not run("status", "--porcelain", "--", "JWPLC/"), "PRODUCT_DIRTY")
    source_before = norm_bytes(ROOT / F1_SOURCE)
    passthrough = {path: norm_bytes(ROOT / path) for path in UNCHANGED_BY_DESIGN}
    need(source_before == subprocess.run(
        ["git", "-C", str(ROOT), "show", F1_BASE + ":" + F1_SOURCE],
        check=True, capture_output=True).stdout, "F1_BASE_SOURCE_DRIFT")
    # El inventario ya contiene asserts sobre fuentes heredadas, 14 rutas,
    # SafeAccess nuevos, y Dns/Dhcp invariantes.
    manifest = build_manifest()
    outputs = compose()
    need(len(outputs) == 14 and set(outputs) == DESTINATIONS, "TARGETS_DRIFT")
    entries = {f["path"]: f for f in manifest["files"]}
    changed = set()
    for rel, text in outputs.items():
        raw = text.encode("utf-8")
        need(digest(raw) == entries[rel]["composed_sha256"], "COMPOSITION_NON_DETERMINISM_" + rel)
        state = entries[rel]["status"]
        need(state in ("ADDED", "MODIFIED", "UNCHANGED"), "UNKNOWN_CLASS")
        dst = ROOT / rel
        if state == "UNCHANGED":
            need(rel in UNCHANGED_BY_DESIGN and dst.exists(), "UNEXPECTED_UNCHANGED_"+rel)
            continue
        if state == "ADDED":
            need(rel in NEW_FILES and not dst.exists(), "UNEXPECTED_ADDITION_" + rel)
        else:
            need(rel not in NEW_FILES and dst.is_file(), "UNEXPECTED_MODIFICATION_" + rel)
            need(norm_bytes(dst) != raw, "EXPECTED_DIFFERENCE_MISSING_" + rel)
        changed.add(rel)
    need(manifest["classification"] == {"MODIFIED": 10, "ADDED": 2, "UNCHANGED": 2}, "EXPECTED_10_2_2")
    need(len(changed) == 12 and F1_SOURCE not in changed, "PROMOTION_SCOPE_DRIFT")
    need(not MANIFEST.is_absolute() and not (ROOT / MANIFEST).exists(), "MANIFEST_ALREADY_EXISTS")
    # First all preconditions, then materialize exactly the approved product delta.
    for rel in sorted(changed):
        target = ROOT / rel
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(outputs[rel].encode("utf-8"))

    need(norm_bytes(ROOT / F1_SOURCE) == source_before, "F1_W5100_CHANGED")
    need(all(norm_bytes(ROOT / path) == before
             for path, before in passthrough.items()), "DNS_DHCP_CHANGED")
    modified = set(run("diff", "--name-only", "--", "JWPLC/").splitlines())
    untracked = set(run("ls-files", "--others", "--exclude-standard", "--", "JWPLC/").splitlines())
    need(modified | untracked == changed, "ACTUAL_PRODUCT_DIFF_DRIFT")
    need(not any(x.endswith(".a") or "bootloader.bin" in x for x in changed), "FROZEN_BINARY_TOUCH")
    output = {
        "schema": "A13-G10-P1-PRODUCT-MATERIALIZATION-1",
        "approval": "H0 cerrado con observación por el propietario; promoción G10 autorizada",
        "phase": "G10-P1-CANDIDATE_NOT_RELEASED",
        "f1_baseline_head": F1_BASE,
        "promoted_from_head": manifest["candidate_head"],
        "source_inventory": "G10-I functional compose() + G10-P0 verified",
        "result": "PASS_10_MODIFIED_2_ADDED_2_UNCHANGED",
        "product_w5100_f1_sha256": digest(norm_bytes(ROOT / F1_SOURCE)),
        "product_changed_paths": sorted(changed),
        "files": manifest["files"],
        "hardware": "NOT_RUN",
        "com14": "NOT_TOUCHED",
        "g10_r_r1_r4": "NOT_AUTHORIZED",
        "n2_n3_product": "NOT_RUN_YET",
    }
    (ROOT / MANIFEST).write_text(json.dumps(output, ensure_ascii=False, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    print("G10_P1_APPLY=PASS_FUNCTIONAL_10_MODIFIED_2_ADDED")
    print("G10_P1_DNS_DHCP=UNCHANGED")
    print("G10_P1_F1_W5100=UNCHANGED")
    print("G10_P1_PRODUCT_CHANGED_PATHS=12")
    print("G10_P1_NO_HARDWARE=YES")


if __name__ == "__main__":
    try:
        main()
    except (RuntimeError, OSError, subprocess.CalledProcessError) as exc:
        print(str(exc), file=sys.stderr)
        sys.exit(2)
