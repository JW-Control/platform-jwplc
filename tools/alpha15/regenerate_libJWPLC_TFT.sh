#!/usr/bin/env bash
# =============================================================================
# Alpha15 - Regenera JWPLC/2.1.0/libraries/JWPLC_TFT/src/esp32/libJWPLC_TFT.a
# desde el fuente, en Linux (nube o WSL), con la misma receta de alpha12
# (tools/alpha12/gates/alpha12_tft_precompiled_requalify.ps1):
#
#   JWPLC_TFT.h/.cpp/tft_setup.h + TFT_eSPI 2.5.43
#     -> arduino-cli compile (jwplc_local:esp32:jwplcbasic, sketch shapes probe)
#     -> JWPLC_TFT.cpp.o + TFT_eSPI.cpp.o
#     -> xtensa-esp32-elf-gcc-ar rcs libJWPLC_TFT.a
#
# Uso:
#   tools/alpha15/regenerate_libJWPLC_TFT.sh setup          # una vez: toolchain
#   tools/alpha15/regenerate_libJWPLC_TFT.sh build [--install]
#   tools/alpha15/regenerate_libJWPLC_TFT.sh compile <carpeta-sketch> [args arduino-cli]
#
# "build" compara los símbolos definidos (nombre, tipo, tamaño) contra el .a
# del repo: ninguno puede desaparecer ni cambiar de tamaño (compatibilidad ABI
# con los .a que ya enlazan contra JWPLC_TFT, p. ej. libJWPLC_Display.a).
# Con --install copia el .a nuevo al repo.
#
# Requiere red hacia downloads.arduino.cc, raw.githubusercontent.com y
# github.com (releases) / objects.githubusercontent.com.
# =============================================================================
set -euo pipefail

REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
WORK="${JWPLC_BUILD_DIR:-$HOME/jwplc-build}"
CORE_VERSION="2.1.0-alpha.12"
TFT_ESPI_VERSION="2.5.43"
FQBN="jwplc_local:esp32:jwplcbasic"
INDEX_URL="https://raw.githubusercontent.com/JW-Control/platform-jwplc/main/JWPLC/package_jwplc_index_dev.json"
LIB_REL="JWPLC/2.1.0/libraries/JWPLC_TFT"
PROBE_SKETCH="$REPO/tools/modbus-tcp-benchmark/firmware/a14_h3e4a1_jwplc_tft_shapes_probe"

export ARDUINO_DIRECTORIES_DATA="$WORK/data"
export ARDUINO_DIRECTORIES_USER="$WORK/user"
export ARDUINO_DIRECTORIES_DOWNLOADS="$WORK/dl"
export ARDUINO_BOARD_MANAGER_ADDITIONAL_URLS="$INDEX_URL"
if [[ -n "${HTTPS_PROXY:-${https_proxy:-}}" ]]; then
  export ARDUINO_NETWORK_PROXY="${HTTPS_PROXY:-${https_proxy:-}}"
fi
export PATH="$WORK/bin:$PATH"

