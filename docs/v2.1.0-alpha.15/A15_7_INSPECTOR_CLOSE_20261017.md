# Alpha15 · A15-7A..7D — Cierre de BAR e inspector nuevo de TEXT / VALUE / BOOL

Fecha: `2026-10-17`

```text
A15_7A_BAR_FINISH=PASS (BAR-1..BAR-7; paridad lienzo/firmware verificada en 16 casos)
A15_7B_VALUE=PASS
A15_7C_TEXT=PASS
A15_7D_BOOL=PASS
PHYSICAL=[no verificado] (foto TFT vs lienzo -> A15-9)
TEST=tools/jwplc-hmi-designer/tests/inspector-smoke.cjs (Chromium headless, sin g++)
```

Especificación de partida: `A15_7_INSPECTOR_REDESIGN_SPEC_20261016.md`.

## TEXT / VALUE / BOOL `[ejecutado]`

Módulo nuevo `poc/designer-field-inspector.js` (se carga después de `designer-bar.js`).

```text
Inspector — <tipo>
[ Diseño ] [ Datos ] [ Técnico ]
  Diseño: [ Forma ] [ Contenido ] [ Apariencia ] [ Ubicación ]
```

| Pestaña | VALUE | TEXT | BOOL |
|---|---|---|---|
| Forma | En línea / Apilado (tarjetas), tamaño valor y etiqueta 1×-4× (chips con alto en px), marco (tarjetas) | igual | igual |
| Contenido | etiqueta, unidad, enteros (stepper), decimales (chips), `±` y `007` (toggles), reserva → resultado, alineación (tarjetas) | etiqueta, capacidad (stepper) con regla `MMMM` y ancho en px, alineación | etiqueta, unidad, estados FALSE → TRUE, presets (OFF/ON, PARADO/MARCHA, CERRADO/ABIERTO, NO/SI, FALLA/OK), alineación |
| Apariencia | paleta RGB565 + selector libre para etiqueta, valor, fondo y marco (sólo con marco) | igual | igual + "Color por estado" deshabilitado (requiere firmware) |
| Ubicación | página (pestañas mini), X/Y, tamaño calculado | igual | igual |
| Datos | variable C++, tipo `float`, valor de prueba + slider acotado a la reserva | variable, tipo `char[N+1]`, valor de prueba con contador `n/N` | variable, tipo `bool`, estado de prueba como interruptor |
| Técnico | nombre, ID, contrato multilínea copiable, diagnóstico de geometría | igual | igual |

Mecanismo: cada control escribe en el input viejo equivalente (`#fieldLayout`, `#fieldLabelColor`…) y
dispara `input`/`change`. No se duplica lógica: historial, validación de IDs, render y codegen son los de
`app.js`. Los `<details>` viejos se ocultan con `#textFieldControlsSection.insp-v2 > details`.

Unidad en BOOL: se conserva. `JWPLC_UI.cpp` dibuja `def.text.unit` para todos los tipos `[visto]`.

### Corrección de codegen

Un campo con fondo `TRANSPARENT` (proyectos viejos o el select anterior) generaba
`JWPLC_UIColors(..., 0xTRANSPARENT, ...)`, que no compila `[ejecutado]`. El firmware no tiene fondo
transparente para campos: el refresco parcial borra el valor anterior pintando el fondo. Ahora:

- `hex565()` de `app.js` / `designer-bool.js` emite `0x0000` para valores no numéricos;
- el inspector normaliza el fondo a `0x0000` al seleccionar el campo y no ofrece "transparente".

## BAR `[ejecutado]`

Lectura de `JWPLC_UI.cpp` (`computeFieldGeometry`, `drawFieldStatic`, `drawFieldValue`) `[visto]`:

```text
alto de la barra    DEFAULT_BAR_HEIGHT = 12 px (rect.height se ignora)
ancho AUTO          barra de 80 px
etiqueta            (x+pad, y+pad) en ambos layouts
INLINE              barra a la derecha de la etiqueta, MISMA y (y+pad)
STACKED             barra debajo de la etiqueta (+ GAP)
unidad              (valueX + valueW + GAP, valueY)
pista vacía         se borra con colors.background (un solo color para fondo y pista)
```

El lienzo no respetaba varias de estas reglas. Con un BAR INLINE, etiqueta "Nivel", unidad "%" y
ancho 140, antes del cambio:

| | lienzo | TFT |
|---|---|---|
| alto del campo | 29 | 18 |
| X de la barra | 13 | 46 |
| Y de la barra | 34 | 23 |
| ancho de la barra | 125 | 92 |

La causa: la matriz 3×3 cambiaba `alignV`/`alignH` (sólo lienzo) pero no `layout` (lo que llega al
firmware), y en INLINE el lienzo centraba la etiqueta y la unidad.

