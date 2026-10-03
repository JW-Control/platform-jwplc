# Alpha14 - campaña final TCP250 + RTU800 + UDP FAST — preparación 2026-10-03

## Objetivo

Cerrar la caracterización de coexistencia simultánea del JWPLC Basic con una carga industrial equilibrada:

- Modbus TCP: 250 req/s.
- Modbus RTU: 800 req/s.
- RTU EXP-MIX: 8 transacciones por scan.
- objetivo de scan RTU: 100 scans/s.
- interpretación: 8 módulos atendidos una vez cada 10 ms.
- UDP FAST RX: variable a escalar.
- full runtime activo: Display + SD/DataLog + FRAM + RTC + TCA/I/O + botonera.

Este gate es diagnóstico y aditivo. No cambia las APIs productivas.

## Justificación del punto base

La campaña previa mostró:

- RTU-only EXP-MIX: ~1346.7 req/s.
- TCP500 + RTU unpaced: ~900 req/s.
- TCP1000 + RTU100: PASS estricto confirmado.

Para la carga final se fija TCP en 250 req/s y RTU en 800 req/s. Esto representa una carga TCP alta pero más cercana a un uso industrial real y conserva el requisito de 8 expansiones a 100 Hz.

## UDP ladder

Cada escalón dura 300 s:

`0, 1, 2, 4, 6, 8, 10, 12 Mbps`

Características:

- UDP FAST aditivo.
- payload: 1016 bytes.
- batch: 2.
- puerto: 5002.
- número de secuencia de 32 bits al inicio de cada datagrama.
- el punto 0 Mbps mantiene el socket UDP creado pero desactiva el polling FAST para no introducir carga artificial de empty-poll.

## Criterio estricto por escalón

TCP:

- target 250 req/s.
- >=99.9 % del target.
- cero timeout/transport/protocol errors.

RTU:

- target 800 req/s.
- >=99 % del target.
- >=99 scans/s.
- 0 skipped periods.
- 0 failed/rejected/verify/CRC/timeouts.
- mapa de salidas Master/Slave exacto.
- 2 DI + 2 DO + 2 AI + 2 AO por scan.

UDP:

- el host debe ofrecer >=99 % del target solicitado.
- el DUT debe entregar >=99 % del target.
- delivery >=99 % de paquetes enviados.
- 0 wrong-size packets.
- 0 sequence decode errors.
- 0 duplicates.
- 0 reorders.
- 0 transport errors.
- 0 SPI lock errors.

Runtime:

- periféricos ready.
- SD commits sin fallos.
- SPI probe sin fallos.
- sin resets inesperados.

La pérdida por saturación se registra mediante delivery, range missing y throughput DUT; no se confunde con corrupción.

## Selección y confirmación

Se selecciona automáticamente el mayor escalón UDP positivo que cumpla todos los criterios estrictos.

Ese punto se repite durante 600 s.

Resultado de cierre esperado:

`A14_FINAL_TRIPLE_COEXISTENCE=PASS_TRIPLE_COEXISTENCE_CONFIRMED`

## Firmware diagnóstico

Master:

`tools/modbus-tcp-benchmark/firmware/a14_final_full_runtime_tcp250_rtu800_udp_master/`

Es un clon diagnóstico del full-runtime EXP-MIX con:

- comando adicional RTU800;
- socket UDP FAST;
- habilitación/deshabilitación explícita del servicio UDP;
- telemetría de secuencia UDP;
- sin cambios en la API productiva.

Slave:

`tools/modbus-tcp-benchmark/firmware/a14_final_full_runtime_expmix_slave/`

## Runner y gate

Runner:

`tools/modbus-tcp-benchmark/pc/a14_final_tcp250_rtu800_udp_ladder.py`

Gate:

`tools/modbus-tcp-benchmark/gates/a14_final_tcp250_rtu800_udp_ladder.ps1`

## Tiempo de prueba

Ladder:

- 8 x 300 s = 40 min.

Confirmación:

- 600 s = 10 min.

Ventanas de medición: ~50 min.

Añadir compile/upload, boot, snapshots y persistencia de evidencia.

## Evidencia esperada

Raíz:

`tools/modbus-tcp-benchmark/results/a14_tcp250_rtu800_udp_ladder_YYYYMMDD_HHMMSS/`

Archivos principales:

- MANIFEST.txt
- compile_master.log
- compile_slave.log
- upload_master.log
- upload_slave.log
- runner.log
- UDP_LADDER.csv
- SELECTION.json
- FINAL_SUMMARY.json
- FINAL_STATUS.txt
- GATE_STATUS.txt
- snapshots/result.json por caso
- snapshots de confirmación.

## Regla de publicación

La cifra final debe expresarse como coexistencia bajo configuración explícita, por ejemplo:

`TCP 250 req/s + RTU 800 req/s (100 scans/s, 8 módulos) + UDP FAST X Mbps + full runtime`

No sustituye los ceilings RAW individuales ya caracterizados.
