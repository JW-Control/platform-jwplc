# Alpha11 — Gate UX-4 Edición

Fecha: 2026-09-05

## Objetivo

Cerrar la base de edición del JWPLC HMI Designer antes de iniciar `A11-3B VALUE`.

UX-4 no cambia la API pública `JWPLC_Display`. Sólo mejora el editor visual y su modelo local.

## Funciones implementadas

```text
UNDO_REDO=YES
KEYBOARD_NUDGE_1PX=YES
KEYBOARD_NUDGE_10PX=YES
DUPLICATE_TEXT=YES
DELETE_TEXT=YES
MULTI_TEXT_SAME_PAGE=YES
HISTORY_MAX=50
```

Atajos:

```text
Ctrl+Z          Deshacer
Ctrl+Y          Rehacer
Ctrl+Shift+Z    Rehacer
← ↑ → ↓         Mover 1 px
Shift+flechas   Mover 10 px
Ctrl+D          Duplicar TEXT
Delete          Eliminar TEXT
Esc             Deseleccionar
```

## Reglas de historial

- un drag completo del field se registra como una sola operación;
- una secuencia de flechas se registra al soltar la tecla;
- cambios del Inspector se consolidan al evento `change`;
- Delete/Duplicate/New/Demo/Clear crean estados de historial;
- el historial incluye campos TEXT, selección, RAW y `pixelLayer`;
- Undo/Redo restauran framebuffer lógico y propiedades visibles.

## Duplicación TEXT

El duplicado:

- conserva apariencia, capacidad, label, unidad, preview y formato;
- desplaza X/Y aproximadamente 8 px;
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

`Delete` o el botón de papelera eliminan el TEXT seleccionado.

Si queda otro TEXT, se selecciona uno vecino. Si no queda ninguno:

```text
TEXT_FIELDS=0
SELECTION=NONE
```

Pulsar después el componente `TEXT` crea un field nuevo.

## Multi-TEXT

UX-4 permite varios `TEXT` simultáneos en la página 0 para validar selección, duplicación e historial.

Esto NO cierra `A11-3E_MULTI_FIELD_PAGES`; todavía faltan:

- páginas múltiples reales;
- objetos VALUE/BOOL/BAR;
- orden/z-index definitivo;
- serialización del modelo `.jwhmi`.

## Gate manual

Ejecutar en navegador con `Ctrl+F5`:

```text
[ ] 1. Mover TEXT con flecha: X cambia exactamente 1 px.
[ ] 2. Shift+flecha: X/Y cambia exactamente 10 px.
[ ] 3. Mantener flecha varios pasos y Ctrl+Z: vuelve al punto previo a la secuencia.
[ ] 4. Arrastrar field y Ctrl+Z: vuelve a la posición previa al drag.
[ ] 5. Rehacer recupera la posición.
[ ] 6. Ctrl+D crea segundo TEXT visible en Objetos y canvas.
[ ] 7. El duplicado tiene ID y variable distintos.
[ ] 8. Seleccionar cada TEXT desde Objetos cambia Inspector y overlay.
[ ] 9. Mover sólo el duplicado no mueve el original.
[ ] 10. Código generado contiene ambos TEXT y ambas variables.
[ ] 11. Delete elimina sólo el seleccionado.
[ ] 12. Ctrl+Z restaura el eliminado.
[ ] 13. Eliminar todos deja el canvas sin TEXT.
[ ] 14. Pulsar componente TEXT vuelve a crear uno.
[ ] 15. Esc elimina la selección/overlay sin borrar el field.
[ ] 16. Preview 1:1 nunca muestra handles ni overlay naranja.
```

## Criterio de cierre

```text
UX_4_EDITING=PASS
ALPHA11_UX_FOUNDATION=PASS
NEXT=A11_3B_VALUE_FIELD
```
