#!/usr/bin/env bash
# =============================================================================
# Alpha15 - Regenera JWPLC/2.1.0/libraries/JWPLC_Display/src/esp32/libJWPLC_Display.a
# desde el fuente, en Linux (nube o WSL), con la receta de alpha12
# (tools/alpha12/gates/alpha12_display_precompiled_refresh.ps1):
#
#   JWPLC_Display/src/*.cpp + headers
#     -> arduino-cli compile (jwplc_local:esp32:jwplcbasic, sketch mínimo)
#        como librería fuente (sin precompiled=full)
#     -> <cada .cpp>.o
#     -> xtensa-esp32-elf-gcc-ar rcs libJWPLC_Display.a
#
# Uso:
#   tools/alpha15/regenerate_libJWPLC_TFT.sh setup              # una vez: toolchain
#   tools/alpha15/regenerate_libJWPLC_Display.sh build [--install]
#
# "build" verifica:
#   - 0 diagnósticos del compilador;
#   - ABI: ningún símbolo definido y global del .a actual desaparece
#     (los símbolos nuevos se listan);
#   - los imports JWPLC nuevos (respecto del .a actual) están definidos en otra
#     librería precompilada del package (p. ej. libJWPLC_TFT.a) o en el core
#     precompilado (precompiled/core/*/core.a, p. ej. jwplcGetRTCState).
# =============================================================================
set -euo pipefail

REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
WORK="${JWPLC_BUILD_DIR:-$HOME/jwplc-build}"
FQBN="jwplc_local:esp32:jwplcbasic"
LIB_REL="JWPLC/2.1.0/libraries/JWPLC_Display"
ARCHIVE="$REPO/$LIB_REL/src/esp32/libJWPLC_Display.a"

export ARDUINO_DIRECTORIES_DATA="$WORK/data"
export ARDUINO_DIRECTORIES_USER="$WORK/user"
export ARDUINO_DIRECTORIES_DOWNLOADS="$WORK/dl"
if [[ -n "${HTTPS_PROXY:-${https_proxy:-}}" ]]; then
  export ARDUINO_NETWORK_PROXY="${HTTPS_PROXY:-${https_proxy:-}}"
fi
export PATH="$WORK/bin:$PATH"

