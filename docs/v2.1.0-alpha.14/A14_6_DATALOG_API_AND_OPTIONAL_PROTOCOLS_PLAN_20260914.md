# Alpha14.6 — DataLog de alto nivel y cierre de comunicaciones precompiladas

Fecha: 2026-09-14

## Estado

Alpha14 permanece reabierta antes del merge del PR #99 para completar el alcance de producto que quedó pendiente después de las validaciones funcionales de Modbus TCP y rendimiento:

1. integrar realmente en `JW_SD` una capa DataLog de alto nivel;
2. cerrar la experiencia de configuración de comunicaciones desde `setup()`;
3. ejecutar al final el benchmark de compilación y regenerar los precompilados que correspondan.

```text
ALPHA14_TECHNICAL_CLOSURE=REOPENED
PR99=DRAFT
A14_6=IN_PROGRESS
```

---

## 1. Política final de comunicaciones

### 1.1 Ethernet y RS-485

Se conserva el comportamiento vigente del package:

```text
JWPLC_Ethernet=AUTOLOAD_KEEP
JWPLC_RS485=AUTOLOAD_KEEP
AUTOLOAD_PERIPHERALS_REMOVED=NO
```

No se retiran del runtime normal.

El usuario no debería tener que reconstruir manualmente la capa física de transporte para usar Modbus.

### 1.2 Modbus RTU y Modbus TCP

Modbus RTU y Modbus TCP no deben iniciar una sesión/protocolo por el solo hecho de existir en el package.

La configuración funcional se realiza explícitamente desde `setup()` mediante sus APIs públicas.

Ejemplos conceptuales:

```cpp
JWPLC_ModbusRTU.begin(slaveId, baud, config);
```

```cpp
JWPLC_ModbusTCP.beginServer(unitId, port);
```

El transporte subyacente queda integrado:

- `JWPLC_ModbusRTU.h` incluye `JWPLC_RS485.h`;
- `JWPLC_ModbusRTU/library.properties` declara `depends=JWPLC_RS485`;
- `JWPLC_ModbusTCP.h` incluye `JWPLC_Ethernet.h`;
- `JWPLC_ModbusTCP/library.properties` declara `depends=JWPLC_Ethernet`.

El usuario no debe necesitar configurar dos pilas independientes para un mismo protocolo.

### 1.3 Precompilados

Se descarta la decisión previa de convertir RTU/TCP a `source-only`.

Un archive precompilado no significa que el protocolo quede activo automáticamente; únicamente evita recompilar sus unidades fuente cuando Arduino Builder necesita esa librería.

Por tanto, Alpha14 evaluará como objetivo final:

```text
JWPLC_RS485=PRECOMPILED_CANDIDATE
JWPLC_Ethernet=PRECOMPILED_CANDIDATE
JWPLC_ModbusRTU=PRECOMPILED_KEEP_AND_REVALIDATE
JWPLC_ModbusTCP=PRECOMPILED_CANDIDATE
```

La adopción final de cada `.a` queda condicionada a:

- paridad source/archive;
- mismo comportamiento físico;
- compatibilidad Basic/Core;
- ausencia de dependencia en macros del sketch que deban recompilar implementación;
- conteo correcto de TUs;
- símbolos requeridos presentes;
- benchmark cold/warm/warm-touch;
- ausencia de regresión material de flash/RAM.

`JWPLC_ModbusRTU` ya usa `precompiled=full` y se conserva así hasta el gate final; no se debe borrar su archive actual en esta etapa.

### 1.4 Configuración runtime antes que macros de usuario

Para que una librería precompilada siga siendo configurable, los parámetros que formen parte de la experiencia normal deben resolverse mediante API runtime (`begin()`, setters o estructuras de configuración), no dependiendo de recompilar `.cpp` con `#define` del sketch.

Alpha14 revisará especialmente:

- baud/config/slave ID RTU;
- timeout RTU;
- Unit ID/port TCP;
- frame timeout TCP;
- parámetros cooperativos que deban exponerse al usuario.

