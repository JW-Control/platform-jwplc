#!/usr/bin/env python3
"""Alpha13 C: verifica ZIP real de jwplc_release.py y compila el package extraído.

Sin upload, COM, borrado de worktrees locales, push, GitHub Release ni modificación
del package. La instalación es un staging limpio en Arduino CLI, NO Boards Manager
publicado. Reutiliza N3 anterior; sólo dos consumidores representative FQBN.
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
import tempfile
import time
import zipfile

ROOT = Path(__file__).resolve().parents[2]
PKG = ROOT / "JWPLC/2.1.0"
VERSION = "2.1.0-alpha.13"
TREE = "cc17716f173509b1e0f3c0df5eb99549014c1d61"
ARCHIVES = {
    "JWPLC_Ethernet": "f190bbbbd69861941807a290be653de422e11a898d1a9583d289cc573d38a0f4",
    "JWPLC_ModbusTCP": "d5456307c5e8a4d2c4fcd3252464b96859faaa4df3ece3ca23e3c720f3b25a1d",
}
FQBN_BOARDS = ("jwplcbasic", "jwplcbasiccore")
SKETCH = ROOT / "tools/alpha13/g10_promoted_bench/G10P_COM4_Master"
INDEX = ROOT / "JWPLC/package_jwplc_index_dev.json"
STABLE_INDEX = ROOT / "JWPLC/package_jwplc_index.json"


def fail_if(cond: bool, reason: str) -> None:
    if cond:
        raise RuntimeError("A13_C_RELEASE_LIKE_STOP_" + reason)


def sha(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            h.update(chunk)
    return h.hexdigest()


def run(cmd: list[str | Path], *, log: Path | None = None, timeout: int = 1200) -> str:
    p = subprocess.run([str(x) for x in cmd], cwd=ROOT, capture_output=True,
                       text=True, errors="replace", timeout=timeout)
    out = (p.stdout or "") + "\n" + (p.stderr or "")
    if log is not None:
        log.parent.mkdir(parents=True, exist_ok=True)
        log.write_text(out, encoding="utf-8")
    fail_if(p.returncode != 0, "COMMAND_" + str(cmd[0]) +
            "_EXIT_" + str(p.returncode) + "_" + out[-500:].replace("\n", "_"))
    return out


def zip_inventory(zip_path: Path) -> dict:
    fail_if(not zip_path.is_file(), "ZIP_ABSENT")
    fail_if(zip_path.name != f"jwplc-esp32-{VERSION}.zip", "ZIP_NAME")
    fail_if(run(["git", "rev-parse", "HEAD:JWPLC/2.1.0"]).strip() != TREE,
            "PACKAGE_TREE_CHANGED_FROM_N3")
    fail_if(not SKETCH.is_dir(), "MASTER_SKETCH_ABSENT")
    skip = {".git", "__pycache__"}
    source_paths = {
        "2.1.0/" + p.relative_to(PKG).as_posix(): p
        for p in PKG.rglob("*") if p.is_file()
        and not skip.intersection(p.parts)
        and p.name.lower() not in {"thumbs.db", ".ds_store"}
    }
    with zipfile.ZipFile(zip_path) as zf:
        members = [z for z in zf.infolist() if not z.is_dir()]
        names = [z.filename for z in members]
        fail_if(len(set(names)) != len(names), "ZIP_DUPLICATES")
        fail_if(set(names) != set(source_paths), "ZIP_SOURCE_PATHS_DIFFER")
        fail_if(any(".." in Path(n).parts or n.startswith("/") for n in names),
                "ZIP_TRAVERSAL")
        fail_if({n.split("/", 1)[0] for n in names} != {"2.1.0"}, "ZIP_ROOT")
        for m in members:
            expected = sha(source_paths[m.filename])
            actual = hashlib.sha256(zf.read(m.filename)).hexdigest()
            fail_if(expected != actual, "ZIP_BYTE_DRIFT_" + m.filename)
        for lib, digest in ARCHIVES.items():
            base = f"2.1.0/libraries/{lib}/"
            archive = base + f"src/esp32/lib{lib}.a"
            properties = base + "library.properties"
            fail_if(archive not in names or properties not in names,
                    "ARCHIVE_PAIR_MISSING_" + lib)
            fail_if(hashlib.sha256(zf.read(archive)).hexdigest() != digest,
                    "ARCHIVE_SHA_MISMATCH_" + lib)
            fail_if(b"precompiled=full" not in zf.read(properties),
                    "PRECOMPILED_FLAG_MISSING_" + lib)
    print("C_ZIP_STRUCTURE=PASS_UNIQUE_ROOT_ALL_SOURCE_BYTES", flush=True)
    print("C_ARCHIVES=PASS_TWO_SHA256_LIBRARY_PROPERTIES", flush=True)
    return {
        "zip_name": zip_path.name, "zip_sha256": sha(zip_path),
        "zip_size": zip_path.stat().st_size, "entries": len(names),
        "package_tree": TREE,
        "archive_sha256": ARCHIVES,
    }


def index_check(zip_info: dict) -> None:
    dev = json.loads(INDEX.read_text(encoding="utf-8"))
    stable = json.loads(STABLE_INDEX.read_text(encoding="utf-8"))
    entries = [p for package in dev["packages"]
               for p in package.get("platforms", [])
               if p.get("version") == VERSION]
    fail_if(len(entries) != 1, "DEV_INDEX_ENTRY_NOT_UNIQUE")
    entry = entries[0]
    fail_if(entry.get("archiveFileName") != zip_info["zip_name"], "DEV_ZIP_NAME")
    fail_if(entry.get("checksum") != "SHA-256:" + zip_info["zip_sha256"], "DEV_CHECKSUM")
    fail_if(entry.get("size") != str(zip_info["zip_size"]), "DEV_SIZE")
    fail_if(any(p.get("version") == VERSION for package in stable["packages"]
                for p in package.get("platforms", [])), "STABLE_INDEX_ALPHA13_PRESENT")
    print("C_INDEX_SIMULATED=PASS_DEV_ONLY_STABLE_OMITTED", flush=True)


def verify_link(log: str, install: Path, label: str) -> None:
    normalized = re.sub(r"/+", "/", log.replace("\\", "/"))
    for lib in ARCHIVES:
        fail_if("Using library " + lib + " " not in normalized,
                "LIBRARY_DISCOVERY_" + lib + "_" + label)
        target = "/libraries/" + lib + "/src/esp32"
        dirs = re.findall(r'-L(?:["\\]?)?([^"\s]+)', normalized)
        fail_if(not any(x.rstrip("/").endswith(target) for x in dirs),
                "ARCHIVE_SEARCH_PATH_" + lib + "_" + label)
        fail_if("-l" + lib not in normalized, "ARCHIVE_LINK_" + lib + "_" + label)


def isolated_compile(zip_path: Path, output: Path, cli: str) -> list[dict]:
    completed: list[dict] = []
    with tempfile.TemporaryDirectory(prefix="a13_release_like_") as tmp:
        temp = Path(tmp)
        config_dir = temp / "config"
        sketchbook = temp / "sketchbook"
        data = temp / "data"
        downloads = temp / "staging"
        run([cli, "config", "init", "--dest-dir", config_dir, "--overwrite"])
        cfg = config_dir / "arduino-cli.yaml"
        cli_cmd = [cli, "--config-file", cfg]
        for k, value in (("directories.user", sketchbook),
                         ("directories.data", data),
                         ("directories.downloads", downloads)):
            run(cli_cmd + ["config", "set", k, value])
        run(cli_cmd + ["config", "set", "board_manager.additional_urls",
                       "https://raw.githubusercontent.com/JW-Control/platform-jwplc/main/JWPLC/package_jwplc_index_dev.json"])
        run(cli_cmd + ["core", "update-index"], timeout=450)
        # Igual al bootstrap inmutable que superó N3. No se instala Alpha13
        # desde un índice inexistente: se usa la copia exacta del ZIP auditado.
        run(cli_cmd + ["core", "install", "jwplc:esp32@2.1.0-alpha.10"],
            timeout=1000)
        package_root = data / "packages/jwplc/hardware/esp32" / VERSION
        fail_if(package_root.exists(), "PACKAGE_DEST_EXISTS_NOT_CLEAN")
        extract_tmp = temp / "extracted"
        with zipfile.ZipFile(zip_path) as zf:
            zf.extractall(extract_tmp)
        shutil.copytree(extract_tmp / "2.1.0", package_root)
        for lib, digest in ARCHIVES.items():
            p = package_root / "libraries" / lib / "src/esp32" / ("lib" + lib + ".a")
            fail_if(sha(p) != digest, "STAGED_ARCHIVE_DRIFT_" + lib)
        print("C_CLI_STAGE=PASS_ZIP_EXTRACTED_IN_ISOLATED_DATA", flush=True)
        for board in FQBN_BOARDS:
            label = board
            fqbn = "jwplc:esp32:" + board
            props = run(cli_cmd + ["compile", "--fqbn", fqbn,
                                   "--show-properties", SKETCH], timeout=160)
            selected = [x.split("=", 1)[1] for x in props.splitlines()
                        if x.startswith("runtime.platform.path=")]
            fail_if(len(selected) != 1
                    or Path(selected[0]).resolve() != package_root.resolve(),
                    "WRONG_PLATFORM_OR_BOOTSTRAP_SELECTED_" + label)
            build = temp / ("build_" + board)
            started = time.perf_counter()
            log = run(cli_cmd + ["compile", "--fqbn", fqbn, "--build-path",
                                 build, "--clean", "-v", SKETCH],
                      log=output / (label + ".log"), timeout=1500)
            elf_files = list(build.glob("*.elf"))
            fail_if(len(elf_files) != 1, "ELF_NOT_UNIQUE_" + label)
            verify_link(log, package_root, label)
            for lib in ARCHIVES:
                o = build / "libraries" / lib
                fail_if(o.exists() and bool(list(o.rglob("*.cpp.o"))),
                        "SOURCE_RECOMPILED_" + lib + "_" + label)
            record = {
                "fqbn": fqbn, "source": "ZIP_STAGED_OFFICIAL_NAMESPACE",
                "platform": str(package_root),
                "elf_sha256": sha(elf_files[0]),
                "seconds": round(time.perf_counter() - started, 3),
                "archive_flags": "PASS_TWO_LIBRARIES_L_AND_l",
                "cpp_recompiled": False, "status": "PASS",
            }
            completed.append(record)
            print("C_CLI_COMPILE_" + board + "=PASS_ELF_LINK_ARCHIVES", flush=True)
    return completed


def main() -> int:
    p = argparse.ArgumentParser()
    p.add_argument("--zip", required=True, type=Path)
    p.add_argument("--output", required=True, type=Path)
    p.add_argument("--cli", default="arduino-cli")
    args = p.parse_args()
    output = args.output.resolve()
    fail_if(output.is_relative_to(ROOT), "OUTPUT_INSIDE_REPO")
    output.mkdir(parents=True, exist_ok=True)
    report = {
        "schema": "A13-C-RELEASE-LIKE-1", "status": "IN_PROGRESS",
        "hardware": "NOT_TOUCHED", "github_release": "NOT_CREATED",
        "boards_manager_published_install": "NOT_RUN",
        "arduino_ide_gui": "NOT_RUN",
        "head": run(["git", "rev-parse", "HEAD"]).strip(),
    }
    try:
        zip_info = zip_inventory(args.zip.resolve())
        report.update(zip_info)
        index_check(zip_info)
        report["sketches"] = isolated_compile(args.zip.resolve(), output, args.cli)
        fail_if(len(report["sketches"]) != 2, "FQBN_COUNT")
        report["status"] = "PASS_RELEASE_LIKE_ZIP_CLI_TWO_FQBN"
        print("C_RELEASE_LIKE=PASS_ZIP_BOTH_FQBN_NO_UPLOAD", flush=True)
        return 0
    except Exception as exc:
        report["status"] = "REVIEW_STOP"
        report["error"] = str(exc)
        print(str(exc), file=sys.stderr, flush=True)
        return 2
    finally:
        (output / "MANIFEST.json").write_text(
            json.dumps(report, indent=2, ensure_ascii=False) + "\n",
            encoding="utf-8")


if __name__ == "__main__":
    sys.exit(main())
