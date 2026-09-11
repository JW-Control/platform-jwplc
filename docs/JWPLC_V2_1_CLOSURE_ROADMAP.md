# JWPLC Basic v2.1.x — Hoja de ruta de cierre

Fecha de definición: `2026-09-11`

Repositorio:

```text
JW-Control/platform-jwplc
```

Base técnica al definir esta hoja de ruta:

```text
release/v2.1.x @ ded3585a01b2802ddfceccf7ff50f2576de0b16d
v2.1.0-alpha.11 = CLOSED_PUBLISHED
NEXT_ALPHA = UNBLOCKED
```

## 1. Objetivo

Cerrar `2.1.x` como una plataforma PLC compacta estable, reproducible y utilizable en proyectos reales, completando las capacidades que todavía faltan alrededor de OpenPLC, HMI, Modbus TCP, retentividad, registro de datos y diagnóstico.

La prioridad del ciclo continúa siendo:

```text
1. Estabilidad.
2. Compatibilidad Arduino IDE.
3. No romper APIs ya probadas.
4. Registrar decisiones y evidencia.
5. Cerrar pendientes antes de avanzar al siguiente hito de integración.
6. Documentación y PR en español.
```

No se retiran periféricos del autoload normal para mejorar tiempos de build.

No se asume:

```text
OpenPLC integrado al runtime Arduino normal.
OTA definido.
FlashFreq universal futura definida.
bootloader.bin definitivo.
```

## 2. Estrategia general

Los alphas `12` a `17` pueden desarrollarse por tracks parcialmente independientes, pero la publicación debe conservar una secuencia controlada y evidencia acumulativa.

```text
                         ┌─ A12 OpenPLC Closure ───────┐
                         │                             ↓
Alpha11 ─────────────────┼────────────────────────── A13 HMI ↔ OpenPLC
                         │
                         ├─ A14 Modbus TCP ────────────┐
                         │                             │
                         ├─ A15 Retain / FRAM ─────┐   │
                         │                         │   │
                         └─ A16 DataLogger / Time ─┤   │
                                                   ↓   ↓
                                                A17 Diagnostics
                                                     │
                                                     ↓
                                                A18 Qualification
                                                     │
                                                     ↓
                                                2.1.0-rc.1
                                                     │
                                                     ↓
                                                   2.1.0
```

Desarrollo paralelo permitido:

```text
Track A — IEC / HMI       : A12 -> A13
Track B — Comunicaciones  : A14
Track C — Servicios PLC   : A15 + A16
Track D — Sistema         : A17 transversal
```

A18 es un alpha de congelamiento e integración. No debe introducir features nuevas.

## 3. Alpha12 — OpenPLC Engineering Closure

Branch previsto:

```text
v2.1.0-alpha.12/feature/openplc-engineering-closure
```

Objetivo: cerrar la deuda de ingeniería conocida de Alpha9/OpenPLC antes de ampliar nuevas capacidades IEC.

Alcance previsto:

- selector de baudrate del Backplane RTU;
- selector de formato serie;
- propagación completa UI -> proyecto -> HAL;
- persistencia de configuración del Backplane;
- resolución de miembros de Function Blocks como `TON0.Q`, `TOF0.Q`, `TP0.Q`;
- autocomplete tipado de miembros FB;
- gate físico de Remote I/O multibit simultáneo;
- freeze reproducible del fork `openplc-editor` utilizado por JWPLC Edition;
- generación reproducible del instalador JWPLC Edition;
- reapertura/rebuild del proyecto sin pérdida de configuración.

Gate de cierre:

```text
OPENPLC_ENGINEERING_DEBT_ALPHA9=CLOSED
BACKPLANE_CONFIGURATION_PERSISTENCE=PASS
REMOTE_IO_MULTIBIT=PASS_PHYSICAL
JWPLC_EDITOR_REPRODUCIBLE=PASS
```

## 4. Alpha13 — HMI Designer ↔ OpenPLC

Branch previsto:

```text
v2.1.0-alpha.13/feature/hmi-openplc-binding
```

Objetivo: permitir que la HMI integrada consuma símbolos IEC/OpenPLC sin glue manual en C++.

Alcance previsto:

- tabla de símbolos compartida Ladder/HMI;
- binding de `TEXT`, `VALUE`, `BOOL` y `BAR` a variables OpenPLC;
- resolución de tipos y miembros FB;
- LIVE Preview con variables OpenPLC;
- persistencia de bindings en proyecto;
- errores explícitos para símbolo inexistente o tipo incompatible.

Dependencia:

```text
A13_IMPLEMENTATION_FINAL depends_on A12_SYMBOL_CONTRACT
```

