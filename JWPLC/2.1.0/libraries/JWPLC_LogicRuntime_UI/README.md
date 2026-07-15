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
    JWPLC_LogicRuntime_UI.update();
}
```

`begin(runtime)` enlaza la instancia lógica con `USER`. `update()` es liviano y sincroniza los indicadores `RUN` y `ERR` de la pantalla `IDLE`.

La pantalla `USER` continúa siendo refrescada por `JWPLC_Display`; el sketch no llama directamente a funciones de dibujo.

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

## USER v0.1

La primera pantalla muestra:

- estado `READY`, `RUNNING`, `STOPPED` o `FAULT`;
- nombre del programa persistente cargado;
- Program ID y generación;
- cantidad de bloques;
- scan promedio y máximo;
- estado del program store;
- estado retentivo;
- accesos preparados para `PROGRAMA`, `BLOQUES`, `MEMORIA` y `DIAGNOSTICO`.

Controles:

```text
UP / LEFT       selección anterior
DOWN / RIGHT    selección siguiente
OK              confirma la sección
ESC             vuelve a IDLE
```

En esta primera fase, `OK` únicamente confirma visualmente la sección elegida. Las pantallas internas se agregarán en archivos separados.

## Renderizado incremental

Las pantallas USER no deben reconstruirse en cada callback.

Patrón aprobado:

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

`RuntimeUIHome` ya aplica:

- encabezado estático separado de la insignia de estado;
- campos de texto de ancho fijo;
- caché para runtime, programa, identidad, almacenamiento y retentivos;
- scan visual cada 1000 ms y solo si cambió;
- redibujado exclusivo de la selección anterior y actual;
- una única reconstrucción al entrar en `USER`.

Las reglas obligatorias y la plantilla para vistas posteriores están en:

```text
docs/USER_UI_RENDERING_RULES.md
```

## Organización

```text
src/
├── JWPLC_LogicRuntime_UI.h
├── JWPLC_LogicRuntime_UI.cpp
├── screens/
│   ├── RuntimeUIHome.h
│   └── RuntimeUIHome.cpp
└── widgets/
    ├── RuntimeUIWidgets.h
    └── RuntimeUIWidgets.cpp

docs/
├── RUNTIME_UI_HOME_V0_1_TEST.md
└── USER_UI_RENDERING_RULES.md
```

Las siguientes vistas se incorporarán de manera independiente:

```text
RuntimeUIProgram
RuntimeUIBlocks
RuntimeUIStorage
RuntimeUIDiagnostics
```

## Compatibilidad de callbacks

`JWPLC_Display` mantiene callbacks débiles para sketches normales. Cuando se incluye esta librería, sus callbacks fuertes enrutan la pantalla `USER` hacia `JWPLC_LogicRuntime_UI`.

Un sketch que utilice `JWPLC_LogicRuntime_UI` no debe definir simultáneamente sus propios:

```cpp
jwplcUserDisplayEnterCallback()
jwplcUserDisplayRefreshCallback()
jwplcUserDisplayExitCallback()
```

Los sketches que no incluyan esta librería conservan el comportamiento anterior.
