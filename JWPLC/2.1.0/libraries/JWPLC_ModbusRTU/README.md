# JWPLC_ModbusRTU

Librería interna del package **JWPLC ESP32** para Modbus RTU sobre `JWPLC_RS485`.

La versión actual mantiene un alcance deliberadamente acotado: servidor slave con Holding Registers y cliente master básico. No pretende declarar soporte para todo Modbus RTU.

## Inicio

### Slave

```cpp
#include <JWPLC_ModbusRTU.h>

uint16_t holding[16] = {};

void setup()
{
    JWPLC_ModbusRTU.setHoldingRegisters(holding, 16);
    JWPLC_ModbusRTU.begin(1, 115200, SERIAL_8N1);
}

void loop()
{
    JWPLC_ModbusRTU.task();
}
```

También existe el inicio por defecto, históricamente con slave ID `1`, `19200` y `SERIAL_8E1`.

### Master

La misma capa puede actuar como master para las operaciones implementadas:

```cpp
uint16_t values[4];

bool ok = JWPLC_ModbusRTU.readHoldingRegisters(
    slaveId,
    startAddress,
    quantity,
    values,
    1000);
```

## Funciones implementadas

### Slave

| Código | Función |
|---:|---|
| `0x03` | Read Holding Registers |
| `0x06` | Write Single Register |
| `0x10` | Write Multiple Registers |

### Master

API pública principal:

```cpp
readHoldingRegisters(...)
writeSingleRegister(...)
```

No debe asumirse soporte master para funciones que no estén expuestas por la API actual.

## API de ciclo de vida

```cpp
JWPLC_ModbusRTU.begin();
JWPLC_ModbusRTU.begin(slaveId, baud, serialConfig);
JWPLC_ModbusRTU.end();

JWPLC_ModbusRTU.task();
JWPLC_ModbusRTU.poll();
```

`task()`/`poll()` procesan el servicio slave. Las operaciones master esperan cooperativamente su respuesta hasta completar o vencer el timeout configurado para la llamada.

## Holding Registers

```cpp
JWPLC_ModbusRTU.setHoldingRegisters(registers, count);
```

El mapa debe mantenerse válido durante la operación del slave. Un mapa inválido se refleja en `lastError()` y en el diagnóstico `BUS`.

## Estado y estadísticas

La librería expone estado, último error y contadores de diagnóstico. Los errores públicos actuales incluyen:

```text
JWPLC_MODBUS_OK
JWPLC_MODBUS_DISABLED
JWPLC_MODBUS_INVALID_SLAVE_ID
JWPLC_MODBUS_INVALID_REGISTER_MAP
JWPLC_MODBUS_NOT_STARTED
JWPLC_MODBUS_INVALID_FRAME
JWPLC_MODBUS_CRC_ERROR
JWPLC_MODBUS_UNSUPPORTED_FUNCTION
JWPLC_MODBUS_INVALID_ADDRESS
JWPLC_MODBUS_BUFFER_OVERFLOW
JWPLC_MODBUS_TIMEOUT
JWPLC_MODBUS_INVALID_RESPONSE
JWPLC_MODBUS_EXCEPTION_RESPONSE
JWPLC_MODBUS_UNKNOWN_ERROR
```

El README no convierte todos estos errores en un código visual distinto: la pantalla IDLE compacta los que requieren atención en el indicador `BUS`.

## Códigos BUS

Con `JWPLC_Display.setBusLedAuto(true)`, Display combina el estado RS-485 con el último error Modbus.

| Código | Origen | Significado |
|---|---|---|
| `SID` | Modbus | Slave ID inválido. |
| `MAP` | Modbus | Mapa de registros inválido. |
| `TMO` | Modbus | Timeout esperando respuesta. |
| `CRC` | Modbus | CRC inválido. |
| `EXC` | Modbus | Exception Response. |
| `RSP` | Modbus | Respuesta inválida. |
| `OVF` | Modbus | Buffer overflow. |
| `FUN` | Modbus | Función no soportada. |
| `---` | Modbus/RS-485 | Sin error de protocolo. |

Los códigos `DIS`, `INI` y `SER` pertenecen principalmente al estado del transporte `JWPLC_RS485`.

Cuando `lastError()` es `OK`, `DISABLED` o `NOT_STARTED`, la capa Modbus no fuerza por sí sola un error rojo si RS-485 está listo. Esto permite utilizar RS-485 crudo sin que Modbus opcional se interprete como falla.

## Semántica del indicador BUS

- gris: transporte no disponible;
- negro: transporte iniciado y sin actividad reciente, o todavía no iniciado según el código;
- verde: actividad TX/RX reciente sin error;
- rojo: error RS-485/Modbus que requiere atención.

Ejemplo:

```cpp
JWPLC_Display.setBusLedAuto(true);
JWPLC_ModbusRTU.begin(1, 115200, SERIAL_8N1);
```

## Remote I/O

Los ejemplos del package incluyen pruebas master/slave usadas para validar comunicación entre dos JWPLC Basic y el PoC de Remote I/O RTU.

El patrón validado utiliza un JWPLC como master y otro como slave, transportando Holding Registers y actualizando E/S sobre Modbus RTU. Esto es una validación de la librería actual; no convierte automáticamente cualquier sketch en un sistema Remote I/O.

## Validación Alpha6

El gate físico de `BUS` se cerró con dos JWPLC Basic:

1. master activo sin slave disponible → timeout `TMO`, indicador rojo;
2. slave iniciado → lecturas correctas sin reset del master;
3. actividad posterior → `BUS` verde;
4. sin romper la coexistencia del resto del runtime.

También se conserva la validación previa de walking outputs del PoC Remote I/O RTU.

## Precompilación

`JWPLC_ModbusRTU` declara `precompiled=full` para el package. Los cambios documentales de Alpha6 no cambian la API ni el ABI validado.

## Estado

```text
JWPLC ESP32 2.1.0-alpha.6
JWPLC_ModbusRTU 1.0.0
```

La librería sigue siendo una implementación base y controlada. Nuevas funciones Modbus deben añadirse explícitamente y validarse antes de documentarlas como soportadas.
