#!/usr/bin/env bash
# N1 de JW_DWIN en host. C++14: los structs DWIN_* usan agregados con
# inicializadores por defecto (igual que JW_DWIN_RS485 1.x).
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
LIB="$(cd "$HERE/../.." && pwd)"
OUT="$(mktemp -d)"
${CXX:-g++} -std=c++14 -Wall -Wextra -I"$HERE/mock" -I"$LIB/src" \
  "$HERE/test_dwin.cpp" "$LIB/src/JW_DWIN.cpp" -o "$OUT/test_dwin"
"$OUT/test_dwin"
${CXX:-g++} -std=c++14 -Wall -Wextra -I"$HERE/mock" -I"$LIB/src" \
  "$HERE/test_usb.cpp" "$LIB/src/JW_DWIN_ModbusUSB.cpp" "$LIB/src/JW_DWIN.cpp" -o "$OUT/test_usb"
"$OUT/test_usb"
