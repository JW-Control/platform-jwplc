#!/usr/bin/env python3
"""Alpha13 F137: N3 desde los .a REALES del package, sin overlays ni USB.

Ejecutar SOLO tras la adopcion de los dos archives en PR125 Draft.
No modifica el repositorio, variant, core, ni bibliotecas precompiladas.
"""
from __future__ import annotations

import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import time

ROOT = Path(__file__).resolve().parents[3]
PKG = ROOT / "JWPLC/2.1.0"
LIBS = ("JWPLC_Ethernet", "JWPLC_ModbusTCP")
BOARDS = ("jwplcbasic", "jwplcbasiccore")
VENDOR = "jwplc_a13n3"
G10_SKETCH = ROOT / "tools/alpha13/g10_promoted_bench/G10P_COM4_Master"
MATRIX_SCRIPT = ROOT / "tools/alpha13/gates/a13_g10_r_b3_matrix.py"


def require(cond, label):
    if not cond:
        raise RuntimeError("F137_N3_REVIEW_STOP_" + label)


def sha(p):
    return hashlib.sha256(Path(p).read_bytes()).hexdigest()


def cmd(argv, log=None, timeout=1200):
    p = subprocess.run([str(x) for x in argv], cwd=ROOT, capture_output=True,
                       text=True, errors="replace", timeout=timeout)
    output = (p.stdout or "") + "\n" + (p.stderr or "")
    if log is not None:
        log.parent.mkdir(parents=True, exist_ok=True)
        log.write_text(output, encoding="utf-8")
    require(p.returncode == 0, "COMMAND_" + str(argv[0]) + "_" +
            output[-800:].replace("\n", "_"))
    return output


def matrix_sketches():
    spec = importlib.util.spec_from_file_location("a13_f137_legacy_matrix", MATRIX_SCRIPT)
    require(spec is not None and spec.loader is not None, "MATRIX_NOT_LOADABLE")
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    sketches = list(mod.SKETCHES)
    require(len(sketches) == 11 and len(set(sketches)) == 11,
            "MATRIX_COUNT_NOT_11")
    require(all((ROOT / x).is_dir() for x in sketches),
            "MATRIX_DIR_MISSING")
    return sketches


def preflight():
    require(MATRIX_SCRIPT.is_file() and G10_SKETCH.is_dir(), "SKETCH_MISSING")
    for lib in LIBS:
        location = PKG / "libraries" / lib
        prop = (location / "library.properties").read_text(encoding="utf-8")
        a = location / "src/esp32" / ("lib" + lib + ".a")
        require("precompiled=full" in prop and a.is_file() and a.stat().st_size > 0,
                "PRODUCT_ARCHIVE_MISSING_" + lib)
    sketches = matrix_sketches()
    print("F137_N3_N0=PASS_TWO_REAL_ARCHIVES_11_SKETCHES", flush=True)
    return sketches


def verify_selected(log, staged, require_tcp):
    norm = log.replace("\\", "/")
    for lib in LIBS if require_tcp else ("JWPLC_Ethernet",):
        rel = (staged / "libraries" / lib / "src/esp32").as_posix()
        require(("Using library " + lib + " ") in norm,
                "LIBRARY_NOT_SELECTED_" + lib)
        require(rel in norm and ("-l" + lib) in norm,
                "ARCHIVE_NOT_LINKED_" + lib)


