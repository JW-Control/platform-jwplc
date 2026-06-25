# JW_MatrixButtons

**Versión actual:** 1.0.5

Librería ligera para leer botones en proyectos Arduino / ESP32 con **debounce**, eventos (`press` / `release` / `repeat`) y helpers para navegación tipo **HMI / PLC**.

Soporta dos modos de escaneo:

1. **Matriz R×C**: filas + columnas + mapa de botones.
2. **Botones directos 1×N**: cada botón conectado a un GPIO, sin filas ni mapa.

La librería está pensada para que el sketch decida qué hacer con los botones, sin acoplarla a menús, pantallas o lógicas específicas de aplicación.

---

## Características

- Lectura de matriz **R×C** hasta **8×8**.
- Lectura de botones directos **1×N** hasta **32 botones**.
- Debounce por tecla con ventana configurable.
- Eventos:
  - `EV_PRESS`
  - `EV_RELEASE`
  - `EV_REPEAT`
- Repeat configurable:
  - habilitar/deshabilitar por botón
  - retardo inicial
  - perfil de aceleración (`thresholds`, `step` y `delay`)
- Helpers latcheados:
  - `pressed(id)`
  - `released(id)`
  - `isDown(id)`
- Cola de eventos del último `update()`:
  - `eventCount()`
  - `getEvent(...)`
- Helper `applyAxis()` para navegación de valores con botones tipo:
  - `LEFT / RIGHT`
  - `UP / DOWN`
- Helpers para limpiar entradas pendientes al cambiar de pantalla o modo:
  - `clearPendingPresses()`
  - `clearPendingReleases()`
  - `clearPendingRepeats()`
  - `clearEventQueue()`
  - `clearPendingInput()`
- Task opcional en ESP32 para ejecutar `update()` periódicamente.

---

## ¿Para qué sirve?

La librería **no hace menús por sí sola**.

Su función es:

- leer botones;
- aplicar debounce;
- generar eventos consistentes;
- mantener estado por botón;
- entregar esos datos al sketch.

Con eso, se pueden construir:

- menús en TFT / GLCD;
- HMIs sencillas;
- editores de parámetros;
- navegación de listas;
- sistemas tipo PLC / control industrial;
- botoneras matriciales;
- botoneras directas reutilizando la misma lógica.

---

## Instalación manual

1. Crear una carpeta en `Documents/Arduino/libraries/` llamada:

   ```txt
   JW_MatrixButtons
   ```

2. Copiar dentro:

   ```txt
   library.properties
   README.md
   CHANGELOG.md
   src/JW_MatrixButtons.h
   src/JW_MatrixButtons.cpp
   examples/
   ```

3. Reiniciar Arduino IDE.

---

## Modo matriz R×C

### Cableado típico

- **Filas**: pines configurados como `OUTPUT`.
- **Columnas**: pines configurados como `INPUT`.
- Cada botón conecta una fila con una columna.

### Inicialización

```cpp
bool begin(const uint8_t* rowPins, uint8_t nRows,
           const uint8_t* colPins, uint8_t nCols,
           const BtnMapItem* map, uint8_t mapLen,
           uint8_t buttonCount,
           bool invertLogic = false,
           uint32_t debounceMs = 35);
```

### Ejemplo mínimo matriz

```cpp
#include <Arduino.h>
#include <JW_MatrixButtons.h>

static const uint8_t ROWS[] = {25, 26};
static const uint8_t COLS[] = {35, 34, 39, 36};

enum BtnId : uint8_t {
  BTN_A,
  BTN_B,
  BTN_COUNT
};

static const JW_MatrixButtons::BtnMapItem MAP[] = {
  {BTN_A, 0, 0},
  {BTN_B, 1, 1},
};

JW_MatrixButtons btn;

void setup() {
  Serial.begin(115200);

  btn.begin(
    ROWS, 2,
    COLS, 4,
    MAP, sizeof(MAP) / sizeof(MAP[0]),
    BTN_COUNT,
    false,
    35
  );
}

void loop() {
  btn.update();

  if (btn.pressed(BTN_A)) {
    Serial.println("A press");
  }

  if (btn.released(BTN_A)) {
    Serial.println("A release");
  }

  delay(5);
}
```

