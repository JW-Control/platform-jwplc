# Alpha14 — G3B-D2 H3E-R adaptativo — PREP 2026-10-01

## Objetivo

Validar el scheduler INT adaptativo dentro del runtime completo antes de
promoverlo a default.

## Build candidato

El package mantiene defaults OFF/0. El Master del gate se compila con:

```text
-DJWPLC_MODBUS_TCP_INT_GUIDED_RX=1
-DJWPLC_MODBUS_TCP_INT_HOT_POLL_US=1500
```

El wrapper H3E-R acepta únicamente:

- sin override;
- INT puro;
- INT adaptativo exacto de 1500 us.

Además verifica en el compile log que ambos flags estén presentes cuando se usa
el candidato adaptativo.

## Workload

```text
TCP=FC03/125 @1000 req/s
WINDOW=120 s
RTU=50 Hz
DataLog=buffered/autoservice
Display/TFT=activo
FRAM=activo
RTC=activo
TCA/I-O=activo
Botonera=activa
SPI probe=activo
```

## Criterios

- 120000/120000 TCP;
- 1000 req/s dentro de los guards existentes;
- cero timeout/transporte/protocolo/bus-lock;
- RTU 49.5–50.5 Hz, cero skips/fails/CRC/timeouts;
- DataLog sin failed commits;
- periféricos sin failures;
- TFT Master/Slave PASS;
- package default INT=0;
- package default hot-poll=0;
- compile log confirma INT=1 y HOT_POLL_US=1500;
- P95/P99 dentro de los guards H3E-R.

Si pasa:

```text
G3B_D2_H3ER=PASS
NEXT=G3C_PROMOTE_ADAPTIVE_INT_DEFAULT
```
