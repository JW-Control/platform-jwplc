# JW Libraries snapshot

Origen:

```txt
JW-Control/JW-Libraries
commit=9fea488fe95bb5db170c4cf3bb9d781bdae88487
```

| Librería | Versión | Ruta origen |
|---|---:|---|
| `JW_RS485` | `0.1.1` | `libraries/JW_RS485` |
| `JW_ModbusRTU` | `0.1.1` | `libraries/JW_ModbusRTU` |
| `JW_DWIN` | `0.2.0` | `libraries/JW_DWIN` |
| `JW_RTC` | `1.0.3` | `libraries/JW_RTC` |
| `JW_FRAM` | `1.0.4` | `libraries/JW_FRAM` |
| `JW_SD` | `1.0.3` | `libraries/JW_SD` |
| `JW_MatrixButtons` | `1.0.7` | `libraries/JW_MatrixButtons` |

Destino:

```txt
JW-Control/platform-jwplc
base=v2.1.0-alpha.17/JWPLC-Sim/main
sync=v2.1.0-alpha.17/JWPLC-Sim/feature/sync-jw-libraries
path=JWPLC/2.1.0/libraries
```

Modo de cada librería en el package:

```txt
JW_RS485 0.1.1: fuente
JW_ModbusRTU 0.1.1: fuente
JW_DWIN 0.2.0: fuente
JW_RTC 1.0.3: fuente
JW_FRAM 1.0.4: MODO FUENTE (src cambió: regenerar src/esp32/*.a al cierre)
JW_SD 1.0.3: precompilada (src sin cambios)
JW_MatrixButtons 1.0.7: MODO FUENTE (src cambió: regenerar src/esp32/*.a al cierre)
```

Nota:

Este archivo fue generado automáticamente desde JW-Libraries para registrar el snapshot de librerías incluido en el package JWPLC.
