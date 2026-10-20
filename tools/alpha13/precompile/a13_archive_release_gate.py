#!/usr/bin/env python3
"""Gate de promoción reproducible: artifacts CI -> .a real -> consumer Arduino.
Nunca fusiona ramas ni publica paquetes. --adopt muta SOLO cuatro archivos
en el worktree aislado, para commit explícito del job CI con hash verificado.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys

from a13_precompile_benchmark import (
    LIBS, PLATFORM, PROFILES, ROOT, BRANCH, VENDOR,
    CHANGED_PATHS, assert_precompiled, compile_one, stage, digest, require,
    properties,
)

def equivalent_source_hash(path: Path, expected: str) -> bool:
    """Equivalencia de bytes salvo CRLF/LF (Git checkout Windows vs Linux)."""
    raw=path.read_bytes()
    normalized=raw.replace(b"\r\n",b"\n")
    for variant in (raw,normalized,normalized.replace(b"\n",b"\r\n")):
        if hashlib.sha256(variant).hexdigest()==expected:
            return True
    return False

def manifest(inp: Path) -> dict:
    target = inp / "MANIFEST.json"
    require(target.is_file(), "CI_MANIFEST_MISSING_" + str(inp))
    data=json.loads(target.read_text(encoding="utf-8"))
    require(data["status"] == "PASS_TWO_ARCHIVES_TWO_FQBN_PARITY_THREE_PAIRS",
            "SOURCE_CANDIDATE_CI_NOT_PASS")
    require(data["fqbn"] == list(PROFILES), "FQBN_NOT_TWO")
    require(data.get("repetitions") == 3, "BENCHMARK_NOT_THREE_PAIRS")
    for lib in LIBS:
        parity = data.get("fqbn_object_parity", {}).get(lib, {})
        require(parity.get("sources", 0) >= 2 and parity.get("diff_count") == 0,
                "FQBN_OBJECT_MISMATCH_ARCHIVE_FORBIDDEN_" + lib)
    for mode in ("source","archive"):
        row=data["bench"][mode]
        require(all(float(row.get(x,0)) > 0
                    for x in ("cold_s", "warm_s", "basiccore_cold_s", "basiccore_warm_s")),
                "MISSING_BENCHMARK_" + mode)
        require(all(len(row.get(k, [])) == 3 and all(v > 0 for v in row[k])
                    for k in ("cold_samples_s", "warm_samples_s",
                              "basiccore_cold_samples_s", "basiccore_warm_samples_s")),
                "BENCHMARK_SAMPLES_NOT_THREE_" + mode)
    return data

def verify_provenance(data: dict, inp: Path) -> dict[str,Path]:
    found={}
    for lib in LIBS:
        record=data["archives"][lib]
        target=inp / Path(record["stage_path"].replace("\\","/"))
        require(target.is_file(), "ARCHIVE_MISSING_" + lib)
        require(digest(target) == record["sha256"] and
                target.stat().st_size == record["bytes"],
                "ARCHIVE_HASH_SIZE_CHANGED_" + lib)
        lib_root = PLATFORM / "libraries" / lib
        props=lib_root / "library.properties"
        require(equivalent_source_hash(props,record["source_library_properties_sha256"]),
                "LIB_PROPERTIES_DRIFT_" + lib)
        actual=[]
        for obj in record["objects"]:
            src=PLATFORM / Path(obj["src"].replace("\\","/"))
            require(src.is_file() and equivalent_source_hash(src,obj["src_sha256"]),
                    "PRODUCT_SOURCE_CHANGED_" + lib + "_" + src.name)
            actual.append(Path(obj["object"].replace("\\","/")).name)
        require(sorted(record["members"]) == sorted(actual),
                "MEMBER_LIST_MISMATCH_" + lib)
        # ar del anfitrión solo enumera el contenedor, no compila ni re-enlaza.
        if shutil.which("ar"):
            x=subprocess.run(["ar","t",str(target)],
                             capture_output=True,text=True,errors="replace",timeout=20)
            if x.returncode == 0:
                require(sorted(line.strip() for line in x.stdout.splitlines()
                               if line.strip()) == sorted(actual),
                        "RAW_ARCHIVE_MEMBER_LIST_MISMATCH_"+lib)
        found[lib]=target
    return found

def run_gate(ubuntu: Path, windows: Path | None, mode: str,
             out: Path, cli: str) -> None:
    if mode == "adopt":
        branch=subprocess.check_output(["git","branch","--show-current"],cwd=ROOT,
                                       text=True).strip()
        require(branch==BRANCH,"WRONG_ADOPTION_BRANCH")
    dirty=subprocess.check_output(["git","status","--porcelain","--untracked-files=no"],
                                  cwd=ROOT,text=True).strip()
    require(not dirty,"TRACKED_WORKTREE_DIRTY")
    u=manifest(ubuntu)
    src=verify_provenance(u,ubuntu)
    if windows:
        w=manifest(windows)
        require(u["source_sha"] == w["source_sha"],
                "CI_SOURCE_HEAD_DIFFERS_BETWEEN_RUNNERS")
        verify_provenance(w,windows)
        for lib in LIBS:
            left=u["archives"][lib]
            right=w["archives"][lib]
            # Windows puede transformar LF a CRLF en un checkout Git.
            # Ambas huellas originales se validaron contra el mismo
            # source productivo normalizando exclusivamente finales de línea.
            require(sorted(x["src"].replace("\\","/") for x in left["objects"]) ==
                    sorted(x["src"].replace("\\","/") for x in right["objects"]),
                    "CROSS_OS_SOURCE_PATHS_DIFF_" + lib)
            require(sorted(left["members"])==sorted(right["members"]),
                    "CROSS_OS_MEMBER_DIFF_" + lib)
    print("A13_ARCHIVE_PROVENANCE=PASS_CI_SOURCE_SHA_OBJECTS_ARCHIVES",flush=True)
    if mode == "cross":
        require(not out.exists(), "OUTPUT_ALREADY_EXISTS")
        out.mkdir(parents=True)
        hw,cfg=stage(PLATFORM,out / "cross",cli)
        for lib in LIBS:
            root=hw/"libraries"/lib
            target=root/"src/esp32"/f"lib{lib}.a"
            target.parent.mkdir(parents=True,exist_ok=True)
            shutil.copy2(src[lib],target)
            props=root/"library.properties"
            props.write_text(props.read_text(encoding="utf-8").rstrip()+
                             "\nprecompiled=full\n",encoding="utf-8")
        for board in PROFILES:
            properties(cli,cfg,board,hw)
            build=out/("build_"+board)
            _,log=compile_one(cli,cfg,board,build,out/(board+".log"),True)
            assert_precompiled(build,log,hw)
        print("A13_ARCHIVE_CROSS_OS=PASS_LINUX_ARCHIVES_LINKED_ON_WINDOWS_BOTH_FQBN",
              flush=True)
    else:
        require(not out.is_relative_to(ROOT),"OUTPUT_INSIDE_GIT")
        out.mkdir(parents=True,exist_ok=True)
        for lib in LIBS:
            target=PLATFORM/"libraries"/lib/"src/esp32"/f"lib{lib}.a"
            require(not target.exists(), "PRODUCT_ALREADY_ARCHIVED_"+lib)
            target.parent.mkdir(parents=True,exist_ok=True)
            shutil.copy2(src[lib],target)
            props=PLATFORM/"libraries"/lib/"library.properties"
            props.write_text(props.read_text(encoding="utf-8").rstrip()+
                             "\nprecompiled=full\n",encoding="utf-8")
        status=subprocess.check_output(
            ["git","status","--porcelain","--untracked-files=all"],
            cwd=ROOT,text=True)
        # Cada ruta permitida exactamente una vez; CI no stagea otras.
        paths=set(line[3:] for line in status.splitlines() if line.strip())
        require(paths==set(CHANGED_PATHS),
                "PRODUCT_PATH_SCOPE_" + str(sorted(paths)))
        subprocess.run(["git","diff","--check"],cwd=ROOT,check=True)
        print("A13_ARCHIVE_ADOPT=PASS_EXACT_FOUR_PRODUCT_PATHS",flush=True)
    evidence={
        "status":"PASS_"+mode.upper(),"origin_ci_source_sha":u["source_sha"],
        "archives":{x:{"sha256":digest(src[x]),"bytes":src[x].stat().st_size}
                    for x in LIBS},
        "windows_ci_checked":windows is not None,"device":"NOT_TOUCHED",
    }
    (out/"GATE.json").write_text(json.dumps(evidence,indent=2)+"\n",encoding="utf-8")
    print("A13_ARCHIVE_GATE_EVIDENCE="+str(out/"GATE.json"),flush=True)

if __name__=="__main__":
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument("--ubuntu-artifact",type=Path,required=True)
    p.add_argument("--windows-artifact",type=Path)
    p.add_argument("--mode",choices=("cross","adopt"),required=True)
    p.add_argument("--output",type=Path,required=True)
    p.add_argument("--cli",default="arduino-cli")
    a=p.parse_args()
    try:
        run_gate(a.ubuntu_artifact.resolve(),
                 a.windows_artifact.resolve() if a.windows_artifact else None,
                 a.mode,a.output.resolve(),a.cli)
    except (RuntimeError,ValueError,OSError,subprocess.CalledProcessError,
            subprocess.TimeoutExpired) as err:
        print("A13_ARCHIVE_REVIEW_STOP="+str(err),file=sys.stderr)
        sys.exit(2)
