# JW_MatrixButtons

Librería ligera para leer una matriz de botones (filas/columnas) con **debounce**, eventos (`press` / `release` / `repeat`) y helpers para navegación tipo **HMI / PLC**.

Está pensada para proyectos donde el **sketch decide qué hacer con los botones**, sin acoplar la librería a menús, pantallas o lógicas específicas de aplicación.

---

## Características

- Lectura de matriz **R×C** (hasta **8×8**) con tiempos de “settle” configurables.
- Debounce por tecla con ventana configurable.
- Generación de eventos:
  - `EV_PRESS`
  - `EV_RELEASE`
  - `EV_REPEAT` (al mantener presionado, con aceleración)
- Repeat configurable:
  - habilitar/deshabilitar por botón
  - retardo inicial
  - perfil de aceleración (`thresholds`, `step` y `delay`)
- Helper `applyAxis()` para manejar pares tipo:
  - `UP / DOWN`
  - `LEFT / RIGHT`
- Helpers para limpiar entradas pendientes al cambiar de pantalla o modo:
  - `clearPendingPresses()`
  - `clearPendingReleases()`
  - `clearPendingRepeats()`
  - `clearEventQueue()`
  - `clearPendingInput()`

---

## ¿Para qué sirve?

La librería **no hace menús por sí sola**.  
Su función es:

- escanear la matriz de botones
- aplicar debounce
- generar eventos y estados consistentes
- entregar esos datos al sketch

Con eso, se pueden construir:

- menús en TFT / GLCD
- HMIs sencillas
- editores de parámetros
- navegación de listas
- sistemas tipo PLC / control industrial
- teclados matriciales personalizados

---

## Instalación (manual)

1. Crear una carpeta en `Documents/Arduino/libraries/` llamada:

   ```text
   JW_MatrixButtons
   ```

2. Copiar dentro los archivos de la librería.

3. Reiniciar Arduino IDE.

> Si el IDE muestra un error como `invalid library: no header files found`, normalmente significa que el archivo `.h` no está en la ubicación correcta o que el nombre de la carpeta no coincide con el nombre esperado de la librería.

---

## Cableado típico

### Matriz de botones

- **Filas** → pines configurados como `OUTPUT`
- **Columnas** → pines configurados como `INPUT`
- Cada botón conecta una fila con una columna

### Lógica de entrada

- Si las columnas tienen **pulldown externo**, usar:

  ```cpp
  invertLogic = false
  ```

- Si las columnas usan **pullup** y el botón conecta a **GND**, usar:

  ```cpp
  invertLogic = true
  ```

> En ESP32, los pines `34`, `35`, `36` y `39` son **solo entrada** y no soportan `INPUT_PULLUP`, por lo que normalmente se usan con resistencias externas.

---

## Flujo de uso recomendado

En la mayoría de los casos, la librería se usa así:

1. Se llama a `begin(...)` en `setup()`
2. Se llama a `update()` en cada vuelta del `loop()`
3. Se consultan:
   - `pressed(id)`
   - `released(id)`
   - `isDown(id)`
   - o bien la cola de eventos con:
     - `eventCount()`
     - `getEvent(...)`

### Recomendación importante para aplicaciones con múltiples pantallas

En aplicaciones con:

- menús
- editores
- subpantallas
- modos de configuración
- ventanas emergentes

se recomienda:

- leer los flancos una sola vez por ciclo, **o**
- limpiar las entradas pendientes al cambiar de vista usando:

```cpp
clearPendingInput();
```

Esto evita que un `PRESS` pendiente de una pantalla anterior se “consuma” recién en la nueva pantalla.

---

## API rápida

### Tipos

```cpp
JW_MatrixButtons::EvType      // EV_PRESS, EV_RELEASE, EV_REPEAT
JW_MatrixButtons::BtnEvent    // { id, type, mult, held_ms }
JW_MatrixButtons::BtnMapItem  // { id, row, col }
```

---

### Inicialización

```cpp
bool begin(const uint8_t* rowPins, uint8_t nRows,
           const uint8_t* colPins, uint8_t nCols,
           const BtnMapItem* map, uint8_t mapLen,
           uint8_t buttonCount,
           bool invertLogic = false,
           uint32_t debounceMs = 35);
```

---

### En `loop()`

```cpp
void update(); // ideal cada 3–10 ms
```

---

### Estado / eventos del último `update()`

