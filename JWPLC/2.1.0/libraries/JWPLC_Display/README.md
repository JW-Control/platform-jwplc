# JWPLC_Display

`JWPLC_Display` es la librería de alto nivel para usar la pantalla TFT
integrada del **JWPLC Basic**.

Con ella puedes usar la pantalla de estado del equipo, crear una interfaz
`USER`, mostrar valores y estados, trabajar con varias páginas y controlar los
indicadores `RUN`, `ERR`, `BUS` y `ETH`.

## ¿Para qué sirve?

El JWPLC Basic tiene dos vistas principales:

- **IDLE**: pantalla de estado del propio JWPLC;
- **USER**: pantalla de tu aplicación.

En `USER` puedes mostrar, por ejemplo:

```text
Temperatura   62.5 °C
Motor         ON
Estado        CALENTANDO
Carga         75 %
```

Para una HMI normal se recomienda usar `JWPLC_Display` y, cuando corresponda,
**JWPLC HMI Designer**.

Si necesitas dibujar píxeles, líneas o figuras manualmente, utiliza
`JWPLC_TFT` desde la sección avanzada.

## Qué hace automáticamente el JWPLC

La pantalla se inicializa como parte del runtime.

En un sketch normal no tienes que:

- crear un driver para el ST7789;
- configurar los pines de la TFT;
- reinicializar la pantalla;
- administrar el bus compartido;
- refrescar toda la pantalla continuamente.

Puedes comprobar disponibilidad con:

```cpp
if (JWPLC_Display.isReady())
{
    // Display disponible
}
```

pero en la mayoría de sketches ni siquiera necesitas hacer esta comprobación.

## Inicio rápido

Este ejemplo utiliza la pantalla `IDLE`.

```cpp
#include <JWPLC_Display.h>

void setup()
{
    JWPLC_Display.setRunLed(true);
    JWPLC_Display.setErrCode("");

    JWPLC_Display.setBusLedAuto(true);
    JWPLC_Display.setEthLedAuto(true);
}

void loop()
{
}
```

Resultado esperado:

- `RUN` activo;
- sin código de error;
- `BUS` y `ETH` administrados automáticamente por la plataforma.

## Conceptos básicos

### IDLE

Es la pantalla de estado del JWPLC.

Puede mostrar:

- `RUN`;
- `ERR`;
- actividad `BUS`;
- estado `ETH`;
- entradas y salidas;
- RTC.

No tienes que programarla desde cero.

### USER

Es la pantalla de tu aplicación.

Puedes entrar a ella por botones o desde el sketch:

```cpp
JWPLC_Display.enterUserUI();
```

y regresar a IDLE:

```cpp
JWPLC_Display.goIdle();
```

### Fields

Un **Field** es un elemento de la HMI.

La librería soporta cuatro tipos principales:

- `VALUE`: un número;
- `TEXT`: texto;
- `BOOL`: estado tipo ON/OFF;
- `BAR`: barra de nivel.

### Páginas

Una interfaz puede tener varias páginas.

Los IDs de página empiezan en `0`.

### Actualización ON_DEMAND

`USER_REFRESH_ON_DEMAND` significa que la HMI sólo solicita redibujar los
elementos cuyo contenido realmente cambió.

No necesitas llamar `requestUserRefresh()` después de cada `setValue()`.

### PixelMap

Un **PixelMap** es una imagen registrada en la HMI.

Si utilizas HMI Designer, el archivo generado se encarga de registrar las
imágenes y sus IDs. Esos nombres dependen de cada proyecto; no son constantes
universales del package.

## Ejemplo 1 — Básico: IDLE y botonera

- `OK`: alterna un error de prueba;
- `DOWN`: alterna `RUN`.

```cpp
#include <JWPLC_Display.h>

bool run = true;
bool errorActivo = false;

void setup()
{
    JWPLC_Display.setIdleWakeMode(
        IDLE_WAKE_DISABLED);

    JWPLC_Display.setRunLed(run);
    JWPLC_Display.setErrCode("");

    JWPLC_Display.setBusLedAuto(true);
    JWPLC_Display.setEthLedAuto(true);

    JWPLC_Buttons.clearPendingInput();
}

void loop()
{
    if (JWPLC_Buttons.pressed(BTN_OK))
    {
        errorActivo = !errorActivo;

        JWPLC_Display.setErrCode(
            errorActivo ? "TST" : "");
    }

    if (JWPLC_Buttons.pressed(BTN_DOWN))
    {
        run = !run;

        JWPLC_Display.setRunLed(run);
    }
}
```

`setErrCode()` acepta de 1 a 4 caracteres alfanuméricos. Una cadena vacía
elimina el error.

