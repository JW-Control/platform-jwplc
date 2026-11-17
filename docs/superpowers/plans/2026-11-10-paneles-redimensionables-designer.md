# Paneles y Barra Inferior Redimensionables Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Permitir que los paneles laterales (izquierdo y derecho) y la barra inferior de código generado de `jwplc-hmi-designer` se puedan redimensionar interactivamente mediante arrastre, con persistencia en `localStorage`, accesibilidad por teclado y restablecimiento por doble clic.

**Architecture:** Separadores/splitters superpuestos con `PointerEvents` (`setPointerCapture`) que modifican variables CSS (`--left-panel-width`, `--right-panel-width`, `--bottom-panel-height`) directamente en el elemento raíz sin alterar el conteo de celdas de CSS Grid. Módulo modular `designer-layout-resizer.js` acoplado limpiamente al ciclo de vida del Designer.

**Tech Stack:** JavaScript Vanilla (ES6+), CSS3 Grid & Custom Properties, HTML5 PointerEvents API, DOM / Node.js test runner.

**Spec:** `docs/superpowers/specs/2026-11-10-paneles-redimensionables-designer-design.md`

## Global Constraints

- No romper el orden ni contratos de carga de scripts en `poc/index.html` y `poc/desktop.html`.
- No alterar las dimensiones fijas del framebuffer TFT ST7789 (320x170).
- Mantener compatibilidad total con tema claro y tema oscuro.
- En modo `layout-compact` (móvil/carril), los splitters deben ocultarse (`display: none`).
- Las pruebas de regresión existentes en `tests/run-all.cjs` deben pasar al 100%.

## Review Focus

1. **Colisión de límites con canvas central:** Al agrandar ambos paneles laterales, el editor central debe mantener al menos 350px de ancho disponible y no colapsar.
2. **Interacción con panel inferior colapsado:** Si la barra inferior está colapsada (`body.bottom-collapsed`), arrastrar el divisor hacia arriba debe descolapsar automáticamente el panel y asignar la nueva altura.
3. **Pérdida de cursor fuera de la ventana:** Al arrastrar rápidamente fuera de los límites de la ventana o sobre un canvas/iframe, `setPointerCapture` debe retener el control hasta soltar el botón.
4. **Restablecimiento fiable con doble clic:** Un doble clic rápido en cualquier divisor debe borrar el valor en `localStorage` y restaurar los valores por defecto del diseño.
5. **Persistencia limpia:** Al recargar la aplicación en el navegador o Electron, los anchos y alturas personalizados deben restaurarse inmediatamente antes del primer render para evitar parpadeos visuales (FOUC).

---

### Task 1: Reglas CSS y Variables de Dimensiones

**Files:**
- Modify: `tools/jwplc-hmi-designer/poc/styles.css:58-67`, `303-308`
- Modify: `tools/jwplc-hmi-designer/poc/desktop-responsive.css:113-124`, `141-149`

**Interfaces:**
- Produces: Variables CSS `--left-panel-width`, `--right-panel-width`, `--bottom-panel-height` aplicadas en `body` y `.workspace`.
- Produces: Clases `.layout-resizer`, `.layout-resizer-vertical`, `.layout-resizer-horizontal`, `.layout-resizer-left`, `.layout-resizer-right`, `.layout-resizer-bottom` con estilos de hover, dragging, y cursores `col-resize` / `row-resize`.

- [x] **Step 1: Añadir variables CSS a las reglas de grid en `styles.css`**
  Configurar `grid-template-rows` en `body` para usar `var(--bottom-panel-height, 164px)` y `.workspace` para usar `var(--left-panel-width, 292px) minmax(350px, 1fr) var(--right-panel-width, 380px)`.
- [x] **Step 2: Actualizar `desktop-responsive.css` para respetar las variables con fallback**
  Reemplazar valores duros en `.layout-wide .workspace` y `body.layout-wide` por `var(--left-panel-width, var(--responsive-left-wide))` y `var(--bottom-panel-height, 150px)`.
- [x] **Step 3: Añadir estilos de los splitters en `styles.css`**
  Crear estilos para `.layout-resizer` (hitbox de 6-8px, línea central de 1-2px, transiciones suaves, color `--line`, hover/active color `--orange` o `--cyan`, ocultos en `layout-compact`).

