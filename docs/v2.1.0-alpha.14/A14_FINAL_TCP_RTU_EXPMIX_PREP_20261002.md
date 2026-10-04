# Alpha14 — R4 full-runtime TCP x RTU EXP-MIX — PREP 2026-10-02

## Objetivo

Construir la matriz final de coexistencia entre Modbus TCP y el workload RTU
de ocho módulos de expansión, con todo el runtime normal activo.

## Workload RTU

Patrón conservador seleccionado por R3:

`2DI + 2DO + 2AI + 2AO`

Cada scan tiene ocho transacciones:

- 2 x FC02 / 8 DI;
- 2 x FC0F / 8 DO;
- 2 x FC04 / 4 AI;
- 2 x FC10 / 2 AO.

RTU corre unpaced con:

- 500000 baud;
- Master FIFO 9;
- Slave FIFO 8;
- BULK/BULK;
- QUEUED/QUEUED;
- Master GAP;
- Slave STRUCTURAL;
- frame gap 100 us;
- CRC BITWISE.

## Runtime activo

- Modbus TCP Server FC03/125;
- Display/TFT;
- SD/DataLog;
- FRAM;
- RTC;
- TCA/I-O;
- botonera;
- SPI probe.

## Matriz

Cada punto dura 300 s:

| TCP target | RTU |
|---:|---|
| OFF | EXP-MIX unpaced |
| 100 req/s | EXP-MIX unpaced |
| 250 req/s | EXP-MIX unpaced |
| 500 req/s | EXP-MIX unpaced |
| 750 req/s | EXP-MIX unpaced |
| 1000 req/s | EXP-MIX unpaced |

Se reportan TCP achieved/AVG/P95/P99/MAX, RTU req/s, scans/s, ms/scan,
transaction max, loop avg/max, SPI probe y salud de periféricos.

Una incapacidad de alcanzar el target TCP con cero errores se clasifica
`SATURATION_FAIL_CLEAN` y no detiene el barrido. Sí detienen el gate:

- timeout/transport/protocol TCP;
- fallo RTU;
- CRC;
- exception;
- verify/rejected;
- mismatch Master/Slave;
- mismatch de outputs;
- SPI probe failure;
- fallo de periférico;
- reboot;
- pérdida de Ethernet/server.

## Source policy

R4 usa C0 POLLING con INT/D2/D3/E1 en OFF y exige source-first para
`JWPLC_ModbusTCP` y `JWPLC_ModbusRTU`.

Los valores resultantes serán la base para la curva y tabla comercial
redondeada. No se publicarán directamente los ceilings experimentales como
recomendaciones sin margen.