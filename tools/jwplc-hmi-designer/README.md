# JWPLC HMI Designer

Herramienta visual para diseñar la HMI/TFT del **JWPLC Basic** sobre `JWPLC_Display` / `JWPLC_UI`.

Estado de cierre:

```text
VERSION_SCOPE=v2.1.0-alpha.11
TARGET=ST7789_320x170_ROT3_RGB565
TEXT=PASS
VALUE=PASS
BOOL=PASS
BAR=PASS
PIXELMAP=PASS
MULTIPAGE=PASS
LIVE_WEB_SERIAL=PASS
CODEGEN=PASS
DESKTOP_APP=PASS_USER_WINDOWS
ARDUINO_IDE_2_3_4=PASS_EXPERIMENTAL
AUTOCOMPLETE_JWPLC_DISPLAY=PASS_USER
```

---

## Flujo recomendado

El Designer genera:

```text
JWPLC_HMI_Generated.h
```

El header contiene:

```text
HMIPageId
HMIFieldId
variables HMI
JWPLC_UIField[]
PixelMaps
jwplcHMISetup()
jwplcUIUpdate()
```

El `.ino` conserva la lógica de aplicación.

```cpp
#include <JWPLC_Display.h>
#include <JWPLC_HMI_Generated.h>

void setup()
{
    jwplcHMISetup();
}

void loop()
{
    // lógica de proceso, sensores, E/S, comunicaciones y botones
}
```

Regla Alpha11:

```text
loop()          = lógica de aplicación
jwplcUIUpdate() = sincronización gráfica autogenerada
```

---

## Proyecto `.jwhmi`

Convención:

```text
MiProyecto/
├─ MiProyecto.ino
├─ MiProyecto.jwhmi
└─ JWPLC_HMI_Generated.h
```

El Designer no modifica automáticamente el `.ino`.

`Actualizar HMI` escribe `JWPLC_HMI_Generated.h` después de validar IDs/símbolos y pedir confirmación si debe reemplazar un archivo existente.

Límites V1:

```text
MAX_PAGES=16
MAX_FIELDS=32
```

---

## Fields

Tipos V1:

```text
TEXT
VALUE
BOOL
BAR
```

El motor usa dirty refresh y caché para evitar redibujados globales innecesarios.

---

## PixelMap

Alpha11 incorpora una capa de PixelMap con:

- edición por píxel;
- capas;
- brush/eraser;
- fill;
- eyedropper;
- línea;
- rectángulo;
- undo/redo específico;
- RGB565;
- hide/onion-skin en Designer;
- persistencia de proyecto;
- visibilidad runtime;
- codegen optimizado.

El generador puede elegir:

```text
RGB565_RUN
PACKED_SPAN16
```

`PACKED_SPAN16` se usa cuando reduce memoria/código y la paleta cabe en 16 colores.

---

## Navegación multipágina

Indicador físico:

```text
NN/TT
```

Semántica:

```text
PAGE_SELECT
  LEFT / RIGHT -> cambiar página
  OK           -> entrar

PAGE_CONTENT
  LEFT / RIGHT / UP / DOWN / OK -> aplicación
  ESC                            -> volver al selector
```

Al volver de CONTENT a SELECT se limpian eventos pendientes y se resincroniza el estado físico.

---

## LIVE Preview

Transporte:

```text
Web Serial
baud=921600
RX_BUFFER=8192
FRAME_ROWS=32
FLOW_CONTROL=ACK
DIRTY_REGION=JWH2
LATEST_STATE_COALESCING=YES
```

LIVE quedó validado físicamente y no se reemplaza por otro runtime en Alpha11.

---

## Responsive

La interfaz está pensada para 4K y 1080p mediante layout adaptativo y Fit continuo.

Clasificación de referencia:

```text
WIDE     >= 70 %
MEDIUM   38..69 %
COMPACT  < 38 %
```

Los gates visuales WIDE y MEDIUM/50 % fueron aprobados por usuario.

---

## Instalación Windows

Instalador:

```text
Install-JWPLC-HMI-Designer.cmd
```

Destino:

```text
%LOCALAPPDATA%\JWPLC\HMI Designer
```

Entrada:

```text
JWPLC-HMI-Designer.exe
```

Identidad visual final:

```text
DESKTOP_SHORTCUT_ICON=PASS
WINDOW_ICON=PASS
TASKBAR_ICON=PASS
```

La aplicación usa Electron nativo como entrada principal y mantiene un servidor local interno para el frontend/LIVE.

---

## Extensión Arduino IDE 2

