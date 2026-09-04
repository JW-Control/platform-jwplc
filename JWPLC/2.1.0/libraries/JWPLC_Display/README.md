# JWPLC_Display

Librería interna del package **JWPLC ESP32** para la TFT ST7789 integrada del **JWPLC Basic**.

`JWPLC_Display` gestiona la pantalla automática `IDLE`, la pantalla `USER`, la botonera y el acceso coordinado al bus SPI compartido. En `v2.1.0-alpha.6` también concentra la presentación de los diagnósticos laterales `ERR`, `BUS` y `ETH`.

## Uso recomendado

En JWPLC Basic la TFT se inicializa automáticamente. El sketch no necesita crear `Adafruit_ST7789`, configurar sus pines ni llamar un `begin()` propio.

```cpp
#include <JWPLC_Display.h>

void setup()
{
    JWPLC_Display.setIdleWakeMode(IDLE_WAKE_ANY_BUTTON);
    JWPLC_Display.setIdleReturnMode(IDLE_RETURN_ESC_ONLY);
    JWPLC_Display.setUserRefreshPeriodMs(100);

    JWPLC_Display.setRunLed(true);
    JWPLC_Display.setErrCode("");
    JWPLC_Display.setBusLedAuto(true);
    JWPLC_Display.setEthLedAuto(true);
}
```

La API recomendada es el objeto global `JWPLC_Display`. La API histórica `JWPLCDisplay::` se conserva por compatibilidad interna.

## Pantallas IDLE y USER

### IDLE

La pantalla base muestra:

- indicadores `PWR`, `RUN`, `ERR`, `BUS` y `ETH`;
- entradas `I0.0..I0.7`;
- salidas `Q0.0..Q0.7`;
- RTC cuando está disponible.

### USER

`USER` queda disponible para interfaces propias del sketch. Puede abrirse por botonera o mediante:

```cpp
JWPLC_Display.enterUserUI();
```

El acceso directo a la TFT es:

```cpp
auto &tft = JWPLC_Display.tft();
```

También existe `display()` como alias.

No se recomienda guardar una referencia global a la TFT. El dibujo debe realizarse cuando `isReady()` sea verdadero o, preferentemente, dentro de los callbacks USER.

## Navegación y refresco

```cpp
JWPLC_Display.setIdleWakeMode(IDLE_WAKE_ANY_BUTTON);
JWPLC_Display.setIdleWakeButton(BTN_OK);

JWPLC_Display.setIdleReturnMode(IDLE_RETURN_ESC_ONLY);
JWPLC_Display.setIdleReturnButton(BTN_ESC);
JWPLC_Display.setIdleTimeoutMs(15000);

JWPLC_Display.setIdleRefreshPeriodMs(1000);
JWPLC_Display.setUserRefreshPeriodMs(100);
```

Modos de entrada a USER:

```text
IDLE_WAKE_ANY_BUTTON
IDLE_WAKE_BUTTON_ONLY
IDLE_WAKE_DISABLED
```

Modos de retorno a IDLE:

```text
IDLE_RETURN_TIMEOUT
IDLE_RETURN_ESC_ONLY
IDLE_RETURN_DISABLED
IDLE_RETURN_BUTTON_ONLY
```

El runtime usa refresco parcial y cachés internas. Una llamada periódica no implica necesariamente un redibujado completo.

## Indicadores laterales

### PWR y RUN

`PWR` pertenece a la pantalla IDLE. `RUN` puede controlarse desde la aplicación:

```cpp
JWPLC_Display.setRunLed(true);
bool run = JWPLC_Display.runLed();
```

### ERR: error de aplicación

En Alpha6, `ERR` queda reservado para la aplicación del usuario. Los diagnósticos internos de Ethernet o Modbus se muestran en sus indicadores propios y **no deben apropiarse de ERR**.

API recomendada:

```cpp
bool ok = JWPLC_Display.setErrCode("A01");
const char *code = JWPLC_Display.errCode();
```

Formato:

- 1 a 4 caracteres `A-Z` o `0-9`;
- minúsculas se normalizan a mayúsculas;
- `nullptr`, cadena vacía o `0`, `00`, `000`, `0000` significan sin error;
- una entrada inválida devuelve `false` y conserva el estado anterior.

Ejemplos:

```cpp
JWPLC_Display.setErrCode("1");
JWPLC_Display.setErrCode("A01");
JWPLC_Display.setErrCode("TEMP");
JWPLC_Display.setErrCode("0000"); // limpia ERR
```

La API legacy sigue disponible:

```cpp
JWPLC_Display.setErrLed(true);  // rojo, sin texto
JWPLC_Display.setErrLed(false); // apagado
bool err = JWPLC_Display.errLed();
```

### BUS automático

```cpp
JWPLC_Display.setBusLedAuto(true);
bool automatico = JWPLC_Display.busLedAuto();
```

`BUS` refleja el estado de `JWPLC_RS485` y, cuando se usa, de `JWPLC_ModbusRTU`. El modo automático **no inicia RS-485**.

