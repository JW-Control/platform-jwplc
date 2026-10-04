# Alpha14 — A14.1 Invalid MBAP / Reconnect

Fecha: `2026-09-11`

## Entorno

```text
SERVER=JWPLC Basic físico
IP=192.168.0.31
PORT=502
UNIT_ID=1
FIRMWARE_EXAMPLE=01.ModbusTCP_Server
```

## Estado previo

```text
RX_FRAMES=15
TX_FRAMES=15
REQUESTS_OK=12
EXCEPTIONS=3
HOLDING_0=12345
COILS_RAW=0xA5
LAST_ERROR=OK
```

## Trama fatal MBAP

Se abrió una conexión TCP A y se envió una petición con `Protocol ID=1`, inválido para Modbus TCP:

```text
TX_INVALID=00 20 00 01 00 06 01 03 00 00 00 01
TCP_A_CONNECTED=PASS
TCP_A_REMOTE_CLOSE=PASS
```

El JWPLC cerró la conexión remota sin responder una excepción Modbus, conforme al contrato de framing fatal del parser MBAP.

## Reconexión

Se abrió una nueva conexión TCP B y se envió un FC03 válido:

```text
TX_VALID=00 21 00 00 00 06 01 03 00 00 00 01
RX_VALID=00 21 00 00 00 05 01 03 02 30 39
TCP_B_RECONNECT=PASS
VALID_FC03_AFTER_RECONNECT=PASS
HOLDING_0=12345
```

## Diagnóstico interno posterior

```text
SERVER_STATE=READY
CLIENT=NONE
LAST_ERROR=OK
RX_FRAMES=16
TX_FRAMES=16
REQUESTS_OK=13
EXCEPTIONS=3
HOLDING_0=12345
COILS_RAW=0xA5
UNEXPECTED_RESET=0
```

La trama con MBAP inválido no incrementó `rxFrames` porque fue descartada antes de completar un ADU válido. El FC03 posterior sí incrementó `RX/TX` y limpió el estado de error a `OK`.

## Resultado

```text
INVALID_MBAP_FORCES_CLIENT_DROP=PASS
SERVER_REMAINS_AVAILABLE=PASS
NEW_TCP_CONNECTION=PASS
VALID_REQUEST_AFTER_RECONNECT=PASS
MAP_STATE_PRESERVED=PASS
A14_1_INVALID_MBAP_RECONNECT=PASS
```

## Conclusión A14.1

Con este gate se completa la evidencia física de Foundation + Server definida para A14.1:

```text
A14_1_COMPILE=PASS
A14_1_EMPTY_SKETCH_REGRESSION=PASS
A14_1_SERVER_LISTEN_SMOKE=PASS
A14_1_FC01_02_03_04_05_06_15_16=PASS
A14_1_EXCEPTION_RECOVERY=PASS
A14_1_INVALID_MBAP_RECONNECT=PASS
A14_1_SERVER_RUNTIME=PASS
A14_1=PASS
```

El siguiente gate pasa a A14.2 — Client cooperativo.
