#!/usr/bin/env bash
# Verifica sin OpenPLC que el HAL real del VPP + el bloque F0 compilan contra
# el package de esta rama (jwplc_local, JWPLC_Display en modo fuente).
#
#   tools/jwplc-hmi-designer/gates/A15_F0_OpenPLC_Bool_Rele/check_f0_compile.sh
#
# Requiere tools/alpha15/regenerate_libJWPLC_TFT.sh setup (nube o WSL).
# Arma en un directorio temporal:
#   - arduino.cpp  = hal/jwplcbasic.cpp del VPP + bloque F0 + 2 llamadas;
#   - openplc.h    = stub mínimo (tipos IEC y buffers como el runtime ESP32);
#   - <sketch>.ino = imita a Baremetal.ino (setup -> hardwareInit, loop -> scan).
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO="$(cd "$HERE/../../../.." && pwd)"
HAL="$REPO/openplc-editor-installers/v4.2.7/vpp/hal/jwplcbasic.cpp"
OUT="${1:-$(mktemp -d)}/F0_OpenPLC_Bool_Rele"
mkdir -p "$OUT"

python3 - "$HAL" "$HERE" "$OUT" <<'PY'
import pathlib, re, sys
hal, here, out = (pathlib.Path(p) for p in sys.argv[1:])
src = hal.read_text(encoding="utf-8")
header = (here / "JWPLC_HMI_Generated.h").read_text(encoding="utf-8").replace("#pragma once\n", "")
block = (here / "hal_f0_bloque.cpp").read_text(encoding="utf-8").replace("// @@JWPLC_HMI_GENERATED@@", header)

def put(text, anchor, insert, after=True):
    if text.count(anchor) != 1:
        sys.exit(f"ancla no encontrada o repetida en el HAL: {anchor!r}")
    return text.replace(anchor, anchor + insert if after else insert + anchor)

src = put(src, "#include <JWPLC_ModbusRTU.h>\n", "\n" + block + "\n")
src = put(src, "void hardwareInit()\n{\n", "    jwplcHMIF0Begin();\n")
src = put(src, "    jwplcPublishForcedInputs();\n", "    jwplcHMIF0Update();\n\n", after=False)
(out / "arduino.cpp").write_text(src, encoding="utf-8")
(out / "vpp_config.h").write_text("#ifndef VPP_CONFIG_H\n#define VPP_CONFIG_H\n#endif\n", encoding="utf-8")
(out / "openplc.h").write_text("""#ifndef OPENPLC_STUB_H
#define OPENPLC_STUB_H
#include <stdint.h>
typedef uint8_t IEC_BOOL;  typedef int8_t IEC_SINT;  typedef int16_t IEC_INT;  typedef int32_t IEC_DINT;
typedef int64_t IEC_LINT;  typedef uint8_t IEC_USINT; typedef uint16_t IEC_UINT; typedef uint32_t IEC_UDINT;
typedef uint64_t IEC_ULINT; typedef uint8_t IEC_BYTE; typedef uint16_t IEC_WORD; typedef uint32_t IEC_DWORD;
typedef uint64_t IEC_LWORD; typedef float IEC_REAL; typedef double IEC_LREAL;
#define MAX_DIGITAL_INPUT 56
#define MAX_DIGITAL_OUTPUT 56
#define MAX_ANALOG_INPUT 32
#define MAX_ANALOG_OUTPUT 32
#define MAX_REAL_INPUT 32
#define MAX_REAL_OUTPUT 32
#define MAX_MEMORY_WORD 20
#define MAX_MEMORY_DWORD 20
#define MAX_MEMORY_LWORD 20
extern IEC_BOOL *bool_input[MAX_DIGITAL_INPUT/8][8];
extern IEC_BOOL *bool_output[MAX_DIGITAL_OUTPUT/8][8];
extern IEC_UINT *int_input[MAX_ANALOG_INPUT];
extern IEC_UINT *int_output[MAX_ANALOG_OUTPUT];
extern IEC_REAL *real_input[MAX_REAL_INPUT];
extern IEC_REAL *real_output[MAX_REAL_OUTPUT];
extern IEC_UINT *int_memory[MAX_MEMORY_WORD];
extern IEC_UDINT *dint_memory[MAX_MEMORY_DWORD];
extern IEC_ULINT *lint_memory[MAX_MEMORY_LWORD];
#ifdef __cplusplus
extern "C" {
#endif
void hardwareInit();
void hardwareService();
void updateInputBuffers();
void updateOutputBuffers();
#ifdef __cplusplus
}
#endif
#endif
""", encoding="utf-8")
(out / "F0_OpenPLC_Bool_Rele.ino").write_text("""// Imita a Baremetal.ino de OpenPLC: sólo para compilar/enlazar el HAL + F0.
extern "C" {
#include "openplc.h"
}
IEC_BOOL *bool_input[MAX_DIGITAL_INPUT / 8][8];
IEC_BOOL *bool_output[MAX_DIGITAL_OUTPUT / 8][8];
IEC_UINT *int_input[MAX_ANALOG_INPUT];
IEC_UINT *int_output[MAX_ANALOG_OUTPUT];
IEC_REAL *real_input[MAX_REAL_INPUT];
IEC_REAL *real_output[MAX_REAL_OUTPUT];
IEC_UINT *int_memory[MAX_MEMORY_WORD];
IEC_UDINT *dint_memory[MAX_MEMORY_DWORD];
IEC_ULINT *lint_memory[MAX_MEMORY_LWORD];

static IEC_BOOL q00 = 0;   // RELE AT %QX0.0 : BOOL

void setup()
{
    bool_output[0][0] = &q00;
    hardwareInit();
}

void loop()
{
    updateInputBuffers();
    q00 = (millis() / 1000) & 1;   // "programa": conmuta el relé cada segundo
    updateOutputBuffers();
}
""", encoding="utf-8")
print(f"sketch armado en {out}")
PY

"$REPO/tools/alpha15/regenerate_libJWPLC_TFT.sh" compile "$OUT"
