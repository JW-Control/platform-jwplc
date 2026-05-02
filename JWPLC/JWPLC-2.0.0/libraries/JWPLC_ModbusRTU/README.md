# JWPLC_ModbusRTU

Librería interna base para **Modbus RTU sobre RS-485** en JWPLC Basic.

Esta primera versión está pensada para validar la base de comunicación Modbus RTU antes de una integración OpenPLC futura.

---

## Capas del sistema

```txt
JWPLC_RS485        -> UART/RS-485 sobre Serial2
JWPLC_ModbusRTU    -> Modbus RTU base sobre JWPLC_RS485
OpenPLC mapping    -> futuro, fuera de alpha28
```

---

## Defaults

Para `JWPLC_ModbusRTU.begin()`:

```txt
19200 baud
SERIAL_8E1
Slave ID 1
```

Equivale a:

```cpp
JWPLC_ModbusRTU.begin(1, 19200, SERIAL_8E1);
```

---

## Funciones soportadas en la base

Servidor/slave:

- `0x03` Read Holding Registers
- `0x06` Write Single Register
- `0x10` Write Multiple Registers

Cliente/master básico:

- Read Holding Registers
- Write Single Register

---

## Ejemplo slave básico

```cpp
#include <JWPLC_ModbusRTU.h>

uint16_t holdingRegs[10];

void setup()
{
    Serial.begin(115200);
    JWPLC_ModbusRTU.setHoldingRegisters(holdingRegs, 10);
    JWPLC_ModbusRTU.begin();
}

void loop()
{
    JWPLC_ModbusRTU.task();
}
```

---

## Nota sobre OpenPLC

OpenPLC no entra todavía en alpha28. Primero se valida:

- RS-485.
- CRC16 Modbus.
- Slave básico.
- Master básico.

Luego se definirá el mapa industrial JWPLC.
