# JWPLC_Display

Librería interna del package **JWPLC ESP32** para controlar la pantalla TFT ST7789 integrada del **JWPLC Basic**.

Desde `2.0.0-alpha.25`, la API recomendada usa estilo objeto con punto:

```cpp
JWPLC_Display.isReady();
JWPLC_Display.enterUserUI();
JWPLC_Display.setIdleTimeoutMs(8000);
JWPLC_Display.setEthLed(true);
```

La API legacy con namespace `JWPLCDisplay::` se mantiene temporalmente por compatibilidad interna, pero para sketches nuevos se recomienda usar `JWPLC_Display`.

---

## 1. Concepto general

`JWPLC_Display` forma parte del runtime del package JWPLC. En una placa compatible como **JWPLC Basic**, el display se inicializa automáticamente.

El usuario no necesita:

- Crear manualmente el objeto `Adafruit_ST7789`.
- Configurar pines de TFT.
- Configurar SPI para TFT.
- Llamar a un `begin()` propio del display.

El package administra:

- Inicialización de la TFT.
- Pantalla automática `IDLE`.
- Pantalla personalizada `USER`.
- Integración con botonera.
- Retorno `USER -> IDLE`.
- Indicadores laterales `RUN`, `ERR`, `BUS` y `ETH`.
- Protección del bus SPI compartido.

---

## 2. Pantallas IDLE y USER

### IDLE

`IDLE` es la pantalla automática base del JWPLC. Muestra información general del equipo, indicadores laterales, entradas/salidas y RTC cuando está disponible.

### USER

`USER` es la pantalla personalizada del usuario.

Se entra a `USER` cuando:

- Se presiona una tecla de la botonera.
- El sketch llama manualmente a:

```cpp
JWPLC_Display.enterUserUI();
```

En `USER`, el sketch puede dibujar con el objeto interno de Adafruit:

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
        JWPLC_Display.setUserRefreshPeriodMs(250);

        JWPLC_Display.setRunLed(true);
        JWPLC_Display.setErrLed(false);
        JWPLC_Display.setBusLed(false);
        JWPLC_Display.setEthLed(false);

        Serial.println("Display listo");
    }

    // Lógica principal del PLC.
}
```

---

## 4. ¿Qué hace `isReady()`?

`JWPLC_Display.isReady()` devuelve `true` cuando el runtime ya terminó de inicializar la TFT.

En simple:

> La pantalla ya está lista y es seguro usar la API del display.

No se recomienda guardar una referencia global a la TFT:

```cpp
auto &tft = JWPLC_Display.tft();  // No recomendado como global
```

Las variables globales se construyen antes del `setup()`, y en ese momento el display puede no estar inicializado.

Uso recomendado:

```cpp
if (JWPLC_Display.isReady())
{
    auto &tft = JWPLC_Display.tft();
    tft.print("Hola");
}
```

En proyectos reales, lo más ordenado es configurar el display una sola vez desde `loop()`:

```cpp
bool displayConfigured = false;

