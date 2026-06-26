# JWPLC_Display

Librería interna del package **JWPLC ESP32** para controlar la pantalla TFT ST7789 integrada del **JWPLC Basic**.

A partir de **v2.1.0-alpha.2**, `JWPLC_Display` queda orientada a un uso más cómodo desde Arduino IDE:

- las configuraciones principales pueden hacerse directamente desde `setup()`;
- `setUserRefreshPeriodMs()` controla realmente el periodo de refresco de la pantalla `USER`;
- los indicadores `RUN`, `ERR`, `BUS` y `ETH` ya no son pisados por la inicialización interna de la TFT;
- `BUS` y `ETH` usan estados visuales extendidos: gris, apagado, verde y rojo;
- `BUS` puede reflejar automáticamente actividad RS-485 / Modbus RTU;
- `ETH` puede reflejar automáticamente el estado del W5500.

La API recomendada usa estilo objeto:

```cpp
#include <JWPLC_Display.h>

JWPLC_Display.setIdleWakeMode(IDLE_WAKE_ANY_BUTTON);
JWPLC_Display.setIdleReturnMode(IDLE_RETURN_ESC_ONLY);
JWPLC_Display.setUserRefreshPeriodMs(100);
JWPLC_Display.setBusLedAuto(true);
JWPLC_Display.setEthLedAuto(true);
```

La API legacy con namespace `JWPLCDisplay::` se mantiene temporalmente por compatibilidad interna, pero para sketches nuevos se recomienda usar `JWPLC_Display`.

---

## 1. Concepto general

`JWPLC_Display` forma parte del runtime del package JWPLC. En placas compatibles como **JWPLC Basic**, la TFT se inicializa automáticamente.

El usuario no necesita:

- crear manualmente el objeto `Adafruit_ST7789`;
- configurar pines de TFT;
- configurar SPI para TFT;
- llamar a un `begin()` propio del display.

El runtime administra:

- inicialización de la TFT;
- pantalla automática `IDLE`;
- pantalla personalizada `USER`;
- integración con botonera;
- retorno `USER -> IDLE`;
- indicadores laterales `PWR`, `RUN`, `ERR`, `BUS` y `ETH`;
- protección del bus SPI compartido;
- refresco periódico configurable;
- coexistencia con Ethernet, FRAM, SD y otros periféricos SPI.

---

## 2. Pantallas IDLE y USER

### IDLE

`IDLE` es la pantalla automática base del JWPLC. Muestra información general del equipo:

- indicadores laterales de estado;
- entradas digitales;
- salidas digitales;
- RTC cuando está disponible;
- estado de bus y Ethernet.

### USER

`USER` es la pantalla personalizada del usuario.

Se entra a `USER` cuando:

- se presiona una tecla, si el modo de wake lo permite;
- el sketch llama manualmente a `JWPLC_Display.enterUserUI()`.

En `USER`, el sketch puede dibujar usando la TFT interna:

```cpp
auto &tft = JWPLC_Display.tft();
```

---

## 3. Uso recomendado desde setup()

Desde **v2.1.0-alpha.2**, la configuración principal de `JWPLC_Display` puede hacerse directamente en `setup()`.

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

    JWPLC_Display.setIdleTimeoutMs(15000);

    JWPLC_Display.setRunLed(true);
    JWPLC_Display.setErrLed(false);

    JWPLC_Display.setBusLedAuto(true);
    JWPLC_Display.setEthLedAuto(true);
}

