# JWPLC_Display

Librería interna del package **JWPLC ESP32** para controlar la pantalla TFT del **JWPLC Basic**.

Desde `2.0.0-alpha.25`, la API recomendada usa estilo de objeto con punto:

```cpp
JWPLC_Display.isReady();
JWPLC_Display.enterUserUI();
JWPLC_Display.setIdleTimeoutMs(8000);
```

La API anterior con namespace `JWPLCDisplay::` se mantiene temporalmente por compatibilidad interna/legacy, pero para sketches nuevos se recomienda usar `JWPLC_Display`.

---

## 1. Concepto general

`JWPLC_Display` forma parte del runtime del package JWPLC. En una placa compatible como **JWPLC Basic**, el display se inicializa automáticamente.

El usuario no necesita crear manualmente el objeto `Adafruit_ST7789`, configurar pines, iniciar SPI ni llamar a un `begin()` propio para el display base.

El objetivo es que el sketch pueda concentrarse en la lógica del PLC, mientras el package administra:

- Inicialización de la TFT ST7789.
- Pantalla IDLE automática.
- Pantalla USER para interfaz personalizada.
- Integración con botonera.
- Retorno automático USER -> IDLE.
- Indicadores laterales RUN, ERR, BUS y ETH.

---

## 2. Pantalla IDLE y pantalla USER

### IDLE

Es la pantalla automática base del JWPLC. Normalmente muestra información general del equipo, estado, indicadores, entradas/salidas y RTC si está disponible.

### USER

Es la pantalla personalizada del usuario. Se entra a USER cuando se presiona una tecla o cuando el sketch llama:

```cpp
JWPLC_Display.enterUserUI();
```

En USER el usuario puede dibujar directamente usando:

```cpp
auto &tft = JWPLC_Display.tft();
```

---

## 3. Uso rápido

```cpp
bool displayConfigured = false;

void setup()
{
    Serial.begin(115200);
}

void loop()
{
    if (!displayConfigured && JWPLC_Display.isReady())
    {
        displayConfigured = true;

        JWPLC_Display.setIdleReturnMode(IDLE_RETURN_TIMEOUT);
        JWPLC_Display.setIdleTimeoutMs(8000);
        JWPLC_Display.setUserRefreshPeriodMs(100);

        Serial.println("Display listo");
    }
}
```

---

## 4. Qué hace `isReady()`

`JWPLC_Display.isReady()` devuelve `true` cuando el runtime ya terminó de inicializar la pantalla.

En simple, significa:

> La TFT ya está inicializada y es seguro usar la API del display.

Por eso **no se recomienda** esto como variable global:

```cpp
auto &tft = JWPLC_Display.tft();  // No recomendado como global
```

Las variables globales se construyen antes del `setup()`, y en ese momento el display puede no estar listo.

Uso recomendado:

```cpp
if (JWPLC_Display.isReady())
{
    auto &tft = JWPLC_Display.tft();
    tft.print("Hola");
}
```

---

## 5. API principal

### `isReady()`

Indica si el display ya fue inicializado por el runtime.

```cpp
if (JWPLC_Display.isReady())
{
    Serial.println("Display listo");
}
```

### `isIdleMode()`

Indica si la pantalla está en IDLE.

```cpp
Serial.println(JWPLC_Display.isIdleMode() ? "IDLE" : "USER");
```

### `buttonsReady()`

Indica si la botonera está inicializada.

```cpp
if (JWPLC_Display.buttonsReady())
{
    Serial.println("Botonera lista");
}
```

### `forceRedraw()`

Solicita un redibujado de la pantalla.

```cpp
JWPLC_Display.forceRedraw();
```

Evita llamarlo muchas veces por segundo sin necesidad.

### `enterUserUI()`

Entra manualmente a USER.

```cpp
JWPLC_Display.enterUserUI();
```

### `goIdle()`

Vuelve manualmente a IDLE.

```cpp
JWPLC_Display.goIdle();
```

### `notifyActivity()`

Notifica actividad y reinicia el contador de inactividad.

```cpp
JWPLC_Display.notifyActivity();
```

### `setIdleReturnMode(mode)`

Configura cómo se vuelve desde USER hacia IDLE.

```cpp
JWPLC_Display.setIdleReturnMode(IDLE_RETURN_TIMEOUT);
```

### `setIdleTimeoutMs(timeoutMs)`

Configura el timeout de retorno a IDLE.

```cpp
JWPLC_Display.setIdleTimeoutMs(8000);
```

### `setIdleRefreshPeriodMs(ms)`

Configura el periodo de refresco del IDLE.

```cpp
JWPLC_Display.setIdleRefreshPeriodMs(1000);
```

### `setUserRefreshPeriodMs(ms)`

Configura cada cuánto se llama el callback USER de refresco.

```cpp
JWPLC_Display.setUserRefreshPeriodMs(100);
```

### `clearPendingInput()`

Limpia eventos pendientes de la botonera.

```cpp
JWPLC_Display.clearPendingInput();
```

Es útil al entrar a una pantalla nueva para evitar que una pulsación anterior se procese dos veces.

