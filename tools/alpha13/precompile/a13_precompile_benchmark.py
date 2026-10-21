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
import statistics
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
    # Arduino CLI --show-properties devuelve compiler.ar.cmd YA expandido
    # con compiler.prefix en algunos cores (ej.: xtensa-esp32-elf-gcc-ar).
    # No duplicar prefix: antes fallaba solo el runner, no el firmware.
    name = cmd if cmd.startswith(prefix) else prefix + cmd
    binary = Path(name) if Path(name).is_absolute() else Path(path) / name
    if os.name == "nt" and not binary.exists():
        binary = binary.with_suffix(".exe")
    require(binary.exists(), "ARCHIVER_MISSING_" + str(binary))
    return str(binary)

def publish_stage_archive(cli: str, props: dict[str, str], hw: Path,
                          normalized_objects: dict, manifest: dict) -> None:
    archiver = archiver_from_props(props)
    for lib in LIBS:
        objects = [normalized_objects[lib][k] for k in sorted(normalized_objects[lib])]
        libroot = hw / "libraries" / lib
        target = libroot / "src/esp32" / f"lib{lib}.a"
        target.parent.mkdir(parents=True, exist_ok=True)
        run([archiver, "crs", str(target)] +
            [v["archive_member_path"] for v in objects])
        members = [line.strip() for line in run([archiver, "t", str(target)]).splitlines() if line.strip()]
        require(sorted(members) == sorted(Path(x["archive_member_path"]).name for x in objects),
                "ARCHIVE_MEMBERS_MISMATCH_" + lib)
        for item in objects:
            member = Path(item["archive_member_path"]).name
            extracted = subprocess.run([archiver, "p", str(target), member],
                                       capture_output=True, timeout=30)
            require(extracted.returncode == 0 and
                    hashlib.sha256(extracted.stdout).hexdigest() ==
                    item["archive_member_sha256"],
                    "ARCHIVE_MEMBER_HASH_MISMATCH_" + lib + "_" + member)
        props_path = libroot / "library.properties"
        props_text = props_path.read_text(encoding="utf-8")
        props_path.write_text(props_text.rstrip() + "\nprecompiled=full\n",
                              encoding="utf-8")
        manifest["archives"][lib] = {
            "sha256":digest(target), "bytes":target.stat().st_size,
            "objects":objects, "members":members,
            "normalization":"OBJCOPY_STRIP_DEBUG",
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
        # Arduino CLI enlaza archives mediante '-L<directorio> -l<nombre>';
        # el comando verbose no incluye necesariamente 'lib<nombre>.a'.
        libdir = str(expected.parent).replace("\\", "/")
        normalized = log.replace("\\", "/")
        require(("-l" + lib) in normalized and libdir in normalized,
                "ARCHIVE_NOT_LINKED_" + lib)
    print("A13_ARCHIVE_LINK=PASS_TWO_ARCHIVES_ZERO_SOURCE_OBJECTS", flush=True)

REPEAT_PAIRS = 3


def measured_pairs(cli: str, cfg: Path, board: str, build: Path,
                   out: Path, label: str, archive_hw: Path | None = None) -> dict:
    """Tres pares comparables cold/then-warm sin eliminar toda la cache."""
    cold, warm = [], []
    for i in range(1, REPEAT_PAIRS + 1):
        c, log_c = compile_one(
            cli, cfg, board, build, out / f"{label}_{i}_cold.log", True)
        if archive_hw is not None:
            assert_precompiled(build, log_c, archive_hw)
        w, log_w = compile_one(
            cli, cfg, board, build, out / f"{label}_{i}_warm.log", False)
        if archive_hw is not None:
            assert_precompiled(build, log_w, archive_hw)
        cold.append(c)
        warm.append(w)
    return {
        "cold_s": round(statistics.median(cold), 3),
        "warm_s": round(statistics.median(warm), 3),
        "cold_samples_s": cold,
        "warm_samples_s": warm,
        "min_cold_s": min(cold),
        "max_cold_s": max(cold),
        "min_warm_s": min(warm),
        "max_warm_s": max(warm),
    }


def canonicalize_objects(props: dict, objects: dict, out: Path) -> dict:
    """Guardar objetos sin DWARF, sin tocar originales ni el package.
    SOLO estos .o se usarán al crear .a si coinciden en ambos FQBN.
    """
    prefix = props.get("compiler.prefix", "")
    toolpath = Path(props.get("compiler.path", ""))
    objcopy = toolpath / (prefix + "objcopy")
    if os.name == "nt" and not objcopy.exists():
        objcopy = objcopy.with_suffix(".exe")
    require(bool(prefix) and objcopy.is_file(), "OBJCOPY_TOOL_MISSING")
    canon = {}
    for lib in LIBS:
        libroot = out / lib
        libroot.mkdir(parents=True, exist_ok=True)
        canon[lib] = {}
        for key, entry in sorted(objects[lib].items()):
            target = libroot / Path(entry["object"]).name
            require(not target.exists(), "CANON_MEMBER_DUPLICATE_" + lib)
            run([str(objcopy), "--strip-debug", entry["object"], str(target)])
            canon[lib][key] = {
                **entry,
                "archive_member_path": str(target),
                "archive_member_sha256": digest(target),
            }
    return canon


def assert_fqbn_objects_equal(ba_by_lib: dict, co_by_lib: dict,
                              ba_normalized: dict, co_normalized: dict,
                              out: Path) -> dict:
    """Objetos Basic/Core generados en el MISMO build path, distinto --clean."""
    parity = {}
    for lib in LIBS:
        ba = ba_by_lib[lib]
        co = co_by_lib[lib]
        require(set(ba) == set(co), "FQBN_SOURCE_SET_DIFFER_" + lib)
        raw_diffs = [k for k in sorted(ba)
                     if ba[k]["object_sha256"] != co[k]["object_sha256"]]
        require(set(ba_normalized[lib]) == set(co_normalized[lib]),
                "FQBN_CANONICAL_SOURCE_SET_CHANGED_" + lib)
        archive_diffs = [k for k in sorted(ba)
                         if ba_normalized[lib][k]["archive_member_sha256"] !=
                         co_normalized[lib][k]["archive_member_sha256"]]
        parity[lib] = {
            "sources": len(ba),
            "raw_diff_count": len(raw_diffs),
            "raw_different_sources": raw_diffs,
            "diff_count": len(archive_diffs),
            "different_sources": archive_diffs,
            "comparison": "ARCHIVE_READY_OBJCOPY_STRIP_DEBUG",
        }
        print("A13_FQBN_RAW_" + lib + "_DIFFS=" + str(len(raw_diffs)), flush=True)
        print("A13_FQBN_ARCHIVE_READY_" + lib + "=" +
              ("PASS" if not archive_diffs else "REVIEW_STOP") +
              "_DIFFS_" + str(len(archive_diffs)), flush=True)
    (out / "FQBN_OBJECT_PARITY.json").write_text(
        json.dumps(parity, indent=2) + "\n", encoding="utf-8")
    require(all(not p["diff_count"] for p in parity.values()),
            "FQBN_OBJECT_MISMATCH_ARCHIVE_FORBIDDEN")
    return parity


def perform(out: Path, cli: str, detached: bool) -> None:
    head = n0(detached)
    require(not out.exists(), "OUTPUT_EXISTS")
    require(not out.is_relative_to(ROOT), "OUTPUT_INSIDE_WORKTREE")
    out.mkdir(parents=True)
    source_hw, source_cfg = stage(PLATFORM, out / "source", cli)
    basic, core = PROFILES
    source_props = properties(cli, source_cfg, basic, source_hw)
    report = {
        "source_sha": head, "fqbn": list(PROFILES), "archives": {},
        "bench": {}, "repetitions": REPEAT_PAIRS, "hardware": "NOT_TOUCHED",
        "status": "IN_PROGRESS"
    }
    # Mismo directorio de objetos elimina rutas distintas (especialmente -g
    # de toolchains Windows) como falso positivo de incompatibilidad FQBN.
    shared_build = out / "source_build_same_path"
    sb = measured_pairs(
        cli, source_cfg, basic, shared_build, out, "source_basic")
    basic_objects = {
        lib: {x["src"]: x for x in object_manifest(lib, source_hw, shared_build)}
        for lib in LIBS
    }
    basic_normalized = canonicalize_objects(
        source_props, basic_objects, out / "canonical_basic")
    sc = measured_pairs(
        cli, source_cfg, core, shared_build, out, "source_basiccore")
    core_objects = {
        lib: {x["src"]: x for x in object_manifest(lib, source_hw, shared_build)}
        for lib in LIBS
    }
    core_props = properties(cli, source_cfg, core, source_hw)
    core_normalized = canonicalize_objects(
        core_props, core_objects, out / "canonical_core")
    report["bench"]["source"] = {
        **sb, "basiccore_cold_s": sc["cold_s"],
        "basiccore_warm_s": sc["warm_s"],
        "basiccore_cold_samples_s": sc["cold_samples_s"],
        "basiccore_warm_samples_s": sc["warm_samples_s"],
    }
    report["fqbn_object_parity"] = assert_fqbn_objects_equal(
        basic_objects, core_objects, basic_normalized, core_normalized, out)

    archive_hw, archive_cfg = stage(PLATFORM, out / "candidate", cli)
    # Solo empaquetar objetos canónicos idénticos, sin metadata DWARF/FQBN.
    publish_stage_archive(cli, source_props, archive_hw, core_normalized, report)
    for board in PROFILES:
        source_board_props = properties(cli, source_cfg, board, source_hw)
        archive_board_props = properties(cli, archive_cfg, board, archive_hw)
        for key in ("compiler.cpp.flags", "compiler.c.flags",
                    "compiler.prefix", "compiler.path"):
            require(source_board_props.get(key) == archive_board_props.get(key),
                    "STAGE_FLAGS_DIFFER_" + board + "_" + key)
    ab = measured_pairs(
        cli, archive_cfg, basic, out / "archive_build_basic",
        out, "archive_basic", archive_hw)
    ac = measured_pairs(
        cli, archive_cfg, core, out / "archive_build_basiccore",
        out, "archive_basiccore", archive_hw)
    report["bench"]["archive"] = {
        **ab, "basiccore_cold_s": ac["cold_s"],
        "basiccore_warm_s": ac["warm_s"],
        "basiccore_cold_samples_s": ac["cold_samples_s"],
        "basiccore_warm_samples_s": ac["warm_samples_s"],
    }
    for lib in LIBS:
        rel = Path("libraries") / lib / "src/esp32" / f"lib{lib}.a"
        report["archives"][lib]["stage_path"] = str(
            (archive_hw / rel).relative_to(out))
    report["status"] = "PASS_TWO_ARCHIVES_CANONICAL_PARITY_THREE_PAIRS"
    (out / "MANIFEST.json").write_text(
        json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print("A13_ARCHIVE_BUILD=PASS_TWO_ARCHIVES", flush=True)
    print("A13_ARCHIVE_FQBN=PASS_PARITY_BASIC_AND_BASICCORE", flush=True)
    for kind in ("source", "archive"):
        vals = report["bench"][kind]
        print("A13_BENCH_" + kind.upper() +
              "_BASIC_COLD_WARM_MEDIAN_SECONDS=" +
              str(vals["cold_s"]) + "," + str(vals["warm_s"]), flush=True)
        print("A13_BENCH_" + kind.upper() +
              "_BASICCORE_COLD_WARM_MEDIAN_SECONDS=" +
              str(vals["basiccore_cold_s"]) + "," +
              str(vals["basiccore_warm_s"]), flush=True)
    print("A13_ARCHIVE_REPORT=" + str(out / "MANIFEST.json"), flush=True)
    print("A13_ARCHIVE_HW=NOT_TOUCHED_COM=NOT_OPENED_UPLOAD=NOT_RUN",
          flush=True)

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
