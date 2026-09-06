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

```text
UX_1_LAYOUT_BASE=PASS
```

Validado visualmente:

- shell dentro del viewport;
- scroll independiente en laterales;
- panel inferior colapsable;
- Preview 1:1 separado del overlay;
- canvas como zona central dominante;
- borde permanente del display neutro.

## UX-2 — Objetos y selección

```text
UX_2_OBJECT_LIST=PASS_TEXT_BASE
UX_2_SELECTION_OVERLAY=PASS
```

Validado:

- lista de Objetos y canvas representan la misma selección;
- selección naranja sólo en el editor;
- handles, X/Y y dimensiones AUTO;
- región label/value representada sin modificar framebuffer;
- drag actualiza Inspector, overlay y status bar.

UX-4 amplía la lista para permitir varios `TEXT` de la misma página. Esto es infraestructura de edición y no reemplaza el gate A11-3E de composición multi-tipo/páginas.

## UX-3 — Inspector TEXT

```text
UX_3_TEXT_INSPECTOR=PASS
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

Validado:

- `INLINE` / `STACKED`;
- `LEFT` / `CENTER` / `RIGHT`;
- capacidad con recálculo AUTO;
- tamaños label/value;
- frame y colores;
- X/Y;
- cambio contextual a Inspector GFX RAW.

Padding y gap continúan read-only:

```text
FIELD_PADDING=3
FIELD_GAP=4
effectivePadding=max(3,maxTextSize)
```

## UX-4 — Barra superior / edición

Estado actual:

```text
UX_4_EDITING=IMPLEMENTED_PENDING_GATE
UX_4_UNDO_REDO=IMPLEMENTED_PENDING_GATE
UX_4_KEYBOARD_NUDGE=IMPLEMENTED_PENDING_GATE
UX_4_DUPLICATE_DELETE=IMPLEMENTED_PENDING_GATE
UX_4_FIT=PASS_BASE
UX_4_GENERATE_SHORTCUT_TO_CODE_PANEL=PASS_BASE
UX_4_SAVE_OPEN=PENDING_MODEL_JWHMI
```

Gate dedicado:

```text
docs/v2.1.0-alpha.11/A11_UX4_EDITING_GATE.md
```

## UX-5 — Panel técnico

```text
UX_5_BOTTOM_PANEL_COLLAPSIBLE=PASS_BASE
UX_5_PROBLEMS_VALIDATOR=PENDING
UX_5_CODE_PREVIEW=PARTIAL_EXISTING_CONTRACT
UX_5_DIAGNOSTIC_TAB=PENDING
```

## Resultado de Foundation

La revisión visual del 2026-09-05 cerró la arquitectura base:

```text
ALPHA11_UX_FOUNDATION=PASS
```

Antes de iniciar `A11-3B VALUE` sólo falta cerrar el gate funcional de UX-4:

```text
UX_4_EDITING=PASS
NEXT=A11_3B_VALUE_FIELD
```
