# Alpha14.1 — Matriz física Modbus TCP Server

Fecha: `2026-09-11`

## Entorno

```text
BRANCH=v2.1.0-alpha.14/feature/modbus-tcp
DEVICE=JWPLC Basic v2.0 físico
SERVER_IP=192.168.0.31
TCP_PORT=502
UNIT_ID=1
CLIENT=PowerShell / System.Net.Sockets.TcpClient
CONNECTION_MODE=persistente durante toda la matriz
```

Antes de esta matriz ya se habían validado individualmente:

```text
FC03 Read Holding Registers=PASS
FC06 Write Single Register=PASS
RX_TX_BEFORE_MATRIX=2/2
OK_EX_BEFORE_MATRIX=2/0
HOLDING_0_BEFORE_MATRIX=12345
```

## Resultado de la matriz

```text
PASS_COUNT=9
FAIL_COUNT=0
A14_1_SERVER_FUNCTION_MATRIX=PASS
```

### FC01 — Read Coils

```text
TX=00 0A 00 00 00 06 01 01 00 00 00 08
RX=00 0A 00 00 00 04 01 01 01 00
RESULT=PASS
COILS_RAW=0x00
```

### FC02 — Read Discrete Inputs

```text
TX=00 0B 00 00 00 06 01 02 00 00 00 08
RX=00 0B 00 00 00 04 01 02 01 7A
RESULT=PASS
DISCRETE_RAW=0x7A
```

### FC04 — Read Input Registers

```text
TX=00 0C 00 00 00 06 01 04 00 00 00 02
RX=00 0C 00 00 00 07 01 04 04 01 7A 00 04
RESULT=PASS
INPUT_0=378
INPUT_1=4
```

### FC05 — Write Single Coil + FC01 readback

```text
TX_FC05=00 0D 00 00 00 06 01 05 00 00 FF 00
RX_FC05=00 0D 00 00 00 06 01 05 00 00 FF 00
FC05_RESULT=PASS

TX_READBACK=00 0E 00 00 00 06 01 01 00 00 00 01
RX_READBACK=00 0E 00 00 00 04 01 01 01 01
READBACK_COIL_0=True
READBACK_RESULT=PASS
```

### FC15 — Write Multiple Coils + FC01 readback

Patrón escrito: `0xA5`, 8 coils, empaquetado LSB-first según Modbus.

```text
TX_FC15=00 0F 00 00 00 08 01 0F 00 00 00 08 01 A5
RX_FC15=00 0F 00 00 00 06 01 0F 00 00 00 08
FC15_RESULT=PASS

TX_READBACK=00 10 00 00 00 06 01 01 00 00 00 08
RX_READBACK=00 10 00 00 00 04 01 01 01 A5
EXPECTED=0xA5
RECEIVED=0xA5
READBACK_RESULT=PASS
```

### FC16 — Write Multiple Holding Registers + FC03 readback

Se escribe desde `Holding[1]` para no modificar el valor `12345` previamente validado en `Holding[0]`.

```text
HOLDING_1=0x1111
HOLDING_2=0x2222
HOLDING_3=0x3333

TX_FC16=00 11 00 00 00 0D 01 10 00 01 00 03 06 11 11 22 22 33 33
RX_FC16=00 11 00 00 00 06 01 10 00 01 00 03
FC16_RESULT=PASS

TX_READBACK=00 12 00 00 00 06 01 03 00 01 00 03
RX_READBACK=00 12 00 00 00 09 01 03 06 11 11 22 22 33 33
READBACK_RESULT=PASS
```

## Diagnóstico interno posterior

```text
SERVER_STATE=READY
CLIENT=NONE
LAST_ERROR=OK
RX_FRAMES=11
TX_FRAMES=11
REQUESTS_OK=11
EXCEPTIONS=0
HOLDING_0=12345
COILS_RAW=0xA5
UNEXPECTED_RESET=0
```

La matriz agregó nueve requests sobre una única conexión TCP persistente y el Server volvió luego a `CLIENT=NONE` sin error.

## Conclusión

Quedan validadas físicamente, end-to-end, las ocho Function Codes objetivo del Server Alpha14.1:

```text
FC01=PASS
FC02=PASS
FC03=PASS
FC04=PASS
FC05=PASS
FC06=PASS
FC15=PASS
FC16=PASS
A14_1_FC01_02_03_04_05_06_15_16=PASS
A14_1_PERSISTENT_CONNECTION_MATRIX=PASS
A14_1_SERVER_FUNCTION_MATRIX=PASS
```

Este gate prueba requests válidos. Antes de cerrar completamente A14.1 todavía deben validarse explícitamente excepciones/protocolo inválido y recuperación posterior; la coexistencia RTU+TCP pertenece a un gate posterior de Alpha14 y no se infiere de esta matriz.
