# Alpha14 — PERF-S2 FULL_RUNTIME_REALISTIC @ 1000 req/s

Fecha: `2026-09-13`

## Objetivo

Validar la referencia operativa TCP-only de `1000 req/s` bajo el perfil integrado `FULL_RUNTIME_REALISTIC`, manteniendo activos simultáneamente:

- Modbus TCP Server FC03 / 125 registros.
- TFT con contenido dinámico.
- FRAM con ciclos de write/read/verify/restore.
- microSD con append y read/verify.
- RTC mediante runtime normal.
- TCA / I/O mediante runtime normal.
- botonera mediante su task normal.
- Ethernet / W5500 y bus SPI compartido.

La comparación directa usa como referencia TCP-only ya validada:

```text
TCP_ONLY_REFERENCE_REQ_S=1000
TCP_ONLY_REFERENCE_TOTAL_MBPS=2.1680
TCP_ONLY_REFERENCE_USEFUL_MBPS=2.0000
```

## Condiciones

```text
FC03_QUANTITY_REGISTERS=125
REQUESTED_REQ_S=1000
DURATION_S=60
STABLE_THRESHOLD_PCT=95.0
```

## Preparación / estado inicial

Antes de iniciar la ventana de medición:

```text
INITIAL_FULL_RUNTIME_READY=PASS
FULL_RUNTIME_READY=YES
DISPLAY_READY=YES
FRAM_READY=YES
SD_READY=YES
RTC_PRESENT=YES
IO_INITIALIZED=YES
BUTTONS_READY=YES
PERIPHERAL_FAILURE_COUNT=0
```

El runtime ya llevaba actividad acumulada sin fallos antes de la prueba.

## Establecimiento TCP

El cliente PC necesitó cuatro intentos para establecer la sesión:

```text
TCP_CONNECT_ATTEMPT_1=RETRY TimeoutError
TCP_CONNECT_ATTEMPT_2=RETRY TimeoutError
TCP_CONNECT_ATTEMPT_3=RETRY TimeoutError
TCP_CONNECT_ATTEMPT_4=PASS
```

Estos intentos ocurrieron antes del reset estadístico y de la ventana de throughput, por lo que no afectan directamente las cifras medidas. Sin embargo, se mantiene la observación ya existente:

```text
A14_3_TCP_REACCEPT_LATENCY=REVIEW
```

## Resultado TCP

```text
REQUESTED_REQ_S=1000
ACHIEVED_REQ_S=954.78
ACHIEVED_PCT=95.478
REQUESTS_OK=57291/60000
REQUESTS_SENT=57291
TCP_TOTAL_PAYLOAD_MBPS=2.0700
MODBUS_USEFUL_DATA_MBPS=1.9096
TOTAL_MBPS_RETENTION_VS_TCP_ONLY_PCT=95.48
USEFUL_MBPS_RETENTION_VS_TCP_ONLY_PCT=95.48
LATENCY_AVG_US=1041.2
LATENCY_P95_US=1223.5
LATENCY_P99_US=17190.0
LATENCY_MAX_US=30959.2
LOOP_GAP_AVG_US=467
LOOP_GAP_MAX_US=30070
TIMEOUTS=0
TRANSPORT_ERRORS=0
PROTOCOL_ERRORS=0
BUS_LOCK_TIMEOUTS=0
CROSS_COUNT_PASS=YES
TCP_CLEAN=YES
TCP_RATE_PASS=YES
```

Clasificación TCP:

```text
RESULT=STABLE_PASS
```

La tasa alcanzada supera el criterio formal de estabilidad del 95 %, aunque con margen pequeño (`95.478 %`). Por ello `1000 req/s` queda aceptado para qualification, pero todavía no se considera resultado estable de larga duración.

## Estado de periféricos durante la ventana

Al finalizar los 60 s:

```text
FULL_RUNTIME_READY=YES
DISPLAY_READY=YES
DISPLAY_FRAMES=581
DISPLAY_GAP_MAX_MS=198

FRAM_READY=YES
FRAM_CYCLES=235
FRAM_FAILS=0
FRAM_MAX_US=853

SD_READY=YES
SD_APPEND_CYCLES=60
SD_APPEND_FAILS=0
SD_APPEND_MAX_US=29276
SD_VERIFY_CYCLES=12
SD_VERIFY_FAILS=0
SD_VERIFY_MAX_US=28035

RTC_PRESENT=YES
RTC_SAMPLES=235
RTC_UNAVAILABLE=0
RTC_STALE=0
RTC_MAX_AGE_MS=1017
RTC_CURRENT_AGE_MS=275

IO_INITIALIZED=YES
IO_SAMPLES=2910
IO_STALE=0
IO_MAX_AGE_MS=35
IO_CURRENT_AGE_MS=17

BUTTONS_READY=YES
BUTTON_SAMPLES=2906
BUTTON_NOT_READY=0
BUTTON_SAMPLE_GAP_MAX_MS=118

SPI_PROBE_SAMPLES=589
SPI_PROBE_FAILS=0
SPI_PROBE_MAX_WAIT_US=8
SPI_PROBE_OVER_1MS=0
SPI_PROBE_OVER_10MS=0

PERIPHERAL_FAILURE_COUNT=0
```

Los criterios integrados quedaron en:

```text
READINESS_PASS=YES
PERIPHERAL_FAILURES_PASS=YES
PERIPHERAL_ACTIVITY_PASS=YES
PERIPHERAL_FRESHNESS_PASS=YES
PERIPHERAL_PASS=YES
```

## Interpretación

El perfil integrado retuvo aproximadamente el `95.48 %` del throughput de la referencia TCP-only en esta ventana de 60 s:

```text
TCP-only @1000 req/s:
  2.1680 Mbps payload TCP
  2.0000 Mbps datos útiles

FULL_RUNTIME_REALISTIC @1000 req/s:
  2.0700 Mbps payload TCP
  1.9096 Mbps datos útiles
```

La caída observada es aproximadamente `4.52 %` en esta ventana corta.

La pérdida de rendimiento no vino acompañada de errores funcionales ni degradación periférica. No hubo timeouts, errores de transporte/protocolo, timeouts del mutex SPI, fallos FRAM/SD, stale de RTC/I/O ni pérdida de readiness.

La microSD sigue siendo la operación periférica individual con mayor duración observada (`~28–29 ms`), mientras que FRAM y ownership SPI permanecen muy por debajo.

La latencia p99 y el máximo de loop gap muestran jitter notable asociado a la carga periférica, aun cuando la tasa formal todavía cumple el umbral de estabilidad.

## Clasificación

```text
A14_3_FULL_RUNTIME_REALISTIC_1000RPS_QUALIFICATION=PASS_PHYSICAL
FULL_RUNTIME_MAX_STABLE_REQ_S=NOT_MEASURED_YET
A14_3_TCP_REACCEPT_LATENCY=REVIEW
```

## Siguiente paso

Ejecutar una prueba de mayor duración en `FULL_RUNTIME_REALISTIC @ 1000 req/s`.

Debido a que el resultado de 60 s quedó sólo `0.478` puntos porcentuales por encima del criterio mínimo del 95 %, la prueba larga es necesaria antes de aceptar `1000 req/s` como referencia integrada sostenible.
