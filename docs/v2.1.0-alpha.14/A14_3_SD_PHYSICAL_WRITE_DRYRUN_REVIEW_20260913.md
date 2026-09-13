# Alpha14 A14.3 — Dry-run de escritura física SD

Fecha: 2026-09-13

## Resultado

```text
A14_3_SD_PHYSICAL_WRITE_ISOLATION=PASS_DIAGNOSTIC
PHYSICAL_WRITE_PATH_CONFIRMED_DOMINANT=NO
WRITE_CALL_NOT_SUFFICIENT_TO_EXPLAIN_LOSS=YES
NEXT=REPRODUCE_SD_WORKLOAD_OFF_BASELINE
```

## Objetivo

Aislar el coste de la llamada física `JWPLCFile::write()` dentro del workload microSD del perfil `FULL_RUNTIME_REALISTIC`.

La variante diagnóstica cargada mantuvo:

- microSD inicializada y disponible;
- archivo persistente abierto;
- `serviceSdAppend()` activo cada 1 s;
- construcción del registro de 32 bytes activa;
- comprobaciones de estado SD activas;
- escritura física `sdAppendFile.write()` deshabilitada;
- `flush()` periódico deshabilitado;
- `serviceSdVerify()` deshabilitado;
- resto del runtime integrado activo.

No se modificó `JW_SD`, no se retiró SD del autoload y el harness tracked quedó restaurado/limpio después del upload diagnóstico.

## Primer gate D0

Se ejecutaron dos corridas consecutivas de FC03/125 a 1000 req/s durante 60 s.

```text
RUN1_PCT=85.355
RUN2_PCT=86.008
AVG_PCT=85.681
MIN_PCT=85.355
MAX_PCT=86.008
SPREAD_PP=0.653
TCP_CLEAN=YES en ambas
RUNTIME_READY=YES en ambas
SD_APPEND_FAILS=0
SD_VERIFY_FAILS=0
PERIPHERAL_FAILURE_COUNT=0
```

La clasificación automática inicial quedó en `REVIEW` porque el runner intentó calcular actividad SD mediante deltas entre snapshots previos/posteriores.

Ese criterio era incorrecto: el benchmark `run_case()` envía `R` antes de la ventana medida y el firmware reinicia `runtimeStats` mediante `runtimeStats = RuntimeStats{}`. Por ello los deltas observados (`-747` y `0`) no representan inactividad del scheduler.

## Gate D0b — validación con contadores absolutos

Se repitió una corrida de 60 s usando contadores absolutos posteriores al reset estadístico.

Resultado:

```text
ACHIEVED_REQ_S=860.00
ACHIEVED_PCT=86.000
LATENCY_P95_US=1345.4
LATENCY_P99_US=18011.7
LATENCY_MAX_US=19107.7
TCP_CLEAN=YES
RUNTIME_READY=YES

SD_APPEND_CYCLES_ABSOLUTE=60
SD_APPEND_MAX_US=41
SD_APPEND_FAILS=0
SD_FLUSH_CYCLES_ABSOLUTE=0
SD_VERIFY_CYCLES_ABSOLUTE=0
SD_VERIFY_FAILS=0
PERIPHERAL_FAILURE_COUNT=0

SD_APPEND_ACTIVE=YES
PERIODIC_FLUSH_OFF=YES
VERIFY_OFF=YES
FAILURE_FREE=YES
A14_3_D0B_COUNTER_VALIDATION=PASS_PHYSICAL
```

## Interpretación

El dry-run confirma físicamente que `serviceSdAppend()` se ejecutó 60 veces durante la ventana de 60 s, sin escritura física, sin flush periódico, sin verify y sin fallos.

El coste máximo registrado del ciclo lógico de append fue sólo `41 us`, pero el throughput Modbus TCP permaneció alrededor de `86 %` en tres corridas altamente consistentes.

Por tanto, la evidencia actual **no confirma** que `JWPLCFile::write()` sea el componente dominante de la pérdida. La hipótesis anterior derivada del gate sin flush debe revisarse.

La comparación histórica relevante queda:

| Perfil | Achieved |
|---|---:|
| SD workload OFF | 95.993 % |
| Append + flush, verify OFF | 91.427 % |
| Write ON, flush OFF, verify OFF — promedio 2 runs | 89.448 % |
| Write dry-run, flush OFF, verify OFF — promedio D0 | 85.681 % |
| Write dry-run, D0b adicional | 86.000 % |

La caída del dry-run no es compatible con una explicación simple basada sólo en tiempo ocupado por la llamada física de escritura. Además, `SD_APPEND_MAX_US=41` hace improbable que los 60 ciclos de append lógico por sí solos expliquen una pérdida cercana a 10 puntos porcentuales.

## Conclusión revisada

```text
A14_3_SD_PHYSICAL_WRITE_ISOLATION=PASS_DIAGNOSTIC
PHYSICAL_WRITE_PATH_CONFIRMED_DOMINANT=NO
PHYSICAL_WRITE_PATH_MATERIAL=NOT_PROVEN
WRITE_CALL_NOT_SUFFICIENT_TO_EXPLAIN_LOSS=YES
DRYRUN_RESULT_REPEATABLE=YES
TCP_FAILURE=NO
PERIPHERAL_FAILURE=NO
```

No se debe avanzar aún al sweep de batches `160/512/1024/4096 B`, porque la premisa de que el coste dominante ya estaba aislado a la escritura física no quedó validada.

## Siguiente gate

Reproducir el perfil diagnóstico `SD workload OFF` sobre la base y entorno actuales, manteniendo SD inicializada y el archivo abierto pero dejando fuera del scheduler `serviceSdAppend()` y `serviceSdVerify()`.

Objetivo:

- si vuelve a `>=95 %`, confirmar que existe una diferencia real entre no ejecutar el workload SD y ejecutar `serviceSdAppend()` en dry-run, y aislar después esa diferencia;
- si queda también alrededor de `86–90 %`, tratar el antiguo `95.993 %` como no reproducido bajo las condiciones actuales y recalibrar el baseline antes de continuar.

```text
FINAL_FULL_RUNTIME_1000RPS_60S=ON_HOLD
FINAL_FULL_RUNTIME_1000RPS_30MIN=ON_HOLD
NEXT_GATE=REPRODUCE_SD_WORKLOAD_OFF_BASELINE
```
