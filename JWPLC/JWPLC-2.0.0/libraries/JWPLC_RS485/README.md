# JWPLC_RS485

Librería interna del ecosistema **JWPLC** para usar el puerto **RS-485** del JWPLC Basic mediante `Serial2`.

Esta librería no implementa Modbus directamente. Su objetivo es exponer una capa UART genérica sobre la capa física RS-485. Luego, protocolos superiores como Modbus RTU, DWIN o protocolos propios pueden trabajar encima de este puerto.

---

## Hardware JWPLC Basic

| Señal | ESP32 | Descripción |
|---|---:|---|
| RX2 | IO16 | Recepción RS-485 |
| TX2 | IO17 | Transmisión RS-485 |

Transceptor:

```txt
MAX13487EESA+
```

El MAX13487 usa auto-direccionamiento, por lo que el JWPLC Basic actual no necesita controlar pines DE/RE desde el ESP32.

---

## Uso básico

```cpp
void setup()
{
    Serial.begin(115200);
    delay(1200);

    JWPLC_RS485.begin(); // 115200, SERIAL_8N1
    JWPLC_RS485.println("Hola RS485");
}

void loop()
{
}
```

`JWPLC_RS485.begin()` equivale a:

```cpp
JWPLC_RS485.begin(115200, SERIAL_8N1);
```

---

## Configuración personalizada

```cpp
JWPLC_RS485.begin(9600, SERIAL_8N1);
JWPLC_RS485.begin(19200, SERIAL_8E1);
JWPLC_RS485.begin(115200, SERIAL_8N1);
```

---

## Default acordado

Para UART genérico sobre RS-485:

```txt
115200, SERIAL_8N1
```

Uso recomendado:

- Debug por RS-485.
- Bridge USB <-> RS-485.
- Pantallas DWIN.
- Protocolos propios.
- Banco de pruebas.

Para Modbus RTU se recomienda que el wrapper futuro `JWPLC_ModbusRTU` use por defecto:

```txt
19200, SERIAL_8E1
```

---

## Acceso como Stream

```cpp
Stream &port = JWPLC_RS485.stream();
```

Esto permite conectar protocolos superiores que trabajen sobre `Stream`.

---

## Hooks weak para hardware futuro

Aunque el JWPLC Basic actual usa auto-direccionamiento, la librería deja hooks weak para hardware futuro con DE/RE manual:

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

Por defecto estos callbacks no hacen nada.

---

## Ejemplos incluidos

- `RS485_Status`
- `RS485_Basic_Send`
- `RS485_Basic_Echo`
- `RS485_USB_Bridge`
