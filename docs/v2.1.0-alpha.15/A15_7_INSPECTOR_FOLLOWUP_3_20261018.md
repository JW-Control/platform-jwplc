# Alpha15 · A15-7 seguimiento (3.ª ronda) — Marco 3×/4×, BOOL fondo completo, flechas, IDLE y NAV

Fecha: `2026-10-18` · tercera revisión del jefe sobre el Designer en su PC.

```text
FRAME_CLIP_3X_4X=PASS_SOURCE (firmware + lienzo + BAR; test "MARCO 1×..4×" sobre 128 combinaciones)
SIZE_CHIPS_ORDER=PASS (etiqueta antes que valor)
BOOL_STATE_BACKGROUND_WHOLE_FIELD=PASS_SOURCE
APPEARANCE_ELBOW_ARROWS=PASS (TEXT/VALUE/BOOL y MENU)
IDLE_WAKE_RETURN_CONFIG=PASS (Proyecto · Pantalla y arranque -> jwplcHMISetup)
NAV_DELETE_PAGE=PASS (clic + confirmación)
NAV_COUNTER_PRESET=PASS_SOURCE (indicador NN/TT de Alpha11 configurable)
NAV_NATIVE_INDICATOR_PARITY=PASS_SOURCE (antes: NN/TT del firmware tapado/oculto en el lienzo)
NAV_SHORT_NAME_ACCENTS=PASS ("Página 4" ya no sale "P?_4")
ESP32_COMPILE=PASS (5 sketches, JWPLC_Display desde fuente)
PHYSICAL=[no verificado]
```

## 1. El marco se recortaba en 3× y 4×

Causa `[visto]`:

- El texto con fondo pinta la celda clásica completa de 6×8 por escala.
- El layout mide el cuerpo de 5×7, así que sobra una columna y una fila de `escala` px.
- Con margen derecho/inferior `max(3, escala)`, ese sobrante caía sobre la línea del marco en 3× y 4×.
- En 1× y 2× cabía.

Esto afectaba a etiqueta, valor y unidad, en TEXT, VALUE, BOOL y BAR.

Corrección (`JWPLC_UI.cpp:effectiveFieldPadding`, `app.js`, `designer-bar.js`):

```text
pad (derecha/abajo) = max(3, escala_mayor + 1)   -> 1×:3  2×:3  3×:4  4×:5
lead (arriba/izquierda) = 3 fijo                 (sin cambio: nada se desplaza)
limpieza del valor: nunca sobre la línea inferior del marco
```

Test `inspector-smoke` · "MARCO 1×..4×" `[ejecutado]`:

- Recorre TEXT, VALUE, BOOL y BOOL por estado, con etiqueta 1–4×, valor 1–4× y los dos layouts.
- Usa textos con descendentes (`gy`).
- Comprueba que todos los píxeles del marco quedan de su color.
- Con la fórmula anterior falla (72–96 px borrados); con la nueva pasa.

### Separación entre el valor y la unidad (VALUE)

No es un recorte: es la **reserva**.

- El firmware reserva el ancho del valor más largo posible (`888.8` para 3 enteros y 1 decimal) y pone la unidad después de esa reserva.
- La unidad se dibuja una sola vez al entrar a la página, no en cada cambio.
- Con alineación «Izquierda», `25.6` deja libre el hueco del cuarto dígito.

El inspector ahora lo explica bajo «Alineación del valor» cuando hay unidad y la alineación no es «Derecha». Con «Derecha», que es la opción por defecto, el valor queda pegado a la unidad.

### Orden de los tamaños

En Forma, «Tamaño de la etiqueta» va antes que «Tamaño del valor», en el mismo orden en que se ven.

## 2. BOOL: fondo por estado en todo el campo

Antes el fondo por estado sólo pintaba la región del valor. Ahora pinta **todo el campo**: fondo, etiqueta, unidad y valor. El marco conserva su color.

Firmware:

- `fieldBackground()` devuelve el fondo del estado actual.
- `drawFieldStatic` y `drawFieldValue` lo usan.
- Al cambiar de estado, `setBool` marca `staticDirty` y `drawDirty` repinta el campo completo **sólo en ese cambio**. El resto de ciclos sigue con dirty refresh normal.

El lienzo y los mini-modelos usan la misma regla.

## 3. Apariencia: flechas con codo de 90°

Nuevo módulo `poc/designer-legend-arrows.js`, compartido por TEXT/VALUE/BOOL y MENU:

- Cada fila de la leyenda se une a su zona con una flecha de codos redondeados de 90°:
  - **Marco**: borde del marco;
  - **Etiqueta**: texto de la etiqueta;
  - **Valor**: texto del valor;
  - **Fondo**: margen inferior.
- La fila en edición va en naranja y su zona se enmarca con línea discontinua en el mini-modelo.
- En el MENU, la flecha naranja apunta al elemento exacto de la fila. Por ejemplo, «Selección T» señala el texto seleccionado y no la fila entera.
- Si el campo es ancho, la leyenda pasa **debajo** del mini-modelo, que se dibuja a ×2. Los carriles de las flechas van por la izquierda.
- BOOL por estado: Caso FALSE y Caso TRUE tienen cada uno Marco, Etiqueta, Valor (T) y Fondo (F) del estado. Etiqueta y marco son comunes.

## 4. Proyecto · Pantalla y arranque (IDLE)

Sin selección, el inspector muestra la configuración del proyecto (`poc/designer-idle.js`):

| Tarjeta | Opciones |
|---|---|
| Flujo de pantallas | Diagrama IDLE ⇄ HMI con la tecla o condición de cada sentido y el estado de arranque |
| Al encender | Pantalla IDLE · Primera página del HMI |
| Entrar al HMI | Cualquier tecla · Una tecla (botonera gráfica ▲◀OK▶▼ + ESC) · Sólo desde el sketch |
| Volver a la IDLE | ESC · Otra tecla (botonera) · Por inactividad (10 s … 2 min) · Nunca |

