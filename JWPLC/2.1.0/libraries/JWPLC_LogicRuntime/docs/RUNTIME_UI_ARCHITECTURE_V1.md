# Arquitectura de interfaz USER para JWPLC Logic Runtime v1

## Decisión

La interfaz gráfica del motor lógico no se incorpora directamente al núcleo `JWPLC_LogicRuntime`.

Se crea una librería compañera:

```text
JWPLC_LogicRuntime_UI
```

Separación aprobada:

```text
JWPLC_LogicRuntime
└── ejecución, validación, almacenamiento y retentivos

JWPLC_Display
└── TFT, IDLE, botonera, SPI y transición IDLE/USER

JWPLC_LogicRuntime_UI
└── vistas USER específicas del motor lógico
```

## Motivos

- evitar que el motor lógico dependa de Adafruit GFX o ST7789;
- no aumentar la compilación de sketches que solo usan el runtime;
- mantener la pantalla IDLE ya validada;
- dividir cada vista en archivos independientes;
- permitir commits pequeños sin reconstruir un callback monolítico;
- preservar los callbacks USER anteriores cuando la nueva librería no está incluida.

## IDLE

IDLE continúa perteneciendo a `JWPLC_Display` y conserva:

```text
PWR
RUN
ERR
BUS
ETH
I0.0..I0.7
Q0.0..Q0.7
RTC
```

La nueva librería enlaza:

```text
RUN = runtime.state() == Running
ERR = Fault o error crítico
```

No se considera error crítico:

- almacenamiento sin formato;
- ausencia de programa;
- ausencia de snapshot retentivo;
- snapshot de otra identidad.

## USER

Activación inicial:

```cpp
JWPLC_LogicRuntime_UI.begin(runtime);
```

Sincronización liviana desde `loop()`:

```cpp
JWPLC_LogicRuntime_UI.update();
```

La vista USER se conecta mediante definiciones fuertes de los callbacks existentes:

```text
jwplcUserDisplayEnterCallback
jwplcUserDisplayRefreshCallback
jwplcUserDisplayExitCallback
```

`JWPLC_Display` conserva sus implementaciones débiles para sketches que no incluyan esta librería.

## Organización inicial

```text
JWPLC_LogicRuntime_UI/
├── src/
│   ├── JWPLC_LogicRuntime_UI.h
│   ├── JWPLC_LogicRuntime_UI.cpp
│   ├── screens/
│   │   ├── RuntimeUIHome.h
│   │   └── RuntimeUIHome.cpp
│   └── widgets/
│       ├── RuntimeUIWidgets.h
│       └── RuntimeUIWidgets.cpp
├── docs/
│   ├── RUNTIME_UI_HOME_V0_1_TEST.md
│   └── USER_UI_RENDERING_RULES.md
└── examples/
    └── JWPLC_LogicRuntime_UI_Home/
```

## Primera vista

`RuntimeUIHome` muestra:

- estado del runtime;
- programa cargado;
- Program ID y generación;
- cantidad de bloques;
- scan promedio y máximo;
- estado de almacenamiento;
- estado retentivo;
- accesos a Programa, Bloques, Memoria y Diagnóstico.

La primera etapa deja las cuatro secciones como navegación visual. No ejecuta todavía guardar, rollback, restaurar o RUN.

## Vistas siguientes

Cada una se implementará en archivos propios:

```text
RuntimeUIProgram
RuntimeUIBlocks
RuntimeUIStorage
RuntimeUIDiagnostics
```

Posteriormente:

```text
RuntimeUIBlockEditor
RuntimeUIConfirmDialog
```

## Política de renderizado

La validación visual inicial de `RuntimeUIHome` confirmó el diseño, pero reveló parpadeo periódico causado por borrar y volver a escribir datos que no cambiaban.

Desde esta etapa, todas las vistas deben usar el patrón:

```text
enter()
├── estructura estática una vez
├── caché dinámica invalidada
└── primer estado dinámico

refresh()
├── entrada de botonera
├── comparación contra caché
└── actualización exclusiva de campos sucios
```

Reglas resumidas:

- `fillScreen()` solo al entrar o ante `forceRedraw()`;
- marcos, títulos y etiquetas no se dibujan desde el refresh normal;
- RUN, programa, storage y retentivos se actualizan solo al cambiar;
- scan promedio/máximo se evalúa cada 1000 ms;
- evaluar no implica escribir: si el dato no cambió no se toca la TFT;
- los textos variables usan campos de ancho fijo;
- la navegación redibuja solo la selección anterior y la nueva;
- no se usa framebuffer completo de 108800 bytes;
- sprites parciales quedan reservados para animaciones que realmente los necesiten.

La norma completa y la plantilla para nuevas pantallas están en:

```text
JWPLC_LogicRuntime_UI/docs/USER_UI_RENDERING_RULES.md
```

## Aplicación en RuntimeUIHome

La pantalla principal ya fue migrada a:

- `drawHeaderStatic()` para la parte fija;
- `updateHeaderState()` para la insignia dinámica;
- etiquetas dibujadas una sola vez;
- `DynamicCache` para runtime, programa, identidad, storage, retentivos y scan;
- `updateTextField()` para texto de ancho fijo sin borrado previo;
- actualización de scan cada 1000 ms;
- redibujado exclusivo de los dos botones implicados al mover el selector;
- una sola reconstrucción al entrar en USER.

## Estado

```text
ARQUITECTURA APROBADA
ESQUELETO USER IMPLEMENTADO
DISEÑO VISUAL INICIAL VALIDADO
DIRTY RENDERING IMPLEMENTADO
REVALIDACIÓN FÍSICA SIN PARPADEO PENDIENTE
```
