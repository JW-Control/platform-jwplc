# JWPLC_LogicRuntime_UI

`JWPLC_LogicRuntime_UI` muestra en la pantalla USER información y controles
del motor `JWPLC_LogicRuntime`.

Es una librería **avanzada y experimental**. No es necesaria para usar la
pantalla del JWPLC ni para crear una HMI normal. Para HMIs de aplicación usa
`JWPLC_Display` y HMI Designer.

## ¿Para qué sirve?

Esta librería sirve para explorar o controlar visualmente el motor lógico:

- ver el estado del runtime v1;
- abrir vistas de programa/diagrama;
- iniciar o detener acciones soportadas por la UI v1;
- visualizar el mapa FBD experimental del motor v2;
- integrar esa UI con la pantalla USER del JWPLC.

No es un editor OpenPLC ni implica que OpenPLC esté integrado al package.

## Qué hace automáticamente el JWPLC

Cuando enlazas la UI con un runtime:

```cpp
JWPLC_LogicRuntime_UI.begin(runtime);
```

la librería se integra con `JWPLC_Display`.

No necesitas:

- crear un driver TFT;
- procesar manualmente los callbacks gráficos;
- dibujar las pantallas internas de LogicRuntime;
- sincronizar manualmente los LEDs RUN/ERR de esta UI.

Tu sketch sigue siendo responsable de ejecutar el motor lógico.

## Inicio rápido

Este ejemplo carga una lógica simple y adjunta la UI.

```cpp
#include <JWPLC_LogicRuntime.h>
#include <JWPLC_LogicRuntime_UI.h>

JWPLC_LogicRuntime runtime;

static const LogicBlockDefinition BLOCKS[] =
{
    {
        LogicBlockType::DigitalInput,
        JWPLC_LOGIC_NO_SOURCE,
        JWPLC_LOGIC_NO_SOURCE,
        0,
        0
    },
    {
        LogicBlockType::DigitalOutput,
        0,
        JWPLC_LOGIC_NO_SOURCE,
        0,
        0
    }
};

static const LogicProgram PROGRAM =
{
    "I0_0 a Q0_0",
    BLOCKS,
    2
};

void setup()
{
    runtime.begin();
    runtime.loadProgram(PROGRAM);
    runtime.start();

    JWPLC_Display.setIdleWakeMode(
        IDLE_WAKE_ANY_BUTTON);

    JWPLC_LogicRuntime_UI.begin(
        runtime);
}

void loop()
{
    JWPLC_LogicRuntime_UI.update();

    if (runtime.state() ==
        JWPLCLogicRuntimeState::Running)
    {
        runtime.tick();
    }
}
```

Pulsa un botón para entrar a USER y ver la interfaz del runtime.

## Conceptos básicos

### Runtime y UI son cosas diferentes

`JWPLC_LogicRuntime` ejecuta la lógica.

`JWPLC_LogicRuntime_UI` sólo muestra/controla su interfaz visual.

Por eso el loop conserva:

```cpp
runtime.tick();
```

cuando el runtime está en ejecución.

### Attach

`begin(...)` no “crea” el motor. Conecta la UI a un motor que ya existe.

### update()

```cpp
JWPLC_LogicRuntime_UI.update();
```

procesa trabajo de la UI que debe ejecutarse fuera del dibujo.

Debe llamarse frecuentemente.

### Runtime v1 y motor v2

La UI admite dos backends públicos:

```cpp
begin(JWPLC_LogicRuntime &runtime);
begin(LogicV2EnginePrototype &engine);
```

El motor v2 y su editor siguen siendo experimentales.

## Ejemplo 1 — Básico: UI del runtime v1

