# JWPLC_RS485

Librería interna del package **JWPLC ESP32** para utilizar el puerto RS-485 integrado del **JWPLC Basic**.

`JWPLC_RS485` encapsula la UART usada por el hardware, mantiene la API de transporte y expone telemetría de actividad para el indicador `BUS` de la pantalla IDLE.

## Inicio

Uso típico:

```cpp
#include <JWPLC_RS485.h>

void setup()
{
    JWPLC_RS485.begin(115200, SERIAL_8N1);
}
```

También puede utilizarse la configuración por defecto de la librería:

```cpp
JWPLC_RS485.begin();
```

La configuración histórica por defecto es `19200` y `SERIAL_8E1`.

RS-485 **no se autoinicia sólo para encender el indicador BUS**. La aplicación o una capa superior como `JWPLC_ModbusRTU` debe iniciar el transporte con los parámetros requeridos por el proyecto.

## Hardware JWPLC Basic

El transporte usa la interfaz serial asignada por la plataforma JWPLC, históricamente `Serial2` en el ESP32 del JWPLC Basic. Los pines y el manejo del transceptor pertenecen al package/hardware y no deben reconfigurarse en un sketch normal.

La librería conserva hooks internos para el control físico del transceptor cuando sean necesarios por una variante.

## API base

```cpp
JWPLC_RS485.begin();
JWPLC_RS485.begin(baud, config);
JWPLC_RS485.end();

JWPLC_RS485.isEnabled();
JWPLC_RS485.isReady();
JWPLC_RS485.lastError();
JWPLC_RS485.lastErrorString();
```

Errores públicos:

```text
JWPLC_RS485_OK
JWPLC_RS485_DISABLED
JWPLC_RS485_INVALID_SERIAL
JWPLC_RS485_NOT_STARTED
JWPLC_RS485_UNKNOWN_ERROR
```

## Lectura y escritura

La API permite utilizar RS-485 como transporte binario o por frames simples.

```cpp
JWPLC_RS485.write(data, length);
JWPLC_RS485.writeFrame(data, length);

JWPLC_RS485.available();
JWPLC_RS485.read(buffer, maxLength);
JWPLC_RS485.readFrame(buffer, maxLength, timeoutMs);
JWPLC_RS485.flush();
```

`write()`/`read()` son la base recomendada para protocolos binarios como Modbus RTU. Los helpers de frame son útiles para pruebas o protocolos simples y no deben confundirse con el framing Modbus.

## Telemetría de actividad

La librería registra actividad TX/RX para que otras capas puedan diagnosticar el bus sin duplicar lógica:

```cpp
JWPLC_RS485.lastTxMs();
JWPLC_RS485.lastRxMs();
JWPLC_RS485.lastActivityMs();
JWPLC_RS485.hasRecentActivity(windowMs);
```

Esta telemetría alimenta el modo automático del indicador `BUS`.

## Integración con BUS

```cpp
JWPLC_Display.setBusLedAuto(true);
```

La capa Display combina estado de `JWPLC_RS485` con el error actual de `JWPLC_ModbusRTU` cuando Modbus está activo.

Códigos originados directamente por la capa RS-485:

| Código | Significado |
|---|---|
| `DIS` | Transporte deshabilitado/no disponible. |
| `INI` | Transporte todavía no iniciado. |
| `SER` | Configuración o estado serial inválido. |
| `---` | RS-485 listo y sin error propio. |

Cuando Modbus RTU está operativo pueden aparecer códigos adicionales (`TMO`, `CRC`, `EXC`, etc.); esos pertenecen a la capa `JWPLC_ModbusRTU` y se documentan en su README.

Semántica visual de `BUS`:

- gris: RS-485 deshabilitado;
- negro: no iniciado o listo sin actividad reciente;
- verde: actividad TX/RX reciente y sin error;
- rojo: error RS-485 o Modbus.

El indicador es diagnóstico: no cambia baudrate, UART ni configuración del bus.

## Convivencia con Modbus RTU

`JWPLC_ModbusRTU` usa `JWPLC_RS485` como transporte. No deben iniciarse dos propietarios con configuraciones incompatibles sobre la misma UART.

Ejemplo slave:

```cpp
JWPLC_ModbusRTU.begin(1, 115200, SERIAL_8N1);
```

En ese caso la capa Modbus se encarga de iniciar/usar el transporte requerido.

## Validación física

En el ciclo 2.1.0 se ha validado RS-485 entre dos JWPLC Basic, incluyendo:

- tráfico binario;
- Modbus RTU master/slave;
- walking de salidas Remote I/O;
- actividad visible en `BUS`;
- timeout Modbus visible como `TMO` rojo;
- recuperación a lecturas correctas y `BUS` verde al volver a estar disponible el peer.

## Estado

```text
JWPLC ESP32 2.1.0-alpha.6
JWPLC_RS485 1.0.1
```

La actualización documental de Alpha6 no cambia la API del transporte; alinea el README con los diagnósticos BUS y las pruebas físicas actuales.
