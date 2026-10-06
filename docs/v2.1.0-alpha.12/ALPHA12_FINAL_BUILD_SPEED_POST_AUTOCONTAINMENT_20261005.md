# Alpha12 — Benchmark final post-autocontención

Fecha: 2026-10-05

## Identidad

```text
BRANCH=v2.1.0-alpha.12/feature/modbus-tcp
HEAD=5dddbdcf407f62f51f7d4653b7690ad526333c15
RESULT_ROOT=tools/alpha12/results/final_build_speed_benchmark/20261005_123241
```

## Preflight

```text
ENTRY_DIRTY_COUNT=0
JW_FRAM_ARCHIVE_BYTES=126440
JW_FRAM_ARCHIVE_SHA256=b8734763bfa1287167feda72a5c9b30df97340d41ca0c3fd000321e218632ceb
JW_FRAM_ARCHIVE_SHA_PASS=True
BENCHMARK_RUNNER_PARSE=PASS
PRECOMPILED_FREEZE_PREFLIGHT=PASS
```

## Resultado

```text
ALPHA12_FINAL_BUILD_SPEED_BENCHMARK=PASS
ALPHA12_BUILD_SPEED_MATRIX_COMPLETE=YES
RESULT_ROW_COUNT=12
JOBS=0
UPLOADS=SKIPPED
PRECOMPILED_FREEZE=PASS
FINAL_TRACKED_DIRTY_COUNT=0
```

## Matriz final

| Target | Fase | Tiempo | Compiler invocations | Link invocations | Binary bytes |
|---|---|---:|---:|---:|---:|
| Basic | managed cold | 69.528 s | 16 | 1 | 0 |
| Basic | managed warm no-change | 17.856 s | 1 | 1 | 0 |
| Basic | managed warm touch | 17.799 s | 1 | 1 | 0 |
| Basic | explicit cold | 59.336 s | 16 | 1 | 4633648 |
| Basic | explicit warm no-change | 17.781 s | 1 | 1 | 4633648 |
| Basic | explicit warm touch | 17.751 s | 1 | 1 | 4633648 |
| Core | managed cold | 67.009 s | 79 | 1 | 0 |
| Core | managed warm no-change | 17.013 s | 1 | 1 | 0 |
| Core | managed warm touch | 17.037 s | 1 | 1 | 0 |
| Core | explicit cold | 63.289 s | 79 | 1 | 4585120 |
| Core | explicit warm no-change | 17.003 s | 1 | 1 | 4585120 |
| Core | explicit warm touch | 17.196 s | 1 | 1 | 4585120 |

## Comparación contra rerun post-P8 previo

Referencia: `ALPHA12_FINAL_BUILD_SPEED_POST_P8_20261005.md`.

| Target | Métrica | Post-P8 | Post-autocontención | Delta |
|---|---|---:|---:|---:|
| Basic | cold avg | 67.128 s | 64.432 s | -4.02 % |
| Basic | warm avg | 17.928 s | 17.797 s | -0.73 % |
| Core | cold avg | 73.636 s | 65.149 s | -11.53 % |
| Core | warm avg | 17.835 s | 17.062 s | -4.33 % |
| Combined | cold avg | 70.382 s | 64.790 s | -7.94 % |
| Combined | warm avg | 17.881 s | 17.429 s | -2.53 % |

## Cambio estructural observado

```text
BASIC_COLD_COMPILER_INVOCATIONS=20 -> 16
CORE_COLD_COMPILER_INVOCATIONS=83 -> 79
COLD_COMPILER_INVOCATIONS_DELTA=-4 en ambos targets
```

La reducción de cuatro invocaciones de compilador en ambos targets confirma un
cambio estructural real del build después de retirar del package activo:

- `Adafruit_GFX_Library`
- `Adafruit_ST7735_and_ST7789_Library`

Esto no retira periféricos del autoload normal. La funcionalidad de display
permanece sobre `JWPLC_TFT`.

Los tiempos individuales siguen sujetos al ruido normal del host. Por ejemplo,
`Basic managed cold` fue más lento en esta corrida, mientras `Basic explicit cold`
y ambos cold de Core mejoraron de forma clara. Por ello la conclusión principal
se apoya en el conteo de translation units y no en una sola celda temporal.

## Tamaño de binarios

```text
BASIC_EXPLICIT_BINARY=4644736 -> 4633648  DELTA=-11088 bytes (-0.24 %)
CORE_EXPLICIT_BINARY=4596096 -> 4585120   DELTA=-10976 bytes (-0.24 %)
```

## Conclusión

```text
FINAL_BUILD_SPEED_BENCHMARK_POST_AUTOCONTAINMENT=PASS
BUILD_STRUCTURE_IMPROVED=YES
BASIC_COLD_TU_DELTA=-4
CORE_COLD_TU_DELTA=-4
COMBINED_COLD_AVG_DELTA=-7.94%
COMBINED_WARM_AVG_DELTA=-2.53%
AUTOLOAD_PERIPHERALS_REMOVED=NO
PRECOMPILED_FREEZE=PASS
FINAL_TRACKED_DIRTY_COUNT=0
```

Este benchmark reemplaza al rerun post-P8 como medición final oficial de Alpha12.
