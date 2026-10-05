# JWPLC_LogicRuntime

`JWPLC_LogicRuntime` es un motor lógico por bloques incluido en el ecosistema
JWPLC para ejecutar programas lógicos desde un sketch.

**No necesitas esta librería para programar normalmente el JWPLC Basic.**
Si estás aprendiendo Arduino, empieza con entradas/salidas, Display, RS-485 o
Modbus. LogicRuntime es una función avanzada y parte de su línea v2 sigue siendo
experimental.

## ¿Para qué sirve?

Permite representar una lógica como una lista de bloques y ejecutarla
repetidamente.

Por ejemplo:

```text
I0_0
  |
  v
 NOT
  |
  v
Q0_0
```

La librería contiene dos líneas distintas:

- **runtime v1**: integra lógica con E/S y dispone de almacenamiento/persistencia;
- **motor v2**: modelo experimental en RAM con entradas variables y más tipos
  de bloques.

No es OpenPLC y no implica que OpenPLC forme parte del autoload del JWPLC.

## Qué hace automáticamente el JWPLC

El runtime v1 utiliza las abstracciones de E/S del JWPLC cuando ejecuta bloques
de entrada y salida.

No necesitas:

- manipular directamente el expansor de E/S;
- crear un driver para FRAM cuando usas la integración prevista del runtime;
- implementar la evaluación booleana de cada bloque.

Sin embargo, **tu sketch sigue controlando cuándo iniciar y ejecutar el motor**.

## Inicio rápido

Este ejemplo ejecuta:

```text
I0_0 -> NOT -> Q0_0
```

```cpp
#include <JWPLC_LogicRuntime.h>

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
    },
    {
        LogicBlockType::DigitalOutput,
        1,
        JWPLC_LOGIC_NO_SOURCE,
        0,
        0
    }
};

static const LogicProgram PROGRAM =
{
    "NOT I0_0",
    BLOCKS,
    3
};

void setup()
{
    Serial.begin(115200);

    if (!runtime.begin())
    {
        Serial.println("Runtime no disponible");
        return;
    }

    if (!runtime.loadProgram(PROGRAM))
    {
        Serial.println("Programa invalido");
        return;
    }

    if (!runtime.start())
    {
        Serial.println("No se pudo iniciar");
    }
}

void loop()
{
    if (!runtime.tick())
    {
        Serial.println(
            JWPLC_LogicRuntime::errorName(
                runtime.lastError()));
    }
}
```

## Conceptos básicos

### Programa

Un `LogicProgram` contiene una lista ordenada de bloques.

### Bloque

Un `LogicBlockDefinition` describe qué hace una posición del programa.

El runtime v1 incluye bloques como entrada digital, salida digital, compuertas
lógicas y temporizadores.

### Fuente

Los bloques pueden recibir el resultado de bloques anteriores.

`JWPLC_LOGIC_NO_SOURCE` significa que una entrada del bloque no utiliza una
fuente.

### Scan

Cada llamada:

```cpp
runtime.tick();
```

ejecuta un ciclo lógico del programa cargado.

No añadas `delay()` sólo para “dar tiempo” al runtime. Ejecuta `tick()`
frecuentemente y programa otras tareas con `millis()` cuando sea necesario.

### Estados

El runtime v1 puede estar en:

```text
Stopped
Ready
Running
Fault
```

## Ejemplo 1 — Básico: leer el resultado de bloques

Partimos del programa del Inicio rápido y mostramos sus valores una vez por
segundo.

```cpp
#include <JWPLC_LogicRuntime.h>

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
    "Monitor",
    BLOCKS,
    2
};

void setup()
{
    Serial.begin(115200);

    runtime.begin();
    runtime.loadProgram(PROGRAM);
    runtime.start();
}

void loop()
{
    runtime.tick();

    static uint32_t ultimoReporte = 0;

    if (millis() - ultimoReporte >= 1000)
    {
        ultimoReporte = millis();

        Serial.print("I0_0=");
        Serial.print(
            runtime.blockValue(0));

        Serial.print(" NOT=");
        Serial.println(
            runtime.blockValue(1));
    }
}
```

## Ejemplo 2 — Intermedio: TON y salida

Este ejemplo usa una compuerta AND y un temporizador TON de 2 segundos.

```cpp
#include <JWPLC_LogicRuntime.h>

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
        LogicBlockType::DigitalInput,
        JWPLC_LOGIC_NO_SOURCE,
        JWPLC_LOGIC_NO_SOURCE,
        1,
        0
    },
    {
        LogicBlockType::And,
        0,
        1,
        0,
        0
    },
    {
        LogicBlockType::Ton,
        2,
        JWPLC_LOGIC_NO_SOURCE,
        0,
        2000
    },
    {
        LogicBlockType::DigitalOutput,
        3,
        JWPLC_LOGIC_NO_SOURCE,
        0,
        0
    }
};

static const LogicProgram PROGRAM =
{
    "AND + TON",
    BLOCKS,
    5
};

void setup()
{
    Serial.begin(115200);

    if (runtime.begin() &&
        runtime.loadProgram(PROGRAM))
    {
        runtime.start();
    }
}

void loop()
{
    if (!runtime.tick())
    {
        Serial.println(
            JWPLC_LogicRuntime::errorName(
                runtime.lastError()));
    }
}
```

