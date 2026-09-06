# Alpha11 · A11-6 Gate de icono de ventana y barra de tareas

Fecha: 2026-09-06

## Objetivo

Cerrar la identidad visual del JWPLC HMI Designer en Windows antes del cierre de Alpha11, verificando que el icono del acceso directo, la ventana de aplicación y la barra de tareas sean coherentes y legibles también a tamaños pequeños.

## Problema reproducido

El icono grande del acceso directo se veía correctamente, pero la barra de tareas mostraba una versión muy borrosa. El motivo es que Windows/Chromium trabaja con tamaños pequeños y puede reescalar agresivamente un recurso demasiado detallado.

## Ajuste Alpha11

Se separa la identidad visual grande del recurso optimizado para ventana/taskbar:

```text
DESKTOP_SHORTCUT_ICON=JWPLC-HMI-Designer.ico
WINDOW_TASKBAR_ICON=jwplc-hmi-icon.svg
TASKBAR_SMALL_SIZE_OPTIMIZED=YES
ICON_CACHE_BUST=alpha11-taskbar-v1
```

El SVG de ventana/taskbar mantiene los rasgos del icono aprobado pero simplifica detalle fino:

```text
DARK_NAVY_BACKGROUND=YES
JWPLC_FRONT_PANEL=YES
TFT_CYAN=YES
DIRECTION_BUTTONS=YES
ESC_RED=YES
OK_GREEN=YES
BUZZER=YES
TEXT_LOGO=NO
SMALL_DETAILS_REDUCED=YES
```

La aplicación fuerza una nueva identidad de carga mediante:

```text
desktop.html?app=alpha11-taskbar-v1
manifest.webmanifest?v=alpha11-taskbar-v1
jwplc-hmi-icon.svg?v=alpha11-taskbar-v1
```

## Commits relacionados

```text
5ee2a08  fix(hmi): optimizar icono de ventana para tamanos pequenos
5635a10  fix(hmi): usar icono vectorial optimizado en barra de tareas
33c1290  fix(hmi): priorizar icono vectorial en manifest de escritorio
80ca4fd  fix(hmi): invalidar cache del icono optimizado de taskbar
```

## Gate de usuario

Después de hacer pull debe reinstalarse la aplicación, porque la copia real vive en `%LOCALAPPDATA%\JWPLC\HMI Designer`.

Secuencia:

```text
1. Cerrar JWPLC HMI Designer.
2. Cerrar Arduino IDE.
3. git pull --ff-only.
4. Reejecutar Install-JWPLC-HMI-Designer.cmd.
5. Instalar también el launcher Arduino IDE cuando se pregunte.
6. Abrir el Designer desde el acceso del Escritorio.
7. Confirmar icono grande del acceso directo.
8. Confirmar icono en barra de título de la app.
9. Confirmar icono nítido en barra de tareas a escala Windows actual.
10. Abrir el Designer desde el botón JW HMI de Arduino IDE.
11. Confirmar que no aparece flash blanco/HTML sin estilos durante el arranque.
12. Confirmar que Guardar, Actualizar HMI y LIVE siguen operativos.
```

## Resultado esperado

```text
A11_6_DESKTOP_ICON=PASS
A11_6_WINDOW_ICON=PASS
A11_6_TASKBAR_ICON=PASS
A11_6_APP_BOOT_GUARD=PASS
A11_6_ARDUINO_IDE_LAUNCHER=PASS_EXPERIMENTAL
```

Hasta la confirmación física/visual del usuario:

```text
A11_6_ICON_TASKBAR_GATE=IMPLEMENTED_PENDING_USER_GATE
```

Una vez aprobado este gate, el siguiente paso es el cierre técnico de Alpha11: regeneración de `JWPLC_Display` precompilado, comparación source/precompiled, benchmark final, README/checklist y documentación de PR/PreRelease.
