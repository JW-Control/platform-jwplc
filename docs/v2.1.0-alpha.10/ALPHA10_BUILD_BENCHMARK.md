# Alpha10 - Benchmark de compilación

## Objetivo actual

Revalidar `v2.1.0-alpha.10` después de retirar el guard de shadowing de `JWPLC_Ethernet` y comprobar que el ciclo normal de Arduino IDE / Arduino CLI vuelve al entorno de tiempos de Alpha9 sin sacrificar periféricos ni compatibilidad.

## Evidencia histórica que motivó el cambio

Durante el Alpha10 inicial se midió el coste de markers bundled en `tools/build-speed-benchmark/sketches/01_empty`:

| Variante | Markers adicionales JW/JWPLC | Warm promedio | Delta vs M0 |
|---|---:|---:|---:|
| `M0_NONE` | 0 | 22.094 s | base |
| `M1_ETH` | 1 | 23.327 s | +1.233 s / +5.6% |
| `M4_OBSERVED_STALE` | 4 | 26.888 s | +4.794 s / +21.7% |
| `M7_ALL` | 7 | 30.353 s | +8.259 s / +37.4% |

La estructura de compilación y el tamaño binario se mantuvieron equivalentes. El efecto observado fue principalmente coste de library discovery/warm build.

## Decisión de esta revisión

Se retira `M1_ETH` y se vuelve al comportamiento de Alpha9 para `JWPLC_GlobalPeripherals_Auto.h`.

```text
ALPHA10_MARKER_SET_JW_JWPLC=NONE
JWPLC_ETHERNET_SHADOW_GUARD=REMOVED
JWPLC_ETHERNET_LIBRARY_VERSION=1.0.0
ADAFRUIT_BUNDLED_MARKERS=RETAINED
AUTOLOAD_PERIPHERALS_REMOVED=NO
```

Los markers Adafruit no pertenecen a esta eliminación: protegen dependencias externas precompiladas que pueden coexistir legítimamente con copias del sketchbook.

Commit técnico:

```text
35385c7286c8a4fdf33aec1af1175b8bb4f45e64
```

## Validación de fuente

El 2026-09-05 se validó el candidato sobre la rama:

```text
v2.1.0-alpha.10/optimize/remove-shadow-guard
HEAD=456d5b9f55088091fcadcb87e9f33ffb90d3754c
DIRTY_COUNT=0
ETH_MARKER_EXISTS=False
GLOBAL_HAS_ETH_MARKER=False
ETH_VERSION_1_0_0=True
ADAFRUIT_MARKERS_MISSING=0
ALPHA10_CLEANUP_SOURCE=PASS
```

## Matriz ejecutada

Host y herramienta:

```text
Arduino CLI: C:\Program Files\Arduino PLC IDE Tools\arduino-cli.exe
Package namespace: jwplc_local
Jobs: 0
Targets: Basic, Core
Sketches: 01_empty, 02_io_basic
Réplicas: 3
```

Resultados locales:

```text
r1: tools/build-speed-benchmark/results/20260905_110955
r2: tools/build-speed-benchmark/results/20260905_112529
r3: tools/build-speed-benchmark/results/20260905_113956
```

Todas las fases reportadas terminaron `OK`.

## Resultados - Basic / 01_empty

| Fase | r1 | r2 | r3 | Promedio |
|---|---:|---:|---:|---:|
| managed cold | 73.049 s | 56.007 s | 59.750 s | 62.935 s |
| managed warm no-change | 25.110 s | 22.360 s | 24.575 s | 24.015 s |
| managed warm touch | 24.126 s | 21.860 s | 23.866 s | 23.284 s |
| explicit cold | 62.940 s | 56.853 s | 58.808 s | 59.534 s |
| explicit warm no-change | 24.559 s | 22.636 s | 23.811 s | 23.669 s |
| explicit warm touch | 25.437 s | 22.890 s | 23.359 s | 23.895 s |

Compiler invocations:

```text
Basic cold = 15
Basic warm = 1
```

en las tres réplicas.

## Resultados - Basic / 02_io_basic

