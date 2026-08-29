# JWPLC_LogicRuntime_UI

Interfaz gráfica experimental para conectar `JWPLC_LogicRuntime` con la pantalla `USER` y la botonera del **JWPLC Basic**.

La librería mantiene compatibilidad con el runtime v1 y contiene además la evolución del editor FBD sobre el motor v2 RAM-only. La UI no sustituye al motor lógico ni debe mezclar rendering TFT con persistencia o comunicación.

## Estado de versión

`library.properties` declara actualmente:

```text
JWPLC_LogicRuntime_UI 0.5.8
```

El branch 2.1.0 contiene trabajo experimental posterior de consolidación del renderer FBD y del contrato v2 que todavía no se presenta como una nueva versión publicada de la librería. Este README describe el **estado real del código del branch**, distinguiendo lo estable de lo experimental.

## Separación de responsabilidades

```text
JWPLC_LogicRuntime
└── motor, programas, validación y almacenamiento

JWPLC_Display
└── TFT, IDLE/USER, SPI y callbacks gráficos

JWPLC_LogicRuntime_UI
└── navegación USER, vistas del runtime y editor FBD
```

No se debe asumir OpenPLC integrado.

## API pública

```cpp
#include <JWPLC_LogicRuntime_UI.h>
```

### Runtime v1

```cpp
JWPLC_LogicRuntime runtime;

void setup()
{
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

`update()` procesa trabajo no gráfico y acciones diferidas. No reemplaza el scan del runtime.

### Motor v2

```cpp
JWPLCLogicV2::Engine engine;

void setup()
{
    JWPLC_LogicRuntime_UI.begin(engine);
}

