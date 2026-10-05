# JWPLC_GlobalPeripherals

`JWPLC_GlobalPeripherals` reúne varios objetos que el JWPLC Basic deja listos
para usar desde tu sketch: botonera, RTC, FRAM, microSD y vistas rápidas de las
entradas, salidas y hora del sistema.

Si estás empezando, piensa en esta librería como el punto de acceso a los
periféricos que el JWPLC ya inicializa por ti.

## ¿Para qué sirve?

Te permite, entre otras cosas:

- leer la botonera frontal;
- consultar rápidamente entradas y salidas;
- consultar la fecha y hora que el runtime ya mantiene actualizada;
- acceder a los objetos globales `JWPLC_RTC`, `JWPLC_FRAM` y `JWPLC_SD`;
- usar esos periféricos sin crear nuevas instancias ni reasignar pines.

Los objetos globales más importantes son:

```cpp
JWPLC_Buttons
JWPLC_IO
JWPLC_Time
JWPLC_RTC
JWPLC_FRAM
JWPLC_SD
```

## Qué hace automáticamente el JWPLC

En un JWPLC Basic normal no tienes que:

- crear otra instancia de la botonera;
- escanear manualmente la matriz de botones;
- volver a inicializar RTC, FRAM o microSD;
- conocer los pines internos de esos periféricos;
- leer nuevamente el hardware cada vez que consultas `JWPLC_IO` o `JWPLC_Time`.

El runtime mantiene esos recursos y expone una vista lista para el sketch.

## Inicio rápido

Este ejemplo enciende o apaga `Q0_0` cada vez que pulsas el botón `OK`.

```cpp
#include <JWPLC_GlobalPeripherals.h>

bool salida = false;

void setup()
{
    pinMode(Q0_0, OUTPUT);

    JWPLC_Buttons.clearPendingInput();

    digitalWrite(Q0_0, LOW);
}

void loop()
{
    if (JWPLC_Buttons.pressed(BTN_OK))
    {
        salida = !salida;

        digitalWrite(
            Q0_0,
            salida ? HIGH : LOW);
    }
}
```

No necesitas llamar `JWPLC_Buttons.update()`: el JWPLC escanea la botonera
automáticamente.

## Conceptos básicos

### Botones físicos

Los IDs disponibles son:

```text
BTN_LEFT
BTN_UP
BTN_RIGHT
BTN_ESC
BTN_OK
BTN_DOWN
```

Las tres consultas más útiles son:

```cpp
JWPLC_Buttons.pressed(BTN_OK);
JWPLC_Buttons.released(BTN_OK);
JWPLC_Buttons.isDown(BTN_OK);
```

- `pressed()`: devuelve `true` una vez por pulsación.
- `released()`: devuelve `true` una vez al soltar.
- `isDown()`: indica el estado físico actual mientras el botón siga presionado.

`pressed()` y `released()` son eventos consumibles: si ya los leíste, no
vuelven a ser `true` hasta que ocurra un nuevo evento.

### Vista de entradas y salidas

`JWPLC_IO` permite consultar el último estado que ya conoce el runtime.

```cpp
bool i0 = JWPLC_IO.input(0);
bool q0 = JWPLC_IO.output(0);

uint8_t entradas = JWPLC_IO.inputs();
uint8_t salidas = JWPLC_IO.outputs();
```

Los índices válidos de `input()` y `output()` para JWPLC Basic son `0..7`.

### Vista de fecha y hora

`JWPLC_Time` consulta el último snapshot del RTC.

```cpp
if (JWPLC_Time.valid())
{
    uint8_t hora = JWPLC_Time.hour();
    uint8_t minuto = JWPLC_Time.minute();
    uint8_t segundo = JWPLC_Time.second();
}
```

Esto es ideal para HMI, registro de datos y lógica que consulta la hora
frecuentemente.

## Ejemplo 1 — Básico: botonera y salida

En este ejemplo:

- `OK` enciende `Q0_0`;
- `ESC` apaga `Q0_0`;
- `UP` informa por Serial mientras permanece presionado.

```cpp
#include <JWPLC_GlobalPeripherals.h>

void setup()
{
    Serial.begin(115200);

    pinMode(Q0_0, OUTPUT);
    digitalWrite(Q0_0, LOW);

    JWPLC_Buttons.clearPendingInput();
}

void loop()
{
    if (JWPLC_Buttons.pressed(BTN_OK))
    {
        digitalWrite(Q0_0, HIGH);
        Serial.println("Salida ON");
    }

    if (JWPLC_Buttons.pressed(BTN_ESC))
    {
        digitalWrite(Q0_0, LOW);
        Serial.println("Salida OFF");
    }

    static uint32_t ultimoMensaje = 0;

    if (JWPLC_Buttons.isDown(BTN_UP) &&
        millis() - ultimoMensaje >= 250)
    {
        ultimoMensaje = millis();
        Serial.println("UP sigue presionado");
    }
}
```

