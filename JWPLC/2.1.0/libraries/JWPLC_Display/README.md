# JWPLC_Display

`JWPLC_Display` es la librería de alto nivel para usar la pantalla TFT
integrada del **JWPLC Basic**.

Para una HMI normal, el camino recomendado es diseñar la interfaz con
**JWPLC HMI Designer**, generar `JWPLC_HMI_Generated.h` y trabajar desde el
sketch con las variables que el Designer creó para esa interfaz.

## ¿Para qué sirve?

Con `JWPLC_Display` puedes:

- usar la pantalla de estado `IDLE` del JWPLC;
- crear una interfaz `USER`;
- mostrar valores, textos, estados y barras;
- trabajar con varias páginas;
- utilizar imágenes PixelMap;
- controlar los indicadores `RUN`, `ERR`, `BUS` y `ETH`;
- crear una HMI visual con JWPLC HMI Designer;
- acceder a dibujo directo cuando una interfaz necesita algo más personalizado.

Para un usuario que recién empieza, la idea principal es:

```text
Diseño mi HMI con JWPLC HMI Designer
              ↓
el Designer genera JWPLC_HMI_Generated.h
              ↓
el header contiene variables de mi HMI
              ↓
mi sketch modifica esas variables
              ↓
JWPLC actualiza la TFT automáticamente
```

## Qué hace automáticamente el JWPLC

La pantalla y la botonera se inicializan como parte del runtime del JWPLC.

Si utilizas HMI Designer, además:

- el archivo generado registra los Fields, páginas y recursos de la HMI;
- `jwplcUIUpdate()` sincroniza las variables generadas con la pantalla;
- el runtime llama `jwplcUIUpdate()` automáticamente mientras USER está activo.

Por tanto, el sketch **no debe llamar manualmente `jwplcUIUpdate()` desde
`loop()`**.

Tampoco necesitas crear otro driver de pantalla, configurar pines de la TFT ni
reinicializar el hardware.

## Inicio rápido

Supongamos que en HMI Designer creaste dos indicadores BOOL y les asignaste las
variables:

```text
q0
q1
```

El Designer genera `JWPLC_HMI_Generated.h`.

Tu sketch puede quedar así:

```cpp
#include <JWPLC_Display.h>
#include <JWPLC_HMI_Generated.h>

void setup()
{
    jwplcHMISetup();

    JWPLC_Display.setIdleWakeMode(
        IDLE_WAKE_BUTTON_ONLY);

    JWPLC_Display.setIdleWakeButton(
        BTN_OK);

    JWPLC_Display.setIdleReturnMode(
        IDLE_RETURN_ESC_ONLY);
}

void loop()
{
    q0 = digitalRead(I0_0);
    q1 = digitalRead(I0_1);
}
```

Eso es suficiente.

No añadas:

```cpp
jwplcUIUpdate();
```

El runtime ya se encarga de ejecutar esa sincronización.

> Los nombres `q0` y `q1` son ejemplos de variables definidas en el
> proyecto HMI. Los nombres reales dependen de lo que configures en Designer.

## Conceptos básicos

### IDLE

`IDLE` es la pantalla de estado del JWPLC.

Puede mostrar información como:

- `RUN`;
- `ERR`;
- actividad `BUS`;
- estado `ETH`;
- E/S;
- RTC.

No tienes que construirla manualmente.

### USER

`USER` es la pantalla de tu aplicación.

Puede abrirse desde un botón configurado:

```cpp
JWPLC_Display.setIdleWakeMode(
    IDLE_WAKE_BUTTON_ONLY);

JWPLC_Display.setIdleWakeButton(
    BTN_OK);
```

y puede regresar a IDLE con:

```cpp
JWPLC_Display.setIdleReturnMode(
    IDLE_RETURN_ESC_ONLY);
```

También existen:

```cpp
JWPLC_Display.enterUserUI();
JWPLC_Display.goIdle();
```