| Fase | r1 | r2 | r3 | Promedio |
|---|---:|---:|---:|---:|
| managed cold | 68.030 s | 57.708 s | 57.137 s | 60.958 s |
| managed warm no-change | 24.919 s | 23.286 s | 23.095 s | 23.767 s |
| managed warm touch | 24.141 s | 23.030 s | 22.271 s | 23.147 s |
| explicit cold | 65.671 s | 60.065 s | 56.733 s | 60.823 s |
| explicit warm no-change | 24.943 s | 24.017 s | 22.276 s | 23.745 s |
| explicit warm touch | 24.391 s | 23.471 s | 22.138 s | 23.333 s |

Compiler invocations:

```text
Basic cold = 15
Basic warm = 1
```

## Resultados - Core / 01_empty

| Fase | r1 | r2 | r3 | Promedio |
|---|---:|---:|---:|---:|
| managed cold | 75.036 s | 67.774 s | 63.950 s | 68.920 s |
| managed warm no-change | 23.986 s | 22.576 s | 21.988 s | 22.850 s |
| managed warm touch | 22.660 s | 23.156 s | 22.150 s | 22.655 s |
| explicit cold | 70.029 s | 65.729 s | 64.294 s | 66.684 s |
| explicit warm no-change | 23.323 s | 22.339 s | 22.016 s | 22.559 s |
| explicit warm touch | 22.122 s | 22.911 s | 22.455 s | 22.496 s |

Compiler invocations:

```text
Core cold = 78
Core warm = 1
```

en las tres réplicas.

## Resultados - Core / 02_io_basic

| Fase | r1 | r2 | r3 | Promedio |
|---|---:|---:|---:|---:|
| managed cold | 63.902 s | 66.243 s | 66.399 s | 65.515 s |
| managed warm no-change | 22.296 s | 22.820 s | 23.227 s | 22.781 s |
| managed warm touch | 22.905 s | 22.599 s | 23.148 s | 22.884 s |
| explicit cold | 67.650 s | 67.505 s | 67.760 s | 67.638 s |
| explicit warm no-change | 23.288 s | 22.316 s | 22.231 s | 22.612 s |
| explicit warm touch | 22.264 s | 22.748 s | 23.271 s | 22.761 s |

Compiler invocations:

```text
Core cold = 78
Core warm = 1
```

## Interpretación

El candidato conserva exactamente la estructura esperada de compilación:

```text
Basic cold: 15 compiladores
Core cold:  78 compiladores
Warm:        1 compilador
COMPILER_STRUCTURE_PARITY=PASS
```

El primer run muestra deriva de host apreciable, especialmente en cold. Por ello no se usa un único wall-clock como afirmación causal.

Para el ciclo de edición habitual, `Basic / 01_empty / managed_warm_touch` promedió `23.284 s` en las tres réplicas. En r2+r3, una vez estabilizado el host, el promedio fue `22.863 s`, cercano al baseline histórico `M0_NONE=22.094 s` y por debajo del estado `M1_ETH=23.327 s`.

La conclusión defendible es:

```text
JW_JWPLC_DISCOVERY_MARKER_OVERHEAD=REMOVED
COMPILER_STRUCTURE_PARITY=PASS
WARM_BEHAVIOR_RETURNED_TO_M0_RANGE=PASS_WITH_HOST_VARIATION
EXACT_PERCENT_RECOVERY_CLAIM=NOT_USED
```

No se reclama una recuperación exacta de `5.6%` porque las mediciones actuales y las históricas no son una comparación A/B simultánea. Sí se confirma que el guard que causaba el coste fue eliminado y que los warm builds estabilizados vuelven al entorno de tiempos de M0.

## Pendientes del benchmark de cierre

Antes de cerrar Alpha10 faltan únicamente los datos que no aparecen en la salida resumida recibida:

- comparar `BinaryBytes` desde los `results.csv`;
- ejecutar la matriz funcional 5/5 sobre el candidato;
- validación Arduino IDE;
- smoke físico.

## Estado

```text
ALPHA10_BUILD_BENCHMARK_HISTORICAL_EVIDENCE=RECORDED
ALPHA10_CLEANUP_SOURCE=PASS
ALPHA10_BENCHMARK_RUNS=3_PASS
ALPHA10_COMPILER_STRUCTURE_PARITY=PASS
ALPHA10_WARM_BEHAVIOR=PASS_WITH_HOST_VARIATION
ALPHA10_BINARY_SIZE_CHECK=PENDING
ALPHA10_FUNCTIONAL_MATRIX=PENDING
ALPHA10_BUILD_BENCHMARK_FINAL=PENDING_REMAINING_GATES
```
