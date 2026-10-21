
#!/usr/bin/env python3
"""Alpha13: benchmark local Windows source-first frente a precompilados.

Sólo crea copias temporales del package y compila un consumidor representativo
con ambos perfiles. No hace upload, abre COM, elimina librerías instaladas,
reconstruye archives oficiales ni modifica JWPLC/2.1.0.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import re
import shutil
import statistics
import subprocess
import sys
import tempfile
import time
from datetime import datetime
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
PACKAGE = ROOT / "JWPLC/2.1.0"
SKETCH = ROOT / "tools/alpha13/g10_promoted_bench/G10P_COM4_Master"
BASE = "d86ccdd3fab2352d0a6579e676b07a54662ccfc3"
FROZEN = {
    "JWPLC_Ethernet": "f190bbbbd69861941807a290be653de422e11a898d1a9583d289cc573d38a0f4",
    "JWPLC_ModbusTCP": "d5456307c5e8a4d2c4fcd3252464b96859faaa4df3ece3ca23e3c720f3b25a1d",
}
ALLOWED_READMES = {
    "JWPLC/2.1.0/libraries/JWPLC_Ethernet/README.md",
    "JWPLC/2.1.0/libraries/JWPLC_ModbusTCP/README.md",
}
VENDOR = "jwplc_a13local"
BOARDS = ("jwplcbasic", "jwplcbasiccore")
MODES = ("source", "archive")


def check(condition, name):
    if not condition:
        raise RuntimeError("A13_LOCAL_BENCH_REVIEW_STOP_" + name)


def digest(path):
    h = hashlib.sha256()
    with Path(path).open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            h.update(block)
    return h.hexdigest()


def call(argv, logfile=None, timeout=1800):
    p = subprocess.run([str(x) for x in argv], cwd=ROOT,
                       capture_output=True, text=True, errors="replace",
                       timeout=timeout)
    output = (p.stdout or "") + "\n" + (p.stderr or "")
    if logfile is not None:
        logfile.parent.mkdir(parents=True, exist_ok=True)
        logfile.write_text(output, encoding="utf-8")
    check(p.returncode == 0,
          "COMMAND_FAILED_" + str(argv[0]) + "_" + output[-500:].replace("\n", "_"))
    return output


def preflight(cli):
    head = call(["git", "rev-parse", "HEAD"]).strip()
    tree = call(["git", "rev-parse", "HEAD:JWPLC/2.1.0"]).strip()
    check(call(["git", "rev-parse", BASE + ":JWPLC/2.1.0"]).strip()
          == "cc17716f173509b1e0f3c0df5eb99549014c1d61", "BASELINE_TREE")
    changed = set(filter(None, call(["git", "diff", "--name-only",
                                    BASE, "HEAD", "--", "JWPLC/2.1.0"]).splitlines()))
    check(not changed - ALLOWED_READMES, "FUNCTIONAL_PRODUCT_CHANGED")
    check(not call(["git", "status", "--porcelain", "--untracked-files=no"]).strip(),
          "DIRTY_TRACKED_WORKTREE")
    check(SKETCH.is_dir(), "BENCH_SKETCH_MISSING")
    archives = {}
    for lib, value in FROZEN.items():
        base = PACKAGE / "libraries" / lib
        archive = base / "src/esp32" / ("lib" + lib + ".a")
        check(archive.is_file() and digest(archive) == value,
              "ARCHIVE_SHA_" + lib)
        check("precompiled=full" in (base / "library.properties").read_text(
            encoding="utf-8"), "PROPERTY_NOT_FROZEN_" + lib)
        archives[lib] = value
    cli_version = call([cli, "version"]).strip()
    print("A13_LOCAL_N0=PASS_SOURCE_UNCHANGED_TWO_ARCHIVES", flush=True)
    return {"head": head, "package_tree": tree,
            "baseline_functional_tree":
                "cc17716f173509b1e0f3c0df5eb99549014c1d61",
            "allowed_package_docs": sorted(changed), "archives": archives,
            "arduino_cli_version": cli_version, "hardware": "NOT_TOUCHED"}


def stage(mode, temp, cli):
    root = temp / mode
    user = root / "sketchbook"
    platform = user / "hardware" / VENDOR / "esp32"
    platform.parent.mkdir(parents=True)
    shutil.copytree(PACKAGE, platform)
    if mode == "source":
        for lib in FROZEN:
            d = platform / "libraries" / lib
            archive = d / "src/esp32" / ("lib" + lib + ".a")
            check(archive.is_file(), "SOURCE_STAGE_ARCHIVE_MISSING_" + lib)
            archive.unlink()  # Sólo se borra la copia temporal.
            props = d / "library.properties"
            text = props.read_text(encoding="utf-8")
            lines = [l for l in text.splitlines()
                     if l.strip() != "precompiled=full"]
            check(len(lines) + 1 == len(text.splitlines()), "PRECOMPILE_FLAG_" + lib)
            props.write_text("\n".join(lines) + "\n", encoding="utf-8")
    else:
        for lib, sha in FROZEN.items():
            name = "lib" + lib + ".a"
            check(digest(platform / "libraries" / lib / "src/esp32" / name)
                  == sha, "STAGE_SHA_" + lib)
    cfg_root = root / "config"
    call([cli, "config", "init", "--dest-dir", cfg_root, "--overwrite"])
    cfg = cfg_root / "arduino-cli.yaml"
    cmd = [cli, "--config-file", cfg]
    call(cmd + ["config", "set", "directories.user", user])
    return platform, cmd


def verify_selection(cli_cmd, board, platform):
    fqbn = VENDOR + ":esp32:" + board
    output = call(cli_cmd + ["compile", "--fqbn", fqbn,
                             "--show-properties", SKETCH], timeout=240)
    selected = [x.split("=", 1)[1].strip() for x in output.splitlines()
                if x.startswith("runtime.platform.path=")]
    check(len(selected) == 1 and
          Path(selected[0]).resolve() == platform.resolve(),
          "WRONG_PACKAGE_SELECTION_" + board)
    return fqbn


def compile_pair(cli_cmd, fqbn, mode, board, repetition, temp, output):
    label = mode + "_" + board + "_" + f"{repetition:02d}"
    build = temp / ("build_" + label)
    base = cli_cmd + ["compile", "--fqbn", fqbn, "--build-path", build, "-v", SKETCH]
    start = time.perf_counter()
    cold_log = call(base + ["--clean"], output / (label + "_cold.log"), timeout=1800)
    cold = round(time.perf_counter() - start, 3)
    elf = list(build.glob("*.elf"))
    check(len(elf) == 1, "NO_UNIQUE_ELF_" + label)
    objs = {}
    for lib in FROZEN:
        directory = build / "libraries" / lib
        objs[lib] = len(list(directory.rglob("*.cpp.o"))) if directory.exists() else 0
        if mode == "archive":
            check(objs[lib] == 0, "ARCHIVE_RECOMPILED_" + lib + "_" + label)
            check("-l" + lib in cold_log,
                  "ARCHIVE_NOT_LINKED_" + lib + "_" + label)
        else:
            check(objs[lib] > 0, "SOURCE_NOT_COMPILED_" + lib + "_" + label)
    check(all(("Using library " + lib + " ") in cold_log for lib in FROZEN),
          "MISSING_PACKAGE_LIBRARY_" + label)
    start = time.perf_counter()
    call(base, output / (label + "_warm.log"), timeout=1800)
    warm = round(time.perf_counter() - start, 3)
    print(f"A13_LOCAL={label} COLD={cold:.3f}s WARM={warm:.3f}s "
          f"OBJECTS={objs}", flush=True)
    return {"mode": mode, "board": board, "repetition": repetition,
            "cold_seconds": cold, "warm_seconds": warm,
            "objects": objs, "elf_sha256": digest(elf[0]), "status": "PASS"}


def median_summary(rows):
    stats = {}
    for board in BOARDS:
        rec = {"board": board}
        for mode in MODES:
            cases = [x for x in rows if x["board"] == board and x["mode"] == mode]
            check(len(cases) == 3, "INCOMPLETE_REPETITIONS_" + mode + board)
            rec[mode + "_cold_median_s"] = round(
                statistics.median(x["cold_seconds"] for x in cases), 3)
            rec[mode + "_warm_median_s"] = round(
                statistics.median(x["warm_seconds"] for x in cases), 3)
        s, a = rec["source_cold_median_s"], rec["archive_cold_median_s"]
        rec["cold_savings_s"] = round(s-a, 3)
        rec["cold_savings_percent"] = round(100*(s-a)/s, 2) if s else None
        stats[board] = rec
    return stats


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--check", action="store_true", help="Sólo comprobar fuente")
    p.add_argument("--cli", default="arduino-cli")
    p.add_argument("--output", type=Path,
                   help="Directorio de resultados FUERA del repo; no debe existir")
    args = p.parse_args()
    try:
        meta = preflight(args.cli)
        if args.check:
            print("A13_LOCAL_BENCH_PREFLIGHT=PASS_NO_BUILD", flush=True)
            return 0
        if args.output is None:
            name = "jwplc_alpha13_pc_benchmark_" + datetime.now().strftime(
                "%Y%m%d_%H%M%S")
            destination = Path(tempfile.gettempdir()) / name
        else:
            destination = args.output.resolve()
        check(not destination.exists(), "OUTPUT_ALREADY_EXISTS")
        check(not destination.is_relative_to(ROOT), "OUTPUT_WITHIN_REPOSITORY")
        destination.mkdir(parents=True)
        report = {"schema": "A13-WINDOWS-LOCAL-BENCH-1",
                  "status": "IN_PROGRESS", "host": "LOCAL_OPERATOR_PC",
                  "repetitions": 3, "metrics": {}, "runs": [], **meta}
        try:
            with tempfile.TemporaryDirectory(prefix="jwplc_a13_bench_") as td:
                t = Path(td)
                for mode in MODES:
                    platform, cli_cmd = stage(mode, t, args.cli)
                    for board in BOARDS:
                        fqbn = verify_selection(cli_cmd, board, platform)
                        for i in range(1, 4):
                            report["runs"].append(compile_pair(
                                cli_cmd, fqbn, mode, board, i, t, destination))
            report["metrics"] = median_summary(report["runs"])
            report["status"] = "PASS_LOCAL_PC_THREE_PAIRS_TWO_FQBN"
            print("A13_LOCAL_BENCH=PASS_THREE_PAIRS_BOTH_FQBN", flush=True)
            for board, stats in report["metrics"].items():
                print(board + " " + json.dumps(stats, ensure_ascii=False),
                      flush=True)
            return 0
        except Exception as e:
            report["status"] = "REVIEW_STOP"
            report["error"] = str(e)
            raise
        finally:
            (destination / "MANIFEST.json").write_text(
                json.dumps(report, ensure_ascii=False, indent=2) + "\n",
                encoding="utf-8")
            print("RESULT_ROOT=" + str(destination), flush=True)
    except Exception as e:
        print("A13_LOCAL_BENCH_ERROR=" + str(e), file=sys.stderr, flush=True)
        return 2


if __name__ == "__main__":
    sys.exit(main())