def build(output: Path, cli: str):
    sketches = preflight()
    require(not output.exists(), "OUTPUT_ALREADY_EXISTS")
    require(not output.is_relative_to(ROOT), "OUTPUT_INSIDE_WORKTREE")
    output.mkdir(parents=True)
    report = {"status": "IN_PROGRESS", "hardware": "NOT_TOUCHED",
              "sketches": [], "archives": {}, "profile_gate": {}}
    head = cmd(["git", "rev-parse", "HEAD"]).strip()
    report["head"] = head
    with tempfile.TemporaryDirectory(prefix="f137_product_n3_") as t:
        temp = Path(t)
        staged = temp / "sketchbook/hardware" / VENDOR / "esp32"
        staged.parent.mkdir(parents=True)
        shutil.copytree(PKG, staged)
        for lib in LIBS:
            rel = Path("libraries") / lib / "src/esp32" / ("lib" + lib + ".a")
            require(sha(PKG / rel) == sha(staged / rel),
                    "STAGED_PRODUCT_SHA_CHANGED_" + lib)
            report["archives"][lib] = sha(PKG / rel)
        for lib in ("JWPLC_TFT", "JW_FRAM", "JWPLC_ModbusRTU", "JW_SD"):
            original = list((PKG / "libraries" / lib / "src").rglob("*.a"))
            require(bool(original), "FROZEN_ARCHIVE_ABSENT_" + lib)
            for a in original:
                require(sha(a) == sha(staged / a.relative_to(PKG)),
                        "FROZEN_ARCHIVE_DRIFT_" + lib)
        cfg_dir = temp / "config"
        cmd([cli, "config", "init", "--dest-dir", cfg_dir, "--overwrite"])
        cfg = cfg_dir / "arduino-cli.yaml"
        cmd([cli, "--config-file", cfg, "config", "set",
             "directories.user", temp / "sketchbook"])

        def compile_sketch(board, sketch, label, require_tcp):
            fqbn = VENDOR + ":esp32:" + board
            props = cmd([cli, "--config-file", cfg, "compile", "--fqbn",
                         fqbn, "--show-properties", sketch])
            selected = [line for line in props.splitlines()
                        if line.startswith("runtime.platform.path=")]
            require(len(selected) == 1 and
                    Path(selected[0].split("=", 1)[1]).resolve() == staged.resolve(),
                    "WRONG_PACKAGE_" + label)
            bd = temp / ("build_" + label)
            start = time.perf_counter()
            log = cmd([cli, "--config-file", cfg, "compile", "--fqbn", fqbn,
                       "--build-path", bd, "--clean", "-v", sketch],
                      output / (label + ".log"), timeout=1500)
            elapsed = round(time.perf_counter() - start, 3)
            elfs = list(bd.glob("*.elf"))
            require(len(elfs) == 1, "ELF_NOT_UNIQUE_" + label)
            for lib in LIBS:
                o = bd / "libraries" / lib
                require(not list(o.rglob("*.cpp.o")) if o.exists() else True,
                        "SOURCE_RECOMPILED_" + lib + "_" + label)
            if require_tcp:
                verify_selected(log, staged, True)
            report["sketches"].append({
                "profile": board, "sketch": str(sketch.relative_to(ROOT)),
                "elf_sha256": sha(elfs[0]), "seconds": elapsed,
                "tcp_archives_required": require_tcp, "status": "PASS",
            })
            print("F137_N3_SKETCH=" + label + "_PASS", flush=True)
        compile_sketch(BOARDS[0], G10_SKETCH, "basic_dual_eth_tcp", True)
        compile_sketch(BOARDS[1], G10_SKETCH, "core_dual_eth_tcp", True)
        for index, rel in enumerate(sketches):
            p = ROOT / rel
            tcp = "JWPLC_ModbusTCP" in rel
            compile_sketch(BOARDS[0], p, "n3_%02d" % (index + 1), tcp)
        require(len(report["sketches"]) == 13, "NOT_ALL_SKETCHES")
    report["status"] = "PASS_PRODUCT_ARCHIVES_N3_11_PLUS_DUAL_FQBN"
    (output / "MANIFEST.json").write_text(
        json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print("F137_N3=PASS_PRODUCT_ARCHIVES_11_PLUS_DUAL", flush=True)
    print("F137_N3_HW=NO_USB_NO_MAC_NO_UPLOAD", flush=True)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check", action="store_true")
    parser.add_argument("--output", type=Path)
    parser.add_argument("--cli", default="arduino-cli")
    args = parser.parse_args()
    try:
        if args.check:
            preflight()
        else:
            require(args.output is not None, "OUTPUT_REQUIRED")
            build(args.output.resolve(), args.cli)
    except (RuntimeError, OSError, subprocess.TimeoutExpired) as ex:
        print(str(ex), file=sys.stderr)
        sys.exit(2)
