#!/usr/bin/env bash
# =============================================================================
# Alpha15 - Regenera el Live Bridge que instala el flasheador del HMI Designer,
# en el formato de exportación del package JWPLC («Exportar binario compilado»):
#
#   tools/jwplc-hmi-designer/gates/A11_LivePreview_WebSerial/JWPLC_HMI_Live_Bridge/*.ino
#     -> arduino-cli compile (jwplc_local:esp32:jwplcbasic, el package de este repo)
#     -> poc/firmware/jwplc.esp32.jwplcbasic/
#          JWPLC_HMI_Live_Bridge.ino.bootloader.bin · .partitions.bin · .bin · boot_app0.bin
#          flash_args   (lo genera el package: partes, direcciones y modo de flash;
#                        designer-flasher.js lo usa como manifiesto)
#
# La imagen .merged.bin (4 MB) no se copia: el flasher graba las partes de
# flash_args, igual que la subida de Arduino.
#
# Uso (Linux, nube o WSL; requiere `tools/alpha15/regenerate_libJWPLC_TFT.sh setup`):
#   tools/alpha15/build_live_bridge_firmware.sh             # compila en $WORK y compara con el repo
#   tools/alpha15/build_live_bridge_firmware.sh --install   # además copia a poc/firmware/jwplc.esp32.jwplcbasic/
#
# Un binario nuevo debe probarse en la placa (flasheo + LIVE conectado) antes de
# publicarse: el flasheador lo instala sin Arduino IDE.
# =============================================================================
set -euo pipefail

REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
WORK="${JWPLC_BUILD_DIR:-$HOME/jwplc-build}"
FQBN="jwplc_local:esp32:jwplcbasic"
SKETCH="$REPO/tools/jwplc-hmi-designer/gates/A11_LivePreview_WebSerial/JWPLC_HMI_Live_Bridge"
FW_DIR="$REPO/tools/jwplc-hmi-designer/poc/firmware/jwplc.esp32.jwplcbasic"
NAME="JWPLC_HMI_Live_Bridge.ino"

export ARDUINO_DIRECTORIES_DATA="$WORK/data"
export ARDUINO_DIRECTORIES_USER="$WORK/user"
export ARDUINO_DIRECTORIES_DOWNLOADS="$WORK/dl"
export PATH="$WORK/bin:$PATH"

command -v arduino-cli >/dev/null || { echo "ERROR: falta arduino-cli; correr regenerate_libJWPLC_TFT.sh setup"; exit 1; }

OUT="$WORK/live_bridge_$(date +%Y%m%d_%H%M%S)"
EXPORT="$OUT/jwplc.esp32.jwplcbasic"
mkdir -p "$OUT/build" "$EXPORT"
# --build-path fuera del sketch: el hook savehex del package no deja
# artefactos de compilación dentro de gates/.
arduino-cli compile --fqbn "$FQBN" --build-path "$OUT/build" "$SKETCH" | tail -3

# Mismos nombres que deja «Exportar binario compilado» (platform.txt, recipe.hooks.savehex).
FILES=("$NAME.bootloader.bin" "$NAME.partitions.bin" "$NAME.bin" boot_app0.bin flash_args)
cp "$OUT/build/$NAME.bootloader.bin" "$OUT/build/$NAME.partitions.bin" "$OUT/build/$NAME.bin" "$OUT/build/flash_args" "$EXPORT/"
cp "$REPO/JWPLC/2.1.0/tools/partitions/boot_app0.bin" "$EXPORT/boot_app0.bin"
# flash_args debe nombrar exactamente esos binarios.
for f in "$NAME.bootloader.bin" "$NAME.partitions.bin" boot_app0.bin "$NAME.bin"; do
  grep -q " $f" "$EXPORT/flash_args" || { echo "ERROR: flash_args no menciona $f"; cat "$EXPORT/flash_args"; exit 1; }
done

echo "EXPORT_DIR=$EXPORT"
sed 's/[[:space:]]*$//' "$EXPORT/flash_args" | sed 's/^/  flash_args: /'
for f in "${FILES[@]}"; do
  if [[ "$f" == flash_args ]]; then
    a=$(sed 's/[[:space:]]*$//' "$EXPORT/$f"); b=$(sed 's/[[:space:]]*$//' "$FW_DIR/$f" 2>/dev/null || true)
    [[ "$a" == "$b" ]] && state=IGUAL || state=DISTINTO
  elif cmp -s "$EXPORT/$f" "$FW_DIR/$f"; then state=IGUAL; else state=DISTINTO; fi
  printf '%-42s %9s bytes  repo=%s\n' "$f" "$(stat -c %s "$EXPORT/$f")" "$state"
done

if [[ "${1:-}" == "--install" ]]; then
  mkdir -p "$FW_DIR"
  for f in "${FILES[@]}"; do cp "$EXPORT/$f" "$FW_DIR/$f"; done
  echo "INSTALL=PASS ($FW_DIR); probar en la placa antes de publicar"
fi
