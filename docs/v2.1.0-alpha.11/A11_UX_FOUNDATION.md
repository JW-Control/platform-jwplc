# Alpha11 — UX Foundation del JWPLC HMI Designer

Fecha: 2026-09-05

## Objetivo

Congelar la arquitectura visual del editor antes de incorporar `VALUE`, `BOOL`, `BAR` y múltiples páginas.

La base sigue siendo:

```text
DESIGNER_FRONTEND_OF_EXISTING_API=YES
SECOND_HMI_RUNTIME=NO
TARGET=ST7789_320x170_ROT3_RGB565
DESIGNER_GENERATES_JWPLC_UI_UPDATE=NO
USER_WRITES_JWPLC_UI_UPDATE=YES
```

## Arquitectura adoptada

```text
┌───────────────────────────────────────────────────────────────┐
│ Barra superior / comandos                                    │
├──────────────┬───────────────────────────────┬────────────────┤
│ Páginas      │                               │ Preview 1:1    │
│ Objetos      │       Canvas 320×170          │ Inspector      │
│ Componentes  │                               │ contextual     │
│ Herramientas │                               │                │
├──────────────┴───────────────────────────────┴────────────────┤
│ Estado/Problemas | Código generado | Diagnóstico             │
├───────────────────────────────────────────────────────────────┤
│ Status bar                                                    │
└───────────────────────────────────────────────────────────────┘
```

## Reglas UX congeladas

```text
LEFT_PANEL=PAGES_OBJECTS_COMPONENTS_TOOLS
CENTER_PANEL=PIXEL_PERFECT_CANVAS
RIGHT_PANEL=PREVIEW_PLUS_CONTEXTUAL_INSPECTOR
BOTTOM_PANEL=TECHNICAL_OUTPUT_COLLAPSIBLE
ORANGE=ACTIVE_SELECTION_OR_ACTION
CANVAS_BORDER=NEUTRAL
```

El panel izquierdo no contiene propiedades del field. El Inspector derecho es el único editor contextual de propiedades.

## UX-1 — Layout base

Estado:

```text
UX_1_LAYOUT_BASE=IMPLEMENTED_PENDING_VISUAL_GATE
```

Incluye:

- shell fijo al viewport;
- scroll independiente en laterales;
- panel inferior colapsable;
- Preview 1:1 siempre visible;
- canvas como zona central dominante;
- borde permanente del display neutro.

## UX-2 — Objetos y selección

Estado:

```text
UX_2_OBJECT_LIST=IMPLEMENTED_TEXT_ONLY_PENDING_VISUAL_GATE
UX_2_SELECTION_OVERLAY=IMPLEMENTED_PENDING_VISUAL_GATE
```

Para A11-3A existe un único objeto `TEXT` y se sincroniza con el objeto seleccionado.

El overlay de geometría no forma parte del framebuffer ni del Preview 1:1. Puede mostrar:

- `field rect`;
- handles;
- X/Y;
- ancho/alto AUTO resuelto;
- región label;
- región value;
- región unit.

## UX-3 — Inspector TEXT

Estado:

```text
UX_3_TEXT_INSPECTOR=IMPLEMENTED_PENDING_VISUAL_GATE
```

Secciones:

```text
Identidad
Dato HMI
Geometría
Tipografía
Apariencia
Contrato C++
```

Padding y gap continúan read-only:

```text
FIELD_PADDING=3
FIELD_GAP=4
effectivePadding=max(3,maxTextSize)
```

Las dimensiones AUTO se presentan como valores calculados y no como propiedades libres.

## UX-4 — Barra superior / teclado

Estado:

```text
UX_4_TOOLBAR_SHELL=PARTIAL
UX_4_FIT=IMPLEMENTED
UX_4_GENERATE_SHORTCUT_TO_CODE_PANEL=IMPLEMENTED
UX_4_UNDO_REDO=PENDING
UX_4_KEYBOARD_NUDGE=PENDING
UX_4_SAVE_OPEN=PENDING_MODEL_JWHMI
```

Los controles que todavía no tienen contrato real quedan deshabilitados; la interfaz no debe simular funcionalidad inexistente.

## UX-5 — Panel técnico

Estado:

```text
UX_5_BOTTOM_PANEL_COLLAPSIBLE=IMPLEMENTED
UX_5_PROBLEMS_VALIDATOR=PENDING
UX_5_CODE_PREVIEW=PARTIAL_EXISTING_CONTRACT
UX_5_DIAGNOSTIC_TAB=PENDING
```

## Criterio de salida

Antes de iniciar `A11-3B VALUE` deben quedar visualmente validados al menos:

```text
[ ] Layout completo cabe en viewport de referencia.
[ ] Lista de objetos y canvas representan la misma selección.
[ ] Inspector TEXT conserva toda la funcionalidad A11-3A.
[ ] Overlay Geometría no altera framebuffer ni Preview 1:1.
[ ] Borde del canvas es neutro y naranja queda reservado a selección.
[ ] Panel inferior puede contraerse sin desplazar el workspace.
[ ] Geometría mostrada coincide con JWPLC_UITextField/runtime.
```

Cuando estos puntos pasen:

```text
ALPHA11_UX_FOUNDATION=PASS
NEXT=A11_3B_VALUE_FIELD
```