void loop()
{
    if (!displayConfigured && JWPLC_Display.isReady())
    {
        displayConfigured = true;
        JWPLC_Display.setIdleReturnMode(IDLE_RETURN_TIMEOUT);
    }
}
```

---

## 5. API principal

### `isReady()`

Indica si el display ya fue inicializado.

```cpp
if (JWPLC_Display.isReady())
{
    Serial.println("Display listo");
}
```

### `isIdleMode()`

Indica si la pantalla está en `IDLE`.

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

Solicita un redibujado.

```cpp
JWPLC_Display.forceRedraw();
```

Úsalo con moderación. No es buena práctica llamarlo muchas veces por segundo sin necesidad.

### `enterUserUI()`

Entra manualmente a `USER`.

```cpp
JWPLC_Display.enterUserUI();
```

### `goIdle()`

Vuelve manualmente a `IDLE`.

```cpp
JWPLC_Display.goIdle();
```

### `notifyActivity()`

Notifica actividad del usuario y reinicia el contador de inactividad.

```cpp
JWPLC_Display.notifyActivity();
```

Es útil si tu pantalla USER tiene interacciones propias que no pasan por la botonera estándar.

### `setIdleReturnMode(mode)`

Configura cómo se vuelve de `USER` a `IDLE`.

```cpp
JWPLC_Display.setIdleReturnMode(IDLE_RETURN_TIMEOUT);
```

### `setIdleTimeoutMs(timeoutMs)`

Configura el tiempo de inactividad para retorno automático.

```cpp
JWPLC_Display.setIdleTimeoutMs(8000);
```

### `setIdleRefreshPeriodMs(ms)`

Configura cada cuánto se refresca la pantalla `IDLE`.

```cpp
JWPLC_Display.setIdleRefreshPeriodMs(1000);
```

### `setUserRefreshPeriodMs(ms)`

Configura cada cuánto se llama `jwplcUserDisplayRefreshCallback()` mientras se está en `USER`.

```cpp
JWPLC_Display.setUserRefreshPeriodMs(250);
```

### `clearPendingInput()`

Limpia eventos pendientes de la botonera.

```cpp
JWPLC_Display.clearPendingInput();
```

Es útil al entrar a una pantalla nueva para evitar que una pulsación previa se procese dos veces.

---

## 6. Modos de retorno a IDLE

Hay tres modos disponibles. Para evitar escribir `JWPLC_DisplayClass::`, se exponen alias globales:

```cpp
IDLE_RETURN_TIMEOUT
IDLE_RETURN_ESC_ONLY
IDLE_RETURN_DISABLED
```

### 6.1 `IDLE_RETURN_TIMEOUT`

Vuelve automáticamente de `USER` a `IDLE` después de un tiempo sin actividad.

Uso recomendado para pantallas informativas o pantallas temporales.

```cpp
JWPLC_Display.setIdleReturnMode(IDLE_RETURN_TIMEOUT);
JWPLC_Display.setIdleTimeoutMs(8000);
```

Comportamiento:

- Entra a `USER`.
- Si no se presiona nada durante 8 segundos, vuelve a `IDLE`.
- Si se presiona una tecla, se reinicia el contador de inactividad.

Ejemplo:

```cpp
if (JWPLC_Display.isReady())
{
    JWPLC_Display.setIdleReturnMode(IDLE_RETURN_TIMEOUT);
    JWPLC_Display.setIdleTimeoutMs(8000);
}
```

### 6.2 `IDLE_RETURN_ESC_ONLY`

Vuelve a `IDLE` solo con `ESC` o con una acción equivalente definida por el sistema.

Uso recomendado para menús donde el usuario debe decidir cuándo salir.

```cpp
JWPLC_Display.setIdleReturnMode(IDLE_RETURN_ESC_ONLY);
```

Comportamiento:

- Entra a `USER`.
- No vuelve por timeout.
- Sale con `ESC`.

Ejemplo:

```cpp
if (JWPLC_Display.isReady())
{
    JWPLC_Display.setIdleReturnMode(IDLE_RETURN_ESC_ONLY);
}
```

### 6.3 `IDLE_RETURN_DISABLED`

Desactiva el retorno automático y el retorno por `ESC` gestionado por el display.

Uso recomendado solo cuando el sketch quiere controlar manualmente toda la navegación.

```cpp
JWPLC_Display.setIdleReturnMode(IDLE_RETURN_DISABLED);
```

Comportamiento:

- Entra a `USER`.
- No vuelve automáticamente.
- El sketch debe llamar manualmente a:

```cpp
JWPLC_Display.goIdle();
```

Ejemplo:

```cpp
if (JWPLC_Display.isReady())
{
    JWPLC_Display.setIdleReturnMode(IDLE_RETURN_DISABLED);
}

