# Alpha11 · A11-7C — Workbench de edición PIXEL

Fecha: 2026-09-07

## Motivo

Durante la validación de A11-7B se confirmó que la corrección de transparencia/onion skin funciona correctamente. En la misma sesión se detectaron dos necesidades para que `PIXELMAP` pueda usarse como editor práctico de pixel art:

1. `Ctrl+Z / Ctrl+Y` todavía pertenecían al historial base de fields de `app.js`. Al trabajar sobre un PIXEL, `Ctrl+Z` podía restaurar un snapshot antiguo del field inicial y volver a seleccionar `TEXT` (`TEXT 1 / FIELD_TEXT_1` o el estado equivalente del snapshot), mientras el dibujo PIXEL no se deshacía.
2. Pincel y borrador no mostraban visualmente su huella antes de modificar el mapa, por lo que un tamaño grande podía sobreescribir/borrar más píxeles de los esperados.

A11-7C añade un workbench específico para edición PIXEL sin modificar el contrato C++ ni el runtime RGB565.

## Historial contextual PIXEL

Mientras un objeto PIXEL está seleccionado:

```text
Ctrl+Z         = deshacer último cambio PIXEL
Ctrl+Y         = rehacer último cambio PIXEL
Ctrl+Shift+Z   = rehacer último cambio PIXEL
```

También se reutilizan los botones `Deshacer` y `Rehacer` de la barra superior.

El historial PIXEL registra hasta 80 snapshots y agrupa cambios continuos del inspector mediante un debounce corto. Un trazo de pincel/borrador queda como una sola acción porque `designer-pixelmap.js` emite `jwplc:pixelmap-changed` al finalizar el gesto.

El historial es deliberadamente contextual:

- PIXEL seleccionado → historial PIXEL;
- field seleccionado → historial base existente.

Esto evita que `Ctrl+Z` sobre un dibujo haga saltar la selección hacia el TEXT inicial.

## Herramientas de edición

A las herramientas existentes se añaden:

- `Relleno` — flood fill exacto RGB565 dentro de los bounds actuales del PIXEL;
- `Color` — cuentagotas sobre los PixelMaps visibles de la página;
- `Línea` — línea pixel-perfect usando el tamaño actual del pincel;
- `Rect.` — rectángulo sin relleno usando el tamaño actual del pincel.

Se mantiene:

- Pincel;
- Borrador;
- Mover;
- Duplicar;
- ocultar/mostrar;
- onion skin/opacidad de edición.

### Atajos

```text
B = Pincel
E = Borrador
G = Relleno
I = Cuentagotas
L = Línea
R = Rectángulo
V = Mover
Alt + clic = tomar color temporalmente
```

## Cursor de huella

Se añade `pixelToolCursorCanvas`, un overlay independiente del framebuffer y de `geometryCanvas`.

El cursor muestra antes de editar:

- huella exacta del pincel;
- huella exacta del borrador;
- huella del pincel para Línea/Rectángulo;
- celda objetivo para Relleno/Cuentagotas.

El overlay tiene `pointer-events:none`, no modifica el framebuffer, no cambia el PixelMap almacenado y no aparece en el codegen.

## Relleno

El flood fill usa conectividad 4-neighbor y comparación exacta RGB565.

Para evitar un relleno accidental de toda la TFT, la expansión se limita a los `bounds` actuales del objeto PIXEL seleccionado. Esto permite colorear regiones cerradas del dibujo sin convertir automáticamente los 320×170 píxeles transparentes del framebuffer en datos del PixelMap.

## Archivos

- `poc/designer-pixelmap-workbench.js` — historial, herramientas y cursor de huella;
- `poc/desktop.html` — carga del workbench en la aplicación de escritorio;
- `poc/service-worker.js` — cache actualizado.

## Estado

```text
A11_7B_EDITOR_TRANSPARENCY_USER_FEEDBACK=PASS
A11_7C_PIXEL_HISTORY=IMPLEMENTED_PENDING_USER_GATE
A11_7C_PIXEL_UNDO_REDO_SHORTCUTS=IMPLEMENTED_PENDING_USER_GATE
A11_7C_PIXEL_UNDO_REDO_TOOLBAR=IMPLEMENTED_PENDING_USER_GATE
A11_7C_FILL_TOOL=IMPLEMENTED_PENDING_USER_GATE
A11_7C_EYEDROPPER=IMPLEMENTED_PENDING_USER_GATE
A11_7C_LINE_TOOL=IMPLEMENTED_PENDING_USER_GATE
A11_7C_RECT_TOOL=IMPLEMENTED_PENDING_USER_GATE
A11_7C_BRUSH_FOOTPRINT_CURSOR=IMPLEMENTED_PENDING_USER_GATE
A11_7C_ERASER_FOOTPRINT_CURSOR=IMPLEMENTED_PENDING_USER_GATE
A11_7C_CODEGEN_CHANGE=NO
A11_7C_RUNTIME_DISPLAY_API_CHANGE=NO
```

## Gate del usuario

1. Seleccionar un PIXEL, dibujar tres trazos diferentes y ejecutar `Ctrl+Z` tres veces. Cada pulsación debe eliminar un trazo sin seleccionar un TEXT.
2. Ejecutar `Ctrl+Y` tres veces y confirmar que los trazos reaparecen en orden.
3. Repetir usando los botones `Deshacer / Rehacer`.
4. Cambiar pincel a `1×1`, `4×4` y `16×16`; confirmar que el marco previo coincide con la huella real.
5. Cambiar borrador a tamaños distintos del pincel; confirmar que su marco usa el tamaño del borrador.
6. Dibujar una región cerrada y probar `Relleno` con dos colores RGB565.
7. Usar `Color` y `Alt+clic` sobre un píxel existente; confirmar que el color activo cambia al RGB565 muestreado.
8. Probar `Línea` y `Rect.` con pincel 1×1 y luego con un tamaño mayor.
9. Confirmar que ninguna herramienta altera la opacidad/onion skin ya validada.
10. Generar C++ y confirmar que las nuevas herramientas de edición no cambian el contrato PixelMap/runtime.

No marcar A11-7C como PASS hasta recibir validación del usuario.
