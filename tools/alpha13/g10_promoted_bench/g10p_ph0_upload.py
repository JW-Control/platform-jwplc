#!/usr/bin/env python3
"""Alpha13 G10-PH0: preparación y upload físico del package promovido, sin MAC/backup.

Crea binarios y huellas fuera del checkout; solo abre COM al pedir --upload-*.
No modifica el package, la flash de la otra placa ni la configuración Arduino global.
"""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import time

ROOT = Path(__file__).resolve().parents[3]
BENCH = Path(__file__).resolve().parent
BRANCH = "codex/alpha13-g10-promote-functional-20261019"
FQBN = "jwplc_g10ph0:esp32:jwplcbasic"
ROLES = {
    "COM14_SLAVE": ("COM14", "G10P_COM14_Slave"),
    "COM4_MASTER": ("COM4", "G10P_COM4_Master"),
}


def require(ok: bool, reason: str) -> None:
    if not ok:
        raise RuntimeError("G10P_PH0_REVIEW_STOP_" + reason)


def run(cmd: list[str], timeout: int = 900) -> str:
    p = subprocess.run(cmd, cwd=ROOT, capture_output=True, text=True,
                       errors="replace", timeout=timeout)
    if p.returncode:
        # No guardar ni reproducir logs completos de esptool: pueden mostrar MAC.
        raise RuntimeError("G10P_PH0_COMMAND_FAILED_" +
                           str(p.returncode) + "_" + cmd[-1])
    return (p.stdout or "") + "\n" + (p.stderr or "")


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def guard(head: str, allow_ci_detached: bool = False) -> None:
    require(run(["git", "rev-parse", "HEAD"]).strip() == head, "HEAD")
    branch = run(["git", "branch", "--show-current"]).strip()
    require(branch == BRANCH or (allow_ci_detached and branch == ""), "BRANCH")
    require(not run(["git", "status", "--porcelain",
                     "--untracked-files=no"]).strip(), "DIRTY")
    check_cmd = [sys.executable, "-B", str(BENCH / "g10p_dual_build.py"),
                 "--expected-head", head, "--check"]
    if allow_ci_detached and branch == "":
        check_cmd.append("--ci-detached")
    run(check_cmd)


def artifacts(out: Path, manifest: dict) -> dict:
    stage = out / "sketchbook/hardware/jwplc_g10ph0/esp32"
    require(manifest.get("status") == "PASS_BOTH_BOARDS_REAL_G10_PACKAGE",
            "BUILD_STATUS")
    require(manifest.get("fqbn") == FQBN, "FQBN")
    require(manifest.get("upload") == "NOT_RUN", "BUILD_WAS_UPLOADED")
    require(sha(stage / "libraries/JWPLC_Ethernet/src/utility/w5100.cpp") ==
            manifest.get("f1_sha256"), "F1_CHANGED")
    for name, expected in manifest.get("frozen_archives", {}).items():
        require(sha(stage / name) == expected, "FROZEN_ARCHIVE_" + name)
    require(len(manifest.get("frozen_archives", {})) >= 5,
            "FROZEN_ARCHIVES_MISSING")
    result = {}
    for role, (_, sketch) in ROLES.items():
        entry = manifest["boards"].get(role, {})
        require(entry.get("status") == "PASS_SOURCE_TO_OBJECT_ELF",
                "BUILD_" + role)
        build = out / ("build_" + role)
        # El project_name de Arduino CLI conserva la extensión .ino.
        # No adivinar el nombre desde la carpeta ni aceptar otro ELF.
        prefix = sketch + ".ino"
        elfs = sorted(build.glob("*.elf"))
        require(len(elfs) == 1 and elfs[0].name == prefix + ".elf",
                "ELF_UNEXPECTED_" + role)
        elf = elfs[0]
        require(sha(elf) == entry.get("elf_sha256"),
                "ELF_CHANGED_" + role)
        required_bins = (
            prefix + ".bin",
            prefix + ".bootloader.bin",
            prefix + ".partitions.bin",
        )
        require(all((build / name).is_file() for name in required_bins),
                "UPLOAD_BINARIES_MISSING_" + role)
        binaries = sorted(build.glob(prefix + "*.bin"))
        require(len(binaries) >= len(required_bins),
                "UPLOAD_BINARIES_INCOMPLETE_" + role)
        for obj in entry.get("source_objects", []):
            rel = obj["relative_source"]
            source = stage / rel
            require(source.is_file() and sha(source) == obj["source_sha256"],
                    "SOURCE_CHANGED_" + role)
            matches = list(build.rglob(Path(rel).name + ".o"))
            require(len(matches) == 1 and sha(matches[0]) ==
                    obj["object_sha256"], "OBJECT_CHANGED_" + role)
        require(len(entry.get("source_objects", [])) == 3,
                "SOURCE_OBJECT_COVERAGE_" + role)
        result[role] = {
            "elf": sha(elf),
            "bins": {p.name: sha(p) for p in binaries},
        }
    require((out / "config/arduino-cli.yaml").is_file(),
            "CONFIG_MISSING")
    return result