## Ejemplo 2 — Intermedio: primera HMI con Fields

Este ejemplo crea manualmente cuatro Fields. Es útil para entender el modelo
antes de utilizar HMI Designer.

```cpp
#include <JWPLC_Display.h>

enum FieldId : uint8_t
{
    FIELD_CONTADOR = 1,
    FIELD_ESTADO,
    FIELD_MOTOR,
    FIELD_NIVEL
};

static const JWPLC_UIField FIELDS[] =
{
    JWPLC_UIValueField(
        FIELD_CONTADOR,
        10, 18,
        "Contador", "",
        JWPLC_UIValueFormat(
            5, 0, false, false)),

    JWPLC_UITextField(
        FIELD_ESTADO,
        10, 52,
        "Estado",
        12),

    JWPLC_UIBoolField(
        FIELD_MOTOR,
        10, 86,
        "Motor",
        JWPLC_UIBoolText(
            "OFF", "ON")),

    JWPLC_UIBarField(
        FIELD_NIVEL,
        10, 120,
        "Nivel",
        JWPLC_UIRange(
            0.0f, 100.0f),
        200, 28)
};

uint32_t contador = 0;
bool motor = false;
float nivel = 40.0f;

void setup()
{
    JWPLC_Display.setIdleWakeMode(
        IDLE_WAKE_BUTTON_ONLY);

    JWPLC_Display.setIdleWakeButton(
        BTN_OK);

    JWPLC_Display.setIdleReturnMode(
        IDLE_RETURN_ESC_ONLY);

    JWPLC_Display.setUserRefreshMode(
        USER_REFRESH_ON_DEMAND);

    JWPLC_Display.setFields(
        FIELDS,
        sizeof(FIELDS) /
        sizeof(FIELDS[0]));

    JWPLC_Display.setValue(
        FIELD_CONTADOR,
        contador);

    JWPLC_Display.setValue(
        FIELD_ESTADO,
        "LISTO");

    JWPLC_Display.setValue(
        FIELD_MOTOR,
        motor);

    JWPLC_Display.setBar(
        FIELD_NIVEL,
        nivel);
}

void loop()
{
    static uint32_t ultimaActualizacion = 0;

    if (millis() - ultimaActualizacion >= 1000)
    {
        ultimaActualizacion = millis();

        contador++;

        motor =
            (contador % 2) != 0;

        nivel += 10.0f;

        if (nivel > 100.0f)
        {
            nivel = 0.0f;
        }

        JWPLC_Display.setValue(
            FIELD_CONTADOR,
            contador);

        JWPLC_Display.setValue(
            FIELD_ESTADO,
            motor ? "MARCHA" : "PARADO");

        JWPLC_Display.setValue(
            FIELD_MOTOR,
            motor);

        JWPLC_Display.setBar(
            FIELD_NIVEL,
            nivel);
    }
}
```

No hay `delay(1000)`: la lógica del programa puede seguir ejecutándose.

## Ejemplo 3 — Aplicación real: HMI con dos páginas

Página 0 muestra entradas. Página 1 muestra la hora.

```cpp
#include <JWPLC_Display.h>

enum FieldId : uint8_t
{
    FIELD_ENTRADAS = 1,
    FIELD_I0,
    FIELD_HORA,
    FIELD_RTC_OK
};

static const JWPLC_UIField FIELDS[] =
{
    JWPLC_UIValueField(
        FIELD_ENTRADAS,
        20, 45,
        "Entradas", "",
        JWPLC_UIValueFormat(
            3, 0, false, false),
        0),

    JWPLC_UIBoolField(
        FIELD_I0,
        20, 90,
        "I0_0",
        JWPLC_UIBoolText(
            "LOW", "HIGH"),
        0),

    JWPLC_UITextField(
        FIELD_HORA,
        20, 45,
        "Hora",
        12,
        1),

    JWPLC_UIBoolField(
        FIELD_RTC_OK,
        20, 90,
        "RTC",
        JWPLC_UIBoolText(
            "INVALID", "OK"),
        1)
};

char hora[12] = "--:--:--";

void setup()
{
    JWPLC_Display.setIdleWakeMode(
        IDLE_WAKE_BUTTON_ONLY);

    JWPLC_Display.setIdleWakeButton(
        BTN_OK);

    JWPLC_Display.setIdleReturnMode(
        IDLE_RETURN_ESC_ONLY);

    JWPLC_Display.setUserRefreshMode(
        USER_REFRESH_ON_DEMAND);

    JWPLC_Display.setFields(
        FIELDS,
        sizeof(FIELDS) /
        sizeof(FIELDS[0]));

    JWPLC_Display.setUserPageCount(2);
    JWPLC_Display.setUserPage(0);

    JWPLC_Buttons.clearPendingInput();
}

void loop()
{
    if (JWPLC_Buttons.pressed(BTN_LEFT))
    {
        JWPLC_Display.setUserPage(0);
    }

    if (JWPLC_Buttons.pressed(BTN_RIGHT))
    {
        JWPLC_Display.setUserPage(1);
    }

    static uint32_t ultimaActualizacion = 0;

    if (millis() - ultimaActualizacion >= 500)
    {
        ultimaActualizacion = millis();

        JWPLC_Display.setValue(
            FIELD_ENTRADAS,
            JWPLC_IO.inputs());

        JWPLC_Display.setValue(
            FIELD_I0,
            JWPLC_IO.input(0));

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

        JWPLC_Display.setValue(
            FIELD_HORA,
            hora);

        JWPLC_Display.setValue(
            FIELD_RTC_OK,
            JWPLC_Time.valid());
    }
}
```

