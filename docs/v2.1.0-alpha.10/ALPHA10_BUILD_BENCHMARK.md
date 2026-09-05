# Alpha10 - Benchmark de compilación

Fecha de cierre técnico: 2026-09-05.

## Objetivo

Revalidar `v2.1.0-alpha.10` después de retirar el guard de shadowing de `JWPLC_Ethernet` y comprobar que el ciclo normal de Arduino IDE / Arduino CLI vuelve al entorno de tiempos de Alpha9 sin sacrificar periféricos ni compatibilidad.

## Evidencia histórica

Durante la primera variante de Alpha10 se midió el coste de markers bundled sobre `tools/build-speed-benchmark/sketches/01_empty`:

| Variante | Markers adicionales JW/JWPLC | Warm promedio | Delta vs M0 |
|---|---:|---:|---:|
| `M0_NONE` | 0 | 22.094 s | base |
| `M1_ETH` | 1 | 23.327 s | +1.233 s / +5.6% |
| `M4_OBSERVED_STALE` | 4 | 26.888 s | +4.794 s / +21.7% |
| `M7_ALL` | 7 | 30.353 s | +8.259 s / +37.4% |

La estructura de compilación y el tamaño binario permanecían equivalentes; el efecto era principalmente coste de library discovery/warm build.

## Decisión Alpha10 final

```text
ALPHA10_MARKER_SET_JW_JWPLC=NONE
JWPLC_ETHERNET_SHADOW_GUARD=REMOVED
JWPLC_ETHERNET_LIBRARY_VERSION=1.0.0
ADAFRUIT_BUNDLED_MARKERS=RETAINED
AUTOLOAD_PERIPHERALS_REMOVED=NO
SUPPORTED_LIBRARY_MODEL=PACKAGE_MANAGED
MANUAL_JW_JWPLC_OVERRIDES=OUT_OF_SCOPE
```

Commit técnico:

```text
35385c7286c8a4fdf33aec1af1175b8bb4f45e64
```

## Validación de fuente

```text
BRANCH=v2.1.0-alpha.10/optimize/remove-shadow-guard
HEAD=456d5b9f55088091fcadcb87e9f33ffb90d3754c
DIRTY_COUNT=0
ETH_MARKER_EXISTS=False
GLOBAL_HAS_ETH_MARKER=False
ETH_VERSION_1_0_0=True
ADAFRUIT_MARKERS_MISSING=0
ALPHA10_CLEANUP_SOURCE=PASS
```

## Matriz de benchmark

Herramienta y parámetros:

```text
Arduino CLI: C:\Program Files\Arduino PLC IDE Tools\arduino-cli.exe
Package namespace: jwplc_local
Jobs: 0
Targets: Basic, Core
Sketches: 01_empty, 02_io_basic
Réplicas: 3
```

Runs:

```text
r1: tools/build-speed-benchmark/results/20260905_110955
r2: tools/build-speed-benchmark/results/20260905_112529
r3: tools/build-speed-benchmark/results/20260905_113956
```

Todas las fases reportadas terminaron `OK`.

## Basic / 01_empty

| Fase | r1 | r2 | r3 | Promedio |
|---|---:|---:|---:|---:|
| managed cold | 73.049 s | 56.007 s | 59.750 s | 62.935 s |
| managed warm no-change | 25.110 s | 22.360 s | 24.575 s | 24.015 s |
| managed warm touch | 24.126 s | 21.860 s | 23.866 s | 23.284 s |
| explicit cold | 62.940 s | 56.853 s | 58.808 s | 59.534 s |
| explicit warm no-change | 24.559 s | 22.636 s | 23.811 s | 23.669 s |
| explicit warm touch | 25.437 s | 22.890 s | 23.359 s | 23.895 s |

```text
Basic cold = 15 compiladores
Basic warm = 1 compilador
```

## Basic / 02_io_basic

| Fase | r1 | r2 | r3 | Promedio |
|---|---:|---:|---:|---:|
| managed cold | 68.030 s | 57.708 s | 57.137 s | 60.958 s |
| managed warm no-change | 24.919 s | 23.286 s | 23.095 s | 23.767 s |
| managed warm touch | 24.141 s | 23.030 s | 22.271 s | 23.147 s |
| explicit cold | 65.671 s | 60.065 s | 56.733 s | 60.823 s |
| explicit warm no-change | 24.943 s | 24.017 s | 22.276 s | 23.745 s |
| explicit warm touch | 24.391 s | 23.471 s | 22.138 s | 23.333 s |

```text
Basic cold = 15 compiladores
Basic warm = 1 compilador
```

## Core / 01_empty

| Fase | r1 | r2 | r3 | Promedio |
|---|---:|---:|---:|---:|
| managed cold | 75.036 s | 67.774 s | 63.950 s | 68.920 s |
| managed warm no-change | 23.986 s | 22.576 s | 21.988 s | 22.850 s |
| managed warm touch | 22.660 s | 23.156 s | 22.150 s | 22.655 s |
| explicit cold | 70.029 s | 65.729 s | 64.294 s | 66.684 s |
| explicit warm no-change | 23.323 s | 22.339 s | 22.016 s | 22.559 s |
| explicit warm touch | 22.122 s | 22.911 s | 22.455 s | 22.496 s |

```text
Core cold = 78 compiladores
Core warm = 1 compilador
```

## Core / 02_io_basic