```cpp
bool pressed(uint8_t id);
bool released(uint8_t id);
bool isDown(uint8_t id);

uint8_t eventCount() const;
bool getEvent(uint8_t idx, BtnEvent& out) const;
```

#### Nota sobre `pressed()` y `released()`

Estas funciones son **consumibles / latcheadas**.  
Eso significa que un `PRESS` o `RELEASE` detectado puede permanecer pendiente hasta que el sketch lo lea.

Por eso, en aplicaciones con múltiples pantallas, es buena práctica:

- centralizar la lectura de flancos por ciclo, o
- limpiar pendientes al cambiar de modo con `clearPendingInput()`

---

### Repeat

```cpp
void setRepeatEnabled(uint8_t id, bool enabled);
void setRepeatInitialDelay(uint32_t ms);

void setRepeatProfile(uint16_t thr1, uint16_t thr2, uint16_t thr3,
                      int16_t s1, int16_t s2, int16_t s3, int16_t s4,
                      uint32_t d1, uint32_t d2, uint32_t d3, uint32_t d4);
```

#### Perfil por defecto

- `initialDelay = 300 ms`
- `thresholds = 7 / 15 / 25`
- `steps = 1 / 10 / 100 / 1000`
- `delays = 180 / 170 / 150 / 120 ms`

---

### Helper de eje

```cpp
bool applyAxis(uint32_t* val, uint32_t minv, uint32_t maxv,
               uint8_t decId, uint8_t incId,
               bool circularWrapOnPress = true,
               bool snapToStepOnRepeat = true) const;
```

#### Devuelve

- `true` si modificó `*val` en ese ciclo
- `false` si no hubo cambio

#### Comportamiento

- `circularWrapOnPress`
  - si se está en `maxv` y llega `INC` por `PRESS`, salta a `minv`
  - si se está en `minv` y llega `DEC` por `PRESS`, salta a `maxv`

- `snapToStepOnRepeat`
  - cuando el cambio ocurre por `EV_REPEAT`, alinea al múltiplo del `step` para que los saltos sean más limpios

---

### Limpieza de pendientes

```cpp
void clearPendingPresses() const;
void clearPendingReleases() const;
void clearPendingRepeats() const;
void clearEventQueue() const;
void clearPendingInput() const;
```

#### ¿Qué limpian?

Estas funciones limpian **entradas pendientes de consumir**, pero **no alteran el estado físico del botón**.

Es decir:

- limpian `PRESS` pendientes
- limpian `RELEASE` pendientes
- limpian repeats pendientes
- limpian cola de eventos
- **no fuerzan** `isDown()` a `false`

#### Cuándo usarlas

Son especialmente útiles al:

- cambiar de pantalla
- entrar a un modo de edición
- cerrar un popup
- salir de un submenú
- abrir otra vista inmediatamente después de una pulsación

---

## Ejemplo 1: lectura básica

```cpp
#include <Arduino.h>
#include <JW_MatrixButtons.h>

static const uint8_t ROWS[] = {25, 26};
static const uint8_t COLS[] = {35, 34, 39, 36};

enum BtnId : uint8_t {
  BTN_A,
  BTN_B,
  BTN__COUNT
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
    BTN__COUNT,
    false,
    35
  );

  btn.setRepeatEnabled(BTN_A, true);
}

void loop() {
  btn.update();

  if (btn.pressed(BTN_A)) {
    Serial.println("A press");
  }

  if (btn.released(BTN_A)) {
    Serial.println("A release");
  }

  if (btn.pressed(BTN_B)) {
    Serial.println("B press");
  }

  JW_MatrixButtons::BtnEvent ev;
  for (uint8_t i = 0; i < btn.eventCount(); i++) {
    if (btn.getEvent(i, ev)) {
      Serial.print("Evento id=");
      Serial.print(ev.id);
      Serial.print(" type=");
      Serial.print((int)ev.type);
      Serial.print(" mult=");
      Serial.print(ev.mult);
      Serial.print(" held_ms=");
      Serial.println(ev.held_ms);
    }
  }

  delay(5);
}
```

---

## Ejemplo 2: navegación con `applyAxis()`

