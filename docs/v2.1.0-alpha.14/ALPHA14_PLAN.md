# v2.1.0-alpha.14 — JWPLC_ModbusTCP

Fecha de inicio: `2026-09-11`

## Base

```text
BRANCH=v2.1.0-alpha.14/feature/modbus-tcp
BASE_BRANCH=release/v2.1.x
BASE_SHA=165f94fb25617d3a6035fdfd9899bd38a1331583
ALPHA11_STATUS=CLOSED_PUBLISHED
ROADMAP_V2_1_CLOSURE=DEFINED
```

## Objetivo

Añadir una implementación Modbus TCP nativa para sketches Arduino sobre el W5500 integrado del JWPLC Basic, reutilizando `JWPLC_Ethernet` y preservando la arquitectura actual del package.

```text
JWPLC_ModbusTCP
    -> JWPLC_Ethernet
        -> EthernetClient / EthernetServer
            -> W5500
```

No se mezcla el transporte TCP con `JWPLC_ModbusRTU`:

```text
JWPLC_RS485     -> transporte RS-485
JWPLC_ModbusRTU -> protocolo Modbus RTU
JWPLC_Ethernet  -> transporte Ethernet/W5500
JWPLC_ModbusTCP -> protocolo Modbus TCP
```

## Decisiones iniciales

1. `JWPLC_ModbusTCP` será una librería propia del package.
2. La librería será opt-in durante el desarrollo de Alpha14:

```cpp
#include <JWPLC_ModbusTCP.h>
```

3. No se añade todavía al autoload global. Es una capacidad nueva, no un periférico existente que se retire.
4. Esta decisión protege el gate de build speed/discovery cerrado en Alpha10.
5. El servidor no se inicia automáticamente: el sketch debe solicitarlo explícitamente.
6. Puerto por defecto: `502`, configurable.
7. El motor final Client/Master será cooperativo/no bloqueante por defecto, siguiendo el patrón validado en `JWPLC_ModbusRTU`.
8. Las variantes Sync, si se exponen, se construirán sobre el mismo motor cooperativo.
9. Todo acceso al W5500 debe respetar el mutex SPI compartido JWPLC.
10. No se declarará coexistencia RTU+TCP como PASS hasta completar gate físico simultáneo.
11. Alpha14 incluirá un benchmark físico de rendimiento Modbus TCP para determinar tasa máxima estable, pico de saturación y polling recomendado, tanto aislado como bajo runtime normal del JWPLC.

## Alcance funcional Alpha14

Server/Slave:

- FC01 — Read Coils;
- FC02 — Read Discrete Inputs;
- FC03 — Read Holding Registers;
- FC04 — Read Input Registers;
- FC05 — Write Single Coil;
- FC06 — Write Single Register;
- FC15 — Write Multiple Coils;
- FC16 — Write Multiple Registers;
- MBAP Transaction ID;
- Protocol ID `0`;
- Length;
- Unit ID configurable;
- excepciones `01`, `02`, `03`, `04`;
- estadísticas de requests/responses/excepciones/errores.

Client/Master:

- FC01/02/03/04/05/06/15/16;
- motor cooperativo;
- transaction ID;
- timeout;
- reconexión;
- estado/resultados consultables;
- API Sync explícita sólo sobre el mismo motor.

## Límites Modbus previstos

```text
FC01/FC02 read bits        : 1..2000
FC03/FC04 read registers   : 1..125
FC15 write coils           : 1..1968
FC16 write registers       : 1..123
ADU TCP máxima             : 260 bytes
PDU máxima                 : 253 bytes
```

Los rangos deben validarse además contra el mapa registrado por el usuario.

## Gates incrementales

### A14.1 — Foundation + Server

- crear librería `JWPLC_ModbusTCP`;
- API de mapas coherente con RTU;
- parser MBAP incremental;
- Server FC01/02/03/04/05/06/15/16;
- excepciones y estadísticas;
- ejemplo Server;
- compilación Arduino CLI/IDE.

```text
A14_1_COMPILE=NOT_EXECUTED
A14_1_SERVER_RUNTIME=NOT_EXECUTED
```

### A14.2 — Client cooperativo

- extensión Ethernet mínima si es necesaria para conexión TCP no bloqueante;
- TX TCP cooperativo para no depender de `EthernetClient::write()` bloqueante;
- requests FC01/02/03/04/05/06/15/16;
- state machine;
- ejemplo Client;
- timeout/recovery.

```text
A14_2_CLIENT_COMPILE=NOT_EXECUTED
A14_2_CLIENT_RUNTIME=NOT_EXECUTED
```

### A14.3 — Matriz funcional + benchmark de rendimiento

- PC/JW Modbus Tool contra Server JWPLC;
- JWPLC Client contra servidor de prueba;
- FC por FC;
- Unit ID;
- excepciones;
- rangos;
- pérdida/reconexión Ethernet;
- benchmark físico de rendimiento según `A14_MODBUS_TCP_PERFORMANCE_BENCHMARK_PLAN.md`;
- sweep de 10, 20, 50, 100, 200, 500 y 1000 req/s, más saturación sin espera;
- tamaños representativos y máximos de FC01/03/15/16;
- latencia p50/p95/p99, throughput, timeouts, errores, service gap y resets;
- tasa máxima estable y tasa pico para Server y Client;
- repetición de puntos representativos manteniendo runtime/periféricos normales.

### A14.4 — Coexistencia industrial

Gate físico simultáneo:

```text
TFT
RTC
FRAM
SD
Ethernet
Modbus RTU
Modbus TCP
```

Requiere evidencia física y logs.

Además se repetirá un subconjunto del benchmark para conocer el throughput TCP sostenible con RTU y periféricos activos simultáneamente.

### A14.5 — Caso robot / interoperabilidad

Validar contra el equipo real disponible cuando se confirme:

```text
robot/controller exacto
software/OS
opción/licencia fieldbus
rol Modbus TCP del robot
IP/port
mapa de coils/registers
endianness de datos multirregistro
```

No asumir rol Client/Server del robot sin manual/configuración exactos.

## Criterios de cierre

```text
MODBUS_TCP_SERVER=PASS
MODBUS_TCP_CLIENT=PASS
FC01=PASS
FC02=PASS
FC03=PASS
FC04=PASS
FC05=PASS
FC06=PASS
FC15=PASS
FC16=PASS
MODBUS_TCP_RECONNECT=PASS
MODBUS_TCP_PERFORMANCE_BENCHMARK=PASS_PHYSICAL
SERVER_MAX_STABLE_REQ_S=MEASURED
CLIENT_MAX_STABLE_REQ_S=MEASURED
FULL_RUNTIME_MAX_STABLE_REQ_S=MEASURED
MODBUS_RTU_TCP_SIMULTANEOUS=PASS_PHYSICAL
SPI_SHARED_BUS_REGRESSION=0
BUILD_SPEED_MATERIAL_REGRESSION=NO
AUTOLOAD_PERIPHERALS_REMOVED=NO
```

## Estado

```text
ALPHA14_STATUS=IN_PROGRESS
CURRENT_GATE=A14.2_CLIENT_COOPERATIVE
```
