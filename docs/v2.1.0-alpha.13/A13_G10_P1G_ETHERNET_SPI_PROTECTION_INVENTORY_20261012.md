# Alpha13 G10-P1G — Inventario de protección SPI Ethernet y GPIO4

Fecha: 2026-10-12.
Rama auditada: `v2.1.0-alpha.13/feature/cleanup-robustness`.
Naturaleza: **read-only**, sin cambio de producto, sin hardware.

## 1. Hallazgo hardware frente al software

El esquema facilitado por el desarrollador confirma:

| Señal JWPLC Basic v2 | ESP32-WROOM |
|---|---|
| `RST-ETH` | GPIO4 / pin físico 26 |
| `CS-ETH` | GPIO5 / pin físico 29 |
| `INT-ETH` | GPIO15 / pin físico 23 |

Sin embargo, `JWPLC_Ethernet.h` define actualmente
`JWPLC_ETH_RESET_PIN=255` si ninguna configuración externa la
sobrescribe, y `JWPLC_EthernetClass` inicializa
`_resetPin(JWPLC_ETH_RESET_PIN)`. `resetHardwareIfNeeded()`
retorna inmediatamente si `_resetPin==255`.

En el `boards.txt` de `jwplcbasic` no aparece un
`-DJWPLC_ETH_RESET_PIN=4`. La inicialización estándar
`peripherals_init.cpp` tampoco configura esa señal.

**Conclusión:** no se ha encontrado habilitación por defecto del reset
físico GPIO4 en el package normal; la ruta puede habilitarse
explícitamente mediante `setResetPin(4)` o
`configure(csPin,4)`. Es una **discrepancia entre esquema
y configuración**, no evidencia de que Alpha12 sufriera resets físicos
GPIO repetidos. No cambiar el default automáticamente: GPIO4
habilitaría una ruta de reset que hoy ocurre antes de adquirir SPI.

## 2. Cómo se obtuvo el inventario

Inspección de archivos `.cpp` fuente en la rama indicada.
Cada definición `Class::method(...)` cuenta como un método;
las sobrecargas cuentan independientemente. **No** se suman
constructores, destructores ni funciones libres/inline.

- `PROTECCIÓN LOCAL`: cuerpo contiene `acquireBus(...)` que
  llama a `jwplcSPI_acquire(...)`, sin contar la implementación
  del propio helper `acquireBus`.
- `OPERACIÓN DIRECTA`: cuerpo contiene una llamada explícita
  `Ethernet.*`, `W5100.*` o transacción/transferencia `SPI.*`.
  Es un recuento mecánico: incluye consultores software,
  no sigue llamadas indirectas, y NO certifica exclusión
  de todos los caminos.
- `SIN PROTECCIÓN LOCAL`: no significa necesariamente carrera.
  La llamada puede estar bajo el lock del invocador. Tampoco
  significa que toda API pública tenga ya ese contrato.

## 3. Resultado numérico reproducible

| Archivo | Métodos C++ | Métodos con protección local explícita | Operaciones directas backend/SPI SIN protección local |
|---|---:|---:|---:|
| `JWPLC_Ethernet.cpp` | 42 | 11 | 2* |
| `JWPLC_ModbusTCP.cpp` | 63 | 4 | 3** |
| `JWPLC_ModbusTCP_Client.cpp` | 44 | 4 | 0 |
| `EthernetClient.cpp` | 34 | 0 | 24 |
| `EthernetServer.cpp` | 6 | 0 | 5 |
| `EthernetUdp.cpp` | 20 | 0 | 14 |
| `socket.cpp` | 26 | 0 | 20 |
| `JWPLC_W5x00_Ethernet.cpp` | 22 | 0 | 18 |

- Total de **19 métodos operativos de alto nivel con adquisición
  explícita**: 11 runtime + 4 servidor Modbus + 4 cliente Modbus.
- En la capa de Client/Server/UDP compatibles con Arduino:
  **43 de 43 métodos con llamadas directas backend/SPI no adquieren
  el mutex JWPLC por sí mismos** (24+5+14).
- En los dos archivos del backend inferior:
  **38 de 38** bajo la misma regla mecánica (20+18).
- Por tanto, existen **81 definiciones de métodos con acceso directo
  al backend/SPI y sin mutex propio** dentro de las cinco unidades
  inferiores. **Esto NO significa 81 fallos ni 81 funciones
  que necesariamente requieran agregar un lock**; muchos se ejecutan
  legítimamente bajo el lock de su llamador, y otros necesitan
  rediseño centralizado de ownership.