## Ejemplo 2 — Intermedio: entradas, salidas y hora

Este ejemplo refleja `I0_0` en `Q0_0` y, cada segundo, imprime un resumen.

```cpp
#include <JWPLC_GlobalPeripherals.h>

void setup()
{
    Serial.begin(115200);

    pinMode(I0_0, INPUT);
    pinMode(Q0_0, OUTPUT);
}

void loop()
{
    const bool entrada =
        digitalRead(I0_0);

    digitalWrite(
        Q0_0,
        entrada ? HIGH : LOW);

    static uint32_t ultimoReporte = 0;

    if (millis() - ultimoReporte >= 1000)
    {
        ultimoReporte = millis();

        Serial.print("I0_0=");
        Serial.print(JWPLC_IO.input(0));

        Serial.print(" Q0_0=");
        Serial.print(JWPLC_IO.output(0));

        if (JWPLC_Time.valid())
        {
            Serial.print(" Hora=");
            Serial.print(JWPLC_Time.hour());
            Serial.print(':');
            Serial.print(JWPLC_Time.minute());
            Serial.print(':');
            Serial.print(JWPLC_Time.second());
        }

        Serial.println();
    }
}
```

Observa que no usamos `delay(1000)`. El resto del programa puede seguir
ejecutándose mientras esperamos el siguiente reporte.

## Ejemplo 3 — Aplicación real: START / STOP con horario visible

Supongamos una máquina sencilla:

- `OK` funciona como START;
- `ESC` funciona como STOP;
- `Q0_0` representa el contactor o relé de marcha;
- el estado se muestra por Serial junto con la hora.

```cpp
#include <JWPLC_GlobalPeripherals.h>

bool enMarcha = false;

void setup()
{
    Serial.begin(115200);

    pinMode(Q0_0, OUTPUT);
    digitalWrite(Q0_0, LOW);

    JWPLC_Buttons.clearPendingInput();
}

void loop()
{
    if (JWPLC_Buttons.pressed(BTN_OK))
    {
        enMarcha = true;
    }

    if (JWPLC_Buttons.pressed(BTN_ESC))
    {
        enMarcha = false;
    }

    digitalWrite(
        Q0_0,
        enMarcha ? HIGH : LOW);

    static uint32_t ultimoReporte = 0;

    if (millis() - ultimoReporte >= 1000)
    {
        ultimoReporte = millis();

        Serial.print(enMarcha ? "MARCHA" : "PARADO");

        if (JWPLC_Time.valid())
        {
            Serial.print(" | ");
            Serial.print(JWPLC_Time.hour());
            Serial.print(':');
            Serial.print(JWPLC_Time.minute());
            Serial.print(':');
            Serial.print(JWPLC_Time.second());
        }

        Serial.println();
    }
}
```

## Ejemplo 4 — Avanzado de usuario: leer y escribir las 8 E/S como un byte

Cuando necesitas procesar las ocho entradas o salidas juntas puedes usar la API
de bloque del core JWPLC:

```cpp
#include <JWPLC_GlobalPeripherals.h>

void setup()
{
}

void loop()
{
    uint8_t entradas =
        JWPLC_readInputs();

    JWPLC_writeOutputs(entradas);
}
```

Correspondencia:

```text
bit 0 -> I0_0 / Q0_0
bit 1 -> I0_1 / Q0_1
...
bit 7 -> I0_7 / Q0_7
```

Úsalo cuando trabajar con un bitmap sea más cómodo que ocho llamadas
individuales.

## API de usuario

### Botonera — Básico

| Función | Qué hace | Cuándo usarla |
|---|---|---|
| `pressed(id)` | Detecta una nueva pulsación y consume ese evento. | Botones tipo START, OK, navegación |
| `released(id)` | Detecta una nueva liberación y consume ese evento. | Acciones al soltar |
| `isDown(id)` | Consulta si el botón sigue físicamente presionado. | Mantener una acción mientras se sostiene |
| `clearPendingInput()` | Descarta eventos anteriores. | Al iniciar una pantalla o modo |

Ejemplo:

```cpp
if (JWPLC_Buttons.pressed(BTN_OK))
{
    // Nueva pulsación
}
```

### Botonera — Intermedio

