# Plan de Implementación: Selector Visual de Tipografía y Escala Unificado (Opción A)

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Unificar y modernizar la selección de tipografía y tamaño de texto en el HMI Designer (`tools/jwplc-hmi-designer`), eliminando los controles `<select>` nativos y las tarjetas redundantes de tamaño en favor de una tarjeta interactiva con chips de familia y tamaños contextuales en un solo clic, aplicable a todos los componentes con texto (`VALUE`, `TEXT`, `BOOL`, `BAR` y `MENU`).

**Architecture:** 
- En `poc/designer-fonts.js`, agregar helpers puros (`fontFamilyOf(id)`, `fontSizeOf(id)`, `fontIdFrom(family, size, bold)`, `fontHeightPx(fontId, scale)`) para resolver familias y alturas reales en px sin depender del DOM.
- En `poc/styles.css`, definir clases reutilizables (`.insp-font-family-row`, `.insp-font-size-row`, `.insp-font-chip`, `.insp-bold-btn`) basadas en tokens de diseño semánticos (`--tab-bg`, `--tab-border`, `--tab-text`, `--tab-active-text`, `--orange`) con soporte automático para modo claro y oscuro.
- En `poc/designer-field-inspector.js`, reemplazar las dos tarjetas antiguas ("Tamaño de la etiqueta/valor" y "Tipografía") por la tarjeta unificada de dos niveles para `VALUE`, `TEXT` y `BOOL`.
- En `poc/designer-bar.js` y `poc/designer-menu.js`, adaptar la configuración tipográfica de etiquetas de barra y menús al mismo estándar visual de chips.
- Mantener paridad absoluta con el modelo de datos `.jwhmi`, el sistema de historial (Undo/Redo) y la generación C++ en `JWPLC_HMI_Generated.h`.

**Tech Stack:** Vanilla JavaScript (ES6 / CommonJS UMD), CSS3 Custom Properties (tokens de diseño), HTML5 Canvas 2D, Node.js para pruebas unitarias.

**Spec:** `docs/superpowers/specs/2026-11-10-selector-tipografia-inspector-design.md`

---

## Global Constraints

- No usar dependencias externas en Node.js ni bibliotecas externas en frontend (mantener JavaScript y CSS vanilla puro).
- Respetar la regla de temas: no usar colores hexadecimales rígidos en CSS/HTML; usar variables de tokens semánticos (`var(--tab-bg)`, `var(--line)`, `var(--text)`, `var(--orange)`, etc.).
- Conservar la compatibilidad del esquema JSON `schema/jwhmi.schema.json` y del codegen C++ (emisión de `labelFont`, `valueFont`, `font` y multiplicadores `labelSize`/`valueSize` para GLCD).
- La fuente display `FONT_7SEG_48` debe estar restringida al valor en campos numéricos (`VALUE`); no debe permitirse en etiquetas ni en textos alfanuméricos.
- El canvas ST7789 (`#tftCanvas`, 320×170 px) debe permanecer inmutable en `#000000`.

---

## Review Focus

1. **Persistencia de escala en GLCD:** Al seleccionar `Clásica 5×7`, el multiplicador `labelSize` o `valueSize` (1× a 4×) debe conservarse y sincronizarse con la vista previa del canvas.
2. **Normalización al cambiar de familia:** Al cambiar de `Bitmap` a `Clásica`, `labelFont`/`valueFont` debe volver a `'GLCD'` sin dejar valores huérfanos.
3. **Restricción de 7-Segmentos:** En etiquetas de `VALUE`, en `TEXT`, en `BOOL` y en `BAR`, la opción `7-Seg` no debe aparecer.
4. **Métricas en modo claro:** Las etiquetas, chips inactivos y chips activos deben mantener un contraste WCAG AA sobre `--panel` claro (`#ffffff`) y oscuro (`#0d1822`).
5. **Undo / Redo:** Cambiar de familia o tamaño mediante los nuevos chips debe registrar un solo paso atómico en el historial y responder a `Ctrl+Z` / `Ctrl+Y`.

---

### Task 1: Motor de Resolución Tipográfica y Alturas en `designer-fonts.js`

**Files:**
- Modify: `tools/jwplc-hmi-designer/poc/designer-fonts.js`
- Test: `tools/jwplc-hmi-designer/tests/fonts-resolver.test.cjs` (nuevo)

