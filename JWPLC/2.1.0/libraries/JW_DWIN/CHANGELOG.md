# Changelog

## [0.2.0] - 2026-10-26

### Añadido
- Transporte `JW_DWIN_ModbusUSB`: Modbus RTU (03, 06 y 16) dentro de un sobre `1B 4A 57 LEN` por el USB de la tarjeta, el mismo `Serial` del texto de depuración. Para el modo Simular → USB de JW DWIN. Arma las tramas y el CRC por su cuenta (no usa `JW_ModbusRTU`) y envía cada sobre en una sola escritura.
- El texto que llega fuera de los sobres queda en `text()` (un `Stream`) para que el firmware lea sus comandos de ahí. Un sobre con largo o CRC inválido se descarta.
- Ejemplo `Simular_USB`.
- Prueba N1 del transporte (`extras/test/test_usb.cpp`).
- `checkKeys(keys, count)`: lee varias teclas con una lectura 03 por tramo de VPs cercanos (hasta `JW_DWIN_MAX_READ` registros, huecos de hasta `JW_DWIN_READ_MAX_GAP`) y escribe 0 solo en las pulsadas. `checkKey()` no cambia.
- `readValues(objs, count, changed)`: `readValue()` de varios valores con una lectura por tramo, como `checkKeys()`.
- `readRegisters(vp, count, values)` público.

## [0.1.1] - 2026-10-23

### Documentación
- `keywords.txt` completo: métodos `isConfirmed`, `currentPage`, `failureCount`, `lastSuccessMs`, `setTouchDelay`, `lastErrorString`, `setDebugMask`, `debugMask`, `readRegisters`, `writeRegisters`, `timeout`, `slaveId` y macros de configuración.
- README: API completa de `JW_DWIN` y `JW_DWIN_ModbusRTU`, opciones de compilación y cómo escribir un transporte propio.

## [0.1.0] - 2026-10-22

### Añadido
- Primera versión. API de la clase `JW_DWIN` de `JW_DWIN_RS485` 1.x (local, sep-2026) con los mismos structs `DWIN_Key`, `DWIN_Value` y `DWIN_String`.
- Interfaz `JW_DWIN_Transport`; transporte `JW_DWIN_ModbusRTU` (RS-485, DGUS Modbus) sobre `JW_ModbusRTU`.
- Cache confirmada por la pantalla: un envío fallido se reintenta en la siguiente llamada.
- Modo sin enlace (`setOfflinePolicy`), aviso de reconexión (`reconnected()`) e invalidación de cache.
- `isConfirmed(obj)`; `readValue()` no reporta cambio en la lectura que restablece el enlace.
- Prueba N1 en host (`extras/test`).

### Cambiado respecto a JW_DWIN_RS485 1.x
- El constructor recibe un transporte en lugar de `HardwareSerial` y pines.
- `keyEncoder()` ya no imprime por `Serial`; usa la salida de debug (`DWIN_DBG_FLOW`).
- Sin `setPrePostTransmission()` ni `modbus()`: la dirección del bus la maneja `JW_RS485`.
