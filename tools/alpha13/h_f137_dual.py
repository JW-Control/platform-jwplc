#!/usr/bin/env python3
"""F137: sello Master previo + compila Slave; subida COM14 y COM4 en un solo gate.
Placas de desarrollo: sin backup. Upload prohibido en CI, sin erase-all.
"""
from __future__ import annotations
import argparse
import json
from pathlib import Path
import re
import sys
import time
import subprocess

import h_f137_com4 as usb
import h_f137_com4_build as m

SLAVE = m.ROOT / "tools/alpha13/g10_promoted_bench/G10P_COM14_Slave"
INO = SLAVE / "G10P_COM14_Slave.ino"
MARK = "VER=G10_PROMOTED_PRODUCT_F137_V1 RTU="
CONSENT = "AUTORIZO UPLOAD F137 COM14 Y COM4 SIN BACKUP"
SCHEMA = "JWPLC-F137-COM14-SEALED-V1"

def req(cond, reason):
    m.require(cond, "DUAL_" + reason)

def git(*args):
    return m.cmd(["git", *args], timeout=30).strip()

def source_sha():
    req(INO.is_file(), "SLAVE_SOURCE_MISSING")
    data = INO.read_text(encoding="utf-8")
    req(MARK in data and "JWPLC_ModbusRTU.begin(2, 115200, SERIAL_8N1)" in data,
        "SLAVE_ROLE_OR_MARKER")
    for s in ("FILE_WRITE", "jwplcWriteQ(", "ESP.restart(", "gpio_set_level("):
        req(s not in data, "SLAVE_UNSAFE_" + s)
    return m.sha(INO)

def check_master(head, out):
    orig = out / "MANIFEST.json"
    req(orig.is_file(), "MASTER_SEAL_MISSING")
    seal = json.loads(orig.read_text(encoding="utf-8"))
    previous = seal.get("head", "")
    req(bool(re.fullmatch(r"[0-9a-f]{40}", previous)), "MASTER_ORIGIN_BAD")
    m.cmd(["git", "merge-base", "--is-ancestor", previous, head], timeout=30)
    protected = ["JWPLC/", "tools/alpha13/g10_promoted_bench/G10P_COM4_Master/",
                 "tools/alpha13/h_f137_com4_build.py"]
    diff = git("diff", "--name-only", previous, head, "--", *protected)
    req(not diff, "MASTER_COMPILED_INPUT_CHANGED")
    m.check_seal(previous, out)
    return seal

def files(folder):
    name = "G10P_COM14_Slave.ino"
    fn = [name + suffix for suffix in (".elf", ".bin",
                                        ".bootloader.bin", ".partitions.bin")]
    req(sorted(p.name for p in folder.glob("*.elf")) == [fn[0]],
        "SLAVE_ELF_NOT_UNIQUE")
    result = {}
    for f in fn:
        p = folder / f
        req(p.is_file() and p.stat().st_size > 0, "SLAVE_BINARY_MISSING_" + f)
        result[f] = {"sha256": m.sha(p), "bytes": p.stat().st_size}
    return result

def stage_check(out):
    stage = out / "sketchbook/hardware" / m.VENDOR / "esp32"
    for lib, expected in m.LIBS.items():
        file = stage / "libraries" / lib / "src/esp32" / ("lib" + lib + ".a")
        req(file.is_file() and m.sha(file) == expected, "STAGE_ARCHIVE_" + lib)
    req((out / "config/arduino-cli.yaml").is_file(), "ARDUINO_CONFIG_MISSING")
    return stage

