# Alpha10 - Benchmark de compilación

## Objetivo

Validar que la corrección de shadowing de librerías no introduzca una regresión de compilación desproporcionada en el ciclo normal de Arduino IDE / Arduino CLI.

El caso base fue:

```text
Target: Basic y Core
Sketch: tools/build-speed-benchmark/sketches/01_empty
Namespace: jwplc_local
Mismo host / mismo Arduino CLI / Jobs=0
```

`01_empty` es representativo porque el autoload normal del JWPLC Basic sigue activo aun cuando el sketch no declare periféricos explícitamente.

## Comparación inicial Alpha9 vs propuesta de 7 markers

La primera propuesta protegía siete librerías bundled:

```text
JW_RTC
JW_FRAM
JW_MatrixButtons
JW_SD
JWPLC_Ethernet
JWPLC_RS485
JWPLC_ModbusRTU
```

La estructura de compilación permaneció idéntica:

```text
Basic cold: 15 compiladores -> 15
Core cold:  78 compiladores -> 78
Warm:        1 compilador    -> 1
BINARY_SIZE_DIFFERENCES=0
COMPILER_STRUCTURE_PARITY=PASS
BINARY_SIZE_PARITY=PASS
```

Sin embargo, el wall-clock warm aumentó de forma consistente. En la comparación completa se observaron incrementos aproximados entre `+26.4%` y `+40.1%` según target/fase.

Una contraprueba en orden inverso, ejecutando Alpha10 antes que Alpha9, confirmó el efecto:

```text
ALPHA10_WARM_AVG_S=30.996
ALPHA9_WARM_AVG_S=22.227
WARM_DELTA_S=8.769
WARM_DELTA_PCT=39.5
```

Por tanto, la regresión no se atribuye únicamente al orden de ejecución o deriva del host.

## Perfil por cantidad de markers

Se midió el coste de library discovery para cuatro configuraciones:

| Variante | Markers | Warm promedio | Delta vs 0 markers |
|---|---:|---:|---:|
| `M0_NONE` | 0 | 22.094 s | 0.0% |
| `M1_ETH` | 1 | 23.327 s | +1.233 s / +5.6% |
| `M4_OBSERVED_STALE` | 4 | 26.888 s | +4.794 s / +21.7% |
| `M7_ALL` | 7 | 30.353 s | +8.259 s / +37.4% |

Los tiempos cold se mantuvieron alrededor de 57-59 s y no mostraron una penalización equivalente.

## Decisión

Alpha10 adopta únicamente el marker de `JWPLC_Ethernet`.

```text
ALPHA10_MARKER_SET=JWPLC_ETHERNET_ONLY
BUNDLED_MARKER_COUNT=1
PROFILED_WARM_OVERHEAD=+5.6_PERCENT
PROFILED_WARM_DELTA=+1.233_SECONDS
GENERALIZED_7_MARKER_OPTION=REJECTED_BUILD_COST
GENERALIZED_7_MARKER_WARM_OVERHEAD=+37.4_PERCENT
```

Motivo:

- `JWPLC_Ethernet` fue la causa primaria reproducida del fallo de linker observado en el taller;
- el marker único mantiene el coste warm en un incremento acotado;
- proteger siete librerías mediante el mismo mecanismo añade un coste lineal de discovery demasiado alto;
- no se retiran periféricos del autoload;
- no se modifica runtime ni API pública;
- no cambian archives precompilados.

La protección general de otras librerías homónimas queda como línea de trabajo separada: si se aborda, debe buscar un mecanismo de menor coste antes de adoptarlo.

## Estado

```text
COMPILER_STRUCTURE_PARITY=PASS
BINARY_SIZE_PARITY=PASS
M1_ETHERNET_HOSTILE_SHADOW_TEST=PASS
ALPHA10_BUILD_BENCHMARK=PASS_WITH_SCOPED_FIX
```
