# Alpha15 · A15-7 seguimiento — Colores en vivo, BOOL por estado, regla, Apariencia del MENU

Fecha: `2026-10-17` · revisión del jefe sobre el Designer en su PC.

```text
LIVE_COLOR_PREVIEW=PASS (TEXT/VALUE/BOOL y MENU; BAR, figuras y RAW_TEXT ya lo hacían)
BOOL_STATE_COLORS=PASS_SOURCE (JWPLC_Display en modo fuente; .a al cierre)
RULER_CURSOR_ALIGNMENT=PASS
MENU_APPEARANCE_REDESIGN=PASS
TEXT_X4_LABEL_SHIFT=CORREGIDO en firmware y Designer (ver A15_7_INSPECTOR_FOLLOWUP_2_20261017.md)
JWPLC_DISPLAY_LINKAGE=SOURCE_FIRST hasta el cierre del alpha
PHYSICAL=[no verificado]
```

## 1. Librerías en modo fuente durante el alpha

Convención del equipo, la misma que `docs/v2.1.0-alpha.14/A14_MODBUS_RTU_SOURCE_FIRST_20260930.md`:

- una librería que va a cambiar durante el alpha se compila desde fuente;
- el `.a` se regenera una sola vez antes del cierre;
- después se corre el benchmark de compilación en la PC del jefe y se documenta.

```text
JWPLC_Display=SOURCE_FIRST_DEVELOPMENT   (se retiró sólo precompiled=full; dot_a_linkage=true se mantiene)
JWPLC_Display/src/esp32/libJWPLC_Display.a
  SHA256=0317c3ac22790e975ec775b1150bba658df34c0d8e8f98aa2e63d8f9d46aed0c   (MENU 1.1.0, sin color por estado)
  ROLE=ARTEFACTO HISTÓRICO · DEVELOPMENT_LINKAGE=FORBIDDEN · regenerar al cierre
JWPLC_TFT=PRECOMPILED (0.3.0)
```

`JWPLC_TFT` sigue precompilada por dos motivos:

- su `.a` incluye TFT_eSPI, que no viene como fuente en el package; en modo fuente fallaría la compilación en una PC sin TFT_eSPI instalado;
- ya no está previsto cambiarla en este alpha.

`dot_a_linkage=true` mantiene el enlazado por archivo: el MENU sólo entra si el sketch usa `setMenus`.

Compilación ESP32 con `JWPLC_Display` desde fuente `[ejecutado]`:

| Gate | Flash | RAM |
|---|---|---|
| `A15_1_3_Codegen_Static` | 445433 | 34924 |
| `A15_8_Menu` | 441413 | 35556 |
| `A15_8_Menu_Designer` | 440005 | 35460 |

La RAM sube 296 B respecto del `.a`: es el color por estado, 2 × 4 B por campo × 32 slots más la tabla del sketch.

Tests: `codegen-smoke` y `menu-smoke` verifican las llamadas contra el `.a` si la librería es precompilada, o contra las definiciones de `src/*.cpp` si está en modo fuente (`tests/lib-symbols.cjs`).

Al cierre:

```bash
tools/alpha15/regenerate_libJWPLC_Display.sh build --install
# volver a agregar precompiled=full en JWPLC_Display/library.properties
# benchmark de compilación en la PC del jefe y documentación del alpha
```

## 2. Colores en vivo

El selector de color nativo emite `input` mientras se arrastra y `change` al cerrarse:

- `input` actualiza el modelo y el lienzo sin registrar historial;
- `change` registra un solo paso de historial.

Aplica a TEXT/VALUE/BOOL (`designer-field-inspector.js`) y a MENU (`designer-menu.js`). BAR, figuras y RAW_TEXT ya funcionaban así. Test: `inspector-smoke` dispara `input` sobre el selector y comprueba el modelo.

## 3. BOOL: color del valor por estado

Firmware `[ejecutado: host + ESP32]`:

