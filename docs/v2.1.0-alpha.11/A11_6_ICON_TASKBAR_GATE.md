# Alpha11 · A11-6 Gate de icono de ventana y barra de tareas

Fecha: 2026-09-06

## Objetivo

Cerrar la identidad visual del JWPLC HMI Designer en Windows antes del cierre de Alpha11, verificando que el icono del acceso directo, la ventana de aplicación y la barra de tareas sean coherentes y legibles también a tamaños pequeños.

## Resultado visual confirmado

La aplicación nativa Electron ya usa correctamente el mismo arte aprobado del acceso directo para:

```text
DESKTOP_SHORTCUT_ICON=PASS
WINDOW_ICON=PASS
TASKBAR_ICON=PASS
```

Se abandona el enfoque Chromium `--app` como camino principal. El Designer Alpha11 queda empaquetado como aplicación Electron nativa, siguiendo el mismo criterio general utilizado por JW-Serial: aplicación con identidad Windows propia, `BrowserWindow` y recurso `.ico` asociado al ejecutable.

## Arquitectura final Alpha11

```text
JWPLC-HMI-Designer.exe
        |
        +-- Electron
              |
              +-- BrowserWindow nativa
              +-- AppUserModelID propio
              +-- JWPLC-HMI-Designer.ico
              +-- servidor HTTP local interno
              +-- frontend HMI existente
```

La ventana se crea inicialmente oculta y se muestra en `ready-to-show`, con `backgroundColor` oscuro, para evitar el flash blanco/HTML sin estilos durante el arranque.

## Integración Arduino IDE

El botón `JW HMI` continúa siendo una integración experimental y externa al runtime de Arduino IDE.

Problema observado en gate físico:

```text
ARDUINO_IDE_STATUS=JW HMI: abriendo Designer...
DESIGNER_WINDOW=NO_OPEN
DESKTOP_LAUNCH=PASS
```

El ejecutable instalado sí funciona correctamente al abrirse desde Escritorio; por tanto el fallo queda acotado al método de lanzamiento desde el host Theia/VS Code de Arduino IDE.

Corrección aplicada en launcher `0.1.3`:

```text
WINDOWS_PRIMARY_LAUNCH=PowerShell Start-Process
WINDOWS_DIRECT_SPAWN=FALLBACK_1
CUSTOM_PROTOCOL=jwplc-hmi://open FALLBACK_2
WINDOWS_HIDE_ON_DESIGNER=FALSE
OLD_VSIX_VERSION=0.1.2
NEW_VSIX_VERSION=0.1.3
```

`Start-Process` reproduce el comportamiento de un acceso directo de Windows y evita heredar flags del proceso host de Arduino IDE que puedan impedir que Electron muestre su ventana.

El incremento de versión del VSIX también fuerza a Arduino IDE a cargar una extensión nueva en lugar de reutilizar la versión `0.1.2` desde caché.

## Gate de usuario

```text
1. Cerrar completamente JWPLC HMI Designer.
2. Cerrar todas las ventanas de Arduino IDE.
3. git pull --ff-only.
4. Reejecutar Install-JWPLC-HMI-Designer.cmd.
5. Confirmar VSIX jwplc-hmi-launcher-0.1.3.vsix.
6. Abrir nuevamente Arduino IDE.
7. Pulsar JW HMI.
8. Confirmar apertura del Designer.
9. Confirmar que una segunda pulsación enfoca la misma instancia.
10. Confirmar que Escritorio sigue abriendo la misma aplicación y mantiene el icono correcto.
```

## Estado

```text
A11_6_DESKTOP_ICON=PASS
A11_6_WINDOW_ICON=PASS
A11_6_TASKBAR_ICON=PASS
A11_6_APP_BOOT_GUARD=PASS
A11_6_ARDUINO_IDE_LAUNCHER=IMPLEMENTED_0.1.3_PENDING_USER_GATE
```

Si el gate de `0.1.3` pasa, no se vuelve a modificar UI/launcher en Alpha11. El siguiente paso es cierre técnico: regeneración de `JWPLC_Display` precompilado, comparación source/precompiled, benchmark final, smoke físico, README/checklist y PR/PreRelease.
