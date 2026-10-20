#!/usr/bin/env python3
"""Alpha13: generar, medir y verificar dos archives Arduino sin tocar hardware.

- Copia el package exacto al TEMP y compila desde fuentes reales Arduino CLI.
- Genera .a desde TODOS los .o compilados por el toolchain elegido.
- Verifica selección de librerías precompiled=full, sin compilar .cpp de nuevo.
- Comprueba ambos perfiles de placa y compara tiempos fríos/calientes.
- Solo --adopt copia archives YA VALIDADOS al worktree, nunca hace commit.
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
PLATFORM = ROOT / "JWPLC/2.1.0"
BRANCH = "codex/alpha13-precompile-benchmark-docs-20261020"
SOURCE_BASE = "a8b6f83b39ce950c0da0d2911b5b001be8e77754"
VENDOR = "jwplc_a13archive"
PROFILES = ("jwplcbasic", "jwplcbasiccore")
LIBS = ("JWPLC_Ethernet", "JWPLC_ModbusTCP")
SKETCH = ROOT / "tools/alpha13/g10_promoted_bench/G10P_COM4_Master"
PRJ = "G10P_COM4_Master.ino"
BUNDLE = ("precompiled=full",)
CHANGED_PATHS = tuple(
    str(Path("JWPLC/2.1.0/libraries") / lib / path)
    for lib in LIBS
    for path in ("library.properties", f"src/esp32/lib{lib}.a")
)

def require(ok: bool, reason: str) -> None:
    if not ok:
        raise RuntimeError("A13_ARCHIVE_REVIEW_STOP_" + reason)

def digest(p: Path) -> str:
    return hashlib.sha256(p.read_bytes()).hexdigest()

def run(argv: list[str], *, log: Path | None = None, timeout: int = 900) -> str:
    p = subprocess.run(argv, capture_output=True, text=True, errors="replace", timeout=timeout)
    out = (p.stdout or "") + "\n" + (p.stderr or "")
    if log:
        log.parent.mkdir(parents=True, exist_ok=True)
        log.write_text("EXIT=" + str(p.returncode) + "\n" + out, encoding="utf-8")
    require(p.returncode == 0, "COMMAND_FAILED_" + argv[0] + "_" + out[-650:].replace("\n", "_"))
    return out

def git(*args: str) -> str:
    return run(["git", "-C", str(ROOT), *args], timeout=60).strip()

def n0(detached: bool) -> str:
    head = git("rev-parse", "HEAD")
    branch = git("branch", "--show-current")
    require(branch == BRANCH or (detached and not branch), "WRONG_BRANCH")
    require(not git("status", "--porcelain", "--untracked-files=no"), "DIRTY_TRACKED")
    git("merge-base", "--is-ancestor", SOURCE_BASE, "HEAD")
    for lib in LIBS:
        root = PLATFORM / "libraries" / lib
        props = (root / "library.properties").read_text(encoding="utf-8")
        require("precompiled=full" not in props, "ALREADY_PRECOMPILED_" + lib)
        require(not list((root / "src").rglob("*.a")), "ARCHIVE_ALREADY_EXISTS_" + lib)
        require(len(list((root / "src").rglob("*.cpp"))) >= 2, "LIB_CPP_MISSING_" + lib)
    for name in ("JWPLC_TFT", "JW_FRAM", "JWPLC_ModbusRTU", "JW_SD"):
        require(bool(list((PLATFORM / "libraries" / name / "src").rglob("*.a"))),
                "EXISTING_ARCHIVE_MISSING_" + name)
    require(SKETCH.is_file() if SKETCH.suffix == ".ino" else
            (SKETCH / PRJ).is_file(), "SKETCH_MISSING")
    print("A13_ARCHIVE_N0=PASS_SOURCE_ONLY_TWO_LIBS_OTHERS_FROZEN", flush=True)
    return head

def stage(platform_root: Path, out: Path, cli: str) -> tuple[Path, Path]:
    hw = out / "sketchbook/hardware" / VENDOR / "esp32"
    hw.parent.mkdir(parents=True)
    shutil.copytree(platform_root, hw)
    cfg_dir = out / "config"
    run([cli, "config", "init", "--dest-dir", str(cfg_dir), "--overwrite"])
    cfg = cfg_dir / "arduino-cli.yaml"
    run([cli, "--config-file", str(cfg), "config", "set",
         "directories.user", str(out / "sketchbook")])
    return hw, cfg

def parse_props(log: str) -> dict[str, str]:
    props: dict[str, str] = {}
    for line in log.splitlines():
        if "=" in line:
            k, v = line.split("=", 1)
            props[k] = v.strip().strip('"')
    return props

def properties(cli: str, cfg: Path, board: str, hw: Path) -> dict[str, str]:
    props = parse_props(run([cli, "--config-file", str(cfg), "compile",
                            "--fqbn", f"{VENDOR}:esp32:{board}", "--show-properties",
                            str(SKETCH)]))
    selected = props.get("runtime.platform.path", "")
    require(selected and os.path.samefile(Path(selected), hw),
            "WRONG_PLATFORM_" + board)
    return props

def compile_one(cli: str, cfg: Path, board: str, build: Path,
                logfile: Path, clean: bool) -> tuple[float, str]:
    args = [cli, "--config-file", str(cfg), "compile", "--fqbn",
            f"{VENDOR}:esp32:{board}", "--warnings", "default",
            "--build-path", str(build)]
    if clean:
        args.append("--clean")
    args += ["-v", str(SKETCH)]
    start = time.perf_counter()
    log = run(args, log=logfile, timeout=1500)
    elapsed = round(time.perf_counter() - start, 3)
    require(len(list(build.glob("*.elf"))) == 1, "ELF_MISSING_" + board)
    for lib in LIBS:
        rows = [x for x in log.splitlines()
                if x.startswith("Using library " + lib + " ")]
        require(len(rows) == 1 and "/sketchbook/hardware/" in
                rows[0].replace("\\", "/"), "SELECTED_LIBRARY_" + lib)
    return elapsed, log

def object_manifest(lib: str, root: Path, build: Path) -> list[dict]:
    paths = sorted((root / "libraries" / lib / "src").rglob("*.cpp"))
    paths += sorted((root / "libraries" / lib / "src").rglob("*.c"))
    libbuild = build / "libraries" / lib
    require(libbuild.is_dir(), "LIB_BUILD_DIR_MISSING_" + lib)
    objects = sorted(libbuild.rglob("*.o"))
    result = []
    for src in paths:
        suffix = src.name + ".o"
        matches = [p for p in objects if p.name == suffix]
        require(len(matches) == 1, "OBJECT_NOT_UNIQUE_" + suffix)
        obj = matches[0]
        dep = obj.with_suffix(".d")
        require(dep.is_file(), "DEP_MISSING_" + suffix)
        d = dep.read_text(encoding="utf-8", errors="replace").replace("\\\n", "")
        require(src.name in d, "DEPENDENCY_WRONG_" + suffix)
        result.append({"src":str(src.relative_to(root)), "src_sha256":digest(src),
                       "object":str(obj), "object_sha256":digest(obj)})
    require(len(result) == len(objects), "UNEXPECTED_OBJECT_COUNT_" + lib)
    require(len({Path(x["object"]).name for x in result}) == len(result),
            "DUPLICATE_OBJECT_BASENAME_" + lib)
    return result

def archiver_from_props(props: dict[str, str]) -> str:
    prefix = props.get("compiler.prefix", "")
    cmd = props.get("compiler.ar.cmd", "")
    path = props.get("compiler.path", "")
    require(bool(path) and bool(cmd), "ARCHIVER_PROPERTIES_MISSING")
    binary = Path(path) / (prefix + cmd)
    if os.name == "nt" and not binary.exists():
        binary = binary.with_suffix(".exe")
    require(binary.exists(), "ARCHIVER_MISSING_" + str(binary))
    return str(binary)

def publish_stage_archive(cli: str, props: dict[str, str], hw: Path,
                          build: Path, manifest: dict) -> None:
    archiver = archiver_from_props(props)
    for lib in LIBS:
        objects = object_manifest(lib, hw, build)
        libroot = hw / "libraries" / lib
        target = libroot / "src/esp32" / f"lib{lib}.a"
        target.parent.mkdir(parents=True, exist_ok=True)
        run([archiver, "crs", str(target)] + [v["object"] for v in objects])
        members = run([archiver, "t", str(target)]).splitlines()
        require(sorted(members) == sorted(Path(x["object"]).name for x in objects),
                "ARCHIVE_MEMBERS_MISMATCH_" + lib)
        props_path = libroot / "library.properties"
        props_text = props_path.read_text(encoding="utf-8")
        props_path.write_text(props_text.rstrip() + "\nprecompiled=full\n",
                              encoding="utf-8")
        manifest["archives"][lib] = {
            "sha256":digest(target), "bytes":target.stat().st_size,
            "objects":objects, "members":members,
            "source_library_properties_sha256":digest(PLATFORM / "libraries" / lib / "library.properties")
        }

def assert_precompiled(build: Path, log: str, hw: Path) -> None:
    for lib in LIBS:
        expected = hw / "libraries" / lib / "src/esp32" / f"lib{lib}.a"
        require(expected.is_file(), "ARCHIVE_LOST_" + lib)
        # precompiled=full debe evitar la compilación de fuentes del library.
        objdir = build / "libraries" / lib
        require(not list(objdir.rglob("*.cpp.o")) if objdir.exists() else True,
                "FALLBACK_COMPILE_CPP_" + lib)
        require(not list(objdir.rglob("*.c.o")) if objdir.exists() else True,
                "FALLBACK_COMPILE_C_" + lib)
        archive_name = "lib" + lib + ".a"
        require(archive_name.lower() in log.lower(), "ARCHIVE_NOT_LINKED_" + lib)
    print("A13_ARCHIVE_LINK=PASS_TWO_ARCHIVES_ZERO_SOURCE_OBJECTS", flush=True)

def perform(out: Path, cli: str, detached: bool) -> None:
    head = n0(detached)
    require(not out.exists(), "OUTPUT_EXISTS")
    require(not out.is_relative_to(ROOT), "OUTPUT_INSIDE_WORKTREE")
    out.mkdir(parents=True)
    source_hw, source_cfg = stage(PLATFORM, out / "source", cli)
    source_props = properties(cli, source_cfg, PROFILES[0], source_hw)
    report = {"source_sha": head, "fqbn":list(PROFILES), "archives":{},
              "bench":{}, "hardware":"NOT_TOUCHED", "status":"IN_PROGRESS"}
    source_build = out / "source_build_basic"
    a, source_log = compile_one(cli, source_cfg, PROFILES[0], source_build,
                                out / "source_basic_cold.log", True)
    b, _ = compile_one(cli, source_cfg, PROFILES[0], source_build,
                       out / "source_basic_warm.log", False)
    report["bench"]["source"] = {"cold_s":a, "warm_s":b}
    archive_hw, archive_cfg = stage(PLATFORM, out / "candidate", cli)
    publish_stage_archive(cli, source_props, archive_hw, source_build, report)
    archive_props = properties(cli, archive_cfg, PROFILES[0], archive_hw)
    for key in ("compiler.cpp.flags", "compiler.c.flags", "compiler.prefix", "compiler.path"):
        require(source_props.get(key) == archive_props.get(key),
                "STAGE_FLAGS_DIFFER_" + key)
    cand_build = out / "archive_build_basic"
    a2, c_log = compile_one(cli, archive_cfg, PROFILES[0], cand_build,
                            out / "archive_basic_cold.log", True)
    assert_precompiled(cand_build, c_log, archive_hw)
    b2, c_warm = compile_one(cli, archive_cfg, PROFILES[0], cand_build,
                             out / "archive_basic_warm.log", False)
    assert_precompiled(cand_build, c_warm, archive_hw)
    report["bench"]["archive"] = {"cold_s":a2, "warm_s":b2}
    # El segundo perfil no crea otro .a: debe enlazar el mismo archivo.
    for label, hw, cfg, kind in (
        ("source", source_hw, source_cfg, "source"),
        ("archive", archive_hw, archive_cfg, "archive"),
    ):
        props = properties(cli, cfg, PROFILES[1], hw)
        require(props.get("compiler.prefix") == source_props.get("compiler.prefix"),
                "INCOMPATIBLE_COMPILER_SECOND_FQBN")
        build = out / f"{label}_build_basiccore"
        t, log = compile_one(cli, cfg, PROFILES[1], build,
                             out / f"{label}_basiccore_cold.log", True)
        if kind == "archive":
            assert_precompiled(build, log, archive_hw)
        report["bench"][kind]["basiccore_cold_s"] = t
    for lib in LIBS:
        rel = Path("libraries") / lib / "src/esp32" / f"lib{lib}.a"
        report["archives"][lib]["stage_path"] = str((archive_hw / rel).relative_to(out))
    report["status"] = "PASS_TWO_ARCHIVES_TWO_FQBN_SOURCE_VS_PRECOMPILED"
    (out / "MANIFEST.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print("A13_ARCHIVE_BUILD=PASS_TWO_ARCHIVES", flush=True)
    print("A13_ARCHIVE_FQBN=PASS_BASIC_AND_BASICCORE", flush=True)
    for kind in ("source","archive"):
        vals=report["bench"][kind]
        print("A13_BENCH_" + kind.upper() + "_BASIC_COLD_WARM_SECONDS=" +
              str(vals["cold_s"]) + "," + str(vals["warm_s"]), flush=True)
    print("A13_ARCHIVE_REPORT=" + str(out / "MANIFEST.json"), flush=True)
    print("A13_ARCHIVE_HW=NOT_TOUCHED_COM=NOT_OPENED_UPLOAD=NOT_RUN", flush=True)

def main() -> None:
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument("--check", action="store_true")
    p.add_argument("--ci-detached", action="store_true")
    p.add_argument("--output",type=Path)
    p.add_argument("--cli",default="arduino-cli")
    args=p.parse_args()
    try:
        n0(args.ci_detached) if args.check else perform(
            args.output.resolve() if args.output else Path(""), args.cli,
            args.ci_detached)
        if args.check:
            print("A13_ARCHIVE_N0_ONLY=PASS_NO_BUILD_NO_NETWORK_NO_USB", flush=True)
    except (RuntimeError, OSError, ValueError, subprocess.TimeoutExpired) as exc:
        print(str(exc),file=sys.stderr)
        sys.exit(2)

if __name__=="__main__":
    main()
