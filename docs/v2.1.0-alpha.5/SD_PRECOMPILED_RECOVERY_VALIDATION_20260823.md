# Alpha5 - validación de recuperación precompilada de SD

Fecha: 2026-08-23.

## Objetivo

Validar que la librería nativa `SD` pueda volver a consumirse como archive compartido precompilado sin romper compatibilidad entre ESP32 Board genérico, JWPLC Basic y JWPLC Basic Core.

Archive histórico reutilizado:

```txt
JWPLC/2.1.0/libraries/SD/src/esp32/libSD.a
Git blob: 694fcaacdb5de064049921eea2599be14ebbcbb9
```

No se regeneró el archive ni se modificó la API de SD.

## Auditoría bridge-compatible - PASS

Con `Audit-JWPLCPrecompiledLibraries.ps1 -AllowGenericGpioBridge` se detectaron 11 archives. Para SD:

```txt
[BRIDGE] SD/libSD.a: jwplc_digitalWrite, jwplc_pinMode
```

Ambos símbolos están cubiertos por el bridge GPIO genérico de Alpha5. No apareció ningún símbolo `jwplc_*` adicional bloqueante y la auditoría terminó con exit code 0.

## Gate ESP32 Board genérico - PASS

FQBN:

```txt
jwplc_local:esp32:esp32
```

Se seleccionó `SD 3.3.8` vendorizada del package JWPLC y Arduino Builder la consumió desde `SD/src/esp32` como precompilada. El enlace terminó sin referencias `jwplc_*` sin resolver.

Resultado:

```txt
Sketch uses 287276 bytes
Global variables use 22244 bytes
```

## Gate JWPLC Basic - PASS

FQBN:

```txt
jwplc_local:esp32:jwplcbasic
```

`SD` se consumió como precompilada desde `SD/src/esp32` y el enlace terminó correctamente.

Resultado:

```txt
Sketch uses 394305 bytes
Global variables use 27596 bytes
```

## Gate JWPLC Basic Core - PASS

FQBN:

```txt
jwplc_local:esp32:jwplcbasiccore
```

`SD` se consumió como precompilada desde `SD/src/esp32` y el enlace terminó correctamente.

Resultado:

```txt
Sketch uses 339728 bytes
Global variables use 26804 bytes
```

## Gate físico microSD - PASS

Sketch:

```txt
tools/build-speed-benchmark/sketches/06_sd_physical/06_sd_physical.ino
```

Con tarjeta microSD insertada se observó:

```txt
JWPLC Alpha5 - gate fisico microSD
SD enabled: YES
SD ready: YES
Card present: YES
Leido: JWPLC Alpha5 SD source PASS
[PASS] Escritura y lectura microSD correctas
JWPLC_Display inicializado
```

La prueba cubre inicialización, detección de tarjeta, creación/escritura de archivo, reapertura, lectura y comparación del contenido. El build/upload utilizó `libSD.a` precompilada.

## Conclusión

**Piloto SD: CERRADO / ADOPTADO.**

La librería nativa `SD` puede permanecer precompilada en Alpha5 mediante el bridge GPIO genérico. Se recuperan 3 translation units sin romper los tres targets evaluados ni la operación física de la microSD.

Progreso de recuperación tras este piloto:

```txt
19 / 23 translation units recuperados
```

Pendientes del fallback a fuente:

```txt
JWPLC_Display      2
JW_MatrixButtons   1
JW_SD              1
--------------------
Total              4
```
