#!/usr/bin/env python3
"""Arma la plataforma «ESP32 Board» (ESP32 Board / ESP32 Family).

Arduino IDE muestra en Archivo > Ejemplos todas las librerías de la plataforma de la placa elegida, sin filtrar
por placa. Para que el JWPLC Basic no muestre ejemplos del core ESP32 y ESP32 Board no muestre librerías JW/JWPLC,
las placas genéricas viven en otra plataforma (packager `jwesp`, FQBN `jwesp:esp32:esp32`).

Esa plataforma no se versiona aparte: se arma al publicar desde el core del package (`JWPLC/<versión>`):

- copia core, herramientas y librerías del core de arduino-esp32;
- quita las librerías JW/JWPLC, la variante y el core precompilado del JWPLC Basic;
- usa el boards.txt de `JWPLC/esp32-generico/` (placas genéricas visibles);
- devuelve a las librerías del core los ejemplos originales de arduino-esp32 (en el package del Basic se quitan).

Uso:
  python JWPLC/tools/jwplc_esp32_generico.py \
      --source JWPLC/2.1.0 --out dist/esp32-generico [--esp32-libraries <arduino-esp32>/libraries]

Sin --esp32-libraries descarga de GitHub los ejemplos de la versión de arduino-esp32 indicada en
`JWPLC/<versión>/package.json` (clon superficial y disperso de espressif/arduino-esp32).
"""

from __future__ import annotations

import argparse
import json
import re
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

NOMBRE_PLATAFORMA = "ESP32 Board"

# Librerías propias del JWPLC Basic (y sus dependencias de terceros) que no van en la plataforma genérica.
PREFIJOS_JW = ("JW_", "JWPLC_")
LIBRERIAS_SOLO_BASIC = {"Adafruit_BusIO"}

# Partes del core que solo usa la placa JWPLC Basic.
RUTAS_SOLO_BASIC = [
    "variants/jwplcbasic",
    "cores/jwcontrol_precompiled_stub",
    "precompiled",
]


def log(msg: str) -> None:
    print(f"[esp32-generico] {msg}", flush=True)


def es_libreria_jw(nombre: str) -> bool:
    return nombre.startswith(PREFIJOS_JW) or nombre in LIBRERIAS_SOLO_BASIC


def version_arduino_esp32(source: Path) -> str:
    data = json.loads((source / "package.json").read_text(encoding="utf-8"))
    return str(data["version"])


def descargar_librerias_upstream(version: str, destino: Path) -> Path:
    url = "https://github.com/espressif/arduino-esp32"
    log(f"Descargando ejemplos de arduino-esp32 {version} desde {url}")
    subprocess.run(
        ["git", "clone", "-q", "--depth", "1", "--branch", version, "--filter=blob:none", "--sparse", url, str(destino)],
        check=True,
    )
    subprocess.run(["git", "-C", str(destino), "sparse-checkout", "set", "libraries"], check=True)
    return destino / "libraries"


def armar(source: Path, generico: Path, out_root: Path, librerias_upstream: Path | None) -> Path:
    if not (source / "platform.txt").is_file():
        sys.exit(f"No es una plataforma Arduino: {source}")
    if not (generico / "boards.txt").is_file():
        sys.exit(f"Falta {generico / 'boards.txt'}")

    out = out_root / source.name
    if out.exists():
        shutil.rmtree(out)
    out_root.mkdir(parents=True, exist_ok=True)

    def ignorar(carpeta: str, nombres: list[str]) -> set[str]:
        rel = Path(carpeta).resolve().relative_to(source.resolve())
        fuera = {"__pycache__", ".git"}
        if rel == Path("libraries"):
            fuera |= {n for n in nombres if es_libreria_jw(n)}
        for ruta in RUTAS_SOLO_BASIC:
            r = Path(ruta)
            if r.parent == rel and r.name in nombres:
                fuera.add(r.name)
        return fuera & set(nombres)

    log(f"Copiando {source} -> {out}")
    shutil.copytree(source, out, ignore=ignorar, symlinks=False)

    # boards.txt y variantes propias de las placas genéricas.
    shutil.copy2(generico / "boards.txt", out / "boards.txt")
    if (generico / "variants").is_dir():
        shutil.copytree(generico / "variants", out / "variants", dirs_exist_ok=True)

    # Nombre visible en el gestor de placas.
    platform_txt = out / "platform.txt"
    texto = platform_txt.read_text(encoding="utf-8")
    texto = re.sub(r"^name=.*$", f"name={NOMBRE_PLATAFORMA}", texto, count=1, flags=re.M)
    platform_txt.write_text(texto, encoding="utf-8")

    # Ejemplos originales de arduino-esp32 en las librerías del core.
    tmp = None
    if librerias_upstream is None:
        tmp = Path(tempfile.mkdtemp(prefix="arduino-esp32-"))
        librerias_upstream = descargar_librerias_upstream(version_arduino_esp32(source), tmp / "arduino-esp32")
    restaurados = 0
    for lib in sorted((out / "libraries").iterdir()):
        ejemplos = librerias_upstream / lib.name / "examples"
        if lib.is_dir() and ejemplos.is_dir():
            if (lib / "examples").exists():
                shutil.rmtree(lib / "examples")
            shutil.copytree(ejemplos, lib / "examples")
            restaurados += 1
    if tmp is not None:
        shutil.rmtree(tmp, ignore_errors=True)

    quedan_jw = [p.name for p in (out / "libraries").iterdir() if es_libreria_jw(p.name)]
    if quedan_jw:
        sys.exit(f"Quedaron librerías JW en la plataforma genérica: {quedan_jw}")
    if re.search(r"^jwplcbasic", (out / "boards.txt").read_text(encoding="utf-8"), flags=re.M):
        sys.exit("El boards.txt genérico no debe definir placas JWPLC")

    log(f"Ejemplos de arduino-esp32 restaurados en {restaurados} librerías")
    log(f"Plataforma genérica lista: {out}")
    return out


def main(argv: list[str] | None = None) -> int:
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--source", default="JWPLC/2.1.0", help="Plataforma del JWPLC Basic (core del package)")
    p.add_argument("--generico", default="JWPLC/esp32-generico", help="boards.txt y variantes de las placas genéricas")
    p.add_argument("--out", default="dist/esp32-generico", help="Carpeta donde se arma <out>/<versión base>")
    p.add_argument("--esp32-libraries", default="", help="Carpeta libraries/ de arduino-esp32 (sin red)")
    a = p.parse_args(argv)
    upstream = Path(a.esp32_libraries) if a.esp32_libraries else None
    armar(Path(a.source), Path(a.generico), Path(a.out), upstream)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