## Ejemplo 4 — Avanzado de usuario: dibujo directo

Cuando una interfaz no puede expresarse con Fields puedes obtener el renderer
`JWPLC_TFT`.

```cpp
#include <JWPLC_Display.h>

void setup()
{
    JWPLC_Display.setIdleWakeMode(
        IDLE_WAKE_BUTTON_ONLY);

    JWPLC_Display.setIdleWakeButton(
        BTN_OK);

    JWPLC_Display.setIdleReturnMode(
        IDLE_RETURN_ESC_ONLY);
}

void loop()
{
    static bool dibujado = false;

    if (!JWPLC_Display.isIdleMode() &&
        !dibujado)
    {
        dibujado = true;

        auto &tft =
            JWPLC_Display.tft();

        tft.fillScreen(
            JWPLC_TFT_BLACK);

        tft.setCursor(
            20, 20);

        tft.setTextColor(
            JWPLC_TFT_CYAN);

        tft.setTextSize(2);

        tft.println(
            "JWPLC USER");
    }

    if (JWPLC_Display.isIdleMode())
    {
        dibujado = false;
    }
}
```

Para dibujo continuo o animaciones revisa el README de `JWPLC_TFT` y los
ejemplos oficiales de Display. Una HMI normal debería seguir usando Fields o
HMI Designer.

## API de usuario

### Estado — Básico

| Función | Qué hace | Nivel |
|---|---|---|
| `isReady()` | Indica si Display está disponible | Básico |
| `isIdleMode()` | Indica si se muestra IDLE | Básico |
| `buttonsReady()` | Indica si la botonera usada por Display está lista | Intermedio |
| `forceRedraw()` | Solicita redibujar completamente la vista actual | Avanzado |

### Entrar y salir de USER — Básico

```cpp
JWPLC_Display.enterUserUI();
JWPLC_Display.goIdle();
```

Si utilizas un timeout de actividad:

```cpp
JWPLC_Display.notifyActivity();
```

### Wake desde IDLE — Básico / Intermedio

```cpp
JWPLC_Display.setIdleWakeMode(
    IDLE_WAKE_ANY_BUTTON);

JWPLC_Display.setIdleWakeButton(
    BTN_OK);
```

Modos válidos:

```text
IDLE_WAKE_ANY_BUTTON
IDLE_WAKE_BUTTON_ONLY
IDLE_WAKE_DISABLED
```

Getters:

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

Configurar:

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

### Refresco de USER — Intermedio

Modo recomendado para HMIs normales:

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

No la llames después de cada `setValue()` en modo ON_DEMAND.

El periodo de IDLE también puede consultarse/configurarse:

```cpp
JWPLC_Display.setIdleRefreshPeriodMs(
    500);

JWPLC_Display.idleRefreshPeriodMs();
```

Nivel: **Avanzado de usuario**.

### Páginas — Intermedio

```cpp
JWPLC_Display.setUserPage(0);

uint8_t pagina =
    JWPLC_Display.userPage();
```

Para navegación multipágina:

```cpp
JWPLC_Display.setUserPageCount(3);

uint8_t total =
    JWPLC_Display.userPageCount();

bool seleccionando =
    JWPLC_Display.isUserPageSelection();
```

HMI Designer puede configurar el número de páginas por ti.

### Fields — Básico / Intermedio

Registrar manualmente:

```cpp
JWPLC_Display.setFields(
    FIELDS,
    count);
```

Consultar:

```cpp
JWPLC_Display.fieldCount();
```

Eliminar:

```cpp
JWPLC_Display.clearFields();
```