El Designer **no** se incrusta dentro de Arduino IDE y no se mantiene un fork del IDE.

La integración usa un VSIX de usuario:

```text
%USERPROFILE%\.arduinoIDE\plugins
```

Versión final Alpha11:

```text
jwplc-hmi-launcher-0.1.6.vsix
```

Objetivo validado:

```text
Arduino IDE 2.3.4
Theia 1.41.x
Windows
```

La extensión aporta:

1. comando `JWPLC: Abrir HMI Designer`;
2. botón `JW HMI` en barra de estado;
3. icono de editor best-effort;
4. autocompletado contextual de `JWPLC_Display`.

Después de instalar/actualizar el VSIX se debe cerrar completamente Arduino IDE y abrirlo otra vez.

---

## Autocompletado `JWPLC_Display`

Al escribir:

```cpp
JWPLC_Display.
```

se ofrece una **API recomendada curada**, no un dump de todos los métodos compatibles.

Principio:

```text
API_COMPATIBLE_WIDE=YES
AUTOCOMPLETE_RECOMMENDED_PATH_ONLY=YES
```

Por ejemplo, `tft()` se prioriza como acceso raw canónico y `display()` no se duplica en la lista.

Los getters de configuración siguen existiendo por compatibilidad, pero no se priorizan porque pueden confundir al usuario con setters equivalentes.

Contextos configurables:

```text
setIdleWakeMode(     -> IDLE_WAKE_*
setIdleWakeButton(   -> BTN_*
setIdleReturnMode(   -> IDLE_RETURN_*
setIdleReturnButton( -> BTN_*
setUserRefreshMode(  -> USER_REFRESH_*
```

Gate físico/UX:

```text
A11_6_ARDUINO_IDE_AUTOCOMPLETE=PASS_USER_2_3_4
```

---

## Instalador del launcher

Diagnóstico/instalación independiente:

```powershell
.\Install-ArduinoIDE-Launcher.ps1
```

El instalador del VSIX quedó desacoplado de la presencia del Designer:

```text
DESIGNER_REQUIRED_FOR_AUTOCOMPLETE=NO
DESIGNER_REQUIRED_FOR_LAUNCH_BUTTON=YES
```

Si la app no está instalada, el VSIX/autocompletado se instala igualmente y sólo se advierte que el botón `JW HMI` no podrá abrirla.

---

## Botonera y loops cerrados

El sketch normal puede consultar:

```cpp
JWPLC_Buttons.pressed(BTN_UP);
JWPLC_Buttons.released(BTN_OK);
JWPLC_Buttons.isDown(BTN_LEFT);
```

sin añadir `delay()` ni Serial.

También quedó validado un `loop()` intensivo con `digitalWrite(Q0_0, estado)` en cada vuelta sin necesidad de `delay(1)`.

```text
ALPHA11_USER_DELAY_REQUIRED=NO
ALPHA11_RUNTIME_CLOSED_LOOP=PASS_PHYSICAL
```

---

## Archivos principales

```text
tools/jwplc-hmi-designer/
├─ Install-JWPLC-HMI-Designer.cmd
├─ Install-JWPLC-HMI-Designer.ps1
├─ Build-JWPLC-HMI-Designer-Electron.ps1
├─ Build-ArduinoIDE-Launcher.ps1
├─ Install-ArduinoIDE-Launcher.ps1
├─ arduino-ide-launcher/
├─ electron/
├─ poc/
└─ gates/
```

---

## Documentación de cierre

```text
docs/v2.1.0-alpha.11/ALPHA11_STATUS.md
docs/v2.1.0-alpha.11/ALPHA11_CLOSURE_CHECKLIST.md
docs/v2.1.0-alpha.11/ALPHA11_BUILD_BENCHMARK.md
docs/v2.1.0-alpha.11/ALPHA11_HMI_DESIGNER_ARCHITECTURE.md
docs/v2.1.0-alpha.11/A11_6_DESKTOP_INTEGRATION_GATE.md
docs/v2.1.0-alpha.11/A11_6_ICON_TASKBAR_GATE.md
```

## Estado final Alpha11

```text
HMI_DESIGNER_V1=PASS_USER
DESKTOP_INSTALL=PASS_USER_WINDOWS
ARDUINO_IDE_LAUNCHER=PASS_EXPERIMENTAL_2_3_4
AUTOCOMPLETE=PASS_USER
CODEGEN=PASS
LIVE=PASS
PIXELMAP=PASS
MULTIPAGE=PASS
NEXT=PR_RELEASE_V2_1_X
```
