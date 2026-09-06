# Alpha11 · A11-6 Integración de escritorio y sketch

Fecha: 2026-09-06

## Objetivo

Convertir el PoC web del JWPLC HMI Designer en una herramienta de uso diario sin exigir al usuario arrancar manualmente `localhost`, manteniendo intactos Web Serial/LIVE y el codegen ya validado.

La integración no depende de APIs privadas de Arduino IDE ni requiere mantener un fork del IDE.

## Arquitectura

```text
JWPLC HMI Designer
  -> launcher Windows
  -> servidor localhost privado
  -> Edge/Chrome --app
  -> desktop.html
  -> Designer existente + designer-project.js
  -> desktop-responsive.js / .css

Sketch Arduino
  -> Proyecto.ino
  -> JWPLC_HMI_Generated.h
  -> Proyecto.jwhmi (recomendado junto al sketch)
```

El servidor localhost existe sólo para conservar el contexto seguro requerido por Web Serial y File System Access. El usuario no debe iniciarlo manualmente.

## Funciones A11-6

```text
DESKTOP_LAUNCHER=YES
LOCALHOST_MANUAL_START=NO
EDGE_CHROME_APP_MODE=YES
PWA_MANIFEST=YES
SERVICE_WORKER=YES
NETWORK_FIRST_CACHE=YES

PROJECT_OPEN_JWHMI=YES
PROJECT_SAVE_JWHMI=YES
PROJECT_FORMAT_VERSION=1
PROJECT_DECLARATIVE_LAYER=YES
RAW_PIXEL_PROJECT_PERSISTENCE=NO_ALPHA11

SKETCH_FOLDER_LINK=YES
SKETCH_MUST_CONTAIN_INO=YES
LINK_HANDLE_PERSISTENCE=INDEXED_DB
GENERATED_HEADER_WRITE=YES
GENERATED_HEADER_NAME=JWPLC_HMI_Generated.h
GENERATED_HEADER_OVERWRITE_CONFIRM=YES
DUPLICATE_IDENTIFIER_BLOCKS_WRITE=YES

LIVE_WEB_SERIAL=PRESERVED
ARDUINO_IDE_FORK_REQUIRED=NO
```

## Responsive adaptativo

La clasificación horizontal ya no depende de un breakpoint fijo en píxeles. Se calcula principalmente mediante la proporción de la ventana respecto al ancho disponible de la pantalla actual:

```text
WINDOW_RATIO = window.outerWidth / screen.availWidth

WIDE     >= 70 %
MEDIUM   38..69 %
COMPACT  < 38 %
```

Existen únicamente límites absolutos de seguridad para viewports extremadamente pequeños; la decisión normal se basa en porcentaje para que una ventana al 50 % entre siempre en `MEDIUM` aunque la pantalla sea 1080p, 1440p o 4K.

Contrato:

```text
RESPONSIVE_STRATEGY=SCALE_PLUS_RELAYOUT
RESPONSIVE_PRIMARY_BASIS=SCREEN_PERCENTAGE
CANVAS_PRIORITY=HIGH
TOOLBAR_WRAP=NO
MIN_UI_TEXT=11PX
```

### WIDE

```text
LEFT_PANEL=OPEN
RIGHT_PANEL=PREVIEW_PLUS_INSPECTOR
BOTTOM_PANEL=USER_STATE
TOOLBAR=FULL
DEFAULT_ZOOM=3X_IF_FITS
```

### MEDIUM

```text
LEFT_PANEL=OPEN_COMPACT
COMPONENTS_TOOLS=COLLAPSIBLE
RIGHT_PANEL=TABBED_INSPECTOR_OR_PREVIEW
BOTTOM_PANEL=COLLAPSED_DEFAULT
TOOLBAR=COMPACT_WITH_OVERFLOW
DEFAULT_ZOOM=2X_OR_FIT
```

Acciones secundarias pasan al menú `⋯`; estado LIVE y gate técnico se llevan a la barra de estado. El nombre del sketch vinculado se conserva en título/status y no consume ancho crítico del toolbar.

### COMPACT

```text
LEFT_PANEL=RAIL_PLUS_DRAWER
RIGHT_PANEL=DRAWER
BOTTOM_PANEL=OVERLAY
TOOLBAR=MINIMAL_WITH_OVERFLOW
DEFAULT_ZOOM=FIT
```

Rail:

```text
P=Páginas
O=Objetos
C=Componentes
T=Herramientas
```

El panel derecho permite alternar `Inspector` / `Vista 1:1` bajo demanda.

### Persistencia Alpha11

Se conserva mediante `localStorage`:

```text
RIGHT_ACTIVE_VIEW
LEFT_COLLAPSE_STATE
BOTTOM_COLLAPSED
ZOOM_PER_LAYOUT_MODE
```

El resize manual de anchos de panel queda fuera del gate inmediato; primero se cierra paridad visual/funcional en WIDE, MEDIUM y COMPACT.

## Launcher

Archivos:

```text
tools/jwplc-hmi-designer/JWPLC-HMI-Designer.cmd
tools/jwplc-hmi-designer/Start-JWPLC-HMI-Designer.ps1
tools/jwplc-hmi-designer/JWPLC-HMI-Server.ps1
tools/jwplc-hmi-designer/Install-JWPLC-HMI-Designer.ps1
```

El launcher:

1. verifica si el servidor local ya está activo;
2. si no está activo, inicia el servidor oculto en `127.0.0.1:8765`;
3. espera el endpoint `/__health`;
4. abre Edge o Chrome con `--app=http://127.0.0.1:8765/desktop.html`;
5. usa el navegador predeterminado sólo como fallback.

El servidor se cierra por inactividad después de 60 minutos.

## Vinculación con sketch

El botón `Vincular sketch…` usa File System Access API. La carpeta seleccionada debe contener al menos un `.ino`.

Una vez vinculada, `Actualizar HMI`:

1. valida IDs/variables C++;
2. genera el output actual;
3. exige confirmación si `JWPLC_HMI_Generated.h` ya existe;
4. escribe directamente el header dentro de la carpeta del sketch.

No se escribe ni modifica el `.ino` del usuario.

## Proyecto `.jwhmi`

Formato Alpha11:

```text
format=JWPLC_HMI_PROJECT
version=1
pages<=16
fields<=32
target=ST7789_320x170_ROT3_RGB565
```

Se persisten páginas, fields declarativos, propiedades, geometría, colores, variables/IDs y página activa.

Convención recomendada:

```text
MiProyecto/
  MiProyecto.ino
  MiProyecto.jwhmi
  JWPLC_HMI_Generated.h
```

`.jwhmi` no es una unidad compilable por Arduino y puede convivir en la carpeta del sketch.

Limitación explícita:

```text
RAW_GFX_LAYER_PERSISTENCE=NO_ALPHA11
PIXEL_LAYER_PERSISTENCE=NO_ALPHA11
```

## Gate manual

Ya confirmados por usuario en Windows:

```text
A11_6_DESKTOP_LAUNCHER=PASS_USER_WINDOWS
A11_6_PROJECT_SAVE_OPEN=PASS_USER_WINDOWS
A11_6_SKETCH_LINK=PASS_USER_WINDOWS
A11_6_HEADER_DIRECT_WRITE=PASS_USER_WINDOWS
A11_6_LIVE_FROM_DESKTOP_APP=PASS_USER_WINDOWS
```

Gate responsive pendiente después de la implementación adaptativa:

```text
1. Pantalla completa -> WIDE.
2. Ventana exactamente ~50 % -> MEDIUM sin necesidad de achicar manualmente.
3. Confirmar toolbar sin wrap en MEDIUM.
4. Confirmar Inspector / Vista 1:1 por tabs.
5. Confirmar bottom colapsado por defecto en MEDIUM.
6. Reducir a ~25 % -> COMPACT.
7. Confirmar rail P/O/C/T y drawers.
8. Confirmar panel derecho bajo demanda.
9. Confirmar bottom overlay.
10. Confirmar LIVE y edición sin regresiones.
```

Criterio de salida:

```text
A11_6_RESPONSIVE_WIDE=PASS
A11_6_RESPONSIVE_MEDIUM_50_PERCENT=PASS
A11_6_RESPONSIVE_COMPACT_25_PERCENT=PASS
A11_6_RESPONSIVE_LAYOUT=PASS
A11_6_SKETCH_INTEGRATION=PASS
```

Hasta completar esa prueba:

```text
A11_6_RESPONSIVE_LAYOUT=IMPLEMENTED_PENDING_GATE
A11_6_SKETCH_INTEGRATION=IN_PROGRESS_UX_POLISH
```
