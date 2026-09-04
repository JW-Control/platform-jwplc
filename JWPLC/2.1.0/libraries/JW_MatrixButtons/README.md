# JW_MatrixButtons

**Versión actual:** 1.0.5

Librería reusable para leer botones en Arduino/ESP32 con **debounce**, eventos `PRESS / RELEASE / REPEAT`, lectura de estado y helpers para navegación tipo HMI/PLC.

Puede usarse de dos formas:

1. como librería genérica `JW_MatrixButtons` en cualquier proyecto compatible;
2. como botonera global ya integrada al runtime del **JWPLC Basic** mediante el objeto `JWPLC_Buttons`.

---

## Características

- matriz R×C hasta 8×8;
- botones directos 1×N hasta 32 entradas;
- debounce por tecla;
- eventos:
  - `EV_PRESS`;
  - `EV_RELEASE`;
  - `EV_REPEAT`;
- repeat configurable por botón;
- perfil de aceleración configurable;
- estado físico mediante `isDown(id)`;
- latches consumibles mediante `pressed(id)` / `released(id)`;
- inspección de eventos del último scan mediante `eventCount()` / `getEvent()`;
- helper `applyAxis()`;
- limpieza de pendientes;
- task opcional en ESP32.

---

# Uso en JWPLC Basic

## Objeto global recomendado

En JWPLC Basic la botonera física ya está creada por el package:

```cpp
JWPLC_Buttons
```

IDs disponibles:

```text
BTN_LEFT
BTN_UP
BTN_RIGHT
BTN_ESC
BTN_OK
BTN_DOWN
```

La aplicación puede usar directamente:

```cpp
if (JWPLC_Buttons.pressed(BTN_OK))
{
    Serial.println("OK");
}

if (JWPLC_Buttons.isDown(BTN_UP))
{
    // UP físicamente sostenido
}
```

Para sketches JWPLC se recomienda la API de objeto `JWPLC_Buttons` en lugar de helpers namespace internos.

---

## No llamar `update()` manualmente en JWPLC Basic

El package mantiene un task de escaneo propio para la botonera integrada.

Por lo tanto, en un sketch normal de JWPLC Basic **no** se debe hacer:

```cpp
JWPLC_Buttons.update();
```

ni iniciar un segundo task con:

```cpp
JWPLC_Buttons.startTask(...);
```

Hacer doble scan introduce dos productores sobre la misma instancia y no forma parte del contrato del autoload JWPLC.

Patrón correcto:

```cpp
void loop()
{
    if (JWPLC_Buttons.pressed(BTN_LEFT))
    {
        // Acción
    }

    delay(5);
}
```

---

## Matriz física del JWPLC Basic

La integración actual del package utiliza:

```text
             GPIO35     GPIO34     GPIO36
             COL0       COL1       COL2
GPIO12 ROW0  LEFT       UP         RIGHT
GPIO2  ROW1  ESC        OK         DOWN
```

Los GPIO 34/35/36 del ESP32 son entradas only y no ofrecen pull-up/pull-down interno general. El package configura las columnas de esta matriz como `INPUT` y deja la topología eléctrica a la placa JWPLC.

No reconfigurar estos pines desde el sketch mientras se usa la botonera integrada.

---

# Semántica de eventos

## `isDown(id)`

Devuelve el estado físico/debounced actual.

```cpp
bool held = JWPLC_Buttons.isDown(BTN_UP);
```

No consume eventos.

---

## `pressed(id)`

`pressed()` es **latcheado y consumible**.

Si se generó un `PRESS`, queda pendiente hasta que alguien lo lea.

```cpp
if (JWPLC_Buttons.pressed(BTN_OK))
{
    // Consume un PRESS pendiente de OK
}
```

Una segunda lectura inmediata normalmente devuelve `false` porque el evento anterior ya fue consumido.

---

## `released(id)`

Misma semántica consumible para un flanco de liberación:

```cpp
if (JWPLC_Buttons.released(BTN_OK))
{
}
```

---

## `eventCount()` y `getEvent()`

Estos métodos inspeccionan la cola de eventos producida por el scan actual.

