# Alpha14 — Cierre A14.4 RTU + TCP simultáneo

Fecha: 2026-09-13

Rama: `v2.1.0-alpha.14/feature/modbus-tcp`

## Resultado final

**A14.4 = PASS_PHYSICAL**

Clasificación final del soak:

`SIMULTANEOUS_30MIN_STRONG_PASS`

A14.4 queda cerrada después de validar físicamente la coexistencia simultánea de:

- Modbus TCP Server.
- Modbus RTU Slave.
- HMI declarativa Dirty / On-Demand.
- microSD con workload real de append, flush y verify.
- FRAM.
- RTC.
- TCA / I/O.
- Botonera.
- Probe SPI.

La validación se realizó con dos JWPLC Basic físicos:

- COM4: JWPLC RTU Master.
- COM14: DUT con Modbus TCP Server + Modbus RTU Slave ID 2 + full runtime.

## Perfil del gate G3

### TCP

- Función: FC03.
- Cantidad: 125 Holding Registers.
- Carga: 500 req/s.
- Duración: 1800 s.

### RTU

- Función: FC03.
- Cantidad: 16 Holding Registers.
- Objetivo de periodo: 20 ms.
- Objetivo nominal: 50 Hz.
- Baudrate: 115200.
- Formato: 8N1.
- Timeout Master: 250 ms.

### Runtime

- HMI Dirty / On-Demand activa.
- SD full workload activo.
- FRAM activa.
- RTC activo.
- I/O activo.
- Botonera activa.
- Probe SPI activo.

## Resultados TCP

- Requests: 900000 / 900000.
- Achieved: 500.000 req/s.
- Achieved: 100.0000 %.
- P95: 3725.2 us.
- P99: 4517.7 us.
- Máximo: 30365.1 us.
- Timeouts: 0.
- Transport errors: 0.
- Protocol errors: 0.
- Bus lock timeouts: 0.
- Cross count: PASS.
- Resultado formal: `STABLE_PASS`.

## Resultados RTU Master

- Requests iniciadas: 89875.
- Requests completadas: 89875.
- Requests exitosas: 89875.
- Requests fallidas: 0.
- Requests rechazadas: 0.
- Verify fails: 0.
- Success ratio: 100.0000 %.
- Effective rate: 49.907 Hz.
- Periods skipped: 18.
- Start late max: 139191 us.
- Start gap max: 159191 us.
- Latency avg: 11190 us.
- Latency max: 158436 us.
- CRC errors: 0.
- Master timeouts: 0.
- Master clean: YES.
- Rate strong: YES.
- Latency guard < 250 ms: YES.

## Resultados RTU del DUT

- RX: 89873.
- TX: 89873.
- Requests OK: 89873.
- CRC errors: 0.
- Exceptions: 0.
- Service calls: 1135044.
- Service avg: 312 us.
- Service max: 10479 us.
- Service gap max: 149013 us.
- DUT RTU clean: YES.
- Cross count reasonable: YES.
- Service gap guard < 250 ms: YES.

La pequeña diferencia Master/DUT corresponde a la ventana inicial donde `run_case()` resetea contadores del DUT después de que el Master ya comenzó tráfico. No representa pérdida de requests del enlace RTU.

## Full runtime

### HMI

- Display frames: 17628.
- HMI I/O pushes: 89251.
- HMI RTC pushes: 17972.
- HMI BAR pushes: 17972.

### SD

- Append cycles: 1800.
- Append fails: 0.
- Flush cycles: 360.
- Verify cycles: 360.
- Verify fails: 0.

### FRAM

- Cycles: 7194.
- Fails: 0.

### RTC

- Samples: 7194.
- Unavailable: 0.
- Stale: 0.

### I/O

- Samples: 89111.
- Stale: 0.
- Max age: 27 ms.
- Current age final: 3 ms.

### Botonera

- Samples: 88816.
- Not ready: 0.

### SPI

- Probe samples: 17893.
- Probe fails: 0.

### Global

- Peripheral failure count: 0.
- Readiness clean: YES.
- Failure counters clean: YES.
- Full workload active: YES.
- Full runtime clean: YES.

## Incidente de instrumentación previo

Durante G2 apareció un `IO_STALE=1` acompañado de `IO_MAX_AGE_MS=4294967295`.

La revisión G2R demostró que era un falso positivo de instrumentación:

1. El benchmark tomaba `now = millis()`.
2. `jwplcSystemTask`, ejecutándose concurrentemente, podía actualizar `io->last_scan_ms` en el milisegundo siguiente.
3. La resta unsigned `now - io->last_scan_ms` podía equivaler a `-1`.
4. El cast a `uint32_t` producía `4294967295`.

G2b corrigió temporalmente el diagnóstico tomando primero `last_scan_ms` y luego el timestamp de referencia.

Con la corrección:

- `IO_STALE=0`.
- `IO_MAX_AGE_MS=24 ms` en G2b.
- `IO_MAX_AGE_MS=27 ms` en G3.
- `PERIPHERAL_FAILURE_COUNT=0`.

Por tanto, el incidente no correspondía a una congelación real del TCA/I/O.

## Conclusión técnica

La plataforma JWPLC Basic puede sostener simultáneamente, durante 30 minutos:

- Modbus TCP FC03/125 a 500 req/s reales.
- Modbus RTU FC03/16 a aproximadamente 50 Hz.
- HMI declarativa activa.
- SD con append/flush/verify real.
- FRAM, RTC, I/O, botonera y SPI activos.

Sin pérdidas funcionales ni errores de protocolo en ninguno de los dos transportes.

### Referencia industrial

500 req/s TCP equivale a aproximadamente 10 transacciones Modbus por cada scan de 20 ms. Esta carga queda validada como referencia industrial fuerte para Alpha14.

1000 req/s se conserva como stress/frontier benchmark, no como requisito funcional de cierre.

## Decisión A14.4

**CERRADA / PASS_PHYSICAL**

No quedan pendientes bloqueantes dentro de A14.4.

## Siguiente paso

Ejecutar readiness documental y de rama para cierre de Alpha14.

A14.5 — interoperabilidad con robots — permanece `DEFERRED_NON_BLOCKING` y no bloquea el cierre de Alpha14.
