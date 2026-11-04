# Plan de Implementación: Indicadores de Nivel 100% Operativos (HMI Designer & Firmware)

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Dejar los indicadores de nivel (barra horizontal, barra vertical/tanque y medidor circular) 100% integrados y operativos entre el firmware `JWPLC_Display` y el HMI Designer (`tools/jwplc-hmi-designer`), con redibujado incremental ultra-optimizado (sin parpadeos ni redibujos completos), estilos sólido y bloques/segmentado (descartando diagonales, puntos y rayas), y soporte de degradado.

**Architecture:** 
- En `JWPLC_Display` (`JWPLC_UI.h` / `JWPLC_UI.cpp`), extender `JWPLC_UIBarField` para soportar dimensiones libres en X e Y (no solo 12 px fijos), orientación vertical (llenado de abajo hacia arriba para tanques) y horizontal (izquierda a derecha), y estilos `SOLID` y `BLOCKS` (segmentado industrial) con actualización diferencial (`fillRect` exclusivo del delta que sube o baja).
- En el Designer (`designer-bar.js`, `designer-codegen-core.js`, `index.html`, `app.js`), desbloquear la barra vertical y el grosor en el inspector, descartar Diagonales, Puntos y Rayas dejando únicamente Sólido y Bloques tanto en Barra como en Circular, habilitar selector directo de Barra o Medidor Circular en el menú de herramientas, y asegurar paridad visual y de código generado.

**Tech Stack:** C++11 (ESP32 / Arduino / JWPLC_Display / TFT_eSPI), Vanilla JavaScript (DOM, Canvas 2D, CommonJS en Node para pruebas).

**Spec / Requirements:**
- Mantener compatibilidad ABI y retrocompatibilidad con proyectos existentes de `JWPLC_UIBarField`.
- Rendimiento crítico: actualización incremental delta (si el nivel sube, pintar solo el avance; si baja, borrar solo el retroceso con el fondo).
- Descartar estilos pesados (`STRIPES` diagonales, `DOTS` puntos, `TICKS` rayas radiales) por rendimiento en MCU; mantener únicamente `SOLID` (continuo) y `BLOCKS` (segmentos discretos) tanto para barra como para circular.
- Soporte de degradado estático sampleado por posición (el avance toma el color interpolado del punto donde se encuentra; el retroceso borra en bloque con el color de fondo).
- Ambas orientaciones operativas: Horizontal (barra estándar) y Vertical (tanques de almacenamiento, silos, depósitos).

---

## Global Constraints
- No usar dependencias externas nuevas en JS ni en C++.
- Mantener la regla del proyecto: API C++ pública sin `::` (usar constantes y métodos con formato estándar del package).
- El tiempo de refresco en MCU debe ser sub-milisegundo (< 100 µs por actualización de barra).
- Todo cambio en el codegen debe reflejarse en `designer-codegen-core.js` y pasar `tests/codegen-core.test.cjs`.

## Review Focus
1. **Delta de bajada vertical:** Al vaciarse un tanque vertical, el bloque borrado debe calcularse desde la posición superior previa hacia abajo sin dejar artefactos de 1 px.
2. **Delta en bloques/segmentos:** Al cruzar un umbral de bloque, solo se debe dibujar o borrar el bloque afectado, no toda la barra.
3. **Escala y márgenes:** En orientación vertical, la etiqueta debe poder ubicarse arriba (`STACKED`) o abajo, o sin etiqueta, respetando la caja del campo.
4. **Degradado diferencial:** Al avanzar en modo degradado, cada segmento o línea toma el color interpolado exacto sin requerir redibujar el inicio de la barra.
5. **Persistencia `.jwhmi`:** Guardar y abrir proyectos con barras verticales y circulares debe mantener intactas todas las propiedades.

---

### Task 1: Firmware `JWPLC_Display` — Geometría y Estilos de `JWPLC_UIBarField`

**Files:**
- Modify: `JWPLC/2.1.0/libraries/JWPLC_Display/src/JWPLC_UI.h`
- Modify: `JWPLC/2.1.0/libraries/JWPLC_Display/src/JWPLC_UI.cpp`
- Test: `tools/jwplc-hmi-designer/tests/codegen-core.test.cjs`

