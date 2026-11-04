#!/usr/bin/env bash
# Regenera los .a de las librerías precompiladas que se trabajaron en modo fuente
# durante el alpha (cierre de 2.1.0-alpha.16: JWPLC_TFT, JWPLC_Display y
# JW_MatrixButtons) y verifica la ABI frente al .a anterior.
#
# Uso:
#   tools/alpha16/regenerate_precompiled.sh build [--install] [LIB...]
#
# Requiere el entorno de compilación de tools/alpha15/regenerate_libJWPLC_TFT.sh
# (arduino-cli + tools esp-x32 + plataforma jwplc_local enlazada al repo) en
# $JWPLC_BUILD_DIR (por defecto ~/jwplc-build). TFT_eSPI va incluida en
# JWPLC_TFT/src desde alpha.16: ya no se instala ni se parchea aparte.
#
# Pasos por ejecución:
#   1. Copia cada librería a un directorio temporal sin precompiled=full.
#   2. Compila un sketch sonda que usa las tres (jwplc_local:esp32:jwplcbasic)
#      con -v y exige que arduino-cli use las copias en modo fuente.
#   3. Empaqueta los .o de cada librería con xtensa-esp32-elf-gcc-ar.
#   4. ABI: ningún símbolo global definido del .a anterior desaparece y todo lo
#      que importan los demás .a del package sigue definido.
#   5. Con --install copia los .a a src/esp32/ (precompiled=full se pone aparte).
set -euo pipefail

REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
WORK="${JWPLC_BUILD_DIR:-$HOME/jwplc-build}"
FQBN="${JWPLC_FQBN:-jwplc_local:esp32:jwplcbasic}"
LIBS_DIR="$REPO/JWPLC/2.1.0/libraries"

export ARDUINO_DIRECTORIES_DATA="$WORK/data"
export ARDUINO_DIRECTORIES_USER="$WORK/user"
export ARDUINO_DIRECTORIES_DOWNLOADS="$WORK/dl"
if [[ -n "${HTTPS_PROXY:-${https_proxy:-}}" ]]; then
  export ARDUINO_NETWORK_PROXY="${HTTPS_PROXY:-${https_proxy:-}}"
fi
export PATH="$WORK/bin:$PATH"