- `JWPLC_UIBoolText` agrega `falseColor` y `trueColor`, más un constructor de 4 argumentos. El de 2 argumentos deja ambos en 0.
- Si los colores difieren, `drawFieldValue` usa el del estado actual; si son iguales, se usa `colors.value` como siempre.
- `setBool` repinta al cambiar de estado aunque el texto sea el mismo (por ejemplo "●").
- Macro `JWPLC_UI_HAS_BOOL_STATE_COLORS`.

Designer:

- Apariencia → "Color del valor": tarjetas "Un color" / "Por estado", más los colores FALSE y TRUE. El lienzo usa el color del estado de prueba (Datos).
- Codegen: el bloque de 4 argumentos va dentro de `#ifdef JWPLC_UI_HAS_BOOL_STATE_COLORS`, con respaldo de un solo color. Un package anterior compila igual.

## 4. Regla y marca del cursor

- La grilla se dibuja en `floor(x·zoom)`, pero las marcas de la regla usaban `round(x·zoom)`. Con zoom fraccionario (3.95) se desplazaban 1 px en la mitad de las posiciones. Ahora todo usa `floor`: marcas, números, bordes de selección y cursor.
- La marca del cursor era una línea en el centro del píxel, entre dos marcas. Ahora resalta la celda completa del píxel, de su borde al siguiente, alineada con marcas y grilla. El número va centrado y el crosshair queda al centro de la celda.

## 5. MENU · Apariencia

El orden anterior (texto, valor, fondo, selección, edición, título, marco) no seguía la lectura de la pantalla. Ahora:

- **Caso 1 · Navegando**: mini-modelo pintado con el mismo pintor del lienzo (título, ítem normal, ítem seleccionado). A la derecha, a la altura de cada fila y con flecha, sus colores:
  - Título: F, T;
  - Ítem: T, V;
  - Selección: F, T;
  - Fondo: F.
- **Caso 2 · Editando un valor (OK)**: la misma vista con la fila en edición y sus colores, Edición F y T.
- **Marco**: sólo si el marco está activo.
- **Paleta rápida**: se aplica al cuadro elegido (resaltado).

Cada cuadro abre el selector nativo, con vista previa en vivo. Los mini-modelos se repintan con cada cambio.

## 6. TEXT: la etiqueta se mueve 1 px al pasar el valor a 4×

> **Actualización (2.ª ronda):** corregido en el firmware. El margen superior/izquierdo es siempre 3 px
> y el extra de 4× sólo agranda el campo a la derecha y abajo. Detalle en
> `A15_7_INSPECTOR_FOLLOWUP_2_20261017.md`. El texto de abajo queda como registro del diagnóstico.

No es un error del Designer: replica lo que hace el firmware.

`effectiveFieldPadding()` (`JWPLC_UI.cpp`) usa un padding de `max(3, tamaño_etiqueta, tamaño_valor)`. Entre 1× y 3× el padding es 3 px; en 4× pasa a 4 px. Esa escala necesita una celda más para la fila de descendentes (la 8.ª fila del GLCD × 4). Etiqueta y valor arrancan en `(x + pad, y + pad)`, así que en 4× se corren 1 px a la derecha y 1 px abajo. El lienzo usa la misma regla (`app.js:effectiveFieldPadding`), y la TFT hace exactamente lo mismo.

Opciones (decisión del jefe):

| Opción | Efecto |
|---|---|
| A. Mantener | Igual que hoy; lienzo y TFT coinciden. |
| B. Padding fijo de 3 px | La etiqueta no se mueve; en 4× la fila de descendentes puede tocar el borde del marco. |
| C. Crecer hacia afuera | El contenido queda fijo y el marco crece 1 px hacia arriba y a la izquierda (el rectángulo deja de empezar en x, y). |

B y C cambian `JWPLC_UI.cpp` (en modo fuente, sin regenerar el `.a`) y el Designer a la vez.
