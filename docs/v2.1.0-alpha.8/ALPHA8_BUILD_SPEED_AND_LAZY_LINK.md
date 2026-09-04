# Alpha8 — Build speed, precompilación y lazy-link

Fecha de cierre técnico: 2026-09-04

## Objetivo

Alpha8 añadió una HMI declarativa y nuevas vistas cacheadas sin retirar periféricos del autoload normal. Este documento registra el impacto de compilación y las optimizaciones aplicadas para evitar que esa funcionalidad incremente innecesariamente el costo de builds que no usan HMI.

## Baseline de referencia

Baseline corregido Alpha6:

```text
tools/build-speed-benchmark/results/20260828_174534
HEAD 379246c9
12/12 PASS
```

Valores:

| Target | Fase | Alpha6 |
|---|---|---:|
| Basic | managed cold | 60.683 s |
| Basic | managed warm nochange | 22.122 s |
| Basic | managed warm touch | 22.922 s |
| Basic | explicit cold | 60.369 s |
| Basic | explicit warm nochange | 21.774 s |
| Basic | explicit warm touch | 21.813 s |
| Core | managed cold | 68.545 s |
| Core | managed warm nochange | 20.803 s |
| Core | managed warm touch | 20.589 s |
| Core | explicit cold | 62.366 s |
| Core | explicit warm nochange | 20.065 s |
| Core | explicit warm touch | 19.905 s |

## Regresión inicial detectada en Alpha8

La primera implementación de `JWPLC_IO` / `JWPLC_Time` añadió un TU separado `JWPLC_RuntimeView.cpp`.

Resultado observado:

```text
Basic cold: 16 compilaciones
Core cold:  79 compilaciones
```

Se eliminó el TU adicional moviendo la implementación de las vistas a `JWPLC_GlobalPeripherals.cpp`, manteniendo el header público `JWPLC_RuntimeView.h`.

Resultado:

```text
Basic cold: 15 compilaciones
Core cold:  78 compilaciones
Warm:        1 compilación
```

Marcadores:

```text
ALPHA8_COLD_TU_REGRESSION=RESOLVED
ALPHA8_COMPILER_COUNT_PARITY=PASS
```

## Benchmark Alpha8 previo al lazy-link

Run:

```text
tools/build-speed-benchmark/results/20260903_214913
```

| Target | Fase | Tiempo | Compiladores |
|---|---|---:|---:|
| Basic | managed cold | 62.087 s | 15 |
| Basic | managed warm nochange | 24.004 s | 1 |
| Basic | managed warm touch | 22.872 s | 1 |
| Basic | explicit cold | 61.263 s | 15 |
| Basic | explicit warm nochange | 23.947 s | 1 |
| Basic | explicit warm touch | 23.599 s | 1 |
| Core | managed cold | 71.932 s | 78 |
| Core | managed warm nochange | 24.026 s | 1 |
| Core | managed warm touch | 24.109 s | 1 |
| Core | explicit cold | 77.915 s | 78 |
| Core | explicit warm nochange | 27.395 s | 1 |
| Core | explicit warm touch | 25.495 s | 1 |

La estructura de compilación ya había recuperado la paridad de TUs, pero el sketch `01_empty` seguía enlazando parte de la nueva HMI.

## Lazy-link del motor HMI

Se desacopló `JWPLC_Display.cpp` de llamadas directas a `JWPLCUI::*` mediante hooks internos con implementación weak por defecto en Display y strong cuando la API HMI es realmente utilizada.

Además, constructores/helpers HMI dejaron de estar implementados inline en `JWPLC_UI.h` y pasaron a `JWPLC_UI_API.cpp`.

API pública preservada:

```text
JWPLC_Display.setFields(...)
JWPLC_Display.setValue(...)
JWPLC_Display.setText(...)
JWPLC_Display.setBool(...)
JWPLC_Display.setBar(...)
JWPLC_Display.setUserPage(...)
```

## Gate source lazy-link

Resultado:

```text
JWPLC_DISPLAY_CPP_REFERENCES=1
JWPLC_UI_CPP_REFERENCES=1
JWPLC_UI_API_CPP_REFERENCES=1
OLD_DISPLAY_ARCHIVE_USED=False
ALPHA8_LAZYLINK_SOURCE_COMPILE=PASS
```

Las tres TUs son esperadas en el gate HMI porque ese sketch sí usa la HMI.

## Archive Display candidato Alpha8

Generado con:

```powershell
.\Build-JWPLCPrecompiledLibraries.ps1 -Libraries JWPLC_Display
```

Resultado P1:

```text
JWPLC_Display: 4 objetos
Bytes: 642576
SHA256: D47B72A48B82DA99952A3C5FD042D3D5864DFEB5BB786B95BC5684F9AEDC73BC
```

Miembros esperados:

```text
JWPLC_Display.cpp.o
JWPLC_IdleScreen.cpp.o
JWPLC_UI.cpp.o
JWPLC_UI_API.cpp.o
```