- [ ] **Paso 1.1:** En `JWPLC_UI.h`, definir constantes y enumeraciones para la barra:
  - `JWPLC_UI_BAR_HORIZONTAL = 0`, `JWPLC_UI_BAR_VERTICAL = 1`
  - `JWPLC_UI_BAR_STYLE_SOLID = 0`, `JWPLC_UI_BAR_STYLE_BLOCKS = 1`
  - Extender `JWPLC_UIBarStyle` y struct de opciones para incluir `orientation` (horizontal/vertical), `barStyle` (sólido/bloques) y color final de degradado con valores por defecto compatibles con código anterior.
- [ ] **Paso 1.2:** En `JWPLC_UI.cpp`, actualizar `computeFieldGeometry()` para `JWPLC_UI_FIELD_BAR`:
  - Si `rect.height != JWPLC_UI_AUTO`, respetar la altura indicada en lugar de fijar `DEFAULT_BAR_HEIGHT = 12`.
  - Si la orientación es `VERTICAL`, calcular el área de valor `valueW` y `valueH` acordes (la altura es la dimensión principal del tanque, el ancho es el grosor del tanque).
- [ ] **Paso 1.3:** Compilar sintaxis y pruebas de núcleo para verificar que las firmas sigan siendo válidas.

---

### Task 2: Firmware `JWPLC_Display` — Motor de Renderizado Incremental Delta (Ultra-Optimizado)

**Files:**
- Modify: `JWPLC/2.1.0/libraries/JWPLC_Display/src/JWPLC_UI.cpp`
- Test: Compilación contra biblioteca ESP32 / pruebas locales.

- [ ] **Paso 2.1:** Implementar función auxiliar de dibujo delta `drawBarDelta(tft, field, oldLevel, newLevel)`:
  - **Horizontal Sólido:**
    - Subida: `tft.fillRect(valX + oldLevel, valY, newLevel - oldLevel, valH, color)`.
    - Bajada: `tft.fillRect(valX + newLevel, valY, oldLevel - newLevel, valH, background)`.
  - **Vertical Sólido (Llenado de abajo hacia arriba para Tanques):**
    - Subida: `tft.fillRect(valX, valY + valH - newLevel, valW, newLevel - oldLevel, color)`.
    - Bajada: `tft.fillRect(valX, valY + valH - oldLevel, valW, oldLevel - newLevel, background)`.
  - **Estilo Bloques (`BLOCKS`):**
    - Dividir la longitud en segmentos discretos (bloques con separación de 1-2 px).
    - Iterar solo sobre los bloques cuyo estado de activación haya cambiado entre `oldLevel` y `newLevel`.
    - Dibujar cada bloque activo con su color y cada bloque desactivado con el color de fondo/pista.
  - **Modo Degradado (`GRADIENT`):**
    - Si `valueColorEnd != 0` y distinto de `valueColor`, calcular el color interpolado según la posición relativa del tramo.
    - En bloques: cada bloque tiene su color fijo precalculado según su posición porcentual.
    - En sólido: pintar las líneas/columnas del delta con el color correspondiente a su avance relativo.
- [ ] **Paso 2.2:** Conectar `drawBarDelta` tanto en el refresco inicial (`drawFieldDynamic` inicial cuando `barDrawnW == -1`) como en las actualizaciones periódicas (cuando el valor cambia).

---

### Task 3: Codegen Core — Soporte de Generación C++ para Barras Verticales y Estilos

**Files:**
- Modify: `tools/jwplc-hmi-designer/poc/designer-codegen-core.js`
- Modify: `tools/jwplc-hmi-designer/cli/lib/spec.cjs`
- Test: `tools/jwplc-hmi-designer/tests/codegen-core.test.cjs`

