# Alpha12 — inventario final del package

Fecha: 2026-10-03

Rama canónica:

```text
v2.1.0-alpha.12/feature/modbus-tcp
```

Este documento clasifica el estado real del package después del desarrollo
histórico realizado bajo la etiqueta Alpha14.

Clasificaciones:

```text
PUBLIC_PRODUCT   = API/configuración soportada para usuario.
INTERNAL_PRODUCT = implementación productiva interna, no API recomendada.
QUALIFICATION    = switch/hook disponible para gates/diagnóstico.
BENCHMARK_ONLY   = tooling/instrumentación que no forma parte del contrato.
DEFERRED         = decisión futura explícita.
```

## 1. Core / runtime

Estado: PRODUCT.

Se preserva el autoload normal de JWPLC Basic.

El system task mantiene servicios para:

- I/O;
- RTC;
- Ethernet;
- DataLog;
- Display.

No se retiró ningún periférico por rendimiento.

### Modbus TCP y autoload

`JWPLC_ModbusTCP` continúa siendo una librería opt-in: no se incluye
automáticamente en todo sketch.

Cuando la librería Server queda enlazada, define:

```cpp
jwplcModbusTCPLoopServiceCallback()
```

que sustituye el weak callback del core y ejecuta:

```cpp
JWPLC_ModbusTCP.task();
```

alrededor del ciclo de usuario.

Clasificación:

```text
MODBUS_TCP_GLOBAL_AUTOLOAD=NO
MODBUS_TCP_SERVER_LINKED_AUTOSERVICE=YES
MODBUS_TCP_CLIENT_EXPLICIT_TASK=YES
```

El Client conserva `task()/poll()` explícito.

## 2. JWPLC_ModbusTCP

Estado: PUBLIC_PRODUCT + INTERNAL_PRODUCT.

Nueva librería del package:

```text
JWPLC/2.1.0/libraries/JWPLC_ModbusTCP
version=0.1.0
```

### Server público

APIs principales verificadas en header:

```text
beginServer()
task()
poll()
serverEnabled()
serverReady()
clientConnected()
unitId()
port()
serverState()
setFrameTimeoutMs()
frameTimeoutMs()

setCoils()
getCoil()
setCoil()
setDiscreteInputs()
getDiscreteInput()
setHoldingRegisters()
getHoldingRegister()
setHoldingRegister()
setInputRegisters()
getInputRegister()

lastError()
stats()
resetStats()
printStatus()
```

FC soportadas y validadas:

```text
FC01
FC02
FC03
FC04
FC05
FC06
FC15
FC16
```

### Client público

Objeto:

```cpp
JWPLC_ModbusTCPClient
```

APIs verificadas:

```text
begin()
end()
task()
poll()

requestReadCoils()
requestReadDiscreteInputs()
requestReadHoldingRegisters()
requestReadInputRegisters()
requestWriteSingleCoil()
requestWriteSingleRegister()
requestWriteMultipleCoils()
requestWriteMultipleRegisters()

configured()
sessionConnected()
busy()
done()
succeeded()
state()
result()
exceptionCode()
transactionId()
serverIP()
serverPort()
unitId()
clearResult()
stats()
resetStats()
printStatus()
```

No se añaden wrappers Sync bloqueantes en Alpha12.

### Defaults Server

```text
PORT=502
UNIT_ID=1
MAX_ADU=260
FRAME_TIMEOUT_MS=1000
RX_BUDGET=64
```

### Scheduler RX experimental

Los flags existen para qualification, pero los defaults finales son:

```text
JWPLC_MODBUS_TCP_INT_GUIDED_RX=0
JWPLC_MODBUS_TCP_INT_HOT_POLL_US=0
JWPLC_MODBUS_TCP_INT_LOAD_ADAPTIVE=0
JWPLC_MODBUS_TCP_INT_RSR_DRAIN=0
```

Decisión:

```text
TCP_RX_POLICY=POLLING_C0
INT_POLICY_PRODUCTIZED_FOR_BASIC_V2=NO
```

Los hooks de profiling permanecen OFF por defecto.

## 3. JWPLC_Ethernet / W5500

Estado: PUBLIC_PRODUCT + INTERNAL_PRODUCT.

