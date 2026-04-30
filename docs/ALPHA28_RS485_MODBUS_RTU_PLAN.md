# Alpha28 RS485 + Modbus RTU Plan

Objetivo de `alpha28-rs485-modbus-rtu`: añadir la base industrial de comunicación serie del JWPLC Basic mediante RS-485 sobre `Serial2`, y preparar una primera capa Modbus RTU mínima sin tocar todavía la integración OpenPLC.

---

## 1. Alcance de alpha28

### Sí entra

- Crear librería interna `JWPLC_RS485` dentro del package.
- Exponer objeto global `JWPLC_RS485`.
- Usar `Serial2` como puerto dedicado RS-485 del JWPLC Basic.
- Permitir modo UART genérico sobre capa física RS-485.
- Crear ejemplos de prueba RS-485.
- Crear base Modbus RTU mínima.
- Preparar separación clara entre:
  - capa física/puerto: `JWPLC_RS485`
  - protocolo general: futuro `JW_Modbus`
  - wrapper específico JWPLC: futuro `JWPLC_ModbusRTU`

### No entra todavía

- Integración OpenPLC.
- Mapeo automático de I/O JWPLC a coils/registers.
- Modbus TCP.
- Gateway Modbus TCP <-> Modbus RTU.
- DWIN RS485.
- Optimización profunda de throughput.
- Diagnóstico industrial avanzado.

---

## 2. Separación de capas

### `JWPLC_RS485`

Librería interna del package, específica para JWPLC Basic.

Responsabilidad:

- Inicializar `Serial2` con pines/configuración del hardware JWPLC.
- Exponer funciones UART genéricas.
- Proveer acceso a `Stream` / `HardwareSerial` para protocolos superiores.
- Manejar TX flush y tiempos básicos si el hardware requiere control de dirección.
- Mantener compatibilidad con MAX13487 auto-direction si no existe pin DE/RE.

Uso esperado:

```cpp
JWPLC_RS485.begin(9600);
JWPLC_RS485.println("Hola RS485");
```

Uso avanzado:

```cpp
JWPLC_RS485.begin(19200, SERIAL_8E1);
Stream &port = JWPLC_RS485.stream();
```

### `JW_Modbus`

Librería futura en `JW-Libraries`, reutilizable fuera del package.

Responsabilidad:

- CRC16 Modbus.
- Construcción y parseo de tramas RTU.
- Cliente/master RTU.
- Servidor/slave RTU.
- Validación de timeout/inter-frame gap.
- API independiente del hardware usando `Stream`.

### `JWPLC_ModbusRTU`

Wrapper futuro específico para JWPLC Basic.

Responsabilidad:

- Usar `JW_Modbus` sobre `JWPLC_RS485`.
- Definir configuración por defecto industrial.
- Preparar mapeo futuro de OpenPLC.

---

## 3. API inicial propuesta para `JWPLC_RS485`

```cpp
JWPLC_RS485.begin();
JWPLC_RS485.begin(baud);
JWPLC_RS485.begin(baud, config);
JWPLC_RS485.end();
JWPLC_RS485.isEnabled();
JWPLC_RS485.isReady();
JWPLC_RS485.baudRate();
JWPLC_RS485.config();
JWPLC_RS485.available();
JWPLC_RS485.read();
JWPLC_RS485.write(byte);
JWPLC_RS485.print(...);
JWPLC_RS485.println(...);
JWPLC_RS485.flush();
JWPLC_RS485.stream();
JWPLC_RS485.serial();
JWPLC_RS485.statusString();
JWPLC_RS485.printStatus(Serial);
```

---

## 4. Parámetros por confirmar antes de codificar

- Pin RX de `Serial2` conectado al transceptor RS-485.
- Pin TX de `Serial2` conectado al transceptor RS-485.
- Si el MAX13487 trabaja completamente con auto-dirección.
- Si existe pin DE/RE conectado al ESP32 o no.
- Baudios por defecto deseados:
  - 9600
  - 19200
  - 115200
- Configuración por defecto:
  - `SERIAL_8N1`
  - `SERIAL_8E1`
  - otra
- Nombre físico del puerto en documentación:
  - RS485
  - COM
  - Modbus RTU

---

## 5. Ejemplos RS-485 propuestos

### `RS485_Basic_Send`

Envía texto periódico por RS-485.

### `RS485_Basic_Echo`

Lee bytes desde RS-485 y los devuelve por el mismo puerto.

### `RS485_USB_Bridge`

Puente entre Monitor Serie USB y RS-485:

```txt
USB Serial <-> Serial2 RS485
```

Útil para depurar DWIN, Modbus, sensores RS-485 o equipos externos.

### `RS485_Status`

Muestra estado del puerto:

- habilitado
- listo
- baudrate
- configuración
- RX/TX pins

---

## 6. Modbus RTU base en alpha28

Primera etapa mínima:

- CRC16 Modbus probado.
- Parser básico de trama RTU.
- Constructor básico de respuesta.
- Soporte inicial de funciones frecuentes:
  - `0x03` Read Holding Registers
  - `0x06` Write Single Register
  - `0x10` Write Multiple Registers
- Ejemplo slave/server con holding registers en RAM.
- Ejemplo master/client consultando un slave.

Funciones digitales (`0x01`, `0x02`, `0x05`, `0x0F`) pueden entrar si el tiempo alcanza, pero no son requisito para cerrar la primera base.

---

## 7. OpenPLC queda fuera de alpha28

OpenPLC se abordará después de tener una beta funcional.

Futuro alcance OpenPLC:

- Definir mapa JWPLC:
  - coils
  - discrete inputs
  - holding registers
  - input registers
- Vincular I/O real del JWPLC con Modbus RTU.
- Integrar OpenPLC nativo para JWPLC Basic.

---

## 8. Criterio para cerrar alpha28

Alpha28 puede cerrarse cuando:

- `JWPLC_RS485` compile en JWPLC Basic.
- `JWPLC_RS485` reporte disabled/safe en placas sin RS-485, si aplica.
- Se pueda enviar texto por RS-485.
- Se pueda recibir texto por RS-485.
- El bridge USB <-> RS-485 funcione.
- CRC16 Modbus esté probado con vectores conocidos.
- Modbus RTU slave mínimo responda a `Read Holding Registers`.
- Modbus RTU master mínimo pueda hacer una consulta básica.
- Ejemplos principales compilen en Arduino IDE.

---

## 9. Branch

```txt
develop/alpha28-rs485-modbus-rtu
```