```cpp
#include <JWPLC_LogicRuntime.h>
#include <JWPLC_LogicRuntime_UI.h>

JWPLC_LogicRuntime runtime;

static const LogicBlockDefinition BLOCKS[] =
{
    {
        LogicBlockType::DigitalInput,
        JWPLC_LOGIC_NO_SOURCE,
        JWPLC_LOGIC_NO_SOURCE,
        0,
        0
    },
    {
        LogicBlockType::Not,
        0,
        JWPLC_LOGIC_NO_SOURCE,
        0,
        0
    }
};

static const LogicProgram PROGRAM =
{
    "NOT I0_0",
    BLOCKS,
    2
};

void setup()
{
    runtime.begin();
    runtime.loadProgram(PROGRAM);
    runtime.start();

    JWPLC_Display.setIdleWakeMode(
        IDLE_WAKE_ANY_BUTTON);

    JWPLC_LogicRuntime_UI.begin(
        runtime);
}

void loop()
{
    JWPLC_LogicRuntime_UI.update();

    if (runtime.state() ==
        JWPLCLogicRuntimeState::Running)
    {
        runtime.tick();
    }
}
```

## Ejemplo 2 — Intermedio: comprobar que la UI está enlazada

```cpp
#include <JWPLC_LogicRuntime.h>
#include <JWPLC_LogicRuntime_UI.h>

JWPLC_LogicRuntime runtime;

void setup()
{
    Serial.begin(115200);

    runtime.begin();

    const bool uiOk =
        JWPLC_LogicRuntime_UI.begin(
            runtime);

    Serial.print("UI attached: ");
    Serial.println(uiOk);
}

void loop()
{
    JWPLC_LogicRuntime_UI.update();

    static uint32_t ultimoReporte = 0;

    if (millis() - ultimoReporte >= 1000)
    {
        ultimoReporte = millis();

        Serial.print("Attached=");
        Serial.println(
            JWPLC_LogicRuntime_UI.isAttached());
    }
}
```

Este ejemplo sólo demuestra la unión UI/runtime. Un proyecto real debe cargar
un programa antes de iniciar su ejecución.

## Ejemplo 3 — Aplicación real: runtime almacenado + UI

Si tu proyecto utiliza el almacenamiento v1, puedes preparar el programa antes
de abrir la UI.

```cpp
#include <JWPLC_LogicRuntime.h>
#include <JWPLC_LogicRuntime_UI.h>

JWPLC_LogicRuntime runtime;

void setup()
{
    Serial.begin(115200);

    runtime.storage().begin(
        JWPLC_FRAM);

    if (!runtime.begin(
            JWPLCLogicStorageProfiles::FRAM_8K.framBytes))
    {
        Serial.println("Runtime FAIL");
        return;
    }

    runtime.prepareStoredProgram();

    JWPLC_Display.setIdleWakeMode(
        IDLE_WAKE_ANY_BUTTON);

    JWPLC_LogicRuntime_UI.begin(
        runtime);
}

void loop()
{
    JWPLC_LogicRuntime_UI.update();

    if (runtime.state() ==
        JWPLCLogicRuntimeState::Running)
    {
        runtime.tick();
    }
}
```

Preparar un programa almacenado no significa iniciarlo automáticamente. La UI
v1 puede exponer acciones de programa según el estado disponible.

## Ejemplo 4 — Avanzado de usuario: mapa FBD v2 en RAM

El motor v2 es experimental y no conmuta automáticamente salidas físicas.

```cpp
#include <JWPLC_LogicRuntime.h>
#include <JWPLC_LogicRuntime_UI.h>

JWPLCLogicV2::Engine engine;

static const JWPLCLogicV2::InputLink LINKS[] =
{
    JWPLCLogicV2::InputLink::block(0)
};

static const JWPLCLogicV2::BlockRecord BLOCKS[] =
{
    JWPLCLogicV2::BlockRecord(
        JWPLCLogicV2::BlockType::DigitalInput,
        0,
        0,
        0),

    JWPLCLogicV2::BlockRecord(
        JWPLCLogicV2::BlockType::Not,
        0,
        1)
};

static const JWPLCLogicV2::Program PROGRAM =
{
    BLOCKS,
    2,
    LINKS,
    1
};

void setup()
{
    engine.loadProgram(
        PROGRAM,
        1,
        0);

    engine.start();

    JWPLC_Display.setIdleWakeMode(
        IDLE_WAKE_ANY_BUTTON);

    JWPLC_LogicRuntime_UI.begin(
        engine);
}

void loop()
{
    bool entradas[1] =
    {
        digitalRead(I0_0) != 0
    };

    engine.scan(
        entradas,
        1);

    JWPLC_LogicRuntime_UI.update();

    JWPLC_LogicRuntime_UI.processV2EditorPending();
}
```

