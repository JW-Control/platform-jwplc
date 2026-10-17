# Alpha15 · A15-7 seguimiento (2.ª ronda) — Anclaje 4×, BOOL texto+fondo, Apariencia con mini-modelos

Fecha: `2026-10-17` · segunda revisión del jefe sobre el Designer en su PC.

```text
TEXT_X4_ANCHOR_FIX=PASS_SOURCE (firmware JWPLC_UI.cpp + lienzo + BAR; .a al cierre)
BOOL_STATE_TEXT_AND_BACKGROUND=PASS_SOURCE (constructor de 6 argumentos)
FIELD_APPEARANCE_MINI_MODELS=PASS (TEXT/VALUE/BOOL con el patrón del MENU)
COLOR_PICKER_THROTTLE=PASS (40 ms, global)
NEW_OBJECTS_ON_TOP=PASS
GRID_STEP_1_2_PX=PASS
RULER_Y_ALIGNMENT=PASS (ya incluido en la 1.ª ronda)
PHYSICAL=[no verificado]
```

## 1. Etiqueta y valor ya no se mueven en 4×

Antes, el padding era `max(3, tamaño_etiqueta, tamaño_valor)` en los cuatro lados. En 4× pasaba a 4 px,
y etiqueta y valor se corrían 1 px a la derecha y 1 px abajo.

Ahora `JWPLC_UI.cpp:computeFieldGeometry` separa los dos márgenes `[visto]`:

```text
lead = FIELD_PADDING (3 px fijo)         -> arriba / izquierda
pad  = effectiveFieldPadding() (3 o 4)   -> derecha / abajo (fila de descendentes en 4×)
ancho/alto AUTO = lead + pad + contenido
valueX/valueY y etiqueta = rect.x/y + lead
```

- La fila de descendentes de 4× sigue dentro del marco.
- El campo crece 1 px sólo hacia la derecha y abajo.
- BAR usa la misma regla.

Designer: `app.js:computeFieldGeometry`/`drawField` y `designer-bar.js` aplican la misma regla. El
ancho mínimo del valor de BAR pasó de 4 a 1, igual que el firmware.

Tests `[ejecutado]`:

- `inspector-smoke` · "ANCLAJE 1×..4×": etiqueta y valor en `y+3` (y `x+3`) para todas las combinaciones de tamaño, en los dos layouts;
- paridad BAR lienzo/firmware en 24 casos.

## 2. BOOL: texto y fondo por estado

Firmware `[ejecutado: compilación ESP32]`:

```cpp
JWPLC_UIBoolText("OFF", "ON");                                   // colors.value / colors.background
JWPLC_UIBoolText("OFF", "ON", 0xF800, 0x07E0);                   // texto por estado
JWPLC_UIBoolText("OFF", "ON", 0xF800, 0x07E0, 0x0000, 0x001F);   // texto y fondo por estado
```

- `stateColors` (`JWPLC_UI_BOOL_STATE_TEXT | JWPLC_UI_BOOL_STATE_BACKGROUND`) indica qué usa el estado.
  Así, un color igual en ambos estados ya no desactiva el modo.
- El fondo por estado pinta la región del valor (el mismo `fillRect` de limpieza). El resto del campo
  conserva `colors.background`.
- `setBool` repinta al cambiar de estado si hay algún color por estado.
- Macro `JWPLC_UI_HAS_BOOL_STATE_BACKGROUNDS`. El codegen emite el constructor de 6 argumentos dentro
  de ese `#ifdef`, con respaldo de 2 argumentos.

## 3. Apariencia de TEXT/VALUE/BOOL con mini-modelos

Mismo patrón que el MENU:

- Una vista real del campo, pintada con `app.js:drawField` mediante `renderFieldPreview()` en un
  framebuffer propio. Se recorta al campo y se escala ×1..×3.
- A su lado, con flecha, los colores de cada parte: Etiqueta T, Valor T, Fondo F, Marco M (sólo con marco).
- En BOOL, "Por estado (texto y fondo)" muestra dos casos:
  - Caso FALSE: Valor T, F;
  - Caso TRUE: Valor T, F;
  - "Comunes a ambos estados": Etiqueta, Fondo, Marco.
- Clic en un cuadro: abre el selector nativo y marca el cuadro para la paleta rápida.
- Los mini-modelos sólo se repintan con la pestaña Apariencia a la vista.

Propuesta para la siguiente ronda: extender el patrón a BAR, figuras, RAW_TEXT y NAV_BAR (A15-7E).

## 4. Selector de color: menos carga al arrastrar

Medido en la nube: cada evento `input` del selector nativo costaba ~8 ms de render más los listeners
de `jwplc:editor-refresh`. Chromium emite `input` a la frecuencia del mouse.

`app.js` limita esos eventos a uno cada 40 ms (~25/s) con un listener en captura:

- el primero pasa al instante;
- el último se reenvía al terminar la ventana;
- `change` cancela el reenvío pendiente.

Aplica a todos los inspectores (campos, BAR, MENU, figuras, RAW_TEXT).

## 5. Objetos nuevos al frente

`assignNewLayerOrder` le da al objeto nuevo `min(layerOrder) − 1` de la página (menor = más al frente).
Si no hay espacio, desplaza los demás +1. NAV_BAR no cuenta.

## 6. Grilla de 1 y 2 px

`#gridSizeSelect` agrega 1 px y 2 px. La grilla se omite si la celda queda por debajo de 3 px de
pantalla (antes 4), para que 1 px sea visible desde zoom 3.

## 7. Compilación ESP32 con JWPLC_Display desde fuente `[ejecutado]`

| Sketch | Flash | RAM |
|---|---|---|
| `A15_1_3_Codegen_Static` | 445781 | 35500 |
| `A15_8_Menu` | 441565 | 36084 |
| `A15_8_Menu_Designer` | 440117 | 35972 |
| Header de `inspector-smoke` (BOOL de 6 argumentos) | 422805 | 35484 |

La RAM sube ~576 B respecto de la 1.ª ronda. Cada uno de los 32 slots de campo guarda
`JWPLC_UIBoolText` con 2 colores de fondo y los flags, más `boolState`. Si molesta, la reducción
natural es guardar `boolText` sólo en los slots BOOL. Se revisa al regenerar el `.a`.

Tests: `node tools/jwplc-hmi-designer/tests/run-all.cjs` → 6/6 PASS `[ejecutado]`.
