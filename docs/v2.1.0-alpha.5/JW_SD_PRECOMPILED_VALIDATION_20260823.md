# Alpha5 - validación de recuperación precompilada de JW_SD

Fecha: 2026-08-23.

## Objetivo

Validar que `JW_SD` pueda volver a consumirse como archive compartido precompilado sin romper compatibilidad entre ESP32 Board genérico, JWPLC Basic y JWPLC Basic Core, y manteniendo operación física real de la microSD.

Archive histórico reutilizado:

```txt
JWPLC/2.1.0/libraries/JW_SD/src/esp32/libJW_SD.a
Git blob: df8023da4bb431cc90aa1d56fd51f47c43612a4c
```

No se regeneró el archive ni se modificó la API pública de `JW_SD`.

## Auditoría completa de símbolos JWPLC - PASS

Con el auditor corregido para detectar cualquier símbolo cuyo nombre comience por `jwplc` y `-AllowGenericGpioBridge`, el archive quedó clasificado como:

```txt
[BRIDGE] JW_SD/libJW_SD.a: jwplc_digitalRead, jwplc_pinMode
```

No aparecieron dependencias `jwplcSPI_*`, `jwplcSystem*` ni `jwplcI2C_*`.

Resultado global de la auditoría con JW_SD restaurada:

```txt
Archives encontrados: 11
EXIT CODE: 0
```

## Gates cross-board - PASS

Sketch:

```txt
tools/build-speed-benchmark/sketches/13_jwsd_bridge_link/13_jwsd_bridge_link.ino
```

### ESP32 Board genérico

FQBN:

```txt
jwplc_local:esp32:esp32
```

Resultado:

```txt
JW_SD precompiled: sí
SD nativa precompiled: sí
Sketch uses 288340 bytes
Global variables use 22244 bytes
```

La copia seleccionada de SD fue la vendorizada del package JWPLC, no la copia 1.3.0 del sketchbook.

### JWPLC Basic

FQBN:

```txt
jwplc_local:esp32:jwplcbasic
```

Resultado:

```txt
JW_SD precompiled: sí
SD nativa precompiled: sí
Sketch uses 394313 bytes
Global variables use 27596 bytes
```

### JWPLC Basic Core

FQBN:

```txt
jwplc_local:esp32:jwplcbasiccore
```

Resultado:

```txt
JW_SD precompiled: sí
SD nativa precompiled: sí
Sketch uses 339736 bytes
Global variables use 26804 bytes
```

Los tres targets terminaron correctamente y sin referencias `jwplc_*` sin resolver.

## Gate físico microSD - PASS

Sketch:

```txt
tools/build-speed-benchmark/sketches/06_sd_physical/06_sd_physical.ino
```

Build/upload:

```txt
BUILD/UPLOAD EXIT CODE: 0
JW_SD -> src/esp32 precompiled
SD -> src/esp32 precompiled
Sketch uses 411277 bytes
Global variables use 27604 bytes
```

El link incluyó explícitamente:

```txt
-lJW_SD
-lSD
```

El upload a COM4 terminó con verificación de hashes y hard reset.

Salida física observada:

```txt
JWPLC Alpha5 - gate fisico microSD
SD enabled: YES
SD ready: YES
Card present: YES
Leido: JWPLC Alpha5 SD source PASS
[PASS] Escritura y lectura microSD correctas
JWPLC_Display inicializado
```

La cadena real validada fue:

```txt
JW_SD precompilada -> SD nativa precompilada -> FS/SPI -> microSD física
```

## Conclusión

**Piloto JW_SD: CERRADO / ADOPTADO.**

`JW_SD` puede permanecer precompilada en Alpha5 utilizando exclusivamente el bridge GPIO genérico permitido.

Con este cierre quedan recuperados y adoptados:

```txt
JWPLC_Ethernet_W5x00_Backend    8 TUs
Adafruit_GFX_Library            4 TUs
Adafruit_BusIO                  4 TUs
SD nativa                       3 TUs
JW_SD                           1 TU
------------------------------------
Total                          20 TUs
```

Permanecen desde fuente por decisión o pendiente:

```txt
JWPLC_Display                   2 TUs  decisión: fuente
JW_RTC                          1 TU   decisión: fuente
JW_MatrixButtons                1 TU   pendiente
------------------------------------
Total                           4 TUs
```

Antes de continuar con `JW_MatrixButtons` se repetirá el probe físico de arranque/coexistencia SPI de Alpha4 para verificar que no haya regresado una retención anómala del mutex y aclarar el `SPI lock timeout` transitorio observado durante pruebas recientes.
