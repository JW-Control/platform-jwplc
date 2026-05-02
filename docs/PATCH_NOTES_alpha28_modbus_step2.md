# Alpha28 Modbus RTU step 2 - cambios puntuales

## 1. Crear librería interna

Crear carpeta:

```txt
JWPLC/JWPLC-2.0.0/libraries/JWPLC_ModbusRTU
```

Archivos:

```txt
JWPLC/JWPLC-2.0.0/libraries/JWPLC_ModbusRTU/library.properties
JWPLC/JWPLC-2.0.0/libraries/JWPLC_ModbusRTU/README.md
JWPLC/JWPLC-2.0.0/libraries/JWPLC_ModbusRTU/src/JWPLC_ModbusRTU.h
JWPLC/JWPLC-2.0.0/libraries/JWPLC_ModbusRTU/src/JWPLC_ModbusRTU.cpp
```

Ejemplos:

```txt
JWPLC/JWPLC-2.0.0/libraries/JWPLC_ModbusRTU/examples/ModbusRTU_CRC_Test/ModbusRTU_CRC_Test.ino
JWPLC/JWPLC-2.0.0/libraries/JWPLC_ModbusRTU/examples/ModbusRTU_Slave_HoldingRegisters/ModbusRTU_Slave_HoldingRegisters.ino
JWPLC/JWPLC-2.0.0/libraries/JWPLC_ModbusRTU/examples/ModbusRTU_Master_ReadHoldingRegisters/ModbusRTU_Master_ReadHoldingRegisters.ino
JWPLC/JWPLC-2.0.0/libraries/JWPLC_ModbusRTU/examples/ModbusRTU_Master_WriteSingleRegister/ModbusRTU_Master_WriteSingleRegister.ino
```

---

## 2. Modificar `JWPLC_GlobalPeripherals.h` opcionalmente

Archivo:

```txt
JWPLC/JWPLC-2.0.0/libraries/JWPLC_GlobalPeripherals/src/JWPLC_GlobalPeripherals.h
```

Agregar:

```cpp
#include <JWPLC_ModbusRTU.h>
```

junto a:

```cpp
#include <JWPLC_RS485.h>
```

Esto no es obligatorio para probar los ejemplos, porque los ejemplos incluyen `<JWPLC_ModbusRTU.h>` directamente.

---

## 3. Modificar `JWPLC_GlobalPeripherals/library.properties` si agregas el include anterior

Agregar `JWPLC_ModbusRTU` al depends:

```txt
depends=JW_RTC,JW_FRAM,JW_MatrixButtons,JW_SD,JWPLC_Ethernet,JWPLC_RS485,JWPLC_ModbusRTU
```

---

## 4. Pruebas en orden

Primero compilar:

```txt
ModbusRTU_CRC_Test
ModbusRTU_Slave_HoldingRegisters
ModbusRTU_Master_ReadHoldingRegisters
ModbusRTU_Master_WriteSingleRegister
```

Luego ejecutar:

```txt
ModbusRTU_CRC_Test
```

Debe mostrar:

```txt
Calculated CRC: 0xCDC5
Expected CRC:   0xCDC5
CRC OK
```

Después, para prueba real, necesitas dos equipos RS-485:

```txt
Equipo A: ModbusRTU_Slave_HoldingRegisters
Equipo B: ModbusRTU_Master_ReadHoldingRegisters
```

Ambos deben estar en:

```txt
19200 baud
SERIAL_8E1
```

---

## 5. Commit sugerido

```txt
alpha28: add Modbus RTU base library
```

Descripción:

```md
Adds the internal JWPLC_ModbusRTU base library.

Includes:
- Modbus CRC16.
- Basic RTU frame parsing.
- Slave/server holding register map.
- Function 0x03 Read Holding Registers.
- Function 0x06 Write Single Register.
- Function 0x10 Write Multiple Registers.
- Basic master read holding registers.
- Basic master write single register.
- CRC test example.
- Slave holding registers example.
- Master read example.
- Master write example.
```
