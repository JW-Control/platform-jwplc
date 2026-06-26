# JWPLC ESP32 v2.1.0-alpha.2

Pre-release interna del package Arduino **JW Control ESP32 Boards** para **JWPLC Basic**.

Esta alpha está enfocada en mejoras y correcciones de `JWPLC_Display`, refresco TFT y estados visuales de los indicadores `BUS` y `ETH`.

---

## Resumen

`v2.1.0-alpha.2` corrige y consolida el flujo de pantalla TFT integrado al runtime JWPLC:

- `setUserRefreshPeriodMs()` ahora controla realmente el refresco de la pantalla `USER`;
- las configuraciones principales de `JWPLC_Display` pueden hacerse desde `setup()`;
- los LEDs laterales del `IDLE` ya no son pisados por la inicialización interna de la TFT;
- `BUS` y `ETH` pasan a estados visuales extendidos;
- `BUS` puede funcionar automáticamente con actividad RS-485 / Modbus RTU;
- se agregan códigos internos de prueba en `JWPLC/Test_Codes`.

---

## Cambios principales

### JWPLC_Display

- Se corrigió la compuerta interna que mantenía el refresco de display fijo en `1000 ms`.
- `jwplcSystemDisplayHook()` ahora usa el periodo definido por `jwplcDisplayDesiredPeriod_ms()`.
- Se agregaron límites seguros al periodo de refresco:
  - mínimo: `20 ms`;
  - máximo: `60000 ms`;
  - fallback: `1000 ms`.
- `setUserRefreshPeriodMs()` fue validado en hardware a:
  - `100 ms`;
  - `20 ms`.

### Configuración desde setup()

Ahora se permite configurar desde `setup()`:

```cpp
JWPLC_Display.setIdleWakeMode(...);
JWPLC_Display.setIdleReturnMode(...);
JWPLC_Display.setIdleWakeButton(...);
JWPLC_Display.setIdleReturnButton(...);
JWPLC_Display.setIdleTimeoutMs(...);
JWPLC_Display.setIdleRefreshPeriodMs(...);
JWPLC_Display.setUserRefreshPeriodMs(...);
JWPLC_Display.setRunLed(...);
JWPLC_Display.setErrLed(...);
JWPLC_Display.setBusLed(...);
JWPLC_Display.setBusLedAuto(...);
JWPLC_Display.setEthLed(...);
JWPLC_Display.setEthLedAuto(...);
```

La inicialización interna de la TFT ya no reinicia configuración de transición, refresh ni LEDs hecha por el usuario.

### Indicadores BUS y ETH

`BUS` y `ETH` pasan a usar estados visuales extendidos:

| Color | Significado |
|---|---|
| Gris | Periférico no disponible, deshabilitado o no iniciado. |
| Negro / apagado | Disponible, pero inactivo. |
| Verde | OK o actividad reciente. |
| Rojo | Error. |

### BUS automático por RS-485 / Modbus RTU

Se agrega modo automático para el LED `BUS`:

```cpp
JWPLC_Display.setBusLedAuto(true);
```

En modo automático:

- gris: RS-485 no disponible o no iniciado;
- apagado: RS-485 iniciado sin actividad reciente;
- verde: actividad TX/RX reciente;
- rojo: error RS-485 o Modbus RTU.

### ETH automático

Se ajusta la semántica del LED `ETH`:

- gris: Ethernet no disponible/deshabilitado, por ejemplo en Basic Core;
- apagado: Ethernet disponible, pero sin link o sin inicio activo;
- verde: Ethernet OK;
- rojo: falla real de Ethernet.

---

## Cambios en librerías

### JWPLC_Display

- Setup-friendly para configuración de transición, refresh e indicadores.
- Nuevo modo `setBusLedAuto()`.
- Nuevo getter `busLedAuto()`.
- BUS y ETH con estado gris/apagado/verde/rojo.
- Refresco USER validado con benchmark y juegos TFT.

### JWPLC_RS485

- Se agrega tracking de actividad TX/RX:
  - `lastActivityMs()`;
  - `lastRxActivityMs()`;
  - `lastTxActivityMs()`;
  - `hasRecentActivity(windowMs)`.
