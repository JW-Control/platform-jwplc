# JWPLC_RS485

`JWPLC_RS485` es la librería nativa del ecosistema JWPLC para usar el puerto físico RS-485 del **JWPLC Basic** como comunicación UART industrial sobre `Serial2`.

Permite usar RS-485 como puerto genérico y también sirve como base para `JWPLC_ModbusRTU`.

```cpp
#include <JWPLC_RS485.h>

JWPLC_RS485.begin();
JWPLC_RS485.println("Hola RS485");
```

Cuando el sketch usa `JWPLC_ModbusRTU`, no es necesario incluir `JWPLC_RS485.h` directamente porque `JWPLC_ModbusRTU.h` ya lo incluye.

---

## 1. Qué es RS-485

RS-485 es una capa física diferencial. Define cómo viajan los datos eléctricamente por las líneas A/B, pero no define qué significan esos datos.

Sobre RS-485 puedes transportar:

- texto o comandos propios;
- DWIN por RS-485;
- Modbus RTU;
- tramas binarias;
- protocolos propietarios.

En JWPLC se separa así:

```txt
JWPLC_RS485      -> puerto UART/RS-485 genérico
JWPLC_ModbusRTU  -> protocolo Modbus RTU sobre JWPLC_RS485
```

---

## 2. Hardware JWPLC Basic

| Señal | ESP32 | Uso |
|---|---:|---|
| RX2 | IO16 | Recepción RS-485 |
| TX2 | IO17 | Transmisión RS-485 |

Transceptor:

```txt
MAX13487EESA+
```

El MAX13487 usa auto-direccionamiento. Por eso el JWPLC Basic actual no necesita controlar pines DE/RE por software.

---

## 3. Default

```cpp
JWPLC_RS485.begin();
```

equivale a:

```cpp
JWPLC_RS485.begin(115200, SERIAL_8N1);
```

Configuración por defecto:

```txt
Baudrate: 115200
Formato:  SERIAL_8N1
UART:     Serial2
RX:       IO16
TX:       IO17
```

Este default es práctico para debug, bridge USB-RS485, DWIN, protocolos propios y pruebas rápidas.

---

## 4. Por qué no inicia automáticamente

`JWPLC_RS485` es nativo, pero no auto-iniciado.

Esto es intencional porque RS-485 y Modbus RTU comparten `Serial2`, pero pueden usar defaults distintos:

```txt
JWPLC_RS485.begin()      -> 115200, SERIAL_8N1
JWPLC_ModbusRTU.begin()  -> 19200, SERIAL_8E1
```

Si el runtime iniciara RS-485 automáticamente, podría bloquear `Serial2` con una configuración que el usuario no quería.

---

## 5. SERIAL_8N1 vs SERIAL_8E1

`SERIAL_8N1`:

```txt
8 bits de datos
sin paridad
1 bit de parada
```

`SERIAL_8E1`:

```txt
8 bits de datos
paridad par
1 bit de parada
```

Para que dos equipos comuniquen, ambos deben usar el mismo baudrate y el mismo formato.

---

## 6. API principal

### `begin()`

```cpp
JWPLC_RS485.begin();
```

Inicializa RS-485 con `115200, SERIAL_8N1`.

### `begin(baud)`

```cpp
JWPLC_RS485.begin(9600);
```

Inicializa con baudrate personalizado y `SERIAL_8N1`.

### `begin(baud, config)`

```cpp
JWPLC_RS485.begin(19200, SERIAL_8E1);
```

Inicializa con baudrate y formato personalizados.

### `end()`

```cpp
JWPLC_RS485.end();
```

Cierra el puerto y limpia el estado de actividad.

### `isEnabled()`

```cpp
if (JWPLC_RS485.isEnabled())
{
    Serial.println("RS485 disponible");
}
```

Indica si la placa seleccionada tiene RS-485 disponible.

### `isReady()`

```cpp
if (JWPLC_RS485.isReady())
{
    JWPLC_RS485.println("OK");
}
```