- [ ] **Paso 3.1:** Escribir prueba unitaria en `tests/codegen-core.test.cjs` que valide la generación de una barra vertical y de una barra con estilo `BLOCKS`.
- [ ] **Paso 3.2:** En `designer-codegen-core.js` (`barFieldBlock`), emitir los parámetros correspondientes de ancho/alto reales (`rect`), orientación (`VERTICAL` o `HORIZONTAL`) y estilo (`SOLID` o `BLOCKS`).
- [ ] **Paso 3.3:** Ejecutar `node tools/jwplc-hmi-designer/tests/codegen-core.test.cjs` y verificar que pase al 100%.

---

### Task 4: UI Designer (`designer-bar.js`) — Desbloqueo y Refinamiento del Inspector

**Files:**
- Modify: `tools/jwplc-hmi-designer/poc/designer-bar.js`
- Test: Sintaxis y validación en navegador / Desktop.

- [ ] **Paso 4.1:** En `closeBarInspector()` y template HTML del inspector:
  - Retirar `markNeedsFirmware` del botón de orientación `VERTICAL`.
  - Retirar el bloqueo forzado de `barFormaHeight` (permitir grosor editable tanto en horizontal como en vertical).
  - Retirar las opciones de `STRIPES` (Diagonales) y `DOTS` (Puntos) del DOM del inspector de Barra, dejando visibles y activos solo `SOLID` y `BLOCKS`.
  - Retirar las opciones de `DOTS` (Puntos) y `TICKS` (Rayas) del DOM del inspector de Circular, dejando visibles y activos solo `CONTINUOUS` (Sólido) y `SEGMENTED` (Bloques).
- [ ] **Paso 4.2:** En `applyFirmwareCaps(field)`:
  - Permitir que `field.barOrientation` conserve `'VERTICAL'`.
  - Permitir que `field.barStyle` conserve `'BLOCKS'`.
  - Permitir que `field.circleStyle` sea `'CONTINUOUS'` o `'SEGMENTED'`.
  - Permitir que `field.barHeight` sea editable cuando la barra es horizontal (grosor) o vertical (longitud).
- [ ] **Paso 4.3:** En `barGeometry(field)` y `drawBarField(ctx, field, scale)`:
  - Asegurar que el renderizado en canvas 2D del Designer dibuje exactamente igual que el firmware:
    - Vertical llenando desde la base hacia arriba.
    - Bloques discretos con su separación adecuada.
    - Mini-modelo de Apariencia A1 sincronizado con la orientación y estilo activos.

---

### Task 5: Barra de Herramientas y UX de Inserción

**Files:**
- Modify: `tools/jwplc-hmi-designer/poc/index.html`
- Modify: `tools/jwplc-hmi-designer/poc/app.js`

- [ ] **Paso 5.1:** En el menú desplegable de variables del lienzo (`#variableDropdownMenu`):
  - Ofrecer opciones claras:
    - **Indicador de Barra (Nivel / Tanque)** (`barField`): crea barra lineal.
    - **Medidor Circular (Gauge / Carga)** (`circularField`): crea directamente un medidor circular.
- [ ] **Paso 5.2:** En `app.js`, gestionar la creación directa del tipo seleccionado sin obligar al usuario a crearlo horizontal y luego cambiar de preset en el inspector.

---

### Task 6: Verificación Integral, Pruebas y Paridad

**Files:**
- Test: `tools/jwplc-hmi-designer/tests/codegen-core.test.cjs`
- Test: `tools/jwplc-hmi-designer/tests/inspector-smoke.cjs`
- Update: `tools/jwplc-hmi-designer/poc/service-worker.js` (incrementar cache version)
- Update: Versión de assets `?v=` en `index.html` y `desktop.html`.

- [ ] **Paso 6.1:** Actualizar pruebas unitarias e inspector smoke para comprobar la orientación vertical y el selector de preset.
- [ ] **Paso 6.2:** Probar en la app de escritorio / navegador local (`poc/desktop.html`) la creación de:
  - Tanque vertical (nivel 0% a 100%, llenado de abajo a arriba).
  - Barra horizontal sólida y por bloques.
  - Medidor circular (360° y 270°).
- [ ] **Paso 6.3:** Comprobar la generación C++ contra `JWPLC_Display` verificando que compile limpiamente sin advertencias.
