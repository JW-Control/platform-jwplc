#!/usr/bin/env python3
"""Alpha13 TFT-CLOSURE integrator. No product writes, staging, commit, pull or reset.

Phases: preflight -> normal precompiled build -> physical upload -> unique serial
provenance -> USB-only human visual checkpoint -> regression -> idempotent source
rebuild -> reports. All generated outputs live under TEMP / ignored results.
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
import uuid
from datetime import datetime

REPO = Path(__file__).resolve().parents[3]
BRANCH = "v2.1.0-alpha.13/feature/cleanup-robustness"
FQBN = "jwplc_local:esp32:jwplcbasic"
LIBS = REPO / "JWPLC" / "2.1.0" / "libraries"
TFT = LIBS / "JWPLC_TFT"
CORE_A = REPO / "JWPLC/2.1.0/precompiled/core/JWPLCBASIC/core.a"
DISPLAY_A = LIBS / "JWPLC_Display/src/esp32/libJWPLC_Display.a"
P2A_BACKUP = Path(tempfile.gettempdir()) / "jwplc_a13_tft_pre6_p2a_20261009_172339" / "originals"
PROBE_SRC = REPO / "tools/alpha13/firmware/a13_tft_pre1_startup_baseline_probe/a13_tft_pre1_startup_baseline_probe.ino"
REL = (
    "JWPLC/2.1.0/libraries/JWPLC_TFT/src/JWPLC_TFT.cpp",
    "JWPLC/2.1.0/libraries/JWPLC_TFT/src/tft_setup.h",
    "JWPLC/2.1.0/libraries/JWPLC_TFT/src/esp32/libJWPLC_TFT.a",
)
SHA = {
    "cpp": "494440b00e74e74a7b23975420574e035ef2138fdf5a0cea2fed51c986c29d25",
    "setup": "8fa079444ca130772d3a642eaf814035100bc098032b403c922c458d351ae5e1",
    "archive": "ab73b244c44ebd75d29a4eeb3cd97f5d18c08470f535eb16d55c2fdbf2310ff8",
    "old_archive": "5d860a131811dd9a7eb6fa55f5674b1d78b0de7dfaf8748ce18a60ceed2d3738",
    "old_cpp_blob": "2bdb55d504cfd2a1481ff561d33535d1740bb472",
    "old_setup_blob": "773f8123844f783bfc67c3123150c27f29f45d34",
    "core": "6f328eeb796091070c8d852a2c0e90f71047d8d2b5786fe3cd5079b2fb6ff983",
    "display": "c960d718433e29a40e3cc55bc745c9a2e121ee1ee598ec72c04872327592dc02",
    "backend_cpp": "01ed6edb0530d38b94ddeac079ba81633aa21d77d049b12da21a37f4bec69ee1",
    "backend_h": "b1b2789ace7ac8fd4a4c414054757d91e6e62649a23d74226ea28ceb8d6f4462",
    "backend_init": "e21cae2ac84285dc0e77648eca67ca753f41f7da3c271594ede750b725136c10",
    "patched_init": "44873be82fe836084934a328df77f098e9ab88d212dd1d570da5e8aac74671bc",
    "instrumented_cpp": "3fe3c601830aad09e579e8fec1223c6a0e9b6a31a82f491743fbe166a5b16350",
}
EXPECTED_ARCHIVE_BYTES = 1091990
RUN_ID = datetime.now().strftime("%Y%m%d_%H%M%S") + "_" + uuid.uuid4().hex[:8]
RUN_ROOT = REPO / "tools/alpha13/results" / ("tft_closure_" + RUN_ID)
TEMP_ROOT = Path(tempfile.gettempdir()) / ("jwplc_a13_tft_closure_" + RUN_ID)
PHASE = "INIT"
RESULT = {"hito": "TFT-CLOSURE", "status": "REVIEW", "run_id": RUN_ID,
          "phases": {}, "paths": {"run": str(RUN_ROOT), "temp": str(TEMP_ROOT)},
          "product_commit": False, "product_mutated": False}


class GateStop(RuntimeError):
    def __init__(self, reason: str, category: str = "PRECONDITION"):
        super().__init__(reason)
        self.category = category


def require(ok: bool, why: str, cat: str = "PRECONDITION") -> None:
    if not ok:
        raise GateStop(why, cat)


def h(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def emit(key: str, value: object) -> None:
    print(f"{key}={value}", flush=True)


def stage(phase: str, state: str) -> None:
    global PHASE
    PHASE = phase
    RESULT["phases"][phase] = state
    emit("PHASE", phase + "_" + state)
    save()


def save() -> None:
    RUN_ROOT.mkdir(parents=True, exist_ok=True)
    (RUN_ROOT / "MANIFEST.json").write_text(json.dumps(RESULT, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    lines = ["HITO=TFT-CLOSURE", f"STATUS={RESULT['status']}", f"CURRENT_PHASE={PHASE}",
             f"RUN_ID={RUN_ID}", f"RUN_ROOT={RUN_ROOT}", "PRODUCT_COMMIT=NO",
             "PRODUCT_MUTATION_BY_GATE=NO"]
    for name, value in RESULT["phases"].items():
        lines.append(f"{name}={value}")
    if "reason" in RESULT:
        lines.append("REASON=" + str(RESULT["reason"]))
    if "category" in RESULT:
        lines.append("CLASSIFICATION=" + str(RESULT["category"]))
    (RUN_ROOT / "SUMMARY.log").write_text("\n".join(lines) + "\n", encoding="utf-8")


def run(argv: list[str], label: str, cwd: Path | None = None, check: bool = True) -> str:
    log = RUN_ROOT / (label + ".log")
    p = subprocess.run(argv, cwd=str(cwd or REPO), stdout=subprocess.PIPE,
                       stderr=subprocess.STDOUT, errors="backslashreplace", encoding="utf-8")
    text = p.stdout or ""
    log.write_text("ARGV=" + repr(argv) + "\nEXIT=" + str(p.returncode) + "\n" + text, encoding="utf-8")
    emit(label + "_EXIT", p.returncode)
    if check and p.returncode:
        emit(label + "_LAST_LINES", "\\n".join(text.splitlines()[-8:]))
        raise GateStop(label + "_EXIT_" + str(p.returncode), "ENVIRONMENT" if label.startswith("CLI_") else "PRODUCT_OR_ENVIRONMENT")
    return text


def git(*args: str) -> str:
    p = subprocess.run(["git", "-C", str(REPO), *args], capture_output=True, text=True,
                       errors="replace")
    require(p.returncode == 0, "GIT_FAILED:" + " ".join(args) + ":" + p.stderr[-150:], "ENVIRONMENT")
    return p.stdout.strip()


def git_raw(*args: str) -> bytes:
    p = subprocess.run(["git", "-C", str(REPO), *args], capture_output=True)
    require(p.returncode == 0, "GIT_RAW_FAILED:" + " ".join(args), "ENVIRONMENT")
    return p.stdout


def verify_product() -> None:
    stage("PREFLIGHT", "RUNNING")
    require(git("branch", "--show-current") == BRANCH, "WRONG_BRANCH")
    head = git("rev-parse", "HEAD")
    RESULT["head_local"] = head
    git("merge-base", "--is-ancestor", "2a2d2943db22c69d2da6cc8377f0341a5e6f8ddb", "HEAD")
    staged = [x for x in git("diff", "--cached", "--name-only").splitlines() if x]
    require(not staged, "STAGED_FILES_PRESENT:" + repr(staged))
    dirty = [x for x in git("diff", "--name-only").splitlines() if x]
    require(set(dirty) == set(REL) and len(dirty) == len(REL), "UNEXPECTED_TRACKED_DIFF:" + repr(dirty))
    require(not git("diff", "--check"), "DIFF_CHECK_FAILED")
    for idx, rel in enumerate(REL):
        path = REPO / rel
        require(path.is_file(), "MISSING_PRODUCT:" + rel)
        expected = (SHA["cpp"], SHA["setup"], SHA["archive"])[idx]
        require(h(path) == expected, "PRODUCT_SHA_MISMATCH:" + rel)
        emit(("CPP", "SETUP", "TFT_ARCHIVE")[idx] + "_SHA256", expected)
        backup = P2A_BACKUP / rel
        require(backup.is_file(), "P2A_ORIGINAL_BACKUP_MISSING:" + rel)
        if idx == 2:
            require(h(backup) == SHA["old_archive"], "P2A_OLD_ARCHIVE_BACKUP_MISMATCH")
        else:
            expected_old = git_raw("show", "HEAD:" + rel)
            original = backup.read_bytes()
            require(original == expected_old or original.replace(b"\r\n", b"\n") == expected_old,
                    "P2A_BACKUP_NOT_MATCHING_HEAD:" + rel)
            blob = git("rev-parse", "HEAD:" + rel)
            require(blob == (SHA["old_cpp_blob"], SHA["old_setup_blob"])[idx], "OLD_GIT_BLOB_IDENTITY_MISMATCH:" + rel)
    require((REPO / REL[2]).stat().st_size == EXPECTED_ARCHIVE_BYTES, "TFT_ARCHIVE_SIZE_MISMATCH")
    for path, name in ((CORE_A, "core"), (DISPLAY_A, "display")):
        require(path.is_file() and h(path) == SHA[name], "INHERITED_ARCHIVE_CHANGED:" + name)
    props = (TFT / "library.properties").read_text(encoding="utf-8")
    require("precompiled=full" in props and "dot_a_linkage=true" in props, "PRECOMPILED_PROPERTIES_MISSING")
    require(PROBE_SRC.is_file(), "STARTUP_PROBE_SOURCE_NOT_FOUND")
    snapshot = TEMP_ROOT / "product_snapshot"
    for rel in REL:
        target = snapshot / rel
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(REPO / rel, target)
    RESULT["snapshot"] = str(snapshot)
    RESULT["p2a_original_backup"] = str(P2A_BACKUP)
    RESULT["dirty_paths"] = dirty
    # Reading an already fetched remote-tracking ref is not a fetch/pull/reset.
    p = subprocess.run(["git", "-C", str(REPO), "rev-parse", "--verify", "refs/remotes/origin/" + BRANCH], capture_output=True, text=True)
    RESULT["remote_tracking_head_local_cache"] = p.stdout.strip() if p.returncode == 0 else "UNAVAILABLE"
    stage("PREFLIGHT", "PASS")


def verify_unchanged() -> None:
    dirty = git("diff", "--name-only").splitlines()
    require(set(dirty) == set(REL) and len(dirty) == len(REL), "FINAL_DIFF_CHANGED")
    require(not git("diff", "--cached", "--name-only"), "STAGE_MUTATED")
    require(not git("diff", "--check"), "FINAL_DIFF_CHECK_FAILED")
    for rel, key in zip(REL, ("cpp", "setup", "archive")):
        require(h(REPO / rel) == SHA[key], "PRODUCT_MUTATED_BY_GATE:" + rel)
    require(h(CORE_A) == SHA["core"] and h(DISPLAY_A) == SHA["display"], "DEPENDENCY_ARCHIVE_MUTATED")


def toolchain(cli: str | None) -> str:
    stage("TOOLCHAIN", "RUNNING")
    resolved = str(Path(cli).resolve()) if cli and Path(cli).is_file() else None
    if not resolved:
        path = Path(r"C:\Program Files\Arduino PLC IDE Tools\arduino-cli.exe")
        resolved = str(path) if path.is_file() else shutil.which("arduino-cli")
    require(bool(resolved), "ARDUINO_CLI_NOT_FOUND", "ENVIRONMENT")
    output = run([resolved, "version"], "CLI_VERSION")
    require(re.search(r"Version:\s*1\.0\.2\b", output) is not None, "CLI_VERSION_NOT_BASELINED_1_0_2", "ENVIRONMENT")
    try:
        import serial  # type: ignore
    except ImportError as exc:
        raise GateStop("PYSERIAL_MISSING:" + str(exc), "ENVIRONMENT") from exc
    RESULT["pyserial_version"] = serial.__version__
    RESULT["cli"] = resolved
    stage("TOOLCHAIN", "PASS")
    return resolved


def port_check(port: str | None) -> str:
    from serial.tools import list_ports  # type: ignore
    stage("SERIAL_PORT", "RUNNING")
    ports = list(list_ports.comports())
    candidates = [p for p in ports if p.device.upper() != "COM1"]
    port_table = [{"device": p.device, "description": p.description, "vid": p.vid, "pid": p.pid} for p in ports]
    RESULT["ports"] = port_table
    emit("PORTS_VISIBLE", ",".join(p.device for p in ports))
    if port:
        found = [p for p in ports if p.device.upper() == port.upper()]
        require(len(found) == 1, "EXPLICIT_PORT_NOT_FOUND:" + port, "ENVIRONMENT")
        selected = found[0]
    else:
        require(len(candidates) == 1, "PORT_SELECTION_AMBIGUOUS_USE_EXPLICIT_COM", "PRECONDITION")
        selected = candidates[0]
    RESULT["selected_port"] = selected.device
    emit("SELECTED_PORT", selected.device)
    if selected.vid is not None and selected.pid is not None:
        emit("USB_VID_PID", f"{selected.vid:04X}:{selected.pid:04X}")
    stage("SERIAL_PORT", "PASS")
    return selected.device


def normal_compile(cli: str, sketch: Path, label: str, temp_lib: Path | None = None) -> Path:
    build = TEMP_ROOT / ("build_" + label)
    args = [cli, "compile", "--fqbn", FQBN, "-j", "0", "-v", "--clean", "--build-path", str(build),
            "--library", str(temp_lib or TFT), "--libraries", str(LIBS), str(sketch)]
    output = run(args, "COMPILE_" + label)
    expected_lib = (temp_lib or TFT).resolve()
    found = re.findall(r"^Using library JWPLC_TFT at version .+ in folder: (.+)\s*$", output, flags=re.M)
    require(len(found) == 1 and Path(found[0].strip()).resolve() == expected_lib,
            label + "_WRONG_TFT_LIBRARY", "HARNESS")
    require(re.search(r"Using precompiled library .*JWPLC_TFT", output) is not None,
            label + "_NOT_PRECOMPILED", "PRODUCT")
    require(re.search(r"^Using library TFT_eSPI at version", output, re.M) is None,
            label + "_EXTERNAL_TFT_ESPI_SELECTED", "PRODUCT")
    require("Using core 'jwcontrol_precompiled_stub'" in output and
            re.search(r"[\\/]precompiled[\\/]core[\\/]JWPLCBASIC[\\/]core\.a", output) is not None,
            label + "_CORE_PRECOMPILED_NOT_LINKED", "HARNESS")
    require(not list(build.rglob("JWPLC_TFT.cpp.o")) and not list(build.rglob("TFT_eSPI.cpp.o")),
            label + "_SOURCE_OBJECT_FOUND", "PRODUCT")
    archive = (temp_lib or TFT) / "src/esp32/libJWPLC_TFT.a"
    require(archive.is_file(), label + "_ARCHIVE_FILE_MISSING", "PRODUCT")
    if temp_lib is None:
        require(h(archive) == SHA["archive"], label + "_ARCHIVE_IDENTITY_DRIFT", "PRODUCT")
    # The archive itself is the proof of the version used by normal Arduino linking;
    # paired with selected library path, archive SHA and zero source TUs.
    emit(label + "_NORMAL_ARCHIVE", "PASS")
    return build


def build_unique_probe(cli: str) -> tuple[Path, str, str]:
    stage("BUILD_PHYSICAL", "RUNNING")
    sketch_dir = TEMP_ROOT / "TFT_Closure_Probe"
    sketch_dir.mkdir(parents=True, exist_ok=False)
    src = PROBE_SRC.read_text(encoding="utf-8")
    anchor = 'Serial.println("A13_TFT_PRE1_RESULT=BEGIN");'
    require(src.count(anchor) == 1, "PROVENANCE_INSERT_ANCHOR_NOT_UNIQUE", "HARNESS")
    token = RUN_ID + "_" + uuid.uuid4().hex
    new = (anchor + "\n    Serial.println(\"TFT_CLOSURE_RUN_ID=" + token + "\");"
           + "\n    Serial.println(\"TFT_CLOSURE_ARCHIVE_SHA256=" + SHA["archive"] + "\");")
    (sketch_dir / "TFT_Closure_Probe.ino").write_text(src.replace(anchor, new, 1), encoding="utf-8")
    build = normal_compile(cli, sketch_dir, "PHYSICAL")
    apps = list(build.rglob("*.ino.bin"))
    require(len(apps) == 1, "APP_BINARY_CARDINALITY:" + str(len(apps)), "HARNESS")
    app_sha = h(apps[0])
    RESULT["app_binary"] = str(apps[0])
    RESULT["app_binary_sha256"] = app_sha
    RESULT["runtime_token"] = token
    RESULT["runtime_archive_sha256"] = SHA["archive"]
    emit("APP_BINARY_SHA256", app_sha)
    stage("BUILD_PHYSICAL", "PASS")
    return build, token, app_sha


def capture_serial(port: str, token: str, label: str, timeout_s: float = 18) -> dict[str, str]:
    import serial  # type: ignore
    stage(label, "RUNNING")
    start = time.monotonic()
    received: list[str] = []
    ser = None
    while time.monotonic() - start < timeout_s:
        try:
            ser = serial.Serial(port, 115200, timeout=0.2)
            break
        except (serial.SerialException, OSError):
            time.sleep(0.35)
    require(ser is not None, "SERIAL_PORT_NOT_REOPENED", "ENVIRONMENT")
    block: list[str] = []
    recording = False
    data: dict[str, str] = {}
    try:
        while time.monotonic() - start < timeout_s:
            raw = ser.readline()
            if not raw:
                continue
            s = raw.decode("utf-8", errors="backslashreplace").strip()
            if not s:
                continue
            if len(received) < 4000:
                received.append(s)
            if s == "A13_TFT_PRE1_RESULT=BEGIN":
                recording = True
                block = []
                continue
            if not recording:
                continue
            if s == "A13_TFT_PRE1_RESULT=END":
                recording = False
                fields: dict[str, str] = {}
                for entry in block:
                    if "=" in entry:
                        k, v = entry.split("=", 1)
                        if k in fields:
                            fields = {}
                            break
                        fields[k] = v
                if fields.get("TFT_CLOSURE_RUN_ID") != token or fields.get("TFT_CLOSURE_ARCHIVE_SHA256") != SHA["archive"]:
                    continue
                data = fields
                break
            block.append(s)
    finally:
        ser.close()
        (RUN_ROOT / (label + ".log")).write_text("\n".join(received) + "\n", encoding="utf-8")
    require(data, "UNIQUE_RUNTIME_PROVENANCE_NOT_SEEN", "HARDWARE_OR_ENVIRONMENT")
    need = {"DISPLAY_READY": "YES", "IO_READY": "YES", "TFT_RST_OUTPUT_ENABLE": "YES",
            "TFT_RST_OUTPUT_LATCH": "HIGH", "TFT_CS_OUTPUT_ENABLE": "YES", "TFT_CS_OUTPUT_LATCH": "HIGH"}
    for k, v in need.items():
        require(data.get(k) == v, "RUNTIME_CONTRACT_FAILED:" + k, "PRODUCT_OR_HARDWARE")
    try:
        setup_ms = int(data["SETUP_ENTRY_MS"])
        uptime_ms = int(data["UPTIME_MS"])
        require(0 <= setup_ms <= uptime_ms, "SERIAL_RUNTIME_CLOCK_INVALID", "HARNESS")
    except (ValueError, KeyError) as exc:
        raise GateStop("SERIAL_RUNTIME_CLOCK_MISSING_OR_INVALID", "HARNESS") from exc
    RESULT[label + "_fields"] = data
    stage(label, "PASS")
    return data


def physical(cli: str, port: str, build: Path, token: str) -> None:
    stage("PHYSICAL_UPLOAD", "RUNNING")
    run([cli, "upload", "--fqbn", FQBN, "--port", port, "--input-dir", str(build),
         str(TEMP_ROOT / "TFT_Closure_Probe")], "UPLOAD_PHYSICAL")
    stage("PHYSICAL_UPLOAD", "PASS")
    capture_serial(port, token, "SERIAL_FIRST_BOOT")
    print("\n=== PUNTO DE OBSERVACION FISICA ===", flush=True)
    print("Apaga/desconecta de forma segura otras alimentaciones y RJ45 si aplica; deja USB-only.")
    print("Desconecta y vuelve a conectar USB. Observa el arranque COMPLETO y graba video/capturas.")
    response = input("Escribe REINICIADO cuando hayas realizado el ciclo USB-only (o ABORTAR): ").strip().upper()
    require(response == "REINICIADO", "USB_ONLY_POWER_CYCLE_NOT_CONFIRMED", "PRECONDITION")
    capture_serial(port, token, "SERIAL_AFTER_USB_CYCLE", 25)
    print("\nEl dibujo escalonado de IDLE NO cuenta por si mismo como defecto GRAM.")
    white = input("¿Aparecio fondo blanco irregular persistente/flicker? (NO/SI): ").strip().upper()
    clean = input("¿La pantalla queda limpia, IDLE coherente y sin boot-loop? (SI/NO): ").strip().upper()
    require(white in ("SI", "NO") and clean in ("SI", "NO"), "INVALID_VISUAL_RESPONSE")
    RESULT["visual_user_report"] = {"white_problem": white, "idle_clean": clean,
                                   "usb_only_power_cycle": "USER_CONFIRMED",
                                   "video_review_by_assistant": "PENDING"}
    if white == "SI" or clean == "NO":
        raise GateStop("VISUAL_REGRESSION_REPORTED_STOP", "PRODUCT_OR_HARDWARE")
    stage("VISUAL_USER", "REPORTED_PASS_NEEDS_VIDEO_REVIEW")
    verify_unchanged()


def regress(cli: str) -> None:
    stage("REGRESSION", "RUNNING")
    cases = (
        ("IDLE_STATUS", LIBS / "JWPLC_Display/examples/01.Display_IDLE_Status"),
        ("HMI_FIELDS", LIBS / "JWPLC_Display/examples/02.Display_HMI_Fields"),
        ("IDLE_MODES", LIBS / "JWPLC_Display/examples/Display_Idle_Return_Modes"),
        ("LOGIC_RUNTIME_UI", LIBS / "JWPLC_LogicRuntime_UI/examples/JWPLC_LogicRuntime_UI_Home"),
    )
    for name, sketch in cases:
        require(sketch.is_dir(), "REGRESSION_SKETCH_MISSING:" + name, "HARNESS")
        normal_compile(cli, sketch, "REG_" + name)
    RESULT["regressions"] = [x[0] for x in cases]
    verify_unchanged()
    stage("REGRESSION", "PASS")


def backend_path(explicit: str | None) -> Path:
    if explicit:
        return Path(explicit).expanduser()
    env = os.getenv("JWPLC_TFT_ESPI_BACKEND", "")
    if env:
        return Path(env).expanduser()
    # The project standard maintainer copy (not the Arduino15 installed core).
    return Path.home() / "Documentos/Programacion/Arduino/libraries/TFT_eSPI"


def archiver() -> str:
    base = Path(os.getenv("LOCALAPPDATA", "")) / "Arduino15/packages/jwplc_local/tools/esp-x32"
    prefer = base / "2601/bin/xtensa-esp32-elf-gcc-ar.exe"
    if prefer.is_file():
        return str(prefer)
    found = list(base.rglob("xtensa-esp32-elf-gcc-ar.exe")) if base.is_dir() else []
    require(len(found) == 1, "ESP32_ARCHIVER_UNAVAILABLE_OR_AMBIGUOUS", "ENVIRONMENT")
    return str(found[0])


def make_rebuild_source(backend: Path) -> tuple[Path, Path, Path, dict[str, str]]:
    src_lib = TEMP_ROOT / "rebuilt" / "JWPLC_TFT"
    backend_copy = TEMP_ROOT / "rebuilt" / "TFT_eSPI"
    src = src_lib / "src"
    src.mkdir(parents=True)
    for file in ("JWPLC_TFT.cpp", "JWPLC_TFT.h", "tft_setup.h"):
        shutil.copy2(TFT / "src" / file, src / file)
    props = ("name=JWPLC_TFT\nversion=0.1.0-alpha13-closure-source\n"
             "author=JW Control\nmaintainer=JW Control\n"
             "sentence=Fuente temporal de mantenimiento ST7789.\n"
             "paragraph=No es un artefacto distribuible al usuario final.\n"
             "category=Display\narchitectures=esp32\n"
             "includes=JWPLC_TFT.h\ndepends=TFT_eSPI,SPI\n")
    (src_lib / "library.properties").write_text(props, encoding="utf-8")
    shutil.copytree(backend, backend_copy)
    orig = (backend / "TFT_Drivers/ST7789_Init.h").read_bytes().decode("utf-8")
    pattern = re.compile(r"(?m)^(?P<indent>\s*)writecommand\(ST7789_DISPON\);\s*//\s*Display on\s*\r?\n(?P=indent)delay\(120\);")
    require(len(pattern.findall(orig)) == 2, "BACKEND_PATCH_ANCHOR_COUNT_NOT_2", "HARNESS")
    patched = None
    for eol in ("\n", "\r\n"):
        def replace(m: re.Match[str]) -> str:
            ind = m.group("indent")
            return eol.join((ind + "#ifndef JWPLC_TFT_DEFER_DISPON",
                             ind + "writecommand(ST7789_DISPON);    // Display on",
                             ind + "delay(120);", ind + "#endif"))
        maybe = pattern.sub(replace, orig).encode("utf-8")
        if hashlib.sha256(maybe).hexdigest() == SHA["patched_init"]:
            patched = maybe
            break
    require(patched is not None, "BACKEND_PATCH_SHA_NOT_REPRODUCED", "HARNESS")
    (backend_copy / "TFT_Drivers/ST7789_Init.h").write_bytes(patched)
    source_backend_cpp = backend_copy / "TFT_eSPI.cpp"
    raw = source_backend_cpp.read_bytes()
    include = b'#include "TFT_eSPI.h"'
    require(raw.count(include) == 1, "BACKEND_GUARD_ANCHOR_NOT_UNIQUE", "HARNESS")
    eol = os.linesep.encode("ascii")
    guard = ("#if !defined(ST7789_DRIVER) || !defined(JWPLC_TFT_DEFER_DISPON)" +
             os.linesep + "#error A13_TFT_PRE6_BACKEND_CONFIGURATION_NOT_PROPAGATED" +
             os.linesep + "#endif").encode("ascii")
    instrumented = raw.replace(include, include + eol + guard, 1)
    source_backend_cpp.write_bytes(instrumented)
    require(h(source_backend_cpp) == SHA["instrumented_cpp"], "BACKEND_SOURCE_GUARD_SHA_MISMATCH", "HARNESS")
    test_dir = TEMP_ROOT / "rebuilt" / "TFT_Rebuild_Probe"
    test_dir.mkdir()
    shutil.copy2(PROBE_SRC, test_dir / "TFT_Rebuild_Probe.ino")
    shutil.copy2(src / "tft_setup.h", test_dir / "tft_setup.h")
    return src_lib, backend_copy, test_dir, {"source_cpp": h(src / "JWPLC_TFT.cpp"),
                                            "source_setup": h(src / "tft_setup.h"),
                                            "patched_init": h(backend_copy / "TFT_Drivers/ST7789_Init.h"),
                                            "guarded_backend_cpp": h(source_backend_cpp)}


def rebuild(cli: str, root: Path) -> None:
    stage("REBUILD_RECIPE", "RUNNING")
    require(root.is_dir(), "BACKEND_PATH_MISSING:" + str(root), "ENVIRONMENT")
    original = (("TFT_eSPI.cpp", "backend_cpp"), ("TFT_eSPI.h", "backend_h"),
                ("TFT_Drivers/ST7789_Init.h", "backend_init"))
    for rel, key in original:
        require((root / rel).is_file() and h(root / rel) == SHA[key],
                "BACKEND_ORIGINAL_SHA_CHANGED:" + rel, "ENVIRONMENT")
    require('#define TFT_ESPI_VERSION "2.5.43"' in (root / "TFT_eSPI.h").read_text(encoding="utf-8"),
            "BACKEND_VERSION_NOT_2_5_43", "ENVIRONMENT")
    temp_lib, temp_backend, sketch, src_manifest = make_rebuild_source(root)
    require(src_manifest["source_cpp"] == SHA["cpp"] and src_manifest["source_setup"] == SHA["setup"] and
            src_manifest["patched_init"] == SHA["patched_init"], "REBUILD_SOURCE_SHA_MISMATCH", "HARNESS")
    build = TEMP_ROOT / "build_SOURCE_RECIPE"
    cmd = [cli, "compile", "--fqbn", FQBN, "-j", "0", "-v", "--clean", "--build-path", str(build),
           "--library", str(temp_lib), "--library", str(temp_backend), "--libraries", str(LIBS), str(sketch)]
    output = run(cmd, "SOURCE_REBUILD")
    for lib, path in (("JWPLC_TFT", temp_lib), ("TFT_eSPI", temp_backend)):
        found = re.findall(r"^Using library " + re.escape(lib) + r" at version .+ in folder: (.+)\s*$", output, re.M)
        require(len(found) == 1 and Path(found[0].strip()).resolve() == path.resolve(),
                "REBUILD_SELECTED_WRONG_LIB:" + lib, "HARNESS")
    require("Using core 'jwcontrol_precompiled_stub'" in output and
            re.search(r"[\\/]precompiled[\\/]core[\\/]JWPLCBASIC[\\/]core\.a", output) is not None,
            "SOURCE_REBUILD_CORE_CONTRACT_FAILED", "HARNESS")
    objects = {}
    for name in ("JWPLC_TFT.cpp.o", "TFT_eSPI.cpp.o"):
        found = list(build.rglob(name))
        require(len(found) == 1, "SOURCE_REBUILD_OBJECT_COUNT:" + name + ":" + str(len(found)), "HARNESS")
        objects[name] = found[0]
    (temp_lib / "src/esp32").mkdir(parents=True)
    temp_archive = temp_lib / "src/esp32/libJWPLC_TFT.a"
    ar = archiver()
    run([ar, "crs", str(temp_archive), str(objects["JWPLC_TFT.cpp.o"]), str(objects["TFT_eSPI.cpp.o"])], "ARCHIVE_CREATE")
    members = run([ar, "t", str(temp_archive)], "ARCHIVE_LIST").splitlines()
    members = [x.strip() for x in members if x.strip()]
    require(len(members) == 2 and set(members) == set(objects), "REBUILT_ARCHIVE_MEMBERS_MISMATCH", "HARNESS")
    extract = TEMP_ROOT / "archive_extracted"
    extract.mkdir()
    run([ar, "x", str(temp_archive)], "ARCHIVE_EXTRACT", cwd=extract)
    obj_sha = {name: h(path) for name, path in objects.items()}
    for name, original_sha in obj_sha.items():
        require(h(extract / name) == original_sha, "ARCHIVE_MEMBER_BIT_PARITY_FAILED:" + name, "HARNESS")
    src_manifest.update({"object_sha256": obj_sha, "archive_sha256": h(temp_archive),
                         "archive_bytes": temp_archive.stat().st_size,
                         "member_parity": "PASS"})
    RESULT["rebuild_manifest"] = src_manifest
    # Use the rebuilt archive as a normal precompiled consumer, never source fallback.
    shutil.copy2(TFT / "library.properties", temp_lib / "library.properties")
    normal_compile(cli, LIBS / "JWPLC_Display/examples/01.Display_IDLE_Status", "REBUILT_ARCHIVE", temp_lib)
    verify_unchanged()
    for rel, key in original:
        require(h(root / rel) == SHA[key], "GLOBAL_BACKEND_WAS_MUTATED:" + rel, "HARNESS")
    stage("REBUILD_RECIPE", "PASS")


def commit_plan() -> None:
    stage("COMMIT_PLAN", "RUNNING")
    text = """# TFT-CLOSURE — propuesta de commit, NO ejecutado

