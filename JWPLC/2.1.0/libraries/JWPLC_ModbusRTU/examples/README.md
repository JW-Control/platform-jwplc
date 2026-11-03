# Ejemplos JWPLC_ModbusRTU

Serie corta recomendada para taller:

```text
01.ModbusRTU_Slave_Holding
02.ModbusRTU_Master_Read
03.ModbusRTU_Master_Write
```

Configuración común:

```text
Slave ID 2
115200 8N1
Master local ID 247
```

En 2.1.0-alpha.17 se retiraron los duplicados, las pruebas internas (CRC, tool, native/extern, gate cooperativo) y
Remote I/O, que reemplaza `JWPLC_Expansion` (expansión RS-485 del Runtime).