---

## Modo botones directos

Este modo es para proyectos donde no existe una matriz real. Cada botón entra directo a un GPIO.

Ejemplo conceptual:

```txt
LEFT   -> GPIO directo
RIGHT  -> GPIO directo
CENTER -> GPIO directo
```

En modo directo:

- no se usan `rowPins`;
- no se usan `colPins`;
- no se usa `BtnMapItem`;
- no se configura ningún pin como `OUTPUT`;
- cada botón se configura con `pinMode(buttonPins[i], inputMode)`;
- `update()` lee cada pin con `digitalRead(buttonPins[i])`;
- se reutiliza la misma lógica de debounce, eventos, repeat y `applyAxis()`.

### Inicialización

```cpp
bool beginDirect(const uint8_t* buttonPins,
                 uint8_t buttonCount,
                 bool invertLogic = false,
                 uint32_t debounceMs = 35,
                 uint8_t inputMode = INPUT);
```

### `invertLogic`

Usar:

```cpp
invertLogic = true
```

cuando el botón sea activo en `LOW`, por ejemplo:

```txt
GPIO ---- botón ---- GND
GPIO con pull-up externo
```

Usar:

```cpp
invertLogic = false
```

cuando el botón sea activo en `HIGH`, por ejemplo:

```txt
GPIO ---- botón ---- 3V3
GPIO con pulldown externo
```

### `inputMode`

Valores típicos:

```cpp
INPUT
INPUT_PULLUP
INPUT_PULLDOWN // si el core/placa lo soporta
```

En ESP32, los GPIO `34`, `35`, `36` y `39` son solo entrada y **no tienen pull-up/pull-down interno usable**. Para esos pines usa `INPUT` y resistencias externas.

### Ejemplo mínimo directo

```cpp
#include <Arduino.h>
#include <JW_MatrixButtons.h>

enum BtnId : uint8_t {
  BTN_LEFT = 0,
  BTN_RIGHT,
  BTN_CENTER,
  BTN_COUNT
};

static const uint8_t BTN_PINS[] = {
  35, 34, 39
};

JW_MatrixButtons btn;
uint32_t value = 0;

void setup() {
  Serial.begin(115200);

  btn.beginDirect(
    BTN_PINS,
    BTN_COUNT,
    true, // true si presionado = LOW
    35,
    INPUT
  );

  btn.setRepeatEnabled(BTN_LEFT, true);
  btn.setRepeatEnabled(BTN_RIGHT, true);
}

void loop() {
  btn.update();

  if (btn.pressed(BTN_CENTER)) {
    Serial.println("CENTER press");
  }

  if (btn.applyAxis(&value, 0, 1000, BTN_LEFT, BTN_RIGHT)) {
    Serial.print("value=");
    Serial.println(value);
  }

  delay(5);
}
```

---

## Flujo de uso recomendado

1. Llamar a `begin(...)` o `beginDirect(...)` en `setup()`.
2. Llamar a `update()` en cada vuelta del `loop()`.
3. Consultar:
   - `pressed(id)`
   - `released(id)`
   - `isDown(id)`
   - o la cola de eventos con `eventCount()` / `getEvent(...)`.

```cpp
void loop() {
  btn.update();

  if (btn.pressed(BTN_OK)) {
    // Acción de OK
  }

  delay(5);
}
```

---

## Task opcional en ESP32

En ESP32, la librería puede ejecutar `update()` desde un task interno:

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

Notas:

- Si el task está activo, no necesitas llamar `update()` manualmente en `loop()`.
- `stackBytes` se expresa en bytes en Arduino-ESP32 / ESP-IDF.
- Para aplicaciones simples, `4096` bytes es conservador y adecuado.