Los límites internos que formen parte del ABI/protocolo pueden permanecer constantes si no necesitan personalización normal.

### 1.5 Exposición global

Se conserva el autoload/global actual mientras se completa el benchmark final.

`JWPLC_ModbusRTU` ya está expuesto indirectamente por `JWPLC_GlobalPeripherals.h`.

Para `JWPLC_ModbusTCP`, se evaluará al final si conviene exponer también su header desde la API global para permitir una experiencia de uso basada únicamente en configuración desde `setup()`.

La decisión dependerá del impacto real de library discovery/build medido con la versión precompilada final.

---

## 2. Objetivo DataLog

La arquitectura DataLog definida durante A14.3 debe quedar implementada dentro de `JW_SD`.

No se acepta como solución final obligar al usuario a copiar manualmente una secuencia de:

- `open()` persistente;
- ring buffer;
- conteo de bytes/registros;
- política de `flush()`;
- timeout;
- reapertura/reintento.

La API de bajo nivel actual se conserva sin cambios para compatibilidad:

- `JW_SD`;
- `JWPLCFile`;
- `open()`;
- `openNative()`;
- `write()`;
- `read()`;
- `flush()`;
- `close()`.

Encima de ella se añadirá una capa de alto nivel orientada a DataLog/PLC.

---

## 3. DataLog Alpha14: RAM + microSD

### 3.1 FRAM diferida

Se descarta el journal FRAM como requisito de Alpha14.

Razones:

1. el JWPLC Basic v2.0 actual dispone de 8 KB de FRAM;
2. no existe todavía un memory map global reservado para servicios internos;
3. asignar silenciosamente una zona puede colisionar con datos del usuario;
4. la futura variante con FRAM de 32 KB permitirá definir correctamente particiones/ownership.

Decisión:

```text
DATALOG_FRAM_JOURNAL=DEFERRED
DATALOG_FRAM_BLOCKER_ALPHA14=NO
DATALOG_STORAGE_PIPELINE=RAM_RING_TO_SD
```

La futura versión con FRAM mayor deberá definir primero un mapa de memoria explícito antes de añadir journal persistente.

### 3.2 Ring buffer RAM

Candidato inicial:

```text
RAM_RING_TARGET=4096 bytes
```

Requisitos:

- sólo reservar memoria cuando DataLog se utilice;
- tamaño configurable;
- sin allocation por registro durante operación normal;
- índices head/tail y pending bytes;
- soporte de wrap-around;
- rechazo controlado cuando no exista espacio suficiente.

Política de overflow por defecto:

```text
OVERFLOW_POLICY=REJECT_NEW_RECORD
SILENT_OVERWRITE=NO
```

El usuario debe poder detectar el error y el contador de registros rechazados.

---

## 4. Política de persistencia

Las pruebas A14.3 demostraron que el costo dominante estaba en `open()`/metadata y `flush()`, no en transferir registros pequeños.

Referencia validada a 20 MHz:

```text
OPEN_AVG_US≈6750
WRITE_32B_AVG_US≈23..30
FLUSH_EVERY_RECORD_AMORTIZED≈2151 us/record
FLUSH_EVERY_5_RECORDS_AMORTIZED≈565 us/record
FLUSH_EVERY_10_RECORDS_AMORTIZED≈362 us/record
SD_DEFAULT_FREQUENCY=20 MHz
```

La capa DataLog usará:

```text
FILE_POLICY=PERSISTENT_OPEN
COMMIT_TRIGGER=BUFFER_THRESHOLD_OR_TIMEOUT
```

Es decir:

1. `DataLogWrite()` acepta el registro en RAM;
2. no fuerza una escritura física inmediata;
3. el servicio automático observa bytes pendientes;
4. si se alcanza el umbral configurado, escribe/flush a SD;
5. si el tráfico es bajo, el timeout máximo fuerza igualmente el commit.

El valor final del umbral se elegirá por benchmark físico entre candidatos como 160/512/1024 bytes.

Timeout inicial de referencia:

