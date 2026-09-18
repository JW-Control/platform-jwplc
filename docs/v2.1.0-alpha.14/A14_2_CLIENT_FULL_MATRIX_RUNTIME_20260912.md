# Alpha14 — A14.2 Client Full Matrix Runtime

Fecha: `2026-09-12`

## Objetivo

Validar físicamente la matriz funcional completa del `JWPLC_ModbusTCPClient` sobre una única conexión TCP persistente, comprobando secuencia de Transaction ID, payloads exactos y estadísticas internas sin errores.

## Topología

```text
PC Server de prueba: 192.168.0.4:15026
JWPLC Basic Client : 192.168.0.31
Unit ID            : 1
TCP connections    : 1
```

## Resultado PC

Se recibieron ocho requests consecutivas sobre la misma sesión TCP:

```text
REQ1_FC01_TID1=PASS
REQ2_FC02_TID2=PASS
REQ3_FC03_TID3=PASS
REQ4_FC04_TID4=PASS
REQ5_FC05_TID5=PASS
REQ6_FC06_TID6=PASS
REQ7_FC15_TID7=PASS
REQ8_FC16_TID8=PASS
FC15_PADDING_MASK=PASS
FC16_BIG_ENDIAN_PAYLOAD=PASS
PASS_COUNT=8
FAIL_COUNT=0
TCP_CONNECTIONS=1
A14_2_CLIENT_FULL_MATRIX_PC=PASS
```

Requests observadas:

```text
FC01: 00 01 00 00 00 06 01 01 00 00 00 0A
FC02: 00 02 00 00 00 06 01 02 00 00 00 0A
FC03: 00 03 00 00 00 06 01 03 00 00 00 02
FC04: 00 04 00 00 00 06 01 04 00 00 00 02
FC05: 00 05 00 00 00 06 01 05 00 02 FF 00
FC06: 00 06 00 00 00 06 01 06 00 03 24 68
FC15: 00 07 00 00 00 09 01 0F 00 10 00 0A 02 A5 03
FC16: 00 08 00 00 00 0D 01 10 00 20 00 03 06 11 11 AB CD 24 68
```

Para FC15, el buffer de origen usado por el sketch fue `A5 FF` con `quantity=10`; en wire se transmitió `A5 03`, confirmando limpieza determinista de los seis bits de padding no pertenecientes a la solicitud.

Para FC16, los registros `0x1111`, `0xABCD`, `0x2468` se transmitieron big-endian como `11 11 AB CD 24 68`.

## Resultado JWPLC

```text
FC01_BYTES=A5,02
FC02_BYTES=5A,01
FC03_VALUES=4369,8738
FC04_VALUES=4660,43981
TID_SEQUENCE=1,2,3,4,5,6,7,8
SESSION_CONNECTED=YES
CONNECTIONS=1
TX_FRAMES=8
RX_FRAMES=8
REQUESTS_OK=8
EXCEPTIONS=0
TIMEOUTS=0
TRANSPORT_ERRORS=0
PROTOCOL_ERRORS=0
BUS_LOCK_TIMEOUTS=0
A14_2_CLIENT_FULL_MATRIX_RUNTIME=PASS
```

Las respuestas FC01/FC02 incluyeron bits altos sucios en el último byte y el Client los enmascaró según `quantity`, manteniendo la semántica LSB-first ya validada con Modbus RTU.

## Conclusión

```text
A14_2_CLIENT_FC01=PASS_PHYSICAL
A14_2_CLIENT_FC02=PASS_PHYSICAL
A14_2_CLIENT_FC03=PASS_PHYSICAL
A14_2_CLIENT_FC04=PASS_PHYSICAL
A14_2_CLIENT_FC05=PASS_PHYSICAL
A14_2_CLIENT_FC06=PASS_PHYSICAL
A14_2_CLIENT_FC15=PASS_PHYSICAL
A14_2_CLIENT_FC16=PASS_PHYSICAL
A14_2_CLIENT_FULL_MATRIX=PASS_PHYSICAL
A14_2_CLIENT_PERSISTENT_SESSION=PASS
A14_2_CLIENT_TRANSACTION_ID_SEQUENCE=PASS
```

A14.2 continúa abierto únicamente para hardening de timeout/reconexión, revisión final del backend Ethernet y cierre de API/ejemplo Client.