| # | Acción |
|---|---|
| BAR-1 | Slider de simulación oculto en Forma; vive sólo en Datos |
| BAR-2 | "Modo de ancho" oculto (ancho siempre fijo), "Tipo de relleno" y "% de carga" pasan a tarjetas deshabilitadas, "Tamaño etiqueta" a chips, "Página" a pestañas mini |
| BAR-3 | Deshabilitados con "Requiere firmware": Vertical, Diagonales/Bloques/Puntos, Leve/Píldora, borde de barra, Degradado, Circular, celdas 3×3 salvo ↖ (STACKED) y ← (INLINE), "% de carga". `applyFirmwareCaps()` normaliza el modelo para que el lienzo no muestre lo que la TFT no dibuja |
| BAR-4 | Grosor fijo 12 px (control deshabilitado, "fijo en firmware") |
| BAR-5 | Pestaña "Datos"; tipo `float` de sólo lectura |
| BAR-6 | Contrato multilínea: declaración + `JWPLC_Display.setBar(ID, var)` que emite `jwplcUIUpdate()` |
| BAR-7 | Checklist abajo |

Otros:

- Fondo y pista: un único control ("Fondo y pista vacía"). El codegen ya emitía `trackColor` como
  `colors.background`, así que la salida de la TFT no cambia.
- Proyectos con ancho AUTO: se convierte al ancho total equivalente (barra de 80 px) para que la
  TFT dibuje lo mismo.

Paridad: `inspector-smoke.cjs` traduce `computeFieldGeometry()` a JS. Lo compara con
`JWPLCHMIBar.computeGeometry()` en 16 casos:

```text
INLINE|STACKED × con/sin etiqueta × con/sin unidad × etiqueta 1×/2×
```

Resultados `[ejecutado]`:

```text
designer-bar.js anterior   FAIL  (fieldH, valueX, valueY, valueW y unidad distintos)
designer-bar.js nuevo      PASS  (16/16)
```

### BAR-7 · checklist de la Propuesta 2 (`…Especificacion_UI.md` §8)

| Criterio | Estado |
|---|---|
| Pestañas Diseño / Datos / Técnico | PASS (renombrada "Datos PLC" → "Datos") |
| Diseño con Forma / Contenido / Apariencia / Ubicación | PASS |
| Tarjetas visuales claras y clicables | PASS (las no soportadas, visibles y deshabilitadas) |
| Barra muestra sólo controles de Barra; Circular sólo de Circular | PASS para Barra; Circular deshabilitado (requiere firmware) |
| Cambio de variante visible en el lienzo sin incoherencias | PASS: el lienzo ya no muestra variantes que la TFT no dibuja |
| Simulación sólo en Datos | PASS (BAR-1) |
| Slider y valor sincronizados | PASS (sin cambios) |
| La simulación no modifica PLC, firmware ni sketch | PASS (sólo `field.barValue`, no se emite) |
| Rango/escalado auditado | PASS: `JWPLC_UIRange(min, max)` se emite; el firmware satura a 0..1 |
| Técnico: nombre, ID, contrato copiable, diagnóstico | PASS (contrato completo, BAR-6) |
| Contrato `.h` / `.ino` verificado, sin dobles declaraciones | PASS (`codegen-smoke.cjs` compila el header) |
| Sin parámetros duplicados contradictorios | PASS: alto, ancho y fondo/pista tienen un solo control |
| Colores y estados según la Propuesta 2 | PASS |
| 1080p sin scroll horizontal | PASS en 1600×1400 headless; 4K/DPI Windows `[no verificado]` |
| Lienzo utilizable | PASS (sin cambios de layout del panel) |
| Undo/redo, duplicar/eliminar, guardar/abrir, generar C++ | PASS (`inspector-smoke` + `codegen-smoke`) |
| Proyectos BAR anteriores cargan con salida equivalente | PASS: el header no cambia salvo AUTO → ancho equivalente; las variantes no soportadas ya no se emitían |
| Sin regresiones TEXT/VALUE/BOOL | PASS (inspector nuevo, mismo modelo) |
| Capturas de pestañas | `inspector-smoke.cjs` las guarda en su carpeta de salida |

## Cómo probar

```bash
node tools/jwplc-hmi-designer/tests/inspector-smoke.cjs [carpeta]   # 25+ verificaciones + capturas
node tools/jwplc-hmi-designer/tests/codegen-smoke.cjs               # headers compilan contra el package
```

A mano: abrir `poc/desktop.html` y crear VALUE, TEXT, BOOL y BAR desde la barra de herramientas.

## Pendiente

```text
OPENPLC_DATA_TAB=selector de variable PLC en "Datos" -> A15-6
BOOL_COLOR_BY_STATE=petición a Alpha13 (JWPLC_UIColors tiene un único color de valor)
BAR_TRACK_COLOR=petición a Alpha13 (color de pista distinto del fondo)
BAR_VARIANTS=vertical, alto, radio, estilos, borde, degradado, circular, % de carga -> Alpha13
PHYSICAL_PARITY=foto de la TFT vs lienzo para cada tipo -> A15-9
```
