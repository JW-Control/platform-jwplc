# Alpha12 — Resultado de reestructuración de README JWPLC

Fecha: 2026-10-04

## Objetivo

Reestructurar los README de las librerías `JWPLC_*` pensando primero en una
persona que acaba de comprar un JWPLC Basic y está aprendiendo Arduino IDE.

La documentación debe enseñar primero el camino recomendado de producto y dejar
compatibilidad, APIs manuales y detalles avanzados fuera del flujo inicial.

## README reestructurados

| Librería | Versión | Resultado |
|---|---:|---|
| `JWPLC_GlobalPeripherals` | 1.0.0 | PASS |
| `JWPLC_RS485` | 1.0.1 | PASS |
| `JWPLC_Ethernet` | 1.0.0 | PASS |
| `JWPLC_ModbusRTU` | 1.0.0 | PASS |
| `JWPLC_ModbusTCP` | 0.1.0 | PASS con inconsistencia de Client reportada |
| `JWPLC_Display` | 1.0.1 | PASS |
| `JWPLC_TFT` | 0.1.0 | PASS |
| `JWPLC_LogicRuntime` | 0.2.0 | PASS como avanzado/experimental |
| `JWPLC_LogicRuntime_UI` | 0.5.8 | PASS como avanzado/experimental |

## Estructura documental

Validación estática final:

```text
README_COUNT=9
SINGLE_H1=9/9
STANDARD_SECTIONS=9/9
EXAMPLE_1=9/9
EXAMPLE_2=9/9
EXAMPLE_3=9/9
EXAMPLE_4=9/9
VERSION_MATCH_LIBRARY_PROPERTIES=9/9
INTERNAL_SHA_ARCHIVE_HARDENING_GATE_JARGON=0
```

Se comprobó que los README incluyen:

```text
¿Para qué sirve?
Qué hace automáticamente el JWPLC
Inicio rápido
Conceptos básicos
Ejemplo 1
Ejemplo 2
Ejemplo 3
Ejemplo 4
API de usuario
Errores comunes
API avanzada
Compatibilidad
Versión
```

## Criterio pedagógico por librería

### JWPLC_GlobalPeripherals

Camino principal:

```text
botones
E/S
JWPLC_IO
JWPLC_Time
objetos globales
```

La configuración física de `JW_MatrixButtons` no se enseña como API normal.

No se recomienda llamar manualmente:

```text
begin()
update()
startTask()
setScanDelays()
setRepeatProfile()
```

sobre la matriz que ya administra el JWPLC Basic.

### JWPLC_RS485

Camino principal:

```text
begin()
available()
read()
write()/print()/println()
diagnóstico
```

La transmisión encolada y acceso al `Stream` quedan como avanzados.

No se enseña control manual DE/RE ni reconfiguración directa de la UART.

### JWPLC_Ethernet

Camino principal:

```text
DHCP / IP estática
isReady()
EthernetClient
EthernetServer
EthernetUDP
```

No se enseña `Ethernet.begin()` como inicialización normal del producto.

Las funciones de transporte internas de bajo nivel no se incluyen en la guía de
usuario.

### JWPLC_ModbusRTU

Camino recomendado Master:

```text
begin(...)
motor(ASYNC)
task()
read...()/write...()
masterDone()
masterSucceeded()
clearMasterResult()
```

`SYNC` y las familias `request...()` / `...Sync()` se mantienen en
avanzado/compatibilidad.

### JWPLC_ModbusTCP

Server documentado con:

```text
FC01 FC02 FC03 FC04 FC05 FC06 FC15 FC16
```

Client documentado únicamente con las operaciones que tienen implementación
actual:

```text
FC03 requestReadHoldingRegisters()
FC06 requestWriteSingleRegister()
```

Las seis declaraciones Client sin implementación no se presentan como
capacidad de producto.

### JWPLC_Display

Corrección principal:

```text
JWPLC HMI Designer
        ↓
JWPLC_HMI_Generated.h
        ↓
variables HMI generadas
        ↓
el sketch modifica variables
        ↓
jwplcUIUpdate() automático
        ↓
TFT
```

Flujo recomendado:

```cpp
#include <JWPLC_Display.h>
#include <JWPLC_HMI_Generated.h>

void setup()
{
    jwplcHMISetup();
}

void loop()
{
    // lógica de aplicación
    // modificar variables generadas
}
```

No se enseña:

```cpp
jwplcUIUpdate();
```

dentro de `loop()`.

Tampoco se enseña `setFields()/setValue()/setBool()/setText()/setBar()` como
requisito inicial cuando la HMI viene del Designer.

Esas funciones quedan en:

```text
API avanzada -> HMI manual
```

También se deja explícito:

```text
NO_EDITAR_JWPLC_HMI_GENERATED_H=YES
```

### JWPLC_TFT

Se enseña como API de dibujo directo avanzada.

El autoload normal inicializa la TFT antes de `setup()`, por lo que los
ejemplos ya no llaman:

```cpp
JWPLC_TFT.begin();
```

`begin(timeoutMs)` queda sólo para integración aislada/compatibilidad.

### JWPLC_LogicRuntime

Se documenta como avanzado.

Se deja explícito:

```text
NORMAL_ARDUINO_REQUIRES_LOGIC_RUNTIME=NO
OPENPLC_INTEGRATED_BY_THIS_LIBRARY=NO
V2_ENGINE=EXPERIMENTAL_RAM_ONLY
```

