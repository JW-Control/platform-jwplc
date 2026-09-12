# Alpha14.2 — Client Modbus TCP FC15/FC16 — Runtime físico

Fecha: `2026-09-12`

Branch: `v2.1.0-alpha.14/feature/modbus-tcp`

Parser consolidado:

```text
c55eacde039f391b0c40ab1573ef474c4c6b5d5a
feat(modbus-tcp): validar respuestas Client FC15 y FC16
```

## Objetivo

Validar físicamente el Client Modbus TCP para:

- FC15 — Write Multiple Coils
- FC16 — Write Multiple Registers
- persistencia de la sesión TCP
- Transaction ID incremental
- regresión FC03 posterior a FC15/FC16
- limpieza de bits de padding en FC15
- serialización big-endian del payload FC16

Endpoint PC usado:

```text
IP=192.168.0.4
PORT=15025
UNIT_ID=1
```

JWPLC obtenido por DHCP:

```text
IP=192.168.0.31
```

## Evidencia PC

Se aceptó una única conexión TCP:

```text
TCP_ACCEPT=PASS
REMOTE=192.168.0.31:59082
TCP_CONNECTIONS=1
```

### FC15

Solicitud recibida:

```text
00 01 00 00 00 09 01 0F 00 10 00 0A 02 A5 03
```

Resultado:

```text
REQ1_FC15_TID1=PASS
FC15_PADDING_MASK=A5_FF_TO_A5_03_PASS
```

El sketch entregó como fuente `A5 FF` para `quantity=10`; el Client transmitió `A5 03`, limpiando correctamente los 6 bits no utilizados del último byte.

Respuesta enviada por PC:

```text
00 01 00 00 00 06 01 0F 00 10 00 0A
```

### FC16

Solicitud recibida:

```text
00 02 00 00 00 0D 01 10 00 20 00 03 06 11 11 AB CD 24 68
```

Resultado:

```text
REQ2_FC16_TID2=PASS
FC16_BIG_ENDIAN_PAYLOAD=PASS
```

Los tres registros `0x1111`, `0xABCD`, `0x2468` fueron serializados correctamente en big-endian.

Respuesta enviada por PC:

```text
00 02 00 00 00 06 01 10 00 20 00 03
```

### Regresión FC03

Solicitud recibida:

```text
00 03 00 00 00 06 01 03 00 00 00 01
```

Resultado:

```text
REQ3_FC03_REGRESSION_TID3=PASS
```

Respuesta enviada:

```text
00 03 00 00 00 05 01 03 02 13 57
```

Resumen PC:

```text
PASS_COUNT=3
FAIL_COUNT=0
TCP_CONNECTIONS=1
A14_2_CLIENT_FC15_FC16_PC=PASS
```

## Evidencia JWPLC

```text
FC15_TID=1
FC16_TID=2
FC03_CONTROL_TID=3
FC03_CONTROL_VALUE=4951
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
A14_2_CLIENT_FC15_FC16_RUNTIME=PASS
```

`4951` decimal corresponde a `0x1357`.

## Conclusión

```text
A14_2_CLIENT_FC15=PASS
A14_2_CLIENT_FC16=PASS
A14_2_CLIENT_FC15_FC16_RUNTIME=PASS
A14_2_CLIENT_FC15_PADDING_MASK=PASS
A14_2_CLIENT_FC16_BIG_ENDIAN=PASS
A14_2_CLIENT_PERSISTENT_SESSION=PASS
A14_2_CLIENT_TID_INCREMENTAL=PASS
A14_2_CLIENT_FC03_POST_WRITE_REGRESSION=PASS
```

Con esta prueba, FC01/FC02/FC03/FC04/FC05/FC06/FC15/FC16 ya cuentan con validación física dentro de Alpha14.2. A14.2 permanece en progreso hasta completar la matriz Client consolidada, ejemplo de uso y validaciones de timeout/reconexión.