def check_output(out: Path, head: str) -> dict:
    require(out.is_dir(), "OUTPUT_MISSING")
    require(not out.is_relative_to(ROOT), "OUTPUT_INSIDE_REPO")
    manifest_file = out / "MANIFEST.json"
    require(manifest_file.is_file(), "BUILD_MANIFEST_MISSING")
    m = json.loads(manifest_file.read_text(encoding="utf-8"))
    build_head = m.get("head")
    require(isinstance(build_head, str) and len(build_head) == 40,
            "BUILD_HEAD_INVALID")
    if build_head != head:
        # Permite reutilizar CI/build anterior SOLO si ningun input compilado
        # ha cambiado en Git entre ambos commits; la instrumentación sí puede.
        run(["git", "merge-base", "--is-ancestor", build_head, head])
        changed_inputs = run([
            "git", "diff", "--name-only", build_head, head, "--",
            "JWPLC/",
            "tools/alpha13/g10_promoted_bench/G10P_COM4_Master/",
            "tools/alpha13/g10_promoted_bench/G10P_COM14_Slave/",
            "tools/alpha13/g10_promoted_bench/g10p_dual_build.py",
            "tools/alpha13/gates/a13_g10_promoted_contract.py",
        ]).strip()
        require(not changed_inputs, "COMPILED_INPUT_CHANGED")
    fingerprints = artifacts(out, m)
    return {"head": head, "build_head": build_head, "fqbn": FQBN,
            "artifacts": fingerprints}


def seal_existing(out: Path, head: str) -> None:
    require(not (out / "PH0_SEAL.json").exists(), "SEAL_ALREADY_EXISTS")
    seal = check_output(out, head)
    (out / "PH0_SEAL.json").write_text(
        json.dumps(seal, indent=2) + "\n", encoding="utf-8")
    print("G10P_PH0_BUILD_SEAL=PASS_VERIFIED_NO_USB")
    print("G10P_PH0_OUTPUT=" + str(out))


def prepare(out: Path, head: str, cli: str) -> None:
    guard(head)
    require(not out.is_relative_to(ROOT), "OUTPUT_INSIDE_REPO")
    require(not out.exists(), "OUTPUT_ALREADY_EXISTS")
    run([sys.executable, "-B", str(BENCH / "g10p_dual_build.py"),
         "--expected-head", head, "--output", str(out), "--cli", cli])
    seal_existing(out, head)
    print("G10P_PH0_PREPARE=PASS_TWO_BUILDS_NO_USB")
    print("G10P_PH0_UPLOAD=NOT_RUN")
    print("G10P_PH0_OUTPUT=" + str(out))


