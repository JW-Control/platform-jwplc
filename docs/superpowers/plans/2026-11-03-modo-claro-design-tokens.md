# Sistema de Tokens de Diseño para Soporte Nativo de Temas (Claro y Oscuro) en JWPLC HMI Designer Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Implementar un sistema escalable de tokens de diseño con variables CSS (`--panel`, `--line`, `--text`, `--tab-*`, `--panel-inset`) para que JWPLC HMI Designer soporte nativamente tanto el modo claro como el modo oscuro en toda la jerarquía del Inspector y componentes compartidos.

**Architecture:** Centralización de variables semánticas en `poc/styles.css` tanto para `:root` (dark) como para `html.theme-light` (light). Refactorización de componentes del Inspector en `styles.css` para consumir tokens en lugar de valores hexadecimales hardcodeados. Limpieza de estilos en línea rígidos (`style="color: #dce9f1;"`) en `poc/index.html` y `poc/designer-bar.js`. Poda de selectores de sobreescritura conflictivos en `poc/designer-light-theme.css`.

**Tech Stack:** Vanilla HTML5, CSS3 Custom Properties (`var(--...)`), Vanilla JavaScript, Electron (Chromium engine).

**Spec:** `docs/superpowers/specs/2026-11-03-modo-claro-design-tokens-design.md`

## Global Constraints

- Preserve 100% of the existing dark mode appearance, contrasts, and behavior.
- The ST7789 TFT canvas simulation (320×170 px) must remain a hardware screen with black background (`#000000`) regardless of the active UI theme.
- No external CSS frameworks, bundlers, or preprocessors (maintain pure vanilla HTML/CSS/JS).
- No modifications to C++ code generation logic (`JWPLC_HMI_Generated.h`) or `.jwhmi` schema.
- High industrial legibility in light mode meeting WCAG 2.1 AA standards (minimum 4.5:1 text contrast).

## Review Focus

1. **Light Mode Inspector Heading Contrast:** Headings in `#shapeInspectorSection`, `#rawTextSection`, `#pixelMapSection` must be dark (`#0f172a`) in light mode and light (`#dce9f1`) in dark mode, not faint white-on-white.
2. **Tab Bar Container & Buttons Cohesion:** In light mode, `.bar-nav-header` must have a light surface (`#f8fafc`) and tabs (`Diseño`, `Técnico`, `Forma`, `Apariencia`, `Ubicación`) must not display dark rectangular borders or unstyled white blocks.
3. **Alignment Matrix 3×3 Surface:** `.shape-align-matrix-row` and `.shape-align-matrix-3x3` must adapt cleanly to light background with crisp readable arrows and labels.
4. **Scrollbars Theme Consistency:** Scrollbars must match the active theme and not flash white against dark canvas/panels.
5. **No Regression on Dark Mode:** In default dark mode, every single inspector card, tab, button, and text label must look identical to the original dark palette.

---

### Task 1: Centralización de Tokens Semánticos y `color-scheme` en `styles.css` y encabezados HTML

**Files:**
- Modify: `tools/jwplc-hmi-designer/poc/styles.css:1-30`
- Modify: `tools/jwplc-hmi-designer/poc/index.html:1-12`
- Modify: `tools/jwplc-hmi-designer/poc/desktop.html:1-10`

**Interfaces:**
- Consumes: Existing `:root` variables in `styles.css`
- Produces: Complete tokens `--panel-inset`, `--text-heading`, `--tab-bg`, `--tab-border`, `--tab-text`, `--tab-active-text` available globally in both dark and light scopes

- [ ] **Step 1: Declarar `<meta name="color-scheme" content="light dark">` en `poc/index.html` y `poc/desktop.html`**
  Add `<meta name="color-scheme" content="light dark">` in the `<head>` of both files right after `<meta name="viewport" ... />`.

- [ ] **Step 2: Definir el bloque de tokens extendido para `:root` en `poc/styles.css`**
  Add tokens `--panel-inset: #091218;`, `--text-heading: #dce9f1;`, `--tab-bg: #13222e;`, `--tab-border: #253948;`, `--tab-text: #8ea3b3;`, `--tab-active-text: #ff9a43;` in `:root`.