```cpp
JW_MatrixButtons::BtnEvent ev;

for (uint8_t i = 0; i < JWPLC_Buttons.eventCount(); ++i)
{
    if (JWPLC_Buttons.getEvent(i, ev))
    {
        Serial.print("id=");
        Serial.print(ev.id);
        Serial.print(" type=");
        Serial.print((int)ev.type);
        Serial.print(" mult=");
        Serial.print(ev.mult);
        Serial.print(" held_ms=");
        Serial.println(ev.held_ms);
    }
}
```

Tipos:

```cpp
JW_MatrixButtons::EV_PRESS
JW_MatrixButtons::EV_RELEASE
JW_MatrixButtons::EV_REPEAT
```

`getEvent()` no reemplaza los latches `pressed()` / `released()`; son dos mecanismos complementarios.

---

## Repeat en JWPLC Basic

El package habilita repeat para:

```text
LEFT
UP
RIGHT
DOWN
```

y lo deja deshabilitado para:

```text
OK
ESC
```

Perfil integrado actual:

```text
initial delay: 220 ms
thresholds:    6 / 12 / 20
steps:         1 / 1 / 1 / 1
delays:        120 / 90 / 70 / 50 ms
```

La existencia de `EV_REPEAT` no significa que `pressed()` se repita. `pressed()` representa el flanco de pulsación; los repeats se consultan por la cola/eventos o mediante helpers que los procesen.

---

## Display y aplicación no compiten por `pressed()` en Alpha8

Desde `v2.1.0-alpha.8`, `JWPLC_Display` no usa los latches de aplicación `pressed()` / `released()` para su navegación interna.

El Display observa el estado físico y mantiene su propio snapshot de flancos.

Esto permite:

```text
ESC físico
├── Display detecta su flanco y retorna a IDLE
└── sketch sigue pudiendo leer JWPLC_Buttons.pressed(BTN_ESC)
```

El gate físico Alpha8 validó los seis botones y confirmó esta independencia.

---

## Limpieza de pendientes

```cpp
JWPLC_Buttons.clearPendingPresses();
JWPLC_Buttons.clearPendingReleases();
JWPLC_Buttons.clearPendingRepeats();
JWPLC_Buttons.clearEventQueue();
JWPLC_Buttons.clearPendingInput();
```

Estas funciones limpian eventos/latches pendientes. No modifican el estado físico actual del botón.

Son útiles al cambiar de contexto de aplicación, por ejemplo:

```cpp
mode = MODE_EDIT;
JWPLC_Buttons.clearPendingInput();
```

En Alpha8, `JWPLC_Display` ya no borra los latches del usuario al entrar/salir de USER como mecanismo normal de navegación.

---

# Uso genérico de la librería

Fuera del runtime JWPLC, el usuario crea su propia instancia y decide si escanea desde `loop()` o desde el task opcional.

---

## Modo matriz R×C

### Inicialización

```cpp
bool begin(const uint8_t *rowPins, uint8_t nRows,
           const uint8_t *colPins, uint8_t nCols,
           const BtnMapItem *map, uint8_t mapLen,
           uint8_t buttonCount,
           bool invertLogic = false,
           uint32_t debounceMs = 35);
```

Ejemplo:

```cpp
#include <Arduino.h>
#include <JW_MatrixButtons.h>

static const uint8_t ROWS[] = {25, 26};
static const uint8_t COLS[] = {35, 34, 39, 36};

enum BtnId : uint8_t
{
    BTN_A,
    BTN_B,
    BTN_COUNT
};

static const JW_MatrixButtons::BtnMapItem MAP[] = {
    {BTN_A, 0, 0},
    {BTN_B, 1, 1},
};

JW_MatrixButtons buttons;

void setup()
{
    buttons.begin(
        ROWS, 2,
        COLS, 4,
        MAP, sizeof(MAP) / sizeof(MAP[0]),
        BTN_COUNT,
        false,
        35);
}

void loop()
{
    buttons.update();

    if (buttons.pressed(BTN_A))
    {
        // Acción
    }

    delay(5);
}
```

---

## Modo botones directos

