# JWPLC_LogicRuntime_UI

Interfaz gráfica modular para mostrar y administrar `JWPLC_LogicRuntime` desde la pantalla `USER` del JWPLC Basic.

## Separación de responsabilidades

```text
JWPLC_LogicRuntime
└── motor, programas, almacenamiento y retentivos

JWPLC_Display
└── TFT, IDLE, botonera, SPI y transición IDLE/USER

JWPLC_LogicRuntime_UI
└── vistas USER específicas del motor lógico
```

La capa gráfica solo se compila cuando el sketch incluye:

```cpp
#include <JWPLC_LogicRuntime_UI.h>
```

## Uso

```cpp
#include <JWPLC_GlobalPeripherals.h>
#include <JWPLC_LogicRuntime.h>
#include <JWPLC_LogicRuntime_UI.h>

JWPLC_LogicRuntime runtime;

void setup()
{
    runtime.storage().begin(JWPLC_FRAM);
    runtime.begin(JWPLCLogicStorageProfiles::FRAM_8K.framBytes);
    JWPLC_LogicRuntime_UI.begin(runtime);
}

void loop()
{
    JWPLC_LogicRuntime_UI.update();

    if (runtime.state() == JWPLCLogicRuntimeState::Running)
    {
        runtime.tick();
    }
}
```

`update()` procesa acciones diferidas y sincroniza `RUN/ERR` de IDLE. No reemplaza el scan explícito.

## Identidad visual

Paleta aprobada:

```text
verde
blanco
negro
```

- verde: estado normal, títulos y selección;
- amarillo: `STOPPED` o advertencia;
- rojo: falla o peligro;
- negro y panel oscuro: fondos;
- blanco: texto principal.

## IDLE

La pantalla automática existente mantiene:

- `PWR`, `RUN`, `ERR`, `BUS`, `ETH`;
- entradas `I0.0..I0.7`;
- salidas `Q0.0..Q0.7`;
- RTC.

Con esta librería:

```text
RUN = runtime.state() == Running
ERR = Fault o error crítico persistente
```

FRAM sin formato, programa ausente o snapshot inexistente no encienden `ERR` por sí solos.

## HOME v0.1

Muestra:

- estado del runtime;
- nombre del programa cargado, incluso si vive únicamente en RAM;
- Program ID y generación cuando existe identidad persistente;
- cantidad de bloques;
- scan promedio y máximo;
- estado del program store;
- estado retentivo;
- accesos a `PROGRAMA`, `BLOQUES`, `MEMORIA` y `DIAGNOSTICO`.

## PROGRAMA v0.2

Acciones:

```text
PREPARAR
RUN
STOP
VOLVER
```

### PREPARAR

Ejecuta fuera del callback TFT:

```text
prepareStoredProgram()
→ restoreStoredRetentiveState() cuando corresponde
```

No inicia automáticamente y no formatea la FRAM.

### RUN

```cpp
runtime.start();
```

### STOP

```cpp
runtime.stop();
```

Apaga salidas y limpia estados temporales. El guardado retentivo sigue siendo explícito.

### VOLVER

Regresa a HOME sin pasar por IDLE. `ESC` vuelve directamente a IDLE.

## BLOQUES v0.3

Vista de solo lectura del programa que ya está cargado en el motor.

Incluye:

```text
lista desplazable de bloques
valor booleano en vivo
detalle de tipo, fuentes, recurso, parámetro y flags
retorno a HOME
```

Controles de lista:

```text
UP / DOWN     seleccionar y desplazar
LEFT / RIGHT  alternar DETALLE / VOLVER
OK            ejecutar acción
ESC           IDLE
```

Controles de detalle:

```text
UP / LEFT      bloque anterior
DOWN / RIGHT   bloque siguiente
OK             regresar a lista
ESC            IDLE
```

Los valores se revisan visualmente cada 100 ms, pero solo se redibuja la fila o campo cuyo booleano cambió.

## API de lectura del runtime

La UI usa:

```cpp
const LogicProgram *program = runtime.program();
uint16_t count = runtime.blockCount();
const LogicBlockDefinition *block = runtime.blockDefinition(index);
bool value = runtime.blockValue(index);
```

Estas vistas son `const`. Los punteros pertenecen al runtime y dejan de ser válidos conceptualmente al cargar o descargar otro programa.

## Acciones diferidas y bus SPI

Una vista nunca accede directamente a FRAM, SD o Ethernet mientras el callback gráfico mantiene el bus TFT.

```text
callback USER
→ registra solicitud pequeña
→ libera SPI
→ JWPLC_LogicRuntime_UI.update() ejecuta desde loop()
→ siguiente refresh muestra el resultado
```

Norma:

```text
docs/USER_UI_ACTION_RULES.md
```

## Renderizado

Patrón aprobado:

```text
enter()
├── estructura estática una sola vez
├── caché dinámica invalidada
└── valores iniciales

refresh()
├── botonera
├── comparación contra caché
└── únicamente regiones modificadas
```

Desde v0.2.1 los campos variables usan:

```text
limpieza rectangular continua
+ texto transparente solo con caracteres útiles
```

Esto eliminó el parpadeo y aceleró fuertemente la construcción inicial.

Norma:

```text
docs/USER_UI_RENDERING_RULES.md
```

## Ejemplos

```text
JWPLC_LogicRuntime_UI_Home
JWPLC_LogicRuntime_UI_Blocks
```

El ejemplo de Bloques carga siete bloques en RAM, ejecuta el scan y no contiene `DigitalOutput`, por lo que Q0 permanece apagado. Tampoco escribe ni formatea la FRAM.

## Organización

```text
src/
├── JWPLC_LogicRuntime_UI.h
├── JWPLC_LogicRuntime_UI.cpp
├── RuntimeUIView.h
├── screens/
│   ├── RuntimeUIHome.*
│   ├── RuntimeUIProgram.*
│   └── RuntimeUIBlocks.*
└── widgets/
    └── RuntimeUIWidgets.*

docs/
├── RUNTIME_UI_HOME_V0_1_TEST.md
├── RUNTIME_UI_PROGRAM_V0_2_TEST.md
├── RUNTIME_UI_BLOCKS_V0_3_TEST.md
├── USER_UI_RENDERING_RULES.md
├── USER_UI_VISUAL_GUIDE.md
└── USER_UI_ACTION_RULES.md
```

## Vistas pendientes

```text
RuntimeUIStorage
RuntimeUIDiagnostics
RuntimeUIBlockEditor
RuntimeUIConfirmDialog
```

## Compatibilidad de callbacks

`JWPLC_Display` mantiene callbacks débiles para sketches normales. Cuando se incluye esta librería, sus callbacks fuertes enrutan USER hacia `JWPLC_LogicRuntime_UI`.

No deben definirse simultáneamente en el sketch:

```cpp
jwplcUserDisplayEnterCallback()
jwplcUserDisplayRefreshCallback()
jwplcUserDisplayExitCallback()
```

Los sketches que no incluyan esta librería conservan el comportamiento anterior.