| Fase | r1 | r2 | r3 | Promedio |
|---|---:|---:|---:|---:|
| managed cold | 63.902 s | 66.243 s | 66.399 s | 65.515 s |
| managed warm no-change | 22.296 s | 22.820 s | 23.227 s | 22.781 s |
| managed warm touch | 22.905 s | 22.599 s | 23.148 s | 22.884 s |
| explicit cold | 67.650 s | 67.505 s | 67.760 s | 67.638 s |
| explicit warm no-change | 23.288 s | 22.316 s | 22.231 s | 22.612 s |
| explicit warm touch | 22.264 s | 22.748 s | 23.271 s | 22.761 s |

```text
Core cold = 78 compiladores
Core warm = 1 compilador
```

## BinaryBytes

Los `results.csv` de las tres réplicas muestran paridad exacta en `explicit_cold`:

| Target | Sketch | r1 | r2 | r3 | Paridad |
|---|---|---:|---:|---:|---|
| Basic | `01_empty` | 4,618,688 | 4,618,688 | 4,618,688 | PASS |
| Basic | `02_io_basic` | 4,618,784 | 4,618,784 | 4,618,784 | PASS |
| Core | `01_empty` | 4,574,464 | 4,574,464 | 4,574,464 | PASS |
| Core | `02_io_basic` | 4,574,576 | 4,574,576 | 4,574,576 | PASS |

```text
ALPHA10_BINARY_SIZE_PARITY=PASS
```

## Matriz funcional local

Sobre el candidato se compiló con `jwplc_local:esp32:jwplcbasic`:

```text
DigitalIO_Basic=PASS
Buttons_Basic=PASS
Display_HMI_Fields=PASS
Ethernet_Diagnostics=PASS
RemoteIO_Slave_RTU=PASS
COMPILE_TOTAL=5
COMPILE_PASS=5
COMPILE_FAIL=0
UNDEFINED_REFERENCE_HITS=0
ALPHA10_LOCAL_FUNCTIONAL_MATRIX=5/5_PASS
ALPHA10_LOCAL_COMPILE_GATE=PASS
```

## Interpretación

La estructura esperada se conserva exactamente:

```text
Basic cold: 15 compiladores
Core cold:  78 compiladores
Warm:        1 compilador
COMPILER_STRUCTURE_PARITY=PASS
```

El primer run muestra deriva de host apreciable, especialmente en cold. Por ello no se usa un único wall-clock como afirmación causal.

Para el ciclo habitual de edición, `Basic / 01_empty / managed_warm_touch` promedió `23.284 s` en r1-r3. En r2-r3, con el host más estabilizado, el promedio fue `22.863 s`, cercano a `M0_NONE=22.094 s` y por debajo de `M1_ETH=23.327 s`.

Conclusión:

```text
JW_JWPLC_DISCOVERY_MARKER_OVERHEAD=REMOVED
COMPILER_STRUCTURE_PARITY=PASS
BINARY_SIZE_PARITY=PASS
LOCAL_FUNCTIONAL_MATRIX=5/5_PASS
UNDEFINED_REFERENCE_HITS=0
WARM_BEHAVIOR_RETURNED_TO_M0_RANGE=PASS_WITH_HOST_VARIATION
EXACT_PERCENT_RECOVERY_CLAIM=NOT_USED
ALPHA10_BUILD_BENCHMARK_FINAL=PASS
```

## Arduino IDE y gate físico

Después del benchmark se utilizó Arduino IDE para compilar y subir el gate de autoload normal:

```text
tools/build-speed-benchmark/sketches/06_alpha4_local_physical_gate/06_alpha4_local_physical_gate.ino
```

Resultado:

```text
ALPHA4_DISPLAY_READY=PASS
ALPHA4_RTC=PASS
ALPHA4_FRAM=PASS
ALPHA4_SD=PASS
ALPHA4_BUTTONS=PASS
ALPHA4_INPUTS=PASS
ALPHA4_OUTPUTS=PASS
ALPHA4_DISPLAY_VISUAL=PASS
ALPHA4_LOCAL_PHYSICAL_GATE=PASS
```

```text
ALPHA10_ARDUINO_IDE_VALIDATION=PASS
ALPHA10_PHYSICAL_VALIDATION=PASS_WITH_SCOPED_INHERITED_ETH_RTU_EVIDENCE
```

Ethernet y RS-485/Modbus no se someten a nuevo stress porque Alpha10 no modifica esos runtimes. La regresión de compilación sí cubre `Ethernet_Diagnostics` y `RemoteIO_Slave_RTU`, y se conserva la evidencia física cerrada en Alpha6/Alpha7/Alpha9.

Detalle: `ALPHA10_PHYSICAL_VALIDATION_20260905.md`.

## CI

`CI JWPLC Package Smoke` quedó verde hasta:

```text
09ba7395450ce9d85a174dbd96a57f255371590c
```

El commit documental de cierre debe quedar verde antes de merge.

## Estado

```text
ALPHA10_CLEANUP_SOURCE=PASS
ALPHA10_BENCHMARK_RUNS=3_PASS
ALPHA10_COMPILER_STRUCTURE_PARITY=PASS
ALPHA10_BINARY_SIZE_PARITY=PASS
ALPHA10_LOCAL_FUNCTIONAL_MATRIX=5/5_PASS
ALPHA10_LOCAL_COMPILE_GATE=PASS
ALPHA10_BUILD_BENCHMARK_FINAL=PASS
ALPHA10_ARDUINO_IDE_VALIDATION=PASS
ALPHA10_PHYSICAL_VALIDATION=PASS_WITH_SCOPED_INHERITED_ETH_RTU_EVIDENCE
ALPHA10_TECHNICAL_CLOSURE=PASS
ALPHA10_PUBLICATION_REPLACEMENT=PENDING
```