### Runtime público existente

Se conserva la API de:

- DHCP cooperativo;
- IP estática;
- estado de hardware/link;
- errores/diagnóstico;
- autoload mediante `JWPLC_Ethernet.service()`.

Se añadió/reforzó refresh L2 best-effort:

```text
JWPLC_ETH_L2_REFRESH_PERIOD_MS=120000
JWPLC_ETH_L2_REFRESH_UDP_PORT=9
```

### Extensiones cooperativas EthernetClient

Producto aditivo:

```text
beginConnectAsync()
pollConnectAsync()
connectAsyncInProgress()
cancelConnectAsync()

beginStopAsync()
pollStopAsync()
stopAsyncInProgress()
cancelStopAsync()

beginFlushAsync()
pollFlushAsync()
flushAsyncInProgress()
cancelFlushAsync()

beginWriteAsync()
pollWriteAsync()
writeAsyncInProgress()
cancelWriteAsync()
```

Estas extensiones permiten construir motores cooperativos sin cambiar la
semántica de las APIs Arduino legacy.

### UDP cooperativo

Extensión aditiva:

```text
beginEndPacketAsync()
pollEndPacketAsync()
endPacketAsyncInProgress()
cancelEndPacketAsync()
```

### Fast RX aditivo/interno

UDP:

```text
jwplcReadPacketFastDeferred()
jwplcCommitRxFast()
```

TCP:

```text
jwplcReadTcpFastDeferred()
jwplcCommitRxFast()
```

Decisión de compatibilidad:

```text
LEGACY_PARSE_PACKET_READ_UNCHANGED=YES
FAST_PATH_TRANSPARENT_REPLACEMENT=NO
FAST_PATH_ADDITIVE=YES
```

### W5500

Perfil actual:

```text
SPI_ETHERNET_HZ=26000000
JWPLC_W5500_RX_FIFO_REUSE=1
JWPLC_W5500_RX_DIRECT_TRANSFER_BYTES=0
```

DIRECT RX no queda habilitado globalmente.

## 4. SPI compartido

Estado: INTERNAL_PRODUCT.

Defaults verificados:

```text
JWPLC_SPI_FIFO_REUSE_DLEN_CACHE=1
JWPLC_SPI_FIFO_REUSE_COPY_OUT_64=1
JWPLC_SPI_PROFILE_FIFO_REUSE_CHUNKS=0
```

Por tanto:

```text
DLEN_CACHE=ON
COPY_OUT_64=ON
PROFILE_HOOKS=OFF
```

Son optimizaciones internas; no añaden obligación a sketches Arduino.

## 5. JWPLC_ModbusRTU

Estado: PUBLIC_PRODUCT + QUALIFICATION controls.

### Defaults reales de librería

Header:

```text
DEFAULT_BAUD=19200
DEFAULT_CONFIG=SERIAL_8E1
DEFAULT_SLAVE_ID=1
MAX_FRAME=256
```

Constructor/runtime:

```text
FRAME_GAP_US=5000
MOTOR=ASYNC
QUEUED_TX_ENABLED=true
BULK_RX_ENABLED=false
EARLY_SERVER_DISPATCH=false
```

Importante:

- 115200 y 500000 baud fueron perfiles explícitos de qualification;
- no sustituyen automáticamente el default público 19200/8E1.

### Motor unificado

```cpp
JWPLC_ModbusRTU.motor(ASYNC); // default/recomendado
JWPLC_ModbusRTU.motor(SYNC);  // compatibilidad/commissioning
```

Semántica:

- ASYNC: `true` = request aceptado/iniciado;
- SYNC: `true` = transacción terminada correctamente.

### Timing

API pública aditiva:

```text
setFrameGapUs()
frameGapUs()
```

Se preservan:

```text
setFrameGapMs()
frameGapMs()
```

### TX queued

APIs:

```text
setQueuedTxEnabled()
queuedTxEnabled()
queuedTxActive()
```

El motor ASYNC usa la ruta queued cuando el transporte/hardware la soporta.

### Qualification controls

Existen:

```text
setBulkRxEnabled()
bulkRxEnabled()
setEarlyServerDispatchEnabled()
earlyServerDispatchEnabled()
setCrcLookupEnabled()
crcLookupEnabled()
```