---

## 6. Modos de retorno a IDLE

Para evitar `JWPLC_DisplayClass::`, se exponen alias globales.

### `IDLE_RETURN_TIMEOUT`

Retorna automáticamente a IDLE luego de un tiempo sin actividad.

```cpp
JWPLC_Display.setIdleReturnMode(IDLE_RETURN_TIMEOUT);
JWPLC_Display.setIdleTimeoutMs(8000);
```

### `IDLE_RETURN_ESC_ONLY`

Solo vuelve a IDLE con ESC o acción equivalente.

```cpp
JWPLC_Display.setIdleReturnMode(IDLE_RETURN_ESC_ONLY);
```

### `IDLE_RETURN_DISABLED`

No vuelve automáticamente a IDLE.

```cpp
JWPLC_Display.setIdleReturnMode(IDLE_RETURN_DISABLED);
```

---

## 7. Acceso directo a la TFT

`JWPLC_Display.tft()` entrega una referencia al objeto interno `Adafruit_ST7789`.

Para usar constantes como `ST77XX_BLACK`, `ST77XX_WHITE` o funciones de Adafruit, incluye:

```cpp
#include <JWPLC_Display.h>
```

Ejemplo:

```cpp
#include <JWPLC_Display.h>

void drawScreen()
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

Para proyectos reales, la forma más ordenada de crear pantallas es usar callbacks.

### `jwplcUserDisplayEnterCallback()`

Se ejecuta al entrar a USER. Ideal para dibujar la pantalla completa una vez.

```cpp
extern "C" void jwplcUserDisplayEnterCallback()
{
    auto &tft = JWPLC_Display.tft();

    tft.fillScreen(ST77XX_BLACK);
    tft.setCursor(10, 20);
    tft.print("Pantalla USER");
}
```

### `jwplcUserDisplayRefreshCallback(...)`

Se ejecuta periódicamente mientras se está en USER.

```cpp
extern "C" void jwplcUserDisplayRefreshCallback(const JWPLC_IOState *io, const JWPLC_RTCState *rtc)
{
    (void)io;
    (void)rtc;

    auto &tft = JWPLC_Display.tft();

    tft.fillRect(10, 60, 160, 16, ST77XX_BLACK);
    tft.setCursor(10, 60);
    tft.print(millis());
}
```

### `jwplcUserDisplayExitCallback()`

Se ejecuta al salir de USER hacia IDLE.

```cpp
extern "C" void jwplcUserDisplayExitCallback()
{
    Serial.println("Saliendo de USER");
}
```

---

## 9. Indicadores RUN, ERR, BUS y ETH

La pantalla IDLE puede mostrar indicadores laterales.

```cpp
JWPLC_Display.setRunLed(true);
JWPLC_Display.setErrLed(false);
JWPLC_Display.setBusLed(true);
JWPLC_Display.setEthLed(false);
```

Lectura de estado:

```cpp
bool run = JWPLC_Display.runLed();
bool err = JWPLC_Display.errLed();
bool bus = JWPLC_Display.busLed();
bool eth = JWPLC_Display.ethLed();
```

Uso típico:

```cpp
JWPLC_Display.setRunLed(maquinaEnMarcha);
JWPLC_Display.setErrLed(hayError);
JWPLC_Display.setBusLed(modbusOk);
JWPLC_Display.setEthLed(ethernetOk);
```

---

## 10. Uso eficiente de la TFT ST7789

La TFT funciona por SPI. En JWPLC Basic, el bus SPI puede compartirse con FRAM, microSD y futuros periféricos como Ethernet. Por eso conviene dibujar con cuidado.

### 10.1 Evita `fillScreen()` dentro de refrescos rápidos

No recomendado:

```cpp
void loop()
{
    auto &tft = JWPLC_Display.tft();
    tft.fillScreen(ST77XX_BLACK);
    tft.print(millis());
}
```

Recomendado:

```cpp
auto &tft = JWPLC_Display.tft();

tft.fillRect(10, 50, 180, 20, ST77XX_BLACK);
tft.setCursor(10, 50);
tft.print(millis());
```

`fillScreen()` está bien al entrar a una pantalla nueva, no en cada actualización.

### 10.2 Redibuja solo las zonas que cambian

```cpp
tft.fillRect(90, 40, 80, 16, ST77XX_BLACK);
tft.setCursor(90, 40);
tft.print(valor);
```

### 10.3 Guarda el último valor dibujado

```cpp
static int lastValue = -1;

