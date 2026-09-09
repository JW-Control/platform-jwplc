# Alpha11 - Benchmark final de compilación

Fecha de cierre técnico: 2026-09-09.
Branch: `v2.1.0-alpha.11/feature/hmi-designer`.

## Objetivo

Medir el comportamiento final de compilación de Alpha11 después de:

- cerrar la API moderna de `JWPLC_Display`;
- migrar ejemplos públicos fuera de callbacks históricos;
- convertir los diagnósticos TFT de Ethernet a fields declarativos/on-demand;
- retirar includes redundantes en ejemplos integrados;
- publicar `JWPLC_Display` como archive precompilado final.

No se retiró ningún periférico del autoload normal del JWPLC Basic.

## Archive precompilado evaluado

Commit que publica el archive:

```text
4142f801fbacc9388bf63c3a6352696522d6b445
```

Archive:

```text
JWPLC/2.1.0/libraries/JWPLC_Display/src/esp32/libJWPLC_Display.a
```

Identidad validada:

```text
ARCHIVE_BYTES=849596
ARCHIVE_SHA256=2974d42c847c1b7c7ab3a7b74da42e2f17969fb852b47a8d434f57f70da924af
DISPLAY_TUS=6
ARCHIVE_MEMBERS_EXACT=PASS
PRECOMPILED_DISPLAY_SOURCE_TUS=0
SOURCE_ARCHIVE_EMPTY_PARITY=PASS
SOURCE_ARCHIVE_HMI_PARITY=PASS
ALPHA11_DISPLAY_FINAL_ARCHIVE=PASS
```

El archive contiene exactamente las seis unidades de traducción de `JWPLC_Display` esperadas y, en modo precompilado, Arduino CLI no recompila fuentes Display.

## Metodología

Herramienta:

```text
tools/build-speed-benchmark/Run-JWPLCBuildBenchmark.ps1
```

Parámetros:

```text
Arduino CLI: C:\Program Files\Arduino PLC IDE Tools\arduino-cli.exe
Package namespace: jwplc_local
Jobs: 0
Targets: Basic, Core
Sketches: 01_empty, 02_io_basic
Réplicas: 3
Uploads: omitidos
```

Cada target/sketch ejecuta:

```text
managed_cold
managed_warm_nochange
managed_warm_touch
explicit_cold
explicit_warm_nochange
explicit_warm_touch
```

Total:

```text
2 targets x 2 sketches x 6 fases x 3 replicas = 72 fases
```

Resultado global:

```text
TOTAL_PHASES=72
FAILED_PHASES=0
ALPHA11_BUILD_BENCHMARK_3X=PASS
```

Runs locales:

```text
tools/build-speed-benchmark/results/20260908_232422
tools/build-speed-benchmark/results/20260908_233908
tools/build-speed-benchmark/results/20260908_235424
```

## Resultados Alpha11

### Basic / 01_empty

| Fase | Promedio | Mínimo | Máximo | Compiladores | BinaryBytes |
|---|---:|---:|---:|---:|---:|
| managed cold | 58.900 s | 58.690 s | 59.130 s | 15 | n/a |
| managed warm no-change | 22.930 s | 22.370 s | 23.440 s | 1 | n/a |
| managed warm touch | 22.880 s | 22.180 s | 23.570 s | 1 | n/a |
| explicit cold | 61.240 s | 59.750 s | 62.030 s | 15 | 4,618,720 |
| explicit warm no-change | 24.170 s | 23.350 s | 24.600 s | 1 | 4,618,720 |
| explicit warm touch | 24.340 s | 22.910 s | 25.080 s | 1 | 4,618,720 |

### Basic / 02_io_basic

| Fase | Promedio | Mínimo | Máximo | Compiladores | BinaryBytes |
|---|---:|---:|---:|---:|---:|
| managed cold | 61.970 s | 58.650 s | 67.090 s | 15 | n/a |
| managed warm no-change | 24.160 s | 23.390 s | 25.660 s | 1 | n/a |
| managed warm touch | 23.820 s | 23.290 s | 24.210 s | 1 | n/a |
| explicit cold | 63.370 s | 62.730 s | 63.900 s | 15 | 4,618,832 |
| explicit warm no-change | 23.950 s | 22.390 s | 24.940 s | 1 | 4,618,832 |
| explicit warm touch | 23.160 s | 22.060 s | 24.010 s | 1 | 4,618,832 |

### Core / 01_empty

| Fase | Promedio | Mínimo | Máximo | Compiladores | BinaryBytes |
|---|---:|---:|---:|---:|---:|
| managed cold | 71.000 s | 66.390 s | 73.490 s | 78 | n/a |
| managed warm no-change | 22.600 s | 22.000 s | 23.480 s | 1 | n/a |
| managed warm touch | 22.500 s | 21.460 s | 23.430 s | 1 | n/a |
| explicit cold | 69.920 s | 69.620 s | 70.350 s | 78 | 4,574,512 |
| explicit warm no-change | 22.640 s | 22.060 s | 23.180 s | 1 | 4,574,512 |
| explicit warm touch | 22.350 s | 21.800 s | 23.100 s | 1 | 4,574,512 |

### Core / 02_io_basic

| Fase | Promedio | Mínimo | Máximo | Compiladores | BinaryBytes |
|---|---:|---:|---:|---:|---:|
| managed cold | 68.640 s | 68.140 s | 69.440 s | 78 | n/a |
| managed warm no-change | 22.750 s | 22.330 s | 23.120 s | 1 | n/a |
| managed warm touch | 23.200 s | 21.950 s | 24.680 s | 1 | n/a |
| explicit cold | 70.240 s | 68.440 s | 73.110 s | 78 | 4,574,624 |
| explicit warm no-change | 23.290 s | 22.350 s | 24.200 s | 1 | 4,574,624 |
| explicit warm touch | 23.220 s | 22.660 s | 23.880 s | 1 | 4,574,624 |

