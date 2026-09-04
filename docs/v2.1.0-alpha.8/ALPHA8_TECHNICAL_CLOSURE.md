# v2.1.0-alpha.8 — Cierre técnico

Fecha: 2026-09-04

## Resultado

Alpha8 queda técnicamente cerrado para abrir PR hacia `release/v2.1.x`.

```text
ALPHA8_SOURCE_FREEZE=PASS
ALPHA8_HARDWARE_GATE=PASS
ALPHA8_BUILD_ARCHITECTURE=PASS
ALPHA8_FINAL_DISPLAY_ARCHIVE=PASS
ALPHA8_DOCUMENTATION=PASS
ALPHA8_WORKSHOP_EXAMPLES_COMPILE=PASS
ALPHA8_WORKSHOP_EXAMPLES_TOTAL=19
ALPHA8_TECHNICAL_CLOSURE=PASS
ALPHA8_STATUS=TECHNICALLY_CLOSED
ALPHA8_PUBLICATION=PENDING_PR_CI_RELEASE
```

## Display / botonera

Se cierra el cambio de interacción entre TFT y botonera con:

- `IDLE_WAKE_DISABLED` como comportamiento por defecto;
- navegación Display por flancos físicos propios;
- `pressed()` / `released()` preservados para el sketch;
- `ESC` observado simultáneamente por Display y aplicación;
- `anyPressedOrRepeated()` limitado a `PRESS/REPEAT`;
- pulsación sostenida sin congelamiento observado.

Gate físico:

```text
ALPHA8_IDLE_SOAK_180S=PASS
ALPHA8_NO_SPONTANEOUS_AUTOWAKE=PASS
ALPHA8_HMI_HARDWARE_GATE=PASS
ALPHA8_BUTTON_RUNTIME=PASS
ALPHA8_DISPLAY_RUNTIME=PASS
ALPHA8_ESC_DISPLAY_AND_SKETCH=PASS
ALPHA8_HELD_BUTTON_REGRESSION=PASS
```

El incidente histórico del taller se considera resuelto operacionalmente para Alpha8, sin atribuir retrospectivamente todos los casos a una única causa no reproducida de forma concluyente.

```text
WORKSHOP_BUTTONS_FREEZE=HISTORICAL_INCIDENT
EXACT_ROOT_CAUSE=NOT_CONCLUSIVELY_REPRODUCED
OPERATIONAL_STATUS=RESOLVED_FOR_ALPHA8
```

## HMI Arduino

Alpha8 incorpora HMI declarativa sobre `JWPLC_Display` con:

- hasta 32 campos;
- VALUE, TEXT, BOOL y BAR;
- múltiples páginas;
- formato numérico, alineación, colores y rangos;
- dirty refresh;
- refresh bajo demanda o periódico;
- vistas cacheadas `JWPLC_IO` y `JWPLC_Time`;
- lazy-link para no enlazar el motor HMI cuando el sketch no lo utiliza.

## Build / precompilación

La estructura de compilación queda en paridad con el baseline Alpha6:

```text
Basic cold = 15 TUs
Core cold  = 78 TUs
Warm       = 1 TU
```

El gate final verificó cero recompilaciones source de Display en:

```text
Basic / 01_empty      = 0
Basic Core / 01_empty = 0
Basic / HMI gate      = 0
```

Archive final:

```text
Archivo : JWPLC/2.1.0/libraries/JWPLC_Display/src/esp32/libJWPLC_Display.a
Bytes   : 642576
SHA256  : D47B72A48B82DA99952A3C5FD042D3D5864DFEB5BB786B95BC5684F9AEDC73BC
Commit  : 08073e5a0244d9461d3889f02d411b68f727d638
```

`JW_MatrixButtons 1.0.5` no cambió su fuente durante Alpha8, por lo que se mantiene el archive ya validado:

```text
Bytes  : 129506
SHA256 : 55BE8D7791DDAD79D613DBB199C10A504DE0F20CDF3330B6679A35DD64E25C81
```

## Ejemplos de taller

Se añadieron 19 ejemplos compactos y comentados, numerados con prefijo `XX.` y ubicados en las librerías `JWPLC_*` correspondientes.

Distribución:

```text
JWPLC_GlobalPeripherals = 6
JWPLC_Display           = 4
JWPLC_Ethernet          = 3
JWPLC_RS485             = 3
JWPLC_ModbusRTU         = 3
TOTAL                    = 19
```

Los drivers genéricos `JW_*` no recibieron ejemplos dependientes del hardware JWPLC Basic.

Gate final:

```text
TOTAL=19
PASS=19
FAIL=0
ALPHA8_WORKSHOP_EXAMPLES_COMPILE=PASS
```

Después del lote:

```text
git diff --check = PASS
git status       = CLEAN
```

## Decisiones preservadas

```text
APP_ONLY=VALIDATED_DEVELOPMENT_TOOL
APP_ONLY_DEFAULT_UPLOAD=NO
BOOTLOADER_PRECOMPILED=NOT_ADOPTED
BOOTLOADER_GENERATION=SDK_ELF_AUTOMATIC
CURRENT_FLASH_PROFILE=VALIDATED_CURRENT_PROFILE
FINAL_UNIVERSAL_FLASH_CONFIGURATION=PENDING
OTA=NOT_DEFINED
```

No se retira ningún periférico del autoload normal y OpenPLC no se convierte en dependencia del package Arduino.

## Pendiente de publicación

El cierre técnico no sustituye el gate del package publicado. Después del merge/release todavía deben aprobarse:

- CI del PR;
- generación del ZIP de `v2.1.0-alpha.8`;
- PreRelease e índice dev;
- instalación aislada desde `package_jwplc_index_dev.json`;
- compilación desde `jwplc:esp32@2.1.0-alpha.8`;
- verificación de `Used platform/library`;
- upload físico;
- boot/TFT post-upload;
- cierre documental post-publicación.

## Transferencia a Alpha9

Alpha9 debe partir de la base Alpha8 publicada/cerrada y mantener fuera de este branch cualquier integración HMI ↔ OpenPLC/Ladder.
