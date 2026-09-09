# Alpha11 · A11-7 — Objeto PIXEL y selector RGB565

Fecha: 2026-09-06

## Objetivo

Alinear el pincel del Designer con el contrato público `JWPLC_UIPixelMap` ya
incorporado en `JWPLC_Display`.

El comportamiento anterior usaba un framebuffer global de píxeles manuales.
Eso servía para el PoC inicial, pero no modelaba una HMI por capas.

## Decisión

```text
PIXEL_UI_NAME=Pixel
PIXEL_INTERNAL_TYPE=PIXELMAP
PIXEL_IS_OBJECT=YES
PIXEL_PER_PIXEL_LAYER=NO
PIXELMAP_MULTI_COLOR=YES
PIXELMAP_MAX_OBJECTS=16
PIXELMAP_STATIC_PAGE_OBJECT=YES
```

Al pulsar **Pixel** se crea un nuevo objeto en la lista Objetos. El dibujo sólo
modifica el PIXEL actualmente seleccionado.

Cada objeto contiene su propio conjunto de píxeles RGB565 y se manipula como
una única capa:

- Pincel;
- Borrador;
- Mover;
- X/Y de la capa;
- duplicar;
- eliminar;
- selección desde la lista Objetos;
- selección desde el canvas cuando el píxel no está cubierto por un field.

Los fields declarativos conservan prioridad visual sobre los PixelMaps, igual
que el runtime: los PixelMaps se consideran fondo estático de página.

## Selector RGB565

El selector deja de limitarse a una paleta cerrada.

```text
RGB565_RANGE=0x0000..0xFFFF
RGB565_VALUES=65536
VISUAL_COLOR_PICKER=YES
HEX_RGB565_EDIT=YES
PRESETS=SHORTCUTS_ONLY
```

El usuario puede:

1. elegir visualmente un color RGB888;
2. el Designer lo cuantiza al RGB565 representable más cercano;
3. ver y editar directamente el valor `0x0000..0xFFFF`;
4. usar presets BLACK/WHITE/RED/GREEN/BLUE/CYAN/MAGENTA/YELLOW/ORANGE.

El mismo control RGB565 se reutiliza en **Apariencia** para:

- color de etiqueta;
- color de valor;
- fondo;
- borde.

Por tanto, el Designer puede generar cualquier literal RGB565 de 16 bits, no
sólo los nombres/preajustes de la paleta.

## Codegen

El objeto PIXEL no genera llamadas directas a `tft()`.

El Designer comprime píxeles horizontales consecutivos del mismo color en
`JWPLC_UIPixelRun` y genera:

```cpp
static const JWPLC_UIPixelRun ...[] = { ... };
static const JWPLC_UIPixelMap HMI_PIXEL_MAPS[] = { ... };

JWPLC_Display.setPixelMaps(
    HMI_PIXEL_MAPS,
    sizeof(HMI_PIXEL_MAPS) / sizeof(HMI_PIXEL_MAPS[0]));
```

Esto mantiene el contrato:

```text
DIRECT_TFT_CODEGEN=NO
PUBLIC_JWPLC_DISPLAY_API=YES
PIXELMAP_RUN_COMPRESSION=YES
FIELDS_OVER_PIXELMAPS=YES
```

## Alcance de este gate

Implementado en el frontend:

```text
A11_7_PIXEL_OBJECT_MODEL=IMPLEMENTED_PENDING_USER_GATE
A11_7_PIXEL_LAYER_SELECTION=IMPLEMENTED_PENDING_USER_GATE
A11_7_PIXEL_DRAW_ERASE_MOVE=IMPLEMENTED_PENDING_USER_GATE
A11_7_PIXEL_MULTI_COLOR=IMPLEMENTED_PENDING_USER_GATE
A11_7_RGB565_PICKER=IMPLEMENTED_PENDING_USER_GATE
A11_7_RGB565_APPEARANCE=IMPLEMENTED_PENDING_USER_GATE
A11_7_PIXELMAP_CODEGEN=IMPLEMENTED_PENDING_USER_GATE
A11_7_FIELDS_OVER_PIXELMAPS=IMPLEMENTED_PENDING_USER_GATE
```

Pendiente deliberado antes de cerrar A11-7:

```text
A11_7_JWHMI_PIXELMAP_PERSISTENCE=PENDING
```

Primero se valida la UX del objeto/capa y del selector de color. Después se
incorpora el bloque `pixelMaps` al formato `.jwhmi` canónico para no congelar
un esquema de proyecto antes de validar la interacción.

## Gate del usuario

1. Crear `Pixel 1` con el botón Pixel.
2. Dibujar varios píxeles y comprobar que sigue existiendo un solo objeto.
3. Cambiar RGB565 y dibujar otro color dentro del mismo Pixel.
4. Crear `Pixel 2` y comprobar independencia entre capas.
5. Seleccionar `Pixel 1` desde Objetos y volver a editarlo.
6. Usar Mover y comprobar que se desplaza el conjunto completo.
7. Usar Borrador y comprobar que sólo borra del Pixel seleccionado.
8. Seleccionar TEXT/VALUE/BOOL/BAR y comprobar que Pixel deja de estar activo.
9. Probar un color no incluido en presets, por ejemplo `0x39E7`.
10. Cambiar un color de Apariencia con el selector visual/hex y generar C++.
11. Confirmar que el código PIXEL usa `JWPLC_Display.setPixelMaps()` y no `tft.*`.
12. Superponer un PIXEL con un field y comprobar que el field queda visualmente encima.

No marcar PASS hasta recibir evidencia del navegador/Designer del usuario.