La API v2 nueva se enseña mediante aliases:

```text
JWPLCLogicV2::Engine
JWPLCLogicV2::Program
JWPLCLogicV2::BlockRecord
JWPLCLogicV2::InputLink
JWPLCLogicV2::BlockType
```

Los nombres `Prototype` quedan en compatibilidad.

### JWPLC_LogicRuntime_UI

Se documenta como UI avanzada/experimental para LogicRuntime.

No se presenta como HMI general del JWPLC.

`JWPLC_Display` + HMI Designer sigue siendo el camino normal para una HMI de
aplicación.

## Verificación HMI Designer

Se verificó en la rama actual que el pipeline final de HMI Designer genera:

```text
JWPLC_HMI_Generated.h
HMIPageId
HMIFieldId
variables HMI
JWPLC_UIField[]
PixelMaps cuando corresponda
jwplcHMISetup()
jwplcUIUpdate()
```

El generador final construye `jwplcUIUpdate()` con los setters correspondientes
por página.

También se verificó en `JWPLC_Display.cpp` que el runtime ejecuta:

```cpp
jwplcUIUpdate();
```

durante el refresco USER.

Por tanto:

```text
USER_CALLS_JWPLC_UI_UPDATE_FROM_LOOP=NO
RUNTIME_CALLS_JWPLC_UI_UPDATE=YES
DESIGNER_VARIABLES_ARE_PRIMARY_USER_BRIDGE=YES
```

## Validación de enums y símbolos

Se compararon tokens personalizados usados por los README contra headers de la
rama.

Verificados, entre otros:

```text
BTN_*
IDLE_WAKE_*
IDLE_RETURN_*
USER_REFRESH_*
JWPLC_TFT_*
JWPLCLogicRuntimeState::*
JWPLCLogicV2::BlockType::*
LogicBlockType::*
```

Resultado:

```text
UNKNOWN_CUSTOM_ENUM_OR_CONSTANT_IN_READMES=0
```

## Cobertura de API pública

Cruce automático de métodos públicos principales:

```text
JWPLC_RS485=PASS
JWPLC_ModbusRTU=PASS
JWPLC_ModbusTCP_Server=PASS
JWPLC_Display=PASS
JWPLC_TFT=PASS
JWPLC_LogicRuntime=PASS
```

### Excepción conocida: Modbus TCP Client

Siguen declaradas en el header, pero no implementadas en el `.cpp` actual:

```text
requestReadCoils()
requestReadDiscreteInputs()
requestReadInputRegisters()
requestWriteSingleCoil()
requestWriteMultipleCoils()
requestWriteMultipleRegisters()
```

Clasificación:

```text
HEADER_IMPLEMENTATION_MISMATCH=YES
DOC_TASK_CHANGED_FIRMWARE=NO
README_TEACHES_MISSING_FUNCTIONS=NO
```

La inconsistencia debe resolverse en una tarea de firmware separada.

### APIs públicas deliberadamente no enseñadas como camino normal

`JW_MatrixButtons` contiene configuración pública para uso genérico de la
librería, pero el JWPLC Basic ya configura su botonera.

Por tanto, métodos de configuración física/task/repeat no se presentan como
flujo normal del producto.

No se considera una API faltante del tutorial; se clasifica como:

```text
PUBLIC_LIBRARY_API_BUT_NOT_NORMAL_JWPLC_PRODUCT_API
```

## Ejemplos y buenas prácticas

La revisión final exige:

```text
NO_REQUIRED_DELAY_FOR_NORMAL_FLOW=YES
MILLIS_FOR_PERIODIC_ACTIONS=YES
AUTOLOAD_RESPECTED=YES
NO_REINIT_MANAGED_TFT=YES
NO_REINIT_ETHERNET=YES
NO_DIRECT_INTERNAL_PINS=YES
HMI_DESIGNER_UPDATE_DUPLICATION=NO
```

Los README mencionan `delay()` únicamente para explicar por qué debe evitarse
en esos casos, no como requisito del flujo recomendado.

## Alcance de la validación

Se verificó estáticamente:

- existencia de símbolos;
- firmas y nombres públicos;
- enums y constantes;
- semántica ambigua contra implementación;
- flujo HMI Designer contra su codegen;
- autoload TFT contra inicialización real;
- estructura Markdown;
- un único H1;
- progresión de ejemplos;
- separación usuario/avanzado/compatibilidad.

Esta tarea no ejecutó un gate Arduino CLI que extraiga y compile cada bloque
```cpp``` del Markdown como sketch independiente.

Los ejemplos se construyeron a partir de APIs reales y ejemplos existentes de la
rama, pero una validación automática de compilación de todos los snippets puede
añadirse como gate documental separado si se decide exigirla para release.

## Resultado

```text
ALPHA12_LIBRARY_README_RESTRUCTURE=PASS_STATIC
README_COUNT=9
PRODUCT_API_CHANGED=NO
FIRMWARE_CHANGED=NO
DOCUMENTATION_ONLY=YES

KNOWN_CODE_INCONSISTENCY:
MODBUS_TCP_CLIENT_HEADER_IMPLEMENTATION_MISMATCH

DISPLAY_HMI_DESIGNER_FLOW=PASS_VERIFIED
TFT_AUTOLOAD_DOCUMENTATION=PASS
```