---

### Task 2: Módulo `designer-layout-resizer.js`

**Files:**
- Create: `tools/jwplc-hmi-designer/poc/designer-layout-resizer.js`

**Interfaces:**
- Produces: Objeto global `window.JWPLCHMILayoutResizer` con métodos:
  - `init()`
  - `getSizes() -> { left: number, right: number, bottom: number }`
  - `setSizes({ left, right, bottom })`
  - `reset(target?: 'left' | 'right' | 'bottom')`

- [x] **Step 1: Implementar lectura y aplicación inicial de `localStorage`**
  Leer claves `jwplc-hmi-layout.leftPanelWidth`, `rightPanelWidth`, `bottomPanelHeight` y aplicarlas a `document.documentElement.style` al inicio.
- [x] **Step 2: Implementar lógica de arrastre con `PointerEvents`**
  Manejadores `pointerdown` con `setPointerCapture`, `pointermove` calculando delta con respecto a los bordes del workspace / statusbar, y `pointerup` persistiendo a `localStorage`.
- [x] **Step 3: Implementar límites ergonómicos (clamping)**
  Panel izquierdo (180–500px), panel derecho (240–650px), dejando mínimo 350px para el canvas; panel inferior (60px a `innerHeight - 150px`).
- [x] **Step 4: Soporte de doble clic para resetear**
  Evento `dblclick` en cada splitter que llama a `reset('left')`, `reset('right')` o `reset('bottom')`.
- [x] **Step 5: Accesibilidad por teclado y notificación de redibujado**
  Teclas ArrowLeft / ArrowRight / ArrowUp / ArrowDown con ajuste de 10px, y disparo de `window.dispatchEvent(new Event('resize'))` para actualizar el canvas.

---

### Task 3: Inserción de Elementos DOM y Registro en `index.html` y `desktop.html`

**Files:**
- Modify: `tools/jwplc-hmi-designer/poc/index.html`
- Modify: `tools/jwplc-hmi-designer/poc/desktop.html` (si requiere registrar script o metadatos)

**Interfaces:**
- Consumes: `designer-layout-resizer.js`
- Produces: Elementos `<div id="resizerLeft" class="layout-resizer layout-resizer-vertical layout-resizer-left"></div>`, `#resizerRight` y `#resizerBottom`.

- [x] **Step 1: Insertar splitters en el DOM de `index.html`**
  Colocar `#resizerLeft` y `#resizerRight` en `.workspace` (o dentro de `.editor-panel`) y `#resizerBottom` en el panel inferior `.code-panel`.
- [x] **Step 2: Registrar `designer-layout-resizer.js` en `index.html`**
  Añadir `<script src="./designer-layout-resizer.js"></script>` antes de `ux-foundation.js`.
- [x] **Step 3: Verificar que `desktop.html` cargue adecuadamente el script**
  Comprobar la inyección de scripts en `desktop.html` para asegurar paridad entre modo navegador y aplicación de escritorio Electron.

---

### Task 4: Tests Automatizados y Verificación

**Files:**
- Create: `tools/jwplc-hmi-designer/tests/layout-resizer.test.cjs`
- Modify: `tools/jwplc-hmi-designer/tests/run-all.cjs` (agregar a la suite de tests)

**Interfaces:**
- Consumes: `poc/designer-layout-resizer.js`
- Produces: Verificación en Node.js de:
  - Constantes de límites (min/max).
  - Cálculo de clamp para panel izquierdo, derecho e inferior.
  - Almacenamiento y recuperación en localStorage / mock.
  - Comportamiento de reset ante doble clic.

- [x] **Step 1: Escribir prueba unitaria `tests/layout-resizer.test.cjs`**
  Validar cálculo de dimensiones, clamping, claves de almacenamiento y reseteo.
- [x] **Step 2: Ejecutar `node tools/jwplc-hmi-designer/tests/layout-resizer.test.cjs` y verificar PASS**
- [x] **Step 3: Agregar la nueva prueba a `tests/run-all.cjs` y ejecutar la suite completa**
- [x] **Step 4: Comprobar que no hay regresiones en la suite general de pruebas**
