# Alpha11 — Gate UX-4 Edición

Fecha: 2026-09-05

## Objetivo

Cerrar la base de edición del JWPLC HMI Designer antes de iniciar `A11-3B VALUE`.

UX-4 no cambia la API pública `JWPLC_Display`. Sólo mejora el editor visual y su modelo local.

## Resultado

La validación visual/funcional fue aceptada y se autoriza continuar con `VALUE`.

```text
UX_4_EDITING=PASS
ALPHA11_UX_FOUNDATION=PASS
NEXT=A11_3B_VALUE_FIELD
```

Se confirmó especialmente la regla de interacción del canvas:

```text
CLICK_EMPTY_CANVAS=DESELECT
CLICK_EMPTY_CANVAS_REPOSITIONS_FIELD=NO
DRAG_STARTS_ONLY_ON_FIELD=YES
```

Esto evita que un clic accidental en una zona vacía cambie X/Y del objeto seleccionado.

## Funciones implementadas

```text
UNDO_REDO=YES
KEYBOARD_NUDGE_1PX=YES
KEYBOARD_NUDGE_10PX=YES
DUPLICATE_DELETE=YES
MULTI_FIELD_INFRASTRUCTURE=YES
HISTORY_MAX=50
```

Atajos:

```text
Ctrl+Z          Deshacer
Ctrl+Y          Rehacer
Ctrl+Shift+Z    Rehacer
← ↑ → ↓         Mover 1 px
Shift+flechas   Mover 10 px
Ctrl+D          Duplicar objeto
Delete          Eliminar objeto
Esc             Deseleccionar
```

## Reglas de historial

- un drag completo del field se registra como una sola operación;
- una secuencia de flechas se registra al soltar la tecla;
- cambios del Inspector se consolidan al evento `change`;
- Delete/Duplicate/New/Demo/Clear crean estados de historial;
- el historial incluye fields HMI, selección, RAW y `pixelLayer`;
- Undo/Redo restauran framebuffer lógico y propiedades visibles.

## Duplicación

El duplicado:

- conserva apariencia, contenido/formato y geometría;
- desplaza X/Y aproximadamente 8 px cuando existe espacio;
- genera `key` interno nuevo;
- genera ID simbólico único;
- genera variable HMI única;
- queda seleccionado inmediatamente.

Ejemplo conceptual:

```text
FIELD_STATUS      -> FIELD_STATUS_COPY
estadoTexto       -> estadoTextoCopy
```

Si ya existen, el Designer añade sufijos para evitar colisiones.

## Eliminación

`Delete` o el botón de papelera eliminan sólo el objeto seleccionado.

Si queda otro field, se selecciona uno vecino. Si no queda ninguno:

```text
FIELDS=0
SELECTION=NONE
```

Pulsar un componente habilitado crea un field nuevo del tipo correspondiente.

## Alcance de multi-field

UX-4 introdujo la infraestructura para varios objetos en la página 0 con selección, duplicación, hit-test e historial.

Esto NO cierra `A11-3E_MULTI_FIELD_PAGES`; todavía faltan:

- páginas múltiples reales;
- BOOL/BAR;
- orden/z-index definitivo;
- serialización del modelo `.jwhmi`.

## Gate manual validado

Se validó el comportamiento general de:

```text
[PASS] movimiento por teclado;
[PASS] Undo/Redo;
[PASS] drag sobre field;
[PASS] no mover por click vacío;
[PASS] duplicación/eliminación;
[PASS] selección desde Objetos;
[PASS] Preview 1:1 sin overlay naranja.
```

La base UX queda congelada para incorporar `VALUE`, `BOOL` y `BAR` sobre la misma interacción.