para cambios explícitos desde el sketch.

### Qué genera HMI Designer

El archivo:

```text
JWPLC_HMI_Generated.h
```

puede contener, según tu proyecto:

- IDs de páginas `PAGE_*`;
- IDs de Fields `FIELD_*`;
- variables HMI;
- `JWPLC_UIField[]`;
- PixelMaps;
- `jwplcHMISetup()`;
- `jwplcUIUpdate()`.

Por ejemplo, un proyecto puede generar:

```cpp
bool q0 = false;
bool q1 = false;

float temperatura = 0.0f;

char estado[16] = {};
```

Tu sketch trabaja normalmente con esas variables.

### `jwplcHMISetup()`

Debes llamarla una vez desde `setup()`:

```cpp
void setup()
{
    jwplcHMISetup();
}
```

Esta función registra la HMI generada y prepara sus recursos.

### `jwplcUIUpdate()`

El Designer genera la sincronización gráfica.

Conceptualmente puede contener llamadas como:

```cpp
JWPLC_Display.setBool(
    FIELD_Q0,
    q0);

JWPLC_Display.setText(
    FIELD_ESTADO,
    estado);
```

pero el usuario de HMI Designer **no debe repetir esas llamadas en el
`.ino`**.

El runtime del Display llama automáticamente:

```cpp
jwplcUIUpdate();
```

durante el refresco de USER.

### Variables generadas

La variable es el puente entre tu lógica y la pantalla.

Si Designer generó:

```cpp
bool motor = false;
```

tu sketch puede hacer:

```cpp
motor = digitalRead(I0_0);
```

Si generó:

```cpp
float temperatura = 0.0f;
```

puedes hacer:

```cpp
temperatura = 62.5f;
```

### Variables TEXT

Para textos generados como arreglos `char[]`, usa funciones con límite de
tamaño.

Ejemplo:

```cpp
snprintf(
    estado,
    sizeof(estado),
    "%s",
    motor ? "MARCHA" : "PARADO");
```

Evita copiar texto sin comprobar la capacidad del buffer.

### No editar el header generado

> **No edites manualmente `JWPLC_HMI_Generated.h`.**

Es un archivo generado por HMI Designer y puede sobrescribirse cuando vuelvas
a actualizar la HMI.

Las decisiones visuales —Fields, páginas, tamaños, colores, PixelMaps— deben
hacerse en Designer.

La lógica de la máquina debe permanecer en tu `.ino`.

## Ejemplo 1 — Básico: mostrar entradas en una HMI generada

Supongamos que Designer generó:

```cpp
bool q0 = false;
bool q1 = false;
```

y que esos valores están asociados a dos indicadores BOOL.

```cpp
#include <JWPLC_Display.h>
#include <JWPLC_HMI_Generated.h>

void setup()
{
    jwplcHMISetup();

    JWPLC_Display.setIdleWakeMode(
        IDLE_WAKE_BUTTON_ONLY);

    JWPLC_Display.setIdleWakeButton(
        BTN_OK);

    JWPLC_Display.setIdleReturnMode(
        IDLE_RETURN_ESC_ONLY);
}

void loop()
{
    q0 = digitalRead(I0_0);
    q1 = digitalRead(I0_1);
}
```

El alumno sólo modifica las variables.

La sincronización con los Fields está dentro del código generado.

## Ejemplo 2 — Intermedio: lógica de usuario y HMI

Supongamos que Designer generó:

```cpp
bool motor = false;
```

El botón `OK` alterna el motor, la salida física y la HMI.

```cpp
#include <JWPLC_Display.h>
#include <JWPLC_HMI_Generated.h>

void setup()
{
    pinMode(Q0_0, OUTPUT);

    digitalWrite(
        Q0_0,
        LOW);

    jwplcHMISetup();

    JWPLC_Display.setIdleWakeMode(
        IDLE_WAKE_BUTTON_ONLY);

    JWPLC_Display.setIdleWakeButton(
        BTN_OK);

    JWPLC_Display.setIdleReturnMode(
        IDLE_RETURN_ESC_ONLY);

    JWPLC_Buttons.clearPendingInput();
}

void loop()
{
    if (JWPLC_Buttons.pressed(BTN_OK))
    {
        motor = !motor;

        digitalWrite(
            Q0_0,
            motor ? HIGH : LOW);
    }
}
```