void loop()
{
    JWPLC_LogicRuntime_UI.update();
    JWPLC_LogicRuntime_UI.processV2EditorPending();

    // El scan v2 continúa bajo responsabilidad del sketch.
}
```

La edición v2 actúa sobre RAM. No escribe FRAM ni conmuta salidas físicas por el solo hecho de editar.

También existe una entrada experimental de consolidación:

```cpp
JWPLC_LogicRuntime_UI.beginUnifiedPreview(engine);
```

Se utiliza para validar el renderer FBD unificado sin declarar finalizada la migración completa del editor.

## Vistas del runtime v1

La UI conserva las vistas históricas para:

- HOME;
- PROGRAMA;
- DIAGRAMA;
- BLOQUES.

Estas vistas permiten inspeccionar el runtime, preparar/correr/detener la lógica y consultar bloques sin duplicar el motor dentro de la capa gráfica.

Las acciones que requieren almacenamiento se difieren fuera del callback TFT.

## Editor FBD v2

La línea v2 añade un mapa FBD navegable y edición transaccional en RAM.

Capacidades desarrolladas en el branch:

- mapa y detalle de bloques;
- selección visual de bloque;
- actividad lógica en vivo;
- navegación por botonera;
- edición de fuentes/entradas;
- edición TON;
- asistente de nuevo bloque;
- nodo virtual `+` para creación append-only;
- configuración jerárquica de fuente/parámetros;
- mini mapa contextual;
- sesión de edición transaccional;
- refresco regional y cachés para reducir parpadeo;
- política de refresco TFT adaptable;
- gate previo a adquirir SPI cuando una vista estática no requiere redibujado;
- renderer/fachada FBD activa que evita que el sketch dependa directamente de revisiones internas históricas.

El motor v2 puede ejecutar más tipos que los habilitados por el asistente gráfico. No debe confundirse capacidad del engine con capacidad disponible en la UI.

## Contrato v2

La UI depende del contrato explícito:

```cpp
#include <JWPLC_LogicRuntime_V2.h>
```

El motor es la fuente de verdad para:

- resolución de entradas;
- `HI`, `LO` y `OPEN`;
- negación de enlaces;
- orden topológico;
- validación de programa;
- evaluación de bloques.

La UI no debe reimplementar esas reglas.

Documento principal:

```text
../JWPLC_LogicRuntime/docs/LOGIC_RUNTIME_V2_CONTRACT.md
```

## Edición transaccional

El flujo aprobado es:

```text
UI crea/edita borrador
→ RuntimeUIV2EditSession valida
→ callback TFT sólo registra la solicitud
→ se libera SPI
→ processV2EditorPending() aplica desde loop
→ el motor recibe una copia válida
→ siguiente refresh muestra el resultado
```

Esto evita FRAM, SD, Ethernet o trabajo pesado dentro de un callback gráfico.

La sesión estructural soporta las operaciones implementadas por el contrato actual, incluyendo append y eliminación controlada/rollback en RAM. Que una operación exista en el modelo no significa que todas sus pantallas estén promovidas al flujo principal de usuario.

## TON y edición temporal

La UI conserva la duración efectiva del TON en milisegundos y permite presentarla con bases tipo LOGO!:

```text
segundos : centésimas
minutos : segundos
horas : minutos
```

Cambiar la base de presentación no debe alterar silenciosamente el tiempo efectivo del bloque.

La edición TON utiliza actualizaciones parciales para evitar barridos completos y parpadeo durante repeat de botonera.

## Renderizado y SPI

Regla central:

> Un refresh de lógica no obliga a transmitir toda la pantalla TFT.

La UI usa:

- cachés de regiones;
- invalidación explícita;
- redraw completo sólo al cambiar layout/página o aplicar una edición estructural;
- periodos distintos según contexto;
- consulta `displayRefreshNeeded()` antes del lock SPI cuando el flujo lo permite.

La entrada de botonera y el rendering aún comparten rutas en parte del editor FBD, por lo que el mapa v2 conserva callbacks suficientes para no perder eventos mientras continúa la consolidación.

## Navegación

Botonera base:

```text
UP
DOWN
LEFT
RIGHT
ESC
OK
```

La regla de diseño para pantallas anidadas es que `ESC` vuelva al padre antes de permitir que el router global abandone USER hacia IDLE.

Las pantallas activas deben consumir sus eventos una sola vez. `JWPLC_Buttons.pressed()` es consumible y no debe consultarse en dos capas para el mismo evento.

## Integración con IDLE

La UI puede sincronizar `RUN` y el estado de error del runtime con los indicadores de `JWPLC_Display` cuando opera sobre runtime v1.

Con Alpha6 debe respetarse la separación general del display:

- `ERR`: error de aplicación/runtime lógico cuando corresponde;
- `BUS`: RS-485/Modbus;
- `ETH`: Ethernet.

La UI no debe reutilizar `ERR` para fallas de red o bus.

## Callbacks Display

`JWPLC_Display` proporciona callbacks débiles para sketches normales. Cuando esta librería está enlazada, la UI enruta los callbacks USER hacia su objeto global.

No conviene definir simultáneamente en el sketch implementaciones incompatibles de:

```cpp
jwplcUserDisplayEnterCallback()
jwplcUserDisplayRefreshNeededCallback()
jwplcUserDisplayRefreshCallback()
jwplcUserDisplayExitCallback()
```

## Documentación interna relevante

La carpeta `docs/` conserva planes, resultados físicos y reglas de UI. Entre los documentos de referencia están:

```text
JWPLC_LOGIC_RUNTIME_UI_CHAT_TRANSFER_V0_5_8.md
RUNTIME_UI_FBD_CONFIG_GROUPS_V0_5_8_TEST.md
RUNTIME_UI_FBD_CONFIG_GROUPS_V0_5_8_PHYSICAL_RESULT.md
RUNTIME_UI_FBD_UNIFIED_MIGRATION_PLAN.md
USER_UI_ACTION_RULES.md
USER_UI_RENDERING_RULES.md
USER_UI_STYLE_GUIDE.md
USER_UI_NAVIGATION_STACK_RULES.md
```

Los documentos históricos de V4..V14 describen iteraciones internas del renderer. Para código nuevo debe usarse la fachada activa/publicada por `JWPLC_LogicRuntime_UI`, no instanciar revisiones internas por número.

## Límites actuales

No documentar como resuelto o estable sin un gate específico:

- persistencia automática del programa v2;
- codec FRAM v2;
- retentividad v2;
- salidas físicas v2;
- todos los tipos del motor disponibles en el asistente;
- migración completa de todos los flujos históricos al renderer unificado.

El editor FBD sigue siendo una línea experimental dentro del package.

## Estado

```text
JWPLC ESP32 2.1.0-alpha.6
JWPLC_LogicRuntime_UI: metadata 0.5.8
runtime v1: compatible
editor v2: RAM-only / experimental
renderer FBD unificado: migración en curso
```

El README debe avanzar junto con el contrato del motor y con los gates físicos; no usar números de revisión interna del renderer como API pública.
