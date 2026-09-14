# Alpha14 — A14.4 G2 RTU + TCP simultáneo — REVIEW

Fecha: 2026-09-13

## Clasificación

`REVIEW`

El gate completo no se cierra todavía porque el runner reportó `FULL_RUNTIME_CLEAN=NO`. Sin embargo, la coexistencia física de ambos protocolos fue muy fuerte y quedó demostrada.

## Topología

- DUT COM14:
  - Modbus TCP Server
  - Modbus RTU Slave ID 2
  - 115200 8N1
  - servicio RTU periódico cada 1000 us
  - full runtime activo
  - HMI Dirty / On-Demand
  - SD full workload
- Master COM4:
  - Modbus RTU Master
  - FC03
  - 16 holding registers
  - ciclo objetivo 20 ms
  - timeout 250 ms
- Carga TCP simultánea:
  - FC03
  - 125 registros
  - 500 req/s
  - 60 s

## Resultado TCP

- 30000/30000 solicitudes respondidas.
- 499.999 req/s.
- 99.9998 % del objetivo.
- P95: 3721.6 us.
- P99: 4457.3 us.
- Máximo: 25371.4 us.
- Timeouts: 0.
- Errores de transporte: 0.
- Errores de protocolo: 0.
- Bus lock timeouts: 0.
- `TCP_REFERENCE_500_PASS=YES`.

## Resultado RTU físico simultáneo

- Duración medida: 60488 ms.
- Requests iniciadas: 3000.
- Requests completadas: 3000.
- Requests exitosas: 3000.
- Requests fallidas: 0.
- Requests rechazadas: 0.
- Verify fails: 0.
- Success ratio: 100.0000 %.
- Frecuencia efectiva: 49.597 Hz.
- Periodos omitidos: 12.
- Start late máximo: 138790 us.
- Start gap máximo: 158790 us.
- Latencia media: 11290 us.
- Latencia máxima: 176731 us.
- TX Master: 3000.
- RX Master: 3000.
- OK Master: 3000.
- CRC errors Master: 0.
- Timeouts Master: 0.
- `RTU_MASTER_CLEAN=YES`.
- `RTU_20MS_STRONG=YES`.
- `RTU_20MS_USABLE=YES`.

## Resultado RTU en DUT

- RX: 2997.
- TX: 2997.
- OK: 2997.
- CRC errors: 0.
- Exceptions: 0.
- RTU service gap max: 149481 us.
- `DUT_RTU_CLEAN=YES`.
- `RTU_CROSS_COUNT_REASONABLE=YES`.

La diferencia de 3 requests respecto al Master es compatible con el desfase del reset de contadores del DUT respecto al inicio del tráfico del Master y fue aceptada por el gate.

## Conclusión parcial

La coexistencia física RTU + TCP queda demostrada funcionalmente:

- TCP sostuvo prácticamente el 100 % de 500 req/s.
- RTU sostuvo aproximadamente 50 Hz efectivos.
- No hubo pérdida de solicitudes RTU iniciadas.
- No hubo CRC errors ni timeouts.
- No hubo errores TCP.

El `REVIEW` no proviene de los protocolos, sino exclusivamente de que `FULL_RUNTIME_CLEAN=NO`. El runner no imprimió cuál condición del full runtime produjo el fallo.

## Pendiente inmediato

Recuperar un snapshot completo del DUT sin repetir el ensayo de 60 s para identificar cuál de estos indicadores quedó distinto de cero o no-ready:

- `FULL_RUNTIME_READY`
- `DISPLAY_READY`
- `FRAM_READY`
- `SD_READY`
- `RTC_PRESENT`
- `IO_INITIALIZED`
- `BUTTONS_READY`
- `PERIPHERAL_FAILURE_COUNT`
- `SD_APPEND_FAILS`
- `SD_VERIFY_FAILS`
- `FRAM_FAILS`
- `RTC_STALE`
- `IO_STALE`
- `BUTTON_NOT_READY`
- `SPI_PROBE_FAILS`

No repetir todavía el tráfico simultáneo. Primero recuperar diagnóstico acumulado mediante snapshot `S` del DUT.