**Interfaces:**
- Consumes: Catálogo `FONTS` y mapa de fuentes de `designer-fonts.js`.
- Produces: 
  - `fontFamilyOf(id)`: Retorna `'GLCD'`, `'BITMAP'`, `'7SEG'`, `'SANS'`, `'SERIF'` o `'MONO'`.
  - `fontSizeVariantOf(id, scale)`: Retorna el identificador de tamaño (ej. `1`, `2`, `3`, `4` para GLCD; `'16'`, `'26'` para Bitmap; `'48'` para 7SEG; `9`, `12`, `18`, `24` para FreeFonts).
  - `isBold(id)`: Retorna booleano.
  - `fontIdFrom(family, sizeVariant, bold)`: Compone el ID oficial de fuente.
  - `fontHeightPx(id, scale)`: Retorna la altura visual real en píxeles.

- [ ] **Paso 1.1: Escribir la prueba unitaria `tests/fonts-resolver.test.cjs`**
  Validar resolución de familias, variantes de tamaño y cálculo de alturas en px para GLCD (1..4x), FONT_16, FONT_26, FONT_7SEG_48 y SANS_9/12.
- [ ] **Paso 1.2: Ejecutar la prueba y verificar que falle**
  Run: `node tools/jwplc-hmi-designer/tests/fonts-resolver.test.cjs`
  Expected: FAIL con métodos inexistentes en `JWPLCDesignerFonts`.
- [ ] **Paso 1.3: Implementar funciones de resolución en `poc/designer-fonts.js`**
  Añadir las funciones al objeto de exportación de `designer-fonts.js`.
- [ ] **Paso 1.4: Ejecutar la prueba y verificar que pase**
  Run: `node tools/jwplc-hmi-designer/tests/fonts-resolver.test.cjs`
  Expected: PASS al 100%.

---

### Task 2: Estilos CSS de la Botonera de Familias y Tamaños en `poc/styles.css`

**Files:**
- Modify: `tools/jwplc-hmi-designer/poc/styles.css`

**Interfaces:**
- Consumes: Variables semánticas (`--tab-bg`, `--tab-border`, `--tab-text`, `--tab-active-text`, `--orange`, `--panel-inset`, `--muted`).
- Produces: Clases CSS `.insp-font-group`, `.insp-font-row`, `.insp-font-chip`, `.insp-font-chip.active`, `.insp-font-chip.bold-toggle`.

- [ ] **Paso 2.1: Definir las reglas CSS en `poc/styles.css`**
  - Contenedor `.insp-font-group` con espaciado vertical compacto y borde suave.
  - Fila `.insp-font-row` con visualización flex y wrap (`gap: 4px`).
  - Botones `.insp-font-chip` con padding táctil, radio de borde, tipografía nítida y transición suave de hover/active.
  - Botón de negrita `.insp-font-chip.bold-toggle` para fuentes Sans/Mono.
- [ ] **Paso 2.2: Verificar carga de sintaxis CSS**
  Comprobar que no haya errores de parseo CSS en el navegador y compatibilidad con `.theme-light`.

---

### Task 3: Inspector de Campos Dinámicos (`VALUE`, `TEXT`, `BOOL`) en `designer-field-inspector.js`

**Files:**
- Modify: `tools/jwplc-hmi-designer/poc/designer-field-inspector.js`
- Test: Navegador / Electron en `poc/desktop.html`

**Interfaces:**
- Consumes: `JWPLCDesignerFonts.fontFamilyOf()`, `fontIdFrom()`, `fontHeightPx()` de Task 1, clases CSS de Task 2.
- Produces: Interfaz unificada de selección tipográfica para etiqueta y valor en la pestaña `Forma`.

- [ ] **Paso 3.1: Reemplazar el marcado HTML en el template del inspector**
  - Retirar la tarjeta antigua `#barCard` de tamaño con `chips('labelSize')` y `chips('valueSize')`.
  - Retirar los `<select class="field-input" data-font-select="..."></select>`.
  - Insertar la nueva tarjeta unificada `<div class="bar-card" data-font-inspector>` con:
    - Bloque **Etiqueta**: botón de familias (`Clásica 5×7`, `Bitmap`, `Sans`, `Mono`) + fila dinámica de tamaños + readout de altura en px.
    - Bloque **Valor / Estado**: botón de familias (`Clásica 5×7`, `Bitmap`, `Sans`, `7-Seg` solo en `VALUE`, `Mono` en `TEXT`/`BOOL`) + fila dinámica de tamaños + readout de altura en px.
