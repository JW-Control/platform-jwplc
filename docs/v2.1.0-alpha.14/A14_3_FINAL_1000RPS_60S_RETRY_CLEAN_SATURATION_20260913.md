# Alpha14.3 — Retry final FULL_RUNTIME_REALISTIC @ 1000 req/s × 60 s

Fecha: 2026-09-13

## Estado

```text
FINAL_FULL_RUNTIME_1000RPS_60S=REVIEW
CLASSIFICATION=SATURATION_FAIL_CLEAN
FINAL_FULL_RUNTIME_1000RPS_30MIN=ON_HOLD
```

## Contexto

El retry se ejecutó después de cerrar el bug de verificación microSD con handle único y de consolidar ese ajuste en:

```text
bb166325410cdea765c8c6bd25ce641e77354a44
test(alpha14): corregir verify SD con handle único
```

No se recompiló ni reflasheó para este retry. El gate verificó que el firmware cargado coincidía con el harness actual.

## Resultado TCP

```text
REQUESTED_REQ_S=1000
ACHIEVED_REQ_S=886.10
ACHIEVED_PCT=88.610
TOTAL_TCP_PAYLOAD_MBPS=1.9211
USEFUL_DATA_MBPS=1.7722
LATENCY_AVG_US=1119.1
LATENCY_P95_US=1316.8
LATENCY_P99_US=17608.2
LATENCY_MAX_US=21782.1
LOOP_GAP_AVG_US=491
LOOP_GAP_MAX_US=21454
TIMEOUTS=0
TRANSPORT_ERRORS=0
PROTOCOL_ERRORS=0
BUS_LOCK_TIMEOUTS=0
CROSS_COUNT_PASS=YES
TCP_CLEAN=YES
TCP_RATE_PASS=NO
```

La clasificación del runner fue:

```text
A14_3_FULL_RUNTIME_REALISTIC_1000RPS_QUALIFICATION=SATURATION_FAIL_CLEAN
```

## Estado de periféricos

Todos los periféricos permanecieron activos y sin fallos:

```text
READINESS_PASS=YES
PERIPHERAL_FAILURES_PASS=YES
PERIPHERAL_ACTIVITY_PASS=YES
PERIPHERAL_FRESHNESS_PASS=YES
PERIPHERAL_PASS=YES
PERIPHERAL_FAILURE_COUNT=0
```

### microSD

```text
SD_READY=YES
SD_APPEND_FILE_OPEN=YES
SD_APPEND_CYCLES=60
SD_APPEND_FAILS=0
SD_APPEND_MAX_US=20110
SD_FLUSH_EVERY_RECORDS=5
SD_RECORDS_SINCE_FLUSH=1
SD_FLUSH_CYCLES=12
SD_VERIFY_CYCLES=12
SD_VERIFY_FAILS=0
SD_VERIFY_MAX_US=20709
POST_1000RPS_RUNTIME_SD=PASS
```

Esto confirma que el bug anterior de verificación SD quedó corregido. La caída de rendimiento persiste de forma separada.

## Comparación con referencias previas

Referencia integrada previa antes de la política SD final y antes del cierre productivo del refresh L2:

```text
ACHIEVED_REQ_S≈954.78
ACHIEVED_PCT≈95.478
```

Primer intento con política SD final, antes de corregir verify:

```text
ACHIEVED_REQ_S=900.89
ACHIEVED_PCT=90.089
SD_VERIFY_FAILS=11
```

Retry después de corregir verify:

```text
ACHIEVED_REQ_S=886.10
ACHIEVED_PCT=88.610
SD_VERIFY_FAILS=0
```

Conclusión provisional: el fallo de verify SD no explica la regresión de throughput. Existe una regresión real o una interacción adicional que debe aislarse antes de ejecutar el soak de 30 minutos.

## Próximo diagnóstico

Aislar primero el coste del workload SD manteniendo el resto del full runtime intacto. El siguiente gate debe desactivar temporalmente únicamente `serviceSdAppend()` y `serviceSdVerify()` del scheduler de benchmark, sin quitar SD del autoload ni modificar librerías de producto.

Interpretación esperada:

- Si el throughput vuelve cerca de la referencia previa (~95 % o más), el cuello de botella está dentro del workload SD final.
- Si permanece cerca de ~89–90 %, la regresión no proviene principalmente del workload SD y se continúa con aislamiento del refresh L2 productivo.

No avanzar al soak final de 30 minutos hasta cerrar este diagnóstico.
