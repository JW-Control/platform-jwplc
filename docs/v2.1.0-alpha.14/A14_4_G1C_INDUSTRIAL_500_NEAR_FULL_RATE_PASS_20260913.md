# Alpha14 — A14.4 G1c: referencia industrial Modbus TCP a 500 req/s

Fecha: 2026-09-13

## Clasificación

- `PASS_PHYSICAL`
- `INDUSTRIAL_500_NEAR_FULL_RATE_PASS`

## Objetivo

Validar una referencia de carga industrial elevada para Modbus TCP mientras el runtime completo permanece activo y el stack Modbus RTU Slave está inicializado y atendido cooperativamente cada 1 ms, sin tráfico RTU externo durante este gate.

La referencia elegida fue:

- FC03
- 125 registros por solicitud
- 500 requests/s
- 2 corridas de 60 s
- equivalente conceptual: 10 transacciones por scan de 20 ms / 50 Hz

## Configuración

- Modbus TCP Server: activo
- Modbus RTU Slave: activo
- RTU ID: 2
- RTU: 115200 8N1
- servicio RTU: periodo objetivo 1000 us
- tráfico RTU externo: no
- HMI: Dirty / On-Demand
- SD workload completo: activo
- FRAM, RTC, TCA/I/O, botonera y probe SPI: activos

No se recompiló ni volvió a subir firmware para G1c; se reutilizó el firmware físico validado en G1b.

## Resultados

### Run 1

- achieved: 499.971 req/s
- achieved percent: 99.9942 %
- OK/SENT: 29999 / 29999
- P95: 1279.0 us
- P99: 2661.7 us
- max: 22175.6 us
- timeouts: 0
- transport errors: 0
- protocol errors: 0
- bus lock timeouts: 0
- RTU ready: sí
- RTU CRC errors: 0
- peripheral failure count: 0

### Run 2

- achieved: 499.841 req/s
- achieved percent: 99.9682 %
- OK/SENT: 29997 / 29997
- P95: 1289.7 us
- P99: 2736.3 us
- max: 25271.5 us
- timeouts: 0
- transport errors: 0
- protocol errors: 0
- bus lock timeouts: 0
- RTU ready: sí
- RTU CRC errors: 0
- peripheral failure count: 0

### Resumen

- promedio: 99.9812 %
- mínimo: 99.9682 %
- máximo: 99.9942 %
- spread: 0.0260 pp
- todas las condiciones diagnósticas: limpias

## Interpretación

500 requests/s queda validado como una referencia industrial muy exigente y prácticamente full-rate para el JWPLC Basic bajo este runtime completo.

A 50 scans/s, 500 requests/s equivalen a 10 transacciones Modbus por scan de 20 ms. Dado que Modbus transporta bloques completos de bits o registros por solicitud, esta capacidad deja un margen muy amplio para E/S digitales y analógicas agrupadas correctamente.

No se interpreta la diferencia de unas pocas solicitudes respecto al número nominal como pérdida de datos TCP. Todas las solicitudes realmente enviadas fueron respondidas correctamente, sin timeouts ni errores de transporte/protocolo.

## Observación pendiente RTU

El servicio RTU cooperativo recuperó el rendimiento TCP frente a atender RTU en cada loop, pero se observaron gaps máximos de servicio RTU cercanos a 139 ms bajo el full runtime.

Ese dato no invalida G1c porque no hubo tráfico RTU externo, pero impide declarar cerrada la coexistencia RTU+TCP.

## Próximo gate

A14.4 debe continuar con tráfico RTU físico real simultáneo a la carga Modbus TCP, verificando:

- solicitudes RTU correctas y sostenidas;
- cero CRC/errors/timeouts no esperados;
- coherencia del mapa compartido;
- mantenimiento del rendimiento TCP de referencia;
- impacto de los gaps máximos del servicio RTU;
- ausencia de fallos en periféricos.