Cuando las dos entradas permanecen verdaderas durante 2 segundos, el resultado
del TON activa la salida lógica conectada a `Q0_0`.

## Ejemplo 3 — Aplicación real: RUN/STOP y estadísticas

Este ejemplo permite detener y reiniciar el motor con la botonera.

```cpp
#include <JWPLC_LogicRuntime.h>

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
    Serial.begin(115200);

    runtime.begin();
    runtime.loadProgram(PROGRAM);
    runtime.start();

    JWPLC_Buttons.clearPendingInput();
}

void loop()
{
    if (JWPLC_Buttons.pressed(BTN_OK) &&
        runtime.state() !=
            JWPLCLogicRuntimeState::Running)
    {
        runtime.start();
    }

    if (JWPLC_Buttons.pressed(BTN_ESC))
    {
        runtime.stop();
    }

    if (runtime.state() ==
        JWPLCLogicRuntimeState::Running)
    {
        runtime.tick();
    }

    static uint32_t ultimoReporte = 0;

    if (millis() - ultimoReporte >= 1000)
    {
        ultimoReporte = millis();

        Serial.print("Estado=");
        Serial.print(
            JWPLC_LogicRuntime::stateName(
                runtime.state()));

        Serial.print(" scans=");
        Serial.print(
            runtime.scanCount());

        Serial.print(" avg_us=");
        Serial.println(
            runtime.averageScanMicros());
    }
}
```

## Ejemplo 4 — Avanzado de usuario: motor v2 en RAM

El motor v2 es experimental. No escribe salidas físicas automáticamente y no
usa FRAM por sí solo.

Para código nuevo del motor v2 usa los aliases de:

```cpp
#include <JWPLC_LogicRuntime_V2.h>
```

Ejemplo lógico:

```text
entrada 0 -> NOT
```

```cpp
#include <JWPLC_LogicRuntime_V2.h>

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
    Serial.begin(115200);

    if (!engine.loadProgram(
            PROGRAM,
            1,
            0))
    {
        Serial.println("Programa v2 invalido");
        return;
    }

    engine.start();
}

void loop()
{
    bool entradas[1] =
    {
        digitalRead(I0_0) != 0
    };

    if (engine.scan(
            entradas,
            1))
    {
        Serial.println(
            engine.blockValue(1));
    }
}
```

Este ejemplo es educativo. El motor v2 todavía no debe presentarse como
reemplazo estable del runtime v1.

## API de usuario

### Ciclo de vida v1 — Avanzado de usuario

| Función | Qué hace |
|---|---|
| `begin(framBytes)` | Inicializa el runtime |
| `loadProgram(program)` | Valida y carga un programa en RAM |
| `start()` | Pasa el motor a ejecución |
| `tick()` | Ejecuta un scan |
| `stop()` | Detiene el motor y aplica su política de parada |
| `hasProgram()` | Indica si existe un programa cargado |

Camino mínimo:

```cpp
runtime.begin();
runtime.loadProgram(PROGRAM);
runtime.start();

void loop()
{
    runtime.tick();
}
```

### Estado y errores v1

```cpp
runtime.state();
runtime.lastError();
runtime.validationError();

JWPLC_LogicRuntime::stateName(
    runtime.state());

JWPLC_LogicRuntime::errorName(
    runtime.lastError());
```

Nivel: **Avanzado de usuario**.

### Inspeccionar el programa v1

```cpp
runtime.blockCount();
runtime.blockValue(index);
runtime.blockDefinition(index);
runtime.program();
```

`program()` y `blockDefinition()` devuelven vistas de sólo lectura. No
modifiques el programa a través de esos punteros.

### Estadísticas de scan v1

```cpp
runtime.scanCount();
runtime.lastScanMicros();
runtime.minScanMicros();
runtime.maxScanMicros();
runtime.averageScanMicros();
runtime.outputWriteCount();

runtime.resetScanStatistics();
```

Son útiles para diagnóstico y commissioning.

### Programa almacenado v1 — Avanzado

Preparar un programa almacenado:

```cpp
JWPLCLogicStorageBootState boot =
    runtime.prepareStoredProgram();
```

Ruta booleana de compatibilidad:

```cpp
bool ok =
    runtime.loadStoredProgram();
```

Estas funciones preparan/cargan, pero no arrancan automáticamente el motor.
`start()` sigue siendo explícito.

### Retentividad v1 — Avanzado