- [ ] **Step 3: Definir el bloque de tokens para `html.theme-light, body.theme-light` en `poc/styles.css`**
  Insert the light theme tokens block directly below `:root` in `poc/styles.css`:
  ```css
  html.theme-light,
  body.theme-light {
    color-scheme: light;
    background: #f1f5f9;
    color: #0f172a;
    --bg: #f1f5f9;
    --panel: #ffffff;
    --panel-2: #f8fafc;
    --panel-3: #f1f5f9;
    --panel-inset: #e2e8f0;
    --line: #cbd5e1;
    --line-soft: #e2e8f0;
    --text: #0f172a;
    --text-heading: #0f172a;
    --muted: #64748b;
    --orange: #ea580c;
    --orange-soft: rgba(234, 88, 12, 0.12);
    --tab-bg: #ffffff;
    --tab-border: #cbd5e1;
    --tab-text: #475569;
    --tab-active-text: #c2410c;
    --cyan: #0284c7;
    --green: #16a34a;
    --warn: #d97706;
    --danger: #dc2626;
  }
  ```

- [ ] **Step 4: Verificar que el archivo cargue sin errores sintácticos de CSS**

---

### Task 2: Refactorización de Clases del Inspector a Tokens en `poc/styles.css`

**Files:**
- Modify: `tools/jwplc-hmi-designer/poc/styles.css:1990-2055, 2225-2420`

**Interfaces:**
- Consumes: Design tokens from Task 1
- Produces: Theme-adaptive classes `.bar-nav-header`, `.bar-main-tab`, `.shape-main-tab`, `.bar-sub-tab`, `.shape-sub-tab`, `.bar-card`, `.shape-align-matrix-*`

- [ ] **Step 1: Migrar `.bar-nav-header` en `styles.css`**
  Replace hardcoded `#0d1720` and `#1e313f` with:
  ```css
  .bar-nav-header {
    position: sticky;
    top: 0;
    z-index: 10;
    background: var(--panel-2);
    border-bottom: 1px solid var(--line);
    margin: 0 -10px 10px;
    padding: 0 10px;
  }
  ```

- [ ] **Step 2: Migrar pestañas y subpestañas (`.bar-main-tab`, `.shape-main-tab`, `.bar-sub-tab`, `.shape-sub-tab`)**
  Update background, border, and color properties to use:
  - Default/Inactive: `background: var(--tab-bg); border: 1px solid var(--tab-border); color: var(--tab-text);`
  - Hover: `background: var(--panel-3); border-color: var(--line); color: var(--text);`
  - Active: `background: var(--orange-soft); border-color: var(--orange); color: var(--tab-active-text);`

- [ ] **Step 3: Migrar `.bar-card` y títulos de sección**
  - `.bar-card`: `background: var(--panel-3); border: 1px solid var(--line);`
  - `.bar-card-section-title`: `color: var(--muted);`
  - `.bar-card-title`: `color: var(--text-heading);`
  - `.bar-field-subtitle`: `color: var(--muted);`
  - `.bar-visual-group-title`: `color: var(--muted);`

- [ ] **Step 4: Migrar la matriz de alineación 3×3 (`.shape-align-matrix-*`)**
  - `.shape-align-matrix-row`: `background: var(--panel-2); border: 1px solid var(--line);`
  - `.shape-align-matrix-3x3`: `background: var(--panel-inset); border: 1px solid var(--line-soft);`
  - `.shape-matrix-btn`: `background: var(--panel); border: 1px solid var(--line); color: var(--text);`
  - `.shape-matrix-legend-title`: `color: var(--text-heading);`
  - `.shape-matrix-legend-desc`: `color: var(--muted);`

- [ ] **Step 5: Migrar controles geométricos e inputs (`.geom-label`, `.geom-input`, `.transform-btn`)**
  - `.geom-label`: `color: var(--muted);`
  - `.geom-input`: `background: var(--panel); border: 1px solid var(--line); color: var(--text);`
  - `.transform-btn`: `background: var(--panel); border: 1px solid var(--line); color: var(--text);`

---

### Task 3: Limpieza de Estilos en Línea y Cabeceras en `poc/index.html` y `poc/designer-bar.js`

**Files:**
- Modify: `tools/jwplc-hmi-designer/poc/index.html:530-545, 1430-1440, 1655-1665, 1925-1935`
- Modify: `tools/jwplc-hmi-designer/poc/designer-bar.js:220-270, 755-765`
- Modify: `tools/jwplc-hmi-designer/poc/styles.css` (add `.inspector-heading` rule)

