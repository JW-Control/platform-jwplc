# Alpha14 — PERF-S2 FULL_RUNTIME_REALISTIC smoke físico

Fecha: `2026-09-13`

## Resultado

```text
A14_3_FULL_RUNTIME_REALISTIC_PHYSICAL_SMOKE=PASS_PHYSICAL
```

Se validó físicamente el perfil integrado `FULL_RUNTIME_REALISTIC` con el JWPLC Basic ejecutando simultáneamente:

```text
Modbus TCP Server
TFT
FRAM
microSD
RTC
botonera
TCA / I-O
Ethernet / W5500
```

No se conmutaron relés mecánicos durante la prueba.

## Build y upload

```text
BUILD_UPLOAD_EXIT=0
COMPILE_WARNINGS=0
FLASH=427421 bytes (10%)
RAM=29796 bytes (9%)
```

Upload físico realizado por `COM14` y verificado por `esptool` sin errores.

## Smoke Modbus TCP

Perfil de tráfico:

```text
FUNCTION=FC03
QUANTITY_REGISTERS=125
REQUESTED_REQ_S=100
DURATION_S=60
```

Resultado:

```text
TCP_RESULT=STABLE_PASS
TCP_ACHIEVED_REQ_S=100.00
TCP_ACHIEVED_PCT=99.998
TCP_TOTAL_MBPS=0.2168
TCP_USEFUL_MBPS=0.2000
TCP_P95_US=13443.6
TCP_P99_US=17143.7
TCP_MAX_US=25636.4
TCP_ERRORS=0
TCP_CROSS_COUNT_PASS=YES
REQUESTS_OK=6000/6000
CONNECT_ATTEMPTS=1
TIMEOUTS=0
TRANSPORT_ERRORS=0
PROTOCOL_ERRORS=0
BUS_LOCK_TIMEOUTS=0
```

El smoke confirma estabilidad funcional, pero muestra una diferencia importante respecto al escenario TCP-only: aun a `100 req/s`, la latencia p95/p99 sube a aproximadamente `13.4 ms / 17.1 ms`, con máximo `25.6 ms`. Esto es consistente con un runtime donde TFT, FRAM y microSD comparten tiempo de servicio y bus SPI con Ethernet. No se interpreta como fallo: el throughput solicitado se sostuvo completo y no hubo errores, pero el jitter debe considerarse al buscar el máximo integrado.

## Periféricos — snapshot final

### TFT

```text
DISPLAY_READY=YES
DISPLAY_FRAMES=592
DISPLAY_GAP_MAX_MS=190
TFT_VISUAL_CONFIRMATION=S
```

La observación física confirmó la pantalla `A14 PERF-S2` y contenido dinámico actualizándose durante la prueba.

### FRAM

```text
FRAM_READY=YES
FRAM_CYCLES=237
FRAM_FAILS=0
FRAM_MAX_US=828
```

El workload de FRAM usa escritura + lectura/verificación + restauración de la zona reservada, por lo que la prueba es no destructiva respecto al contenido previo.

### microSD

```text
SD_READY=YES
SD_APPEND_CYCLES=59
SD_APPEND_FAILS=0
SD_APPEND_MAX_US=27525
SD_VERIFY_CYCLES=12
SD_VERIFY_FAILS=0
SD_VERIFY_MAX_US=25970
```

El archivo dedicado del benchmark es:

```text
/A14S2.LOG
```

Las operaciones SD muestran ventanas de aproximadamente `26–28 ms`, relevantes para interpretar el jitter TCP bajo carga integrada.

### RTC

```text
RTC_PRESENT=YES
RTC_SAMPLES=237
RTC_UNAVAILABLE=0
RTC_STALE=0
RTC_MAX_AGE_MS=977
RTC_CURRENT_AGE_MS=585
```

### TCA / I-O

```text
IO_INITIALIZED=YES
IO_SAMPLES=2951
IO_STALE=0
IO_MAX_AGE_MS=34
IO_CURRENT_AGE_MS=18
```

No se generó carga artificial mediante conmutación rápida de relés.

### Botonera

```text
BUTTONS_READY=YES
BUTTON_SAMPLES=2949
BUTTON_NOT_READY=0
BUTTON_SAMPLE_GAP_MAX_MS=110
```

La botonera mantuvo su servicio normal sin requerir pulsaciones humanas continuas.

### SPI compartido

```text
SPI_PROBE_SAMPLES=593
SPI_PROBE_FAILS=0
SPI_PROBE_MAX_WAIT_US=8
SPI_PROBE_OVER_1MS=0
SPI_PROBE_OVER_10MS=0
```

No se observaron fallos de adquisición del mutex SPI durante el smoke.

## Salud integrada

```text
FULL_RUNTIME_READY=YES
READINESS_PASS=YES
PERIPHERAL_FAILURE_COUNT=0
PERIPHERAL_FAILURES_PASS=YES
PERIPHERAL_ACTIVITY_PASS=YES
```

## Interpretación

El perfil `FULL_RUNTIME_REALISTIC` queda habilitado para pruebas de rendimiento superiores.

El smoke demuestra que todos los periféricos previstos permanecen activos y sanos mientras Modbus TCP atiende tráfico real. Sin embargo, la latencia observada ya evidencia que el techo integrado será probablemente inferior o más jittery que el techo TCP-only, por lo que `1000 req/s` debe tratarse como siguiente punto de qualification, no como tasa integrada asumida.

Estado actual:

```text
TCP_ONLY_OPERATIONAL_REFERENCE_REQ_S=1000
TCP_ONLY_OPERATIONAL_REFERENCE_TOTAL_MBPS=2.1680
TCP_ONLY_OPERATIONAL_REFERENCE_USEFUL_MBPS=2.0000
FULL_RUNTIME_REALISTIC_100RPS_SMOKE=PASS_PHYSICAL
FULL_RUNTIME_REALISTIC_MAX_STABLE_REQ_S=NOT_MEASURED_YET
```

## Siguiente paso

1. Consolidar el script PC del smoke físico.
2. Ejecutar qualification de `FULL_RUNTIME_REALISTIC` en `FC03/125 @ 1000 req/s` con ventana corta.
3. Si `1000 req/s` no resulta sostenible, rebracketear la frontera integrada antes de cualquier soak largo.