La misma variable `motor` representa el estado de proceso y alimenta la HMI.

## Ejemplo 3 — Aplicación real: valor, estado y hora

Supongamos que Designer generó:

```cpp
float temperatura = 0.0f;

bool alarma = false;

char estado[16] = {};
char hora[9] = {};
```

Este ejemplo simula una temperatura y actualiza los textos sin bloquear el
programa.

```cpp
#include <JWPLC_Display.h>
#include <JWPLC_HMI_Generated.h>

uint32_t ultimaActualizacion = 0;

void setup()
{
    jwplcHMISetup();

    JWPLC_Display.setIdleWakeMode(
        IDLE_WAKE_BUTTON_ONLY);

    JWPLC_Display.setIdleWakeButton(
        BTN_OK);

    JWPLC_Display.setIdleReturnMode(
        IDLE_RETURN_ESC_ONLY);
}

void loop()
{
    if (millis() - ultimaActualizacion < 500)
    {
        return;
    }

    ultimaActualizacion = millis();

    temperatura =
        20.0f +
        (float)((millis() / 1000UL) % 70UL);

    alarma =
        temperatura >= 80.0f;

    snprintf(
        estado,
        sizeof(estado),
        "%s",
        alarma ? "ALTA" : "NORMAL");

    if (JWPLC_Time.valid())
    {
        snprintf(
            hora,
            sizeof(hora),
            "%02u:%02u:%02u",
            JWPLC_Time.hour(),
            JWPLC_Time.minute(),
            JWPLC_Time.second());
    }
    else
    {
        snprintf(
            hora,
            sizeof(hora),
            "--:--:--");
    }
}
```

Observa que:

- no se llama `jwplcUIUpdate()`;
- no se llama `setValue()`, `setBool()` ni `setText()`;
- no se usa `delay(500)`.

La HMI generada realiza la sincronización.

## Ejemplo 4 — Avanzado de usuario: HMI manual sin Designer

La API manual sigue siendo pública y válida.

Úsala cuando tengas una razón concreta para construir los Fields desde código
en lugar de usar Designer.

```cpp
#include <JWPLC_Display.h>

enum FieldId : uint8_t
{
    FIELD_CONTADOR = 1,
    FIELD_MOTOR
};

static const JWPLC_UIField FIELDS[] =
{
    JWPLC_UIValueField(
        FIELD_CONTADOR,
        20, 40,
        "Contador", "",
        JWPLC_UIValueFormat(
            5, 0, false, false)),

    JWPLC_UIBoolField(
        FIELD_MOTOR,
        20, 90,
        "Motor",
        JWPLC_UIBoolText(
            "OFF", "ON"))
};

uint32_t contador = 0;
bool motor = false;
uint32_t ultimaActualizacion = 0;

void setup()
{
    JWPLC_Display.setFields(
        FIELDS,
        sizeof(FIELDS) /
        sizeof(FIELDS[0]));

    JWPLC_Display.setUserRefreshMode(
        USER_REFRESH_ON_DEMAND);

    JWPLC_Display.setIdleWakeMode(
        IDLE_WAKE_BUTTON_ONLY);

    JWPLC_Display.setIdleWakeButton(
        BTN_OK);

    JWPLC_Display.setIdleReturnMode(
        IDLE_RETURN_ESC_ONLY);
}

void loop()
{
    if (millis() - ultimaActualizacion >= 1000)
    {
        ultimaActualizacion = millis();

        contador++;
        motor = !motor;

        JWPLC_Display.setValue(
            FIELD_CONTADOR,
            contador);

        JWPLC_Display.setBool(
            FIELD_MOTOR,
            motor);
    }
}
```