tool() { ls "$WORK"/data/packages/jwplc/tools/esp-x32/*/bin/"$1" 2>/dev/null | head -1; }
NM="$(tool xtensa-esp32-elf-nm)"; AR="$(tool xtensa-esp32-elf-gcc-ar)"
[[ -n "$NM" && -n "$AR" ]] || { echo "ERROR: falta toolchain esp-x32 en $WORK"; exit 1; }

# Defina CTAGS_DIR si el entorno no tiene ctags (p. ej. nube sin downloads.arduino.cc):
# un ctags falso que sale con 0 basta porque el sketch sonda no necesita prototipos.
EXTRA=()
[[ -n "${CTAGS_DIR:-}" ]] && EXTRA+=(--build-property "runtime.tools.ctags.path=$CTAGS_DIR")

globals() { "$NM" --defined-only -g "$1" 2>/dev/null | awk 'NF>=3{print $2, $3}' | sort -u; }

cmd_build() {
  local install=""
  [[ "${1:-}" == "--install" ]] && { install=1; shift; }
  local libs=("$@")
  [[ ${#libs[@]} -gt 0 ]] || libs=(JWPLC_TFT JWPLC_Display JW_MatrixButtons)

  local out="$WORK/precompiled_$(date +%Y%m%d_%H%M%S)"
  mkdir -p "$out/src-lib" "$out/build" "$out/a" "$out/sketch/probe"
  local libargs=()
  for L in "${libs[@]}"; do
    cp -r "$LIBS_DIR/$L" "$out/src-lib/$L"
    rm -rf "$out/src-lib/$L/src/esp32" "$out/src-lib/$L/examples"
    sed -i -e '/^precompiled=/d' -e 's/^version=.*/&-source/' "$out/src-lib/$L/library.properties"
    libargs+=(--library "$out/src-lib/$L")
  done
  # Sonda: incluye la cabecera principal de cada librería pedida (alpha.17 añade JW_FRAM).
  { for L in "${libs[@]}"; do echo "#include <$L.h>"; done
    echo 'void setup() {}'; echo 'void loop() {}'; } > "$out/sketch/probe/probe.ino"

  arduino-cli compile --fqbn "$FQBN" -j 0 -v --clean --build-path "$out/build" \
    "${libargs[@]}" --libraries "$LIBS_DIR" "${EXTRA[@]}" \
    "$out/sketch/probe" > "$out/source.log" 2>&1 || { tail -40 "$out/source.log"; exit 1; }
  echo "LOG=$out/source.log"
  echo "DIAGNOSTICS=$(grep -ciE '\b(warning|error):' "$out/source.log" || true)"

  local fail=0
  for L in "${libs[@]}"; do
    grep -q "Using library $L at version .* in folder: $out/src-lib/$L" "$out/source.log" ||
      { echo "ERROR: $L no se compiló desde la copia en modo fuente"; exit 1; }
    local objdir="$out/build/libraries/$L" a="$out/a/lib$L.a" old="$LIBS_DIR/$L/src/esp32/lib$L.a"
    local objs; mapfile -t objs < <(find "$objdir" -name '*.o' | sort)
    [[ ${#objs[@]} -gt 0 ]] || { echo "ERROR: sin objetos para $L"; exit 1; }
    "$AR" rcs "$a" "${objs[@]}"
    echo "== $L"
    echo "  OBJECTS=$(printf '%s ' "${objs[@]##*/}")"
    echo "  ARCHIVE_BYTES=$(stat -c %s "$a")"
    echo "  ARCHIVE_SHA256=$(sha256sum "$a" | cut -d' ' -f1)"
    if [[ -f "$old" ]]; then
      globals "$old" | awk '$1!="V" && $1!="W"{print $2}' | sort -u > "$out/$L.old"
      globals "$a" | awk '{print $2}' | sort -u > "$out/$L.new"
      local removed; removed=$(comm -23 "$out/$L.old" "$out/$L.new" | wc -l)
      echo "  STRONG_SYMBOLS_REMOVED=$removed"
      echo "  SYMBOLS_ADDED=$(comm -13 "$out/$L.old" "$out/$L.new" | wc -l)"
      if [[ "$removed" != "0" ]]; then
        comm -23 "$out/$L.old" "$out/$L.new" | c++filt | sed 's/^/    - /'
        fail=1
      fi
    fi
  done

  # Lo que importan los .a que no se regeneran debe seguir definido en algún .a.
  cat "$out"/a/*.a >/dev/null
  { for a in "$out"/a/*.a; do "$NM" --defined-only "$a" 2>/dev/null | awk 'NF>=3{print $3}'; done;
    for a in "$LIBS_DIR"/*/src/esp32/*.a; do
      local base; base=$(basename "$a" .a); [[ " ${libs[*]/#/lib} " == *" $base "* ]] && continue
      "$NM" --defined-only "$a" 2>/dev/null | awk 'NF>=3{print $3}'; done; } | sort -u > "$out/defined.txt"
  local missing=0
  for a in "$LIBS_DIR"/*/src/esp32/*.a; do
    local base; base=$(basename "$a" .a); [[ " ${libs[*]/#/lib} " == *" $base "* ]] && continue
    while read -r sym; do
      grep -qxF "$sym" "$out/defined.txt" && continue
      echo "  ! $base importa $(echo "$sym" | c++filt) y no existe"; missing=$((missing + 1))
    done < <("$NM" -u "$a" 2>/dev/null | awk '{print $2}' | grep -E 'JWPLC_(TFT|Display|UI|Idle)|JW_MatrixButtons|JW_FRAM|JWPLC_FONT_' | sort -u)
  done
  echo "IMPORTS_FROM_OTHER_LIBS_MISSING=$missing"
  [[ "$missing" == "0" && "$fail" == "0" ]] || { echo "ABI_COMPAT=FAIL"; exit 1; }
  echo "ABI_COMPAT=PASS"

  if [[ -n "$install" ]]; then
    for L in "${libs[@]}"; do
      mkdir -p "$LIBS_DIR/$L/src/esp32"
      cp "$out/a/lib$L.a" "$LIBS_DIR/$L/src/esp32/lib$L.a"
      echo "INSTALLED=$LIBS_DIR/$L/src/esp32/lib$L.a"
    done
  fi
}

case "${1:-}" in
  build) shift; cmd_build "$@" ;;
  *) echo "uso: $0 build [--install] [LIB...]"; exit 2 ;;
esac
