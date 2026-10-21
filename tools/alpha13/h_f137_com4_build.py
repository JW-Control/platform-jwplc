#!/usr/bin/env python3
"""H-F137: compilar/sellar Master COM4; upload solo con consentimiento separado.
No respaldos para COM4/COM14 de desarrollo (decisión de 17/10/2026).
--check/--selftest/--prepare NO abren puertos. CI nunca permite upload.
"""
from __future__ import annotations
import argparse
import hashlib
import json
from pathlib import Path
import re
import shutil
import subprocess
import sys
import time

ROOT = Path(__file__).resolve().parents[2]
PKG = ROOT / "JWPLC/2.1.0"
SKETCH = ROOT / "tools/alpha13/g10_promoted_bench/G10P_COM4_Master"
INO = SKETCH / "G10P_COM4_Master.ino"
BRANCH = "codex/alpha13-precompile-benchmark-docs-20261020"
VENDOR = "jwplc_f137"
FQBN = VENDOR + ":esp32:jwplcbasic"
LIBS = {
    "JWPLC_Ethernet": "f190bbbbd69861941807a290be653de422e11a898d1a9583d289cc573d38a0f4",
    "JWPLC_ModbusTCP": "d5456307c5e8a4d2c4fcd3252464b96859faaa4df3ece3ca23e3c720f3b25a1d",
}
FILES = (
    "G10P_COM4_Master.ino.elf", "G10P_COM4_Master.ino.bin",
    "G10P_COM4_Master.ino.bootloader.bin",
    "G10P_COM4_Master.ino.partitions.bin",
)
APPROVAL = "AUTORIZO SUBIR F137 SOLO A COM4 SIN BACKUP"


def require(cond, code):
    if not cond:
        raise RuntimeError("H_F137_COM4_REVIEW_STOP_" + code)


def sha(path):
    h = hashlib.sha256()
    with Path(path).open("rb") as f:
        for chunk in iter(lambda: f.read(1024 * 1024), b""):
            h.update(chunk)
    return h.hexdigest()


def cmd(args, *, log=None, timeout=1200):
    p = subprocess.run([str(a) for a in args], cwd=ROOT,
                       capture_output=True, text=True, errors="replace", timeout=timeout)
    value = (p.stdout or "") + "\n" + (p.stderr or "")
    if log is not None:
        Path(log).write_text(value, encoding="utf-8")
    # No publicar salida raw del proceso de upload (puede mostrar MAC).
    require(p.returncode == 0, "COMMAND_FAILED_" + str(args[0]) + "_" + str(p.returncode))
    return value


def git(*args):
    return cmd(["git", *args], timeout=30).strip()


def check(head, ci=False):
    require(bool(re.fullmatch(r"[0-9a-f]{40}", head)), "HEAD_INVALID")
    require(git("rev-parse", "HEAD") == head, "HEAD_CHANGED")
    branch = git("branch", "--show-current")
    require(branch == BRANCH or (ci and not branch), "WRONG_BRANCH")
    require(not git("status", "--porcelain", "--untracked-files=no"),
            "WORKTREE_DIRTY")
    require(INO.is_file(), "MASTER_SKETCH_MISSING")
    source = INO.read_text(encoding="utf-8")
    for forbidden in ("G10RLabDhcpProbe", "LabSocketSnapshot",
                      "gpio_set_level(", "ESP.restart(", "jwplcWriteQ(",
                      "FILE_WRITE", "erase_flash", "write_flash"):
        require(forbidden not in source, "UNSAFE_SKETCH")
    for lib, expected in LIBS.items():
        base = PKG / "libraries" / lib
        a = base / "src/esp32" / ("lib" + lib + ".a")
        require(a.is_file() and sha(a) == expected, "ARCHIVE_SHA_" + lib)
        props = (base / "library.properties").read_text(encoding="utf-8")
        require(bool(re.search(r"(?m)^precompiled=full\s*$", props)),
                "PRECOMPILED_FULL_" + lib)
    print("H_F137_COM4_N0=PASS_HEAD_CLEAN_TWO_ARCHIVES")
    print("H_F137_COM4_BACKUP=WAIVED_DEV_BOARDS")