void loop()
{
    // Lógica normal del sketch.
}
```

Ya no es necesario hacer este tipo de configuración dentro de `loop()` con una bandera `displayConfigured`.

### Qué sí requiere que la TFT esté lista

El acceso directo a la TFT con `JWPLC_Display.tft()` debe hacerse cuando la pantalla ya esté inicializada. Lo recomendado es dibujar dentro de los callbacks:

```cpp
extern "C" void jwplcUserDisplayEnterCallback()
{
    auto &tft = JWPLC_Display.tft();
    tft.fillScreen(ST77XX_BLACK);
}
```

También puede usarse:

```cpp
if (JWPLC_Display.isReady())
{
    auto &tft = JWPLC_Display.tft();
    // Dibujar aquí.
}
```

No se recomienda guardar una referencia global a la TFT:

```cpp
auto &tft = JWPLC_Display.tft();  // No recomendado como global
```

---

## 4. API principal

### Estado

```cpp
JWPLC_Display.isReady();
JWPLC_Display.isIdleMode();
JWPLC_Display.buttonsReady();
```

| API | Descripción |
|---|---|
| `isReady()` | Indica si la TFT ya fue inicializada por el runtime. |
| `isIdleMode()` | Indica si la pantalla actual es `IDLE`. |
| `buttonsReady()` | Indica si la botonera ya está disponible. |

### Navegación

```cpp
JWPLC_Display.enterUserUI();
JWPLC_Display.goIdle();
JWPLC_Display.notifyActivity();
JWPLC_Display.clearPendingInput();
```

| API | Descripción |
|---|---|
| `enterUserUI()` | Entra manualmente a la pantalla `USER`. |
| `goIdle()` | Vuelve manualmente a `IDLE`. |
| `notifyActivity()` | Reinicia el contador de inactividad. |
| `clearPendingInput()` | Limpia eventos pendientes de botonera. |

### Refresco

```cpp
JWPLC_Display.setIdleRefreshPeriodMs(1000);
JWPLC_Display.setUserRefreshPeriodMs(100);
```

| API | Descripción |
|---|---|
| `setIdleRefreshPeriodMs(ms)` | Configura el periodo de refresco de `IDLE`. |
| `setUserRefreshPeriodMs(ms)` | Configura cada cuánto se llama `jwplcUserDisplayRefreshCallback()` en `USER`. |

`setUserRefreshPeriodMs()` fue validado en hardware a 100 ms y 20 ms dentro de la etapa `v2.1.0-alpha.2`.

El runtime aplica límites seguros internamente para evitar valores extremos.

---

## 5. Entrada y salida de USER

### Wake desde IDLE

```cpp
JWPLC_Display.setIdleWakeMode(IDLE_WAKE_ANY_BUTTON);
```

Modos disponibles:

```cpp
IDLE_WAKE_ANY_BUTTON
IDLE_WAKE_BUTTON_ONLY
IDLE_WAKE_DISABLED
```

| Modo | Comportamiento |
|---|---|
| `IDLE_WAKE_ANY_BUTTON` | Cualquier botón puede despertar de `IDLE` a `USER`. |
| `IDLE_WAKE_BUTTON_ONLY` | Solo el botón configurado despierta a `USER`. |
| `IDLE_WAKE_DISABLED` | El wake automático queda deshabilitado. |

Botón específico:

```cpp
JWPLC_Display.setIdleWakeButton(BTN_OK);
```

### Retorno desde USER

```cpp
JWPLC_Display.setIdleReturnMode(IDLE_RETURN_ESC_ONLY);
```

Modos disponibles:

```cpp
IDLE_RETURN_TIMEOUT
IDLE_RETURN_ESC_ONLY
IDLE_RETURN_DISABLED
IDLE_RETURN_BUTTON_ONLY
```

| Modo | Comportamiento |
|---|---|
| `IDLE_RETURN_TIMEOUT` | Vuelve a `IDLE` tras un tiempo sin actividad. |
| `IDLE_RETURN_ESC_ONLY` | Vuelve a `IDLE` solo con `ESC`. |
| `IDLE_RETURN_DISABLED` | El retorno automático queda deshabilitado. |
| `IDLE_RETURN_BUTTON_ONLY` | Vuelve con el botón configurado. |

Timeout:

```cpp
JWPLC_Display.setIdleTimeoutMs(15000);
```

Botón específico:

```cpp
JWPLC_Display.setIdleReturnButton(BTN_ESC);
```

---

## 6. Indicadores laterales

La pantalla `IDLE` muestra indicadores laterales:

```txt
PWR
RUN
ERR
BUS
ETH
```

`PWR` es gestionado por la pantalla IDLE. Los demás pueden controlarse desde `JWPLC_Display`.

### RUN

```cpp
JWPLC_Display.setRunLed(true);
JWPLC_Display.setRunLed(false);
bool run = JWPLC_Display.runLed();
```

### ERR

```cpp
JWPLC_Display.setErrLed(true);
JWPLC_Display.setErrLed(false);
bool err = JWPLC_Display.errLed();
```

### BUS manual

```cpp
JWPLC_Display.setBusLed(true);
JWPLC_Display.setBusLed(false);
bool bus = JWPLC_Display.busLed();
```

Llamar a `setBusLed(true/false)` desactiva el modo automático de BUS.

### BUS automático

```cpp
JWPLC_Display.setBusLedAuto(true);
bool busAuto = JWPLC_Display.busLedAuto();
```

En modo automático, `BUS` refleja actividad reciente de `JWPLC_RS485`, incluyendo tráfico usado por `JWPLC_ModbusRTU`.

Semántica visual:

| Estado | Significado |
|---|---|
| Gris | RS-485 no disponible o no iniciado. |
| Apagado / negro | RS-485 iniciado, sin actividad reciente. |
| Verde | Actividad TX/RX reciente. |
| Rojo | Error de RS-485 o error Modbus RTU reciente. |

El modo automático de BUS no inicializa RS-485. Para que pase de gris a apagado/verde, el usuario debe iniciar `JWPLC_RS485` o `JWPLC_ModbusRTU`.

```cpp
#include <JWPLC_ModbusRTU.h>

