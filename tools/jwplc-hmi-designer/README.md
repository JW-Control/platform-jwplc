# JWPLC HMI Designer

Herramienta visual para diseñar la HMI/TFT del **JWPLC Basic** sobre `JWPLC_Display` / `JWPLC_UI`.

Estado de cierre:

```text
VERSION_SCOPE=v2.1.0-alpha.14 (JWPLC_Display 1.1.0, JWPLC_TFT 0.3.0)
TARGET=ST7789_320x170_ROT3_RGB565
TEXT=PASS
VALUE=PASS (miles, duración, fecha/hora del RTC)
BOOL=PASS (texto, piloto, color por estado)
BAR=PASS (repintado incremental)
MENU=PASS_PHYSICAL
ICON=PASS_HOST (biblioteca de 24 iconos; placa [no verificado])
PIXELMAP=PASS (pincel, borrador, mover, relleno, cuentagotas)
MULTIPAGE=PASS (pestañas con estado y color por página)
LIVE_WEB_SERIAL=PASS (protocolo v2 con pushImage)
LIVE_BRIDGE_FLASHER=PASS_HOST (instalación en un clic; placa pendiente)
CODEGEN=PASS
AGENT_CLI=PASS (jwhmi)
DESKTOP_APP=PASS_USER_WINDOWS
ARDUINO_IDE=recomendado 2.3.10 o superior
AUTOCOMPLETE_JWPLC_DISPLAY=PASS_USER
```

---

## Instalar sin el repositorio (cualquier persona, Windows)

Cada GitHub Release del package trae el Designer listo para usar, sin Node.js ni repositorio:

- **`JWPLC-HMI-Designer-Setup-<versión>-x64.exe`** (recomendado): doble clic y *Siguiente*. Instala por
  usuario, sin administrador, en `%LOCALAPPDATA%\JWPLC\HMI Designer`, con accesos en el Escritorio y
  el Menú Inicio y la extensión para Arduino IDE 2. Se desinstala desde *Aplicaciones* de Windows.
- **`JWPLC-HMI-Designer-Portable-<versión>-x64.exe`**: se abre sin instalar (por ejemplo, desde un USB).
  No crea accesos ni instala la extensión, así que el botón del Arduino IDE no lo encuentra.
- **`JWPLC-HMI-Designer-<versión>-SHA256.txt`**: sumas para verificar las descargas.

El .exe no está firmado: Windows SmartScreen puede mostrar «Windows protegió su PC». Pulsa
*Más información* y luego *Ejecutar de todas formas*.

Los genera `.github/workflows/hmi-designer-windows.yml`, que se ejecuta al publicar la Release
(`release-jwplc-package.yml`). Para regenerarlos en una Release ya publicada, ejecútalo a mano con la versión.
En cada PR que toca el Designer solo compila y deja los .exe como artifact.

## Instalar o actualizar la app de escritorio desde el repositorio (Windows)

Doble clic en **`Generar-e-Instalar-App-Escritorio.cmd`**. Requiere Node.js LTS
(`winget install OpenJS.NodeJS.LTS`); la primera vez descarga Electron.

1. Compila la app de escritorio (Electron) con la versión actual del repositorio.
2. La instala en `%LOCALAPPDATA%\JWPLC\HMI Designer`, con accesos en el Escritorio y el
   Menú Inicio y la extensión para Arduino IDE 2.

Repetirlo después de cada `git pull` que cambie el Designer, o usar el modo desarrollo de abajo.

### Ver los cambios sin reinstalar (modo desarrollo)

Doble clic en **`Abrir-Designer-Desarrollo.cmd`**: abre la misma app de escritorio directo desde el
repositorio, sin compilar ni instalar (la primera vez instala Electron en `electron\node_modules`).
La ventana dice `[desarrollo]` en el título.

- Al guardar cualquier archivo de `poc\` la ventana se recarga sola y **conserva el diseño en pantalla**.
- `F5` (o `Ctrl+R`) es como cerrar y volver a abrir la app: diseño en blanco y sin sketch vinculado.
  `F12` abre las DevTools.
- Si cambian `electron\main.js` o `electron\preload.cjs`, cierra la app y vuelve a abrir el `.cmd`.
- La app instalada no se toca y puede estar abierta a la vez. El modo desarrollo no registra el enlace
  `jwplc-hmi://` del Arduino IDE (sigue abriendo la app instalada).

Si una versión anterior del generador falló con `Cannot create symbolic link : El cliente no dispone de un
privilegio requerido` (en `winCodeSign`), basta con hacer `git pull` y volver a ejecutarlo. El paquete
`winCodeSign` de electron-builder trae enlaces simbólicos de macOS que Windows no deja crear sin modo
desarrollador; ahora el generador lo prepara en la caché de electron-builder sin esa carpeta
(`scripts/JWPLC-ElectronBuilderCache.ps1`). No hace falta activar el modo desarrollador ni ejecutar como administrador.

Sólo la app de escritorio puede **abrir el sketch en el Arduino IDE** y **abrir su
carpeta** (menú «Sketch: nombre ▾»). La versión de navegador no puede lanzar programas.

### Guardar y cambios sin guardar