def verify_link(log, stage):
    norm = re.sub(r"/+", "/", log.replace("\\", "/"))
    tail = "/".join(stage.parts[-4:])
    dirs = re.findall(r'-L([^"\s]+)', norm)
    for lib in LIBS:
        rel = tail + "/libraries/" + lib + "/src/esp32"
        require(("Using library " + lib + " ") in norm,
                "LIB_NOT_SELECTED_" + lib)
        require(any(x.endswith(rel) for x in dirs),
                "LINK_SEARCH_DIR_" + lib)
        require("-l" + lib in norm, "LINK_FLAG_" + lib)


def binaries(build):
    values = {}
    for name in FILES:
        p = build / name
        require(p.is_file() and p.stat().st_size > 0, "FILE_MISSING_" + name)
        values[name] = {"sha256": sha(p), "bytes": p.stat().st_size}
    require(sorted(p.name for p in build.glob("*.elf")) == [FILES[0]],
            "ELF_MISMATCH")
    return values


def prepare(head, output, cli, ci=False):
    check(head, ci=ci)
    out = output.expanduser().resolve()
    require(out != ROOT and ROOT not in out.parents, "OUTPUT_IN_WORKTREE")
    require(not out.exists(), "OUTPUT_ALREADY_EXISTS")
    out.mkdir(parents=True)
    stage = out / "sketchbook/hardware" / VENDOR / "esp32"
    stage.parent.mkdir(parents=True)
    shutil.copytree(PKG, stage)
    for lib, expected in LIBS.items():
        a = stage / "libraries" / lib / "src/esp32" / ("lib" + lib + ".a")
        require(sha(a) == expected, "STAGED_ARCHIVE_DRIFT_" + lib)
    cfgdir = out / "config"
    cmd([cli, "config", "init", "--dest-dir", cfgdir, "--overwrite"])
    cfg = cfgdir / "arduino-cli.yaml"
    cmd([cli, "--config-file", cfg, "config", "set", "directories.user",
         out / "sketchbook"])
    props = cmd([cli, "--config-file", cfg, "compile", "--fqbn", FQBN,
                 "--show-properties", SKETCH])
    selected = [l.split("=", 1)[1].strip().strip('"') for l in props.splitlines()
                if l.startswith("runtime.platform.path=")]
    require(len(selected) == 1 and
            Path(selected[0]).resolve() == stage.resolve(),
            "WRONG_ARDUINO_PLATFORM")
    build = out / "build_COM4_MASTER"
    start = time.monotonic()
    log = cmd([cli, "--config-file", cfg, "compile", "--fqbn", FQBN,
               "--build-path", build, "--clean", "-v", SKETCH],
              log=out / "compile_COM4_MASTER.log", timeout=1800)
    seconds = round(time.monotonic() - start, 3)
    for lib in LIBS:
        directory = build / "libraries" / lib
        require(not directory.exists() or not list(directory.rglob("*.cpp.o")),
                "SOURCE_RECOMPILED_" + lib)
    verify_link(log, stage)
    seal = {
        "schema": "JWPLC-A13-F137-COM4-SEAL-V1",
        "head": head, "role": "COM4_MASTER", "port": "COM4",
        "fqbn": FQBN, "sketch_sha256": sha(INO),
        "archives": LIBS, "files": binaries(build), "seconds": seconds,
        "hardware": "NOT_TOUCHED", "upload": "NOT_RUN",
    }
    (out / "MANIFEST.json").write_text(
        json.dumps(seal, indent=2) + "\n", encoding="utf-8")
    print("H_F137_COM4_BUILD=PASS_ARCHIVES_BINARIES_SEALED")
    print("H_F137_COM4_UPLOAD=NOT_RUN")
    print("H_F137_COM4_OUTPUT=" + str(out))


def check_seal(head, out):
    require(out.is_dir() and (out / "MANIFEST.json").is_file(), "SEAL_MISSING")
    data = json.loads((out / "MANIFEST.json").read_text(encoding="utf-8"))
    require(data.get("schema") == "JWPLC-A13-F137-COM4-SEAL-V1",
            "SEAL_SCHEMA")
    require(data.get("head") == head and data.get("port") == "COM4" and
            data.get("role") == "COM4_MASTER" and data.get("fqbn") == FQBN and
            data.get("upload") == "NOT_RUN", "SEAL_OR_ROLE_MISMATCH")
    require(data.get("sketch_sha256") == sha(INO), "SKETCH_CHANGED")
    require(data.get("archives") == LIBS, "ARCHIVES_CHANGED")
    stage = out / "sketchbook/hardware" / VENDOR / "esp32"
    for lib, expected in LIBS.items():
        a = stage / "libraries" / lib / "src/esp32" / ("lib" + lib + ".a")
        require(a.is_file() and sha(a) == expected, "STAGE_CHANGED_" + lib)
    require(data.get("files") == binaries(out / "build_COM4_MASTER"),
            "BINARIES_CHANGED")
    return data