// Luego, cuando tu lógica decida salir:
JWPLC_Display.goIdle();
```

---

## 7. Acceso directo a la TFT

`JWPLC_Display.tft()` entrega una referencia al objeto interno `Adafruit_ST7789`.

Para usar constantes como `ST77XX_BLACK`, `ST77XX_WHITE`, `ST77XX_GREEN`, etc., incluye:

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

Para proyectos reales, la forma más ordenada de crear pantallas personalizadas es usar callbacks.

Los callbacks deben declararse con `extern "C"` porque el runtime los llama desde una capa C/C++ interna.

### 8.1 `jwplcUserDisplayEnterCallback()`

Se ejecuta **una sola vez** al entrar a `USER`.

Uso recomendado:

- Limpiar la pantalla.
- Dibujar títulos.
- Dibujar etiquetas fijas.
- Dibujar marcos, iconos o layout base.
- Preparar la pantalla inicial.

Ejemplo:

```cpp
extern "C" void jwplcUserDisplayEnterCallback()
{
    auto &tft = JWPLC_Display.tft();

    tft.fillScreen(ST77XX_BLACK);

    tft.setTextSize(2);
    tft.setTextColor(ST77XX_CYAN, ST77XX_BLACK);
    tft.setCursor(10, 10);
    tft.print("MENU");

    tft.setTextSize(1);
    tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
    tft.setCursor(10, 50);
    tft.print("Temperatura:");

    tft.setCursor(10, 70);
    tft.print("Estado:");
}
```

Qué **no** conviene hacer aquí:

- Abrir archivos grandes.
- Leer imágenes pesadas desde SD.
- Hacer operaciones bloqueantes largas.
- Consultar Ethernet de forma repetida.

Una lectura puntual puede funcionar, pero para sistemas robustos es mejor cachear estados en `loop()`.

### 8.2 `jwplcUserDisplayRefreshCallback(const JWPLC_IOState *io, const JWPLC_RTCState *rtc)`

Se ejecuta periódicamente mientras se está en `USER`.

La frecuencia se define con:

```cpp
JWPLC_Display.setUserRefreshPeriodMs(250);
```

Uso recomendado:

- Actualizar valores dinámicos.
- Redibujar solo áreas pequeñas.
- Mostrar entradas/salidas.
- Mostrar hora RTC.
- Mostrar variables ya cacheadas.

Ejemplo básico:

```cpp
extern "C" void jwplcUserDisplayRefreshCallback(const JWPLC_IOState *io, const JWPLC_RTCState *rtc)
{
    (void)io;
    (void)rtc;

    auto &tft = JWPLC_Display.tft();

    tft.fillRect(120, 50, 100, 16, ST77XX_BLACK);
    tft.setCursor(120, 50);
    tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
    tft.print(millis());
}
```

Si usas `io` y `rtc`, puedes leer estados que el runtime ya entrega al callback:

```cpp
extern "C" void jwplcUserDisplayRefreshCallback(const JWPLC_IOState *io, const JWPLC_RTCState *rtc)
{
    auto &tft = JWPLC_Display.tft();

    tft.fillRect(10, 100, 200, 16, ST77XX_BLACK);
    tft.setCursor(10, 100);

    if (rtc)
    {
        tft.print(rtc->hour);
        tft.print(":");
        tft.print(rtc->minute);
        tft.print(":");
        tft.print(rtc->second);
    }
}
```

> Nota: los nombres exactos de campos de `JWPLC_IOState` y `JWPLC_RTCState` dependen de la versión interna del core. Revisa el header correspondiente si necesitas acceso detallado.

### 8.3 `jwplcUserDisplayExitCallback()`

Se ejecuta al salir de `USER` hacia `IDLE`.

Uso recomendado:

- Guardar cambios.
- Liberar estados temporales.
- Imprimir debug.
- Resetear variables de navegación.

Ejemplo:

```cpp
extern "C" void jwplcUserDisplayExitCallback()
{
    Serial.println("Saliendo de USER hacia IDLE");
}
```

---

## 9. Regla importante para SPI compartido

En JWPLC Basic, la TFT comparte SPI con otros periféricos como:

- FRAM
- microSD
- Ethernet W5500

Desde `2.0.0-alpha.26`, `JWPLC_Display` protege sus operaciones internas usando el mutex SPI global.

Aun así, en sketches de usuario se debe respetar esta regla:

> Dentro de callbacks gráficos del display, evita consultar periféricos SPI como `JWPLC_Ethernet`, `JWPLC_SD` o `JWPLC_FRAM`.

Patrón recomendado:

1. En `loop()`, consulta periféricos SPI.
2. Guarda el resultado en variables globales simples.
3. En `jwplcUserDisplayRefreshCallback()`, solo dibuja esas variables.

Ejemplo recomendado:

```cpp
char ethStatusText[32] = "Unknown";

void loop()
{
    strncpy(ethStatusText, JWPLC_Ethernet.statusString(), sizeof(ethStatusText) - 1);
    ethStatusText[sizeof(ethStatusText) - 1] = '\0';
}

extern "C" void jwplcUserDisplayRefreshCallback(const JWPLC_IOState *io, const JWPLC_RTCState *rtc)
{
    (void)io;
    (void)rtc;

    auto &tft = JWPLC_Display.tft();

    tft.fillRect(80, 40, 150, 16, ST77XX_BLACK);
    tft.setCursor(80, 40);
    tft.print(ethStatusText);
}
```

No recomendado:

```cpp
extern "C" void jwplcUserDisplayRefreshCallback(const JWPLC_IOState *io, const JWPLC_RTCState *rtc)
{
    auto &tft = JWPLC_Display.tft();

    // Evitar: Ethernet también usa SPI.
    tft.print(JWPLC_Ethernet.statusString());
}
```

---

## 10. Indicadores RUN, ERR, BUS y ETH

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

## 11. Uso eficiente de la TFT ST7789

La TFT funciona por SPI. Para mantener fluida la interfaz y evitar consumo innecesario, conviene dibujar con cuidado.

### 11.1 Evita `fillScreen()` en refrescos rápidos

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

`fillScreen()` está bien en `jwplcUserDisplayEnterCallback()`, no en cada refresco.

### 11.2 Redibuja solo zonas que cambian

```cpp
tft.fillRect(90, 40, 80, 16, ST77XX_BLACK);
tft.setCursor(90, 40);
tft.print(valor);
```

### 11.3 Guarda el último valor dibujado

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

### 11.4 Usa periodos de refresco razonables

```cpp
JWPLC_Display.setUserRefreshPeriodMs(100);  // rápido
JWPLC_Display.setUserRefreshPeriodMs(250);  // normal
JWPLC_Display.setUserRefreshPeriodMs(500);  // liviano
```

Para pantallas industriales, 100 ms a 500 ms suele ser suficiente.

### 11.5 Evita `String` en refrescos frecuentes

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

### 11.6 Mantén callbacks cortos

Los callbacks USER deben terminar rápido para no afectar:

- Lectura de botonera.
- Escritura en microSD.
- Acceso a FRAM.
- Comunicación Ethernet.
- Sensación de fluidez de la interfaz.

---

## 12. Ejemplo completo: callbacks USER + retorno a IDLE

```cpp
#include <JWPLC_Display.h>

