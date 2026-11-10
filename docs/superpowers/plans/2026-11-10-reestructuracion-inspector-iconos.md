# Plan: Reestructuración y Optimización del Inspector de Iconos (HMI Designer)

> **Para desarrolladores y agentes:** Sub-skill requerida: `superpowers:executing-plans` o `superpowers:subagent-driven-development`.
> Cada paso usa la sintaxis de lista de tareas (`- [ ]`).

**Meta:** Reestructurar el inspector de Iconos (`ICON`) en `tools/jwplc-hmi-designer` para lograr una experiencia sin scroll, eliminar selectores redundantes, incorporar la matriz de alineación 3×3 para etiquetas, reorganizar la pestaña de Apariencia con vista interactiva a dos columnas (icono grande a la izquierda, componentes a la derecha y editor de color grande abajo sin paleta rápida redundante), y simplificar la configuración para iconos no animados.

**Arquitectura y UX:**
1. **Pestaña Forma (`design:shape`):**
   - Retirar la tarjeta inicial de previsualización y selector redundante de icono (`icon-current` y `<select data-icon-prop="iconId">`).
   - Compactar **Tamaño** en una fila horizontal con chips.
   - Unificar **Marco** y **Fondo** en una fila compacta de 2 columnas con toggles segmentados.
   - Tarjeta de **Velocidad de animación** visible única y exclusivamente si el icono es animado (`hasAnim`).
   - Para Piloto y Baliza: selectores de modo/pisos limpios sin duplicar listas de colores en Forma.
2. **Pestaña Contenido (`design:content`):**
   - Selector interactivo de **Estados** en miniatura (clic para previsualizar en el lienzo).
   - Tarjeta **Etiqueta** con input de texto, **Matriz 3×3 de alineación** (`align-matrix-3x3`) y botones de escala (`1×` / `2×`).
   - Sincronización en modelo de `alignV` (`TOP`, `MID`, `BOTTOM`), `alignH` (`LEFT`, `CENTER`, `RIGHT`), `layout` y `align`.
3. **Pestaña Apariencia (`design:appearance`):**
   - Reorganizar `menu-case-body` en layout flex horizontal:
     - **Izquierda:** Icono en grande (escala 2× o 3× según tamaño con renderizado pixelado y centrado).
     - **Derecha:** Leyenda interactiva de componentes seleccionables (`Etiqueta`, `Marco`, `Fondo`, `Cuerpo` / pisos / estados) con sus swatches y atajos.
   - Eliminar la tarjeta de paleta rápida (`data-icon-presets-card`), dejando abajo únicamente el **Editor de color grande** (`JWPLCColorEditor`).
4. **CSS & Tokens (`poc/styles.css`):**
   - Clases y selectores compactos para el contenedor de 2 columnas (Marco + Fondo) y la distribución del mini-modelo.

---

## Archivos Involucrados

- Modificar: `tools/jwplc-hmi-designer/poc/designer-icons.js`
- Modificar: `tools/jwplc-hmi-designer/poc/styles.css`
- Modificar: `tools/jwplc-hmi-designer/poc/app.js` (si se requiere soporte adicional de centrado/alineación de etiqueta de icono)
- Crear: `tools/jwplc-hmi-designer/tests/icon-inspector.test.cjs`
- Modificar: `tools/jwplc-hmi-designer/tests/run-all.cjs`

---

## Tareas de Implementación

### Tarea 1: Reestructuración de la Pestaña Forma (`design:shape`)

**Archivos:**
- Modificar: `tools/jwplc-hmi-designer/poc/designer-icons.js`
- Modificar: `tools/jwplc-hmi-designer/poc/styles.css`

- [ ] **Paso 1.1:** En el template HTML de `iconInspectorSection` en `designer-icons.js`:
  - Eliminar el bloque inicial `.icon-current` (canvas redundante, nombres y select `<select data-icon-prop="iconId">`).
  - Fusionar las tarjetas de **Marco** y **Fondo** en una sola tarjeta compacta `.bar-card` con grid de 2 columnas (`.bar-visual-grid-2`), retirando las notas largas `.insp-note`.
- [ ] **Paso 1.2:** En `patchInspector()` de `designer-icons.js`:
  - Retirar las referencias al select de `iconId` y lectura de icono actual.
  - Asegurar que `data-icon-anim-card` se oculte (`animCard.hidden = !hasAnim`) para iconos no animados.
  - Para `isPilot` y `isBaliza`, asegurar que los selectores de modo y cantidad de pisos se muestren de forma compacta sin forzar scroll.
- [ ] **Paso 1.3:** Verificar con `node --check tools/jwplc-hmi-designer/poc/designer-icons.js`.

---

### Tarea 2: Matriz de Alineación 3×3 en Contenido (`design:content`)