def upload(head, output, cli):
    check(head)
    out = output.expanduser().resolve()
    require(out != ROOT and ROOT not in out.parents, "OUTPUT_IN_WORKTREE")
    check_seal(head, out)
    # Identificacion USB no invasiva, sin COM14 obligatorio.
    import h_f137_com4
    h_f137_com4.inventory()
    print("H_F137_UPLOAD_WARNING=COM4_DEV_FIRMWARE_WILL_BE_OVERWRITTEN")
    print("COM14 no sera abierto ni programado.")
    print("Confirmar COM4 Master fisico, Q sin cargas/tension y monitor cerrado.")
    actual = input("Escribe exactamente '" + APPROVAL + "': ").strip()
    require(actual == APPROVAL, "UPLOAD_NOT_AUTHORIZED")
    cfg = out / "config/arduino-cli.yaml"
    require(cfg.is_file(), "CLI_CONFIG_MISSING")
    cmd([cli, "--config-file", cfg, "upload", "--fqbn", FQBN,
         "--port", "COM4", "--input-dir", out / "build_COM4_MASTER", SKETCH],
        log=out / "upload_COM4.log", timeout=300)
    (out / "UPLOAD_RECEIPT_COM4.json").write_text(
        json.dumps({"head": head, "port": "COM4", "result": "COMMAND_SUCCESS",
                    "uart": "PENDING_OBSERVATION"}, indent=2) + "\n",
        encoding="utf-8")
    print("H_F137_COM4_UPLOAD=COMMAND_SUCCESS_UART_PENDING")


def selftest():
    stage = Path("/tmp/x/sketchbook/hardware/jwplc_f137/esp32")
    log = ("Using library JWPLC_Ethernet at stage\n"
           "Using library JWPLC_ModbusTCP at stage\n"
           " -L/tmp/x/sketchbook/hardware/jwplc_f137/esp32/libraries/"
           "JWPLC_Ethernet/src/esp32 -lJWPLC_Ethernet"
           " -L/tmp/x/sketchbook/hardware/jwplc_f137/esp32/libraries/"
           "JWPLC_ModbusTCP/src/esp32 -lJWPLC_ModbusTCP")
    verify_link(log, stage)
    for broken in (log.replace("-lJWPLC_ModbusTCP", ""),
                   log.replace("-L/tmp/x/sketchbook/hardware/jwplc_f137/esp32/"
                               "libraries/JWPLC_Ethernet/src/esp32", "")):
        try:
            verify_link(broken, stage)
        except RuntimeError:
            continue
        raise RuntimeError("H_F137_COM4_REVIEW_STOP_SELFTEST_FALSE_PASS")
    print("H_F137_COM4_SELFTEST=PASS_NEGATIVES_NO_USB")


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    modes = ap.add_mutually_exclusive_group(required=True)
    modes.add_argument("--selftest", action="store_true")
    modes.add_argument("--check", action="store_true")
    modes.add_argument("--prepare", action="store_true")
    modes.add_argument("--upload-com4", action="store_true")
    ap.add_argument("--expected-head")
    ap.add_argument("--output", type=Path)
    ap.add_argument("--cli", default="arduino-cli")
    ap.add_argument("--ci-detached", action="store_true")
    args = ap.parse_args()
    require(not args.ci_detached or args.check or args.prepare,
            "CI_UPLOAD_FORBIDDEN")
    if args.selftest:
        selftest()
        return
    require(args.expected_head is not None, "HEAD_REQUIRED")
    if args.check:
        check(args.expected_head, args.ci_detached)
    else:
        require(args.output is not None, "OUTPUT_REQUIRED")
        if args.prepare:
            prepare(args.expected_head, args.output, args.cli, args.ci_detached)
        else:
            upload(args.expected_head, args.output, args.cli)


if __name__ == "__main__":
    try:
        main()
    except (RuntimeError, OSError, ValueError, KeyError, TypeError,
            json.JSONDecodeError, subprocess.TimeoutExpired, EOFError) as e:
        print(str(e) if isinstance(e, RuntimeError)
              else "H_F137_COM4_REVIEW_STOP_RUNTIME", file=sys.stderr)
        sys.exit(2)