def upload(out: Path, head: str, role: str, cli: str) -> None:
    guard(head)
    seal_file = out / "PH0_SEAL.json"
    require(seal_file.is_file(), "SEAL_MISSING")
    original = json.loads(seal_file.read_text(encoding="utf-8"))
    require(check_output(out, head) == original,
            "BINARIES_OR_PLATFORM_CHANGED")
    port, sketch = ROLES[role]
    if role == "COM4_MASTER":
        receipt = out / "PH0_UPLOADED_COM14_SLAVE.json"
        require(receipt.is_file(), "COM14_MUST_PASS_FIRST")
        slave = json.loads(receipt.read_text(encoding="utf-8"))
        require(slave.get("head") == head and
                slave.get("status") == "UPLOAD_COMMAND_SUCCESS",
                "COM14_RECEIPT_INVALID")
    print("G10P_PH0_UPLOAD_START=" + role + "_" + port, flush=True)
    # Arduino CLI utiliza la receta de upload del package staged compilado;
    # --input-dir reutiliza esos mismos binarios, sin recompilar ni erase-all.
    run([cli, "--config-file", str(out / "config/arduino-cli.yaml"),
         "upload", "--fqbn", FQBN, "--port", port,
         "--input-dir", str(out / ("build_" + role)),
         str(BENCH / sketch)], timeout=300)
    (out / ("PH0_UPLOADED_" + role + ".json")).write_text(
        json.dumps({"head": head, "role": role, "port": port,
                    "status": "UPLOAD_COMMAND_SUCCESS",
                    "physical_uart": "PENDING_OPERATOR"},
                   indent=2) + "\n", encoding="utf-8")
    print("G10P_PH0_UPLOAD_COMMAND=SUCCESS_" + role)
    print("G10P_PH0_UART=REQUIRES_OBSERVATION_115200")


def main() -> None:
    ap = argparse.ArgumentParser()
    actions = ap.add_mutually_exclusive_group(required=True)
    actions.add_argument("--check", action="store_true")
    actions.add_argument("--prepare", action="store_true")
    actions.add_argument("--seal-existing", action="store_true",
                         help="Reutiliza build verificado sin recompilar ni USB")
    actions.add_argument("--upload-com14", action="store_true")
    actions.add_argument("--upload-com4", action="store_true")
    ap.add_argument("--expected-head", required=True)
    ap.add_argument("--output", type=Path)
    ap.add_argument("--cli", default="arduino-cli")
    ap.add_argument("--ci-detached", action="store_true",
                    help="Solo preflight read-only en Actions, nunca upload")
    args = ap.parse_args()
    require(not args.ci_detached or args.check or args.seal_existing,
            "CI_DETACHED_READ_ONLY")
    if args.check:
        guard(args.expected_head, allow_ci_detached=args.ci_detached)
        print("G10P_PH0_RUNNER_N0=PASS_NO_USB_NO_MAC_NO_BACKUP")
        return
    if args.seal_existing:
        require(args.output is not None, "OUTPUT_REQUIRED")
        guard(args.expected_head, allow_ci_detached=args.ci_detached)
        seal_existing(args.output.resolve(), args.expected_head)
        return
    if args.prepare:
        dest = (args.output or (Path(tempfile.gettempdir()) /
                ("g10p_ph0_" + args.expected_head[:12] + "_" +
                 str(time.time_ns())))).resolve()
        print("G10P_PH0_OUTPUT=" + str(dest), flush=True)
        prepare(dest, args.expected_head, args.cli)
    else:
        require(args.output is not None, "OUTPUT_REQUIRED")
        role = "COM14_SLAVE" if args.upload_com14 else "COM4_MASTER"
        upload(args.output.resolve(), args.expected_head, role, args.cli)


if __name__ == "__main__":
    try:
        main()
    except (RuntimeError, OSError, ValueError, KeyError, TypeError,
            json.JSONDecodeError, subprocess.TimeoutExpired) as ex:
        print(str(ex), file=sys.stderr)
        sys.exit(2)
