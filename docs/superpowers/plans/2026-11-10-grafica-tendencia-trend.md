# Plan de Implementación: Widget de Gráfica de Tendencia (`TREND`) con Muestreo Configurable y Soporte MicroSD

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Implementar en `tools/jwplc-hmi-designer` (y sincronizar con el firmware `JWPLC_UI_Widgets` / `JWPLC_Display`) un widget de campo flexible **`TREND` (Gráfica de Tendencia)** con tiempo de muestreo configurable (muestras por segundo / periodo en ms), auto-escala dinámica de 3 guías, borrado diferencial sin parpadeo y arquitectura de almacenamiento y consulta histórica en MicroSD sin saturar la RAM.

**Architecture:** 
- En el Designer (`spec.cjs`, `designer-codegen-core.js`, `designer-trend-core.js`, `designer-trend.js`, `app.js`), registrar el tipo de campo `TREND` con dimensiones rectangulares libres, selector de frecuencia de muestreo (10 Hz a 60 s), auto-escala inteligente basada en el algoritmo del sensor COD, trazo de 1 o 2 px, y opciones de registro en MicroSD.
- En el Inspector visual, construir una interfaz táctil por chips (Diseño, Datos, Apariencia) con tokens semánticos CSS para soporte automático de modo claro y oscuro, y presets en 1 clic ("Pantalla Completa" y "Recuadro Compacto").
- En el firmware C++ (`JWPLC_UI_Widgets`), ampliar `JWPLC_UITrend` para soportar periodo de muestreo configurable, auto-escala dinámica con 3 guías horizontales y la función de consulta histórica por diezmado en streaming desde la SD en un buffer temporal de 1 KB.

**Tech Stack:** Vanilla JavaScript (ES6 / CommonJS UMD), CSS3 Custom Properties (Tokens de Diseño), HTML5 Canvas 2D, C++11 (ESP32 / Arduino / TFT_eSPI / SD).