**Interfaces:**
- Consumes: `--text-heading` token
- Produces: Clean semantic HTML headers without inline color overrides

- [ ] **Step 1: Agregar regla de estilo `.inspector-heading` en `styles.css`**
  ```css
  .inspector-title h2,
  .inspector-heading {
    font-size: 13px;
    color: var(--text-heading);
    font-weight: 700;
    display: flex;
    align-items: center;
    gap: 6px;
    margin: 0;
  }
  ```

- [ ] **Step 2: Limpiar `style="color: #dce9f1;"` en las cabeceras de `index.html`**
  - `#shapeInspectorSection`: Reemplazar `<h2 style="font-size: 13px; color: #dce9f1; font-weight: 700; display: flex; align-items: center; gap: 6px;">` por `<h2 class="inspector-heading">`.
  - `#rawTextSection`: Reemplazar el `<h2>` con estilo en línea por `<h2 class="inspector-heading">`.
  - `#pixelMapSection`: Reemplazar el `<h2>` con estilo en línea por `<h2 class="inspector-heading">`.
  - `#colorToolSection`: Reemplazar el `<h2>` con estilo en línea por `<h2 class="inspector-heading">`.

- [ ] **Step 3: Limpiar estilos rígidos en `designer-bar.js`**
  In dynamic templates generated in `poc/designer-bar.js`:
  - Replace `style="color:#dce9f1;font-size:11px;"` with `style="color:var(--text-heading);font-size:11px;"`.
  - Replace `style="color:#8ea3b3;"` with `style="color:var(--muted);"`.
  - Replace hardcoded background `#0d1720` or `#1e313f` with `var(--panel-2)` and `var(--line)`.

---

### Task 4: Poda y Armonización en `poc/designer-light-theme.css`

**Files:**
- Modify: `tools/jwplc-hmi-designer/poc/designer-light-theme.css:28-55, 475-495`

**Interfaces:**
- Consumes: Tokens defined in Task 1 and classes refactored in Task 2
- Produces: Clean coexistence without overriding tokenized components

- [ ] **Step 1: Excluir pestañas de inspector del selector genérico `body.theme-light button`**
  In `designer-light-theme.css`, ensure generic button styles don't conflict with `.bar-main-tab`, `.shape-main-tab`, `.bar-sub-tab`, `.shape-sub-tab`, `.shape-matrix-btn`, `.bar-visual-btn` by using `:not()` or letting token classes control them.

- [ ] **Step 2: Ajustar `body.theme-light .bar-card` en `designer-light-theme.css`**
  Remove redundant/clashing overrides for `.bar-card` and `.bar-inspector-wrap` so they defer to `var(--panel-3)` and `var(--line)`.

- [ ] **Step 3: Asegurar la correcta estilización de los botones visuales y switches**
  Ensure `.bar-visual-btn.active` and toggle switches display legible contrast with `--orange` and `--orange-soft`.

---

### Task 5: Verificación Integral y Suite de Pruebas

**Files:**
- Test: `tools/jwplc-hmi-designer/tests/run-all.cjs`
- Inspect: Live visual behavior in Electron/Browser

- [ ] **Step 1: Ejecutar la suite de pruebas unitarias de node**
  Run: `node tools/jwplc-hmi-designer/tests/codegen-core.test.cjs`
  Run: `node tools/jwplc-hmi-designer/tests/icon-catalog.test.cjs`
  Run: `node tools/jwplc-hmi-designer/tests/runtime-core.test.cjs`
  Expected: All PASS.

- [ ] **Step 2: Verificar selectores CSS y ausencia de tokens no resueltos**
  Perform grep across `styles.css`, `designer-light-theme.css`, `index.html` to confirm all newly introduced tokens resolve properly and there are no syntax errors or unclosed blocks.

- [ ] **Step 3: Verificar visualmente la conmutación entre modo claro y oscuro**
  Confirm:
  - "Inspector — LINE 2" (and other shapes/tools) is dark `#0f172a` and 100% crisp on light mode.
  - The tab bar (`Diseño / Técnico` and `Forma / Apariencia / Ubicación`) looks unified and elegant on light mode.
  - The 3×3 matrix has a light recessed background with clear arrows and readable descriptions.
  - Dark mode remains 100% faithful to the original dark industrial aesthetic.