Resultado automatizado sujeto a revision humana de video/capturas y de los logs.
No hacer git pull/reset/checkout/clean sobre el worktree P2A.

Producto (solo estos tres paths, ya modificados por P2A):
"""
    text += "\n".join("- `" + rel + "`" for rel in REL)
    text += """

Commit propuesto (tras autorizacion expresa):
`fix(tft): limpiar GRAM antes de habilitar ST7789 en arranque`

Antes de ejecutarlo: auditar MANIFEST.json / SUMMARY.log, evidencia visual,
receta de rebuild, regresiones, actualizar ALPHA13_STATUS y checklist.
Verificar git diff --check y staged set exactamente igual a los tres productos
+ documentacion/harness que se haya autorizado explicitamente.
Este gate nunca stagea ni realiza commits ni PR ni publica.
"""
    (RUN_ROOT / "COMMIT_PLAN.md").write_text(text, encoding="utf-8")
    stage("COMMIT_PLAN", "PREPARED_NOT_AUTHORIZED")


def self_test() -> None:
    # Tests without the repository, port, Arduino, hardware or mutation.
    anchor = 'Serial.println("A13_TFT_PRE1_RESULT=BEGIN");'
    assert anchor.count("RESULT=BEGIN") == 1
    code = anchor + '\n    Serial.println("TFT_CLOSURE_RUN_ID=TOKEN");'
    assert code.count("TFT_CLOSURE_RUN_ID=TOKEN") == 1
    assert len(set(REL)) == 3
    assert all(len(SHA[n]) == 64 for n in ("cpp", "setup", "archive", "core", "display", "patched_init"))
    print("A13_TFT_CLOSURE_SELF_TEST=PASS")


def main() -> int:
    ap = argparse.ArgumentParser(description="JWPLC Alpha13 TFT-CLOSURE")
    ap.add_argument("--serial-port", default=None, help="Explicit connected JWPLC COM port (e.g. COM4)")
    ap.add_argument("--arduino-cli", default=None)
    ap.add_argument("--backend", default=None, help="Original verified TFT_eSPI 2.5.43 maintainer directory")
    ap.add_argument("--self-test", action="store_true")
    args = ap.parse_args()
    if args.self_test:
        self_test()
        return 0
    TEMP_ROOT.mkdir(parents=True, exist_ok=False)
    save()
    emit("RUN_ROOT", RUN_ROOT)
    emit("TEMP_ROOT", TEMP_ROOT)
    try:
        verify_product()
        cli = toolchain(args.arduino_cli)
        port = port_check(args.serial_port)
        build, token, _ = build_unique_probe(cli)
        physical(cli, port, build, token)
        regress(cli)
        rebuild(cli, backend_path(args.backend))
        commit_plan()
        RESULT["status"] = "PROVISIONAL_PASS_AWAITING_VIDEO_AND_COMMIT_AUTHORIZATION"
        RESULT["reason"] = "PHYSICAL_AND_REGRESSIONS_AND_MAINTAINER_REBUILD_PASSED"
        RESULT["category"] = "NO_FAILURE"
        verify_unchanged()
        save()
        emit("STATUS", RESULT["status"])
        emit("SUMMARY", RUN_ROOT / "SUMMARY.log")
        return 0
    except (GateStop, Exception, KeyboardInterrupt) as exc:
        category = exc.category if isinstance(exc, GateStop) else "HARNESS"
        RESULT["status"] = "REVIEW"
        RESULT["reason"] = str(exc) if str(exc) else type(exc).__name__
        RESULT["category"] = category
        stage(PHASE, "REVIEW")
        try:
            verify_unchanged()
            RESULT["product_identity_preserved"] = True
        except Exception as integrity_exc:
            RESULT["product_identity_preserved"] = False
            RESULT["integrity_exception"] = str(integrity_exc)
        save()
        emit("STATUS", RESULT["status"])
        emit("CLASSIFICATION", category)
        emit("REASON", RESULT["reason"])
        emit("SUMMARY", RUN_ROOT / "SUMMARY.log")
        return 3 if RESULT.get("product_identity_preserved") else 5


if __name__ == "__main__":
    sys.exit(main())
