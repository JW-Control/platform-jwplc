# Alpha14 A14.3 — Reproducción del baseline con workload SD deshabilitado

Fecha: 2026-09-13

## Resultado

```text
A14_3_SD_WORKLOAD_OFF_REPRODUCTION=PASS_DIAGNOSTIC
BASELINE_REPRODUCED_GTE95=NO
BASELINE_NOT_REPRODUCED=YES
CURRENT_AVG_PCT=89.843
NEXT=REVALIDATE_TCP_ONLY_BASELINE
```

## Objetivo

Repetir sobre la base y entorno actuales el A/B que previamente había alcanzado `95.993 %` con la microSD inicializada y lista, el archivo persistente abierto, pero con `serviceSdAppend()` y `serviceSdVerify()` completamente fuera del scheduler.

El objetivo fue determinar si el valor histórico `95.993 %` seguía siendo un baseline reproducible antes de continuar con el aislamiento de la ruta SD o avanzar a pruebas de batching.

## Preparación del firmware

Se aplicó una modificación temporal únicamente al harness `FULL_RUNTIME_REALISTIC`:

- SD permaneció en el autoload normal;
- SD permaneció inicializada y lista;
- se conservó la lógica de apertura del archivo persistente;
- se retiró temporalmente del scheduler la llamada a `serviceSdAppend()`;
- se retiró temporalmente del scheduler la llamada a `serviceSdVerify()`;
- TFT, FRAM, RTC, I/O, botonera, SPI probe, Ethernet y Modbus TCP permanecieron activos;
- no se modificó `JW_SD`;
- no se modificó `JWPLC_Ethernet`;
- no se realizó commit de código fuente diagnóstico;
- después del upload, el harness tracked fue restaurado y el worktree quedó limpio.

La validación previa al compile confirmó:

```text
APPEND_BLOCK_MATCHES_BEFORE=1
VERIFY_BLOCK_MATCHES_BEFORE=1
APPEND_CALLS_AFTER=0
VERIFY_CALLS_AFTER=0
TEMP_PATCH=PASS
```

El firmware compiló y cargó correctamente:

```text
BUILD_UPLOAD_EXIT=0
TEMP_FIRMWARE_UPLOAD=PASS
Sketch=426969 bytes
Global variables=29812 bytes
Hash verification=PASS
```

## Evidencia física

Se ejecutaron dos corridas consecutivas de FC03/125 a `1000 req/s` durante `60 s`.

### Corrida 1

```text
ACHIEVED_REQ_S=904.04
ACHIEVED_PCT=90.404
P95_US=1279.9
P99_US=16986.7
MAX_US=18689.3
TCP_CLEAN=YES
RUNTIME_READY=YES
SD_APPEND_CYCLES=0
SD_VERIFY_CYCLES=0
PERIPHERAL_FAILURE_COUNT=0
SD_WORKLOAD_OFF_CONFIRMED=YES
DIAGNOSTIC_CLEAN=YES
```

### Corrida 2

```text
ACHIEVED_REQ_S=892.82
ACHIEVED_PCT=89.282
P95_US=1294.6
P99_US=17225.0
MAX_US=18624.2
TCP_CLEAN=YES
RUNTIME_READY=YES
SD_APPEND_CYCLES=0
SD_VERIFY_CYCLES=0
PERIPHERAL_FAILURE_COUNT=0
SD_WORKLOAD_OFF_CONFIRMED=YES
DIAGNOSTIC_CLEAN=YES
```

### Resumen

```text
RUN1_PCT=90.404
RUN2_PCT=89.282
AVG_PCT=89.843
MIN_PCT=89.282
MAX_PCT=90.404
SPREAD_PP=1.123
ALL_DIAGNOSTIC_CONDITIONS_CLEAN=YES
```

## Interpretación

El experimento es válido y limpio: el workload periódico SD estuvo realmente fuera del scheduler, el runtime integrado permaneció listo, no hubo fallos periféricos y la comunicación TCP permaneció limpia.

Sin embargo, el rendimiento no reprodujo el valor histórico `95.993 %`. Las dos corridas actuales quedaron en una banda estrecha alrededor de `90 %`, con spread de sólo `1.123 pp`.

Por tanto, el valor histórico de `95.993 %` deja de considerarse un baseline estable suficiente para justificar por sí solo que la pérdida principal esté aislada a SD.

La secuencia acumulada queda:

| Perfil | Resultado |
|---|---:|
| SD workload OFF — histórico | 95.993 % |
| SD workload OFF — reproducción actual run 1 | 90.404 % |
| SD workload OFF — reproducción actual run 2 | 89.282 % |
| SD workload OFF — reproducción actual promedio | 89.843 % |
| Write dry-run — promedio previo | 85.681 % |
| Write dry-run — D0b adicional | 86.000 % |

La diferencia entre `workload OFF` y `dry-run` continúa existiendo en los datos actuales, pero ahora es aproximadamente 4 puntos porcentuales en vez de ~10. Además, antes de atribuir esa diferencia al scheduler o a lógica SD debe volver a validarse el control TCP-only bajo las condiciones actuales.

## Conclusión

```text
A14_3_SD_WORKLOAD_OFF_REPRODUCTION=PASS_DIAGNOSTIC
BASELINE_REPRODUCED_GTE95=NO
BASELINE_NOT_REPRODUCED=YES
TCP_CLEAN_ALL_RUNS=YES
PERIPHERAL_FAILURE_COUNT=0
SD_WORKLOAD_OFF_CONFIRMED=YES
CURRENT_FULL_RUNTIME_NO_SD_WORKLOAD_BASELINE≈89.8_PERCENT
```

No se debe avanzar todavía al sweep de batches `160/512/1024/4096 B` ni al soak de 30 minutos.

## Siguiente gate

Revalidar el firmware `a14_perf_server` TCP-only actual con FC03/125 a `1000 req/s × 60 s`, idealmente en dos corridas consecutivas, usando el mismo PC, red, target y runner formal actuales.

Interpretación prevista:

- si TCP-only vuelve a `>=95 %` en ambas corridas, el límite actual está dentro del full runtime no-SD y deberá aislarse por periférico/scheduler;
- si TCP-only también queda alrededor de `89–91 %`, el cambio está fuera del workload full runtime y se deberá recalibrar primero la referencia de red/host/runner/runtime Ethernet;
- si hay gran dispersión entre corridas, se investigará variabilidad del entorno antes de cualquier optimización funcional.

```text
FINAL_FULL_RUNTIME_1000RPS_60S=ON_HOLD
FINAL_FULL_RUNTIME_1000RPS_30MIN=ON_HOLD
NEXT_GATE=TCP_ONLY_BASELINE_REVALIDATION
```