Se guarda en el `.jwhmi` como `display`. Un proyecto anterior toma los valores por defecto. Se emite al final de `jwplcHMISetup()`:

```cpp
    // Pantalla IDLE (Designer · Pantalla y arranque)
    JWPLC_Display.setIdleWakeMode(JWPLC_DisplayClass::IDLE_WAKE_BUTTON_ONLY);
    JWPLC_Display.setIdleWakeButton(BTN_OK);
    JWPLC_Display.setIdleReturnMode(JWPLC_DisplayClass::IDLE_RETURN_TIMEOUT);
    JWPLC_Display.setIdleTimeoutMs(30000UL);
    JWPLC_Display.enterUserUI();
```

Valores por defecto: arranca en la IDLE, cualquier tecla entra y ESC vuelve. En el firmware, la IDLE no despierta por defecto, así que antes el sketch tenía que llamar a `enterUserUI()`.

Notas del firmware que el panel muestra:

- Con «Por inactividad», ESC no vuelve a la IDLE.
- Con varias páginas, ESC primero sale del contenido al selector.

El gate F0 de Kenyi (`gates/A15_F0_OpenPLC_Bool_Rele`) se regeneró. Su header ya trae esta configuración y el bloque del HAL sólo llama a `jwplcHMISetup()`.

## 5. NAV

### Borrar páginas

Antes había que **mantener** 1,2 s sobre una X de 14 px. Si el puntero se movía un poco, se cancelaba sin aviso. Ahora:

1. el primer clic convierte la X en «¿Borrar (N)?», con N = objetos de esa página;
2. el segundo clic borra;
3. si no hay segundo clic, a los 3 s vuelve a la X.

### Estilo «Contador» (sólo NN/TT)

Es el modelo de Alpha11 (`docs/v2.1.0-alpha.11/A11_3E_MULTI_FIELD_PAGES_GATE.md`):

- caja de 36×12;
- negro con texto blanco mientras se elige página (◀ ▶);
- blanco con texto negro al entrar al contenido (OK);
- ESC vuelve.

La dibuja el firmware, que es el único que sabe el estado. Por eso el codegen no emite dibujo de barra.

- Inspector: tarjeta «Contador · Sólo NN/TT» en Estilo y Prediseño Visual.
- Ubicación: 4 esquinas, o X/Y de precisión. Ancho y alto quedan fijos.
- Se ocultan las opciones que sólo aplican a pestañas (posición de barra, distribución, tipografía, fondo y borde).
- Firmware nuevo: `JWPLC_Display.setPageIndicator(bool visible, int16_t x = 282, int16_t y = 3)` y la macro `JWPLC_UI_HAS_PAGE_INDICATOR_CONFIG`. Es un método nuevo, sin miembros de datos en la clase.

### Paridad del indicador nativo

El firmware dibuja el NN/TT en (282, 3) siempre que haya más de una página, **incluso con una barra NAV**. En el lienzo no se veía:

- con barra, el Designer no lo pintaba;
- sin barra, se pintaba encima del lienzo y el siguiente render lo borraba.

Ahora:

- **Sin barra**: el lienzo, la vista previa y el minimapa muestran el NN/TT en su sitio. Se dibuja en la imagen lógica, como la barra.
- **Barra con pestañas**: el header emite `setPageIndicator(false)`, porque la barra ya muestra la página activa y su propio contador. Así no aparece una caja extra en la TFT.
- **Contador**: `setPageIndicator(true, x, y)`.

Pendiente de taller: con una barra de pestañas no hay señal visual de selector frente a contenido. Si hace falta, se puede volver a mostrar el NN/TT sobre el contador de la barra.

### Abreviaturas

Se quitan las tildes antes de abreviar el nombre de la página: «Página 4» → `PA_4`. Antes salía `PÁ_4`, y la TFT, sin «Á», lo dibujaba como `P?_4`.

### Revisión de opciones

Script con todos los botones del inspector NAV `[ejecutado]`:

- todos cambian la configuración o el dibujo;
- las excepciones son el valor que ya estaba activo y «Centrar V»/«100 %» sobre una barra vertical que ya ocupa los 170 px.

## 6. Compilación ESP32 (JWPLC_Display desde fuente) `[ejecutado]`

| Sketch | Flash | RAM |
|---|---|---|
| `A15_1_3_Codegen_Static` | 446377 | 35628 |
| `A15_8_Menu` | 442209 | 36220 |
| `A15_8_Menu_Designer` | 440757 | 36108 |
| Golden `full` (IDLE con teclas y timeout, barra NAV) | 431729 | 35724 |
| Golden `navCounter` (Contador abajo a la izquierda) | 421681 | 35252 |
| F0 OpenPLC (HAL VPP + bloque, header regenerado) | 422401 | 36612 |

La RAM sube unos 128 B: `staticDirty` por slot y la posición del indicador.

Tests: `node tools/jwplc-hmi-designer/tests/run-all.cjs` → 6/6 PASS `[ejecutado]`. Nuevos:

- `inspector-smoke`: MARCO 1×..4×, PANTALLA IDLE, BORRAR PÁGINA y NAV · CONTADOR;
- `menu-smoke`: flechas de la leyenda;
- `codegen-smoke`: IDLE con teclas compila;
- golden `navCounter`.

Las cabeceras de los gates `A15_1_3` y `A15_8_Menu_Designer` siguen compilando. Al regenerarlas desde el Designer incluirían el bloque IDLE por defecto.