**Spec:** [`docs/superpowers/specs/2026-11-10-grafica-tendencia-trend-design.md`](file:///C:/Users/Andre/Documents/GitHub/platform-jwplc/docs/superpowers/specs/2026-11-10-grafica-tendencia-trend-design.md)

---

## Global Constraints

- Cero dependencias externas nuevas en Node.js ni bundlers frontend (mantener JavaScript y CSS vanilla).
- Respetar la regla de temas: prohibido usar colores hexadecimales rígidos en el inspector; usar tokens semánticos (`var(--tab-bg)`, `var(--line)`, `var(--text)`, `var(--orange)`, etc.).
- El lienzo ST7789 (`#tftCanvas`, 320×170 px) permanece estrictamente en `#000000` en ambos temas.
- La consulta histórica en MicroSD nunca debe cargar más de 1 KB a la vez en DRAM ni provocar desbordamiento de pila.
- El header C++ generado debe ser compatible con la regla del proyecto: sin `::`, `*` ni `&` expuestos en el sketch del usuario.

---

## Review Focus

1. **Auto-escala con datos planos:** Si la variable medida permanece constante (ej. temperatura fija en 25.0 °C), el algoritmo `calculateGraphScale` debe expandir un margen artificial ($\pm 5\%$) sin provocar divisiones por cero ni artefactos gráficos.
2. **Desborde de ventana visible:** Si la cantidad de puntos en vivo supera `visibleSamples`, el buffer circular debe descartar la muestra más antigua sin fugas de memoria en RAM.
3. **Escritura y lectura segura en MicroSD:** El volcado periódico a la SD y la consulta histórica deben respetar el semáforo/mutex del bus SPI de `JWPLC_Display` para no colisionar con la pantalla TFT.
4. **Persistencia `.jwhmi` e Historial (Undo/Redo):** Modificar el periodo de muestreo, la escala o el color de la curva debe registrar un paso atómico en el historial y serializarse íntegro al guardar y abrir el proyecto.
5. **Paridad de Codegen:** La estructura emitida en `JWPLC_HMI_Generated.h` debe coincidir byte a byte con las pruebas unitarias y compilar limpiamente contra `JWPLC_UI_Widgets.h`.

---

### Task 1: Modelo de Datos, Esquema y CLI (`spec.cjs`, `validate.cjs`, `jwhmi.schema.json`)

**Files:**
- Modify: `tools/jwplc-hmi-designer/cli/lib/spec.cjs`
- Modify: `tools/jwplc-hmi-designer/cli/lib/validate.cjs`
- Modify: `tools/jwplc-hmi-designer/schema/jwhmi.schema.json`
- Modify: `tools/jwplc-hmi-designer/agent/REFERENCIA_JWHMI.md`
- Test: `tools/jwplc-hmi-designer/tests/trend-schema.test.cjs` (nuevo)

- [ ] **Paso 1.1:** Escribir la prueba unitaria `tests/trend-schema.test.cjs` que valide la definición, valores por omisión y validación de un campo `TREND` (`sampleIntervalMs`, `scaleMode`, `visibleSamples`, `enableSD`, etc.).
- [ ] **Paso 1.2:** Ejecutar la prueba y verificar que falle:
  `node tools/jwplc-hmi-designer/tests/trend-schema.test.cjs`
- [ ] **Paso 1.3:** En `cli/lib/spec.cjs`, agregar la especificación oficial de `TREND` con sus campos, tipos, defaults y enumeraciones (`scaleMode`: `'AUTO'`, `'MANUAL'`).
- [ ] **Paso 1.4:** En `cli/lib/validate.cjs`, agregar las reglas de validación para `TREND`.
- [ ] **Paso 1.5:** Regenerar el esquema y la documentación de referencia ejecutando:
  `node tools/jwplc-hmi-designer/cli/jwhmi.cjs schema`
- [ ] **Paso 1.6:** Re-ejecutar la prueba unitaria `trend-schema.test.cjs` y verificar que pase al 100%.

---

### Task 2: Motor de Geometría y Algoritmo de Auto-Escala (`designer-trend-core.js`)

**Files:**
- Create: `tools/jwplc-hmi-designer/poc/designer-trend-core.js`
- Test: `tools/jwplc-hmi-designer/tests/trend-core.test.cjs` (nuevo)

- [ ] **Paso 2.1:** Escribir la prueba unitaria `tests/trend-core.test.cjs` probando:
  - Función pura `calculateGraphScale(minVal, maxVal, marginPx, totalHeight)`:
    - Caso normal (ej. 20.0 a 65.5).
    - Caso plano (ej. 30.0 a 30.0).
    - Margen de resguardo para que la curva nunca toque la guía superior ni inferior.
  - Función de diezmado para submuestreo de datos históricos (decimation de N puntos a W píxeles).
- [ ] **Paso 2.2:** Ejecutar la prueba y verificar que falle:
  `node tools/jwplc-hmi-designer/tests/trend-core.test.cjs`
- [ ] **Paso 2.3:** Implementar en `poc/designer-trend-core.js` (con soporte UMD / Node / Browser):
  - `calculateGraphScale` y `axisDecimals`.
  - Cálculo de geometría de ejes: reserva de margen izquierdo para números (35 a 58 px según decimales) y margen inferior para eje de tiempo.
  - Generador de datos sintéticos para vista previa en el lienzo.
- [ ] **Paso 2.4:** Re-ejecutar `trend-core.test.cjs` y verificar que pase al 100%.

---

### Task 3: Generación de Código C++ (`designer-codegen-core.js`)

**Files:**
- Modify: `tools/jwplc-hmi-designer/poc/designer-codegen-core.js`
- Test: `tools/jwplc-hmi-designer/tests/trend-codegen.test.cjs` (nuevo)

- [ ] **Paso 3.1:** Escribir prueba unitaria `tests/trend-codegen.test.cjs` verificando:
  - Generación de estructura de configuración `JWPLC_UITrendConfig`.
  - Instanciación de objeto `JWPLC_UITrend` con los parámetros correspondientes (`sampleIntervalMs`, escala, colores en RGB565).
  - Emisión de la llamada de refresco en `jwplcUIUpdate()`: `jwplcTrendPush(trend_id, variable)`.
- [ ] **Paso 3.2:** Ejecutar la prueba y verificar que falle.
- [ ] **Paso 3.3:** Implementar en `designer-codegen-core.js` el bloque `trendFieldBlock` y los helpers de codegen necesarios.
- [ ] **Paso 3.4:** Re-ejecutar `trend-codegen.test.cjs` y verificar que pase al 100%.

---

### Task 4: Motor de Renderizado en el Lienzo (`poc/app.js`)

**Files:**
- Modify: `tools/jwplc-hmi-designer/poc/app.js`
- Test: Verificación visual en canvas y prueba sintáctica con `node --check`.

- [ ] **Paso 4.1:** En `app.js:computeFieldGeometry(field)`:
  - Agregar soporte para `field.type === 'TREND'`: calcular caja del campo, área útil de trazado (`plotX`, `plotY`, `plotW`, `plotH`) descontando márgenes de ejes Y y X si están activos.
- [ ] **Paso 4.2:** En `app.js:drawField(field)`:
  - Implementar trazado de `TREND`:
    - Marco y fondo de la gráfica.
    - Eje Y con las 3 guías horizontales tenues y etiquetas de valor (`scale.high`, `scale.mid`, `scale.low`).
    - Eje X al pie con marcas de tiempo (`0s` a izquierda y rango a derecha).
    - Polilínea de la curva con grosor de 1 px o 2 px (`lineWidth`).
    - Cabecera compacta con título (`title`), valor actual y unidad (`unit`) si el tamaño lo permite.
- [ ] **Paso 4.3:** Validar sintaxis con `node --check tools/jwplc-hmi-designer/poc/app.js`.

---

### Task 5: Inspector Visual de Tendencia y Barra de Herramientas (`designer-trend.js`, `index.html`)

**Files:**
- Create: `tools/jwplc-hmi-designer/poc/designer-trend.js`
- Modify: `tools/jwplc-hmi-designer/poc/index.html`
- Modify: `tools/jwplc-hmi-designer/poc/ux-foundation.js`
- Modify: `tools/jwplc-hmi-designer/poc/styles.css`
- Test: `tools/jwplc-hmi-designer/tests/trend-inspector.test.cjs` (nuevo)

- [ ] **Paso 5.1:** En `index.html`, agregar la opción "Gráfica de Tendencia" en el menú desplegable de variables de la barra de herramientas (`#variableDropdownMenu`).
- [ ] **Paso 5.2:** En `styles.css`, añadir clases auxiliares para los chips de frecuencia de muestreo y opciones de tendencia basadas en tokens CSS (`--tab-bg`, `--orange`, etc.).
- [ ] **Paso 5.3:** Implementar `poc/designer-trend.js`:
  - Pestaña **Diseño**:
    - Forma: Inputs de ancho/alto, presets rápidos (`[Pantalla Completa]`, `[Recuadro Compacto]`), grosor de trazo (`1 px` / `2 px`), toggles de ejes.
    - Contenido: Título, unidad, selector de decimales (`0..3`).
    - Apariencia: Editor de color grande (`JWPLCColorEditor`) para curva, guías, fondo y texto.
  - Pestaña **Datos**:
    - Selector de variable PLC vinculada.
    - Modo de escala: chips `[Auto-escala Dinámica]` vs `[Rango Fijo (Mín-Máx)]`.
    - Selector táctil de frecuencia de muestreo por chips: `[10 Hz] [5 Hz] [2 Hz] [1 Hz] [2 s] [5 s] [10 s] [60 s]`.
    - Cantidad de muestras visibles.
    - Toggle de Registro en MicroSD con intervalo de volcado.
  - Pestaña **Técnico**: Símbolo C++, identificador y memoria estimada en RAM.
- [ ] **Paso 5.4:** En `ux-foundation.js`, registrar la carga de `designer-trend.js`.
- [ ] **Paso 5.5:** Escribir y correr `trend-inspector.test.cjs` validando sincronización de controles, historial Undo/Redo y persistencia.

---

### Task 6: Firmware C++ — Sincronización en `JWPLC_UI_Widgets`

**Files:**
- Modify: `JWPLC/2.1.0/libraries/JWPLC_UI_Widgets/src/JWPLC_UI_Widgets.h`
- Modify: `JWPLC/2.1.0/libraries/JWPLC_UI_Widgets/src/JWPLC_UI_Widgets.cpp`
- Test: Compilación con g++ sintaxis / host simulator.

- [ ] **Paso 6.1:** En `JWPLC_UI_Widgets.h`, actualizar `JWPLC_UITrend`:
  - Permitir configuración de intervalo de muestreo (`sampleIntervalMs`) y grosor de línea (`lineWidth` 1 o 2 px).
  - Añadir soporte para modo auto-escala dinámica con 3 guías (`JWPLC_TREND_SCALE_AUTO`).
  - Agregar método de lectura histórica desde MicroSD:
    `bool querySDHistory(const char *filename, uint32_t startSec, uint32_t durationSec, float *outBuffer, uint16_t outPoints);`
- [ ] **Paso 6.2:** En `JWPLC_UI_Widgets.cpp`, implementar la lógica de auto-escala con resguardo, el trazo de 2 px mediante líneas paralelas sin solapamiento en guías y el lector por saltos de la SD.
- [ ] **Paso 6.3:** Comprobar compilación sintáctica en C++11 sin advertencias.

---

### Task 7: Verificación Integral, Pruebas de Humo y Regresión

**Files:**
- Modify: `tools/jwplc-hmi-designer/tests/run-all.cjs`
- Modify: `tools/jwplc-hmi-designer/poc/service-worker.js` (incrementar versión)
- Modify: Versiones de assets `?v=` en `index.html` y `desktop.html`

- [ ] **Paso 7.1:** Incorporar las nuevas pruebas en `tests/run-all.cjs`.
- [ ] **Paso 7.2:** Ejecutar `node tools/jwplc-hmi-designer/tests/run-all.cjs` y verificar que el 100% de las suites pasen en verde.
- [ ] **Paso 7.3:** Subir versiones de caché y assets `?v=`.
- [ ] **Paso 7.4:** Probar en la aplicación de escritorio (`poc/desktop.html`): inserción de la gráfica, ajuste de frecuencia de muestreo a 1 Hz y 10 Hz, alternancia de auto-escala a fija, cambio de colores y guardado/recarga del archivo `.jwhmi`.