- Se agrega hook weak:
  - `jwplcRs485ActivityCallback()`.
- `available()` / `read()` registran actividad RX.
- `write()` registra actividad TX.
- Este cambio permite que `JWPLC_Display` refleje actividad RS-485 en el LED `BUS`.

### JWPLC_ModbusRTU

- No requiere cambio directo de API en esta alpha.
- Se valida su uso sobre `JWPLC_RS485` para activar el LED `BUS` automático.
- Se validó comunicación Master/Slave entre dos JWPLC Basic.

---

## Códigos internos de prueba

Se agregan o mantienen códigos históricos en:

```txt
JWPLC/Test_Codes/
```

Pruebas usadas durante esta alpha:

- configuración manual de LEDs desde `setup()`;
- ETH automático desde `setup()`;
- BUS automático por RS-485;
- Modbus RTU Master con BUS automático;
- Modbus RTU Slave con BUS automático;
- benchmark/juegos TFT para refresco.

---

## Validación realizada

| Prueba | Resultado |
|---|---|
| Compilación con `jwplc_local:esp32:jwplcbasic` | OK |
| Confirmación de ruta local `jwplc_local` | OK |
| `setUserRefreshPeriodMs(100)` | OK |
| `setUserRefreshPeriodMs(20)` | OK |
| USER refresh sin depender de botones | OK |
| Wake IDLE -> USER | OK |
| Retorno USER -> IDLE con ESC | OK |
| LEDs manuales desde `setup()` | OK |
| ETH automático | OK |
| BUS automático por RS-485 | OK |
| Modbus RTU Master/Slave entre dos JWPLC Basic | OK |
| BUS parpadea en Master y Slave con tráfico real | OK |
| Estados gris/apagado/verde/rojo en BUS/ETH | OK |

---

## Ejemplo mínimo recomendado

```cpp
#include <Arduino.h>
#include <JWPLC_Display.h>

void setup()
{
    Serial.begin(115200);

    JWPLC_Display.setIdleWakeMode(IDLE_WAKE_ANY_BUTTON);
    JWPLC_Display.setIdleReturnMode(IDLE_RETURN_ESC_ONLY);

    JWPLC_Display.setIdleRefreshPeriodMs(1000);
    JWPLC_Display.setUserRefreshPeriodMs(100);

    JWPLC_Display.setRunLed(true);
    JWPLC_Display.setErrLed(false);
    JWPLC_Display.setBusLedAuto(true);
    JWPLC_Display.setEthLedAuto(true);
}

void loop()
{
    delay(10);
}
```

---

## Notas de compatibilidad

- `setBusLed(true/false)` sigue existiendo y fuerza modo manual.
- `setEthLed(true/false)` sigue existiendo y fuerza modo manual.
- `setBusLedAuto(true)` vuelve al modo automático de BUS.
- `setEthLedAuto(true)` vuelve al modo automático de ETH.
- `JWPLC_ModbusRTU.h` incluye internamente `JWPLC_RS485.h`.
- Para usar `JWPLC_ModbusRTU`, basta incluir:

```cpp
#include <JWPLC_ModbusRTU.h>
```

---

## Limitaciones / decisiones mantenidas

Esta alpha no cambia las decisiones principales del package:

- No se integra OpenPLC como runtime interno del package Arduino.
- No se define OTA.
- No se fija Flash Frequency final más allá de la configuración ya validada.
- No se publica `bootloader.bin` como definitivo.
- No se eliminan periféricos del autoload normal por velocidad.
- No se modifica la arquitectura general del package.

---

## Canal de instalación

Para validar esta alpha desde Boards Manager, usar el índice dev:

```txt
https://raw.githubusercontent.com/JW-Control/platform-jwplc/main/JWPLC/package_jwplc_index_dev.json
```

Para desarrollo local antes de publicar, usar:

```txt
jwplc_local:esp32:jwplcbasic
```

---

## Estado

```txt
Estado: Pre-release
Versión: v2.1.0-alpha.2
Tipo: alpha técnica
Tema: JWPLC_Display / TFT / BUS / ETH
Validación: hardware real JWPLC Basic
```

Esta alpha puede darse por cerrada si el PR asociado queda mergeado y el package se publica como pre-release en el canal dev.
