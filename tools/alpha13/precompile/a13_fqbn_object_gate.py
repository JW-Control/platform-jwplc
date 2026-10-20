#!/usr/bin/env python3
"""Alpha13: contrastar cada objeto compilado en Basic frente a Basic Core.

Gate negativo reproducible: --expect-mismatch solo es PASS si se observa
al menos un objeto distinto; no aprueba ningun archive ni el producto.
Gate de promocion: --require-match exige identidad binaria fuente->objeto
en ambas configuraciones antes de permitir un unico .a compartido.
Sin puertos COM, USB, upload, acceso fisico ni cambios en el worktree.
"""
from __future__ import annotations

import argparse
import json
from pathlib import Path
import subprocess
import sys

from a13_precompile_benchmark import (
    LIBS, PROFILES, PLATFORM, ROOT, compile_one, digest, git, n0,
    object_manifest, properties, require, stage,
)


def objects_by_source(hw: Path, build: Path) -> dict[str, dict[str, dict]]:
    result = {}
    for lib in LIBS:
        result[lib] = {
            row["src"]: row for row in object_manifest(lib, hw, build)
        }
    return result


def perform(output: Path, cli: str, detached: bool, expect_mismatch: bool) -> None:
    sha = n0(detached)
    require(not output.exists(), "OUTPUT_EXISTS")
    require(not output.is_relative_to(ROOT), "OUTPUT_INSIDE_WORKTREE")
    output.mkdir(parents=True)
    hw, cfg = stage(PLATFORM, output / "source", cli)
    a, b = PROFILES
    prop_a = properties(cli, cfg, a, hw)
    prop_b = properties(cli, cfg, b, hw)
    build_a = output / "build_basic"
    build_b = output / "build_basiccore"
    elapsed_a, _ = compile_one(cli, cfg, a, build_a,
                               output / "source_basic.log", True)
    elapsed_b, _ = compile_one(cli, cfg, b, build_b,
                               output / "source_basiccore.log", True)
    objects_a = objects_by_source(hw, build_a)
    objects_b = objects_by_source(hw, build_b)
    report = {
        "head_sha": sha,
        "profiles": list(PROFILES),
        "seconds": {a: elapsed_a, b: elapsed_b},
        "mode": "EXPECT_MISMATCH" if expect_mismatch else "REQUIRE_MATCH",
        "libs": {},
        "status": "IN_PROGRESS",
        "hardware": "NOT_TOUCHED",
    }
    changed_total = 0
    for lib in LIBS:
        keys_a = set(objects_a[lib])
        keys_b = set(objects_b[lib])
        require(keys_a == keys_b, "SOURCES_DIFFER_" + lib)
        changed = []
        for path in sorted(keys_a):
            x, y = objects_a[lib][path], objects_b[lib][path]
            require(x["src_sha256"] == y["src_sha256"],
                    "SOURCE_NOT_IDENTICAL_" + path)
            identical = x["object_sha256"] == y["object_sha256"]
            changed.append({
                "source": path,
                "source_sha256": x["src_sha256"],
                a: x["object_sha256"],
                b: y["object_sha256"],
                "same_object_sha256": identical,
            })
            if not identical:
                changed_total += 1
        report["libs"][lib] = changed

    report["changed_objects"] = changed_total
    report["same_toolchain_prefix"] = (
        prop_a.get("compiler.prefix") == prop_b.get("compiler.prefix")
    )
    report["same_cpp_flags"] = (
        prop_a.get("compiler.cpp.flags") == prop_b.get("compiler.cpp.flags")
    )
    require(report["same_toolchain_prefix"], "TOOLCHAIN_DIFFER")
    passed = (changed_total > 0) if expect_mismatch else (changed_total == 0)
    report["status"] = ("PASS_EXPECTED_PROFILE_MISMATCH" if passed and expect_mismatch
                        else "PASS_OBJECT_PARITY" if passed
                        else "REVIEW_PROFILE_OBJECTS_DIFFER" if changed_total
                        else "REVIEW_NEGATIVE_DID_NOT_REPRODUCE")
    (output / "FQBN_OBJECT_MANIFEST.json").write_text(
        json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8"
    )
    for lib, records in report["libs"].items():
        bad = [x["source"] for x in records if not x["same_object_sha256"]]
        print("A13_FQBN_OBJECTS_" + lib + "=" +
              str(len(bad)) + "/" + str(len(records)) + "_DIFFER", flush=True)
        for path in bad:
            print("A13_FQBN_DIFFER=" + path, flush=True)
    print("A13_FQBN_NEGATIVE_OR_PARITY=" + report["status"], flush=True)
    print("A13_FQBN_MANIFEST=" + str(output / "FQBN_OBJECT_MANIFEST.json"),
          flush=True)
    require(passed, report["status"])


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    mutex = parser.add_mutually_exclusive_group(required=True)
    mutex.add_argument("--expect-mismatch", action="store_true")
    mutex.add_argument("--require-match", action="store_true")
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--cli", default="arduino-cli")
    parser.add_argument("--ci-detached", action="store_true")
    args = parser.parse_args()
    try:
        perform(args.output.resolve(), args.cli, args.ci_detached,
                args.expect_mismatch)
    except (RuntimeError, OSError, ValueError, subprocess.TimeoutExpired) as exc:
        print(str(exc), file=sys.stderr)
        sys.exit(2)
