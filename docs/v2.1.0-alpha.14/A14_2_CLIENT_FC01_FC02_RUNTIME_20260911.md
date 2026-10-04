# A14.2 — Runtime físico Client FC01/FC02

Fecha: `2026-09-11`

## Resultado

```text
A14_2_CLIENT_FC01_FC02_RUNTIME=PASS
A14_2_CLIENT_FC01_FC02_PC=PASS
```

## Servidor PC

```text
TCP_ACCEPT=PASS
REMOTE=192.168.0.31:49911
TCP_CONNECTIONS=1
REQ1_FC01_TID1=PASS
RESP1_TX=00 01 00 00 00 05 01 01 02 A5 FD
FC01_DIRTY_UNUSED_BITS_SENT=PASS
REQ2_FC02_TID2=PASS
RESP2_TX=00 02 00 00 00 05 01 02 02 5A FE
FC02_DIRTY_UNUSED_BITS_SENT=PASS
REQ3_FC03_REGRESSION_TID3=PASS
RESP3_TX=00 03 00 00 00 05 01 03 02 13 57
PASS_COUNT=3
FAIL_COUNT=0
A14_2_CLIENT_FC01_FC02_PC=PASS
```

## JWPLC

```text
FC01_BYTES=A5,01
FC02_BYTES=5A,02
FC03_CONTROL=4951
TID_SEQUENCE=1,2,3
SESSION_CONNECTED=YES
CONNECTIONS=1
TX_FRAMES=3
RX_FRAMES=3
REQUESTS_OK=3
EXCEPTIONS=0
TIMEOUTS=0
TRANSPORT_ERRORS=0
PROTOCOL_ERRORS=0
BUS_LOCK_TIMEOUTS=0
A14_2_CLIENT_FC01_FC02_RUNTIME=PASS
```

## Verificación específica de bits sobrantes

Se solicitaron 10 bits por FC01 y FC02. El servidor envió deliberadamente bits no solicitados en `1` en el último byte (`FD` y `FE`). El Client entregó `01` y `02`, confirmando que limpia los bits sobrantes y mantiene la semántica LSB-first usada por `JWPLC_ModbusRTU`.

## Regresión

FC03 respondió correctamente en la misma sesión TCP después de FC01/FC02. No hubo reconexiones, errores de protocolo, transporte, timeout ni lock SPI.