`processV2EditorPending()` debe llamarse inmediatamente después de
`update()` cuando utilizas la UI v2 editable.

## API de usuario

### Enlazar runtime v1 — Avanzado

```cpp
bool ok =
    JWPLC_LogicRuntime_UI.begin(
        runtime);
```

Parámetro:

- una referencia a `JWPLC_LogicRuntime`.

La UI conserva un puntero al runtime; el objeto debe seguir existiendo.

### Enlazar motor v2 — Avanzado / experimental

```cpp
bool ok =
    JWPLC_LogicRuntime_UI.begin(
        engine);
```

El tipo histórico de `engine` es `LogicV2EnginePrototype`; el alias
recomendado para código nuevo es:

```cpp
JWPLCLogicV2::Engine
```

### Servicio — Avanzado

```cpp
JWPLC_LogicRuntime_UI.update();
```

Debe ejecutarse frecuentemente desde `loop()`.

Para el editor v2:

```cpp
JWPLC_LogicRuntime_UI.update();
JWPLC_LogicRuntime_UI.processV2EditorPending();
```

### Estado del enlace — Avanzado

```cpp
bool attached =
    JWPLC_LogicRuntime_UI.isAttached();
```

Obtener el runtime v1 enlazado:

```cpp
JWPLC_LogicRuntime *r =
    JWPLC_LogicRuntime_UI.runtime();
```

Obtener el motor v2 enlazado:

```cpp
LogicV2EnginePrototype *e =
    JWPLC_LogicRuntime_UI.v2Engine();
```

Puede devolverse `nullptr` cuando la UI está enlazada al otro tipo de motor.

### Cerrar la UI — Avanzado

```cpp
JWPLC_LogicRuntime_UI.end();
```

Desvincula la UI del motor actual.

### Forzar redibujado — Avanzado

```cpp
JWPLC_LogicRuntime_UI.forceRedraw();
```

Úsalo sólo cuando una vista realmente necesita redibujarse. No lo llames en
cada iteración de `loop()`.

## Errores comunes

### Pensar que esta UI ejecuta la lógica

No. Debes seguir llamando `runtime.tick()` o `engine.scan(...)`.

### Usarla para una HMI normal

Para mostrar temperatura, motores, estados o barras utiliza
`JWPLC_Display`.

### Confundir el motor v2 con salidas físicas

Las salidas v2 son valores lógicos en RAM en el estado actual.

### Usar `delay()` largo en el loop

Eso retrasa tanto la lógica como la UI. Prefiere temporización con `millis()`.

### Llamar `forceRedraw()` continuamente

Sólo aumenta trabajo gráfico. Úsalo ante una necesidad concreta.

## API avanzada

### Preview unificado v2

Existe:

```cpp
JWPLC_LogicRuntime_UI.beginUnifiedPreview(
    engine);
```

Es un modo temporal/experimental para el preview consolidado del mapa v2.

No debe enseñarse como flujo estable de aplicación.

### Integración con Display

La clase expone:

```text
onDisplayEnter()
onDisplayRefresh()
onDisplayExit()
displayRefreshNeeded()
```

Estos métodos son parte de la integración entre esta librería y
`JWPLC_Display`.

**No deben llamarse manualmente desde un sketch normal.**

## Compatibilidad

La firma histórica:

```cpp
begin(LogicV2EnginePrototype &engine)
```

se mantiene.

Como `JWPLCLogicV2::Engine` es un alias de ese tipo, para código nuevo puedes
declarar:

```cpp
JWPLCLogicV2::Engine engine;
```

y seguir usando:

```cpp
JWPLC_LogicRuntime_UI.begin(
    engine);
```

## Versión

Documentado para:

```text
JWPLC ESP32 v2.1.0-alpha.12
JWPLC_LogicRuntime_UI 0.5.8
```

Estado:

```text
runtime v1 UI = avanzada
motor v2 UI   = experimental
OpenPLC       = no integrado por esta librería
```

La arquitectura interna de pantallas, modelos de lectura y editores se mantiene
en la documentación de desarrollo y no es necesaria para usar la fachada
pública.
