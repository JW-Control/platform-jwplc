#!/usr/bin/env python3
"""G10-PH0: build aislado de dos sketches con el package PRODUCTIVO del PR #124.

NO hay operaciones serie, esptool ni upload. Todos los archivos generados
quedan en --output, fuera del checkout. Prohibe usar el overlay G10-R.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import time

ROOT = Path(__file__).resolve().parents[3]
PACKAGE = ROOT / "JWPLC/2.1.0"
F1 = "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/utility/w5100.cpp"
PROMOTED_BASE = "0344ff9cbe42e9efa65e7d4caabb60ce15464270"
VENDOR = "jwplc_g10ph0"
FQBN = VENDOR + ":esp32:jwplcbasic"
SKETCHES = (
    ("COM4_MASTER", ROOT / "tools/alpha13/g10_promoted_bench/G10P_COM4_Master"),
    ("COM14_SLAVE", ROOT / "tools/alpha13/g10_promoted_bench/G10P_COM14_Slave"),
)
SOURCE_OBJECTS = (
    ("libraries/JWPLC_Ethernet/src/utility/w5100.cpp", "w5100.cpp.o"),
    ("libraries/JWPLC_Ethernet/src/JWPLC_W5500_SafeAccess.cpp", "JWPLC_W5500_SafeAccess.cpp.o"),
    ("libraries/JWPLC_ModbusTCP/src/JWPLC_ModbusTCP.cpp", "JWPLC_ModbusTCP.cpp.o"),
)


def require(value: bool, code: str) -> None:
    if not value:
        raise RuntimeError("G10P_REVIEW_STOP_" + code)


def command(*args: str, cwd: Path = ROOT, log: Path | None = None) -> str:
    p = subprocess.run([str(a) for a in args], cwd=cwd, capture_output=True,
                       text=True, errors="replace", timeout=900)
    out = (p.stdout or "") + "\n" + (p.stderr or "")
    if log:
        log.write_text("EXIT=" + str(p.returncode) + "\n" + out, encoding="utf-8")
    require(p.returncode == 0, "COMMAND_FAILED_" + str(args[:3]) + "_" + out[-850:])
    return out


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def canonical(path: Path) -> bytes:
    return path.read_bytes().replace(b"\r\n", b"\n")


def prepare(head: str, allow_detached: bool) -> None:
    require(command("git", "rev-parse", "HEAD").strip() == head, "HEAD_CHANGED")
    branch = command("git", "branch", "--show-current").strip()
    require(branch == "codex/alpha13-g10-promote-functional-20261019"
            or (allow_detached and not branch), "WRONG_BRANCH")
    require(not command("git", "status", "--porcelain", "--untracked-files=no").strip(),
            "TRACKED_WORKTREE_DIRTY")
    command("git", "merge-base", "--is-ancestor", PROMOTED_BASE, head)
    # Contrato de promoción: 10 modificados, 2 nuevos y 0 changes fuera.
    command(sys.executable, "-B", "tools/alpha13/gates/a13_g10_promoted_contract.py")
    for label, folder in SKETCHES:
        ino = folder / (folder.name + ".ino")
        require(ino.is_file(), "SKETCH_MISSING_" + label)
        sketch = ino.read_text(encoding="utf-8")
        for forbidden in ("G10RLabDhcpProbe", "LabSocketSnapshot", "g10rH0LinkStage",
                          "gpio_set_level(", "ESP.restart(", "jwplcWriteQ(", "FILE_WRITE"):
            require(forbidden not in sketch, "LAB_OR_WRITE_IN_" + label)
    require(b"#define PIN_SPI_SS_ETHERNET_LIB JWPLC_ETH_CS" in
            canonical(ROOT / F1), "F1_CS_NOT_PRESENT")
    print("G10P_N0=PASS_REAL_PACKAGE_BOTH_SKETCHES_NO_OVERLAY", flush=True)


def verify_objects(build: Path, stage_hw: Path, log: str) -> list[dict]:
    listed = []
    for rel, name in SOURCE_OBJECTS:
        source = stage_hw / rel
        obj = list(build.rglob(name))
        require(len(obj) == 1, "OBJECT_NOT_UNIQUE_" + name + "_" + str(obj))
        dep_candidates = [obj[0].with_suffix(".d"), Path(str(obj[0]) + ".d")]
        deps = [x for x in dep_candidates if x.is_file()]
        require(bool(deps), "OBJECT_DEP_MISSING_" + name)
        dep_text = deps[0].read_text(encoding="utf-8", errors="replace")
        normalized = dep_text.replace("\\\n", "").replace("\\ ", " ").replace("\\", "/").lower()
        source_suffix = ("jwplc_g10ph0/esp32/" + rel).replace("\\", "/").lower()
        # La salida Windows puede usar path corto de carpetas temporales.
        require(name[:-2].lower() in normalized and
                rel.replace("\\", "/").lower() in normalized,
                "OBJECT_SOURCE_DEP_MISMATCH_" + name)
        require(source.is_file(), "STAGED_SOURCE_MISSING_" + rel)
        listed.append({"relative_source": rel, "source_sha256": sha(source),
                       "object_sha256": sha(obj[0]), "dependency": deps[0].name})
    for lib in ("JWPLC_Ethernet", "JWPLC_ModbusTCP", "JWPLC_ModbusRTU"):
        require("Using library " + lib + " " in log, "LIBRARY_NOT_SELECTED_" + lib)
        # No pasar por bibliotecas instaladas en Arduino15.
        matches = [l for l in log.splitlines() if l.startswith("Using library " + lib + " ")]
        require(len(matches) == 1 and
                str(stage_hw).replace("\\", "/").lower() in
                matches[0].replace("\\", "/").lower(),
                "LIBRARY_OUTSIDE_STAGE_" + lib)
    return listed


def execute(out: Path, head: str, allow_detached: bool, cli: str) -> None:
    prepare(head, allow_detached)
    require(not out.is_relative_to(ROOT), "OUTPUT_INSIDE_GIT_CHECKOUT")
    require(not out.exists(), "OUTPUT_ALREADY_EXISTS")
    out.mkdir(parents=True)
    book = out / "sketchbook"
    hw = book / "hardware" / VENDOR / "esp32"
    hw.parent.mkdir(parents=True)
    shutil.copytree(PACKAGE, hw)
    # Copia idéntica: F1 y cada archive precompilado.
    assert canonical(PACKAGE / "libraries/JWPLC_Ethernet/src/utility/w5100.cpp") == \
           canonical(hw / "libraries/JWPLC_Ethernet/src/utility/w5100.cpp")
    frozen = sorted(PACKAGE.rglob("*.a"))
    require(len(frozen) >= 5, "FROZEN_ARCHIVES_MISSING")
    for p in frozen:
        require(sha(p) == sha(hw / p.relative_to(PACKAGE)), "ARCHIVE_COPY_DRIFT")
    cfgdir = out / "config"
    cfgdir.mkdir()
    command(cli, "config", "init", "--dest-dir", str(cfgdir), "--overwrite")
    cfg = cfgdir / "arduino-cli.yaml"
    command(cli, "--config-file", str(cfg), "config", "set", "directories.user", str(book))
    manifest = {"head": head, "platform_vendor": VENDOR, "fqbn": FQBN,
                "f1_sha256": sha(hw / "libraries/JWPLC_Ethernet/src/utility/w5100.cpp"),
                "frozen_archives": {str(x.relative_to(PACKAGE)): sha(x) for x in frozen},
                "boards": {}, "upload": "NOT_RUN", "com4": "NOT_OPENED", "com14": "NOT_OPENED"}
    for label, sketch in SKETCHES:
        props = command(cli, "--config-file", str(cfg), "compile", "--fqbn", FQBN,
                        "--show-properties", str(sketch),
                        log=out / ("props_" + label + ".log"))
        selections = [l.split("=", 1)[1].strip().strip('"')
                      for l in props.splitlines() if l.startswith("runtime.platform.path=")]
        require(len(selections) == 1, "PLATFORM_PROPERTY_" + label)
        chosen = Path(selections[0])
        require(chosen.exists() and os.path.samefile(chosen, hw),
                "WRONG_ARDUINO_PLATFORM_" + label + "_" + selections[0])
        build = out / ("build_" + label)
        log = command(cli, "--config-file", str(cfg), "compile", "--fqbn", FQBN,
                      "--warnings", "default", "--build-path", str(build), "--clean", "-v",
                      str(sketch), log=out / ("compile_" + label + ".log"))
        elf = list(build.glob("*.elf"))
        require(len(elf) == 1, "ELF_NOT_UNIQUE_" + label)
        objects = verify_objects(build, hw, log)
        manifest["boards"][label] = {
            "status": "PASS_SOURCE_TO_OBJECT_ELF", "elf_sha256": sha(elf[0]),
            "source_objects": objects, "sketch": str(sketch.relative_to(ROOT))}
        print("G10P_BUILD_" + label + "=PASS_SOURCE_TO_OBJECT_ELF", flush=True)
    manifest["status"] = "PASS_BOTH_BOARDS_REAL_G10_PACKAGE"
    (out / "MANIFEST.json").write_text(
        json.dumps(manifest, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print("G10P_PLATFORM=PASS_ISOLATED_VENDOR_" + VENDOR, flush=True)
    print("G10P_BUILD=PASS_BOTH_BOARDS", flush=True)
    print("G10P_UPLOAD=NOT_RUN COM4=NOT_OPENED COM14=NOT_OPENED", flush=True)
    print("G10P_EVIDENCE=" + str(out / "MANIFEST.json"), flush=True)


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--expected-head", required=True)
    parser.add_argument("--output", type=Path)
    parser.add_argument("--cli", default="arduino-cli")
    parser.add_argument("--ci-detached", action="store_true")
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()
    try:
        if args.check:
            prepare(args.expected_head, args.ci_detached)
            print("G10P_PREFLIGHT=PASS_NO_HARDWARE", flush=True)
        else:
            require(args.output is not None, "OUTPUT_REQUIRED")
            execute(args.output.resolve(), args.expected_head, args.ci_detached, args.cli)
    except (RuntimeError, OSError, subprocess.TimeoutExpired, ValueError) as ex:
        print(str(ex), file=sys.stderr)
        sys.exit(2)