def prepare_slave(head, out, cli, ci):
    m.check(head, ci=ci)
    req(out != m.ROOT and m.ROOT not in out.parents, "OUTPUT_IN_REPOSITORY")
    req(not (out / "MANIFEST_COM14.json").exists(), "ALREADY_SEALED")
    orig = check_master(head, out)
    stage = stage_check(out)
    sketch_sha = source_sha()
    cfg = out / "config/arduino-cli.yaml"
    props = m.cmd([cli, "--config-file", cfg, "compile", "--fqbn",
                   m.FQBN, "--show-properties", SLAVE])
    selected = [line.split("=", 1)[1].strip().strip('"')
                for line in props.splitlines() if line.startswith("runtime.platform.path=")]
    req(len(selected) == 1 and Path(selected[0]).resolve() == stage.resolve(),
        "WRONG_PLATFORM")
    path = out / "build_COM14_SLAVE"
    req(not path.exists(), "SLAVE_BUILD_ALREADY_EXISTS")
    start = time.monotonic()
    log = m.cmd([cli, "--config-file", cfg, "compile", "--fqbn", m.FQBN,
                 "--build-path", path, "--clean", "-v", SLAVE],
                log=out / "compile_COM14_SLAVE.log", timeout=1800)
    m.verify_link(log, stage)
    for lib in m.LIBS:
        directory = path / "libraries" / lib
        req(not directory.exists() or not list(directory.rglob("*.cpp.o")),
            "SOURCE_RECOMPILED_" + lib)
    manifest = {
        "schema": SCHEMA, "head": head, "master_build_head": orig["head"],
        "port": "COM14", "role": "SLAVE", "fqbn": m.FQBN,
        "sketch_sha256": sketch_sha, "archives": m.LIBS,
        "files": files(path), "compile_seconds": round(time.monotonic()-start, 3),
        "upload": "NOT_RUN", "hardware": "NOT_TOUCHED"
    }
    (out / "MANIFEST_COM14.json").write_text(
        json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    print("H_F137_MASTER_SEAL=PASS_REUSED")
    print("H_F137_SLAVE_BUILD=PASS_ARCHIVES_BINARIES_SEALED")
    print("H_F137_UPLOAD=NOT_RUN")

def validate_dual(head, out):
    m.check(head)
    orig = check_master(head, out)
    stage_check(out)
    file = out / "MANIFEST_COM14.json"
    req(file.is_file(), "SLAVE_SEAL_MISSING")
    slave = json.loads(file.read_text(encoding="utf-8"))
    req(slave.get("schema") == SCHEMA and slave.get("head") == head and
        slave.get("master_build_head") == orig["head"] and
        slave.get("port") == "COM14" and slave.get("role") == "SLAVE" and
        slave.get("fqbn") == m.FQBN and slave.get("upload") == "NOT_RUN",
        "SLAVE_SEAL_INCORRECT")
    req(slave.get("sketch_sha256") == source_sha() and
        slave.get("archives") == m.LIBS and
        slave.get("files") == files(out / "build_COM14_SLAVE"),
        "SLAVE_CONTENT_CHANGED")
    print("H_F137_DUAL_SEAL=PASS_MASTER_AND_SLAVE")

def inventory_both():
    from serial.tools import list_ports
    result = usb.validate_ports(list_ports.comports())
    req(result == "COM4_AND_COM14_CH340_DISTINCT", "BOTH_USB_PORTS_REQUIRED")
    print("H_F137_PORTS=PASS_COM4_MASTER_COM14_SLAVE_DISTINCT_CH340")

def upload_both(head, out, cli):
    validate_dual(head, out)
    req(not (out / "UPLOAD_RECEIPT_COM4.json").exists() and
        not (out / "UPLOAD_RECEIPT_COM14.json").exists(),
        "ALREADY_UPLOADED_OR_PARTIAL")
    inventory_both()
    print("AVISO: subida secuencial COM14 Slave luego COM4 Master, sin backup.")
    print("Confirmar banco aislado, Q sin cargas/tension y monitores serie cerrados.")
    typed = input("Escribe exactamente '" + CONSENT + "': ").strip()
    req(typed == CONSENT, "UPLOAD_NOT_CONFIRMED")
    cfg = out / "config/arduino-cli.yaml"
    for port, sketch, folder in (("COM14", SLAVE, "build_COM14_SLAVE"),
                                 ("COM4", m.SKETCH, "build_COM4_MASTER")):
        print("H_F137_UPLOAD_BEGIN=" + port, flush=True)
        m.cmd([cli, "--config-file", cfg, "upload", "--fqbn", m.FQBN,
               "--port", port, "--input-dir", out / folder, sketch],
              log=out / ("upload_" + port + ".log"), timeout=300)
        (out / ("UPLOAD_RECEIPT_" + port + ".json")).write_text(
            json.dumps({"head": head, "port": port, "upload": "COMMAND_SUCCESS",
                        "uart": "WAIT_OPERATOR"}, indent=2) + "\n",
            encoding="utf-8")
        print("H_F137_UPLOAD_" + port + "=COMMAND_SUCCESS_UART_PENDING", flush=True)
    print("H_F137_DUAL_UPLOAD=COMMAND_SUCCESS_2_OF_2_UART_PENDING")

def selftest():
    req(source_sha() != m.sha(m.INO), "SAME_SKETCH_FOR_TWO_BOARDS")
    print("H_F137_DUAL_SELFTEST=PASS_DISTINCT_SKETCHES_NO_USB")
    print("H_F137_UPLOAD_PROTECT=CI_DETACHED_FORBIDDEN")

def main():
    ap = argparse.ArgumentParser(description=__doc__)
    modes = ap.add_mutually_exclusive_group(required=True)
    for flag in ("selftest", "prepare-slave", "check-dual", "upload-both"):
        modes.add_argument("--" + flag, action="store_true")
    ap.add_argument("--expected-head")
    ap.add_argument("--output", type=Path)
    ap.add_argument("--cli", default="arduino-cli")
    ap.add_argument("--ci-detached", action="store_true")
    a = ap.parse_args()
    req(not a.ci_detached or a.selftest or a.prepare_slave, "CI_UPLOAD_FORBIDDEN")
    if a.selftest:
        selftest()
        return
    req(bool(a.expected_head and re.fullmatch(r"[0-9a-f]{40}", a.expected_head)),
        "HEAD_INVALID")
    req(a.output is not None, "OUTPUT_REQUIRED")
    out = a.output.expanduser().resolve()
    if a.prepare_slave:
        prepare_slave(a.expected_head, out, a.cli, a.ci_detached)
    elif a.check_dual:
        validate_dual(a.expected_head, out)
    else:
        upload_both(a.expected_head, out, a.cli)

if __name__ == "__main__":
    try:
        main()
    except (RuntimeError, OSError, ValueError, KeyError, TypeError,
            ImportError, subprocess.TimeoutExpired, json.JSONDecodeError,
            EOFError, KeyboardInterrupt) as e:
        print(str(e) if isinstance(e, RuntimeError) else
              "H_F137_DUAL_REVIEW_STOP_LOCAL_ENVIRONMENT", file=sys.stderr)
        sys.exit(2)