El diseño de UX puede avanzar en paralelo con A12, pero el contrato de símbolos no debe duplicarse.

## 5. Alpha14 — JWPLC_ModbusTCP

Branch previsto:

```text
v2.1.0-alpha.14/feature/modbus-tcp
```

Objetivo: añadir una API Arduino nativa de Modbus TCP sobre `JWPLC_Ethernet`/W5500 y cerrar la deuda histórica de coexistencia RTU + TCP.

Arquitectura objetivo:

```text
JWPLC_ModbusTCP
    -> JWPLC_Ethernet
        -> EthernetClient / EthernetServer
            -> W5500
```

No se reutiliza `JWPLC_ModbusRTU` como transporte TCP. Se comparten conceptos y semántica Modbus cuando sea útil, manteniendo separadas las capas:

```text
JWPLC_RS485     -> transporte RS-485
JWPLC_ModbusRTU -> Modbus RTU
JWPLC_Ethernet  -> Ethernet/W5500
JWPLC_ModbusTCP -> Modbus TCP
```

Alcance inicial previsto:

- modo Server/Slave;
- modo Client/Master cooperativo;
- puerto estándar `502` configurable;
- MBAP Transaction ID / Protocol ID / Length / Unit ID;
- FC01 Read Coils;
- FC02 Read Discrete Inputs;
- FC03 Read Holding Registers;
- FC04 Read Input Registers;
- FC05 Write Single Coil;
- FC06 Write Single Register;
- FC15 Write Multiple Coils;
- FC16 Write Multiple Registers;
- excepciones Modbus;
- timeouts y reconexión;
- estadísticas y diagnóstico;
- ejemplos Arduino;
- gate simultáneo Ethernet + TFT + RTC + FRAM + SD + RTU + TCP.

La API principal debe ser cooperativa/no bloqueante. Las APIs síncronas, si se conservan, deben construirse sobre el mismo motor para evitar semánticas divergentes, siguiendo el criterio ya usado en `JWPLC_ModbusRTU`.

Gate de cierre mínimo:

```text
MODBUS_TCP_SERVER=PASS
MODBUS_TCP_CLIENT=PASS
FC01_02_03_04_05_06_15_16=PASS
MODBUS_TCP_RECONNECT=PASS
MODBUS_RTU_TCP_SIMULTANEOUS=PASS_PHYSICAL
SPI_SHARED_BUS_REGRESSION=0
AUTOLOAD_PERIPHERALS_REMOVED=NO
```

No publicar métricas de transacciones por segundo hasta guardar cliente/script, FC, tamaño de trama, número de clientes, duración, commit, logs y errores.

## 6. Alpha15 — Retentividad industrial / FRAM

Branch previsto:

```text
v2.1.0-alpha.15/feature/retain-fram
```

Objetivo: formalizar la FRAM existente como servicio de retentividad del PLC.

Alcance previsto:

- `JWPLC_Retain`;
- tipos básicos y estructuras versionadas;
- magic/schema/version/sequence/CRC/payload;
- escritura segura frente a reset/power loss;
- restauración automática;
- migración de schema explícita;
- integración OpenPLC `RETAIN`/persistente si el runtime lo permite sin romper arquitectura.

Gate de cierre:

```text
RETAIN_POWER_CYCLE=PASS_PHYSICAL
RETAIN_CORRUPTION_DETECTION=PASS
FRAM_WEAR_DEPENDENCE=NO_FLASH_WEAR
```

## 7. Alpha16 — DataLogger + tiempo industrial

Branch previsto:

```text
v2.1.0-alpha.16/feature/datalogger-time
```

Objetivo: registrar variables/eventos de proceso con tiempo confiable offline/online.

Alcance previsto:

- `JWPLC_DataLogger`;
- servicio de tiempo común;
- RTC como referencia offline;
- sincronización NTP por Ethernet cuando exista red;
- logging periódico y por evento;
- CSV y/o formato estructurado definido por gate;
- rotación de archivos;
- manejo SD ausente, llena, retirada y reinsertada;
- timestamps monotónicos/coherentes ante pérdida de red.

Gate de cierre:

```text
RTC_OFFLINE_LOGGING=PASS
NTP_SYNC=PASS
SD_REMOVE_RECOVERY=PASS_PHYSICAL
SD_FULL_BEHAVIOR=PASS
```

## 8. Alpha17 — Diagnostics + Watchdog + Event Log

Branch previsto:

```text
v2.1.0-alpha.17/feature/system-diagnostics
```

Objetivo: consolidar diagnóstico industrial transversal para que el JWPLC pueda informar la causa de fallos y recuperaciones.

Diseño previsto:

```text
JWPLC_System
JWPLC_Diagnostics
JWPLC_EventLog
```

Campos/eventos comunes sugeridos:

```text
source
code
severity
timestamp
value
```

Productores esperados:

```text
A12/OpenPLC -> backplane/recovery
A14/TCP     -> connect/timeout/exception/recovery
A15/Retain  -> CRC/schema/recovery
A16/Logger  -> SD/NTP/time
Ethernet    -> link/DHCP/SPI
RTU         -> timeout/CRC/exception/recovery
```

Alcance previsto:

- uptime;
- reset reason;
- last fault;
- heap disponible;
- scan time y max scan time;
- contadores de errores BUS/ETH;
- ring buffer persistente para eventos críticos;
- watchdog/health service compatible con loops de usuario intensivos;
- sin requerir `delay()` artificial del sketch.

Gate de cierre:

```text
DIAGNOSTICS_COMMON_MODEL=PASS
EVENT_LOG_POWER_CYCLE=PASS
USER_DELAY_REQUIRED=NO
WATCHDOG_FALSE_POSITIVES=0
```

## 9. Alpha18 — Freeze / Qualification 2.1

Branch previsto:

```text
v2.1.0-alpha.18/qualification/release-freeze
```

Regla:

```text
NEW_FEATURES=NO
```

Objetivo: integrar y someter a regresión todo el ciclo 2.1 antes del RC.

Matriz mínima:

- Arduino CLI;
- Arduino IDE;
- JWPLC Basic;
- JWPLC Basic Core;
- OpenPLC/JWPLC Edition;
- HMI Designer;
- DI/DO;
- botones/TFT;
- RTC;
- FRAM/Retain;
- SD/DataLogger;
- Ethernet DHCP/IP estática;
- Modbus RTU;
- Modbus TCP;
- RTU + TCP simultáneo;
- Remote I/O;
- power-cycle;
- pérdida/recuperación de red;
- pérdida/recuperación de RS-485;
- SD removal/full;
- cold/warm build;
- package instalado desde índice dev;
- soak 24–72 h según banco disponible.

También debe cerrar explícitamente:

```text
APP_ONLY=conclusion final 2.1
BOOTLOADER_PRECOMPILED=conclusion final 2.1
FINAL_CURRENT_HARDWARE_CONFIGURATION=decision or explicit pending
API_FREEZE=PASS
BACKWARD_COMPATIBILITY=PASS
```

No se declara una configuración universal para futuras revisiones de hardware si no existe evidencia para hacerlo.

## 10. Release Candidate y estable

Flujo previsto:

```text
v2.1.0-alpha.18
    -> v2.1.0-rc.1
        -> bugs/regresiones solamente
            -> v2.1.0
```

Crear `alpha.19` únicamente si el RC descubre una modificación arquitectónica que no corresponda a un simple fix de estabilización.

## 11. Reglas de paralelización

Pueden abrirse desde el cierre canónico de Alpha11:

```text
A12 OpenPLC Engineering Closure
A14 Modbus TCP
A15 Retain/FRAM
A16 DataLogger/Time
```

A13 puede avanzar en UX/diseño, pero su binding final espera el contrato de símbolos de A12.

A17 puede definir desde temprano el modelo común de diagnóstico, pero debe integrar productores reales de A12/A14/A15/A16 antes del cierre.

A18 no puede comenzar como cierre formal hasta que A12–A17 hayan convergido.

## 12. Regla de integración Git

Todo branch 2.1 debe registrar el SHA base real.

Base oficial:

```text
release/v2.1.x
```

No asumir que una rama con número de alpha mayor contiene automáticamente el mejor punto de partida.

Cuando existan desarrollos paralelos:

1. mantener el scope de cada rama aislado;
2. evitar editar el mismo archivo sin necesidad;
3. actualizar/rebasar o recrear el branch desde `release/v2.1.x` antes del PR final cuando hayan entrado otros alphas;
4. ejecutar la regresión correspondiente después de incorporar los cambios previos;
5. preservar el package y APIs ya publicados.

La numeración expresa el orden de publicación, no obliga a desarrollar todo secuencialmente.

## 13. Estado de esta hoja de ruta

```text
ROADMAP_V2_1_CLOSURE=DEFINED
ROADMAP_PARALLEL_TRACKS=DEFINED
ALPHA11_STATUS=CLOSED_PUBLISHED
ALPHA12=READY_TO_START
ALPHA14=READY_TO_START_IN_PARALLEL
ALPHA15=READY_TO_START_IN_PARALLEL
ALPHA16=READY_TO_START_IN_PARALLEL
ALPHA18=BLOCKED_BY_INTEGRATION
```
