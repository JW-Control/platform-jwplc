---
name: hmi-designer-ux
description: Reglas de UX/UI y de paridad del JWPLC HMI Designer (tools/jwplc-hmi-designer). Usar antes de diseñar o cambiar cualquier inspector, panel, mini-modelo de Apariencia, objeto nuevo del lienzo o elemento visual del Designer, y antes de entregar capturas de esos cambios. También al revisar una propuesta de diseño (prompt de otro agente, mockup) para el Designer.
---

# JWPLC HMI Designer — UX/UI de la casa

El Designer edita pantallas para una TFT ST7789 de 320×170 RGB565 con una botonera de 6 teclas
(◀ ▲ ▶ ▼ OK ESC) en un PLC industrial ESP32. Todo lo que se ve en el lienzo debe ser lo que
dibuja el firmware (`JWPLC_Display` / `JWPLC_UI` / `JWPLC_TFT`). Lee también
`tools/jwplc-hmi-designer/CLAUDE.md` (arquitectura y orden de carga) y el `CLAUDE.md` raíz.

## 1. Principios (no negociables)

1. **Lo que se ve es lo que sale en la TFT.** Lienzo, mini-modelos y vista previa usan el mismo
   pintor que el firmware (`app.js:drawField`, `designer-menu.js:paint`, `designer-pages.js:paintNavBar`).
   Nunca un dibujo "aproximado" sólo para el Designer.
2. **No ofrecer lo que el firmware no dibuja.** Si una opción no existe en `JWPLC_UI`, va
   deshabilitada con `.insp-needs-fw` y el codegen cae a lo soportado. Antes de emitir una
   llamada C++, verificar que existe en `JWPLC/2.1.0/libraries/*/src/*.h`.
3. **Reutilizar antes de crear.** Una capacidad nueva se agrega como opción de un objeto
   existente (VALUE, BOOL, BAR, MENU) salvo que aporte algo que esos no resuelvan.
4. **La lógica del proceso y la seguridad viven en el PLC.** La HMI muestra estados y envía
   solicitudes; no temporiza, no acumula, no decide desenergizar.
5. **Dirty refresh y SPI compartido.** Lo estático se dibuja al entrar a la página; en cada tick
   sólo setters. Nada que obligue a redibujar la página completa salvo un cambio de vista.

## 2. Patrón de inspector (Propuesta 2)

- Pestañas principales **Diseño · Datos · Técnico**; Diseño con **Forma · Contenido · Apariencia · Ubicación**.
- Opciones enumerables como **tarjetas visuales clicables** (`bar-card`, `bar-visual-btn`,
  `insp-chip`), con un mini-dibujo SVG de lo que hacen. Nada de `<select>` ni celdas numéricas
  para elegir entre opciones conocidas.
- **Mostrar sólo los controles de la variante elegida** (ocultar con `[hidden]`, no deshabilitar
  en masa).
- Orden de los controles = orden en que se ve en la pantalla (arriba→abajo, izquierda→derecha):
  etiqueta antes que valor, título antes que ítems.
- Inspectores de campos escriben en los inputs viejos y disparan `input`/`change` (historial);
  `input` = vista previa en vivo sin historial, `change` = un paso de historial.
- Textos de la UI en **español**, cortos y en el vocabulario del operador/integrador (no del código).

## 3. Apariencia (colores)

- Mini-modelo pintado con el pintor real, escalado a entero (×1–×3); si el campo es ancho, la
  leyenda va debajo y el modelo a ×2 como mínimo.
- Leyenda: una fila por propiedad con su cuadro de color (`F` fondo, `T` texto, `V` valor, `M`
  marco) y rótulo; la propiedad activa se marca en el cuadro y en la zona del modelo.
