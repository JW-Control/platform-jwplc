# Alpha11 · A11-6 Integración de escritorio y sketch

Fecha: 2026-09-06

## Objetivo

Convertir el JWPLC HMI Designer en una herramienta diaria instalable, vinculada al sketch Arduino y accesible sin arrancar manualmente `localhost`.

La aplicación standalone es el producto principal. Arduino IDE 2 puede recibir un launcher experimental que **sólo abre la aplicación externa**; no se incrusta el Designer ni se mantiene un fork del IDE.

## Arquitectura

```text
JWPLC HMI Designer
  -> instalación usuario (%LOCALAPPDATA%)
  -> launcher Windows
  -> servidor localhost privado
  -> Edge/Chrome --app
  -> desktop.html
  -> Designer + LIVE + File System Access

Sketch Arduino
  -> Proyecto.ino
  -> Proyecto.jwhmi
  -> JWPLC_HMI_Generated.h

Arduino IDE 2 (experimental)
  -> VSIX usuario
  -> comando JWPLC: Abrir HMI Designer
  -> botón JW HMI
  -> abre la app instalada
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
PROJECT_CANONICAL_LOCATION=SKETCH_FOLDER
PROJECT_CANONICAL_NAME=<SKETCH>.jwhmi

SKETCH_FOLDER_LINK=YES
SKETCH_MUST_CONTAIN_INO=YES
LINK_HANDLE_PERSISTENCE=INDEXED_DB
GENERATED_HEADER_WRITE=YES
GENERATED_HEADER_NAME=JWPLC_HMI_Generated.h
GENERATED_HEADER_OVERWRITE_CONFIRM=YES
DUPLICATE_IDENTIFIER_BLOCKS_WRITE=YES

LIVE_WEB_SERIAL=PRESERVED
ARDUINO_IDE_FORK_REQUIRED=NO
ARDUINO_IDE_LAUNCHER_EXPERIMENT=YES
```

## Responsive adaptativo

La clasificación horizontal se calcula principalmente mediante la proporción de la ventana respecto al ancho disponible de la pantalla actual:

```text
WINDOW_RATIO = window.outerWidth / screen.availWidth

WIDE     >= 70 %
MEDIUM   38..69 %
COMPACT  < 38 %
```

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
```

### MEDIUM

```text
LEFT_PANEL=OPEN_COMPACT
COMPONENTS_TOOLS=COLLAPSIBLE
RIGHT_PANEL=TABBED_INSPECTOR_OR_PREVIEW
BOTTOM_PANEL=COLLAPSED_DEFAULT
TOOLBAR=COMPACT_WITH_OVERFLOW
```

### COMPACT

```text
LEFT_PANEL=RAIL_PLUS_DRAWER
RIGHT_PANEL=DRAWER
BOTTOM_PANEL=OVERLAY
TOOLBAR=MINIMAL_WITH_OVERFLOW
```

### Ajustar canvas

`Ajustar canvas` ya no selecciona un zoom entero. Usa Fit continuo:

```text
FIT_ZOOM = min(viewportWidth / 320, viewportHeight / 170)
```

Se conserva la relación 320:170 y se recalcula automáticamente al redimensionar mientras el modo Fit esté activo.

Zoom manual permanece disponible:

```text
1x / 2x / 3x / 4x / 6x / 8x
```

Gate visual confirmado por usuario en WIDE y MEDIUM, incluyendo Fit continuo.

## Instalación Windows

Fuente de desarrollo:

```text
tools/jwplc-hmi-designer/
```

Instalación real de usuario:

```text
%LOCALAPPDATA%\JWPLC\HMI Designer
```

Archivos de instalación:

```text
Install-JWPLC-HMI-Designer.cmd
Install-JWPLC-HMI-Designer.ps1
Start-JWPLC-HMI-Designer.ps1
JWPLC-HMI-Server.ps1
```

El instalador:

1. copia la aplicación fuera del repositorio;
2. crea acceso directo en Escritorio;
3. crea `Menú Inicio/JWPLC/JWPLC HMI Designer`;
4. define `JWPLC_HMI_DESIGNER_HOME` a nivel usuario;
5. opcionalmente instala el launcher experimental de Arduino IDE.

Comando de prueba:

```powershell
.\Install-JWPLC-HMI-Designer.ps1 -InstallArduinoIDELauncher
```

También existe instalador de doble clic:

```text
Install-JWPLC-HMI-Designer.cmd
```

## Vinculación con sketch

`Vincular sketch…` usa File System Access API. La carpeta debe contener al menos un `.ino`.

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

Cuando existe sketch vinculado y aún no hay un archivo de proyecto abierto/guardado, `Guardar` crea automáticamente:

```text
<Sketch>\<Sketch>.jwhmi
```

Convención oficial propuesta:

```text
MiProyecto/
  MiProyecto.ino
  MiProyecto.jwhmi
  JWPLC_HMI_Generated.h