```cpp
bool beginDirect(const uint8_t *buttonPins,
                 uint8_t buttonCount,
                 bool invertLogic = false,
                 uint32_t debounceMs = 35,
                 uint8_t inputMode = INPUT);
```

Ejemplo:

```cpp
static const uint8_t PINS[] = {35, 34, 39};

JW_MatrixButtons buttons;

buttons.beginDirect(
    PINS,
    3,
    true,
    35,
    INPUT);
```

Usar `invertLogic=true` cuando el estado activo sea LOW y `false` cuando el estado activo sea HIGH.

---

## Task opcional ESP32

Para una instancia genérica:

```cpp
buttons.startTask(
    1,    // core
    4096, // stack bytes
    1,    // prioridad
    5);   // periodo ms
```

API:

```cpp
bool startTask(uint8_t core = 1,
               uint32_t stackBytes = 4096,
               uint8_t priority = 1,
               uint16_t periodMs = 5);

void stopTask();
bool taskRunning() const;
void setTaskPeriodMs(uint16_t periodMs);
uint16_t taskPeriodMs() const;
```

Si el task interno está activo, no llamar también `update()` periódicamente desde `loop()`.

---

## Repeat genérico

```cpp
buttons.setRepeatEnabled(BTN_LEFT, true);
buttons.setRepeatInitialDelay(350);

buttons.setRepeatProfile(
    12, 30, 70,
    1, 10, 100, 1000,
    110, 95, 80, 65);
```

Los parámetros representan:

```text
thresholds -> cambios de tramo según cantidad de repeats
steps      -> multiplicador entregado por EV_REPEAT
Delays     -> separación entre repeats en cada tramo
```

---

## `applyAxis()`

```cpp
bool applyAxis(uint32_t *value,
               uint32_t minValue,
               uint32_t maxValue,
               uint8_t decId,
               uint8_t incId,
               bool circularWrapOnPress = true,
               bool snapToStepOnRepeat = true) const;
```

Ejemplo:

```cpp
uint32_t value = 0;

if (buttons.applyAxis(&value, 0, 1000, BTN_LEFT, BTN_RIGHT))
{
    Serial.println(value);
}
```

---

# Instalación y resolución de librerías

## Dentro del package JWPLC

Para JWPLC Basic no se recomienda instalar otra copia manual de `JW_MatrixButtons` en el sketchbook si se pretende usar la versión integrada por el package.

Una copia adicional con el mismo nombre puede participar en la resolución de librerías de Arduino y dificulta la reproducibilidad entre PCs.

Para talleres/validación se recomienda comprobar en la salida verbose:

```text
Used platform
Used library
Using precompiled library
```

y confirmar que `JW_MatrixButtons` provenga de la versión de package esperada.

---

## Instalación manual para proyectos genéricos

Si se usa fuera de JWPLC, instalar como una librería Arduino normal en el sketchbook:

```text
Documents/Arduino/libraries/JW_MatrixButtons
```

Evitar mantener simultáneamente varias copias con el mismo nombre en diferentes carpetas de librerías.

---

# Precompilación en el package JWPLC

La copia integrada en JWPLC usa:

```text
precompiled=full
```

para ESP32 cuando existe el archive correspondiente.

En Alpha8 no se modificó el código fuente de `JW_MatrixButtons`; la corrección de interacción Display/botonera se implementó en las capas `JWPLC_Display` y `JWPLC_GlobalPeripherals`. Por ello el archive 1.0.5 existente no requiere regeneración por el alcance Alpha8.

---

# Validación Alpha8

Se verificó físicamente:

```text
LEFT  -> correcto
UP    -> correcto
RIGHT -> correcto
ESC   -> correcto
OK    -> correcto
DOWN  -> correcto
```

También:

- ningún evento fantasma sostenido durante el gate dirigido;
- estado IDLE estable;
- Display no consume los latches de aplicación;
- pulsación sostenida no congela el runtime;
- aplicación continúa recibiendo eventos mientras la TFT funciona.

---

## Compatibilidad

- ESP32;
- Arduino AVR;
- otras arquitecturas Arduino compatibles con las primitivas utilizadas.

El task interno sólo está disponible en ESP32.

---

## Licencia

MIT

---

## Autor

**JW Control**