El archive fue usado en los gates físicos Alpha8 sin recompilar fuentes Display.

## Gate precompiled lazy-link

Resultado:

```text
EMPTY_DISPLAY_PRECOMPILED=True
HMI_DISPLAY_PRECOMPILED=True
EMPTY_DISPLAY_SOURCE_COMPILES=0
HMI_DISPLAY_SOURCE_COMPILES=0

EMPTY_UI_ENGINE_REFERENCES=0
EMPTY_UI_API_REFERENCES=0
HMI_UI_ENGINE_REFERENCES=175
HMI_UI_API_REFERENCES=118

EMPTY_APP_BYTES=396240
HMI_APP_BYTES=419200
```

Comparación del sketch vacío respecto al Alpha8 previo al lazy-link:

```text
399696 -> 396240 bytes
APP_DELTA_BYTES=-3456
```

Marcadores:

```text
ALPHA8_LAZYLINK_SOURCE_COMPILE=PASS
ALPHA8_LAZYLINK_PRECOMPILED=PASS
ALPHA8_EMPTY_HMI_ENGINE_LINKED=NO
ALPHA8_HMI_GATE_ENGINE_LINKED=YES
ALPHA8_EMPTY_APP_REDUCTION_BYTES=3456
ALPHA8_PUBLIC_HMI_API=PRESERVED
```

## Réplicas de wall-clock

Run R1:

```text
tools/build-speed-benchmark/results/20260903_223853
```

| Target | Fase | Tiempo | Compiladores |
|---|---|---:|---:|
| Basic | managed cold | 68.481 s | 15 |
| Basic | managed warm nochange | 26.470 s | 1 |
| Basic | managed warm touch | 27.016 s | 1 |
| Basic | explicit cold | 74.897 s | 15 |
| Basic | explicit warm nochange | 28.115 s | 1 |
| Basic | explicit warm touch | 30.423 s | 1 |
| Core | managed cold | 91.783 s | 78 |
| Core | managed warm nochange | 24.966 s | 1 |
| Core | managed warm touch | 25.320 s | 1 |
| Core | explicit cold | 84.666 s | 78 |
| Core | explicit warm nochange | 28.278 s | 1 |
| Core | explicit warm touch | 27.412 s | 1 |

Run R2:

```text
tools/build-speed-benchmark/results/20260903_225456
Host inicial: Intel i5-13400F, 10 % load, plan Equilibrado
```

| Target | Fase | Tiempo | Compiladores |
|---|---|---:|---:|
| Basic | managed cold | 70.698 s | 15 |
| Basic | managed warm nochange | 24.073 s | 1 |
| Basic | managed warm touch | 23.811 s | 1 |
| Basic | explicit cold | 65.384 s | 15 |
| Basic | explicit warm nochange | 24.376 s | 1 |
| Basic | explicit warm touch | 24.994 s | 1 |
| Core | managed cold | 84.020 s | 78 |
| Core | managed warm nochange | 26.761 s | 1 |
| Core | managed warm touch | 26.112 s | 1 |
| Core | explicit cold | 77.398 s | 78 |
| Core | explicit warm nochange | 27.830 s | 1 |
| Core | explicit warm touch | 26.997 s | 1 |

## Interpretación

Los tiempos absolutos presentan variación significativa entre ejecuciones del mismo host, especialmente en cold y en el modo explicit. Esa variación no se acompaña de cambios en la cantidad de compilaciones reales.

Por ello Alpha8 no reclama una mejora global de segundos frente a Alpha6.

La conclusión defendible es estructural:

```text
Basic cold = 15 TUs
Core cold  = 78 TUs
Warm       = 1 TU
```

Y de link:

```text
HMI no usada -> motor HMI no extraído
HMI usada    -> motor HMI enlazado
```

## Decisión de freeze

```text
ALPHA8_BUILD_ARCHITECTURE_REGRESSION=NO
ALPHA8_COMPILER_COUNT_PARITY_WITH_ALPHA6=PASS
ALPHA8_RUNTIMEVIEW_TU_REGRESSION=RESOLVED
ALPHA8_EMPTY_UI_ENGINE_LINKED=NO
ALPHA8_BUILD_WALLCLOCK_VARIANCE=HOST_SENSITIVE
ALPHA8_PERFORMANCE_FREEZE=PASS
```

No se realizan más refactors de arquitectura por velocidad dentro de Alpha8.

## Decisiones heredadas que no cambian

```text
APP_ONLY=VALIDATED_DEVELOPMENT_TOOL
APP_ONLY_DEFAULT_UPLOAD=NO

BOOTLOADER_PRECOMPILED=NOT_ADOPTED
BOOTLOADER_GENERATION=SDK_ELF_AUTOMATIC

CURRENT_FLASH_PROFILE=VALIDATED_CURRENT_PROFILE
FINAL_UNIVERSAL_FLASH_CONFIGURATION=PENDING

OTA=NOT_DEFINED
```

No se publica `bootloader.bin` como definitivo y no se retira ningún periférico del autoload normal para ganar tiempo de compilación.
