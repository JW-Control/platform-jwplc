# Alpha11 · A11-7B — Edición de Pixel por capas

Fecha: 2026-09-07

## Objetivo

Mejorar el flujo de dibujo manual de `PIXELMAP` para que el Designer pueda usarse
como editor de capas y como apoyo para construir frames manuales de animación.

## Decisiones de UX

### PIXEL pasa a Componentes

`PIXEL` deja de vivir en la sección `Herramientas` y pasa a `Componentes`, junto
a `TEXT`, `VALUE`, `BOOL` y `BAR`.

La sección `Herramientas` se elimina del lateral izquierdo al retirar:

- Píxel;
- Borrador;
- Texto GFX RAW.

El borrador sigue disponible dentro de `Inspector · PIXEL`. El modo RAW deja de
ser una herramienta visible del flujo normal del Designer.

### Tamaño de pincel y borrador

Pincel y borrador tienen tamaños independientes:

```text
MIN=1x1 px
MAX=16x16 px
SHAPE=SQUARE
PIXEL_PERFECT=YES
```

La huella se aplica sobre una trayectoria rasterizada continua para evitar huecos
al mover rápidamente el puntero.

### Visibilidad y onion skin del Designer

Cada objeto PIXEL incorpora metadatos exclusivos de edición:

```text
editorVisible=true|false
editorOpacity=0.10..1.00
```

El inspector ofrece:

- `Ocultar / Mostrar`;
- botón rápido `Referencia 35% / Opacidad 100%`;
- slider de opacidad de edición 10–100 %.

La intención es permitir flujos como:

1. dibujar un frame en `Pixel 1`;
2. bajarlo a 35 % como referencia;
3. crear `Pixel 2`;
4. dibujar el frame siguiente encima usando el anterior como onion skin;
5. ocultar/mostrar capas según sea necesario.

Estos metadatos no modifican los runs RGB565 ni la visibilidad inicial del
runtime.

### Compositor estable

Durante la validación se detectaron dos fuentes de artefactos cuando un PIXEL
estaba seleccionado y con opacidad reducida:

- el render histórico aplicaba `globalAlpha` sobre el canvas con grilla;
- un render PIXEL adicional podía ejecutarse después del estabilizador y dejar
  por un frame los bordes naranjas/intensidad de selección.

A11-7B corrige el pipeline:

1. `designer-pixelmap-compat.js` conserva el framebuffer base sin PixelMaps;
2. el compositor final restaura ese framebuffer completo;
3. pinta cada PixelMap una sola vez con color de edición atenuado y opaco;
4. restaura los fields por encima;
5. dibuja la geometría de selección fuera del borde del PixelMap, sin iluminar
   los píxeles del arte;
6. el RAF final se registra desde una microtarea para quedar después de todos los
   renders heredados del mismo evento, sin frame intermedio visible.

La selección del objeto no debe cambiar la intensidad del pixelart.

### Duplicado de PIXEL

`Ctrl+D` duplica el objeto PIXEL seleccionado antes de que el manejador global de
fields pueda capturar el atajo.

El botón `Duplicar` del inspector y `Ctrl+D` pasan por la misma operación
`JWPLCHMIPixelMaps.duplicate()`.

## Visibilidad runtime de PixelMaps

La visibilidad del Designer y la visibilidad real en la TFT son conceptos
separados.

El package incorpora API pública compatible con `setPixelMaps()`:

```cpp
bool JWPLC_Display.setPixelMapVisible(size_t index, bool visible);
bool JWPLC_Display.isPixelMapVisible(size_t index) const;
```

Reglas:

- `setPixelMaps()` conserva su firma y registra los mapas como antes;
- todos los PixelMaps quedan visibles al registrarse;
- `setPixelMapVisible()` permite mostrar/ocultar un mapa sin modificar sus runs;
- un cambio de visibilidad invalida el estático para limpiar correctamente el
  frame anterior;
- un índice fuera de rango devuelve `false`;
- RGB565 sigue sin alpha runtime.

El Designer genera IDs simbólicos según el orden de los PixelMaps no vacíos, por
ejemplo:

```cpp
enum HMIPixelMapId : uint8_t
{
    PIXEL_1 = 0,
    PIXEL_FRAME_2 = 1
};
```

Uso esperado desde el sketch:

```cpp
JWPLC_Display.setPixelMapVisible(PIXEL_1, false);
JWPLC_Display.setPixelMapVisible(PIXEL_FRAME_2, true);
```

Esto habilita frames y estados gráficos sin exponer `tft.*` ni cambiar el formato
de `JWPLC_UIPixelMap`.

