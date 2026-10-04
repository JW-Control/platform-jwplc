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

## 2. Estrategia general — revisión 2026-10-03

La numeración original definida el 11-sep queda supersedida por la decisión de
release tomada después del cierre técnico de comunicaciones.

La secuencia canónica pasa a ser:

```text
Alpha11 = CLOSED / PUBLISHED
   |
   v
Alpha12 = Communications + Runtime Consolidation
           Ethernet/W5500
           Modbus TCP
           Modbus RTU hardening/optimization
           full-runtime + coexistence
   |
   v
Alpha13 = TFT / Display Update
           actualizar el alpha originalmente orientado a TFT
           sobre JWPLC_TFT/JWPLC_Display reales post-H3E
   |
   v
Alpha14 = OpenPLC + Integration Improvements
           cerrar deuda OpenPLC
           integrar el nuevo estado de TCP/RTU optimizados
           reauditar bindings/Remote I/O/HMI que correspondan
   |
   v
Alpha15 = Retain / FRAM
Alpha16 = DataLogger / Time
Alpha17 = Diagnostics
Alpha18 = Qualification / Freeze
   |
   v
2.1.0-rc.1 -> 2.1.0
```

La evidencia histórica desarrollada bajo
`v2.1.0-alpha.14/feature/modbus-tcp` se preserva y se promociona como release
real `v2.1.0-alpha.12`. Ver
`docs/v2.1.0-alpha.12/ALPHA12_RENUMBERING_MAP_20261003.md`.

## 3. Alpha12 — Communications + Runtime Consolidation

Branch canónico de cierre:

```text
v2.1.0-alpha.12/feature/modbus-tcp
```

Objetivo: publicar como siguiente alpha real todo el trabajo de comunicaciones
y runtime cerrado después de Alpha11.

Alcance consolidado:

- `JWPLC_ModbusTCP` Server/Client cooperativos;
- FC01/02/03/04/05/06/15/16;
- lifecycle TCP cooperativo;
- TX TCP asíncrono interno;
- Ethernet/W5500 hardening;
- W5500 26 MHz en perfil actual validado;
- fast-path UDP aditivo/interno;
- política TCP RX C0 POLLING;
- optimización/hardening Modbus RTU;
- motor RTU ASYNC/SYNC;
- queued TX sobre AutoDirection;
- timing RTU en microsegundos;
- full runtime con Display/SD/FRAM/RTC/I/O/buttons;
- ceilings TCP/UDP/RTU;
- coexistencia TCP+RTU+UDP.

Perfil coexistente confirmado 600 s:

```text
TCP=250.001 req/s
RTU=796.953 req/s
RTU_SCAN=99.619 scans/s
UDP_FAST=0.998 Mbps
UDP_DELIVERY=100 %
FULL_RUNTIME_CLEAN=YES
```

El contrato RTU es operacional y no hard-real-time cero-skip.

Gate de cierre del release:

```text
PACKAGE_DOCS=PASS
PRECOMPILED_ARTIFACTS=REQUALIFIED
ARDUINO_CLI_FINAL=PASS
ARDUINO_IDE_FINAL=PASS
PHYSICAL_UPLOAD_FINAL=PASS
CI_FINAL=PASS
PUBLISHED_PACKAGE_ISOLATED_VALIDATION=PASS
ALPHA12_STATUS=CLOSED_PUBLISHED
```

## 4. Alpha13 — TFT / Display Update

Branch previsto:

```text
v2.1.0-alpha.13/feature/tft-display-update
```

Objetivo: retomar el alpha originalmente planificado para TFT, pero
actualizándolo al estado real del package después de Alpha12.

Debe partir de:

- `JWPLC_TFT` ya creado;
- migración H3E ya realizada;
- `JWPLC_Display` post-H3E;
- batching/dirty refresh actuales;
- coexistencia SPI ya requalificada con Ethernet/RTU;
- compatibilidad con HMI Designer Alpha11.

Antes de implementar features nuevas se debe reauditar el alcance original de
TFT y eliminar tareas que ya hayan sido resueltas incidentalmente durante el
trabajo de Alpha12.

No se debe volver a introducir TFT_eSPI como dependencia pública del usuario.

## 5. Alpha14 — OpenPLC + mejoras de integración

Branch previsto:

```text
v2.1.0-alpha.14/feature/openplc-integration
```

Objetivo: retomar el alpha originalmente orientado a OpenPLC, actualizando su
arquitectura al package publicado de Alpha12.

El nuevo baseline debe considerar explícitamente:

- `JWPLC_ModbusTCP` nativo ya disponible;
- `JWPLC_ModbusRTU` ASYNC/SYNC actualizado;
- RTU fast/500 kbaud sólo donde el perfil validado aplique;
- coexistencia TCP/RTU medida;
- Ethernet cooperativo;
- Remote I/O existente;
- configuración/persistencia del Backplane;
- integración HMI/OpenPLC pendiente que siga siendo relevante.

No asumir OpenPLC dentro del autoload Arduino normal.

El alcance exacto se congela sólo después de publicar Alpha13 y reauditar el
fork/editor/HAL contra las APIs reales de Alpha12/Alpha13.

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

## 11. Reglas de secuencia

La publicación queda deliberadamente secuencial:

```text
Alpha12 -> Alpha13 -> Alpha14
```

No iniciar formalmente Alpha13 hasta que Alpha12 esté
`CLOSED_PUBLISHED`.

No iniciar formalmente Alpha14 hasta reauditar el resultado publicado de
Alpha13, porque Display/TFT y OpenPLC comparten superficie de integración HMI.

Alpha15/16/17 pueden conservar ideas o prototipos paralelos, pero sus releases
no deben saltar pendientes del cierre anterior.

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
ROADMAP_V2_1_CLOSURE=RENUMBERED_20261003
ALPHA11_STATUS=CLOSED_PUBLISHED
ALPHA12=PACKAGE_CLOSURE_IN_PROGRESS
ALPHA13=TFT_DISPLAY_UPDATE_AFTER_ALPHA12
ALPHA14=OPENPLC_INTEGRATION_AFTER_ALPHA13
ALPHA15=PLANNED_RETAIN_FRAM
ALPHA16=PLANNED_DATALOGGER_TIME
ALPHA17=PLANNED_DIAGNOSTICS
ALPHA18=BLOCKED_BY_INTEGRATION
```

La numeración original del documento se conserva en Git history, pero esta
versión es la fuente canónica para próximos releases.
