# JWPLC_RS485

Librería del package **JWPLC ESP32** para el puerto RS-485 integrado del **JWPLC Basic**.

`JWPLC_RS485` encapsula la UART asignada por la plataforma, expone una interfaz `Stream` y registra actividad TX/RX para el diagnóstico `BUS`.

## Hardware JWPLC Basic

El hardware actual usa un **MAX13487E con autodirección**. Un sketch normal no necesita manejar manualmente DE/RE ni reasignar RX/TX.

Los pines y la instancia `HardwareSerial` pertenecen al package.

## Inicio

RS-485 no se autoinicia con parámetros arbitrarios. La aplicación o una capa superior como Modbus RTU define baudrate y formato.

```cpp
#include <JWPLC_RS485.h>

void setup()
{
    JWPLC_RS485.begin(115200, SERIAL_8N1);
}
```

Overloads disponibles:

```cpp
JWPLC_RS485.begin();
JWPLC_RS485.begin(baud);
JWPLC_RS485.begin(baud, config);
JWPLC_RS485.end();
```

Los defaults de la librería son `115200` y `SERIAL_8N1` en el ciclo actual del package.

## API Stream

`JWPLC_RS485Class` hereda de `Stream`, por lo que puede usarse como un puerto Arduino:

```cpp
JWPLC_RS485.available();
JWPLC_RS485.peek();
JWPLC_RS485.read();

JWPLC_RS485.write(byteValue);
JWPLC_RS485.write(buffer, length);
JWPLC_RS485.print("texto");
JWPLC_RS485.println(value);
JWPLC_RS485.flush();
```

Acceso avanzado:

```cpp
Stream &s = JWPLC_RS485.stream();
HardwareSerial &uart = JWPLC_RS485.serial();
```

No se recomienda reconfigurar directamente la UART obtenida por `serial()` en un JWPLC Basic normal.

## TX encolada / AutoDirection

JWPLC Basic v2 usa MAX13487E con AutoDirection. El package reserva un buffer TX
UART de 512 bytes antes de iniciar Serial2, suficiente para una ADU Modbus RTU
maxima.

La API historica conserva su semantica bloqueante:

```cpp
JWPLC_RS485.write(buffer, length); // espera flush
```

Se anade una ruta aditiva:

```cpp
JWPLC_RS485.writeQueued(buffer, length);
```

`writeQueued()` solo opera sin `flush()` cuando el hardware declara
AutoDirection y el buffer TX fue configurado. En otro hardware cae de forma
segura al `write()` bloqueante, preservando los hooks DE/RE existentes.

Diagnostico:

```cpp
JWPLC_RS485.autoDirection();
JWPLC_RS485.txBufferSize();
JWPLC_RS485.queuedWriteSupported();
```

La ruta encolada se encuentra en qualification RTU-F3; el comportamiento
historico sigue siendo el default hasta cerrar el gate.

## Estado

```cpp
JWPLC_RS485.isEnabled();
JWPLC_RS485.isReady();
JWPLC_RS485.baudRate();
JWPLC_RS485.config();
JWPLC_RS485.configString();
JWPLC_RS485.lastError();
JWPLC_RS485.lastErrorString();
JWPLC_RS485.statusString();
JWPLC_RS485.printStatus(Serial);
```

Errores:

```text
JWPLC_RS485_OK
JWPLC_RS485_DISABLED
JWPLC_RS485_NOT_STARTED
JWPLC_RS485_INVALID_SERIAL
JWPLC_RS485_UNKNOWN_ERROR
```

## Telemetría de actividad

```cpp
JWPLC_RS485.lastActivityMs();
JWPLC_RS485.lastRxActivityMs();
JWPLC_RS485.lastTxActivityMs();
JWPLC_RS485.hasRecentActivity(windowMs);
```

La telemetría se actualiza cuando se leen/escriben datos mediante `JWPLC_RS485` y alimenta el indicador automático `BUS` del Display.

## Integración con Display

```cpp
JWPLC_Display.setBusLedAuto(true);
```

Códigos propios de la capa RS-485:

| Código | Significado |
|---|---|
| `DIS` | Transporte deshabilitado/no disponible. |
| `INI` | Transporte todavía no iniciado. |
| `SER` | Estado/configuración serial inválida. |
| `---` | Transporte listo sin error propio. |

Cuando `JWPLC_ModbusRTU` está activo pueden aparecer códigos adicionales como `TMO`, `CRC`, `EXC`, `RSP`, `OVF` o `FUN`.

## Convivencia con Modbus RTU

`JWPLC_ModbusRTU` usa este mismo transporte. Si Modbus inicia el puerto, no debe existir otro propietario reconfigurando la UART simultáneamente.

```cpp
JWPLC_ModbusRTU.begin(2, 115200, SERIAL_8N1);
```

## Ejemplos numerados para taller

```text
01.RS485_Send
02.RS485_Echo
03.RS485_Status
```

Los ejemplos históricos de bridge/native permanecen disponibles como material avanzado.

## Estado Alpha8

```text
JWPLC ESP32 2.1.0-alpha.8
JWPLC_RS485 1.0.1
```

Alpha8 no cambia el protocolo físico RS-485; actualiza la documentación de la API real y añade una serie compacta de ejemplos de usuario.