Este es un flujo **avanzado / HMI manual**.

Si tu interfaz fue creada con HMI Designer, normalmente no necesitas escribir
esas llamadas.

## API de usuario

### Flujo recomendado con HMI Designer — Básico

1. Diseña la interfaz.
2. Genera o actualiza `JWPLC_HMI_Generated.h`.
3. Incluye el archivo en tu sketch.
4. Llama `jwplcHMISetup()` en `setup()`.
5. Modifica las variables HMI desde tu lógica.
6. No llames manualmente `jwplcUIUpdate()`.
7. No repitas los setters que el Designer ya generó.
8. No edites manualmente el header generado.

Esqueleto recomendado:

```cpp
#include <JWPLC_Display.h>
#include <JWPLC_HMI_Generated.h>

void setup()
{
    jwplcHMISetup();
}

void loop()
{
    // lógica de tu aplicación
    // modifica aquí las variables generadas
}
```

### Estado del Display — Básico

| Función | Qué hace | Nivel |
|---|---|---|
| `isReady()` | Indica si Display está disponible | Básico |
| `isIdleMode()` | Indica si se muestra IDLE | Básico |
| `buttonsReady()` | Indica si la botonera está lista | Intermedio |
| `forceRedraw()` | Solicita redibujar la vista actual | Avanzado |

### Entrar y salir de USER — Básico

```cpp
JWPLC_Display.enterUserUI();
JWPLC_Display.goIdle();
```

Para notificar actividad cuando utilizas timeout:

```cpp
JWPLC_Display.notifyActivity();
```

### Wake desde IDLE — Básico / Intermedio

Configurar cualquier botón:

```cpp
JWPLC_Display.setIdleWakeMode(
    IDLE_WAKE_ANY_BUTTON);
```

Botón específico:

```cpp
JWPLC_Display.setIdleWakeMode(
    IDLE_WAKE_BUTTON_ONLY);

JWPLC_Display.setIdleWakeButton(
    BTN_OK);
```

Deshabilitado:

```cpp
JWPLC_Display.setIdleWakeMode(
    IDLE_WAKE_DISABLED);
```

Modos válidos:

```text
IDLE_WAKE_ANY_BUTTON
IDLE_WAKE_BUTTON_ONLY
IDLE_WAKE_DISABLED
```

Consultar:

```cpp
JWPLC_Display.idleWakeMode();
JWPLC_Display.idleWakeButton();
```

### Retorno a IDLE — Básico / Intermedio

Modos válidos:

```text
IDLE_RETURN_TIMEOUT
IDLE_RETURN_ESC_ONLY
IDLE_RETURN_DISABLED
IDLE_RETURN_BUTTON_ONLY
```

Timeout:

```cpp
JWPLC_Display.setIdleReturnMode(
    IDLE_RETURN_TIMEOUT);

JWPLC_Display.setIdleTimeoutMs(
    15000);
```

Botón personalizado:

```cpp
JWPLC_Display.setIdleReturnMode(
    IDLE_RETURN_BUTTON_ONLY);

JWPLC_Display.setIdleReturnButton(
    BTN_DOWN);
```

Consultar:

```cpp
JWPLC_Display.idleReturnMode();
JWPLC_Display.idleReturnButton();
JWPLC_Display.idleTimeoutMs();
```

### Indicador RUN — Básico

```cpp
JWPLC_Display.setRunLed(true);

bool run =
    JWPLC_Display.runLed();
```

### Indicador ERR — Básico

Camino recomendado:

```cpp
JWPLC_Display.setErrCode("A01");

const char *codigo =
    JWPLC_Display.errCode();
```

Para limpiar:

```cpp
JWPLC_Display.setErrCode("");
```

### Indicadores BUS y ETH — Básico

Automático:

