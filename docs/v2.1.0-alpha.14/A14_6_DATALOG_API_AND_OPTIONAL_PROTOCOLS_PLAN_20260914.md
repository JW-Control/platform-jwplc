# Alpha14.6 — DataLog de alto nivel y política de protocolos opt-in

Fecha: 2026-09-14

## Estado

Alpha14 se reabre antes del merge del PR #99 para completar dos objetivos que forman parte del alcance real del alpha:

1. integrar en `JW_SD` la arquitectura DataLog de alto nivel definida durante A14.3;
2. cerrar la política de compilación/dependencias de Modbus RTU/TCP como protocolos opt-in.

```text
ALPHA14_TECHNICAL_CLOSURE=REOPENED
PR99=DRAFT
A14_6=IN_PROGRESS
```

## 1. Política Modbus / transportes

### Decisión de usuario

Modbus RTU y Modbus TCP son capas de protocolo que el sketch decide utilizar o no.

Contrato deseado:

```cpp
#include <JWPLC_ModbusRTU.h>
```

debe ser suficiente para disponer de RS-485, y:

```cpp
#include <JWPLC_ModbusTCP.h>
```

debe ser suficiente para disponer de Ethernet.

El usuario no debe necesitar dobles includes para cada pareja protocolo/transporte.

### Estado actual

- `JWPLC_ModbusRTU.h` ya incluye `<JWPLC_RS485.h>`.
- `JWPLC_ModbusRTU/library.properties` ya declara `depends=JWPLC_RS485`.
- `JWPLC_ModbusTCP.h` ya incluye `<JWPLC_Ethernet.h>`.
- `JWPLC_ModbusTCP/library.properties` ya declara `depends=JWPLC_Ethernet`.

La ergonomía de dependencia ya existe.

### Correcciones pendientes

`JWPLC_ModbusRTU` todavía declara:

```text
precompiled=full
```

y contiene:

```text
src/esp32/libJWPLC_ModbusRTU.a
```

Por política final de Alpha14, Modbus RTU y Modbus TCP deben quedar **source-only**.

Además, `JWPLC_GlobalPeripherals.h` incluye actualmente `JWPLC_ModbusRTU.h`, por lo que la capa de protocolo RTU se descubre indirectamente en builds normales.

Decisión:

```text
MODBUS_RTU_PRECOMPILED=REMOVE
MODBUS_TCP_PRECOMPILED=NO
MODBUS_RTU_GLOBAL_HEADER_AUTO_INCLUDE=REMOVE
MODBUS_TCP_GLOBAL_HEADER_AUTO_INCLUDE=NO
MODBUS_RTU_DEPENDS_RS485=KEEP
MODBUS_TCP_DEPENDS_ETHERNET=KEEP
```

No se retira en Alpha14 el autoload existente de los transportes físicos Ethernet/RS-485. Esa sería una decisión de runtime más amplia y no es necesaria para cerrar la ergonomía de las APIs Modbus.

## 2. Objetivo DataLog

La arquitectura DataLog definida en A14.3 debe implementarse realmente dentro de `JW_SD`; no debe quedar como una secuencia que cada usuario copie a su sketch.

Se conserva sin cambios la API de bajo nivel existente:

- `JW_SD`
- `JWPLCFile`
- `open()`
- `openNative()`
- `write()`
- `read()`
- `flush()`
- `close()`

Encima de ella se añadirá una capa de alto nivel orientada a PLC/DataLog.

## 3. Principio de uso

El usuario entrega registros lógicos. La librería decide cuándo ocurre el I/O físico a microSD.

No se requerirá que el usuario implemente manualmente:

- ring buffer;
- batching;
- política de `flush`;
- handle persistente;
- reintentos;
- journal FRAM;
- recuperación de registros pendientes.

## 4. API objetivo

La API final debe exponer conceptos equivalentes a:

```text
DataLogCreate
DataLogWrite
DataLogStatus
DataLogCommit
DataLogClose
```

Los nombres C++ definitivos se fijarán durante implementación manteniendo estos conceptos visibles en README y ejemplos.

### Semántica PLC

```text
REQ       = nueva solicitud de registro
BUSY      = existen registros pendientes de completar
DONE      = registro solicitado alcanzó estado durable
ERROR     = la solicitud o persistencia falló
PENDING   = registros/bytes aún no migrados completamente a SD
COMMITTED = último registro confirmado físicamente en microSD
```

Se evita usar un único `DONE` ambiguo.

Definición:

- `ACCEPTED`: el registro fue aceptado en RAM.
- `DURABLE`: el registro existe en medio no volátil (FRAM journal o SD).
- `COMMITTED`: el registro ya fue confirmado en SD.

Cuando el journal FRAM está deshabilitado, `DURABLE` y `COMMITTED` coinciden.

## 5. Arquitectura interna

### RAM

Ring buffer principal:

```text
TARGET=4096 bytes
```

El tamaño debe ser configurable y asignarse al crear/inicializar el DataLog; no se reservarán 4 KB por logger no utilizado.

### FRAM

Journal opcional:

```text
MAX=1024 bytes
```

La FRAM actual del JWPLC Basic es de 8 KB.

No existe todavía un memory map global que permita reservar silenciosamente una zona fija sin riesgo de pisar datos del usuario. Por tanto, Alpha14 no debe hardcodear una dirección global oculta.

El journal se implementará como modo seguro opcional con región explícita/configurable. La API debe validar rango mediante `JW_FRAM::isAddressValid()`.

Una reserva global automática de FRAM requerirá una decisión futura de memory map del sistema.

### microSD

Política validada físicamente durante A14.3:

