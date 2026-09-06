# Alpha11 · A11-6 Gate de icono de ventana y barra de tareas

Fecha: 2026-09-06

## Objetivo

Cerrar la identidad visual del JWPLC HMI Designer en Windows antes del cierre de Alpha11, verificando que el icono del acceso directo, la ventana de aplicación y la barra de tareas sean coherentes y legibles también a tamaños pequeños.

## Problema reproducido

El icono grande del acceso directo se veía correctamente, pero Windows/Chromium reducía el mismo arte detallado a 16–32 px y la barra de tareas mostraba una mancha borrosa. La variante SVG simplificada tampoco dio un resultado suficientemente nítido en el `--app` de Chromium.

## Ajuste final Alpha11

Se separan deliberadamente dos niveles de arte:

```text
DESKTOP_SHORTCUT_ICON=JWPLC-HMI-Designer.ico
EXE_ICON=JWPLC-HMI-Designer.ico
WINDOW_TASKBAR_ICON_FAMILY=PNG_RASTER_PIXEL_ALIGNED
ICON_SIZES=16,20,24,32,40,48,64
ICON_CACHE_BUST=alpha11-taskbar-raster-v3
```

Los PNG pequeños no son una reducción automática del arte de 256 px. Se dibujan sobre la rejilla final para conservar sólo los rasgos reconocibles:

```text
DARK_NAVY_TILE=YES
JWPLC_FRONT_PANEL=YES
TFT_CYAN=YES
DIRECTION_PAD=SIMPLIFIED
ESC_RED=YES
OK_GREEN=YES
TEXT_LOGO=NO
FINE_SCREEN_DETAIL=REMOVED
PIXEL_ALIGNED=YES
```

El `.ico` detallado aprobado se mantiene para el EXE, Escritorio y Menú Inicio.

## Carga y caché

La aplicación usa una identidad nueva:

```text
desktop.html?app=alpha11-taskbar-raster-v3
manifest.webmanifest?v=alpha11-taskbar-raster-v3
icons/icon-16.png ... icons/icon-64.png
```

`desktop.html` declara tamaños concretos de favicon antes y después de inyectar el documento final. No se usa SVG para el icono pequeño.

## Gate de usuario

```text
1. Cerrar completamente JWPLC HMI Designer.
2. Cerrar Arduino IDE.
3. git pull --ff-only.
4. Reejecutar Install-JWPLC-HMI-Designer.cmd.
5. Abrir Designer desde Escritorio.
6. Confirmar que el icono grande sigue correcto.
7. Confirmar que la barra de título muestra un icono legible.
8. Confirmar que la barra de tareas muestra el símbolo JWPLC nítido.
9. Confirmar ausencia del flash blanco/HTML sin estilos.
10. Abrir desde JW HMI en Arduino IDE.
11. Confirmar Guardar, Actualizar HMI y LIVE.
```

## Resultado esperado

```text
A11_6_DESKTOP_ICON=PASS
A11_6_WINDOW_ICON=PASS
A11_6_TASKBAR_ICON=PASS
A11_6_APP_BOOT_GUARD=PASS
A11_6_ARDUINO_IDE_LAUNCHER=PASS_EXPERIMENTAL
```

Hasta confirmación visual:

```text
A11_6_ICON_TASKBAR_GATE=IMPLEMENTED_PENDING_USER_GATE
```

Una vez aprobado, no se vuelve a modificar UI/launcher en Alpha11. El siguiente paso es el cierre técnico: regeneración de `JWPLC_Display` precompilado, comparación source/precompiled, benchmark final, smoke físico, README/checklist y PR/PreRelease.