| Función | Qué hace |
|---|---|
| `eventCount()` | Cantidad de eventos disponibles en la cola actual |
| `getEvent(index, event)` | Obtiene un evento `PRESS`, `RELEASE` o `REPEAT` |
| `clearPendingPresses()` | Limpia pulsaciones pendientes |
| `clearPendingReleases()` | Limpia liberaciones pendientes |
| `clearPendingRepeats()` | Limpia repeats pendientes |
| `clearEventQueue()` | Limpia la cola de eventos |

Estas funciones son útiles cuando quieres construir una navegación más
elaborada.

### Botonera — Avanzado de usuario

`applyAxis()` ayuda a modificar un valor con dos botones:

```cpp
uint32_t setpoint = 50;

JWPLC_Buttons.applyAxis(
    setpoint,
    0,
    100,
    BTN_DOWN,
    BTN_UP);
```

La configuración física de la matriz, `begin()`, `update()`,
`startTask()`, `setScanDelays()` y el perfil interno de repeat ya son
administrados por el JWPLC Basic y no deben reconfigurarse en un sketch normal.

### JWPLC_IO — Básico

| Función | Retorno | Uso |
|---|---|---|
| `inputs()` | `uint8_t` | Estado de I0_0..I0_7 como bitmap |
| `outputs()` | `uint8_t` | Estado lógico de Q0_0..Q0_7 como bitmap |
| `input(index)` | `bool` | Estado de una entrada 0..7 |
| `output(index)` | `bool` | Estado de una salida 0..7 |
| `ready()` | `bool` | Indica si el snapshot de I/O está disponible |
| `lastScanMs()` | `uint32_t` | Momento del último scan registrado |

### JWPLC_Time — Básico / Intermedio

| Función | Retorno |
|---|---|
| `present()` | RTC detectado |
| `valid()` | Fecha/hora válida |
| `lostPower()` | El RTC reportó pérdida de alimentación |
| `second()` | Segundo |
| `minute()` | Minuto |
| `hour()` | Hora |
| `day()` | Día |
| `month()` | Mes |
| `year()` | Año |
| `dayOfWeek()` | Día de semana |
| `lastUpdateMs()` | Momento de la última actualización del snapshot |

Ejemplo:

```cpp
if (JWPLC_Time.present() &&
    JWPLC_Time.valid())
{
    Serial.println(
        JWPLC_Time.hour());
}
```

### Objetos globales de periféricos

También están disponibles:

```cpp
JWPLC_RTC
JWPLC_FRAM
JWPLC_SD
```

Úsalos cuando necesitas funciones específicas del periférico, por ejemplo leer
la temperatura interna del RTC o guardar datos en FRAM/SD.

No crees una segunda instancia del mismo hardware.

## Errores comunes

### Llamar `JWPLC_Buttons.update()` dentro de `loop()`

No es necesario en JWPLC Basic. El runtime ya escanea la botonera.

### Crear otra instancia de RTC, FRAM o SD

Evítalo. Usa los objetos globales que ya pertenecen al package.

### Usar `delay()` para esperar botones

No necesitas frenar el programa para detectar una pulsación.

Esto:

```cpp
if (JWPLC_Buttons.pressed(BTN_OK))
{
    // acción
}
```

puede convivir con el resto de tu lógica sin pausas artificiales.

### Confundir `pressed()` con `isDown()`

- `pressed()`: una vez por pulsación.
- `isDown()`: verdadero durante todo el tiempo que el botón permanezca abajo.

## API avanzada

### Eventos con repeat

Si necesitas conocer un repeat y su multiplicador puedes leer la cola:

```cpp
for (uint8_t i = 0;
     i < JWPLC_Buttons.eventCount();
     ++i)
{
    JW_MatrixButtons::BtnEvent event;

    if (JWPLC_Buttons.getEvent(i, event))
    {
        if (event.type ==
            JW_MatrixButtons::EV_REPEAT)
        {
            Serial.println(event.mult);
        }
    }
}
```

### Acceso en bloque a I/O

```cpp
uint8_t entradas = JWPLC_readInputs();
uint8_t salidas = JWPLC_readOutputs();

JWPLC_writeOutputs(0b00000001);
```

Estas funciones son útiles para protocolos, Remote I/O o lógica basada en
bitmaps.

## Compatibilidad

Los namespaces:

```cpp
JWPLCButtons::
JWPLCSD::
```

siguen existiendo para integración histórica del package.

Para código nuevo se recomienda:

```cpp
JWPLC_Buttons
JWPLC_SD
```

No uses `JWPLCButtons::begin()` ni `JWPLCSD::begin()` para reinicializar
periféricos que el runtime ya administra.

## Versión

Documentado para:

```text
JWPLC ESP32 v2.1.0-alpha.12
JWPLC_GlobalPeripherals 1.0.0
```
