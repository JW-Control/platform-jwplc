# JWPLC HMI Designer

Herramienta visual para diseñar la HMI/TFT del **JWPLC Basic** sobre la API pública `JWPLC_Display` / `JWPLC_UI`.

Estado actual:

```text
Alpha11
Target: ST7789 / 320 x 170 / rotation 3 / RGB565
TEXT / VALUE / BOOL / BAR: PASS
Páginas + indicador NN/TT: PASS
LIVE Web Serial: PASS
Codegen JWPLC_HMI_Generated.h: PASS
Robustez botonera pressed()/released(): PASS_PHYSICAL
Responsive WIDE/MEDIUM/COMPACT: PASS_USER_VISUAL
Integración app/sketch: CLOSING_GATE
Arduino IDE launcher: EXPERIMENTAL_PENDING_GATE
```

## Flujo recomendado

El Designer genera toda la capa de presentación en:

```text
JWPLC_HMI_Generated.h
```

Ese header contiene:

```text
HMIPageId
HMIFieldId
variables HMI
JWPLC_UIField[]
jwplcHMISetup()
jwplcUIUpdate() autogenerado
```

El `.ino` del usuario conserva la lógica de aplicación:

```cpp
#include "JWPLC_HMI_Generated.h"

void setup()
{
    jwplcHMISetup();
    JWPLC_Display.enterUserUI();
}

void loop()
{
    // lógica de proceso, botones, sensores, E/S, Modbus, etc.
}
```

Regla Alpha11:

```text
loop()          = lógica del programa
jwplcUIUpdate() = sincronización gráfica autogenerada
```

## Instalación Windows

Durante desarrollo puede ejecutarse desde el repositorio con:

```text
JWPLC-HMI-Designer.cmd
```

Para una instalación independiente del repositorio existe:

```text
Install-JWPLC-HMI-Designer.cmd
```

El instalador copia la aplicación a:

```text
%LOCALAPPDATA%\JWPLC\HMI Designer
```

crea accesos en Escritorio y menú Inicio `JWPLC`, y define:

```text
JWPLC_HMI_DESIGNER_HOME
```

El usuario instalado ya no depende de `GitHub\platform-jwplc\tools` para ejecutar el Designer.

El launcher:

- inicia automáticamente un servidor privado en `127.0.0.1:8765`;
- abre Edge o Chrome en modo aplicación (`--app`);
- conserva el contexto seguro necesario para Web Serial y acceso controlado a carpetas;
- no requiere ejecutar manualmente `py -m http.server`.

## Abrir / Guardar proyecto

La aplicación usa proyectos:

```text
*.jwhmi
```

Formato Alpha11:

```text
JWPLC_HMI_PROJECT v1
máximo 16 páginas
máximo 32 fields
ST7789 320x170 rotation 3 RGB565
```

Se guardan páginas, campos declarativos, geometría, estilos, colores, IDs/variables y página activa.

Cuando existe un sketch vinculado y el proyecto todavía no tiene archivo propio, **Guardar** crea automáticamente:

```text
<Sketch>\<Sketch>.jwhmi
```

Por ejemplo:

```text
test\
├─ test.ino
├─ test.jwhmi
└─ JWPLC_HMI_Generated.h
```

`.jwhmi` no es una unidad compilable de Arduino y puede convivir en la misma carpeta del sketch para versionado y regeneración.

Limitación Alpha11: las herramientas técnicas `Texto GFX RAW` y capa manual de píxeles no forman parte de la persistencia `.jwhmi` de producción.

## Vincular con sketch

En la aplicación:

```text
Vincular sketch…
```

selecciona una carpeta que contenga un archivo `.ino`.

Después:

```text
Actualizar HMI
```

escribe directamente:

```text
<Sketch>\JWPLC_HMI_Generated.h
```

El Designer valida primero los identificadores C++, y pide confirmación antes de reemplazar un header existente.

El `.ino` nunca se modifica automáticamente.

Arduino IDE detectará el cambio del archivo del sketch y el usuario compila/sube normalmente.

## Launcher experimental para Arduino IDE 2

La aplicación standalone es el camino principal. Alpha11 incluye además un experimento de integración mínima con Arduino IDE 2: **el plugin no incrusta el Designer ni modifica el IDE; sólo abre la aplicación instalada**.

Archivos:

```text
arduino-ide-launcher\
Build-ArduinoIDE-Launcher.ps1
Install-ArduinoIDE-Launcher.ps1
```

Instalación manual del gate:

```powershell
.\Install-ArduinoIDE-Launcher.ps1
```

O desde el instalador principal:

```powershell
.\Install-JWPLC-HMI-Designer.ps1 -InstallArduinoIDELauncher
```

El VSIX se copia a la carpeta de plugins de usuario documentada por Arduino IDE:

```text
%USERPROFILE%\.arduinoIDE\plugins
```

Después se debe cerrar completamente Arduino IDE y abrirlo nuevamente.

Gate esperado en Arduino IDE 2.3.4:

```text
Ctrl+Shift+P -> JWPLC: Abrir HMI Designer
barra de estado -> JW HMI
editor/title -> icono JW (best-effort)
```

La barra de estado es el botón visual principal del experimento. `editor/title` depende de cómo Arduino IDE/Theia exponga el menú en esa versión.

Si el VSIX funcional no es aceptado por Arduino IDE, no se parchea ni forkea el IDE: la aplicación instalada, Escritorio y menú Inicio continúan siendo el flujo soportado.

## Responsive / Ajustar

La shell clasifica la ventana principalmente por porcentaje de pantalla:

```text
WIDE     >= 70 %
MEDIUM   38..69 %
COMPACT  < 38 %
```

`Ajustar canvas` usa Fit continuo y calcula la escala máxima que conserva 320:170 dentro del área editable. Los zoom manuales `1×/2×/3×/4×/6×/8×` siguen disponibles.

## LIVE Preview

LIVE se mantiene sobre Web Serial:

```text
baud: 921600
RX buffer: 8192
frame rows: 32
ACK flow control
JWH2 dirty regions
latest-state coalescing
```

La aplicación de escritorio sigue usando Edge/Chrome precisamente para conservar Web Serial sin introducir un segundo protocolo o runtime.

## Páginas

El indicador físico usa:

```text
01/03
```

Semántica:

```text
PAGE_SELECT
  fondo negro / texto blanco
  LEFT / RIGHT = cambiar página
  OK           = entrar

PAGE_CONTENT
  fondo blanco / texto negro
  LEFT/RIGHT/UP/DOWN/OK = usuario
  ESC                    = volver a selector
```

Los símbolos C++ generados evitan números mágicos:

```cpp
if (!JWPLC_Display.isUserPageSelection() &&
    JWPLC_Display.userPage() == PAGE_PROCESO)
{
    // lógica de esa página
}
```

## Botonera: regla importante de robustez

El package mantiene el escaneo de la matriz automáticamente desde `jwplcBtnScan`.

Un sketch normal puede consultar:

```cpp
JWPLC_Buttons.pressed(BTN_UP)
JWPLC_Buttons.released(BTN_OK)
JWPLC_Buttons.isDown(BTN_LEFT)
```

en un `loop()` cerrado **sin añadir `delay()` ni Serial para estabilizar el escaneo**.

Durante Alpha11 se reprodujo el fallo intermitente observado previamente en taller: un `loop()` intensivo consultando `pressed()` podía competir con el scanner y dejar la interacción aparentemente colgada. Se corrigió elevando la prioridad del task de scan y validando su creación.

También se corrigió la transición `PAGE_CONTENT -> PAGE_SELECT`: al pulsar ESC se limpian PRESS/RELEASE/REPEAT pendientes y se resincroniza el estado físico, evitando reingresos fantasma por un `OK` pendiente.

Por lo tanto, en un sketch normal **no** se debe:

```cpp
JWPLC_Buttons.update();
```

ni iniciar otro task de escaneo sobre `JWPLC_Buttons`.

## PoC / desarrollo manual

La ruta base sigue siendo:

```text
tools/jwplc-hmi-designer/poc/
```

Para depuración web también puede usarse un servidor manual:

```powershell
cd tools\jwplc-hmi-designer\poc
py -m http.server 8080
```

Pero el flujo recomendado para usuario es la instalación independiente.

## Documentación Alpha11

```text
docs/v2.1.0-alpha.11/ALPHA11_STATUS.md
docs/v2.1.0-alpha.11/A11_3B_VALUE_FIELD_GATE.md
docs/v2.1.0-alpha.11/A11_3C_BOOL_FIELD_GATE.md
docs/v2.1.0-alpha.11/A11_3D_BAR_FIELD_GATE.md
docs/v2.1.0-alpha.11/A11_3E_MULTI_FIELD_PAGES_GATE.md
docs/v2.1.0-alpha.11/A11_4_CODEGEN_GATE.md
docs/v2.1.0-alpha.11/A11_BUTTON_ROBUSTNESS_GATE.md
docs/v2.1.0-alpha.11/A11_5_PHYSICAL_PARITY_GATE.md
docs/v2.1.0-alpha.11/A11_6_DESKTOP_INTEGRATION_GATE.md
```
