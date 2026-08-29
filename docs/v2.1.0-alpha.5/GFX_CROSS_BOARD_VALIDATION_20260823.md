# Alpha5 - validación cross-board de Adafruit_GFX_Library precompilada

Fecha: 2026-08-23.

Este documento registra los gates de enlace del piloto 2 de recuperación de precompilados para `Adafruit_GFX_Library`.

## Estado previo

- archive histórico de Alpha4 restaurado: `src/esp32/libAdafruit_GFX_Library.a`;
- Git blob: `0b8b9ad2f7ce449a485635c702e08017194fa204`;
- `precompiled=full` reactivado;
- auditoría global con `-AllowGenericGpioBridge`: PASS BRIDGE-COMPATIBLE;
- dependencias externas permitidas del archive GFX: `jwplc_pinMode`, `jwplc_digitalWrite`, `jwplc_digitalRead`.

## Sketch de gate

```txt
tools/build-speed-benchmark/sketches/09_gfx_bridge_link/09_gfx_bridge_link.ino
```

El sketch fuerza referencias reales a `GFXcanvas1` y métodos implementados por Adafruit GFX para evitar un falso PASS por library discovery sin uso efectivo del archive.

## ESP32 Board genérico - PASS

FQBN:

```txt
jwplc_local:esp32:esp32
```

Evidencia del log:

```txt
Library Adafruit GFX Library has been declared precompiled:
Using precompiled library in ...\Adafruit_GFX_Library\src\esp32
...
-L...\Adafruit_GFX_Library\src\esp32 -lAdafruit_GFX_Library
...
Sketch uses 305720 bytes (23%) of program storage space. Maximum is 1310720 bytes.
Global variables use 23604 bytes (7%) of dynamic memory, leaving 304076 bytes for local variables.
```

No se detectaron `undefined reference` ni errores de build.

Observación: en este target Arduino Builder resolvió `Adafruit BusIO` desde el sketchbook del usuario y no desde la copia vendorizada del package. Esto no invalida el gate de GFX, porque el archive probado fue seleccionado y enlazado explícitamente. La diferencia queda registrada para el piloto posterior de BusIO.

## JWPLC Basic - PASS

FQBN:

```txt
jwplc_local:esp32:jwplcbasic
```

Evidencia del log:

```txt
Library Adafruit GFX Library has been declared precompiled:
Using precompiled library in ...\Adafruit_GFX_Library\src\esp32
...
-L...\Adafruit_GFX_Library\src\esp32 -lAdafruit_GFX_Library
...
Sketch uses 406801 bytes (10%) of program storage space. Maximum is 4063232 bytes.
Global variables use 27956 bytes (8%) of dynamic memory, leaving 299724 bytes for local variables.
```

No se detectaron referencias `jwplc_*` sin resolver ni errores de build.

## JWPLC Basic Core - PASS

FQBN:

```txt
jwplc_local:esp32:jwplcbasiccore
```

Evidencia del log:

```txt
Library Adafruit GFX Library has been declared precompiled:
Using precompiled library in ...\Adafruit_GFX_Library\src\esp32
...
-L...\Adafruit_GFX_Library\src\esp32 -lAdafruit_GFX_Library
...
Sketch uses 352788 bytes (11%) of program storage space. Maximum is 3145728 bytes.
Global variables use 27124 bytes (8%) of dynamic memory, leaving 300556 bytes for local variables.
```

No se detectaron referencias `jwplc_*` sin resolver ni errores de build.

## Conclusión estructural

Los tres targets previstos para Alpha5 enlazan correctamente el mismo archive compartido de `Adafruit_GFX_Library` usando la arquitectura bridge-compatible. El piloto 2 supera sus gates estructurales.

Pendiente antes de adoptar definitivamente el archive:

- gate físico del display ST7789 en JWPLC Basic, dibujando primitivas y texto mediante la pila normal `JWPLC_Display -> Adafruit_ST7789 -> Adafruit_GFX`.