Indica si `begin()` fue ejecutado correctamente.

### `available()`, `read()`

```cpp
while (JWPLC_RS485.available() > 0)
{
    int value = JWPLC_RS485.read();
}
```

Lee bytes recibidos.

Desde `v2.1.0-alpha.2`, `available()` y `read()` marcan actividad RX cuando hay datos recibidos. Esto permite que `JWPLC_Display` refleje actividad del bus en el LED `BUS`.

### `write()`

```cpp
JWPLC_RS485.write(0x55);
```

```cpp
uint8_t data[] = {0x01, 0x02, 0x03};
JWPLC_RS485.write(data, sizeof(data));
```

Envía bytes.

Desde `v2.1.0-alpha.2`, `write()` marca actividad TX cuando se envían bytes correctamente.

### `print()` y `println()`

`JWPLC_RS485` hereda de `Print`, por lo que puede usarse así:

```cpp
JWPLC_RS485.print("Valor: ");
JWPLC_RS485.println(123);
```

### `flush()`

```cpp
JWPLC_RS485.flush();
```

Espera a que termine la transmisión.

### `stream()`

```cpp
Stream &port = JWPLC_RS485.stream();
```

Permite entregar el puerto a protocolos superiores.

### `serial()`

```cpp
HardwareSerial &rs485 = JWPLC_RS485.serial();
```

Accede al `HardwareSerial` interno.

### `printStatus()`

```cpp
JWPLC_RS485.printStatus(Serial);
```

Imprime estado, baudrate, configuración, RX/TX y último error.

---

## 7. API de actividad

A partir de `v2.1.0-alpha.2`, la librería expone estado de actividad TX/RX.

```cpp
uint32_t last = JWPLC_RS485.lastActivityMs();
uint32_t lastRx = JWPLC_RS485.lastRxActivityMs();
uint32_t lastTx = JWPLC_RS485.lastTxActivityMs();

if (JWPLC_RS485.hasRecentActivity(800))
{
    Serial.println("Actividad RS485 reciente");
}
```

| API | Descripción |
|---|---|
| `lastActivityMs()` | Último `millis()` con actividad RX o TX. |
| `lastRxActivityMs()` | Último `millis()` con actividad RX. |
| `lastTxActivityMs()` | Último `millis()` con actividad TX. |
| `hasRecentActivity(windowMs)` | Devuelve `true` si hubo actividad dentro de la ventana indicada. |

Esta API se usa internamente para el LED `BUS` automático de `JWPLC_Display`.

---

## 8. Hook weak de actividad

La librería declara un hook weak:

```cpp
extern "C" void jwplcRs485ActivityCallback(void);
```

Por defecto no hace nada.

`JWPLC_Display` puede sobreescribirlo para actualizar el LED `BUS` cuando se detecta TX/RX reciente.

Este hook está pensado para integración interna del package. En sketches normales no es necesario sobreescribirlo.

---

## 9. Ejemplo: envío simple

```cpp
#include <Arduino.h>
#include <JWPLC_RS485.h>

void setup()
{
    Serial.begin(115200);
    delay(1200);

    JWPLC_RS485.begin();
}

void loop()
{
    JWPLC_RS485.println("Mensaje desde JWPLC Basic");
    delay(1000);
}
```

---

## 10. Ejemplo: recepción simple

```cpp
#include <Arduino.h>
#include <JWPLC_RS485.h>

void setup()
{
    Serial.begin(115200);
    delay(1200);

    JWPLC_RS485.begin();
}

void loop()
{
    while (JWPLC_RS485.available() > 0)
    {
        int value = JWPLC_RS485.read();

        if (value >= 0)
        {
            Serial.write((uint8_t)value);
        }
    }
}
```

---

## 11. Ejemplo: bridge USB ↔ RS-485

