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
CURRENT_GATE=A14.1_FC03_PHYSICAL
A14_1_IMPLEMENTATION=PASS_SOURCE_COMPLETE
A14_1_COMPILE=PASS
A14_1_EMPTY_SKETCH_REGRESSION=PASS
A14_1_SERVER_LISTEN_SMOKE=PASS
A14_1_FC03=NOT_EXECUTED
A14_1_FC06=NOT_EXECUTED
A14_1_SERVER_RUNTIME=PARTIAL_PASS
A14_2_CLIENT=NOT_STARTED
RTU_TCP_SIMULTANEOUS=NOT_EXECUTED
```

## Evidencia de compilación A14.1

Validación ejecutada por el usuario sobre checkout local de la rama Alpha14:

```text
FQBN=jwplc_local:esp32:jwplcbasic
PLATFORM=jwplc_local:esp32 2.1.0-dev
JWPLC_ModbusTCP=0.1.0
COMPILE_EXIT_CODE=0
PROGRAM_BYTES=417893
PROGRAM_PERCENT=10
GLOBAL_VARIABLE_BYTES=28452
GLOBAL_VARIABLE_PERCENT=8
LOCAL_VARIABLE_BYTES_AVAILABLE=299228
A14_1_COMPILE=PASS
```

La resolución de librerías confirmó que `JWPLC_ModbusTCP`, `JWPLC_Ethernet`, Display, RTC, FRAM, SD, botonera, RS-485 y Modbus RTU provinieron del árbol local `JWPLC/2.1.0/libraries`.

```text
LOCAL_PACKAGE_RESOLUTION=PASS
UNEXPECTED_EXTERNAL_JWPLC_LIBRARY=NO
```

## Regresión de sketch vacío / build speed

Run:

```text
RUN=20260911_161448
LABEL=alpha14-empty-regression
TARGET=Basic
SKETCH=01_empty
```

Resultados Alpha14:

| Fase | Tiempo | Compiladores | Binarios |
|---|---:|---:|---:|
| managed cold | 65.517 s | 15 | n/a |
| managed warm no-change | 24.070 s | 1 | n/a |
| managed warm touch | 23.114 s | 1 | n/a |
| explicit cold | 61.851 s | 15 | 4,619,056 bytes |
| explicit warm no-change | 22.779 s | 1 | 4,619,056 bytes |
| explicit warm touch | 22.624 s | 1 | 4,619,056 bytes |

Baseline Alpha11 `Basic / 01_empty`:

```text
cold compilers=15
warm compilers=1
explicit binary bytes=4,618,720
```

Delta agregado de binarios del benchmark:

```text
ALPHA11_BYTES=4618720
ALPHA14_BYTES=4619056
DELTA_BYTES=+336
DELTA_PERCENT≈+0.0073%
```

Conclusión:

```text
A14_1_COMPILER_STRUCTURE_PARITY=PASS
A14_1_WARM_COMPILERS=1
A14_1_EMPTY_SKETCH_BINARY_EXACT_PARITY=NO
A14_1_EMPTY_SKETCH_BINARY_REGRESSION=MATERIAL_NO
A14_1_EMPTY_SKETCH_REGRESSION=PASS
A14_1_EXACT_SPEEDUP_CLAIM=NOT_USED
```

La presencia de la nueva librería no incrementó el número de compiladores del flujo normal del sketch vacío. La variación de `+336 bytes` representa aproximadamente `+0.0073%` del agregado de binarios reportado por el benchmark y se clasifica como no material. No se interpreta el tiempo cold de una sola ejecución como regresión causal debido a la variación del host ya observada en Alpha10/Alpha11.

## Smoke físico del Server

Prueba ejecutada con `01.ModbusTCP_Server` sobre JWPLC Basic físico.

Secuencia observada:

```text
BOOT=POWERON_RESET
MODBUS_TCP_WAITING_ETHERNET=PASS
DHCP_READY=PASS
IP=192.168.0.31
TCP_PORT=502
UNIT_ID=1
SERVER_STATE=READY
LAST_ERROR=OK
CLIENT=NONE
RX_FRAMES=0
TX_FRAMES=0
REQUESTS_OK=0
EXCEPTIONS=0
UNEXPECTED_RESET=0
A14_1_SERVER_LISTEN_SMOKE=PASS
```

El server pasó de espera de Ethernet a `READY` aproximadamente 2.3 s después del mensaje inicial y permaneció `READY` en múltiples impresiones periódicas sin error ni reset observado.

Este gate confirma inicialización Ethernet/DHCP y socket de escucha. Todavía no confirma procesamiento de ninguna Function Code; por eso `A14_1_SERVER_RUNTIME` permanece como `PARTIAL_PASS` hasta ejecutar tráfico Modbus TCP real.

## Implementado en A14.1

- nueva librería opt-in `JWPLC_ModbusTCP`;
- dependencia explícita de `JWPLC_Ethernet`;
- Server configurado por `beginServer(unitId, port)`;
- espera cooperativa a que el autoload Ethernet llegue a `READY`;
- parser MBAP incremental;
- presupuesto RX por llamada a `task()`;
- mutex SPI compartido alrededor de accesos W5500;
- procesamiento de PDU fuera del mutex;
- mapas de Coils, Discrete Inputs, Holding Registers e Input Registers;
- FC01, FC02, FC03, FC04, FC05, FC06, FC15 y FC16;
- excepciones 01/02/03/04;
- estadísticas y estado;
- ejemplo `01.ModbusTCP_Server`;
- README específico.

## Decisiones de integración vigentes

`JWPLC_ModbusTCP` permanece opt-in durante Alpha14 y todavía no se añade a `JWPLC_GlobalPeripherals` ni al autoload/discovery global.

```text
NEW_PROTOCOL_FEATURE=OPT_IN_DURING_ALPHA14
ALPHA10_BUILD_SPEED_GATE=PRESERVED
EMPTY_SKETCH_COMPILER_STRUCTURE_PRESERVED=YES
```

La librería todavía no usa `precompiled=full`. Esa decisión se tomará después de estabilizar la API y completar los gates funcionales.

## Hallazgo para A14.2

El backend actual `EthernetClient::connect()` espera síncronamente a conexión/timeout. No se usará tal cual para declarar un Client/Master cooperativo.

A14.2 debe implementar una extensión mínima de conexión TCP por estados:

```text
start connect
poll socket state
connected / failed / timeout
cancel
```

Cada paso debe mantener ownership SPI sólo durante operaciones W5500 cortas.

## Lo que todavía NO se afirma

```text
MODBUS_TCP_SERVER=PASS            -> NO
MODBUS_TCP_CLIENT=PASS            -> NO
FC01_02_03_04_05_06_15_16=PASS  -> NO
MODBUS_TCP_RECONNECT=PASS         -> NO
MODBUS_RTU_TCP_SIMULTANEOUS=PASS -> NO
ROBOT_INTEROPERABILITY=PASS       -> NO
```

## Próximo gate

Validar una única operación real Modbus TCP de lectura:

```text
CLIENT=PC PowerShell
SERVER=192.168.0.31:502
UNIT_ID=1
FUNCTION=FC03 Read Holding Registers
START_ADDRESS=0
QUANTITY=1
EXPECTED_VALUE=0
```

Si FC03 pasa, el siguiente gate será FC06 sobre Holding Register 0 y lectura FC03 de confirmación.
