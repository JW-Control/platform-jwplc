# Design Doc: Sistema de Tokens de Diseño para Soporte Nativo de Temas (Claro y Oscuro) en JWPLC HMI Designer

**Fecha:** 2026-11-03  
**Estado:** Propuesta revisada tras Builder Check  
**Autor:** André / Antigravity  
**Ubicación:** `tools/jwplc-hmi-designer`  

---

## 1. Contexto e Intención

### 1.1 El Problema Actual
`tools/jwplc-hmi-designer` nació como un editor visual para pantallas TFT industriales ST7789 (320×170 px) de fondo oscuro. Por ello, el código base original de `poc/styles.css` y las plantillas de `poc/index.html` hardcodearon más de 485 colores hexadecimales oscuros (`#081018`, `#0d1720`, `#13222e`, `#213543`, `#8ea3b3`, `#dce9f1`, etc.).

Posteriormente, se introdujo `poc/designer-light-theme.css` como una capa reactiva de sobreescritura bajo el selector `body.theme-light`. A medida que el Designer evolucionó (añadiendo inspectores de figuras, pestañas Diseño/Técnico, matriz 3×3 de alineación, inspectores circulares, etc.), estos nuevos componentes se programaron con colores oscuros fijos y nunca se agregaron al archivo de sobreescritura.

Además, varios elementos (como las cabeceras `<h2>` de los inspectores en `index.html` y plantillas de `designer-bar.js`) incluyeron estilos en línea con especificidad absoluta (`style="color: #dce9f1;"`), provocando textos prácticamente invisibles en modo claro (blanco azulado sobre fondo blanco) y colisiones con selectores globales como `body.theme-light button`.

### 1.2 Objetivo Principal
Migrar la arquitectura de color de JWPLC HMI Designer a un sistema escalable basado en **Tokens de Diseño con Variables CSS** (`CSS Custom Properties`), de manera que cualquier componente actual y futuro soporte nativamente tanto el modo oscuro como el modo claro sin necesidad de mantener hojas de sobreescritura frágiles y redundantes.

---

## 2. Metas y No-Metas

### Metas (Goals)
1. **Tokens Semánticos Completos y Centralizados:** Declarar la paleta completa de tokens en `:root` (modo oscuro) y en `html.theme-light` (modo claro) al inicio de `poc/styles.css`, convirtiéndolo en la única fuente de la verdad para colores de tema.
2. **Refactorización de Clases del Inspector y Componentes Compartidos:** Sustituir los valores hexadecimales duros por variables CSS (`var(--...)`) en las áreas estructurales del Inspector: tarjetas visuales (`.bar-card`), pestañas principales (`.bar-main-tabs`, `.shape-main-tab`), subpestañas (`.bar-sub-tabs`, `.shape-sub-tab`), matrices de alineación (`.shape-align-matrix-*`), controles de geometría (`.geom-*`, `.transform-btn`) y títulos.
3. **Limpieza de Inline Styles en Inspector (`index.html` y `designer-bar.js`):** Eliminar los estilos en línea con colores rígidos (`#dce9f1`, `#0d1720`, `#8ea3b3`, etc.) en las cabeceras estáticas de `index.html` y en las plantillas dinámicas de `designer-bar.js`, reemplazándolos por clases semánticas o permitiendo que hereden `var(--text)`.
4. **Poda de Colisiones en `designer-light-theme.css`:** Ajustar o podar los selectores globales agresivos en `designer-light-theme.css` (como `body.theme-light button`, `body.theme-light .bar-card`) que sobreescribirían los nuevos tokens por especificidad `(0, 1, 1)` o `(0, 2, 0)`.
5. **Preservación Estricta del Modo Oscuro:** El modo oscuro actual debe mantener exactamente su estética industrial y contrastes sin ninguna alteración perceptible.
6. **Legibilidad y Contraste Industrial en Modo Claro:** El modo claro debe cumplir con WCAG 2.1 AA (contraste mínimo 4.5:1 para texto normal y 3:1 para controles activos/gráficos), eliminando cualquier texto fantasma o parche visual.
7. **Protección del Canvas TFT:** El viewport donde se simula la pantalla TFT ST7789 debe conservar su naturaleza de hardware (lienzo negro de 320×170 con píxeles RGB565) independientemente del tema de la aplicación de escritorio.
8. **Sincronización de `color-scheme` con Scrollbars:** Sincronizar dinámicamente `color-scheme: dark` / `color-scheme: light` en `:root` / `.theme-light` para que las barras de scroll nativas del navegador/Electron coincidan con el tema activo y no parpadeen en blanco sobre fondos oscuros.

### No-Metas (Non-Goals)
* No se modificará la generación de código C++ (`JWPLC_HMI_Generated.h`) ni el formato de guardado `.jwhmi`.
* No se cambiará la disposición ni ergonomía de los controles existentes.
* No se reescribirán estilos en subsistemas no relacionados (como flasheo de firmware o serial puente) en esta fase.
* No se añadirán dependencias externas, frameworks CSS (Tailwind, Bootstrap) ni preprocesadores (Sass). Se mantiene la filosofía de HTML/CSS/JS nativo sin bundler.

### Anti-Metas (Anti-Goals)
* Evitar parches visuales con `!important` generalizados.
* Evitar que la apariencia del modo oscuro sufra regresiones.

---

## 3. Enfoques Considerados y Selección