`* JWPLC_Ethernet.cpp`: `isBusy()` consulta estado software;
`serviceL2RefreshLocked()` opera bajo el lock del
`service()` que la llama. Estos dos métodos NO se clasifican como
dos fallos de mutex. Adicionalmente, `resetHardwareIfNeeded()`
manipula GPIO antes del lock; no aparece en el conteo de transacciones
SPI y constituye un **riesgo separado** si GPIO4 se habilita.

`** JWPLC_ModbusTCP.cpp`: `configureRxIntLocked()`,
`disableRxIntLocked()` y `ackRxIntLocked()` operan sobre
W5500/SPI sin adquirir mutex en sus cuerpos. Su contrato exige
ser invocados con lock adquirido. Hay que verificar todos sus
call-sites, incluidas ramas de error y callbacks, antes de afirmar
protección completa.

### Métodos con protección local confirmada

`JWPLC_Ethernet.cpp`:
`probeHardware()`, `service()`, `begin()` síncrono,
`maintain()`, `hardwareStatus()`, `linkStatus()`,
`localIP()`, `subnetMask()`, `gatewayIP()`,
`dnsServerIP()` y `finishNetworkConfiguration()`.

`JWPLC_ModbusTCP.cpp`:
`dropClient()`, `ensureServerListening()`,
`serviceServer()`, `sendResponse()`.

`JWPLC_ModbusTCP_Client.cpp`:
`closeSession()`, `serviceConnection()`,
`serviceSend()`, `serviceResponse()`.

### Entrada pública no demostrada como protegida

Un sketch que invoque directamente `EthernetClient::connected()`,
`available()`, `stop()`, `connect()`,
`EthernetServer::begin()/available()` o
`EthernetUDP::begin()/stop()/parsePacket()` llega a
operaciones inferiores que NO realizan
`jwplcSPI_acquire()` internamente. No hay evidencia de
que todas estas entradas se encuentren envueltas por el
mutex global del JWPLC cuando se llaman directamente
desde `loop()`. Las APIs directas están publicadas en
`JWPLC_Ethernet/README.md`, no pueden descartarse.

`SPI.beginTransaction()` es un mecanismo distinto de
`jwplcSPI_acquire()`; no se debe presentar como adquisición
automática del mutex JWPLC.

## 3A. Enumeración de entradas inferiores con acceso directo

El propósito de la lista es permitir una revisión método por método.
Se indica la función, no se afirma que cada una sea un bug: la
ausencia de adquisición **local** puede ser intencional cuando
el llamador protege correctamente el recorrido.

### EthernetClient.cpp — 24/34 accesos directos sin lock local

| Métodos detectados (sobrecargas separadas) |
|---|
| `connect(const char*,uint16_t)`, `beginConnectAsync()`, `pollConnectAsync()`, `connectAsyncInProgress()`, `cancelConnectAsync()` |
| `availableForWrite()`, `write(const uint8_t*,size_t)`, `pollWriteAsync()`, `cancelWriteAsync()` |
| `available()`, `read(uint8_t*,size_t)`, `jwplcReadTcpFastDeferred()`, `jwplcCommitRxFast()`, `peek()`, `read()` |
| `pollFlushAsync()`, `beginStopAsync()`, `pollStopAsync()`, `cancelStopAsync()` |
| `connected()`, `status()`, `localPort()`, `remoteIP()`, `remotePort()` |

Las otras diez definiciones no contienen acceso explícito al
backend/SPI en el cuerpo, pero varias delegan a las anteriores
(por ejemplo `stop()`, `flush()` y `connect(IPAddress,...)`).
Por tanto, **24** no es la cantidad de entradas públicas
a proteger tras el análisis completo de call graph.

### EthernetServer.cpp — 5/6 accesos directos sin lock local

`begin()`, `available()`, `accept()`,
`statusreport()` y `write(const uint8_t*,size_t)`.
`write(uint8_t)` delega a otra función y también
debe incluirse en la evaluación de rutas públicas.

### EthernetUdp.cpp — 14/20 accesos directos sin lock local

`begin()`, `stop()`, `beginPacket(const char*,uint16_t)`,
`beginPacket(IPAddress,uint16_t)`, `beginEndPacketAsync()`,
`pollEndPacketAsync()`, `write(const uint8_t*,size_t)`,
`parsePacket()`, `jwplcReadPacketFastDeferred()`,
`jwplcCommitRxFast()`, `read(uint8_t*,size_t)`,
`read()`, `peek()`, `beginMulticast()`.

Otras funciones como `endPacket()` y
`write(uint8_t)` delegan indirectamente.

### socket.cpp — 20/26 accesos directos sin lock local

