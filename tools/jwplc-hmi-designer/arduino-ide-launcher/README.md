# JWPLC HMI Designer Launcher · Arduino IDE 2

Integración **experimental** para abrir JWPLC HMI Designer desde Arduino IDE 2 sin incrustar el Designer dentro del IDE y sin mantener un fork.

## Alcance

El plugin registra:

```text
JWPLC: Abrir HMI Designer
```

Además intenta exponer:

```text
barra de estado -> JW HMI
editor/title    -> icono JW (best-effort)
```

La barra de estado es el launcher visual principal del gate. La ubicación exacta del botón `editor/title` puede variar según la versión de Arduino IDE/Theia.

## Dependencia

JWPLC HMI Designer debe estar instalado en:

```text
%LOCALAPPDATA%\JWPLC\HMI Designer
```

O definirse:

```text
JWPLC_HMI_DESIGNER_HOME
```

El plugin sólo abre la aplicación externa. No modifica compilación, board manager, Arduino CLI, sketch ni runtime del IDE.

## Compatibilidad objetivo Alpha11

```text
Arduino IDE: 2.3.4
Theia base: 1.41.x
Windows: objetivo inicial
```

Arduino IDE documenta la carpeta de plugins de usuario para VSIX de terceros en:

```text
%USERPROFILE%\.arduinoIDE\plugins
```

El uso con extensiones funcionales distintas de temas se considera experimental y se valida físicamente antes de declararlo soportado.

## Build sin Node/npm

Desde `tools\jwplc-hmi-designer`:

```powershell
.\Build-ArduinoIDE-Launcher.ps1
```

Genera:

```text
dist\jwplc-hmi-launcher-0.1.0.vsix
```

## Instalación

Con el Designer ya instalado:

```powershell
.\Install-ArduinoIDE-Launcher.ps1
```

Después se debe cerrar completamente Arduino IDE y volver a abrirlo.

## Gate Alpha11

```text
VSIX_DISCOVERED=?
COMMAND_PALETTE_VISIBLE=?
STATUSBAR_BUTTON_VISIBLE=?
EDITOR_TITLE_BUTTON_VISIBLE=?
LAUNCHES_DESIGNER=?
IDE_COMPILE_UPLOAD_REGRESSION=0_REQUIRED
```

Si el VSIX no es aceptado por Arduino IDE 2.3.4, la aplicación standalone continúa siendo el camino oficial y no se parchea ni forkea el IDE.
