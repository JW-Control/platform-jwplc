# Alpha12 — Rerun final de benchmark post-P8

Fecha: 2026-10-05

## Identidad

```text
BRANCH=v2.1.0-alpha.12/feature/modbus-tcp
HEAD=729a870d696b528cb85cd38449dac5edc3a97b62
RESULT_ROOT=tools/alpha12/results/final_build_speed_benchmark/20261005_001201
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

## Matriz post-P8

| Target | Fase | Tiempo | Compiler invocations | Link invocations | Binary bytes |
|---|---|---:|---:|---:|---:|
| Basic | managed cold | 67.093 s | 20 | 1 | 0 |
| Basic | managed warm no-change | 17.956 s | 1 | 1 | 0 |
| Basic | managed warm touch | 17.852 s | 1 | 1 | 0 |
| Basic | explicit cold | 67.164 s | 20 | 1 | 4644736 |
| Basic | explicit warm no-change | 17.934 s | 1 | 1 | 4644736 |
| Basic | explicit warm touch | 17.968 s | 1 | 1 | 4644736 |
| Core | managed cold | 75.404 s | 83 | 1 | 0 |
| Core | managed warm no-change | 17.130 s | 1 | 1 | 0 |
| Core | managed warm touch | 17.092 s | 1 | 1 | 0 |
| Core | explicit cold | 71.868 s | 83 | 1 | 4596096 |
| Core | explicit warm no-change | 18.581 s | 1 | 1 | 4596096 |
| Core | explicit warm touch | 18.537 s | 1 | 1 | 4596096 |

## Comparación contra benchmark Alpha12 previo

Referencia previa: `docs/v2.1.0-alpha.12/ALPHA12_BUILD_SPEED_COMPARISON_20261004.md`.

### Promedios

| Target | Métrica | Run previo | Post-P8 | Delta |
|---|---|---:|---:|---:|
| Basic | cold avg | 66.985 s | 67.128 s | +0.21 % |
| Basic | warm avg | 17.887 s | 17.928 s | +0.23 % |
| Core | cold avg | 73.457 s | 73.636 s | +0.24 % |
| Core | warm avg | 17.229 s | 17.835 s | +3.52 % |
| Combined | cold avg | 70.221 s | 70.382 s | +0.23 % |
| Combined | warm avg | 17.558 s | 17.881 s | +1.84 % |

Los cold quedan esencialmente invariantes. En Core warm, las dos fases
`explicit_warm_*` de este run fueron aproximadamente 7.5 % más lentas que en
el run anterior, mientras las fases managed warm mejoraron ligeramente. Un único
rerun no permite atribuir esa variación a un cambio estructural del package.

## Hallazgo sobre la consolidación Modbus TCP Client

La consolidación de varios translation units del Client TCP en
`JWPLC_ModbusTCP_Client.cpp` no cambia la estructura de este benchmark:

```text
PREVIOUS_BASIC_COMPILER_INVOCATIONS_COLD=20
POST_P8_BASIC_COMPILER_INVOCATIONS_COLD=20

PREVIOUS_CORE_COMPILER_INVOCATIONS_COLD=83
POST_P8_CORE_COMPILER_INVOCATIONS_COLD=83
```

Motivo: el sketch oficial `01_empty` mide el autoload normal del JWPLC, y
`JWPLC_ModbusTCP` no forma parte de ese autoload. Por tanto, esta matriz no
puede demostrar una mejora de tiempo causada por reducir los translation units
internos de Modbus TCP Client.

Esto no invalida el benchmark: su objetivo es medir el package/autoload normal
con metodología histórica comparable.

## Conclusión

```text
FINAL_BUILD_SPEED_BENCHMARK_POST_P8=PASS
COLD_STRUCTURAL_REGRESSION=NO
OFFICIAL_AUTOLOAD_TU_COUNT_CHANGED=NO
MODBUS_TCP_CLIENT_CONSOLIDATION_EFFECT_IN_01_EMPTY=NOT_MEASURABLE
PRECOMPILED_FREEZE=PASS
AUTOLOAD_PERIPHERALS_REMOVED=NO
```

Siguiente gate:

```text
FINAL_CLI_IDE_UPLOAD_GATES
```
