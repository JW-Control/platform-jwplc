#!/usr/bin/env bash
# N1 de JW_RS485 en host.
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
LIB="$(cd "$HERE/../.." && pwd)"
MOCK="$(cd "$LIB/../JW_ModbusRTU/extras/test/mock" && pwd)"
OUT="$(mktemp -d)"
${CXX:-g++} -std=c++11 -Wall -Wextra -Wno-unused-parameter -I"$MOCK" -I"$LIB/src" \
  "$HERE/test_rs485.cpp" "$LIB/src/JW_RS485.cpp" -o "$OUT/test_rs485"
"$OUT/test_rs485"
