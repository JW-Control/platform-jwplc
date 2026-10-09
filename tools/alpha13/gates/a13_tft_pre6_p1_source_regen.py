#!/usr/bin/env python3
"""TFT-PRE6-P1A: generar fuentes de mantenimiento en TEMP, sin tocar el package.

Reproduce las transformaciones exactas, ya validadas físicamente, de TFT-PRE4.
No invoca Arduino, no enlaza, no sube firmware y no altera TFT_eSPI instalada.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import sys
import tempfile

EXPECTED_GIT_BLOBS = {
    "JWPLC_TFT.cpp": "2bdb55d504cfd2a1481ff561d33535d1740bb472",
    "tft_setup.h": "773f8123844f783bfc67c3123150c27f29f45d34",
    "JWPLC_TFT.h": "c24373eddaff5f148092c49fe997fb59f94bdbdc",
}
EXPECTED_BACKEND_SHA256 = {
    "TFT_eSPI.cpp": "01ed6edb0530d38b94ddeac079ba81633aa21d77d049b12da21a37f4bec69ee1",
    "TFT_eSPI.h": "b1b2789ace7ac8fd4a4c414054757d91e6e62649a23d74226ea28ceb8d6f4462",
    "TFT_Drivers/ST7789_Init.h": "e21cae2ac84285dc0e77648eca67ca753f41f7da3c271594ede750b725136c10",
}
EXPECTED_CANDIDATE_SHA256 = {
    "JWPLC_TFT.cpp": "494440b00e74e74a7b23975420574e035ef2138fdf5a0cea2fed51c986c29d25",
    "tft_setup.h": "8fa079444ca130772d3a642eaf814035100bc098032b403c922c458d351ae5e1",
    "ST7789_Init.h": "44873be82fe836084934a328df77f098e9ab88d212dd1d570da5e8aac74671bc",
}

SOURCE_LIB_PROPS = (
    "name=JWPLC_TFT\n"
    "version=0.1.0-alpha13-pre6-source\n"
    "author=JW Control\n"
    "maintainer=JW Control\n"
    "sentence=Fuente temporal de mantenimiento ST7789.\n"
    "paragraph=No es un artifact distribuible al usuario final.\n"
    "category=Display\n"
    "architectures=esp32\n"
    "includes=JWPLC_TFT.h\n"
    "depends=TFT_eSPI,SPI\n"
)


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def git_blob(path: Path, expected: str) -> str:
    # Git can store LF while a Windows checkout contains CRLF.
    # Accept only that exact transformation, not arbitrary content changes.
    data = path.read_bytes()
    def blob_sha(raw: bytes) -> str:
        return hashlib.sha1(
            b"blob " + str(len(raw)).encode("ascii") + b"\0" + raw
        ).hexdigest()
    current = blob_sha(data)
    if current == expected:
        return current
    normalized = blob_sha(data.replace(b"\r\n", b"\n"))
    return normalized


def verified_eq(value: str, expected: str, code: str) -> None:
    if value != expected:
        raise ValueError(f"{code}: actual={value}; expected={expected}")


def replace_once(text: str, old: str, new: str, name: str) -> str:
    count = text.count(old)
    if count != 1:
        raise ValueError(f"ANCHOR_{name}_COUNT={count}")
    return text.replace(old, new, 1)


def read_exact(path: Path) -> str:
    return path.read_bytes().decode("utf-8")


def write_exact(path: Path, text: str) -> None:
    path.write_bytes(text.encode("utf-8"))


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--repo", required=True, type=Path)
    ap.add_argument("--backend", required=True, type=Path)
    ap.add_argument("--output", required=True, type=Path)
    args = ap.parse_args()

    repo = args.repo.resolve(strict=True)
    backend = args.backend.resolve(strict=True)
    out = args.output.resolve(strict=False)
    temp_root = Path(tempfile.gettempdir()).resolve(strict=True)
    if out == temp_root or temp_root not in out.parents:
        raise ValueError("OUTPUT_MUST_BE_NEW_CHILD_OF_OS_TEMP")
    if out.exists():
        raise ValueError("OUTPUT_ALREADY_EXISTS")
    if not backend.is_dir():
        raise ValueError("BACKEND_NOT_DIRECTORY")

    src = repo / "JWPLC" / "2.1.0" / "libraries" / "JWPLC_TFT" / "src"
    original = {
        "JWPLC_TFT.cpp": src / "JWPLC_TFT.cpp",
        "tft_setup.h": src / "tft_setup.h",
        "JWPLC_TFT.h": src / "JWPLC_TFT.h",
    }
    for name, path in original.items():
        verified_eq(
            git_blob(path, EXPECTED_GIT_BLOBS[name]),
            EXPECTED_GIT_BLOBS[name],
            "OFFICIAL_BLOB_" + name,
        )

    for name, expected in EXPECTED_BACKEND_SHA256.items():
        verified_eq(sha256(backend / name), expected, "BACKEND_SHA_" + name)
    if '#define TFT_ESPI_VERSION "2.5.43"' not in read_exact(backend / "TFT_eSPI.h"):
        raise ValueError("BACKEND_VERSION_NOT_2_5_43")

    lf = os.linesep
    cpp = read_exact(original["JWPLC_TFT.cpp"])
    setup = read_exact(original["tft_setup.h"])
    init = read_exact(backend / "TFT_Drivers" / "ST7789_Init.h")

    setup = replace_once(
        setup,
        "#define JWPLC_TFT_BACKEND_SETUP 1",
        "#define JWPLC_TFT_BACKEND_SETUP 1" + lf + "#define JWPLC_TFT_DEFER_DISPON 1",
        "SETUP",
    )
    cpp = replace_once(
        cpp,
        "    static constexpr uint8_t ACTIVE_ROTATION = 1;",
        "    static constexpr uint8_t ACTIVE_ROTATION = 1;"
        + lf
        + "    static constexpr uint8_t ST7789_CMD_DISPON = 0x29;",
        "CONST",
    )
    cpp = replace_once(
        cpp,
        "    g_backend.init();" + lf + "    g_backend.setRotation(ACTIVE_ROTATION);",
        lf.join(
            [
                "    g_backend.init();",
                "    g_backend.setRotation(ACTIVE_ROTATION);",
                "    g_backend.fillScreen(JWPLC_TFT_BLACK);",
                "    g_backend.writecommand(ST7789_CMD_DISPON);",
                "    delay(120);",
            ]
        ),
        "BEGIN",
    )
    pattern = re.compile(
        r"(?m)^(?P<indent>\s*)writecommand\(ST7789_DISPON\);"
        r"\s*//\s*Display on\s*\r?\n(?P=indent)delay\(120\);"
    )
    matches = list(pattern.finditer(init))
    if len(matches) != 2:
        raise ValueError(f"ST7789_DISPON_ANCHOR_COUNT={len(matches)}")

    def delayed_dispon(m: re.Match[str]) -> str:
        ind = m.group("indent")
        return lf.join(
            [
                ind + "#ifndef JWPLC_TFT_DEFER_DISPON",
                ind + "writecommand(ST7789_DISPON);    // Display on",
                ind + "delay(120);",
                ind + "#endif",
            ]
        )

    patched_init = pattern.sub(delayed_dispon, init)
    if patched_init.count("JWPLC_TFT_DEFER_DISPON") != 2:
        raise ValueError("PATCHED_BACKEND_MACRO_COUNT_INVALID")

    # All transformations must match the precise PRE4 physical candidate
    # *before* copying any backend files.
    candidates = {
        "JWPLC_TFT.cpp": cpp,
        "tft_setup.h": setup,
        "ST7789_Init.h": patched_init,
    }
    for name, text in candidates.items():
        actual = hashlib.sha256(text.encode("utf-8")).hexdigest()
        verified_eq(actual, EXPECTED_CANDIDATE_SHA256[name], "CANDIDATE_SHA_" + name)

    # Source and backend are copied only under OS TEMP.
    out.mkdir(parents=True, exist_ok=False)
    lib_root = out / "libraries" / "JWPLC_TFT"
    lib_src = lib_root / "src"
    lib_src.mkdir(parents=True)
    backend_copy = out / "libraries" / "TFT_eSPI"
    shutil.copytree(backend, backend_copy)
    sketch_root = out / "a13_tft_pre1_startup_baseline_probe"
    sketch_root.mkdir(parents=True)
    probe_source = (
        repo / "tools" / "alpha13" / "firmware"
        / "a13_tft_pre1_startup_baseline_probe"
        / "a13_tft_pre1_startup_baseline_probe.ino"
    )
    shutil.copy2(probe_source, sketch_root / probe_source.name)

    write_exact(lib_src / "JWPLC_TFT.cpp", cpp)
    shutil.copy2(original["JWPLC_TFT.h"], lib_src / "JWPLC_TFT.h")
    write_exact(lib_src / "tft_setup.h", setup)
    write_exact(lib_root / "library.properties", SOURCE_LIB_PROPS)
    write_exact(sketch_root / "tft_setup.h", setup)
    write_exact(backend_copy / "TFT_Drivers" / "ST7789_Init.h", patched_init)

    # Guard the compiled backend's own translation unit. Proven to work
    # in PRE5 R3; no changes to upstream TFT_eSPI itself.
    backend_cpp = backend_copy / "TFT_eSPI.cpp"
    current_cpp_bytes = backend_cpp.read_bytes()
    include = b'#include "TFT_eSPI.h"'
    guard = (
        lf
        + "#if !defined(ST7789_DRIVER) || !defined(JWPLC_TFT_DEFER_DISPON)"
        + lf
        + "#error A13_TFT_PRE6_BACKEND_CONFIGURATION_NOT_PROPAGATED"
        + lf
        + "#endif"
    ).encode("ascii")
    if current_cpp_bytes.count(include) != 1:
        raise ValueError("BACKEND_INCLUDE_ANCHOR_COUNT_INVALID")
    backend_cpp.write_bytes(current_cpp_bytes.replace(include, include + guard, 1))

    for name, expected in EXPECTED_CANDIDATE_SHA256.items():
        p = (
            backend_copy / "TFT_Drivers" / "ST7789_Init.h"
            if name == "ST7789_Init.h"
            else lib_src / name
        )
        verified_eq(sha256(p), expected, "COPIED_SHA_" + name)
    verified_eq(sha256(backend / "TFT_eSPI.cpp"), EXPECTED_BACKEND_SHA256["TFT_eSPI.cpp"], "GLOBAL_BACKEND_CPP_UNCHANGED")
    verified_eq(sha256(backend / "TFT_Drivers" / "ST7789_Init.h"), EXPECTED_BACKEND_SHA256["TFT_Drivers/ST7789_Init.h"], "GLOBAL_BACKEND_INIT_UNCHANGED")

    manifest = {
        "schema": "a13-tft-pre6-source-regeneration-v1",
        "source": "PRODUCT_CANONICAL_BLOBS",
        "backend": "MAINTAINER_TFT_ESPI_2.5.43_VERIFIED_SHA",
        "temporary_root": str(out),
        "jwplc_tft_library": str(lib_root),
        "tft_espi_library": str(backend_copy),
        "sketch": str(sketch_root),
        "source_cpp_sha256": sha256(lib_src / "JWPLC_TFT.cpp"),
        "source_setup_sha256": sha256(lib_src / "tft_setup.h"),
        "backend_init_sha256": sha256(backend_copy / "TFT_Drivers" / "ST7789_Init.h"),
        "backend_cpp_instrumented_sha256": sha256(backend_cpp),
        "compile_executed": False,
        "upload_executed": False,
        "product_mutated": False,
    }
    (out / "MANIFEST.json").write_text(json.dumps(manifest, ensure_ascii=False, indent=2), encoding="utf-8")
    print("GATE=A13-TFT-PRE6-P1A")
    print("STATUS=PASS")
    print("REASON=CANONICAL_SOURCE_TRANSFORMATION_REPRODUCED")
    print(f"TEMP_ROOT={out}")
    print(f"TEMP_JWPLC_TFT_ROOT={lib_root}")
    print(f"TEMP_TFT_ESPI_ROOT={backend_copy}")
    print(f"TEMP_SKETCH_ROOT={sketch_root}")
    print(f"CANDIDATE_JWPLC_TFT_CPP_SHA256={manifest['source_cpp_sha256']}")
    print(f"CANDIDATE_TFT_SETUP_SHA256={manifest['source_setup_sha256']}")
    print(f"CANDIDATE_ST7789_INIT_SHA256={manifest['backend_init_sha256']}")
    print(f"TEMP_BACKEND_CPP_INSTRUMENTED_SHA256={manifest['backend_cpp_instrumented_sha256']}")
    print(f"MANIFEST={out / 'MANIFEST.json'}")
    print("INSTALLED_TFT_ESPI_MUTATED=NO")
    print("PRODUCT_REPO_MUTATED=NO")
    print("COMPILE_EXECUTED=NO")
    print("UPLOAD_EXECUTED=NO")
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except Exception as exc:
        print("STATUS=REVIEW", flush=True)
        print(f"REASON={type(exc).__name__}", flush=True)
        print(f"EXCEPTION={exc}", flush=True)
        sys.exit(1)