```text
COMMIT_TIMEOUT_CANDIDATE_MS=5000
```

El timeout y threshold serán configurables.

---

## 5. Semántica de estados sin FRAM

Al no existir journal no volátil en Alpha14, se evita fingir persistencia antes de que la SD confirme el batch.

Estados conceptuales:

```text
ACCEPTED  = registro aceptado en RAM
PENDING   = existen bytes aún no confirmados en SD
BUSY      = DataLog tiene trabajo pendiente
COMMITTED = registro/batch confirmado en microSD
DONE      = operación solicitada alcanzó COMMITTED
ERROR     = fallo de enqueue o persistencia
```

En Alpha14:

```text
DURABLE == COMMITTED
```

Un registro únicamente en RAM no se reporta como `DONE` durable.

Consecuencia explícita:

- un reset o pérdida de energía puede perder los registros todavía pendientes en RAM;
- la ventana máxima normal queda limitada por threshold/timeout;
- esta limitación debe documentarse en README.

---

## 6. API objetivo

Los conceptos visibles para el usuario serán equivalentes a:

```text
DataLogCreate
DataLogWrite
DataLogStatus
DataLogCommit
DataLogClose
```

La forma C++ definitiva se diseñará manteniendo nombres claros y compatibles con Arduino.

Objetivo de uso:

```cpp
void setup()
{
    // Crear/configurar una vez.
    JWPLC_SD.dataLogCreate("/proceso.csv");
}

void loop()
{
    // Entregar datos; no decidir cuándo se escribe físicamente la SD.
    JWPLC_SD.dataLogWrite(...);
}
```

La API debe soportar al menos:

- bytes/binario;
- texto/CSV sencillo;
- consulta de pendientes;
- último error;
- contador de registros aceptados;
- contador de registros committed;
- contador de overflow/rechazos;
- commit manual opcional;
- cierre ordenado.

---

## 7. Servicio automático

El usuario normal no debe tener que llamar `serviceDataLog()` dentro de `loop()`.

Dirección de diseño:

```text
jwplcSystemTask
  -> callback/servicio JWPLC_SD DataLog
```

Se reutiliza el scheduler cooperativo ya existente en lugar de crear una tarea FreeRTOS privada adicional para SD.

Requisitos:

- coste prácticamente nulo cuando DataLog está inactivo;
- no monopolizar SPI;
- respetar mutex/arbitraje existente;
- procesar trabajo limitado por pasada;
- mantener vivos Display, Ethernet, FRAM, RTC, botones e I/O.

Si esta integración modifica unidades incluidas en `core.a`, el core precompilado deberá regenerarse y revalidarse al final.

---

## 8. Bulk read de JWPLCFile

Se mantiene como mejora válida de Alpha14:

```cpp
size_t read(uint8_t *buffer, size_t size);
```

Debe usar la misma protección de bus que el resto de operaciones envueltas por `JWPLCFile`.

No es el mecanismo central de DataLog, pero completa la simetría de API y evita lecturas byte-a-byte innecesarias.

---

## 9. Fallos mínimos a manejar

Alpha14 debe validar:

- SD ausente al crear log;
- SD retirada durante operación;
- fallo de lock SPI;
- ring buffer lleno;
- fallo de write/flush;
- reapertura después de reinserción;
- cierre explícito con datos pendientes;
- `commit()` manual;
- timeout automático;
- threshold automático;
- reset/power-loss documentado como pérdida posible de pending RAM, sin prometer replay.

Mientras la SD esté temporalmente indisponible, la librería puede conservar en RAM los registros ya aceptados mientras exista capacidad; no debe reportarlos como committed.

---

## 10. Compatibilidad

```text
LOW_LEVEL_JW_SD_API=KEEP
PUBLIC_BREAKING_CHANGE=NO
AUTOLOAD_PERIPHERALS_REMOVED=NO
JW_SD_STANDALONE_USE=KEEP
DATALOG_HIGH_LEVEL_API=ADDITIVE
```

---

## 11. Precompilados finales

