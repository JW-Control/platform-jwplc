# Alpha14 — H3E-R current package — plan 2026-09-30

## Objetivo

Repetir el H3E.5 histórico sobre el package actual después de:

- promoción de W5500 RX FIFO_REUSE como default;
- validación S2/S2D;
- adopción obligatoria de JWPLCDataLog para pruebas runtime con microSD;
- cierre de política SINGLE_STATUS.

H3E-R no busca el ceiling de Mbps. Busca demostrar determinismo de runtime bajo
alta tasa de transacciones Modbus TCP.

## Baseline histórico a reproducir

```text
TCP_TARGET_REQ_S=1000
DURATION_S=120
RTU_TARGET_HZ=50
RTU_PERIOD_US=20000
RTU_BAUD=115200
RTU_CONFIG=8N1
RTU_TIMEOUT_MS=25
W5500_SPI_HZ=26000000
TCP_FC03_QUANTITY=125
```

Resultado histórico H3E.5:

```text
TCP_OK=120000/120000
TCP_ACHIEVED_REQ_S=1000.00
TCP_TOTAL_MBPS=2.1680
TCP_USEFUL_MBPS=2.0000
TCP_P95_US=1197.2
TCP_P99_US=3423.3
TCP_MAX_US=18823.7

RTU_REQUESTS_STARTED=6001
RTU_REQUESTS_SUCCESS=6001
RTU_PERIODS_SKIPPED=0
RTU_ACHIEVED_HZ=50.004
```

## Package actual

H3E-R debe compilar sin override de `JWPLC_W5500_RX_FIFO_REUSE`.

```text
FIFO_REUSE_SOURCE=PACKAGE_DEFAULT
FIFO_REUSE_DEFAULT=ON
DIRECT_RX_DEFAULT=OFF
W5500_SPI_HZ=26000000
```

## microSD

La ruta runtime obligatoria es:

```text
JWPLCDataLog
BUFFER=4096 B
THRESHOLD=512 B
TIMEOUT=5000 ms
MANUAL_SERVICE=NO
```

El firmware histórico P5 full-runtime ya cumple este contrato. H3E-R no vuelve
a introducir acceso SD directo.

## Criterio H3E-R

Además de todos los checks P5-B:

```text
TCP_ACHIEVED_PCT >= 99.9
RTU_ACHIEVED_HZ in [49.5, 50.5]
RTU_PERIODS_SKIPPED=0
RTU_STARTED=RTU_SUCCESS
TCP_CLEAN=YES
DATALOG_ACTIVE=YES
DATALOG_FAILED_COMMITS=0
PERIPHERAL_FAILURE_COUNT=0
TFT_MASTER_PHYSICAL=PASS
TFT_SLAVE_PHYSICAL=PASS
```

Se conserva el cross-count exacto Master/Slave con snapshot RTU quiesced.

## Secuencia

```text
FIFO_REUSE_DEFAULT_PROMOTION=CLOSED
SINGLE_STATUS_PRODUCT_DECISION=CLOSED
S2D_DATALOG=CLOSED_PASS

NEXT=H3E_R
AFTER_H3E_R=P4_1_DLEN_REUSE
```
