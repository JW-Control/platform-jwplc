# v2.1.0-alpha.14 — Estado

Actualizado: `2026-09-11`

## Identidad

```text
BRANCH=v2.1.0-alpha.14/feature/modbus-tcp
BASE_BRANCH=release/v2.1.x
BASE_SHA=165f94fb25617d3a6035fdfd9899bd38a1331583
ALPHA14_STATUS=IN_PROGRESS
```

## Gate actual

```text
CURRENT_GATE=A14.1_EXCEPTION_RECOVERY
A14_1_IMPLEMENTATION=PASS_SOURCE_COMPLETE
A14_1_COMPILE=PASS
A14_1_EMPTY_SKETCH_REGRESSION=PASS
A14_1_SERVER_LISTEN_SMOKE=PASS
A14_1_FC01=PASS
A14_1_FC02=PASS
A14_1_FC03=PASS
A14_1_FC04=PASS
A14_1_FC05=PASS
A14_1_FC06=PASS
A14_1_FC15=PASS
A14_1_FC16=PASS
A14_1_SERVER_FUNCTION_MATRIX=PASS
A14_1_EXCEPTION_RECOVERY=NOT_EXECUTED
A14_1_SERVER_RUNTIME=PARTIAL_PASS
A14_2_CLIENT=NOT_STARTED
RTU_TCP_SIMULTANEOUS=NOT_EXECUTED
```

## Evidencia de compilación

```text
FQBN=jwplc_local:esp32:jwplcbasic
PLATFORM=jwplc_local:esp32 2.1.0-dev
JWPLC_ModbusTCP=0.1.0
COMPILE_EXIT_CODE=0
PROGRAM_BYTES=417893
GLOBAL_VARIABLE_BYTES=28452
A14_1_COMPILE=PASS
LOCAL_PACKAGE_RESOLUTION=PASS
```

## Regresión de sketch vacío

Run `20260911_161448`, label `alpha14-empty-regression`.

```text
ALPHA11_BASIC_EMPTY_COLD_COMPILERS=15
ALPHA14_BASIC_EMPTY_COLD_COMPILERS=15
ALPHA11_BASIC_EMPTY_WARM_COMPILERS=1
ALPHA14_BASIC_EMPTY_WARM_COMPILERS=1
ALPHA11_EXPLICIT_BINARY_BYTES=4618720
ALPHA14_EXPLICIT_BINARY_BYTES=4619056
DELTA_BYTES=+336
DELTA_PERCENT≈+0.0073%
A14_1_COMPILER_STRUCTURE_PARITY=PASS
A14_1_EMPTY_SKETCH_BINARY_REGRESSION=MATERIAL_NO
A14_1_EMPTY_SKETCH_REGRESSION=PASS
```

`JWPLC_ModbusTCP` permanece opt-in; no se añade todavía a `JWPLC_GlobalPeripherals` ni al autoload global.

## Smoke físico del Server

Con `01.ModbusTCP_Server` sobre JWPLC Basic físico:

```text
BOOT=POWERON_RESET
DHCP_READY=PASS
IP=192.168.0.31
TCP_PORT=502
UNIT_ID=1
SERVER_STATE=READY
LAST_ERROR=OK
UNEXPECTED_RESET=0
A14_1_SERVER_LISTEN_SMOKE=PASS
```

## Gates físicos individuales

### FC03

```text
TX=00 01 00 00 00 06 01 03 00 00 00 01
RX=00 01 00 00 00 05 01 03 02 00 00
HOLDING_0=0
A14_1_FC03=PASS
```

### FC06

```text
TX=00 02 00 00 00 06 01 06 00 00 30 39
RX=00 02 00 00 00 06 01 06 00 00 30 39
EXACT_ECHO=PASS
HOLDING_0=12345
A14_1_FC06=PASS
```

## Matriz física completa del Server

Evidencia detallada: `docs/v2.1.0-alpha.14/A14_1_SERVER_FUNCTION_MATRIX_20260911.md`.

La matriz se ejecutó sobre una única conexión TCP persistente y validó:

```text
FC01 Read Coils=PASS
FC02 Read Discrete Inputs=PASS
FC04 Read Input Registers=PASS
FC05 Write Single Coil=PASS
FC05 -> FC01 readback=PASS
FC15 Write Multiple Coils=PASS
FC15 -> FC01 readback=PASS
FC16 Write Multiple Registers=PASS
FC16 -> FC03 readback=PASS
PASS_COUNT=9
FAIL_COUNT=0
```

Estado interno posterior:

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

Conclusión funcional válida a este punto:

```text
A14_1_FC01_02_03_04_05_06_15_16=PASS
A14_1_PERSISTENT_CONNECTION_MATRIX=PASS
A14_1_SERVER_FUNCTION_MATRIX=PASS
```

## Contrato de excepciones observado en source

El Server implementa respuestas de excepción Modbus y conserva la conexión para errores de PDU/Function Code válidamente enmarcados:

```text
01=Illegal Function
02=Illegal Data Address
03=Illegal Data Value
04=Server Device Failure
```

`buildException()` incrementa `exceptionsSent`, marca temporalmente `Last error=Modbus exception` y genera la respuesta con `Function | 0x80`. Un request válido posterior incrementa `requestsOk` y vuelve `Last error=OK`.

En cambio, un MBAP inválido o longitud fatal se trata como error de framing/protocolo y fuerza `dropClient()` para evitar desincronización. Esta diferencia debe conservarse en el gate físico.

## Hallazgo para A14.2

El backend actual `EthernetClient::connect()` espera síncronamente a conexión/timeout. A14.2 no reutilizará ese bloqueo como si fuera cooperativo.

Se requiere extensión mínima por estados:

```text
start connect
poll socket state
connected / failed / timeout
cancel
```

El ownership SPI debe mantenerse sólo durante operaciones W5500 cortas.

## Lo que todavía NO se afirma

```text
MODBUS_TCP_SERVER=PASS            -> NO, falta excepción/recuperación y lifecycle
MODBUS_TCP_CLIENT=PASS            -> NO
MODBUS_TCP_RECONNECT=PASS         -> NO
MODBUS_RTU_TCP_SIMULTANEOUS=PASS -> NO
ROBOT_INTEROPERABILITY=PASS       -> NO
```

## Próximo gate

Validar sobre una misma conexión TCP:

```text
EX01 Illegal Function
EX02 Illegal Data Address
EX03 Illegal Data Value
VALID_FC03_AFTER_EXCEPTIONS
```

Debe verificarse que las tres excepciones respondan correctamente, que la conexión siga utilizable y que un FC03 válido posterior recupere `Last error=OK` sin reset ni reapertura obligatoria del Server.