```

`.jwhmi` no es una unidad compilable por Arduino.

## Launcher experimental Arduino IDE 2

### Motivo

Arduino IDE 2 usa Eclipse Theia y carga plugins VSIX desde su carpeta de configuración de usuario. Arduino documenta esa carpeta para temas de terceros; usar un VSIX funcional con comandos se considera **experimental** hasta pasar gate físico en Arduino IDE 2.3.4.

No se modifica el IDE y no se distribuye fork.

### Archivos

```text
arduino-ide-launcher/package.json
arduino-ide-launcher/extension.js
arduino-ide-launcher/media/jw-hmi.svg
Build-ArduinoIDE-Launcher.ps1
Install-ArduinoIDE-Launcher.ps1
```

El build VSIX no necesita npm/Node para empaquetar:

```powershell
.\Build-ArduinoIDE-Launcher.ps1
```

Salida:

```text
dist\jwplc-hmi-launcher-0.1.0.vsix
```

Instalación objetivo:

```text
%USERPROFILE%\.arduinoIDE\plugins\jwplc-hmi-launcher-0.1.0.vsix
```

El plugin busca el Designer en:

```text
JWPLC_HMI_DESIGNER_HOME
```

con fallback:

```text
%LOCALAPPDATA%\JWPLC\HMI Designer
```

### Superficies de acceso

Esperadas:

```text
Command Palette -> JWPLC: Abrir HMI Designer
Status bar      -> JW HMI
Editor title    -> icono JW (best-effort)
```

La barra de estado se considera el botón principal porque usa una API estándar VS Code/Theia. `editor/title` se evalúa como mejora adicional y puede variar con Arduino IDE.

### Gate físico Arduino IDE 2.3.4

```text
1. Instalar Designer standalone.
2. Instalar VSIX.
3. Cerrar todas las ventanas de Arduino IDE.
4. Abrir Arduino IDE 2.3.4.
5. Confirmar que el IDE inicia normalmente.
6. Ctrl+Shift+P -> buscar "JWPLC: Abrir HMI Designer".
7. Confirmar botón "JW HMI" en barra de estado.
8. Pulsar comando/botón -> Designer abre.
9. Compilar sketch JWPLC.
10. Subir sketch JWPLC.
11. Confirmar cero regresiones del IDE.
```

Resultado posible:

```text
ARDUINO_IDE_LAUNCHER=PASS_EXPERIMENTAL
```

o, si Arduino IDE no acepta el VSIX funcional:

```text
ARDUINO_IDE_LAUNCHER=UNSUPPORTED
STANDALONE_APP=PRIMARY
ARDUINO_IDE_FORK=NO
```

## Gates confirmados

```text
A11_6_DESKTOP_LAUNCHER=PASS_USER_WINDOWS
A11_6_PROJECT_SAVE_OPEN=PASS_USER_WINDOWS
A11_6_SKETCH_LINK=PASS_USER_WINDOWS
A11_6_HEADER_DIRECT_WRITE=PASS_USER_WINDOWS
A11_6_LIVE_FROM_DESKTOP_APP=PASS_USER_WINDOWS
A11_6_RESPONSIVE_WIDE=PASS_USER_VISUAL
A11_6_RESPONSIVE_MEDIUM_50_PERCENT=PASS_USER_VISUAL
A11_6_FIT_CONTINUOUS=PASS_USER_VISUAL
```

Pendiente inmediato:

```text
A11_6_PROJECT_CANONICAL_SAVE=PENDING_USER_GATE
A11_6_STANDALONE_INSTALLER=PENDING_USER_GATE
A11_6_ARDUINO_IDE_LAUNCHER=PENDING_USER_GATE
```

Criterio de salida A11-6:

```text
PROJECT_CANONICAL_SAVE=PASS
STANDALONE_INSTALLER=PASS
ARDUINO_IDE_LAUNCHER=PASS_EXPERIMENTAL_OR_EXPLICIT_UNSUPPORTED
SKETCH_INTEGRATION=PASS
```