- **Indicadores del editor fuera de los píxeles de la HMI** y **nunca con un color de la paleta
  RGB565** (el naranja de acento #ff9a43 ≈ `0xFD20`, el color de edición del MENU). Preferir doble
  trazo (negro exterior + acento) o "hormigas", separado 1–2 px de la zona.
- Zonas según el firmware, no según la intuición: la **unidad usa el color de la etiqueta**;
  en BOOL por estado el fondo del estado pinta **todo el campo**; etiqueta y marco son comunes a
  FALSE/TRUE.
- **Color: editor fijo, nunca el selector nativo** (`poc/designer-color-editor.js`). El selector
  nativo flota donde quiere el navegador, tapa las filas de abajo y obliga a cerrarlo para elegir otra
  fila. El editor va dentro de la tarjeta, bajo la leyenda (cuadro S/V, tono, HEX, RGB565 y paleta
  rápida); se cambia de fila sin cerrar nada. En vivo ~25/s mientras se arrastra, historial al soltar.
- Filas de la leyenda: **texto a la izquierda, cuadros a la derecha**, y toda la fila es clicable
  (el texto equivale a su cuadro; con dos cuadros elige el primero salvo que el activo sea de esa fila).
- Implementado en `poc/designer-zone-indicator.js` (A1): contorno de doble trazo, ▶ en un canal
  de 12 px a la izquierda y banda en la mini-regla de arriba; la fila activa de la leyenda va
  resaltada. Sin líneas que crucen el inspector. Cualquier mini-modelo nuevo (BAR, figuras) usa
  ese mismo componente: `draw(body, canvas, rects)` con la clase `zone-host` en el contenedor.
- Análisis y criterios: `docs/v2.1.0-alpha.15/A15_7_APARIENCIA_A1_ANALISIS_20261018.md`.

## 4. Botonera y navegación (no romper)

- IDLE → HMI según *Proyecto · Pantalla y arranque* (`designer-idle.js`).
- Con varias páginas: selector NN/TT negro (◀ ▶ página, OK entra) → contenido NN/TT blanco (5
  teclas del contenido). ESC retrocede: edición → submenú → raíz → selector → IDLE.
- Barra de pestañas (todos los estilos salvo «Contador NN/TT»): la pestaña de la página actual se
  distingue en los dos modos. Posicionada = fondo de color (recorriendo con ◀ ▶) y seleccionada =
  fondo blanco con doble marco hacia adentro (dentro, tras OK). El firmware la redibuja sola al
  cambiar de modo. La tecla que despierta la IDLE no actúa además sobre el selector ni sobre el MENU.
- Volver a la IDLE: ESC, por inactividad (cuenta en cualquier sección) o nunca (ítem de MENU
  «Ir al inicio» o `JWPLC_Display.goIdle()`). No reintroducir "volver con otra tecla".
- Un componente que necesite teclas declara su dueño y consume la pulsación
  (`JWPLC_Buttons.clearPendingInput()`); nunca dos dueños para la misma tecla.

## 5. TFT y tipografía

- 320×170, RGB565. GLCD 5×7 (celda 6×8 por escala) y FreeFonts vía `JWPLC_TFT`.
- Textos para la TFT en ASCII mientras el Designer no traduzca a cp437 (tildes → aviso).
- Márgenes de campo: arriba/izquierda 3 px fijos; derecha/abajo `max(3, escala + 1)`.

## 6. Antes de dar por terminado un cambio visual

1. `for f in tools/jwplc-hmi-designer/poc/*.js; do node --check "$f"; done`
2. `node tools/jwplc-hmi-designer/tests/run-all.cjs` (todos PASS). Si cambió el codegen:
   `codegen-golden.cjs --update` sólo si el cambio es intencional, revisando el diff.
3. **Capturas reales** con Playwright (Chromium headless, `desktop.html`) de cada variante
   tocada; mirarlas antes de entregar (no basta con que el test pase). Los colores se prueban con
   el editor fijo (`[data-ce-hex]`, `[data-ce-sv]`, pastillas `data-*-palette`); no hay selector nativo.
4. Subir `?v=` y `CACHE_NAME` (ver `tools/jwplc-hmi-designer/CLAUDE.md`).
5. Si tocaste firmware: compilar ESP32 (`tools/alpha15/regenerate_libJWPLC_TFT.sh compile <sketch>`)
   y, si aplica, la paridad en host (`tests/menu-host-sim.cjs`).
6. Documentar en `docs/v2.1.0-alpha.15/` (español, flags `[visto]/[ejecutado]/[inferido]/[no verificado]`).

## 7. Revisar una propuesta de diseño ajena

Contrastar cada afirmación con el código antes de aplicarla (qué color usa realmente cada parte,
qué opciones existen en el firmware, dónde queda espacio en 320×170). Entregar las correcciones
como lista, con referencia `archivo:línea`, y un prompt/plan corregido; esperar aprobación si el
cambio es grande.
