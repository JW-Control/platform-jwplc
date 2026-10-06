# Alpha14 - F1 final RAW TCP/UDP ceiling — preparación 2026-10-03

## Motivo

El frente TCP/RTU full-runtime quedó cerrado con:

`1000 Modbus TCP req/s + 100 Modbus RTU req/s = PASS_STRICT_CONFIRMED`.

Antes de cerrar las cifras de capacidad Ethernet del package actual queda pendiente reconciliar el ceiling RAW con la configuración productiva vigente.

La evidencia histórica RAW TCP/UDP sigue siendo útil para comparación, pero el cierre final requiere una medición current-package después de las promociones actuales de W5500/SPI/UDP FAST.

## Gate

`tools/modbus-tcp-benchmark/gates/a14_final_raw_udp_ceiling.ps1`

## Casos

Cada ventana dura 300 s:

1. TCP RX legacy/product path.
2. TCP TX.
3. UDP RX legacy.
4. UDP TX.
5. UDP RX FAST con `jwplcReadPacketFastDeferred()/jwplcCommitRxFast()`.

Tiempo puro de medición: 25 min, más compile/upload y snapshots.

## Configuración

- W5500: 26 MHz.
- FIFO_REUSE: ON.
- SPI DLEN cache: ON.
- SPI COPY_OUT_64: ON.
- UDP FAST API presente.
- UDP FAST payload: 1016 B.
- package/libraries actuales de la rama.

## Interpretación

Este gate caracteriza throughput RAW; no sustituye las cifras de aplicación Modbus.

La salida debe mantener separados:

- TCP RX Mbps.
- TCP TX Mbps.
- UDP RX legacy Mbps.
- UDP TX Mbps.
- UDP RX FAST Mbps.
- pérdida UDP offered-vs-consumed como evidencia de saturación.
- errores de transporte/SPI.
- reset inesperado.
- telemetry loop/SPI disponible por caso.

## Hard fail

Detienen el gate:

- compile/upload fail;
- fallo funcional;
- error de transporte;
- error SPI lock;
- reset inesperado;
- ausencia de summaries.

La pérdida UDP bajo flood no es por sí sola un hard fail; el dato de capacidad DUT es el throughput consumido medido en el dispositivo.

## Resultado esperado

`A14_FINAL_RAW_UDP_CEILING=PASS_CHARACTERIZED`

Luego de F1 quedará por resolver la coexistencia final TCP + UDP + RTU si se desea publicar una cifra conjunta.
