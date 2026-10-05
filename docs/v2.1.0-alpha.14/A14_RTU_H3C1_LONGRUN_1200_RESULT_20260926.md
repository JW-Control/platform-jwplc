# Alpha14 — RTU-H3C.1 — Resultado long-run 1200 s

Fecha: 2026-09-26

## Resultado

```txt
RTU_HZ=774.484
RTU_STARTED=929660
RTU_SUCCESS=929658
RTU_FAILED=2
RTU_TIMEOUTS=2
REQUEST_PATH_GAP=2
RESPONSE_PATH_GAP=0
REQUEST_BYTE_GAP=0
RESPONSE_BYTE_GAP=0
TCP_REQ_S=500.000
MASTER_CRC=0
SLAVE_CRC=0
```

Contabilidad request-side:

```txt
MASTER_TX_BYTES=7437280
SLAVE_RX_BYTES=7437280
SLAVE_DISCARDED_TAILS=4
SLAVE_DISCARDED_BYTES=16
LEN1=1
LEN3=1
LEN5=1
LEN7=1
LAST_LENGTH=7
LAST_AGE_US=120
MAX_AGE_US=15760
```

Los 16 bytes descartados equivalen a dos requests FC03 de 8 bytes, igual que los
dos timeouts. El histograma es compatible con dos fragmentaciones 1+7 y 3+5,
sin afirmar correspondencia evento-a-evento porque el orden no quedó registrado.

También:

```txt
RTU_SERVICE_GAP_MAX_US=19307
LOOP_GAP_MAX_US=19301
```

## Decisión

No aumentar todavía PARTIAL_HOLD_US=1750.

Siguiente gate: H3C.2 cambia solamente el TX del Master a BLOCKING y mantiene el
TX del Slave en QUEUED. El objetivo es determinar si la fragmentación se origina
en la continuidad física de la ruta queued del request-side.