**Archivos:**
- Modificar: `tools/jwplc-hmi-designer/poc/designer-icons.js`
- Modificar: `tools/jwplc-hmi-designer/poc/app.js`

- [ ] **Paso 2.1:** En el template HTML del panel `design:content` en `designer-icons.js`:
  - Reemplazar los botones de posición (`STACKED` / `INLINE`) por la estructura `bar-align-matrix-row` conteniendo la matriz `align-matrix-3x3` con 9 botones (`data-align-v` = `TOP`, `MID`, `BOTTOM` y `data-align-h` = `LEFT`, `CENTER`, `RIGHT`).
  - Colocar a su lado la columna de tamaño de texto de etiqueta (`1×`, `2×`).
- [ ] **Paso 2.2:** En `designer-icons.js`, registrar listeners para los botones de la matriz 3×3:
  - Al hacer clic en una celda, actualizar `field.alignV`, `field.alignH`, `field.layout` (`field.alignV === 'MID' ? 'INLINE' : 'STACKED'`) y `field.align = field.alignH`.
  - En `patchInspector()`, actualizar la clase `.active` de la celda seleccionada según `field.alignV` y `field.alignH`.
- [ ] **Paso 2.3:** En `app.js:computeFieldGeometry()` y `drawField()`:
  - Asegurar que cuando `field.type === 'ICON'`, la etiqueta respete el alineamiento horizontal y vertical indicado en la matriz (arriba centrado/izq/der, en medio izq/der, o abajo centrado/izq/der).
- [ ] **Paso 2.4:** Verificar con `node --check tools/jwplc-hmi-designer/poc/designer-icons.js` y `node --check tools/jwplc-hmi-designer/poc/app.js`.

---

### Tarea 3: Mini-Modelo Horizontal y Eliminación de Paleta Rápida en Apariencia (`design:appearance`)

**Archivos:**
- Modificar: `tools/jwplc-hmi-designer/poc/designer-icons.js`
- Modificar: `tools/jwplc-hmi-designer/poc/styles.css`

- [ ] **Paso 3.1:** En el template HTML del panel `design:appearance`:
  - Retirar la clase `is-below` de `data-icon-case-body="single"` para permitir layout horizontal flex (icono a la izquierda, leyenda a la derecha).
  - Eliminar por completo el bloque `.bar-card data-icon-presets-card` (paleta rápida).
  - Dejar únicamente el contenedor del editor de color grande `[data-icon-color-editor]`.
- [ ] **Paso 3.2:** En `drawAppearance()` y `patchInspector()` de `designer-icons.js`:
  - Retirar `body.classList.add('is-below')`.
  - Configurar escala visual nítida para el canvas (`scale = 2` o `3` si el icono es de 24 o 32 px para que se aprecie con claridad).
  - Retirar lógica y selectores vinculados a `presetsCard`, `presetsTitle` y `presetsRow`.
- [ ] **Paso 3.3:** En `styles.css`, ajustar estilos para `.icon-case-body` si se necesita un ancho o padding óptimo a dos columnas.
- [ ] **Paso 3.4:** Verificar con `node --check tools/jwplc-hmi-designer/poc/designer-icons.js`.

---

### Tarea 4: Pruebas Unitarias y Regresión

**Archivos:**
- Crear: `tools/jwplc-hmi-designer/tests/icon-inspector.test.cjs`
- Modificar: `tools/jwplc-hmi-designer/tests/run-all.cjs`

- [ ] **Paso 4.1:** Escribir prueba unitaria `icon-inspector.test.cjs` que valide:
  - Ausencia de selector redundante de icono en el DOM de Forma.
  - Presencia y 9 celdas operativas de la matriz 3×3 en Contenido.
  - Ausencia de tarjeta de paleta rápida en Apariencia.
  - Presencia del editor de color grande en Apariencia.
  - Comportamiento de iconos animados vs no animados (visibilidad de velocidad).
- [ ] **Paso 4.2:** Registrar `icon-inspector.test.cjs` en `tools/jwplc-hmi-designer/tests/run-all.cjs`.
- [ ] **Paso 4.3:** Ejecutar todas las suites de prueba de la herramienta:
  - `node tools/jwplc-hmi-designer/tests/icon-inspector.test.cjs`
  - `node tools/jwplc-hmi-designer/tests/codegen-core.test.cjs`
  - `node tools/jwplc-hmi-designer/tests/toolbar-dropdowns.test.cjs`
  - `node tools/jwplc-hmi-designer/tests/icon-catalog.test.cjs`
  - `node tools/jwplc-hmi-designer/tests/runtime-core.test.cjs`
  - `node tools/jwplc-hmi-designer/tests/sketch-guide.test.cjs`