`socketBegin()`, `socketBeginMulticast()`,
`socketStatus()`, `socketClose()`,
`socketListen()`, `socketConnect()`,
`socketDisconnect()`, `socketCommitTCPFast()`,
`socketRecvUDPFastDeferred()`, `socketCommitUDPFast()`,
`socketRecvAvailable()`, `socketPeek()`,
`socketSend()`, `socketBeginSendTCP()`,
`socketPollSendTCP()`, `socketSendAvailable()`,
`socketBufferData()`, `socketStartUDP()`,
`socketBeginSendUDP()`, `socketPollSendUDP()`.

Los wrappers como `socketRecv()`,
`socketRecvTCPFastDeferred()`, `socketSendUDP()`
también deben rastrearse indirectamente; 20 no cubre todas
las operaciones efectivas del módulo.

### JWPLC_W5x00_Ethernet.cpp — 18/22 accesos directos sin lock local

`begin(uint8_t*,unsigned long,unsigned long)`,
`beginDHCPAsync()`, `pollDHCP()`,
`begin(uint8_t*,IPAddress,IPAddress,IPAddress,IPAddress)`,
`init()`, `linkStatus()`, `hardwareStatus()`,
`maintain()`, `MACAddress()`, `localIP()`,
`subnetMask()`, `gatewayIP()`, `setMACAddress()`,
`setLocalIP()`, `setSubnetMask()`,
`setGatewayIP()`, `setRetransmissionTimeout()`,
`setRetransmissionCount()`.

**Próximo gate antes del candidato:** construir una matriz
`llamador -> mutex adquirido -> función -> socket -> finalización`
para cada entrada, incluidos métodos delegados y resets.
Es prematuro afirmar que `81/81` «necesitan agregar mutex».
Se necesita una estrategia central sin doble adquisición y
evidencia de coverage completa.

## 4. Qué sí se valida y qué no

**Sí:** partes importantes del runtime Ethernet y el protocolo
Modbus TCP tienen mutex explícito; no es una arquitectura
sin protección general. El problema observado está en el
contrato mixto de wrappers protegidos y APIs directas.

**No:** este inventario NO demuestra colisiones físicas ni que
las latencias de Alpha12 fueran causadas por ausencia de mutex.
Las mediciones históricas indican múltiples contribuciones:
polling vacío, espera de transferencia SPI, gestión de sockets
y contención con periféricos. No alterar la política
POLLING_C0 aprobada ni la frecuencia W5500 26 MHz sin A/B.

## 5. Pendientes antes de implementar candidato G10-P1G

1. Trazar call graph de las 43 entradas con acceso directo,
   incluyendo las delegadas (por ejemplo `stop()`), y
   diferenciar llamadas desde sketch, runtime, DHCP y
   Modbus TCP. La **cantidad exacta de funciones realmente
   desprotegidas bajo concurrencia** permanece `REVIEW_OPEN`.
2. Evaluar y comparar mutex recursivo vs barrera W5500
   específica **sin asumir** que alguno será adoptado;
   preservar compatibilidad con FreeRTOS y el patrón actual.
3. Resolver mutex anidado (no recursivo) y lock-order
   sobre periféricos SD/FRAM/Display.
4. Proteger lease por generación y propietario de socket:
   no reusar o cerrar handles ajenos tras reset, no confundir
   `!isReady()` con reinicio y resetear listeners de forma segura.
5. Decidir `JWPLC_ETH_RESET_PIN=4` con gate dedicado:
   reset inicial y durante uso, bajo lock/quiescencia correcta,
   tiempos del bus, validación hardware y fallo/rollback.
6. Gates con C++ real, negativos y builds Arduino.
   Sin adopción ni upload automático.

```text
A13_G10_P1G_REVIEW=SOURCE_INVENTORY_ONLY
LOCAL_GUARD_HIGH_LEVEL_METHODS=19
LOW_LEVEL_DIRECT_METHODS_WITHOUT_LOCAL_GUARD=81
TRUE_UNPROTECTED_CALLGRAPH_COUNT=NOT_ESTABLISHED
BASIC_V2_RST_ETH_SCHEMATIC_GPIO=4
DEFAULT_JWPLC_ETH_RESET_PIN=255
DEFAULT_GPIO4_RESET=NOT_ENABLED_IN_REVIEWED_SOURCES
RESET_INTERLOCK_AND_SOCKET_EPOCH=NOT_IMPLEMENTED
ALPHA12_PERFORMANCE_CAUSE_MUTEX=NOT_PROVEN
PRODUCT_FILE_CHANGES=NONE
HARDWARE_ACCESS=NONE
```
