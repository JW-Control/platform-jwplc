# Alpha12 — Auditoría para reestructuración de README de librerías JWPLC

Fecha: 2026-10-04

## Alcance

Se reestructuran los README principales de:

- JWPLC_GlobalPeripherals
- JWPLC_RS485
- JWPLC_Ethernet
- JWPLC_ModbusRTU
- JWPLC_ModbusTCP
- JWPLC_Display
- JWPLC_TFT
- JWPLC_LogicRuntime
- JWPLC_LogicRuntime_UI

No se modifica firmware, API ni comportamiento del package.

## Público objetivo

Usuario de JWPLC Basic que conoce setup(), loop(), variables, if, funciones y
tipos básicos de C/C++, pero que todavía está aprendiendo microcontroladores y
no debe manipular drivers, buses, pines internos, mutexes o backends.

## Niveles documentales

Cada README debe distinguir:

- Básico / recomendado
- Intermedio
- Avanzado de usuario
- Compatibilidad
- Interno / no contrato de usuario

Las APIs internas no deben mezclarse con el tutorial.

## Clasificación de librerías

| Librería | Rol documental |
|---|---|
| JWPLC_GlobalPeripherals | Básica. Punto de entrada a objetos globales, botones y vistas de runtime |
| JWPLC_RS485 | Básica/intermedia para transporte RS-485 |
| JWPLC_Ethernet | Básica/intermedia para Ethernet, TCP y UDP |
| JWPLC_ModbusRTU | Intermedia, con camino recomendado ASYNC |
| JWPLC_ModbusTCP | Intermedia, Server + Client |
| JWPLC_Display | Básica/recomendada para HMI |
| JWPLC_TFT | Avanzada de usuario para dibujo directo |
| JWPLC_LogicRuntime | Avanzada/experimental; no necesaria para Arduino normal |
| JWPLC_LogicRuntime_UI | Avanzada/experimental; sólo para LogicRuntime |

## Inconsistencias detectadas antes de reescribir

### DOC-API-001 — Modbus TCP Client declara funciones sin implementación

Header:

```text
JWPLC/2.1.0/libraries/JWPLC_ModbusTCP/src/JWPLC_ModbusTCP_Client.h
```

declara:

```text
requestReadCoils()
requestReadDiscreteInputs()
requestReadHoldingRegisters()
requestReadInputRegisters()
requestWriteSingleCoil()
requestWriteSingleRegister()
requestWriteMultipleCoils()
requestWriteMultipleRegisters()
```

En la implementación actual:

```text
JWPLC/2.1.0/libraries/JWPLC_ModbusTCP/src/JWPLC_ModbusTCP_Client.cpp
```

se encontraron implementadas:

```text
requestReadHoldingRegisters()   FC03
requestWriteSingleRegister()    FC06
```

y no se encontraron definiciones para:

```text
requestReadCoils()              FC01
requestReadDiscreteInputs()     FC02
requestReadInputRegisters()     FC04
requestWriteSingleCoil()        FC05
requestWriteMultipleCoils()     FC15
requestWriteMultipleRegisters() FC16
```

La implementación interna del Client tampoco contiene actualmente un camino
completo para esas seis operaciones.

Clasificación:

```text
HEADER_IMPLEMENTATION_MISMATCH=YES
FIRMWARE_CHANGED_BY_DOC_TASK=NO
README_MUST_NOT_TEACH_MISSING_CLIENT_FUNCTIONS=YES
```

El Server sí contiene handlers para:

```text
FC01 FC02 FC03 FC04 FC05 FC06 FC15 FC16
```

Por tanto, el nuevo README debe distinguir claramente capacidad Server y
capacidad Client real del branch.

### DOC-API-002 — IDs generados por HMI Designer no son símbolos universales

Nombres como:

```text
PAGE_ALARMAS
PIXELMAP_ALARMA
FIELD_TEMP
```

dependen del proyecto generado por HMI Designer. No deben aparecer como si
fueran constantes predefinidas por el package.

En ejemplos de README se debe:

- definir explícitamente los IDs dentro del ejemplo; o
- indicar claramente que provienen del header generado por el Designer.

### DOC-API-003 — LogicRuntime no es API de inicio

`JWPLC_LogicRuntime` y `JWPLC_LogicRuntime_UI` están marcadas por metadata y
código como internas/experimentales.

Se documentarán porque pueden ser usadas por un usuario avanzado, pero el
README debe advertir que:

- no reemplazan el flujo Arduino normal;
- no implican OpenPLC integrado;
- motor v2 sigue siendo RAM-only;
- la UI v2 sigue siendo experimental;
- no debe enseñarse como siguiente paso obligatorio después de Blink/I/O.

## Verificaciones ya realizadas

- RS485: métodos públicos principales respaldados por implementación.
- Modbus RTU: métodos públicos principales respaldados por implementación.
- Modbus TCP Server: handlers FC01/02/03/04/05/06/15/16 presentes.
- Modbus TCP Client: inconsistencia DOC-API-001 confirmada.
- TFT: métodos públicos respaldados por implementación.
- Display: enums de refresh y modos IDLE verificados en headers actuales.
- GlobalPeripherals: objetos globales, botones y vistas cacheadas verificados.
- LogicRuntime: fachada v1 y contrato explícito v2 verificados en headers.
- LogicRuntime UI: fachada pública begin/update/end verificada.

## Regla para esta tarea

Si una función aparece en un header pero su implementación no respalda el uso
normal, se registra aquí y no se promociona en el tutorial.

No se corrige código dentro de esta tarea.