JWPLC_Display.setBusLedAuto(true);
JWPLC_ModbusRTU.begin(1, 115200, SERIAL_8N1);
```

### ETH manual

```cpp
JWPLC_Display.setEthLed(true);
JWPLC_Display.setEthLed(false);
bool eth = JWPLC_Display.ethLed();
```

Llamar a `setEthLed(true/false)` desactiva el modo automático de ETH.

### ETH automático

```cpp
JWPLC_Display.setEthLedAuto(true);
bool ethAuto = JWPLC_Display.ethLedAuto();
```

Semántica visual:

| Estado | Significado |
|---|---|
| Gris | Ethernet no disponible o deshabilitado por la variante. |
| Apagado / negro | Ethernet disponible, pero sin inicio/link/estado activo. |
| Verde | Ethernet OK. |
| Rojo | Error real de Ethernet. |

En `JWPLC Basic Core`, donde Ethernet puede estar deshabilitado, `ETH` debe mostrarse en gris.

---

## 7. Acceso directo a la TFT

`JWPLC_Display.tft()` entrega una referencia al objeto interno `Adafruit_ST7789`.

```cpp
#include <JWPLC_Display.h>

extern "C" void jwplcUserDisplayEnterCallback()
{
    auto &tft = JWPLC_Display.tft();

    tft.fillScreen(ST77XX_BLACK);
    tft.setTextColor(ST77XX_WHITE);
    tft.setCursor(10, 20);
    tft.print("Hola JWPLC");
}
```

`display()` es alias de `tft()`:

```cpp
auto &display = JWPLC_Display.display();
```

Se recomienda usar `tft()` para dejar claro que se está accediendo al objeto gráfico de Adafruit.

---

## 8. Callbacks USER

Los callbacks deben declararse con `extern "C"` porque el runtime los llama desde una capa C/C++ interna.

### `jwplcUserDisplayEnterCallback()`

Se ejecuta una vez al entrar a `USER`.

Uso recomendado:

- limpiar pantalla;
- dibujar títulos;
- dibujar layout base;
- preparar variables visuales.

```cpp
extern "C" void jwplcUserDisplayEnterCallback()
{
    auto &tft = JWPLC_Display.tft();

    tft.fillScreen(ST77XX_BLACK);
    tft.setTextSize(2);
    tft.setTextColor(ST77XX_CYAN, ST77XX_BLACK);
    tft.setCursor(10, 10);
    tft.print("MENU");
}
```

### `jwplcUserDisplayRefreshCallback(const JWPLC_IOState *io, const JWPLC_RTCState *rtc)`

Se ejecuta periódicamente mientras se está en `USER`.

La frecuencia se define con:

```cpp
JWPLC_Display.setUserRefreshPeriodMs(100);
```

Uso recomendado:

- actualizar valores dinámicos;
- redibujar solo áreas pequeñas;
- mostrar entradas/salidas;
- mostrar hora RTC;
- mostrar variables ya cacheadas.

```cpp
extern "C" void jwplcUserDisplayRefreshCallback(const JWPLC_IOState *io,
                                                const JWPLC_RTCState *rtc)
{
    auto &tft = JWPLC_Display.tft();

    tft.fillRect(10, 40, 120, 12, ST77XX_BLACK);
    tft.setCursor(10, 40);
    tft.print("DI: ");
    tft.print(io ? io->di_logical_bank0 : 0);
}
```

### `jwplcUserDisplayExitCallback()`

Se ejecuta al salir de `USER` hacia `IDLE`.

```cpp
extern "C" void jwplcUserDisplayExitCallback()
{
    Serial.println("Saliendo de USER");
}
```

---

## 9. Ejemplo mínimo setup-friendly

```cpp
#include <Arduino.h>
#include <JWPLC_Display.h>