No todos estos switches deben presentarse como configuración normal recomendada
al usuario. README debe distinguir qualification/advanced.

## 6. JWPLC_RS485

Estado: PUBLIC_PRODUCT.

JWPLC Basic v2 declara:

```text
JWPLC_RS485_AUTO_DIRECTION=1
JWPLC_RS485_TX_BUFFER_SIZE=512
```

por el MAX13487E con AutoDirection.

API aditiva verificada:

```text
effectiveBaudRate()
autoDirection()
apbClockForced()
clockSourceString()
txBufferSize()
queuedWriteSupported()
readAvailable()
writeQueued()
```

La API histórica `write()` conserva semántica bloqueante/flush.

`writeQueued()` es la ruta cooperativa aditiva usada por RTU ASYNC cuando el
hardware lo permite.

## 7. JW_SD / DataLog

Estado: PUBLIC_PRODUCT + CORE_AUTOSERVICE.

Alpha12 añadió un DataLog de alto nivel con buffer RAM.

Tipos/APIs principales:

```text
JW_SDDataLogConfig
JW_SDDataLogStatus
JWPLCDataLog

JWPLCDataLog.begin()
write()
writeLine()
service()
commit()
close()
isActive()
pendingBytes()
freeBytes()
commitThreshold()
commitTimeout()
acceptedWrites()
acceptedBytes()
committedBytes()
commitCount()
failedCommits()
status()
```

Manager:

```text
JW_SD::serviceDataLogs()
JW_SD::activeDataLogs()
MAX_DATALOGS=4
```

El core incluye `jwplcDataLogTickCallback()`; el system task ejecuta servicio
del DataLog de forma periódica.

Clasificación:

```text
DATALOG_BUFFERED_API=PRODUCT
DATALOG_SYSTEM_AUTOSERVICE=PRODUCT
FULL_RUNTIME_DATALOG_PHYSICAL=PASS
```

El README actual de JW_SD debe actualizarse para esta API real y no mezclar
texto futuro Alpha31.

## 8. JWPLC_Display / JWPLC_TFT

Estado: INTERNAL_PRODUCT + PUBLIC_COMPATIBILITY.

Durante el desarrollo histórico se realizó una modificación importante de la
arquitectura gráfica:

- `JWPLC_TFT` existe como backend propio;
- `JWPLC_Display` se apoya en `JWPLC_TFT`;
- el system task usa dirty/periodic refresh;
- el backend fue requalificado bajo full runtime;
- se preserva la API de alto nivel de Display/HMI.

Alpha12 no debe venderse como el alpha de features TFT. La documentación de
Alpha12 sólo debe reflejar los cambios arquitectónicos necesarios para
coexistencia/runtime y compatibilidad.

La ampliación/limpieza funcional de TFT queda para Alpha13.

## 9. Cambios benchmark-only / no promocionados

No deben aparecer como defaults de usuario:

```text
TCP INT scheduler variants D1/D2/D3/E1
UDP serial-IDLE/quiescence barriers
profiling detallado de hot path
tail-accounting/freeze de benchmark
single-CS UDP experiment
host UDP pacing harness
scan-paced benchmark scheduler
instrumentación H3E/H4 de diagnóstico
```

## 10. Decisiones diferidas

```text
OPENPLC_RUNTIME_AUTOLOAD=NO
OTA=NOT_DEFINED
FINAL_UNIVERSAL_FLASH_CONFIGURATION=PENDING
BOOTLOADER_BIN_FINAL=NO
TCP_INT_REEVALUATE_ON_BASIC_V3=YES
ALPHA13_TFT_SCOPE=REAUDIT_AFTER_ALPHA12
ALPHA14_OPENPLC_SCOPE=REAUDIT_AFTER_ALPHA13
```

## 11. Reglas para documentación Alpha12

README debe diferenciar siempre:

1. default real de librería;
2. perfil de benchmark;
3. API pública recomendada;
4. API avanzada/qualification;
5. implementación interna;
6. feature diferida.

No convertir cifras de benchmark en defaults de API.
No presentar paths internos de performance como reemplazo de APIs legacy.
