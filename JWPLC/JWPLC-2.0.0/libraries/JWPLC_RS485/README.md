# JWPLC_RS485

`JWPLC_RS485` es la librería nativa del ecosistema JWPLC para usar el puerto físico RS-485 del JWPLC Basic como comunicación UART industrial sobre `Serial2`.

Puedes usarla directamente en sketches de JWPLC Basic, sin `#include` manual:

```cpp
JWPLC_RS485.begin();
JWPLC_RS485.println("Hola RS485");
```

## 1. Qué es RS-485

RS-485 es una capa física diferencial. Define cómo viajan los datos eléctricamente por las líneas A/B, pero no define qué significan esos datos.

Sobre RS-485 puedes transportar:

- Texto o comandos propios.
- DWIN por RS-485.
- Modbus RTU.
- Tramas binarias.
- Protocolos propietarios.

En JWPLC se separa así:

```txt
JWPLC_RS485      -> puerto UART/RS-485 genérico
JWPLC_ModbusRTU  -> protocolo Modbus RTU sobre JWPLC_RS485
```

## 2. Hardware JWPLC Basic

| Señal | ESP32 | Uso |
|---|---:|---|
| RX2 | IO16 | Recepción RS-485 |
| TX2 | IO17 | Transmisión RS-485 |

Transceptor:

```txt
MAX13487EESA+
```

El MAX13487 usa auto-direccionamiento. Por eso el JWPLC Basic actual no necesita controlar pines DE/RE.

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

## 4. Por qué no inicia automáticamente

`JWPLC_RS485` es nativo, pero no auto-iniciado.

Esto es intencional, porque RS-485 y Modbus RTU comparten `Serial2` pero usan defaults distintos:

```txt
JWPLC_RS485.begin()      -> 115200, SERIAL_8N1
JWPLC_ModbusRTU.begin()  -> 19200, SERIAL_8E1
```

Si el runtime iniciara RS-485 automáticamente, podría bloquear `Serial2` con una configuración que el usuario no quería.

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

Cierra el puerto.

### `isEnabled()`

```cpp
if (JWPLC_RS485.isEnabled())
{
    Serial.println("RS485 disponible");
}
```

Indica si la placa seleccionada tiene RS-485.

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

### `write()`

```cpp
JWPLC_RS485.write(0x55);
```

```cpp
uint8_t data[] = {0x01, 0x02, 0x03};
JWPLC_RS485.write(data, sizeof(data));
```

Envía bytes.

### `print()` y `println()`

```cpp
JWPLC_RS485.print("Valor: ");
JWPLC_RS485.println(123);
```

Envía texto.

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

## 7. Ejemplo: envío simple

```cpp
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

## 8. Ejemplo: recepción simple

```cpp
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

## 9. Ejemplo: bridge USB ↔ RS-485

```cpp
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

## 10. Hooks weak para DE/RE futuro

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

## 11. Cableado

```txt
A   <-> A
B   <-> B
COM <-> COM
```

Si no comunica, prueba invertir A/B en uno de los extremos. Algunos fabricantes nombran A/B al revés.

## 12. Terminación y bias

- Terminación 120 Ω solo en los extremos del bus.
- Bias normalmente en un solo punto.
- En banco puede funcionar sin terminación.
- En campo conviene usar par trenzado, COM/GND y configuración correcta de jumpers.

## 13. Problemas comunes

### No llega nada

Revisar A/B, baudrate, formato, COM/GND, cableado y que se haya llamado `JWPLC_RS485.begin()`.

### Llegan caracteres raros

Normalmente es baudrate incorrecto, formato distinto (`8N1` vs `8E1`) o ruido.

### Funciona texto pero no Modbus

RS-485 puede estar bien, pero Modbus requiere ID, CRC, función y dirección correctas.

## 14. Ejemplos incluidos

```txt
RS485_Status
RS485_Basic_Send
RS485_Basic_Echo
RS485_USB_Bridge
```