- **Guardar** (o `Ctrl+S`) con un sketch vinculado hace lo mismo que «Escribir en sketch»: header,
  `.jwhmi` y guía. Sólo pregunta antes de reemplazar archivos de un sketch que no es el del diseño en
  pantalla.
- **Guardar sin sketch** (app de escritorio) abre el cuadro «Guardar» en `Documentos\Arduino` con el
  nombre del proyecto. Si se elige un sketch existente (su carpeta o el nombre de una carpeta con
  `.ino`), se vincula y se escribe ahí. Si no, ofrece **Crear sketch** (`<nombre>\<nombre>.ino` base,
  vinculado y escrito) o **Solo guardar .jwhmi**.
- **Nuevo**, **Abrir** y **cerrar la app** preguntan «¿Guardar los cambios?» (Guardar · No guardar ·
  Cancelar) si el diseño cambió desde lo último guardado o abierto. Zoom, selección y herramienta no
  cuentan como cambios. El `*` junto a «Proyecto:» usa la misma comparación.

### Abrir y «Nuevo proyecto» con el sketch

En la app de escritorio, **Abrir** despliega «Abrir archivo…» y los **proyectos recientes** (los últimos
10 `.jwhmi` abiertos o guardados, con su sketch). Uno que ya no existe se quita al elegirlo; «Limpiar
recientes» vacía la lista. **Abrir** un `.jwhmi` que está junto a un `.ino` vincula ese sketch; un `.jwhmi`
suelto (sin `.ino` en su carpeta) deja el Designer sin sketch vinculado. **Nuevo proyecto** también
desvincula el sketch. Necesita la app generada después del 2026-10-23.

Los avisos y confirmaciones son del propio Designer (`poc/designer-dialog.js`); los cuadros de abrir y
guardar archivos siguen siendo los de Windows.

### Crear un sketch nuevo desde el Designer

«Vincular sketch…» ofrece **Elegir sketch existente…** o **Crear sketch nuevo…** (también en el menú
«Sketch: nombre ▾»). Al crear, se elige nombre y ubicación (por defecto `Documentos\Arduino`) y el
Designer crea `<nombre>\<nombre>.ino` con el sketch base de la guía (el `#include`, `jwplcHMISetup()`
y las variables del diseño ya usadas), lo vincula y hace «Escribir en sketch»: queda listo para abrir y
compilar en el Arduino IDE. Nombre como en el Arduino IDE: letras, números, `_ . -`, empieza con letra o
número, máximo 63. Nunca reemplaza una carpeta que ya tenga archivos. Ese `.ino` es tuyo desde ahí: el
Designer no lo vuelve a modificar. Necesita la app generada después del 2026-10-23.

### Qué deja «Escribir en sketch» en la carpeta del sketch

| Archivo | Qué es |
|---|---|
| `JWPLC_HMI_Generated.h` | Pantallas, campos, MENU e IDLE en C++ (se reemplaza en cada escritura) |
| `<sketch>.jwhmi` | El diseño, para volver a abrirlo («Abrir HMI del sketch») |
| `JWPLC_HMI_LEEME.md` | Guía con el **sketch base** (todas las variables del HMI ya usadas, `menuChanged` del MENU) y los pasos para un sketch existente |

El `.ino` nunca se modifica: la guía trae el código para copiar. Si en la carpeta ya hay un
`JWPLC_HMI_LEEME.md` que no escribió el Designer, no se toca. La guía necesita la app generada
después del 2026-10-19 (si no, se escriben el header y el `.jwhmi` y el aviso pide regenerar la app).

### Qué hace cada archivo

| Archivo | Para qué | ¿Lo uso? |
|---|---|---|
| `Generar-e-Instalar-App-Escritorio.cmd` | Compila e instala la app de escritorio de forma unificada | **Sí: es el único paso** |
| `Abrir-Designer-Desarrollo.cmd` | Abre la app de escritorio desde el repo, sin instalar; recarga al guardar | Para probar cambios |
| `scripts/Install-DesktopApp.ps1` | Pipeline unificado de build Electron e instalación de la app | Invocado por el .cmd |
| `scripts/Build-JWPLC-HMI-Designer-Setup.ps1` | Genera el instalador `Setup.exe` (y con `-Portable` el portable) para repartir | Lo hace el CI al publicar |
| `scripts/Build-ArduinoIDE-Launcher.ps1` | Genera el VSIX de extensión de Arduino IDE 2 | Interno / CI |
| `scripts/Install-ArduinoIDE-Launcher.ps1` | Instala sólo la extensión de Arduino IDE 2 | Interno / Diagnóstico |

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
Generar-e-Instalar-App-Escritorio.cmd
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
.\scripts\Install-ArduinoIDE-Launcher.ps1
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
├─ Abrir-Designer-Desarrollo.cmd
├─ Generar-e-Instalar-App-Escritorio.cmd
├─ arduino-ide-launcher/
├─ electron/
├─ poc/
├─ scripts/
│  ├─ Install-DesktopApp.ps1
│  ├─ Build-JWPLC-HMI-Designer-Setup.ps1
│  ├─ Build-ArduinoIDE-Launcher.ps1
│  ├─ Install-ArduinoIDE-Launcher.ps1
│  └─ JWPLC-ElectronBuilderCache.ps1
└─ tests/
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
