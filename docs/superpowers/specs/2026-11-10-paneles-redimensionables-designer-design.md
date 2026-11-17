# Diseño: Paneles y Barra Inferior Redimensionables en JWPLC HMI Designer

**Fecha:** 2026-11-10  
**Autor:** Antigravity (asistente) & André  
**Estado:** Aprobado  
**Ámbito:** `tools/jwplc-hmi-designer`  

## 1. Intención y Objetivo

Permitir que el usuario en JWPLC HMI Designer pueda ajustar interactivamente mediante arrastre de ratón/cursor (**drag-to-resize**) las dimensiones de:
1. El **panel lateral izquierdo** (vista previa y árbol de objetos).
2. El **panel lateral derecho** (inspector de campos y propiedades).
3. La **barra inferior** (panel de código generado C++ y estado/problemas).

Actualmente estos componentes poseen dimensiones fijas (o semivariables según media-queries predefinidas), lo cual dificulta la lectura cómoda de código C++ largo, la inspección de listas extensas de propiedades o el aprovechamiento de monitores ultrapanorámicos o compactos.

## 2. Metas y No Metas

### Metas (Goals)
* Introducir separadores/splitters arrastrables accesibles y visualmente limpios:
  * Separador vertical izquierdo (`#resizerLeft`) entre el panel izquierdo y el editor.
  * Separador vertical derecho (`#resizerRight`) entre el editor y el panel derecho.
  * Separador horizontal inferior (`#resizerBottom`) sobre el panel inferior de código.
* Control fluido con `PointerEvents` (`setPointerCapture`) para evitar pérdidas de foco al salir de la barra.
* Persistencia automática de las dimensiones elegidas por el usuario en `localStorage`.
* Restablecimiento rápido a los valores por defecto mediante doble clic (`dblclick`) en cualquier separador.
* Soporte de accesibilidad por teclado (`role="separator"`, `tabindex="0"`, `aria-valuenow`, flechas del teclado).
* Redibujado/ajuste automático del viewport del lienzo (canvas) al terminar o durante el arrastre.

### No Metas (Non-Goals)
* No modificar la lógica de serialización de proyectos (`.jwhmi`).
* No alterar el comportamiento de `layout-compact` (modo móvil/carril donde los paneles se convierten en drawers desplegables; en este modo los splitters se ocultan).
* No alterar la lógica interna del codegen ni de los inspectores de campos.

## 3. Enfoques Considerados

### Enfoque 1: Columnas y filas adicionales en el CSS Grid
Añadir elementos `<div class="splitter">` como hijos directos del grid en `body` y `.workspace`, asignándoles columnas y filas de 4px.
* *Desventaja:* Rompe la compatibilidad con reglas existentes de `desktop-responsive.css` que asumen 3 columnas en `.workspace` y 4 filas en `body` (especialmente en modos `layout-medium` y `layout-compact`).

### Enfoque 2: Separadores flotantes/anclados por borde con variables CSS (Seleccionado)
Posicionar los separadores como elementos interactivos anclados en los bordes de los paneles (`position: absolute` en el borde relevante con `z-index: 100`, o elementos superpuestos con hitbox ergonómico de 6px y línea visible de 2px en hover/drag).
* Las dimensiones se gobiernan mediante variables CSS (`--left-panel-width`, `--right-panel-width`, `--bottom-panel-height`).
* El grid de `body` y `.workspace` utiliza estas variables con valores por defecto de fallback (`var(--left-panel-width, 280px)`).
* *Ventajas:* Cero disrupción sobre los contenedores existentes, integración inmediata con temas claro/oscuro, funcionamiento sin fricción en Electron y navegador.

## 4. Especificación Técnica

### 4.1 Variables CSS y Rangos
* **Panel Izquierdo (`--left-panel-width`):**
  * Por defecto: 280px.
  * Mínimo: 180px.
  * Máximo: 500px (o hasta dejar 350px al editor central).
* **Panel Derecho (`--right-panel-width`):**
  * Por defecto: 360px.
  * Mínimo: 240px.
  * Máximo: 650px (o hasta dejar 350px al editor central).
* **Panel Inferior (`--bottom-panel-height`):**
  * Por defecto: 150px (wide) / 164px (base).
  * Mínimo: 60px.
  * Máximo: `window.innerHeight - 200px` (hasta 600px).
  * Si está colapsado (`body.bottom-collapsed`), la altura de 38px se respeta.

### 4.2 Persistencia (`localStorage`)
* Claves:
  * `jwplc-hmi-layout.leftPanelWidth`
  * `jwplc-hmi-layout.rightPanelWidth`
  * `jwplc-hmi-layout.bottomPanelHeight`
* Al cargar la app, se leen y aplican en `:root`. Si se hace doble clic en el separador, se eliminan estas claves y se vuelve al valor por defecto.

### 4.3 Módulo JS
* Archivo: `poc/designer-layout-resizer.js`
* API expuesta: `window.JWPLCHMILayoutResizer` con métodos `init()`, `reset()`, `getSizes()`, `setSizes({ left, right, bottom })`.
* Registrado en `poc/index.html` y compatible con el empaquetado de `poc/desktop.html`.