| Código | Significado |
|---|---|
| `DIS` | RS-485 deshabilitado/no disponible. |
| `INI` | RS-485 aún no iniciado. |
| `SER` | Configuración/estado serial inválido. |
| `SID` | Slave ID Modbus inválido. |
| `MAP` | Mapa de registros inválido. |
| `TMO` | Timeout Modbus. |
| `CRC` | CRC inválido. |
| `EXC` | Respuesta Modbus Exception. |
| `RSP` | Respuesta Modbus inválida. |
| `OVF` | Overflow de buffer. |
| `FUN` | Función Modbus no soportada. |
| `---` | Sin error de bus. |

Semántica visual:

- gris: `DIS`;
- negro: `INI` o bus listo sin actividad;
- verde: `---` con actividad TX/RX reciente;
- rojo: código de error.

Para usarlo:

```cpp
JWPLC_Display.setBusLedAuto(true);
JWPLC_ModbusRTU.begin(1, 115200, SERIAL_8N1);
```

El control manual sigue disponible y desactiva el modo automático:

```cpp
JWPLC_Display.setBusLed(true);
JWPLC_Display.setBusLed(false);
```

### ETH automático

```cpp
JWPLC_Display.setEthLedAuto(true);
bool automatico = JWPLC_Display.ethLedAuto();
```

`ETH` consume `JWPLC_Ethernet.diagnosticCode()`.

| Código | Significado |
|---|---|
| `DIS` | Ethernet deshabilitado por la variante. |
| `INI` | Runtime Ethernet aún no iniciado. |
| `PHY` | Sondeo/preparación del W5500. |
| `LNK` | Sin link físico RJ45. |
| `DHC` | Adquisición o mantenimiento DHCP en curso; rojo sólo ante fallo real. |
| `HW` | W5500 no detectado. |
| `IP` | Configuración/IP inválida. |
| `SPI` | Timeout de arbitraje SPI. |
| `---` | Ethernet operativo. |

Durante `renew/rebind` DHCP, si el lease vigente sigue válido, `DHC` puede mostrarse manteniendo el estado visual verde. La operación de red útil no se invalida sólo por estar renovando el lease.

Semántica visual:

- gris: `DIS`;
- negro: `INI`, `PHY` o `LNK`;
- verde: `---`, o mantenimiento DHCP con lease todavía válido;
- rojo: `HW`, `IP`, `SPI` o fallo DHCP real.

El control manual sigue disponible y desactiva el modo automático:

```cpp
JWPLC_Display.setEthLed(true);
JWPLC_Display.setEthLed(false);
```

## Callbacks USER

Los callbacks deben declararse con `extern "C"`:

```cpp
extern "C" void jwplcUserDisplayEnterCallback()
{
    auto &tft = JWPLC_Display.tft();
    tft.fillScreen(ST77XX_BLACK);
}

extern "C" void jwplcUserDisplayRefreshCallback(
    const JWPLC_IOState *io,
    const JWPLC_RTCState *rtc)
{
    // Actualizar únicamente regiones dinámicas.
}

extern "C" void jwplcUserDisplayExitCallback()
{
}
```

También existe el gate opcional `jwplcUserDisplayRefreshNeededCallback()` usado por capas de UI avanzadas para evitar adquirir SPI cuando no hay nada que redibujar.

## Coexistencia SPI

La TFT comparte SPI con:

- W5500;
- FRAM;
- microSD.

El runtime utiliza el mutex SPI del ecosistema JWPLC. En callbacks gráficos no conviene iniciar operaciones largas o consultar repetidamente otros periféricos SPI.

Patrón recomendado:

1. leer periféricos desde `loop()` o una tarea no gráfica;
2. guardar resultados simples;
3. dibujar valores cacheados en el callback USER.

## Precompilación

`JWPLC_Display` usa `precompiled=full` para el perfil ESP32 del package. En el cierre de Alpha6 se regeneró el archive a partir de los dos objetos fuente actuales y se validó paridad estructural source/archive:

- dos TUs Display en build source;
- cero TUs Display en build precompilado;
- miembros del archive byte-idénticos a los objetos source;
- mismo conjunto de símbolos;
- misma RAM;
- diferencia de flash explicada únicamente por padding/alineamiento del linker.

## Validación Alpha6

Se validó físicamente:

- `ERR` alfanumérico y compatibilidad legacy;
- `BUS` con timeout Modbus y recuperación al iniciar el peer;
- `ETH` con link, DHCP, desconexión/reconexión y recuperación;
- mantenimiento DHCP T1/T2 sin bloquear el runtime;
- coexistencia TFT/W5500/FRAM/microSD sobre SPI;
- archive final precompilado de Display.

## Estado

```text
JWPLC ESP32 2.1.0-alpha.6
JWPLC_Display 1.0.1
```

Las APIs anteriores se mantienen salvo indicación explícita. Alpha6 amplía diagnóstico y comportamiento cooperativo sin retirar periféricos del autoload normal.