```cpp
#include <Arduino.h>
#include <JW_MatrixButtons.h>

static const uint8_t ROWS[] = {25, 26};
static const uint8_t COLS[] = {35, 34, 39, 36};

enum BtnId : uint8_t {
  BTN_LEFT,
  BTN_UP,
  BTN_RIGHT,
  BTN_DOWN,
  BTN__COUNT
};

static const JW_MatrixButtons::BtnMapItem MAP[] = {
  {BTN_LEFT,  0, 0},
  {BTN_UP,    0, 1},
  {BTN_RIGHT, 0, 2},
  {BTN_DOWN,  1, 1},
};

JW_MatrixButtons btn;
uint32_t value = 0;

void setup() {
  Serial.begin(115200);

  btn.begin(
    ROWS, 2,
    COLS, 4,
    MAP, sizeof(MAP) / sizeof(MAP[0]),
    BTN__COUNT,
    false,
    20
  );

  btn.setRepeatEnabled(BTN_LEFT,  true);
  btn.setRepeatEnabled(BTN_RIGHT, true);
}

void loop() {
  btn.update();

  if (btn.applyAxis(&value, 0, 100, BTN_LEFT, BTN_RIGHT, true, true)) {
    Serial.print("Nuevo valor: ");
    Serial.println(value);
  }

  delay(5);
}
```

---

## Ejemplo 3: cambio de pantalla seguro con `clearPendingInput()`

Este ejemplo muestra un caso típico de interfaz con dos pantallas:

- `MAIN`
- `DETAIL`

El botón `OK` entra a `DETAIL` y `ESC` vuelve a `MAIN`.

Al regresar, se limpian las entradas pendientes para evitar que una pulsación anterior se consuma en la nueva pantalla.

```cpp
#include <Arduino.h>
#include <JW_MatrixButtons.h>

static const uint8_t ROWS[] = {25, 26};
static const uint8_t COLS[] = {35, 34, 39, 36};

enum BtnId : uint8_t {
  BTN_LEFT,
  BTN_UP,
  BTN_RIGHT,
  BTN_ESC,
  BTN_OK,
  BTN_DOWN,
  BTN__COUNT
};

static const JW_MatrixButtons::BtnMapItem MAP[] = {
  {BTN_LEFT,  0, 0},
  {BTN_UP,    0, 1},
  {BTN_RIGHT, 0, 2},
  {BTN_ESC,   1, 0},
  {BTN_OK,    1, 1},
  {BTN_DOWN,  1, 2}
};

JW_MatrixButtons btn;

enum ScreenMode : uint8_t {
  SCREEN_MAIN = 0,
  SCREEN_DETAIL
};

ScreenMode screenMode = SCREEN_MAIN;

void setup() {
  Serial.begin(115200);

  btn.begin(
    ROWS, 2,
    COLS, 4,
    MAP, sizeof(MAP) / sizeof(MAP[0]),
    BTN__COUNT,
    false,
    20
  );
}

void loop() {
  btn.update();

  if (screenMode == SCREEN_MAIN) {
    if (btn.pressed(BTN_OK)) {
      screenMode = SCREEN_DETAIL;

      // Limpia cualquier flanco pendiente antes de entrar
      btn.clearPendingInput();

      Serial.println("Entrando a DETAIL");
    }
  }
  else if (screenMode == SCREEN_DETAIL) {
    if (btn.pressed(BTN_ESC)) {
      screenMode = SCREEN_MAIN;

      // Limpia cualquier pendiente al volver
      btn.clearPendingInput();

      Serial.println("Volviendo a MAIN");
    }
  }

  delay(5);
}
```

---

## Consejos de uso

### 1. No usar delays grandes
Evitar `delay()` largos en el `loop()`.  
Si `update()` deja de ejecutarse con frecuencia, el debounce y el repeat se vuelven más toscos.

### 2. Recomendación de frecuencia
En general, llamar `update()` cada **3 a 10 ms** da muy buen resultado.

### 3. Repeat solo donde tenga sentido
Activar repeat solo en botones como:

- `UP`
- `DOWN`
- `LEFT`
- `RIGHT`

y normalmente dejarlo desactivado en:

- `OK`
- `ESC`

### 4. En aplicaciones con varias pantallas
Si se observa que una pulsación “afecta” a la vista siguiente, usar:

```cpp
clearPendingInput();
```

al cambiar de pantalla o de modo.

---

## Compatibilidad

- Arduino AVR
- ESP32
- otras arquitecturas compatibles con Arduino

---

## Licencia

MIT

---

## Autor

**JW Control**
