# A14.2 — Client Modbus TCP FC03/FC06 runtime

Fecha: `2026-09-11`

## Objetivo

Validar físicamente el primer núcleo Client/Master de `JWPLC_ModbusTCPClient` sobre una única sesión TCP persistente, usando FC03 y FC06 con Transaction ID incremental.

## Topología

```text
JWPLC Basic 192.168.0.31
    -> W5500 / JWPLC_Ethernet
    -> JWPLC_ModbusTCPClient
    -> TCP 192.168.0.4:15022
    -> servidor de prueba PowerShell
```

Unit ID: `1`.

## Secuencia validada

```text
TID 1 -> FC03 -> Read Holding Registers [0..1]
TID 2 -> FC06 -> Write Single Register Holding[0] = 12345 (0x3039)
TID 3 -> FC03 -> Read Holding Register [0] y confirmar 12345
```

Las tres transacciones se ejecutaron sobre una sola conexión TCP.

## Evidencia PC

```text
TCP_ACCEPT=PASS
REMOTE=192.168.0.31:56872
CONNECTION_ACCEPT_COUNT=1

REQ1_RX = 00 01 00 00 00 06 01 03 00 00 00 02
REQ1_FC03_TID1=PASS
RESP1_TX = 00 01 00 00 00 07 01 03 04 11 11 22 22

REQ2_RX = 00 02 00 00 00 06 01 06 00 00 30 39
REQ2_FC06_TID2=PASS
RESP2_TX = 00 02 00 00 00 06 01 06 00 00 30 39

REQ3_RX = 00 03 00 00 00 06 01 03 00 00 00 01
REQ3_FC03_TID3=PASS
RESP3_TX = 00 03 00 00 00 05 01 03 02 30 39

HOLDING0_FINAL=12345
PASS_COUNT=3
FAIL_COUNT=0
TCP_CONNECTIONS=1
A14_2_CLIENT_FC03_FC06_PC=PASS
```

## Evidencia Serial JWPLC

```text
INITIAL_HOLDING0=4369
INITIAL_HOLDING1=8738
READBACK_HOLDING0=12345
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
A14_2_CLIENT_FC03_FC06_RUNTIME=PASS
```

`4369 = 0x1111` y `8738 = 0x2222`.

## Conclusión

```text
FC03_CLIENT=PASS
FC06_CLIENT=PASS
TRANSACTION_ID_INCREMENTAL=PASS
PERSISTENT_TCP_SESSION=PASS
FC06_READBACK=PASS
CLIENT_TIMEOUTS=0
CLIENT_TRANSPORT_ERRORS=0
CLIENT_PROTOCOL_ERRORS=0
CLIENT_BUS_LOCK_TIMEOUTS=0
A14_2_CLIENT_FC03_FC06_RUNTIME=PASS
```

El resultado confirma que el JWPLC Basic funciona físicamente como Modbus TCP Client/Master cooperativo para FC03 y FC06, reutilizando una sesión TCP persistente y conservando el motor no bloqueante de connect/TX validado previamente.