tool() { ls "$WORK"/data/packages/jwplc/tools/esp-x32/*/bin/"$1" 2>/dev/null | head -1; }

# "<tipo> <nombre-mangled>" de los símbolos globales definidos.
globals() {
  "$(tool xtensa-esp32-elf-nm)" --defined-only -g "$1" 2>/dev/null |
    awk 'NF>=3{print $2, $3}' | sort -u
}

cmd_build() {
  local install="${1:-}"
  [[ -n "$(tool xtensa-esp32-elf-gcc-ar)" ]] || { echo "ERROR: falta toolchain (ejecuta regenerate_libJWPLC_TFT.sh setup)"; exit 1; }
  local out="$WORK/display_$(date +%Y%m%d_%H%M%S)"
  local lib="$out/src-lib/JWPLC_Display"
  mkdir -p "$lib/src" "$out/build" "$out/a" "$out/sketch/display_gate"
  cp "$REPO/$LIB_REL/src/"*.cpp "$REPO/$LIB_REL/src/"*.h "$lib/src/"
  sed -e 's/^version=.*/version=0.0.0-source/' -e '/^precompiled=/d' -e '/^dot_a_linkage=/d' \
    "$REPO/$LIB_REL/library.properties" > "$lib/library.properties"

  cat > "$out/sketch/display_gate/display_gate.ino" <<'INO'
#include <Arduino.h>
#include <JWPLC_Display.h>

void setup()
{
    JWPLC_Display.setUserPageCount(2);
    JWPLC_Display.clearFields();
}

void loop()
{
}
INO

  arduino-cli compile --fqbn "$FQBN" -j 0 -v --clean \
    --build-path "$out/build" --library "$lib" \
    --libraries "$REPO/JWPLC/2.1.0/libraries" \
    "$out/sketch/display_gate" > "$out/source.log" 2>&1 || { tail -60 "$out/source.log"; exit 1; }

  local diag
  diag=$(grep -E '\b(warning|error):' "$out/source.log" | grep -c 'JWPLC_Display' || true)
  [[ "$diag" == "0" ]] || { grep -E '\b(warning|error):' "$out/source.log" | grep 'JWPLC_Display' | head -20; echo "ERROR: $diag diagnósticos en JWPLC_Display"; exit 1; }

  local objdir members=()
  objdir="$(dirname "$(find "$out/build/libraries" -path '*JWPLC_Display*' -name 'JWPLC_Display.cpp.o' | head -1)")"
  for src in "$lib/src/"*.cpp; do
    local o="$objdir/$(basename "$src").o"
    [[ -f "$o" ]] || { echo "ERROR: falta $(basename "$o")"; exit 1; }
    cp "$o" "$out/a/"
    members+=("$(basename "$o")")
  done
  (cd "$out/a" && "$(tool xtensa-esp32-elf-gcc-ar)" rcs libJWPLC_Display.a "${members[@]}")

  globals "$ARCHIVE" > "$out/current.sym"
  globals "$out/a/libJWPLC_Display.a" > "$out/new.sym"
  local removed added
  removed=$(comm -23 "$out/current.sym" "$out/new.sym" | wc -l)
  added=$(comm -13 "$out/current.sym" "$out/new.sym" | wc -l)
  echo "ARCHIVE=$out/a/libJWPLC_Display.a"
  echo "ARCHIVE_MEMBERS=${members[*]}"
  echo "ARCHIVE_BYTES=$(stat -c %s "$out/a/libJWPLC_Display.a")"
  echo "ARCHIVE_SHA256=$(sha256sum "$out/a/libJWPLC_Display.a" | cut -d' ' -f1)"
  echo "GLOBAL_SYMBOLS_REMOVED=$removed"
  echo "GLOBAL_SYMBOLS_ADDED=$added"
  comm -13 "$out/current.sym" "$out/new.sym" | awk '{print $2}' | c++filt | sed 's/^/  + /'

  # Imports nuevos (que el .a actual no tenía) deben estar definidos en otra
  # librería precompilada del package; los imports que ya existían se resuelven
  # igual que hoy (librerías fuente como JWPLC_GlobalPeripherals).
  local nm missing=0
  nm="$(tool xtensa-esp32-elf-nm)"
  "$nm" -u "$ARCHIVE" 2>/dev/null | awk '{print $2}' | sort -u > "$out/current.imports"
  "$nm" -u "$out/a/libJWPLC_Display.a" 2>/dev/null | awk '{print $2}' | sort -u > "$out/new.imports"
  : > "$out/others.txt"
  for other in "$REPO"/JWPLC/2.1.0/libraries/*/src/esp32/*.a "$REPO"/JWPLC/2.1.0/precompiled/core/*/core.a; do
    [[ "$other" == "$ARCHIVE" ]] && continue
    "$nm" --defined-only "$other" 2>/dev/null | awk 'NF>=3{print $3}' >> "$out/others.txt"
  done
  "$nm" --defined-only "$out/a/libJWPLC_Display.a" 2>/dev/null | awk 'NF>=3{print $3}' >> "$out/others.txt"
  sort -u -o "$out/others.txt" "$out/others.txt"
  while read -r sym; do
    if grep -qxF "$sym" "$out/others.txt"; then
      echo "  ~ import nuevo resuelto: $(echo "$sym" | c++filt)"
    else
      echo "  ? import nuevo: $(echo "$sym" | c++filt)"
      [[ "$sym" =~ JWPLC|jwplc|JW_Matrix ]] && missing=$((missing + 1))
    fi
  done < <(comm -13 "$out/current.imports" "$out/new.imports")
  echo "JWPLC_IMPORTS_MISSING=$missing"

  if [[ "$removed" != "0" || "$missing" != "0" ]]; then
    comm -23 "$out/current.sym" "$out/new.sym" | awk '{print $2}' | c++filt | sed 's/^/  - /'
    echo "ABI_COMPAT=FAIL"; exit 1
  fi
  echo "ABI_COMPAT=PASS"

  if [[ "$install" == "--install" ]]; then
    cp "$out/a/libJWPLC_Display.a" "$ARCHIVE"
    echo "INSTALLED=$ARCHIVE"
  fi
}

case "${1:-}" in
  build) shift; cmd_build "${1:-}" ;;
  *) echo "uso: $0 build [--install]"; exit 2 ;;
esac
