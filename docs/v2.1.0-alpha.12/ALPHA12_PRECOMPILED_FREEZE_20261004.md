# Alpha12 — Freeze final de precompilados

Fecha: 2026-10-04

## Resultado

```text
P7A_GLOBAL_PRECOMPILED_ARCHIVE_AUDIT=PASS_CLOSED
P7B_RELEASE_LIKE_PRECOMPILED_ACTIVATION=PASS_CLOSED
PRECOMPILED_FREEZE=PASS
FINAL_BUILD_SPEED_BENCHMARK=READY
```

El freeze se declara después de:

1. regenerar/requalificar P1-P6;
2. auditar globalmente P7A;
3. activar la política release-like P7B;
4. demostrar enlace real con `01_empty` y autoload normal.

## P7B — evidencia de enlace

```text
COMPILE_EXIT=0
WARNING_LINES=0
ERROR_LINES=0

LINK_JWPLC_Display_PRECOMPILED_MARKER=YES
LINK_JWPLC_Display_SOURCE_OBJECT_COUNT=0

LINK_JWPLC_ModbusRTU_PRECOMPILED_MARKER=YES
LINK_JWPLC_ModbusRTU_SOURCE_OBJECT_COUNT=0

LINK_JWPLC_TFT_PRECOMPILED_MARKER=YES
LINK_JWPLC_TFT_SOURCE_OBJECT_COUNT=0

LINK_JW_SD_PRECOMPILED_MARKER=YES
LINK_JW_SD_SOURCE_OBJECT_COUNT=0

LINK_SPI_PRECOMPILED_MARKER=YES
LINK_SPI_SOURCE_OBJECT_COUNT=0

GLOBAL_TFT_ESPI_REQUIRED_AT_USER_BUILD=NO
NORMAL_AUTOLOAD_COMPLETE=PASS
FINAL_TRACKED_DIRTY_COUNT=0
```

## Política final

### Precompilados release-like activos

| Librería | Política |
|---|---|
| JWPLC_Display | `precompiled=full`, `dot_a_linkage=true` |
| JWPLC_ModbusRTU | `precompiled=full` |
| JWPLC_TFT | `precompiled=full`, `dot_a_linkage=true` |
| JW_SD | `precompiled=full` |
| SPI | `precompiled=full` |

### Source-only intencional

| Librería | Decisión |
|---|---|
| JW_RTC | Source-only por auditoría histórica explícita |
| JWPLC_GlobalPeripherals | P4 no adoptado |
| JWPLC_Ethernet | Implementación Alpha12 source-only |
| JWPLC_RS485 | Implementación Alpha12 source-only |

No se retiran periféricos del autoload normal para ganar velocidad.

## Inventario final de archives

| Artifact | Clase | Bytes | SHA-256 |
|---|---|---:|---|
| `core.a` | REGENERATED | 3042444 | `78d0c0ab14f156b96116529e88872340d51877af40d24ba3559f6081e0bf34fb` |
| `libJWPLC_ModbusRTU.a` | REGENERATED | 292390 | `424ed3f462bb57cce0019d690486e612d5f243c40ae62d44d3ad973b0a521085` |
| `libSPI.a` | REGENERATED | 103412 | `b433758b746380bf8d1ea102aca1d1637b5a8cab50616b56024f4b022a916445` |
| `libJW_SD.a` | REGENERATED | 362316 | `1b9619ba37295782ade1edc6f06439a38e7e534fc08f5b05757c9af7a645acc0` |
| `libJWPLC_Display.a` | REGENERATED | 941228 | `c960d718433e29a40e3cc55bc745c9a2e121ee1ee598ec72c04872327592dc02` |
| `libJWPLC_TFT.a` | REQUALIFIED | 1091098 | `5d860a131811dd9a7eb6fa55f5674b1d78b0de7dfaf8748ce18a60ceed2d3738` |
| `libAdafruit_BusIO.a` | RETAINED | 209912 | `4ef73785e114afb14d69b77701defa2b8f07c9a3b1c561cef974824d471369bf` |
| `libAdafruit_GFX_Library.a` | RETAINED | 548540 | `1a2f8c805ee31c83ed673fa6233d46a872d16fad3b2c333d1abd742c75d8f505` |
| `libAdafruit_ST7735_and_ST7789_Library.a` | RETAINED | 241924 | `f22e578b44cdd48b51831b8ed151ad797d716f63da904af4fd944cb38a672710` |
| `libFS.a` | RETAINED | 415104 | `cbea33c505d28e9b3a6a2e3abddea6cb4c384b8be919ed456c00ed0d8a31c327` |
| `libJW_FRAM.a` | RETAINED | 126304 | `5f40ae66b0100bbc449a33af310d51befdcea8d48aa194fdb57d2cb0d47b24a5` |
| `libJW_MatrixButtons.a` | RETAINED | 129506 | `55be8d7791ddad79d613dbb199c10a504de0f20cdf3330b6679a35dd64e25c81` |
| `libSD.a` | RETAINED | 275694 | `45d1d9b27701403ce7d380838ad723194a3730db5f2859b90d0d01d75fa040fd` |
| `libWire.a` | RETAINED | 166980 | `a864851ebfcb8cd3fee55d3d7834b81254ad4ebe0d75f6ed9ebc846355f9c4aa` |

## Autoload demostrado en P7B

```text
JWPLC_Display=SELECTED
JWPLC_TFT=SELECTED
JW_MatrixButtons=SELECTED
JWPLC_GlobalPeripherals=SELECTED
JW_RTC=SELECTED
JW_FRAM=SELECTED
JW_SD=SELECTED
JWPLC_Ethernet=SELECTED
JWPLC_RS485=SELECTED
JWPLC_ModbusRTU=SELECTED
SPI=SELECTED
SD=SELECTED
```

## Regla del freeze

Desde este punto:

```text
CHANGE_SOURCE_OR_ARCHIVE_OR_PRECOMPILED_POLICY
-> INVALIDATE_PRECOMPILED_FREEZE
-> REQUALIFY_AFFECTED_ARTIFACT
-> RERUN_P7
-> RERUN_FINAL_BUILD_SPEED_BENCHMARK
```

Cambios exclusivamente documentales o de harness que no alteren package/source
no invalidan el freeze.

## Siguiente paso

```text
PRECOMPILED_FREEZE=PASS
-> FINAL_BUILD_SPEED_BENCHMARK
-> FINAL_CLI_IDE_UPLOAD_GATES
-> RELEASE
```