static constexpr uint32_t USER_REFRESH_MS = 100;

extern "C" void jwplcUserDisplayEnterCallback()
{
    auto &tft = JWPLC_Display.tft();

    tft.fillScreen(ST77XX_BLACK);
    tft.setTextSize(2);
    tft.setTextColor(ST77XX_CYAN, ST77XX_BLACK);
    tft.setCursor(20, 25);
    tft.print("USER");
}

extern "C" void jwplcUserDisplayRefreshCallback(const JWPLC_IOState *io,
                                                const JWPLC_RTCState *rtc)
{
    (void)io;
    (void)rtc;

    static uint32_t frames = 0;
    frames++;

    auto &tft = JWPLC_Display.tft();

    tft.fillRect(20, 70, 160, 16, ST77XX_BLACK);
    tft.setTextSize(1);
    tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
    tft.setCursor(20, 70);
    tft.print("Frames: ");
    tft.print(frames);
}

void setup()
{
    Serial.begin(115200);

    JWPLC_Display.setIdleWakeMode(IDLE_WAKE_ANY_BUTTON);
    JWPLC_Display.setIdleReturnMode(IDLE_RETURN_ESC_ONLY);

    JWPLC_Display.setIdleRefreshPeriodMs(1000);
    JWPLC_Display.setUserRefreshPeriodMs(USER_REFRESH_MS);

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

## 10. Ejemplos y códigos de prueba

Ejemplos de usuario final dentro de la librería:

```txt
examples/Display_FlappyBird/
examples/Display_Tetris/
```

Códigos internos de validación histórica:

```txt
JWPLC/Test_Codes/
```

Pruebas usadas durante `v2.1.0-alpha.2`:

- configuración de LEDs desde `setup()`;
- ETH automático;
- BUS automático por RS-485;
- Modbus RTU master/slave entre dos JWPLC Basic;
- refresco USER a 100 ms y 20 ms;
- retorno `USER -> IDLE` con `ESC`.

---

## 11. Reglas de coexistencia SPI

La TFT comparte SPI con:

- Ethernet W5500;
- FRAM;
- microSD.

Regla recomendada para sketches avanzados:

1. consultar periféricos SPI desde `loop()`;
2. guardar resultados en variables simples;
3. dibujar en callbacks usando variables cacheadas.

Evita consultar `JWPLC_Ethernet`, `JWPLC_SD` o `JWPLC_FRAM` de forma repetida dentro de callbacks gráficos.

---

## 12. Estado

Documentación actualizada para:

```txt
JWPLC ESP32 2.1.0-alpha.2
JWPLC_Display
```

Cambios principales:

- configuración setup-friendly;
- refresh USER dinámico;
- indicadores `BUS` y `ETH` con estados extendidos;
- BUS automático por actividad RS-485 / Modbus RTU;
- pruebas de benchmark y juegos para validar refresco TFT.