if (value != lastValue)
{
    lastValue = value;

    auto &tft = JWPLC_Display.tft();
    tft.fillRect(10, 40, 100, 16, ST77XX_BLACK);
    tft.setCursor(10, 40);
    tft.print(value);
}
```

### 10.4 Usa periodos de refresco razonables

```cpp
JWPLC_Display.setUserRefreshPeriodMs(100);  // rápido
JWPLC_Display.setUserRefreshPeriodMs(250);  // normal
JWPLC_Display.setUserRefreshPeriodMs(500);  // liviano
```

Para pantallas industriales, 100 ms a 500 ms suele ser suficiente.

### 10.5 Evita `String` en refrescos frecuentes

Evitar:

```cpp
String texto = "Valor: " + String(valor);
tft.print(texto);
```

Preferir:

```cpp
tft.print("Valor: ");
tft.print(valor);
```

O:

```cpp
char buffer[32];
snprintf(buffer, sizeof(buffer), "Valor: %d", valor);
tft.print(buffer);
```

### 10.6 No llames `setRotation()` continuamente

La rotación se configura al inicializar. Cambiarla en ejecución puede obligar a redibujar toda la pantalla.

### 10.7 Evita imágenes grandes en callbacks rápidos

Leer BMP desde microSD y dibujarlo consume SPI. Es mejor hacerlo al entrar a una pantalla o en una pantalla de carga, no dentro de un refresh rápido.

### 10.8 Mantén los callbacks cortos

Los callbacks USER deben terminar rápido para no afectar:

- Lectura de botonera.
- Escritura en microSD.
- FRAM SPI.
- Comunicación futura por Ethernet.
- Sensación de fluidez de la interfaz.

---

## 11. Recomendación para proyectos reales

Patrón recomendado:

```cpp
#include <JWPLC_Display.h>

bool displayConfigured = false;

void setup()
{
    Serial.begin(115200);
}

void loop()
{
    if (!displayConfigured && JWPLC_Display.isReady())
    {
        displayConfigured = true;

        JWPLC_Display.setIdleReturnMode(IDLE_RETURN_TIMEOUT);
        JWPLC_Display.setIdleTimeoutMs(8000);
        JWPLC_Display.setUserRefreshPeriodMs(250);
    }

    // Lógica principal del PLC.
}
```

Y dibujar mediante callbacks:

```cpp
extern "C" void jwplcUserDisplayEnterCallback()
{
    auto &tft = JWPLC_Display.tft();

    tft.fillScreen(ST77XX_BLACK);
    tft.setCursor(10, 20);
    tft.print("Mi pantalla");
}

extern "C" void jwplcUserDisplayRefreshCallback(const JWPLC_IOState *io, const JWPLC_RTCState *rtc)
{
    (void)io;
    (void)rtc;

    auto &tft = JWPLC_Display.tft();

    // Redibujar solo zonas necesarias.
}
```

---

## 12. Compatibilidad con `JWPLCDisplay::`

La API legacy se mantiene temporalmente:

```cpp
JWPLCDisplay::isReady();
JWPLCDisplay::enterUserUI();
JWPLCDisplay::display();
```

Para código nuevo se recomienda:

```cpp
JWPLC_Display.isReady();
JWPLC_Display.enterUserUI();
JWPLC_Display.tft();
```

---

## 13. Ejemplos incluidos

### `Display_DotAPI_Minimal`

Ejemplo mínimo sin `#include`.

Valida:

- Autoarranque del display.
- API con punto.
- Indicadores RUN, BUS y ETH.
- Cambio IDLE/USER.
- Retorno automático a IDLE.

### `Display_UserUI_Callbacks`

Ejemplo con `#include <JWPLC_Display.h>`.

Valida:

- `JWPLC_Display.tft()`.
- Callbacks USER.
- Dibujo directo con Adafruit.
- Redibujado parcial.
- Retorno automático a IDLE.

### `Display_Efficient_Redraw`

Ejemplo de buenas prácticas.

Valida:

- Redibujar solo zonas necesarias.
- Cachear últimos valores.
- Usar `snprintf()`.
- Evitar `fillScreen()` en refrescos.
- Controlar periodo de refresco.

---

## 14. Problemas comunes

### `JWPLC_Display` no existe

Verifica que estés usando una placa compatible del package JWPLC. Para dibujo directo incluye:

```cpp
#include <JWPLC_Display.h>
```

### `ST77XX_BLACK` no existe

Incluye:

```cpp
#include <JWPLC_Display.h>
```

### La pantalla no dibuja en `setup()`

Puede ocurrir si todavía no terminó el autoarranque. Usa `isReady()` y dibuja desde `loop()` o desde callbacks USER.

### La pantalla parpadea

Causas comunes:

- Uso repetido de `fillScreen()`.
- Redibujar toda la pantalla en cada ciclo.
- Refresh demasiado rápido.
- No limpiar áreas específicas antes de imprimir texto variable.

Solución:

- Usar `fillRect()` en zonas pequeñas.
- Cachear últimos valores.
- Subir `setUserRefreshPeriodMs()` a 100, 250 o 500 ms.

### Texto sobreescrito

Limpia el área antes de imprimir un valor variable:

```cpp
tft.fillRect(80, 40, 100, 16, ST77XX_BLACK);
tft.setCursor(80, 40);
tft.print(valor);
```

### Fallas ocasionales con SD mientras se actualiza pantalla

TFT y microSD usan SPI. Evita dibujar demasiado y abrir/cerrar archivos muy seguido. Mantén callbacks gráficos cortos.

---

## Estado

Documentación correspondiente a:

```text
JWPLC ESP32 2.0.0-alpha.25
JWPLC_Display 1.0.0
```