```text
SD_FREQUENCY=20 MHz
FILE_POLICY=PERSISTENT_OPEN
WRITE_SMALL_RECORD_COST≈23 us
FLUSH_EVERY_5_RECORDS_AMORTIZED≈565 us/record
FLUSH_EVERY_10_RECORDS_AMORTIZED≈362 us/record
REALISTIC_POLICY=FLUSH_EVERY_5_RECORDS
```

Alpha14 usará la política `flush/5` como referencia conservadora inicial para registros pequeños, manteniendo parámetros configurables.

Además del umbral por registros/bytes existirá un timeout máximo de commit para impedir que datos poco frecuentes permanezcan indefinidamente pendientes.

## 6. Servicio automático

El usuario no debe necesitar `serviceDataLog()` en su `loop()` para el uso normal del JWPLC Basic.

Se integrará un servicio cooperativo de DataLog dentro del runtime JWPLC.

Dirección propuesta:

```text
jwplcSystemTask
  -> JWPLC SD/DataLog service callback
```

No se creará una nueva tarea FreeRTOS privada dentro de `JW_SD`; se prefiere reutilizar el scheduler cooperativo existente y el arbitraje SPI ya validado.

La integración deberá ser barata cuando no exista ningún DataLog activo.

## 7. Bulk read

Se añadirá a `JWPLCFile` una lectura protegida de buffer equivalente a:

```cpp
size_t read(uint8_t *buffer, size_t size);
```

La API actual sólo protege `read()` byte a byte, mientras ya existe `write(buffer,size)`.

## 8. Manejo de fallo

Casos mínimos obligatorios:

- SD ausente al crear log;
- SD retirada durante operación;
- fallo de lock SPI;
- ring buffer lleno;
- journal FRAM lleno/no disponible;
- fallo de write/flush;
- reinicio con journal válido pendiente;
- reapertura del archivo tras recuperación;
- cierre explícito con datos pendientes.

La librería nunca debe reportar `DONE/DURABLE` si el nivel de persistencia configurado no se alcanzó.

## 9. Compatibilidad

```text
LOW_LEVEL_JW_SD_API=KEEP
PUBLIC_BREAKING_CHANGE=NO
AUTOLOAD_PERIPHERALS_REMOVED=NO
JW_SD_STANDALONE_USE=KEEP
```

La capa DataLog será adicional.

## 10. Precompilados

`JW_SD` actualmente usa `precompiled=full`. Como su source sí será modificado por A14.6, el archive `libJW_SD.a` deberá regenerarse y pasar paridad source/archive antes del cierre.

Política final prevista:

```text
JW_SD=PRECOMPILED_FULL_REBUILD_REQUIRED
JWPLC_MODBUS_RTU=SOURCE_ONLY
JWPLC_MODBUS_TCP=SOURCE_ONLY
JWPLC_RS485=SOURCE_ONLY
JWPLC_ETHERNET=SOURCE_ONLY
```

El core precompilado deberá regenerarse únicamente si la integración automática de `serviceDataLog()` modifica las unidades que forman `core.a`.

## 11. Gates obligatorios

### G1 — API base DataLog

- ring buffer RAM;
- handle SD persistente;
- enqueue sin escritura SD inmediata;
- flush/commit automático;
- timeout de commit;
- estados ACCEPTED/DURABLE/COMMITTED;
- bulk read;
- compile Basic/Core.

### G2 — FRAM journal

- journal opcional <=1024 B;
- dirección/rango configurable;
- persistencia y replay tras reset;
- no pisar FRAM ajena.

### G3 — runtime automático

- integrar servicio cooperativo en runtime;
- usuario no llama `task()`;
- medir coste idle y activo;
- regenerar `core.a` si corresponde.

### G4 — pruebas físicas

- DataLog frecuente con TCP activo;
- SD remove/reinsert;
- reset con journal pendiente;
- no corrupción;
- ring overflow controlado;
- cero fallos de otros periféricos.

### G5 — documentación y ejemplos

Actualizar:

- `JW_SD/README.md`;
- `JW_SD/CHANGELOG.md`;
- ejemplo oficial DataLog;
- `JWPLC_ModbusTCP/README.md`;
- `JWPLC_ModbusTCP/library.properties`;
- documentación de Modbus RTU source-only.

### G6 — build/precompiled final

- benchmark cold/warm/warm-touch;
- Basic + Core;
- ejemplos SD/DataLog;
- ejemplos Modbus RTU/TCP;
- conteo de TUs;
- regeneración y paridad de `libJW_SD.a`;
- regeneración/paridad de `core.a` si fue modificado;
- confirmar que RTU/TCP sólo se compilan al ser incluidos.

## 12. Criterio de cierre Alpha14

Alpha14 no volverá a `READY_FOR_RELEASE_PR` hasta cumplir:

```text
DATALOG_HIGH_LEVEL_API=PASS
DATALOG_RAM_RING=PASS
DATALOG_AUTO_COMMIT=PASS
DATALOG_FRAM_JOURNAL=PASS
DATALOG_RUNTIME_SERVICE=PASS
JW_SD_PRECOMPILED_PARITY=PASS
MODBUS_RTU_SOURCE_ONLY=PASS
MODBUS_TCP_SOURCE_ONLY=PASS
MODBUS_DEPENDENCY_AUTO_INCLUDE=PASS
MODBUS_TCP_README=PASS
JW_SD_README=PASS
FINAL_BUILD_BENCHMARK=PASS
CI=PASS
```

A14.5 Robot / interoperabilidad continúa `DEFERRED_NON_BLOCKING` y no bloquea este cierre.