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

### Visibilidad y onion skin

Cada objeto PIXEL incorpora metadatos exclusivos del Designer:

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

### Estabilidad visual de la referencia

Durante la validación se detectó que `globalAlpha` mezclaba la referencia con la
grilla ya dibujada en el canvas. A porcentajes bajos, un mismo color RGB565 podía
verse con dos intensidades aunque el objeto no estuviera seleccionado.

Se añade `designer-pixelmap-stability.js` para normalizar la vista final de los
PixelMaps de referencia:

- la atenuación se calcula como color de edición sobre el fondo lógico negro;
- cada RGB565 conserva una única intensidad visual por porcentaje;
- no se modifica el color almacenado;
- no se modifica el C++ generado;
- los fields se restauran encima para mantener el orden `PixelMaps -> fields`.

Esto es una ayuda visual del Designer y no introduce alpha en el runtime.

### Duplicado de PIXEL

`Ctrl+D` debe duplicar el objeto PIXEL seleccionado antes de que el manejador
global de fields pueda capturar el atajo.

El botón `Duplicar` del inspector y `Ctrl+D` pasan por la misma operación
`JWPLCHMIPixelMaps.duplicate()`.

## Alcance deliberado

La visibilidad y opacidad son sólo ayudas de edición.

RGB565 no tiene canal alpha y A11-7B no introduce una semántica nueva en
`JWPLC_Display`.

Por tanto:

```text
EDITOR_VISIBILITY_AFFECTS_CODEGEN=NO
EDITOR_OPACITY_AFFECTS_CODEGEN=NO
RUNTIME_ALPHA=NO
JWPLC_DISPLAY_API_CHANGE=NO
```

`buildCode()` sigue generando todos los PixelMaps no vacíos como RGB565 opacos.
Ocultar una capa en el Designer no la elimina del contrato C++.

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
A11_7B_EDITOR_REFERENCE_SINGLE_TONE=IMPLEMENTED_PENDING_USER_GATE
A11_7B_PIXEL_DUPLICATE_BUTTON=IMPLEMENTED_PENDING_USER_GATE
A11_7B_PIXEL_CTRL_D=IMPLEMENTED_PENDING_USER_GATE
A11_7B_CODEGEN_UNCHANGED_BY_EDITOR_VISIBILITY=IMPLEMENTED_PENDING_USER_GATE
```

## Gate del usuario

1. Confirmar que `PIXEL` aparece en `Componentes` y ya no existe la sección
   `Herramientas`.
2. Crear un PIXEL y probar pincel 1x1, 4x4 y 16x16.
3. Probar borrador con tamaños distintos al pincel.
4. Dibujar dos PixelMaps superpuestos.
5. Poner uno a 35 % y confirmar que, al deseleccionarlo, todo el trazo conserva
   una única intensidad visual sin dos tonos derivados de la grilla.
6. Ocultar/mostrar cualquiera de los dos y confirmar independencia entre capas.
7. Seleccionar un PIXEL y probar el botón `Duplicar`.
8. Seleccionar un PIXEL y probar `Ctrl+D`; debe crear exactamente una copia.
9. Confirmar que el indicador de Objetos muestra estado/porcentaje sin convertirlo
   en un control de visibilidad lateral.
10. Generar C++ y confirmar que opacidad/ocultamiento de edición no aparecen en el
    código ni cambian `JWPLC_Display.setPixelMaps()`.

No marcar A11-7B como PASS hasta recibir validación visual y funcional del usuario.