`JW_SD` ya utiliza `precompiled=full`; como su source cambiará, `libJW_SD.a` deberá regenerarse y pasar paridad completa.

Las comunicaciones se evaluarán con objetivo precompilado, no `source-only`.

Gate final esperado:

```text
JW_SD_PRECOMPILED_PARITY=PASS
JWPLC_MODBUS_RTU_PRECOMPILED_PARITY=PASS
JWPLC_MODBUS_TCP_PRECOMPILED_PARITY=PASS_IF_ADOPTED
JWPLC_RS485_PRECOMPILED_PARITY=PASS_IF_ADOPTED
JWPLC_ETHERNET_PRECOMPILED_PARITY=PASS_IF_ADOPTED
CORE_PRECOMPILED_PARITY=PASS_IF_REBUILT
```

La adopción de nuevos archives para Ethernet/RS485/TCP se decidirá por benchmark y compatibilidad de variantes, no sólo por intuición.

---

## 12. Gates obligatorios

### G1 — API DataLog RAM

- ring buffer 4096 B configurable;
- enqueue sin I/O SD inmediato;
- archivo persistente;
- threshold/timeout configurables;
- commit manual;
- estados ACCEPTED/PENDING/COMMITTED/ERROR;
- overflow controlado;
- bulk read;
- compile source Basic/Core.

### G2 — servicio automático

- integrar DataLog en runtime cooperativo;
- usuario no llama `task()`;
- medir coste idle;
- medir coste activo;
- regenerar `core.a` si corresponde.

### G3 — política física de commit

- comparar threshold 160/512/1024 B;
- timeout de referencia 5 s;
- elegir defaults finales;
- validar handle persistente;
- validar SD remove/reinsert;
- validar buffer lleno;
- validar commit/close.

### G4 — integración full runtime

- DataLog frecuente + Modbus TCP;
- RTU/TCP simultáneo si el cambio de runtime lo justifica;
- Display/HMI activo;
- FRAM/RTC/I/O/botones activos;
- cero corrupción;
- cero fallos periféricos.

### G5 — documentación y ejemplos

Actualizar:

- `JW_SD/README.md`;
- `JW_SD/CHANGELOG.md`;
- ejemplo oficial DataLog;
- `JWPLC_ModbusTCP/README.md`;
- `JWPLC_ModbusTCP/library.properties`;
- README/configuración de RTU/RS485/Ethernet si corresponde;
- explicar claramente autoload vs `begin()` del protocolo.

### G6 — benchmark/precompilados final

- cold/warm/warm-touch;
- Basic + Core;
- empty + I/O + DataLog + RTU + TCP;
- conteo de TUs;
- flash/RAM;
- regenerar `libJW_SD.a`;
- regenerar `core.a` si fue modificado;
- evaluar/generar archives de RS485/Ethernet/ModbusTCP;
- revalidar archive RTU;
- paridad source/archive;
- elegir exposición global final de ModbusTCP según impacto medido.

---

## 13. Criterio de cierre Alpha14

Alpha14 no vuelve a `READY_FOR_RELEASE_PR` hasta cumplir:

```text
DATALOG_HIGH_LEVEL_API=PASS
DATALOG_RAM_RING=PASS
DATALOG_AUTO_COMMIT=PASS
DATALOG_THRESHOLD_OR_TIMEOUT=PASS
DATALOG_RUNTIME_SERVICE=PASS
DATALOG_SD_RECOVERY=PASS
DATALOG_FRAM_JOURNAL=DEFERRED_NON_BLOCKING
JW_SD_PRECOMPILED_PARITY=PASS
COMMUNICATION_PRECOMPILED_POLICY=PASS
MODBUS_DEPENDENCY_AUTO_INCLUDE=PASS
MODBUS_TCP_README=PASS
JW_SD_README=PASS
FINAL_BUILD_BENCHMARK=PASS
CI=PASS
```

A14.5 Robot / interoperabilidad continúa `DEFERRED_NON_BLOCKING` y no bloquea este cierre.