## Comparación contra Alpha10

Referencia: `docs/v2.1.0-alpha.10/ALPHA10_BUILD_BENCHMARK.md`.

La estructura global de compilación se mantiene:

```text
Basic cold = 15 compiladores
Core cold  = 78 compiladores
Warm       = 1 compilador
```

Por tanto:

```text
ALPHA11_COMPILER_STRUCTURE_PARITY=PASS
```

### Ciclo habitual de edición (`managed_warm_touch`)

| Target / Sketch | Alpha10 | Alpha11 | Delta | Delta % |
|---|---:|---:|---:|---:|
| Basic / 01_empty | 23.284 s | 22.880 s | -0.404 s | -1.74% |
| Basic / 02_io_basic | 23.147 s | 23.820 s | +0.673 s | +2.91% |
| Core / 01_empty | 22.655 s | 22.500 s | -0.155 s | -0.68% |
| Core / 02_io_basic | 22.884 s | 23.200 s | +0.316 s | +1.38% |

Promediando ambos sketches por target:

```text
Basic managed_warm_touch: Alpha10 23.216 s -> Alpha11 23.350 s  (+0.58%)
Core  managed_warm_touch: Alpha10 22.770 s -> Alpha11 22.850 s  (+0.35%)
```

### `explicit_warm_touch`

Promedio de ambos sketches:

```text
Basic: Alpha10 23.614 s -> Alpha11 23.750 s  (+0.58%)
Core : Alpha10 22.629 s -> Alpha11 22.785 s  (+0.69%)
```

### Cold

Los tiempos cold muestran variación de host y no se interpretan como una regresión causal única:

```text
Basic managed cold:  Alpha10 61.947 s -> Alpha11 60.435 s  (-2.44%)
Basic explicit cold: Alpha10 60.179 s -> Alpha11 62.305 s  (+3.53%)
Core managed cold:   Alpha10 67.218 s -> Alpha11 69.820 s  (+3.87%)
Core explicit cold:  Alpha10 67.161 s -> Alpha11 70.080 s  (+4.35%)
```

Los sentidos opuestos entre managed/explicit y la dispersión entre réplicas confirman que no debe presentarse un porcentaje de aceleración o regresión cold como conclusión de producto.

## Tamaño binario

Comparación `explicit_cold` Alpha10 -> Alpha11:

| Target | Sketch | Alpha10 | Alpha11 | Delta |
|---|---|---:|---:|---:|
| Basic | 01_empty | 4,618,688 | 4,618,720 | +32 bytes |
| Basic | 02_io_basic | 4,618,784 | 4,618,832 | +48 bytes |
| Core | 01_empty | 4,574,464 | 4,574,512 | +48 bytes |
| Core | 02_io_basic | 4,574,576 | 4,574,624 | +48 bytes |

La diferencia es mínima y consistente con el crecimiento funcional de Alpha11. No se observa un aumento material del tamaño de aplicación.

```text
ALPHA11_BINARY_SIZE_REGRESSION=MATERIAL_NO
```

## Interpretación del precompilado Display

El beneficio demostrado del archive no debe confundirse con una reducción garantizada del wall-clock total del Arduino Builder.

Sí quedó demostrado de forma directa que:

```text
PRECOMPILED_DISPLAY_SOURCE_TUS=0
ARCHIVE_MEMBERS_EXACT=PASS
SOURCE_ARCHIVE_EMPTY_PARITY=PASS
SOURCE_ARCHIVE_HMI_PARITY=PASS
```

Es decir, las fuentes de `JWPLC_Display` dejan de recompilarse cuando el archive es utilizado y el binario conserva paridad con la variante source.

Sin embargo, el contador agregado del benchmark continúa en 15 compiladores cold para Basic y 78 para Core. Esto indica que el coste total de build/discovery/link del package sigue dominado por otras partes del ecosistema y por el propio Arduino Builder. Por ello Alpha11 no reclama una aceleración global porcentual específica.

En warm builds, los promedios permanecen prácticamente en el mismo rango de Alpha10 (deltas agregados menores a 1% en `*_warm_touch`). Esto se considera estabilidad de rendimiento, no una regresión relevante.

## Gates relacionados ya cerrados

Antes del benchmark final:

```text
LEGACY_DISPLAY_FILES=0
ALPHA11_EXAMPLES_LEGACY_API=PASS
TOTAL_EXAMPLES_CLEANUP=57
ALPHA11_JWPLC_EXAMPLES_COMPILE=PASS
```

La matriz de 57 ejemplos modificados por la limpieza de includes terminó:

```text
TOTAL=57
PASS=57
FAIL=0
```

## Conclusión

```text
ALPHA11_BUILD_BENCHMARK_3X=PASS
ALPHA11_COMPILER_STRUCTURE_PARITY=PASS
ALPHA11_WARM_PERFORMANCE_STABLE=PASS
ALPHA11_BINARY_SIZE_REGRESSION=MATERIAL_NO
ALPHA11_DISPLAY_PRECOMPILED_SOURCE_AVOIDANCE=PASS
ALPHA11_DISPLAY_SOURCE_ARCHIVE_PARITY=PASS
ALPHA11_EXACT_SPEEDUP_CLAIM=NOT_USED
```

Alpha11 conserva el rendimiento warm de Alpha10 dentro de la variación normal del host y añade un archive Display reproducible, validado y funcionalmente equivalente a source.

El precompilado se acepta por reducción verificable de recompilación de las TUs de Display y por estabilidad de comportamiento, no por una afirmación artificial de mejora porcentual del tiempo total.