### Enfoque A: Parche reactivo masivo en `designer-light-theme.css`
* Agregar selectores `body.theme-light .bar-nav-header`, `body.theme-light .shape-main-tab`, etc., manteniendo los colores duros en `styles.css`.
* *Desventaja:* Mantiene la deuda técnica; cada nuevo widget o inspector requerirá duplicar código y volverá a fallar en modo claro si el programador lo olvida.

### Enfoque B: Migración a Tokens de Diseño Progresiva y Enfocada (Seleccionado)
* Establecer el contrato semántico de variables CSS en `:root` y en `.theme-light` dentro de `styles.css`. Migrar sistemáticamente toda la jerarquía del Inspector (figuras, barras, textos, píxeles, menús) y podar los selectores conflictivos de `designer-light-theme.css`.
* *Ventaja:* Resuelve el problema de raíz donde se manifiesta, sienta las bases escalables de tokens para toda la aplicación y minimiza el riesgo de regresión en pantallas complejas.

---

## 4. Especificación del Sistema de Tokens

### 4.1 Definición de Tokens Semánticos en `poc/styles.css`

```css
:root {
  color-scheme: dark;
  --bg: #081018;
  --panel: #0d1822;
  --panel-2: #101d28;
  --panel-3: #13222e;
  --panel-inset: #091218;
  --line: #243746;
  --line-soft: #1a2b38;
  --text: #e7eef5;
  --text-heading: #dce9f1;
  --muted: #8da2b3;
  --orange: #ff8a2a;
  --orange-soft: rgba(255, 138, 42, 0.18);
  --tab-bg: #13222e;
  --tab-border: #253948;
  --tab-text: #8ea3b3;
  --tab-active-text: #ff9a43;
  --cyan: #28dce8;
  --green: #43d17a;
  --warn: #ffb53d;
  --danger: #f45f65;
}

html.theme-light,
body.theme-light {
  color-scheme: light;
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

### 4.2 Cumplimiento con Modern Web Guidance (`dark-mode` & `color`)
* Inclusión de `<meta name="color-scheme" content="light dark">` en `<head>` de `index.html` y `desktop.html`.
* Declaración explícita de `color-scheme` contextual (`dark` por defecto, `light` en `.theme-light`).
* Garantía de ratios de contraste accesibles en modo claro (ej. textos sobre fondos `--orange-soft` usan `--tab-active-text` `#c2410c` logrando ratio > 4.5:1).

---

## 5. Mapeo de Componentes Afectados

1. **Cabeceras de Inspector (`poc/index.html` y `poc/designer-bar.js`):**
   * En `index.html`: Sustituir `style="color: #dce9f1; ..."` en `#shapeInspectorSection`, `#rawTextSection`, `#pixelMapSection`, `#colorToolSection` por la clase estándar `.inspector-heading` gobernada por `color: var(--text-heading);`.
   * En `designer-bar.js`: Quitar estilos duros `style="color:#dce9f1;"` y `style="color:#8ea3b3;"` en las plantillas generadas dinámicamente.
2. **Navegación de Pestañas (`poc/styles.css`):**
   * `.bar-nav-header`: `background: var(--panel-2); border-bottom: 1px solid var(--line);`
   * `.bar-main-tab, .shape-main-tab, .bar-sub-tab, .shape-sub-tab`:
     * Inactivo: `background: var(--tab-bg); border-color: var(--tab-border); color: var(--tab-text);`
     * Hover: `background: var(--panel-3); border-color: var(--line); color: var(--text);`
     * Activo (`.active`): `background: var(--orange-soft); border-color: var(--orange); color: var(--tab-active-text);`
3. **Tarjetas de Propiedades (`.bar-card`):**
   * `background: var(--panel-3); border-color: var(--line);`
   * Títulos `.bar-card-title`, `.bar-visual-group-title`: `color: var(--text);`
   * Subtítulos `.bar-card-section-title`, `.bar-field-subtitle`: `color: var(--muted);`
4. **Matriz de Alineación 3×3 (`.shape-align-matrix-*`):**
   * `.shape-align-matrix-row`: `background: var(--panel-2); border-color: var(--line);`
   * `.shape-align-matrix-3x3`: `background: var(--panel-inset); border-color: var(--line-soft);`
   * `.shape-matrix-btn`: `background: var(--panel); border-color: var(--line); color: var(--text);`
   * `.shape-matrix-legend-title`: `color: var(--text);`
   * `.shape-matrix-legend-desc`: `color: var(--muted);`
5. **Controles de Geometría e Inputs:**
   * `.geom-label`: `color: var(--muted);`
   * `.geom-input`: `background: var(--panel); border-color: var(--line); color: var(--text);`
   * `.transform-btn`: `background: var(--panel); border-color: var(--line); color: var(--text);`
6. **Resolución de Colisiones en `designer-light-theme.css`:**
   * Ajustar las reglas de `body.theme-light .bar-card`, `body.theme-light button` para respetar las variables definidas y evitar que sobreescriban con selectores de mayor especificidad.

---

## 6. Verificación y Pruebas
1. **Verificación visual interactiva:** Comprobar la conmutación de tema con el botón de sol/luna (`#themeToggleButton`) asegurando que los inspectores de Rectángulo, Línea, Círculo, Polígono, Texto RAW, PixelMap y Color se visualicen perfectamente nítidos y legibles en modo claro y en modo oscuro.
2. **Verificación de regresión en modo oscuro:** Comparar que ningún elemento en modo oscuro haya perdido su contraste ni estética original.
3. **Verificación de tests automatizados:** Ejecución de la suite `tests/run-all.cjs` para validar que codegen y APIs sigan al 100%.