```cpp
JWPLC_Display.setBusLedAuto(true);
JWPLC_Display.setEthLedAuto(true);
```

Consultar:

```cpp
JWPLC_Display.busLed();
JWPLC_Display.busLedAuto();

JWPLC_Display.ethLed();
JWPLC_Display.ethLedAuto();
```

Control manual:

```cpp
JWPLC_Display.setBusLed(false);
JWPLC_Display.setEthLed(false);
```

Para una aplicación normal se recomienda mantener BUS y ETH automáticos.

### Páginas — Intermedio

HMI Designer puede generar IDs `PAGE_*`.

El nombre exacto depende de cada proyecto.

La API manual de navegación es:

```cpp
JWPLC_Display.setUserPage(
    pagina);

uint8_t pagina =
    JWPLC_Display.userPage();

JWPLC_Display.setUserPageCount(
    total);

uint8_t total =
    JWPLC_Display.userPageCount();

bool seleccionando =
    JWPLC_Display.isUserPageSelection();
```

Si Designer ya configura las páginas, no repitas esa configuración sin una
razón específica.

### Refresco USER — Intermedio / Avanzado

Modo bajo demanda:

```cpp
JWPLC_Display.setUserRefreshMode(
    USER_REFRESH_ON_DEMAND);
```

Modo periódico:

```cpp
JWPLC_Display.setUserRefreshMode(
    USER_REFRESH_PERIODIC);

JWPLC_Display.setUserRefreshPeriodMs(
    100);
```

Consultar:

```cpp
JWPLC_Display.userRefreshMode();
JWPLC_Display.userRefreshPeriodMs();
```

Forzar una solicitud:

```cpp
JWPLC_Display.requestUserRefresh();
```

Cuando utilizas HMI Designer, deja que el código generado configure su
estrategia salvo que sepas que necesitas modificarla.

### Refresco IDLE — Avanzado

```cpp
JWPLC_Display.setIdleRefreshPeriodMs(
    500);

uint32_t periodo =
    JWPLC_Display.idleRefreshPeriodMs();
```

No es necesario cambiarlo en una aplicación normal.

## Errores comunes

### Llamar `jwplcUIUpdate()` manualmente desde `loop()`

No hagas esto:

```cpp
void loop()
{
    // lógica

    jwplcUIUpdate();
}
```

El runtime ya la ejecuta al actualizar USER.

### Duplicar los setters generados

Si el Designer ya sincroniza una variable:

```cpp
motor = true;
```

no necesitas además:

```cpp
JWPLC_Display.setBool(
    FIELD_MOTOR,
    motor);
```

### Editar `JWPLC_HMI_Generated.h`

No lo edites a mano.

Puede ser sobrescrito la próxima vez que actualices la HMI.

### Inventar IDs universales

Nombres como:

```text
FIELD_TEMP
PAGE_ALARMAS
PIXELMAP_ALARMA
```

no son constantes universales del package.

Sólo existen si tu proyecto generado los define.

### Usar `strcpy()` o conversiones temporales para TEXT sin revisar capacidad

Prefiere:

```cpp
snprintf(
    texto,
    sizeof(texto),
    "%s",
    valor);
```

### Forzar un redraw en cada vuelta

Evita:

```cpp
void loop()
{
    JWPLC_Display.forceRedraw();
}
```

Actualiza datos sólo cuando corresponda.

### Crear otro driver para la TFT

La pantalla integrada ya pertenece al runtime del JWPLC.

## API avanzada

### HMI manual: registrar Fields

```cpp
JWPLC_Display.setFields(
    fields,
    count);

size_t total =
    JWPLC_Display.fieldCount();

JWPLC_Display.clearFields();
```

Helpers disponibles:

```text
JWPLC_UIValueField(...)
JWPLC_UITextField(...)
JWPLC_UIBoolField(...)
JWPLC_UIBarField(...)
```

También existen configuraciones como:

