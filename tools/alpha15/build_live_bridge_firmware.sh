#!/usr/bin/env bash
# =============================================================================
# Alpha15 - Regenera los binarios del Live Bridge que instala el flasheador del
# HMI Designer (tools/jwplc-hmi-designer/poc/firmware/) desde su fuente:
#
#   tools/jwplc-hmi-designer/gates/A11_LivePreview_WebSerial/JWPLC_HMI_Live_Bridge/*.ino
#     -> arduino-cli compile (jwplc_local:esp32:jwplcbasic, el package de este repo)
#     -> bootloader.bin @0x1000 · partitions.bin @0x8000 · boot_app0.bin @0xE000
#        · firmware.bin @0x10000 · firmware.merged.bin (imagen completa @0x0)
#
# Mismas partes y direcciones que la subida de Arduino (platform.txt,
# tools.esptool_py.upload) y que poc/firmware/manifest.json.
#
# Uso (Linux, nube o WSL; requiere `tools/alpha15/regenerate_libJWPLC_TFT.sh setup`):
#   tools/alpha15/build_live_bridge_firmware.sh             # compila en $WORK y compara con el repo
#   tools/alpha15/build_live_bridge_firmware.sh --install   # además copia a poc/firmware/
#
# Un binario nuevo debe probarse en la placa (flasheo + LIVE conectado) antes de
# publicarse: el flasheador lo instala sin Arduino IDE.
# =============================================================================
set -euo pipefail

REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
WORK="${JWPLC_BUILD_DIR:-$HOME/jwplc-build}"
FQBN="jwplc_local:esp32:jwplcbasic"
SKETCH="$REPO/tools/jwplc-hmi-designer/gates/A11_LivePreview_WebSerial/JWPLC_HMI_Live_Bridge"
FW_DIR="$REPO/tools/jwplc-hmi-designer/poc/firmware"
NAME="JWPLC_HMI_Live_Bridge.ino"

export ARDUINO_DIRECTORIES_DATA="$WORK/data"
export ARDUINO_DIRECTORIES_USER="$WORK/user"
export ARDUINO_DIRECTORIES_DOWNLOADS="$WORK/dl"
export PATH="$WORK/bin:$PATH"

command -v arduino-cli >/dev/null || { echo "ERROR: falta arduino-cli; correr regenerate_libJWPLC_TFT.sh setup"; exit 1; }

OUT="$WORK/live_bridge_$(date +%Y%m%d_%H%M%S)"
mkdir -p "$OUT/build" "$OUT/firmware"
# --build-path fuera del sketch: el hook savehex del package no deja
# artefactos de compilación dentro de gates/.
arduino-cli compile --fqbn "$FQBN" --build-path "$OUT/build" "$SKETCH" | tail -3

cp "$OUT/build/$NAME.bootloader.bin" "$OUT/firmware/bootloader.bin"
cp "$OUT/build/$NAME.partitions.bin" "$OUT/firmware/partitions.bin"
cp "$REPO/JWPLC/2.1.0/tools/partitions/boot_app0.bin" "$OUT/firmware/boot_app0.bin"
cp "$OUT/build/$NAME.bin" "$OUT/firmware/firmware.bin"
cp "$OUT/build/$NAME.merged.bin" "$OUT/firmware/firmware.merged.bin"
grep -q -- "--flash-mode dio --flash-freq 40m --flash-size 4MB" "$OUT/build/flash_args" ||
  { echo "ERROR: flash_args distinto de manifest.json (dio/40m/4MB):"; cat "$OUT/build/flash_args"; exit 1; }

echo "FIRMWARE_DIR=$OUT/firmware"
for f in bootloader.bin partitions.bin boot_app0.bin firmware.bin firmware.merged.bin; do
  if cmp -s "$OUT/firmware/$f" "$FW_DIR/$f"; then state=IGUAL; else state=DISTINTO; fi
  printf '%-22s %9s bytes  repo=%s\n' "$f" "$(stat -c %s "$OUT/firmware/$f")" "$state"
done

if [[ "${1:-}" == "--install" ]]; then
  cp "$OUT/firmware/"*.bin "$FW_DIR/"
  echo "INSTALL=PASS ($FW_DIR); probar en la placa antes de publicar"
fi