tool() { ls "$WORK"/data/packages/jwplc/tools/esp-x32/*/bin/"$1" 2>/dev/null | head -1; }

symbols() {
  "$(tool xtensa-esp32-elf-nm)" -S --defined-only "$1" 2>/dev/null |
    awk 'NF>=4{print $3, $4, $2} NF==3{print $2, $3, "-"}' | sort
}

cmd_setup() {
  mkdir -p "$WORK/bin" "$WORK/user/hardware/jwplc_local"
  if ! command -v arduino-cli >/dev/null; then
    curl -sSL https://downloads.arduino.cc/arduino-cli/arduino-cli_latest_Linux_64bit.tar.gz |
      tar -xz -C "$WORK/bin" arduino-cli
  fi
  arduino-cli core update-index
  # Sólo se necesitan los tools (esp-x32, esp32-libs); la plataforma se toma del
  # repo vía jwplc_local. Si la instalación de la plataforma publicada falla, los
  # tools ya quedan instalados y alcanza.
  arduino-cli core install "jwplc:esp32@$CORE_VERSION" || true
  [[ -n "$(tool xtensa-esp32-elf-gcc-ar)" ]] || { echo "ERROR: esp-x32 no instalado"; exit 1; }
  ln -sfn "$REPO/JWPLC/2.1.0" "$WORK/user/hardware/jwplc_local/esp32"
  arduino-cli lib install "TFT_eSPI@$TFT_ESPI_VERSION"
  echo "SETUP=PASS"
}

cmd_build() {
  local install="${1:-}"
  local out="$WORK/tft_$(date +%Y%m%d_%H%M%S)"
  mkdir -p "$out/src-lib/JWPLC_TFT/src" "$out/build" "$out/a"
  cp "$REPO/$LIB_REL/src/"{JWPLC_TFT.h,JWPLC_TFT.cpp,tft_setup.h} "$out/src-lib/JWPLC_TFT/src/"
  cat > "$out/src-lib/JWPLC_TFT/library.properties" <<P
name=JWPLC_TFT
version=0.0.0-source
author=JW Control
maintainer=JW Control
sentence=Source build for libJWPLC_TFT.a regeneration.
paragraph=Temporary source-only library.
category=Display
architectures=esp32
includes=JWPLC_TFT.h
depends=TFT_eSPI,SPI
P
  arduino-cli compile --fqbn "$FQBN" -j 0 -v --clean \
    --build-path "$out/build" --library "$out/src-lib/JWPLC_TFT" \
    --libraries "$REPO/JWPLC/2.1.0/libraries" \
    --build-property "compiler.cpp.extra_flags=-DJWPLC_TFT_ESPI_DIAGNOSTIC_NO_DISPLAY_AUTOLOAD=1" \
    "$PROBE_SKETCH" > "$out/source.log" 2>&1 || { tail -40 "$out/source.log"; exit 1; }

  grep -q "Using library TFT_eSPI at version $TFT_ESPI_VERSION" "$out/source.log" ||
    { echo "ERROR: TFT_eSPI $TFT_ESPI_VERSION no seleccionada"; exit 1; }
  local diag
  diag=$(grep -ciE '\b(warning|error):' "$out/source.log" || true)
  [[ "$diag" == "0" ]] || { echo "ERROR: $diag diagnósticos del compilador"; exit 1; }

  cp "$(find "$out/build" -name JWPLC_TFT.cpp.o | head -1)" \
     "$(find "$out/build" -name TFT_eSPI.cpp.o | head -1)" "$out/a/"
  (cd "$out/a" && "$(tool xtensa-esp32-elf-gcc-ar)" rcs libJWPLC_TFT.a JWPLC_TFT.cpp.o TFT_eSPI.cpp.o)

  symbols "$REPO/$LIB_REL/src/esp32/libJWPLC_TFT.a" > "$out/current.sym"
  symbols "$out/a/libJWPLC_TFT.a" > "$out/new.sym"
  local removed added
  removed=$(comm -23 "$out/current.sym" "$out/new.sym" | wc -l)
  added=$(comm -13 "$out/current.sym" "$out/new.sym" | wc -l)
  echo "ARCHIVE=$out/a/libJWPLC_TFT.a"
  echo "ARCHIVE_BYTES=$(stat -c %s "$out/a/libJWPLC_TFT.a")"
  echo "ARCHIVE_SHA256=$(sha256sum "$out/a/libJWPLC_TFT.a" | cut -d' ' -f1)"
  echo "SYMBOLS_REMOVED_OR_CHANGED=$removed"
  echo "SYMBOLS_ADDED=$added"
  comm -13 "$out/current.sym" "$out/new.sym" | c++filt | sed 's/^/  + /'
  if [[ "$removed" != "0" ]]; then
    comm -23 "$out/current.sym" "$out/new.sym" | c++filt | sed 's/^/  - /'
    echo "ABI_COMPAT=FAIL"; exit 1
  fi
  echo "ABI_COMPAT=PASS"

  if [[ "$install" == "--install" ]]; then
    cp "$out/a/libJWPLC_TFT.a" "$REPO/$LIB_REL/src/esp32/libJWPLC_TFT.a"
    echo "INSTALLED=$REPO/$LIB_REL/src/esp32/libJWPLC_TFT.a"
  fi
}

cmd_compile() {
  local sketch="${1:?carpeta del sketch}"; shift
  arduino-cli compile --fqbn "$FQBN" "$@" "$sketch"
}

case "${1:-}" in
  setup) cmd_setup ;;
  build) shift; cmd_build "${1:-}" ;;
  compile) shift; cmd_compile "$@" ;;
  *) echo "uso: $0 setup | build [--install] | compile <sketch> [args]"; exit 2 ;;
esac