## Codegen Pixel-only

Se detectó que un proyecto con PixelMaps y `0/32` fields dejaba el panel de
`Código generado` vacío, porque el parche PIXEL dependía del esqueleto producido
por el codegen base de fields.

A11-7B añade una ruta Pixel-only. Si existen PixelMaps no vacíos y no hay fields,
se genera un header completo con:

- `#pragma once`;
- `#include <JWPLC_Display.h>`;
- `HMIPageId`;
- `HMIPixelMapId`;
- arrays `JWPLC_UIPixelRun`;
- `HMI_PIXEL_MAPS`;
- `jwplcHMISetup()` con `setPixelMaps()`;
- `jwplcUIUpdate()` vacío/documentado para control de frames.

La misma enumeración `HMIPixelMapId` también se inserta cuando PixelMaps y fields
conviven.

## Alcance deliberado

```text
EDITOR_VISIBILITY_AFFECTS_CODEGEN=NO
EDITOR_OPACITY_AFFECTS_CODEGEN=NO
RUNTIME_ALPHA=NO
RUNTIME_VISIBILITY_API=YES
SET_PIXEL_MAPS_COMPATIBLE=YES
PIXEL_ONLY_CODEGEN=YES
```

`editorVisible` y `editorOpacity` siguen siendo ayudas de edición. La nueva API
runtime controla visibilidad real por índice de PixelMap y no representa alpha.

## Persistencia

`exportMaps()` incluye `editorVisible` y `editorOpacity` para que estos metadatos
puedan persistirse cuando se cierre el pendiente del formato `.jwhmi` de PixelMaps.

La persistencia canónica de PixelMaps sigue siendo un pendiente separado de
A11-7B.

## Estado

```text
A11_7A_INSPECTOR_EXCLUSIVE=PASS
A11_7B_PIXEL_IN_COMPONENTS=IMPLEMENTED_PENDING_USER_GATE
A11_7B_LEFT_TOOLS_CLEANUP=IMPLEMENTED_PENDING_USER_GATE
A11_7B_BRUSH_SIZE_1_16=IMPLEMENTED_PENDING_USER_GATE
A11_7B_ERASER_SIZE_1_16=IMPLEMENTED_PENDING_USER_GATE
A11_7B_EDITOR_HIDE_SHOW=IMPLEMENTED_PENDING_USER_GATE
A11_7B_EDITOR_ONION_SKIN=IMPLEMENTED_PENDING_USER_GATE
A11_7B_EDITOR_COMPOSITOR_STABLE=IMPLEMENTED_PENDING_USER_GATE
A11_7B_PIXEL_DUPLICATE_BUTTON=IMPLEMENTED_PENDING_USER_GATE
A11_7B_PIXEL_CTRL_D=IMPLEMENTED_PENDING_USER_GATE
A11_7B_RUNTIME_VISIBILITY_API=IMPLEMENTED_PENDING_COMPILE_GATE
A11_7B_PIXEL_ONLY_CODEGEN=IMPLEMENTED_PENDING_USER_GATE
A11_7B_PIXELMAP_SYMBOLIC_IDS=IMPLEMENTED_PENDING_USER_GATE
```

## Gate del usuario

1. Seleccionar un PIXEL, mover el slider 10/25/30/35/100 % y confirmar que el
   trazo mantiene una sola intensidad estable, sin parpadeo ni laterales/bordes
   que queden iluminados.
2. Con `Geometría` activa, confirmar que el marco de selección queda fuera del
   arte y no altera la intensidad de sus píxeles.
3. Probar `Duplicar` y `Ctrl+D`; cada acción debe crear exactamente una copia.
4. Crear un proyecto con sólo PIXEL (`0/32` fields), pulsar `Generar C++` y
   confirmar que aparece un header completo.
5. Confirmar que el header contiene `HMIPixelMapId`, `JWPLC_UIPixelRun`,
   `JWPLC_UIPixelMap` y `JWPLC_Display.setPixelMaps()`.
6. Compilar el header/sketch con el package de la misma rama.
7. Con dos PixelMaps, validar físicamente:

```cpp
JWPLC_Display.setPixelMapVisible(PIXEL_1, false);
JWPLC_Display.setPixelMapVisible(PIXEL_2, true);
```

   y luego invertir los estados. La TFT debe limpiar el frame anterior y mostrar
   únicamente el habilitado.

No marcar A11-7B como PASS hasta completar gate visual, codegen, compilación y
validación runtime de visibilidad.