```text
JWPLC_UIValueFormat
JWPLC_UIBoolText
JWPLC_UIRange
JWPLC_UIRect
JWPLC_UIText
JWPLC_UIColors
```

### HMI manual: actualizar Fields

API pública:

```cpp
JWPLC_Display.setValue(
    fieldId,
    valor);

JWPLC_Display.setBool(
    fieldId,
    estado);

JWPLC_Display.setText(
    fieldId,
    texto);

JWPLC_Display.setBar(
    fieldId,
    porcentaje);
```

También existe:

```cpp
JWPLC_Display.setNumericValue(
    fieldId,
    valor);
```

Para código nuevo se recomienda `setValue()` cuando corresponde al tipo del
Field.

> Si tu interfaz fue creada con HMI Designer, normalmente no necesitas llamar
> estos setters directamente. El código generado ya realiza esa sincronización
> dentro de `jwplcUIUpdate()`.

### Invalidación manual

```cpp
JWPLC_Display.invalidateField(
    fieldId);

JWPLC_Display.invalidateAllFields();
```

Sirve para forzar el redibujado de Fields sin cambiar su valor.

### PixelMaps

Registro manual:

```cpp
JWPLC_Display.setPixelMaps(
    maps,
    count);

JWPLC_Display.setPackedPixelMaps(
    packedMaps,
    count);

JWPLC_Display.clearPixelMaps();

size_t total =
    JWPLC_Display.pixelMapCount();
```

Visibilidad:

```cpp
JWPLC_Display.setPixelMapVisible(
    indice,
    true);

bool visible =
    JWPLC_Display.isPixelMapVisible(
        indice);
```

Si HMI Designer generó los PixelMaps, deja que el header generado haga el
registro.

### Dibujo directo USER

Para una USER completamente manual, la API corta pública es:

```cpp
extern "C" void jwplcUIEnter();
extern "C" void jwplcUIPageEnter(uint8_t page);
extern "C" void jwplcUIUpdate();
extern "C" void jwplcUIExit();
```

Dentro de esos callbacks puedes acceder a la TFT:

```cpp
auto &tft =
    JWPLC_Display.tft();
```

- `jwplcUIEnter()`: dibujar/inicializar al entrar a USER.
- `jwplcUIPageEnter(page)`: reaccionar al entrar a una página.
- `jwplcUIUpdate()`: actualizar dibujo manual.
- `jwplcUIExit()`: liberar estado propio al salir.

**Importante:** esta es una API de dibujo manual avanzada.

Cuando HMI Designer genera su propio `jwplcUIUpdate()`, no debes definir otro
con el mismo nombre en tu sketch.

La API gráfica completa está documentada en:

```text
JWPLC_TFT/README.md
```

### Entrada pendiente

```cpp
JWPLC_Display.clearPendingInput();
```

Limpia eventos pendientes usados por navegación. Normalmente no se llama
continuamente.

## Compatibilidad

`display()` se conserva como alias:

```cpp
auto &tft =
    JWPLC_Display.display();
```

Para código nuevo se recomienda:

```cpp
auto &tft =
    JWPLC_Display.tft();
```

La API histórica `JWPLCDisplay::` continúa existiendo para sketches antiguos,
pero no debe enseñarse como camino principal.

También siguen disponibles:

```cpp
JWPLC_Display.setErrLed(true);

bool err =
    JWPLC_Display.errLed();
```

Para código nuevo se recomienda `setErrCode()`, porque permite representar
un error identificable.

El tipo gráfico público actual es `JWPLC_TFTClass&`. Código antiguo que
declaraba explícitamente otro tipo de backend gráfico debe migrarse a:

```cpp
auto &tft =
    JWPLC_Display.tft();
```

## Versión

Documentado para:

```text
JWPLC ESP32 v2.1.0-alpha.12
JWPLC_Display 1.0.1
```

Esta guía documenta el flujo de usuario. La arquitectura interna del renderer,
buses, precompilación y pruebas del package se mantiene fuera del tutorial.
