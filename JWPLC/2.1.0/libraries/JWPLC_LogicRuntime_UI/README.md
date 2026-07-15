# JWPLC_LogicRuntime_UI

Interfaz gráfica modular para mostrar y administrar `JWPLC_LogicRuntime` desde la pantalla `USER` del JWPLC Basic.

## Separación de responsabilidades

```text
JWPLC_LogicRuntime
└── motor, programas, almacenamiento y retentivos

JWPLC_Display
└── TFT, pantalla IDLE, botonera, SPI y transición IDLE/USER

JWPLC_LogicRuntime_UI
└── vistas USER específicas del motor lógico
```

La librería gráfica no forma parte del núcleo del runtime. Solo se compila cuando el sketch incluye:

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
    // Procesa acciones solicitadas desde USER fuera del callback de la TFT.
    JWPLC_LogicRuntime_UI.update();

    // El scan lógico continúa siendo explícito.
    if (runtime.state() == JWPLCLogicRuntimeState::Running)
    {
        runtime.tick();
    }
}
```

`begin(runtime)` enlaza la instancia lógica con `USER`.

`update()`:

- sincroniza `RUN` y `ERR` de la pantalla `IDLE`;
- procesa acciones diferidas de las vistas;
- nunca sustituye la llamada explícita a `runtime.tick()`.

La pantalla `USER` continúa siendo refrescada por `JWPLC_Display`; el sketch no llama directamente a funciones de dibujo.

## Paleta visual

La interfaz usa la identidad de JW Control:

```text
verde
blanco
negro
```

El verde se reserva para:

- estado normal o preparado;
- acentos;
- títulos de panel;
- bordes de selección;
- fondo oscuro del elemento seleccionado.

Amarillo continúa indicando `STOPPED` y rojo se reserva para fallas.

## IDLE

La pantalla automática existente permanece como tablero operativo:

- `PWR`;
- `RUN`;
- `ERR`;
- `BUS`;
- `ETH`;
- entradas `I0.0..I0.7`;
- salidas `Q0.0..Q0.7`;
- RTC.

Al usar esta librería:

```text
RUN = runtime.state() == Running
ERR = Fault o error crítico persistente
```

Estados normales como FRAM `UNFORMATTED`, ausencia de programa o snapshot inexistente no encienden `ERR`.

## HOME v0.1

La pantalla principal muestra:

- estado `READY`, `RUNNING`, `STOPPED` o `FAULT`;
- nombre del programa persistente cargado;
- Program ID y generación;
- cantidad de bloques;
- scan promedio y máximo;
- estado del program store;
- estado retentivo;
- accesos a `PROGRAMA`, `BLOQUES`, `MEMORIA` y `DIAGNOSTICO`.

`PROGRAMA` ya abre una vista funcional. Las otras tres secciones permanecen preparadas para etapas posteriores.

## PROGRAMA v0.2

La primera vista funcional ofrece:

```text
PREPARAR
RUN
STOP
VOLVER
```

### PREPARAR

Ejecuta fuera del callback gráfico:

```text
prepareStoredProgram()
→ restoreStoredRetentiveState() cuando existe un programa arrancable
```

No inicia automáticamente el runtime y no formatea la FRAM.

### RUN

Llama a:

```cpp
runtime.start();
```

El scan continúa siendo responsabilidad explícita del sketch mediante `runtime.tick()`.

### STOP

Llama a:

```cpp
runtime.stop();
```

Eso mantiene el comportamiento seguro del motor:

- apaga todas las salidas;
- limpia estados temporales;
- conserva la política de retentivos ya aprobada.

### VOLVER

Regresa a `HOME` sin pasar por `IDLE`.

`ESC` conserva su función global y vuelve directamente a `IDLE` desde cualquier vista USER.

## Acciones diferidas y bus SPI

Las vistas se ejecutan mientras `JWPLC_Display` mantiene adquirido el bus SPI para la TFT. Por eso una pantalla nunca debe acceder directamente a FRAM, SD o Ethernet desde su callback.

Patrón aprobado:

```text
callback USER
→ registra solicitud de un byte
→ termina dibujo y libera SPI
→ JWPLC_LogicRuntime_UI.update() procesa la acción desde loop()
→ siguiente refresh actualiza solo los campos modificados
```

## Renderizado incremental

Las pantallas USER no se reconstruyen en cada callback.

```text
enter()
├── dibuja fondo, marcos, etiquetas y botones una sola vez
├── invalida la caché de valores
└── dibuja los valores dinámicos iniciales

refresh()
├── procesa botonera
├── compara valores actuales con la caché
└── actualiza únicamente campos modificados
```

`RuntimeUIHome` y `RuntimeUIProgram` usan:

- encabezado estático separado de la insignia de estado;
- campos de texto de ancho fijo;
- caché de valores dinámicos;
- redibujado exclusivo de selección anterior y actual;
- una única reconstrucción al entrar en cada vista.

Reglas obligatorias:

```text
docs/USER_UI_RENDERING_RULES.md
```

## Organización

```text
src/
├── JWPLC_LogicRuntime_UI.h
├── JWPLC_LogicRuntime_UI.cpp
├── RuntimeUIView.h
├── screens/
│   ├── RuntimeUIHome.h
│   ├── RuntimeUIHome.cpp
│   ├── RuntimeUIProgram.h
│   └── RuntimeUIProgram.cpp
└── widgets/
    ├── RuntimeUIWidgets.h
    └── RuntimeUIWidgets.cpp

docs/
├── RUNTIME_UI_HOME_V0_1_TEST.md
├── RUNTIME_UI_PROGRAM_V0_2_TEST.md
└── USER_UI_RENDERING_RULES.md
```

Vistas posteriores:

```text
RuntimeUIBlocks
RuntimeUIStorage
RuntimeUIDiagnostics
RuntimeUIBlockEditor
RuntimeUIConfirmDialog
```

## Compatibilidad de callbacks

`JWPLC_Display` mantiene callbacks débiles para sketches normales. Cuando se incluye esta librería, sus callbacks fuertes enrutan la pantalla `USER` hacia `JWPLC_LogicRuntime_UI`.

Un sketch que utilice esta librería no debe definir simultáneamente:

```cpp
jwplcUserDisplayEnterCallback()
jwplcUserDisplayRefreshCallback()
jwplcUserDisplayExitCallback()
```

Los sketches que no incluyan `JWPLC_LogicRuntime_UI` conservan el comportamiento anterior.