Actualizar un Field:

```cpp
JWPLC_Display.setValue(
    FIELD_ID,
    valor);
```

`setValue()` admite valores numéricos, `bool` y texto.

Variantes explícitas:

```cpp
JWPLC_Display.setText(
    FIELD_ID,
    "RUN");

JWPLC_Display.setBool(
    FIELD_ID,
    true);

JWPLC_Display.setBar(
    FIELD_ID,
    75.0f);
```

Para código nuevo se recomienda `setValue()` cuando representa correctamente
el tipo de Field. Las barras usan `setBar()`.

### Invalidación — Avanzado de usuario

```cpp
JWPLC_Display.invalidateField(
    FIELD_ID);

JWPLC_Display.invalidateAllFields();
```

Úsalas cuando necesitas forzar el redibujado de Fields sin cambiar su valor.

### PixelMaps — Intermedio / Avanzado

Si HMI Designer registró PixelMaps, el sketch normalmente sólo necesita:

```cpp
JWPLC_Display.setPixelMapVisible(
    indice,
    true);

bool visible =
    JWPLC_Display.isPixelMapVisible(
        indice);
```

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

El registro manual es **Avanzado de usuario**. Si utilizas HMI Designer, deja
que el archivo generado elija la representación.

### Indicador RUN — Básico

```cpp
JWPLC_Display.setRunLed(true);

bool run =
    JWPLC_Display.runLed();
```

### Indicador ERR — Básico

Recomendado:

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

Para una aplicación normal se recomienda el modo automático.

### Entrada pendiente — Avanzado de usuario

```cpp
JWPLC_Display.clearPendingInput();
```

Limpia eventos pendientes usados por la navegación del Display. Normalmente no
es necesario llamarla continuamente.

## Errores comunes

### Forzar un redraw en cada vuelta

Evita:

```cpp
void loop()
{
    JWPLC_Display.forceRedraw();
}
```

Actualiza los valores y deja que Display decida qué necesita redibujarse.

### Llamar `requestUserRefresh()` después de cada `setValue()`

En `USER_REFRESH_ON_DEMAND`, `setValue()` ya detecta el cambio del Field.

### Usar `setIdleReturnButton()` con `IDLE_RETURN_ESC_ONLY`

No hace falta. El botón personalizado sólo aplica con:

```text
IDLE_RETURN_BUTTON_ONLY
```

### Inventar IDs de Fields, páginas o PixelMaps

Los IDs deben estar definidos en tu sketch o en el archivo generado por HMI
Designer.

No asumas que nombres como `FIELD_TEMP` o `PAGE_ALARMAS` existen en todos
los proyectos.

### Crear otro driver para la misma TFT

No crees una segunda instancia de ST7789/TFT para la pantalla integrada.

## API avanzada

### Crear Fields manualmente

Helpers disponibles en `JWPLC_UI.h`:

```text
JWPLC_UIValueField(...)
JWPLC_UITextField(...)
JWPLC_UIBoolField(...)
JWPLC_UIBarField(...)
```

También existen estructuras de estilo/configuración como:

```text
JWPLC_UIValueFormat
JWPLC_UIBoolText
JWPLC_UIRange
JWPLC_UIRect
JWPLC_UIText
JWPLC_UIColors
```

Para un usuario que está empezando, HMI Designer suele ser más sencillo.

### Dibujo directo

Acceso recomendado:

```cpp
auto &tft =
    JWPLC_Display.tft();
```

La API completa está documentada en:

```text
JWPLC_TFT/README.md
```

## Compatibilidad

`display()` se conserva como alias:

```cpp
auto &tft =
    JWPLC_Display.display();
```

Para código nuevo se recomienda:

```cpp
JWPLC_Display.tft();
```

La API histórica `JWPLCDisplay::` continúa existiendo para compatibilidad con
sketches anteriores, pero no debe usarse como punto de partida para código
nuevo.

También se conservan los controles históricos del LED ERR:

```cpp
JWPLC_Display.setErrLed(true);

bool err =
    JWPLC_Display.errLed();
```

Para código nuevo se recomienda `setErrCode()`, porque además de indicar que
existe un error permite mostrar un código identificable.

El tipo gráfico público actual es `JWPLC_TFTClass&`. Código antiguo que
declaraba explícitamente `Adafruit_ST7789&` debe migrarse.

## Versión

Documentado para:

```text
JWPLC ESP32 v2.1.0-alpha.12
JWPLC_Display 1.0.1
```

Los detalles de arquitectura interna, precompilación, pruebas y backend se
mantienen en la documentación para desarrolladores, no en esta guía de usuario.
