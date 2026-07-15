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

## Reglas de redibujado

- estructura fija al entrar en la vista;
- actualización periódica solo de datos dinámicos;
- redibujado inmediato únicamente ante cambio de selección;
- nada de `fillScreen()` en cada refresco;
- la prueba de convivencia medirá el impacto sobre el scan antes de habilitar acciones reales.

## Estado

```text
ARQUITECTURA APROBADA
ESQUELETO USER IMPLEMENTADO
VALIDACIÓN EN HARDWARE PENDIENTE
```