La API pública incluye:

```text
retentiveStateBytes()
retentiveBlockCount()
exportRetentiveState()
importRetentiveState()
clearRetentiveStates()
restoreStoredRetentiveState()
saveStoredRetentiveState()
retentiveState()
retentiveStoreError()
retentiveStoreStatus()
retentiveStateName()
```

Ejemplo de restauración del snapshot almacenado:

```cpp
runtime.prepareStoredProgram();

JWPLCLogicRetentiveState state =
    runtime.restoreStoredRetentiveState();

runtime.start();
```

La persistencia retentiva es una función avanzada. Prueba cuidadosamente la
política de arranque/parada de tu máquina.

### Storage v1 — Avanzado

```cpp
JWPLCLogicStorage &storage =
    runtime.storage();

const LogicStorageProfile &profile =
    runtime.storageProfile();

const LogicStorageLayout &layout =
    runtime.storageLayout();
```

Estas APIs son para proyectos que gestionan explícitamente programas
persistentes. No son necesarias para ejecutar un `LogicProgram` definido en
el sketch.

### Motor v2 — Avanzado / experimental

Ciclo de vida:

```cpp
engine.loadProgram(...);
engine.start();
engine.scan(...);
engine.stop();
engine.unloadProgram();
```

Estado:

```cpp
engine.hasProgram();
engine.state();
engine.lastError();
engine.validationError();
engine.scanCount();
```

Inspección:

```cpp
engine.blockCount();
engine.linkCount();
engine.digitalInputCount();
engine.digitalOutputCount();

engine.blockValue(index);
engine.inputValue(blockIndex, inputIndex);
engine.digitalOutputValue(outputIndex);

engine.blockDefinition(index);
engine.inputLink(index);
engine.program();
```

TON:

```cpp
engine.tonTiming(index);
engine.tonElapsedMs(index, millis());
engine.tonRemainingMs(index, millis());
```

Nombres de diagnóstico:

```cpp
JWPLCLogicV2::Engine::stateName(
    engine.state());

JWPLCLogicV2::Engine::errorName(
    engine.lastError());
```

Tipos de bloque v2 actuales:

```text
DigitalInput
ConstantFalse
ConstantTrue
Not
And
Or
Nand
Nor
Xor
DigitalOutput
SetReset
Ton
```

## Errores comunes

### Pensar que LogicRuntime es necesario para usar el JWPLC

No lo es. Un sketch Arduino normal puede usar directamente E/S, Display,
Ethernet y Modbus.

### Confundir LogicRuntime con OpenPLC

Son proyectos diferentes.

### Olvidar `tick()`

El runtime v1 no ejecuta scans si tu sketch no llama `tick()`.

### Poner `delay()` largo entre scans

Eso reduce directamente la frecuencia con la que se ejecuta la lógica.

### Modificar el programa a través de un puntero de sólo lectura

`program()` y `blockDefinition()` son para inspección.

### Asumir que DigitalOutput v2 acciona físicamente Q0

El motor v2 actual conserva salidas como valores lógicos. No debe asumirse
integración física completa.

## API avanzada

### Helpers del contrato v2

`JWPLCLogicV2::InputLink` ofrece:

```cpp
JWPLCLogicV2::InputLink::block(
    blockIndex);

JWPLCLogicV2::InputLink::block(
    blockIndex,
    true);

JWPLCLogicV2::InputLink::open();

JWPLCLogicV2::InputLink::constantTrue();

JWPLCLogicV2::InputLink::constantFalse();
```

Consultar:

```cpp
link.source();
link.inverted();
```

### Contrato v2

Información del contrato:

```cpp
JWPLCLogicV2::CONTRACT_MAJOR
JWPLCLogicV2::CONTRACT_MINOR
JWPLCLogicV2::CONTRACT_VERSION
JWPLCLogicV2::RECORD_SCHEMA_VERSION
```

Sirve para herramientas y compatibilidad de formatos, no para un primer
proyecto.

## Compatibilidad

Los nombres históricos:

```text
LogicV2EnginePrototype
LogicV2Program
LogicV2BlockRecord
LogicV2InputLink
LogicV2BlockType
```

siguen existiendo.

Para código nuevo v2 se recomiendan:

```text
JWPLCLogicV2::Engine
JWPLCLogicV2::Program
JWPLCLogicV2::BlockRecord
JWPLCLogicV2::InputLink
JWPLCLogicV2::BlockType
```

## Versión

Documentado para:

```text
JWPLC ESP32 v2.1.0-alpha.12
JWPLC_LogicRuntime 0.2.0
```

Estado de producto:

```text
runtime v1 = avanzado
motor v2   = experimental / RAM-only
OpenPLC    = no integrado por esta librería
```

La documentación técnica de codec, layouts, FRAM y pruebas de desarrollo se
mantiene fuera de esta guía de usuario.
