# Alpha11 · A11-6 Gate de icono de ventana y barra de tareas

Fecha: 2026-09-06

## Objetivo

Cerrar la identidad visual del JWPLC HMI Designer en Windows antes del cierre de Alpha11, verificando que el icono del acceso directo, la ventana de aplicación y la barra de tareas sean coherentes.

## Problema reproducido

El acceso directo de Escritorio usa correctamente `JWPLC-HMI-Designer.ico`, pero el modo Chromium `--app` no conserva esa misma identidad en la barra de tareas. Se probaron favicon SVG y familias PNG raster de 16–64 px; Windows/Chromium siguió mostrando un icono distinto o visualmente degradado.

El problema deja de tratarse como un problema de resolución del favicon y pasa a considerarse un problema de **identidad de aplicación**: la ventana pertenece al ejecutable Chromium, no al ejecutable JWPLC que crea el acceso directo.

## Referencia JW-Serial

JW-Serial no usa un navegador externo como ventana final. Usa Electron:

```text
BrowserWindow(... icon: appIconPath ...)
electron-builder
win.icon=build/icon.ico
appId propio
```

Esto permite que el EXE, la ventana y la barra de tareas compartan una identidad nativa de Windows.

## Último ajuste Alpha11

Se añade un wrapper Electron nativo para JWPLC HMI Designer, manteniendo la aplicación HMI existente como frontend y sin reescribir su lógica.

```text
NATIVE_WRAPPER=ELECTRON
APP_ID=com.jwcontrol.jwplc.hmidesigner
EXE_ICON=JWPLC-HMI-Designer.ico
BROWSERWINDOW_ICON=JWPLC-HMI-Designer.ico
STATIC_FRONTEND=poc/
LOCAL_HTTP=127.0.0.1:puerto_dinamico
LIVE_WEB_SERIAL=SUPPORTED_BY_ELECTRON_SESSION
CHROMIUM_EXTERNAL_MODE=FALLBACK_ONLY
```

El wrapper:

- crea una `BrowserWindow` nativa;
- usa exactamente el mismo `.ico` aprobado para el EXE y la ventana;
- fija un `AppUserModelID` propio de Windows;
- sirve el frontend existente desde un servidor HTTP local interno;
- mantiene LIVE/Web Serial mediante permisos de sesión Electron y selector de puerto;
- usa `show: false` + `ready-to-show` para evitar el flash visual del arranque;
- mantiene una sola instancia del Designer.

## Build de desarrollo

Antes de reinstalar el Designer debe generarse el ejecutable Electron:

```powershell
.\tools\jwplc-hmi-designer\Build-JWPLC-HMI-Designer-Electron.ps1
```

El resultado esperado es:

```text
tools/jwplc-hmi-designer/dist/JWPLC-HMI-Designer-Electron.exe
```

Después:

```powershell
.\tools\jwplc-hmi-designer\Install-JWPLC-HMI-Designer.cmd
```

El instalador debe indicar:

```text
Modo: ELECTRON_NATIVE
```

Si el artifact Electron no existe, el instalador conserva temporalmente el launcher Chromium anterior como fallback y muestra:

```text
Modo: CHROMIUM_FALLBACK
```

## Gate de usuario

```text
1. Cerrar completamente JWPLC HMI Designer.
2. Cerrar Arduino IDE.
3. git pull --ff-only.
4. Ejecutar Build-JWPLC-HMI-Designer-Electron.ps1.
5. Confirmar que termina con Build Electron completado.
6. Ejecutar Install-JWPLC-HMI-Designer.cmd.
7. Confirmar Modo: ELECTRON_NATIVE.
8. Abrir Designer desde Escritorio.
9. Confirmar que el icono grande del acceso directo no cambió.
10. Confirmar que ventana y barra de tareas usan el mismo arte del acceso directo.
11. Confirmar ausencia del flash blanco/HTML sin estilos.
12. Abrir desde JW HMI en Arduino IDE.
13. Confirmar Guardar, Actualizar HMI y LIVE.
```

## Resultado esperado

```text
A11_6_DESKTOP_ICON=PASS
A11_6_WINDOW_ICON=PASS
A11_6_TASKBAR_ICON=PASS
A11_6_APP_BOOT_GUARD=PASS
A11_6_ARDUINO_IDE_LAUNCHER=PASS_EXPERIMENTAL
A11_6_NATIVE_WRAPPER=PASS
```

Hasta confirmación física/visual:

```text
A11_6_ICON_TASKBAR_GATE=IMPLEMENTED_PENDING_USER_GATE
```

Este es el último intento de iconografía para Alpha11. Si el gate nativo no aporta una mejora suficiente, se documentará el defecto visual como pendiente no bloqueante y se procederá al cierre técnico: regeneración de `JWPLC_Display` precompilado, comparación source/precompiled, benchmark final, smoke físico, README/checklist y PR/PreRelease.
