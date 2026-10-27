# JW_RS485

Puerto **RS-485** de propósito general para **ESP32**, sobre cualquier `HardwareSerial`.

Agrega lo propio de un bus RS-485 a un puerto serie normal: control de dirección del transceptor, transmisión en cola para protocolos cooperativos y telemetría de actividad. Se pueden crear varias instancias, una por UART.

`JWPLC_RS485`, del package JWPLC, es una instancia preconfigurada de esta clase para el puerto integrado del JWPLC Basic.

## Compatibilidad

- ESP32 con Arduino core 3.x (probado con `jwplc:esp32` 2.1.0-alpha.12, core 3.3.8).
- `setForceApbClock()` solo tiene efecto en ESP32 clásico con core 3.x.

## Dependencias

Ninguna.

## Conexión básica

| Transceptor | Configuración |
|---|---|
| Con AutoDirection (MAX13487E, JWPLC Basic v2) | `setAutoDirection()` |
| Clásico con DE y /RE unidos a un GPIO (MAX485) | `setDirectionPin(pin)` |
| Dirección manejada por el sketch o por hardware externo | `setCallbackDirection()` + callbacks |

## Ejemplo mínimo

```cpp
#include <JW_RS485.h>

JW_RS485 rs485(Serial2, JWPLC_RS485_RX_PIN, JWPLC_RS485_TX_PIN);  // UART, RX (GPIO16), TX (GPIO17)

void setup()
{
    rs485.setAutoDirection();
    rs485.begin(115200, SERIAL_8N1);
}

void loop()
{
    rs485.println("hola");
    delay(1000);
}
```

## API principal

| Método | Descripción |
|---|---|
| `JW_RS485(serial, rxPin, txPin)` | Crea el puerto. `-1` conserva los pines por defecto del core. |
| `setAutoDirection(txBufferSize)` | Transceptor con AutoDirection. Con buffer > 0 habilita `writeQueued()`. |
| `setDirectionPin(pin, activeHigh)` | Pin DE/RE: nivel `activeHigh` durante TX. |
| `setCallbackDirection()` | Sin pin: TX bloqueante y solo callbacks. Es el modo por defecto. |
| `setPreTransmitCallback / setPostTransmitCallback / setActivityCallback` | Callbacks opcionales con `userData`. |
| `begin(baud, config)` | Inicia la UART. Devuelve `false` si falla. |
| `write(buffer, size)` | TX bloqueante: retorna cuando el último byte salió. |
| `writeQueued(buffer, size)` | TX en cola (solo AutoDirection); si no está soportado, cae a `write()`. |
| `readAvailable(buffer, max)` | Lee de una vez los bytes disponibles. |
| `effectiveBaudRate()` | Baud real informado por la UART. |
| `hasRecentActivity(ms)`, `lastRxActivityMs()`, `lastTxActivityMs()` | Telemetría de actividad. |
| `printStatus(Print&)` | Resumen de configuración y estado. |

Además es un `Stream`: `available()`, `read()`, `peek()`, `print()`, `println()`.

## Errores posibles

`lastError()` / `lastErrorString()`:

| Código | Causa |
|---|---|
| `JW_RS485_NOT_STARTED` | Se intentó transmitir antes de `begin()` o después de `end()`. |
| `JW_RS485_INVALID_SERIAL` | No hay `HardwareSerial` asociado. |
| `JW_RS485_CLOCK_SOURCE_FAILED` | No se pudo forzar el reloj APB de la UART. |

## Notas de uso

- Configura la dirección antes de `begin()`. El tamaño del buffer TX solo se aplica en el primer `begin()`.
- Con `setDirectionPin()`, `write()` espera a que la UART termine (`flush()`) antes de soltar el bus.
- Los callbacks se ejecutan dentro de `write()` y deben ser breves.

## Limitaciones

- Solo ESP32 en esta versión (`HardwareSerial::begin` con pines y `setTxBufferSize`).
- No incluye protocolo: para Modbus RTU usar `JW_ModbusRTU`.

## Pruebas

```bash
bash extras/test/run_tests.sh
```