```cpp
#include <Arduino.h>
#include <JWPLC_RS485.h>

void setup()
{
    Serial.begin(115200);
    delay(1200);

    JWPLC_RS485.begin();
    Serial.println("Bridge USB <-> RS485 listo");
}

void loop()
{
    while (Serial.available() > 0)
    {
        int value = Serial.read();

        if (value >= 0)
        {
            JWPLC_RS485.write((uint8_t)value);
        }
    }

    while (JWPLC_RS485.available() > 0)
    {
        int value = JWPLC_RS485.read();

        if (value >= 0)
        {
            Serial.write((uint8_t)value);
        }
    }
}
```

---

## 12. Ejemplo: BUS automático con Display

```cpp
#include <Arduino.h>
#include <JWPLC_Display.h>
#include <JWPLC_RS485.h>

void setup()
{
    Serial.begin(115200);

    JWPLC_Display.setBusLedAuto(true);
    JWPLC_RS485.begin(115200, SERIAL_8N1);
}

void loop()
{
    JWPLC_RS485.println("Ping RS485");
    delay(1000);
}
```

Resultado esperado en la pantalla IDLE:

| Estado | LED BUS |
|---|---|
| RS-485 no iniciado | Gris |
| RS-485 iniciado sin tráfico | Apagado |
| TX/RX reciente | Verde |
| Error real | Rojo |

---

## 13. Hooks weak para DE/RE futuro

El JWPLC Basic actual no usa DE/RE por software, pero la librería deja hooks weak para hardware futuro:

```cpp
extern "C" void jwplcRs485PreTransmitCallback(void)
{
    digitalWrite(RS485_DE_PIN, HIGH);
}

extern "C" void jwplcRs485PostTransmitCallback(void)
{
    digitalWrite(RS485_DE_PIN, LOW);
}
```

Por defecto no hacen nada.

---

## 14. Cableado

```txt
A   <-> A
B   <-> B
COM <-> COM
```

Si no comunica, prueba invertir A/B en uno de los extremos. Algunos fabricantes nombran A/B al revés.

---

## 15. Terminación y bias

- Terminación 120 Ω solo en los extremos del bus.
- Bias normalmente en un solo punto.
- En banco puede funcionar sin terminación.
- En campo conviene usar par trenzado, COM/GND y configuración correcta de jumpers.

---

## 16. Problemas comunes

### No llega nada

Revisar A/B, baudrate, formato, COM/GND, cableado y que se haya llamado `JWPLC_RS485.begin()`.

### Llegan caracteres raros

Normalmente es baudrate incorrecto, formato distinto (`8N1` vs `8E1`) o ruido.

### Funciona texto, pero no Modbus

RS-485 puede estar bien, pero Modbus requiere ID, CRC, función y dirección correctas.

### El LED BUS queda gris

Significa que RS-485 no está disponible en la variante o no se ha iniciado todavía.

Para usar RS-485 genérico:

```cpp
JWPLC_RS485.begin();
```

Para usar Modbus RTU:

```cpp
#include <JWPLC_ModbusRTU.h>

JWPLC_ModbusRTU.begin(1, 115200, SERIAL_8N1);
```

---

## 17. Ejemplos incluidos

```txt
RS485_Status
RS485_Basic_Send
RS485_Basic_Echo
RS485_USB_Bridge
```

Códigos internos de validación relacionados:

```txt
JWPLC/Test_Codes/
```

Pruebas usadas durante `v2.1.0-alpha.2`:

- BUS automático por actividad RS-485;
- Modbus RTU master/slave entre dos JWPLC Basic;
- parpadeo de BUS en master y slave;
- estados gris/apagado/verde/rojo en pantalla IDLE.

---

## 18. Estado

Documentación actualizada para:

```txt
JWPLC ESP32 2.1.0-alpha.2
JWPLC_RS485
```

Cambios principales:

- tracking de actividad TX/RX;
- `lastActivityMs()`;
- `lastRxActivityMs()`;
- `lastTxActivityMs()`;
- `hasRecentActivity(windowMs)`;
- hook weak `jwplcRs485ActivityCallback()`;
- soporte para LED BUS automático en `JWPLC_Display`.
