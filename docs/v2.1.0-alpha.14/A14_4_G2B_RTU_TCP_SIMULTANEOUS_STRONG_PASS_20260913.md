# Alpha14 — A14.4 G2b — RTU + TCP simultáneo — STRONG PASS

Fecha: 2026-09-13

## Clasificación

- `PASS_PHYSICAL`
- `A14_4_G2B=STRONG_PASS`
- `RTU_TCP_SIMULTANEOUS=VALIDATED`

## Objetivo

Repetir el gate físico simultáneo de A14.4 corrigiendo únicamente el diagnóstico de edad de I/O identificado en G2R. El objetivo era confirmar que el `IO_STALE=1` observado en G2 era un falso positivo de instrumentación y no una pérdida real de actualización del TCA/I/O.

## Topología

### DUT — COM14

- JWPLC Basic bajo prueba.
- Modbus TCP Server en `192.168.0.31:502`.
- Modbus RTU Slave ID 2.
- RTU `115200 8N1`.
- servicio RTU cooperativo cada 1000 us.
- HMI declarativa Dirty / On-Demand.
- workload SD completo.
- FRAM, RTC, botonera, TCA/I/O y probe SPI activos.

### Master — COM4

- JWPLC Basic como Modbus RTU Master cooperativo.
- FC03.
- 16 holding registers.
- periodo objetivo de request: 20 ms.
- timeout: 250 ms.

### TCP

- FC03 / 125 registros.
- 500 req/s.
- 60 s.

## Corrección diagnóstica aplicada

El benchmark original calculaba la edad de I/O usando un `now` capturado antes de leer `io->last_scan_ms`. Como `jwplcSystemTask` actualiza `last_scan_ms` desde una tarea FreeRTOS separada, podía ocurrir que `last_scan_ms` avanzara 1 ms entre ambas lecturas. La resta unsigned producía entonces `0xFFFFFFFF`, interpretándose falsamente como una edad enorme.

G2b captura primero `last_scan_ms` y toma `millis()` después. No se modificó el runtime productivo para esta validación; la corrección fue aplicada al firmware temporal de benchmark.

## Resultado TCP

- `TCP_ACHIEVED_REQ_S=499.976`
- `TCP_ACHIEVED_PCT=99.9952`
- `TCP_CLEAN=YES`
- resultado formal: `STABLE_PASS`
- requests: `29999/30000`
- timeouts: 0
- transport errors: 0
- protocol errors: 0
- bus lock timeouts: 0
- cross count: PASS

## Resultado RTU físico

- `RTU_REQUESTS_STARTED=3002`
- `RTU_REQUESTS_SUCCESS=3002`
- `RTU_REQUESTS_FAILED=0`
- `RTU_EFFECTIVE_HZ=49.622`
- `RTU_MASTER_CLEAN=YES`

DUT:

- `DUT_RTU_RX=2998`
- `DUT_RTU_TX=2998`
- `DUT_RTU_OK=2998`
- `DUT_RTU_CLEAN=YES`

La pequeña diferencia Master/DUT corresponde a las requests realizadas antes del reset estadístico interno del benchmark TCP y no representa pérdida de comunicación.

## Full runtime

- `IO_STALE=0`
- `IO_MAX_AGE_MS=24`
- `IO_CURRENT_AGE_MS=16`
- `PERIPHERAL_FAILURE_COUNT=0`
- `FULL_RUNTIME_CLEAN=YES`

También permanecieron activos y limpios Display/HMI, SD, FRAM, RTC, botonera, SPI y TCA/I/O.

## Conclusión

La corrección diagnóstica confirma la hipótesis de G2R: el `IO_STALE=1` de G2 fue un falso positivo causado por una carrera de instrumentación, no por una congelación real de E/S.

A14.4 queda validada funcionalmente en simultáneo con:

- Modbus TCP a 500 req/s;
- Modbus RTU físico a aproximadamente 50 Hz / periodo objetivo de 20 ms;
- full runtime completo;
- cero errores funcionales o periféricos observados.

El siguiente gate es un soak simultáneo prolongado para validar estabilidad sostenida antes de cerrar A14.4.