bool displayConfigured = false;
uint32_t counter = 0;

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

    counter++;
    delay(10);
}

extern "C" void jwplcUserDisplayEnterCallback()
{
    auto &tft = JWPLC_Display.tft();

    tft.fillScreen(ST77XX_BLACK);

    tft.setTextSize(2);
    tft.setTextColor(ST77XX_CYAN, ST77XX_BLACK);
    tft.setCursor(10, 10);
    tft.print("USER");

    tft.setTextSize(1);
    tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
    tft.setCursor(10, 50);
    tft.print("Counter:");
}

extern "C" void jwplcUserDisplayRefreshCallback(const JWPLC_IOState *io, const JWPLC_RTCState *rtc)
{
    (void)io;
    (void)rtc;

    auto &tft = JWPLC_Display.tft();

    tft.fillRect(90, 50, 120, 16, ST77XX_BLACK);
    tft.setCursor(90, 50);
    tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
    tft.print(counter);
}

extern "C" void jwplcUserDisplayExitCallback()
{
    Serial.println("Saliendo de USER");
}
```

---

## 13. Compatibilidad con `JWPLCDisplay::`

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

Puede ocurrir si todavía no terminó el autoarranque. Usa `isReady()` y dibuja desde `loop()` o callbacks USER.

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

### Ruido gráfico o pantalla corrupta al usar Ethernet/SD/FRAM

Causa probable:

- Acceso simultáneo al bus SPI.
- Consultas a periféricos SPI dentro de callbacks gráficos.
- Dibujo TFT demasiado pesado mientras hay tráfico SPI.

Solución:

- Actualizar a una versión donde `JWPLC_Display` use el mutex SPI global.
- Evitar llamadas a `JWPLC_Ethernet`, `JWPLC_SD` o `JWPLC_FRAM` dentro de callbacks de dibujo.
- Cachear estados en `loop()` y dibujar solo variables simples.

---

## 15. Ejemplos incluidos

### `Display_DotAPI_Minimal`

Ejemplo mínimo de API con punto.

Valida:

- Autoarranque del display.
- API `JWPLC_Display`.
- Indicadores RUN, BUS y ETH.
- Cambio IDLE/USER.
- Retorno automático a IDLE.

### `Display_UserUI_Callbacks`

Ejemplo con callbacks USER.

Valida:

- `jwplcUserDisplayEnterCallback()`.
- `jwplcUserDisplayRefreshCallback()`.
- `jwplcUserDisplayExitCallback()`.
- Dibujo directo con `JWPLC_Display.tft()`.
- Redibujado parcial.

### `Display_Efficient_Redraw`

Ejemplo de buenas prácticas.

Valida:

- Redibujar solo zonas necesarias.
- Cachear últimos valores.
- Usar `snprintf()`.
- Evitar `fillScreen()` en refrescos.
- Controlar periodo de refresco.

### `Display_Idle_Return_Modes`

Ejemplo nuevo recomendado para probar:

- `IDLE_RETURN_TIMEOUT`.
- `IDLE_RETURN_ESC_ONLY`.
- `IDLE_RETURN_DISABLED`.

---

## Validación alpha31

Para alpha31, `JWPLC_Display` se valida como parte del package completo instalado desde cero.

Pruebas recomendadas:

- autoarranque de pantalla `IDLE`;
- entrada a pantalla `USER`;
- retorno `USER -> IDLE` por timeout;
- retorno `USER -> IDLE` por `ESC`;
- modo `IDLE_RETURN_DISABLED`;
- indicadores `RUN`, `ERR`, `BUS` y `ETH`;
- ausencia de superposición de texto;
- ausencia de eventos pendientes de botonera al cambiar de pantalla;
- coexistencia SPI con Ethernet, SD y FRAM.

No se agregan features nuevas de display en alpha31.

---

## Estado

Documentación revisada para:

```text
JWPLC ESP32 2.0.0-alpha.31
JWPLC_Display 1.0.0
```

Alpha31 valida el comportamiento actual del display dentro del package completo. No introduce cambios funcionales grandes en esta librería.
```