- [ ] **Paso 3.2: Implementar renderizado dinámico y manejadores de evento en JS**
  - Función `renderTypographyControl(container, targetType, currentFont, currentSize, onChange)`:
    - Resuelve la familia activa y dibuja los chips de familia.
    - Renderiza la fila de tamaños contextuales según la familia activa.
    - Actualiza el texto de altura real (ej. `26 px (Bitmap)`).
  - Al pulsar un chip de familia: actualiza el modelo `labelFont` / `valueFont` normalizado.
  - Al pulsar un chip de tamaño: si es GLCD actualiza `labelSize` / `valueSize` (1..4); si es Bitmap/FreeFont actualiza `labelFont` / `valueFont` al ID correspondiente.
- [ ] **Paso 3.3: Conectar con el historial Undo/Redo y sincronización de UI**
  Llamar a `setFieldProps(..., true)` para registrar el cambio en el historial y redibujar el canvas.
- [ ] **Paso 3.4: Probar en navegador la interacción en `VALUE`, `TEXT` y `BOOL`**
  Verificar que:
  - En `VALUE`: la opción `7-Seg` solo aparezca en el Valor y al seleccionarse cambie a 48 px.
  - En `TEXT` y `BOOL`: `7-Seg` esté oculto y aparezca `Mono`.
  - En `Clásica`: se puedan seleccionar los tamaños 1x, 2x, 3x, 4x.
  - El canvas ST7789 redibuje instantáneamente el campo con su tipografía y dimensiones reales.

---

### Task 4: Adaptación en Indicadores de Nivel (`BAR`) y Menú (`MENU`)

**Files:**
- Modify: `tools/jwplc-hmi-designer/poc/designer-bar.js`
- Modify: `tools/jwplc-hmi-designer/poc/designer-menu.js`
- Test: Navegador / Electron en `poc/desktop.html`

**Interfaces:**
- Consumes: Mismo componente/helper tipográfico de Task 1 y 2.
- Produces: Selector visual de fuente para etiqueta de BAR y para filas de MENU.

- [ ] **Paso 4.1: En `designer-bar.js`, adaptar el control de etiqueta**
  - Reemplazar el select/chips de tamaño de etiqueta por el selector de familia (`Clásica 5×7`, `Bitmap`, `Sans`) y tamaños contextuales.
- [ ] **Paso 4.2: En `designer-menu.js`, adaptar el control de tipografía del menú**
  - Reemplazar el dropdown por chips de familia (`Clásica 5×7`, `Bitmap 16`, `Bitmap 26`, `Sans`).
- [ ] **Paso 4.3: Validar comportamiento en pantalla y persistencia en `.jwhmi`**

---

### Task 5: Pruebas Automatizadas, Verificación de Regresión y Bump de Assets

**Files:**
- Modify: `tools/jwplc-hmi-designer/poc/index.html` (subir cache bust `?v=`)
- Modify: `tools/jwplc-hmi-designer/poc/desktop.html` (subir cache bust `?v=`)
- Modify: `tools/jwplc-hmi-designer/poc/service-worker.js` (incrementar CACHE_NAME)
- Test: `tools/jwplc-hmi-designer/tests/run-all.cjs`
- Test: `tools/jwplc-hmi-designer/tests/codegen-core.test.cjs`

- [ ] **Paso 5.1: Ejecutar la suite de pruebas unitarias del Designer**
  Run: `node tools/jwplc-hmi-designer/tests/codegen-core.test.cjs`
  Expected: PASS 100% de pruebas de codegen.
  Run: `node tools/jwplc-hmi-designer/tests/toolbar-dropdowns.test.cjs`
  Expected: PASS.
  Run: `node tools/jwplc-hmi-designer/tests/fonts-resolver.test.cjs`
  Expected: PASS.
- [ ] **Paso 5.2: Incrementar versión de assets (`?v=`) y Service Worker**
  - Actualizar `service-worker.js` (`jwplc-hmi-designer-a17-v2`).
  - Actualizar `ux-foundation.js`, `index.html` y `desktop.html`.
- [ ] **Paso 5.3: Confirmar verificación visual completa**
  Comprobar que en ambos temas (Oscuro y Claro) los nuevos controles tipográficos sean perfectamente legibles, atractivos y no generen desbordes.
