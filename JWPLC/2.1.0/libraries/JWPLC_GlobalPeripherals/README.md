# JWPLC_GlobalPeripherals

Capa interna del package **JWPLC ESP32** que conecta los periféricos globales del **JWPLC Basic** con el runtime del core.

No reemplaza a los drivers individuales. Su función es ofrecer:

- objetos globales ya conocidos por el sketch;
- autoload coherente;
- IDs de botonera;
- integración con el mutex SPI;
- snapshots cacheados de I/O y RTC;
- helpers internos utilizados por otras capas del package.

---

## Objetos globales

La capa expone:

```cpp
JWPLC_RTC
JWPLC_FRAM
JWPLC_Buttons
JWPLC_SD
```

También incluye las APIs de:

```cpp
JWPLC_Ethernet
JWPLC_RS485
JWPLC_ModbusRTU
```

Alpha8 añade dos fachadas de lectura cacheada:

```cpp
JWPLC_IO
JWPLC_Time
```

---

# Botonera JWPLC

IDs físicos:

```text
BTN_LEFT
BTN_UP
BTN_RIGHT
BTN_ESC
BTN_OK
BTN_DOWN
```

API pública recomendada en sketches:

```cpp
JWPLC_Buttons.pressed(BTN_OK);
JWPLC_Buttons.released(BTN_OK);
JWPLC_Buttons.isDown(BTN_OK);
JWPLC_Buttons.eventCount();
JWPLC_Buttons.getEvent(...);
JWPLC_Buttons.clearPendingInput();
```

El runtime del JWPLC Basic mantiene el scan automáticamente desde un task interno. Un sketch normal no debe llamar `JWPLC_Buttons.update()` ni iniciar un segundo task sobre la misma instancia.

Los helpers `JWPLCButtons::` continúan disponibles para integración interna/legacy, pero no son la API recomendada para ejemplos de usuario.

### Cambio Alpha8

Antes de Alpha8, los eventos de botonera podían disparar refresh/navegación del Display usando la cola compartida de eventos.

Alpha8 separa responsabilidades:

```text
Task de botonera
  -> actualiza estado físico y latches

Display
  -> observa cambios físicos / flancos propios

Sketch
  -> consume pressed()/released() de aplicación
```

El router de refresh del Display se activa por cambio de máscara física, no por cualquier contenido residual de `eventCount()`.

`anyPressedOrRepeated()` conserva semántica `PRESS/REPEAT` sin tratar `RELEASE` como actividad válida.

---

# Vistas cacheadas Alpha8

## `JWPLC_IO`

`JWPLC_IO` lee el snapshot ya mantenido por el runtime y no genera una nueva transacción I2C.

```cpp
uint8_t inputs = JWPLC_IO.inputs();
uint8_t outputs = JWPLC_IO.outputs();

bool i0 = JWPLC_IO.input(0);
bool q0 = JWPLC_IO.output(0);

bool ready = JWPLC_IO.ready();
uint32_t ageBase = JWPLC_IO.lastScanMs();
```

En JWPLC Basic:

```text
inputs()  -> I0.0..I0.7
outputs() -> Q0.0..Q0.7
```

`outputs()` representa el banco lógico de las 8 salidas expuestas al usuario por el JWPLC Basic. No pretende exponer bancos internos adicionales del expansor.

---

## `JWPLC_Time`

`JWPLC_Time` consume el snapshot RTC ya actualizado por el runtime.

```cpp
JWPLC_Time.present();
JWPLC_Time.valid();
JWPLC_Time.lostPower();

JWPLC_Time.second();
JWPLC_Time.minute();
JWPLC_Time.hour();

JWPLC_Time.day();
JWPLC_Time.month();
JWPLC_Time.year();
JWPLC_Time.dayOfWeek();

JWPLC_Time.lastUpdateMs();
```

Estas llamadas no ejecutan una lectura RTC adicional y son adecuadas para HMI, logging ligero y lógica que sólo necesita el último snapshot del sistema.

---

# microSD

Helpers internos/legacy:

```cpp
JWPLCSD::begin();
JWPLCSD::isEnabled();
JWPLCSD::isReady();
JWPLCSD::isCardPresent();
JWPLCSD::lastErrorString();
```

La API de aplicación permanece en el objeto:

```cpp
JWPLC_SD
```

---

# Includes automáticos

En el perfil JWPLC Basic, los periféricos principales forman parte del ecosistema disponible por el package.

Para sketches simples puede aprovecharse el autoload. Para librerías reutilizables o código que deba compilar fuera del perfil JWPLC, se recomienda incluir explícitamente el header del periférico utilizado.

---

# Separación de responsabilidades

```text
JWPLC_GlobalPeripherals
├── objetos globales
├── integración de autoload
├── botonera física
├── snapshots JWPLC_IO / JWPLC_Time
└── bridges internos de runtime

JWPLC_Display
├── TFT
├── IDLE/USER
├── HMI declarativa Alpha8
└── diagnósticos visuales

JWPLC_Ethernet
└── W5500 / red

JWPLC_RS485
└── transporte RS-485

JWPLC_ModbusRTU
└── protocolo Modbus RTU

JW_* / JW_Libraries
└── drivers reutilizables
```

---

# Consideraciones de rendimiento

Las clases `JWPLC_IOView` y `JWPLC_TimeView` se implementan dentro del mismo TU de `JWPLC_GlobalPeripherals.cpp`.

La decisión es intencional: Alpha8 había introducido temporalmente un TU adicional para estas fachadas y el benchmark detectó una compilación extra en cold build.

La implementación se reintegró al TU existente conservando la API pública:

```text
Basic cold compiler invocations: 15
Core cold compiler invocations:  78
Warm compiler invocations:        1
```

Esto recupera la paridad estructural de Alpha6 sin retirar funcionalidad.

---

# Estado Alpha8

```text
JWPLC ESP32 2.1.0-alpha.8
JWPLC_GlobalPeripherals 1.0.0
```

Alpha8 mantiene todos los periféricos del autoload normal, separa correctamente botonera/Display y añade vistas cacheadas de runtime sin añadir un TU permanente al cold build.