---

## Eventos

```cpp
JW_MatrixButtons::EvType      // EV_PRESS, EV_RELEASE, EV_REPEAT
JW_MatrixButtons::BtnEvent    // { id, type, mult, held_ms }
JW_MatrixButtons::BtnMapItem  // { id, row, col }
```

### Leer eventos del último update

```cpp
JW_MatrixButtons::BtnEvent ev;

for (uint8_t i = 0; i < btn.eventCount(); i++) {
  if (btn.getEvent(i, ev)) {
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

---

## `pressed()` y `released()`

Estas funciones son **latcheadas y consumibles**.

Eso significa que si ocurre un `PRESS` o `RELEASE`, queda pendiente hasta que el sketch lo lea.

```cpp
if (btn.pressed(BTN_OK)) {
  Serial.println("OK");
}
```

Si llamas dos veces seguidas a `pressed(BTN_OK)`, la primera consume el evento y la segunda normalmente devuelve `false`.

---

## Repeat

```cpp
void setRepeatEnabled(uint8_t id, bool enabled);
void setRepeatInitialDelay(uint32_t ms);

void setRepeatProfile(uint16_t thr1, uint16_t thr2, uint16_t thr3,
                      int16_t s1, int16_t s2, int16_t s3, int16_t s4,
                      uint32_t d1, uint32_t d2, uint32_t d3, uint32_t d4);
```

Perfil por defecto:

```txt
initialDelay = 350 ms
thresholds   = 12 / 30 / 70
steps        = 1 / 10 / 100 / 1000
delays       = 110 / 95 / 80 / 65 ms
```

Activar repeat solo donde tenga sentido:

```cpp
btn.setRepeatEnabled(BTN_LEFT, true);
btn.setRepeatEnabled(BTN_RIGHT, true);
```

---

## `applyAxis()`

Helper para modificar un valor usando un botón de decremento y otro de incremento.

```cpp
bool applyAxis(uint32_t* val, uint32_t minv, uint32_t maxv,
               uint8_t decId, uint8_t incId,
               bool circularWrapOnPress = true,
               bool snapToStepOnRepeat = true) const;
```

Devuelve:

- `true` si modificó el valor;
- `false` si no hubo cambio.

Ejemplo:

```cpp
uint32_t value = 0;

if (btn.applyAxis(&value, 0, 1000, BTN_LEFT, BTN_RIGHT)) {
  Serial.println(value);
}
```

---

## Limpieza de pendientes

```cpp
void clearPendingPresses() const;
void clearPendingReleases() const;
void clearPendingRepeats() const;
void clearEventQueue() const;
void clearPendingInput() const;
```

Estas funciones limpian entradas pendientes de consumir, pero **no alteran el estado físico del botón**.

Son útiles al:

- cambiar de pantalla;
- entrar a un modo de edición;
- cerrar un popup;
- salir de un submenú;
- abrir otra vista inmediatamente después de una pulsación.

Ejemplo:

```cpp
screenMode = SCREEN_DETAIL;
btn.clearPendingInput();
```

---

## Consejos de uso

### No usar delays grandes

Evita `delay()` largos en el `loop()`. Si `update()` deja de ejecutarse con frecuencia, el debounce y el repeat se vuelven más toscos.

### Frecuencia recomendada

Llamar `update()` cada **3 a 10 ms** suele dar buen resultado.

### Aplicaciones con varias pantallas

Si una pulsación afecta a la vista siguiente, limpia pendientes al cambiar de pantalla:

```cpp
btn.clearPendingInput();
```

---

## Compatibilidad

- ESP32
- Arduino AVR
- otras arquitecturas compatibles con Arduino

El soporte de task interno solo está disponible cuando se compila para ESP32.

---

## Licencia

MIT

---

## Autor

**JW Control**
