# JWPLC_GlobalPeripherals

Capa interna del package **JWPLC ESP32** que expone los periféricos globales del **JWPLC Basic** y sus helpers comunes.

Esta librería no reemplaza a `JWPLC_Display`, `JWPLC_Ethernet`, `JWPLC_RS485`, `JWPLC_ModbusRTU` ni a las librerías `JW_*`. Su función es unirlas con el runtime del package para que el sketch disponga de objetos globales y una inicialización coherente.

## Objetos globales

La capa declara:

```cpp
JWPLC_RTC
JWPLC_FRAM
JWPLC_Buttons
JWPLC_SD
```

También incluye las APIs del package para:

```cpp
JWPLC_Ethernet
JWPLC_RS485
JWPLC_ModbusRTU
```

La inicialización automática que corresponda a cada periférico la coordina el runtime de la plataforma. La presencia de un objeto global no significa que todos los periféricos deban arrancar con una configuración arbitraria: por ejemplo, RS-485 requiere que la aplicación defina los parámetros de comunicación adecuados.

## Botonera

IDs físicos expuestos por el package:

```text
BTN_LEFT
BTN_UP
BTN_RIGHT
BTN_ESC
BTN_OK
BTN_DOWN
```

Helpers globales:

```cpp
JWPLCButtons::begin();
JWPLCButtons::isReady();
JWPLCButtons::anyPressed();
JWPLCButtons::escPressed();
JWPLCButtons::anyPressedOrRepeated();
JWPLCButtons::clearPendingInput();
```

La botonera es un periférico base del JWPLC; no pertenece conceptualmente a la librería Display aunque la UI la utilice.

## microSD

Helpers globales:

```cpp
JWPLCSD::begin();
JWPLCSD::isEnabled();
JWPLCSD::isReady();
JWPLCSD::isCardPresent();
JWPLCSD::lastErrorString();
```

La API completa de almacenamiento permanece en `JWPLC_SD` / `JW_SD`, según la capa utilizada por el package.

## Includes automáticos

En el perfil JWPLC Basic los periféricos principales quedan disponibles sin obligar al usuario a repetir una cadena de includes de bajo nivel. Para código reutilizable o librerías propias se recomienda incluir explícitamente el header del periférico que se usa.

## Separación de responsabilidades

```text
JWPLC_GlobalPeripherals
├── objetos globales y helpers comunes
├── integración con autoload del package
└── IDs compartidos de botonera

JWPLC_Display
└── TFT, IDLE/USER y diagnósticos visuales

JWPLC_Ethernet
└── W5500 y runtime de red

JWPLC_RS485
└── transporte RS-485

JWPLC_ModbusRTU
└── protocolo Modbus RTU

JW_* / JW_Libraries
└── drivers reutilizables mantenidos en su repositorio propio
```

## Estado

```text
JWPLC ESP32 2.1.0-alpha.6
JWPLC_GlobalPeripherals 1.0.0
```

Este README documenta una capa interna de integración. Las características específicas de cada periférico deben consultarse en su README correspondiente.
