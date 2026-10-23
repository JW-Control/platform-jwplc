# Índice de Arquitectura y Referencia de Código: `poc/`

Este directorio contiene la aplicación cliente (frontend) completa del **JWPLC HMI Designer**.
Está construido en JavaScript estándar (ES6+), HTML5 Canvas y CSS3, sin frameworks pesados ni dependencias externas en tiempo de ejecución, lo que permite su ejecución fluida tanto en navegadores modernos como dentro del entorno de escritorio con **Electron**.

--- 

## 1. Mapa de Módulos y Arquitectura

El frontend está desacoplado en subsistemas especializados que se comunican a través del objeto global principal o mediante hooks e interceptores inyectados en `window`:

| Módulo / Archivo | Responsabilidad Principal | Dependencias Clave |
| :--- | :--- | :--- |
| [`app.js`](file:///c:/Users/Andre/Documents/GitHub/platform-jwplc/tools/jwplc-hmi-designer/poc/app.js) | Núcleo del diseñador: canvas ST7789 (320x170), framebuffer, fields (TEXT, VALUE, SHAPES), historial, paleta, atajos. | Canvas 2D, DOM |
| [`designer-bar.js`](file:///c:/Users/Andre/Documents/GitHub/platform-jwplc/tools/jwplc-hmi-designer/poc/designer-bar.js) | Subsistema de medidores y barras (`JWPLC_UIBarField`): Barra lineal y Medidor circular, inspector con subpestañas, simulación y C++. | `app.js` |
| [`designer-bool.js`](file:///c:/Users/Andre/Documents/GitHub/platform-jwplc/tools/jwplc-hmi-designer/poc/designer-bool.js) | Componentes booleanos (`JWPLC_UIBoolField`): lámparas e indicadores de estado, inspector y codegen. | `app.js` |
| [`designer-codegen.js`](file:///c:/Users/Andre/Documents/GitHub/platform-jwplc/tools/jwplc-hmi-designer/poc/designer-codegen.js) | Generador de código C++ (`JWPLC_HMI_Generated.h`): tablas de campos, enums de páginas, contratos y setters. | `app.js` |
| [`designer-dialog.js`](designer-dialog.js) | Avisos, confirmaciones y pedido de texto con el aspecto del Designer (`JWPLCDialog.confirm/alert/prompt`, asíncronas) en lugar de los cuadros genéricos de Windows. | — |
| [`designer-save-flow.js`](designer-save-flow.js) | Flujo de guardado: cambios sin guardar (Nuevo/Abrir/cerrar preguntan), Guardar y Ctrl+S escriben en el sketch, Guardar sin sketch crea o vincula uno. | `designer-sketch-menu.js`, `designer-dialog.js` |
| [`designer-field-visibility.js`](file:///c:/Users/Andre/Documents/GitHub/platform-jwplc/tools/jwplc-hmi-designer/poc/designer-field-visibility.js) | Controlador de visibilidad condicional del inspector según el tipo de objeto seleccionado. | `app.js` |
| [`designer-fonts.js`](file:///c:/Users/Andre/Documents/GitHub/platform-jwplc/tools/jwplc-hmi-designer/poc/designer-fonts.js) | Motor de renderizado de fuentes: FreeFonts GFX y fuentes bitmap TFT_eSPI (Font 2 y 4). | `designer-fonts-data.js` |
| [`designer-fonts-data.js`](file:///c:/Users/Andre/Documents/GitHub/platform-jwplc/tools/jwplc-hmi-designer/poc/designer-fonts-data.js) | Glifos y tablas de avance empaquetados en base64 de fuentes GFX y bitmap. | Ninguna |
| [`designer-inspector-state.js`](file:///c:/Users/Andre/Documents/GitHub/platform-jwplc/tools/jwplc-hmi-designer/poc/designer-inspector-state.js) | Máquina de estados del inspector lateral para asegurar exclusividad entre modo FIELDS y PIXELMAPS. | `app.js` |
| [`designer-live.js`](file:///c:/Users/Andre/Documents/GitHub/platform-jwplc/tools/jwplc-hmi-designer/poc/designer-live.js) | Conexión WebSerial a 921600 baud para previsualización física en tiempo real en la TFT física del PLC. | Web Serial API, `app.js` |
| [`designer-native-project.js`](file:///c:/Users/Andre/Documents/GitHub/platform-jwplc/tools/jwplc-hmi-designer/poc/designer-native-project.js) | Puente IPC para la app de escritorio Electron: enlace directo con sketch `.ino` y escritura en disco. | `window.JWPLCHMINative` |
| [`designer-pages.js`](file:///c:/Users/Andre/Documents/GitHub/platform-jwplc/tools/jwplc-hmi-designer/poc/designer-pages.js) | Gestor de múltiples páginas: barra de navegación, solapas superiores, simulación de pulsación de botones. | `app.js` |
| [`designer-pixelmap.js`](file:///c:/Users/Andre/Documents/GitHub/platform-jwplc/tools/jwplc-hmi-designer/poc/designer-pixelmap.js) | Editor básico de PixelMaps: capas de píxeles RGB565, paleta de colores, codegen C++. | `app.js` |
| [`designer-pixelmap-workbench.js`](file:///c:/Users/Andre/Documents/GitHub/platform-jwplc/tools/jwplc-hmi-designer/poc/designer-pixelmap-workbench.js) | Banco de herramientas pixel-art: pincel, goma, relleno, primitivas de dibujo, historial propio (Ctrl+Z). | `designer-pixelmap.js` |
| [`designer-pixelmap-optimizer.js`](file:///c:/Users/Andre/Documents/GitHub/platform-jwplc/tools/jwplc-hmi-designer/poc/designer-pixelmap-optimizer.js) | Optimizador de compresión RLE y spans continuos para minimizar memoria Flash/SRAM en C++. | `designer-pixelmap.js` |
| [`designer-pixelmap-codegen-guard.js`](file:///c:/Users/Andre/Documents/GitHub/platform-jwplc/tools/jwplc-hmi-designer/poc/designer-pixelmap-codegen-guard.js) | Guardián de codegen para deduplicar enums `HMIPixelMapId` y normalizar el header autogenerado. | `designer-pixelmap.js` |
| [`designer-pixelmap-compat.js`](file:///c:/Users/Andre/Documents/GitHub/platform-jwplc/tools/jwplc-hmi-designer/poc/designer-pixelmap-compat.js) | Capa de compatibilidad geométrica entre capas de píxeles y campos HMI. | `app.js` |
| [`designer-pixelmap-stability.js`](file:///c:/Users/Andre/Documents/GitHub/platform-jwplc/tools/jwplc-hmi-designer/poc/designer-pixelmap-stability.js) | Prevención de parpadeos y sincronización de repintado del workbench de píxeles. | `designer-pixelmap.js` |
| [`designer-project.js`](file:///c:/Users/Andre/Documents/GitHub/platform-jwplc/tools/jwplc-hmi-designer/poc/designer-project.js) | Persistencia de proyectos (`.jwhmi`) usando la File System Access API del navegador e IndexedDB. | `app.js`, `designer-codegen.js` |
| [`designer-project-integration-fix.js`](file:///c:/Users/Andre/Documents/GitHub/platform-jwplc/tools/jwplc-hmi-designer/poc/designer-project-integration-fix.js) | Parches de robustez para persistencia y vinculación en navegadores que no soportan File System API nativa. | `designer-project.js` |
| [`desktop-responsive.js`](file:///c:/Users/Andre/Documents/GitHub/platform-jwplc/tools/jwplc-hmi-designer/poc/desktop-responsive.js) | Adaptabilidad responsive para ventanas pequeñas/grandes de escritorio, gavetas laterales y zoom óptimo. | DOM |
| [`ux-foundation.js`](file:///c:/Users/Andre/Documents/GitHub/platform-jwplc/tools/jwplc-hmi-designer/poc/ux-foundation.js) | Capa de alta fidelidad visual: manipuladores de selección con badge de tamaño, ajuste exact-fit. | `app.js` |
| [`gfx-classic-font.js`](file:///c:/Users/Andre/Documents/GitHub/platform-jwplc/tools/jwplc-hmi-designer/poc/gfx-classic-font.js) | Datos de la fuente original 5x7 de Adafruit GFX para paridad exacta con el hardware. | Ninguna |
| [`index.html`](file:///c:/Users/Andre/Documents/GitHub/platform-jwplc/tools/jwplc-hmi-designer/poc/index.html) | Markup maestro del diseñador web (estructura de paneles, toolbar, canvas, modals). | Todos los scripts |
| [`desktop.html`](file:///c:/Users/Andre/Documents/GitHub/platform-jwplc/tools/jwplc-hmi-designer/poc/desktop.html) | Envoltorio optimizado para Electron con cache busters y arranque limpio. | `index.html` |
| [`styles.css`](file:///c:/Users/Andre/Documents/GitHub/platform-jwplc/tools/jwplc-hmi-designer/poc/styles.css) | Hoja de estilos principal (tema oscuro, disposición por flexbox/grid, controles custom). | Ninguna |
| [`desktop-responsive.css`](file:///c:/Users/Andre/Documents/GitHub/platform-jwplc/tools/jwplc-hmi-designer/poc/desktop-responsive.css) | Reglas multimedia y de diseño adaptativo para diferentes resoluciones. | `styles.css` |

--- 

## 2. Detalle Exhaustivo de Archivos y Funciones

### `app.js`

- **Ruta absoluta:** [`app.js`](file:///c:/Users/Andre/Documents/GitHub/platform-jwplc/tools/jwplc-hmi-designer/poc/app.js)
- **Líneas totales:** `5791` | **Tamaño:** `251.4 KB`

| Línea Aprox. | Función / Identificador | Parámetros | Descripción / Propósito |
| :---: | :--- | :--- | :--- |
| `L46` | `getSnapStep` | `` | Retorna el paso de la rejilla de ajuste magnético (snap) según la configuración activa. |
| `L55` | `setSnapEnabled` | `enabled` | Activa o desactiva el ajuste magnético (snap) en el canvas. |
| `L67` | `setGridEnabled` | `enabled` | Muestra u oculta la cuadrícula visual de referencia en el canvas. |
| `L81` | `closeAllVertPopovers` | `` | Cierra todos los menús flotantes verticales abiertos en la barra de herramientas. |
| `L88` | `toggleVertPopover` | `popover, force` | Muestra/oculta un menú flotante vertical auxiliar en la barra de herramientas. |
| `L348` | `updateMarqueeOverlay` | `startClient, endClient` | Actualiza el rectángulo de selección múltiple (marquesina CAD) mientras se arrastra el ratón. |
| `L375` | `updateMiniPreviewViewport` | `` | Sincroniza el recuadro visible del minimapa/vista previa con el zoom y scroll del viewport. |
| `L420` | `updateStageTransform` | `` | Aplica las transformaciones CSS (zoom, escala, pan) al contenedor principal del canvas. |
| `L430` | `applyZoom` | `nextZoom` | Aplica un nuevo factor de zoom al canvas (con límites mínimo y máximo). |
| `L450` | `fitCanvas` | `` | Ajusta el zoom automáticamente para que toda la pantalla (320x170) quede visible y centrada. |
| `L484` | `fitSelection` | `` | Ajusta el zoom para enfocar el objeto u objetos actualmente seleccionados. |
| `L578` | `defaultTextField` | `key = 'text-1'` | Crea y retorna un objeto de campo de texto por defecto con valores estándar. |
| `L604` | `defaultValueField` | `key = 'value-2'` | Crea y retorna un objeto de campo de valor numérico con formato y rango estándar. |
| `L638` | `clamp` | `value, min, max` | Restringe un valor numérico entre un mínimo y un máximo. |
| `L642` | `hex565` | `value` | Convierte un color RGB565 numérico a formato hexadecimal C++ (0xXXXX). |
| `L646` | `colorByName` | `name` | Retorna el valor numérico RGB565 correspondiente a un nombre estándar de color. |
| `L664` | `colorName` | `value` | Retorna el nombre estándar de un valor numérico RGB565 si coincide. |
| `L671` | `rgb565ToRgb888` | `value` | Convierte un entero RGB565 a una tupla de componentes {r, g, b} en escala 0-255. |
| `L682` | `rgb565ToCss` | `value` | Convierte un valor RGB565 a string CSS rgb(r, g, b). |
| `L688` | `rgb565ToHex888` | `value` | Convierte un valor RGB565 a formato hexadecimal web #RRGGBB. |
| `L694` | `hex888ToRgb565` | `hex` | Convierte un color hexadecimal web (#RRGGBB) a entero RGB565 de 16 bits. |
| `L703` | `ensureSelectOption` | `select, value` | Garantiza que un elemento <select> contenga una opción específica, creándola si no existe. |
| `L739` | `indexFor` | `x, y` | Función interna de soporte del módulo. |
| `L740` | `inside` | `x, y` | Función interna de soporte del módulo. |
| `L742` | `rotatePoint` | `px, py, cx, cy, angleDeg` | Rota un punto 2D (px, py) alrededor de un centro (cx, cy) por un ángulo en grados. |
| `L755` | `transformShapePoint` | `px, py, cx, cy, angleDeg, flipH, flipV` | Aplica rotación y volteo horizontal/vertical a un punto de una figura. |
| `L762` | `inverseTransformShapePoint` | `px, py, cx, cy, angleDeg, flipH, flipV` | Aplica la transformación inversa de rotación/volteo para mapear coordenadas globales a locales. |
| `L768` | `getMaxCornerRadius` | `field` | Calcula el radio máximo de redondeo permitido para `RECT`, `TRIANGLE` y `POLYGON`. |
| `L778` | `buildRoundedPolygonPoints` | `basePts, radius` | Subdivide los vértices de un polígono/rectángulo/triángulo en arcos tangenciales redondeados. |
| `L831` | `getShapeTransformedVertices` | `field` | Calcula los vértices transformados (con redondeo `cornerRadius`, rotación y espejado) de una figura. |
| `L905` | `getShapeLocalBounds` | `field` | Calcula los límites locales (ancho/alto antes de rotar) de una figura. |
| `L944` | `addRecentColor` | `color565` | Añade un color a la lista de colores recientes en la paleta del usuario. |
| `L950` | `updateEyedropperActiveUI` | `` | Actualiza la UI visual del botón cuentagotas cuando está activado. |
| `L959` | `updateEyedropperLiveUI` | `point` | Actualiza la muestra y tooltip del cuentagotas con el color bajo el cursor. |
| `L984` | `renderRecentColors` | `` | Renderiza las muestras de colores recientes en la paleta del inspector. |
| `L1028` | `pageExists` | `page` | Verifica si un identificador de página existe en el proyecto. |
| `L1032` | `fieldsForPage` | `page = activePage` | Retorna todos los campos (fields) pertenecientes a una página específica. |
| `L1036` | `setSelectedKeys` | `keys` | Establece la lista de claves (IDs) de los campos seleccionados. |
| `L1041` | `selectedFields` | `` | Retorna la lista de objetos de campo correspondientes a la selección actual. |
| `L1045` | `selectedField` | `` | Retorna el primer campo seleccionado o null si no hay ninguno. |
| `L1050` | `toolForField` | `field` | Determina la herramienta activa adecuada según el tipo de campo seleccionado. |
| `L1060` | `setLayerPixel` | `x, y, value` | Pinta un píxel en la capa activa en coordenadas (x, y). |
| `L1068` | `floodFill` | `startX, startY, replacement` | Rellena un área contigua de píxeles del mismo color (algoritmo de bote de pintura). |
| `L1125` | `pickColorAt` | `point` | Muestrea el color RGB565 del framebuffer en las coordenadas del canvas. |
| `L1156` | `setBufferPixel` | `buffer, x, y, value` | Asigna un valor RGB565 a un píxel del búfer y registra `topFieldOrder` cuando escribe en el framebuffer. |
| `L1167` | `fillBufferRect` | `buffer, x, y, width, height, value` | Rellena un rectángulo sólido en el búfer de píxeles. |
| `L1177` | `drawBufferRect` | `buffer, x, y, width, height, value` | Dibuja el contorno de un rectángulo en el búfer de píxeles. |
| `L1185` | `rasterLineBuffer` | `buffer, x0, y0, x1, y1, value, size = 1` | Traza una línea discretizada con grosor en el búfer usando algoritmo Bresenham. |
| `L1207` | `rasterLine` | `x0, y0, x1, y1, value` | Traza una línea en el búfer de dibujo actual. |
| `L1224` | `drawShapeEllipse` | `buffer, a, b, color, size` | Dibuja el contorno elíptico en el búfer. |
| `L1276` | `getPolygonVertices` | `field` | Función selectora / consulta de estado. |
| `L1297` | `fillShapeEllipse` | `buffer, a, b, color` | Rellena una elipse en el búfer. |
| `L1323` | `fillShapeTriangle` | `buffer, p1, p2, p3, color` | Rellena un triángulo definido por 3 puntos en el búfer. |
| `L1328` | `interpolateX` | `y, ya, yb, xa, xb` | Función interna de soporte del módulo. |
| `L1350` | `fillShapePolygon` | `buffer, pts, color` | Rellena un polígono convexo/cóncavo en el búfer usando scanline. |
| `L1383` | `isPointInsideShapeArea` | `px, py, field` | Determina si un punto (px, py) cae dentro del área de una figura. |
| `L1435` | `drawShape` | `buffer, field` | Renderiza una figura geométrica completa (incluyendo esquinas redondeadas) en el búfer. |
| `L1472` | `drawClassicChar` | `buffer, x, y, charCode, foreground, background, size` | Dibuja un caracter individual usando la fuente bitmap clásica de Adafruit GFX. |
| `L1490` | `drawClassicTextAt` | `buffer, text, x, y, foreground, background, size` | Dibuja una cadena de texto en el búfer con fuente clásica y factor de escala. |
| `L1504` | `drawRawTextAt` | `buffer, text, x, y, foreground, background, size, fontId` | Renderiza texto con soporte de fuentes avanzadas FreeFonts o bitmap TFT. |
| `L1540` | `rawTextBounds` | `text, size, fontId` | Calcula el cuadro delimitador exacto de un texto con una fuente específica. |
| `L1548` | `nominalTextBounds` | `text, size` | Calcula el tamaño nominal de un texto con fuente clásica por factor de tamaño. |
| `L1554` | `effectiveFieldPadding` | `field` | Calcula el padding efectivo (izq, der, arriba, abajo) configurado en un campo. |
| `L1558` | `makeNumericSample` | `field` | Genera un número de muestra formateado para previsualizar campos numéricos. |
| `L1564` | `makeOverflowText` | `field` | Genera texto de prueba con caracteres de desbordamiento (ej. 9999). |
| `L1572` | `formatNumericPreview` | `field` | Aplica el formato (decimales, sufijo, prefijo) al valor simulado de un campo numérico. |
| `L1600` | `valueSampleForField` | `field` | Obtiene el valor de muestra a representar en un campo de valor. |
| `L1605` | `previewTextForField` | `field` | Obtiene el texto final formateado para renderizar un campo de texto o valor. |
| `L1610` | `getFieldBounds` | `field` | Calcula el rectángulo delimitador (`x, y, minX, maxX, minY, maxY, width, height`) exacto de un campo. |
| `L1682` | `computeFieldGeometry` | `field` | Calcula métricas geométricas completas (caja, padding, línea base, alineación) de un campo. |
| `L1774` | `alignedValueX` | `field, geometry` | Calcula la coordenada X del texto de un valor según su alineación (LEFT, RIGHT, CENTER). |
| `L1783` | `drawField` | `buffer, field` | Renderiza un campo (TEXT, VALUE, BOOL) en el búfer del canvas. |
| `L1810` | `drawBarToFramebuffer` | `buffer, field, onlyOrder = false` | Rasteriza barras lineales en el framebuffer y registra `topFieldOrder` (solo orden para `CIRCULAR`). |
| `L1864` | `composeFramebuffer` | `` | Compone las capas en el búfer 320x170 y construye el mapa por píxel `topFieldOrder` según `layerOrder`. |
| `L1892` | `rebuildLogicalImage` | `` | Convierte el búfer numérico RGB565 a una imagen ImageData de canvas 2D. |
| `L1912` | `drawGrid` | `` | Dibuja la cuadrícula de puntos o líneas sobre el canvas. |
| `L1947` | `sanitizeSymbol` | `value, fallback` | Limpia una cadena para que sea un identificador C++ válido (elimina espacios y caracteres especiales). |
| `L1953` | `cppString` | `value` | Escapa una cadena para su inserción literal en código C++ ("ejemplo"). |
| `L1957` | `cppBool` | `value` | Convierte un booleano de JavaScript a literal true/false de C++. |
| `L1961` | `fieldFallbackId` | `field, index` | Genera un ID simbólico C++ por defecto si el campo no tiene uno asignado. |
| `L1968` | `variableFallback` | `field, index` | Genera un nombre de variable C++ por defecto si el campo no tiene uno. |
| `L1975` | `variableDeclaration` | `field, index` | Genera la línea de declaración de variable C++ (ej. int16_t var = 0;). |
| `L1982` | `fieldContract` | `field, index` | Genera la macro de registro del campo para la tabla C++ (ej. JWPLC_UITextField(...)). |
| `L1998` | `setterHint` | `field, index` | Genera comentario con ejemplo de uso del setter en runtime C++ (ej. jwplcSetText(...)). |
| `L2007` | `buildContractText` | `` | Genera el bloque C++ completo con la tabla HMI_FIELDS y sus variables asociadas. |
| `L2020` | `buildStatusText` | `` | Construye la cadena de resumen de estado y conteo de campos para la barra inferior. |
| `L2029` | `updateMetrics` | `` | Actualiza los indicadores de memoria, campos usados y límites en la barra de estado. |
| `L2063` | `updateCodePanel` | `` | Sincroniza y muestra el código generado en la pestaña inferior de C++. |
| `L2069` | `ensureLayerOrder` | `item` | Garantiza que cualquier objeto (`field` o `PIXELMAP`) tenga un `layerOrder` numérico único. |
| `L2080` | `assignNewLayerOrder` | `item` | Asigna el siguiente `layerOrder` secuencial al final de la pila al crear o duplicar un objeto. |
| `L2093` | `pageObjectsInOrder` | `page = activePage` | Retorna la lista unificada de objetos (`fields` + `pixelmaps`) de la página ordenados por `layerOrder`. |
| `L2103` | `sortObjectListDOM` | `` | Ordena los nodos `.object-item` dentro de `#objectsList` según su `data-layer-order`. |
| `L2110` | `reorderPageFields` | `sourceKey, targetKey, insertAbove` | Reordena el orden Z unificado entre campos estándar y objetos `Pixel` en la página activa. |
| `L2143` | `updateMiniPreview` | `` | Dibuja la vista en miniatura en el panel lateral. |
| `L2169` | `formatLopakaFieldName` | `field, index` | Formatea el nombre descriptivo del campo para la lista de objetos. |
| `L2194` | `renderObjectList` | `` | Renderiza el árbol de objetos de la página activa en `#objectsList` respetando `layerOrder`. |
| `L2331` | `updateHistoryButtons` | `` | Actualiza el estado habilitado/deshabilitado de los botones Deshacer y Rehacer. |
| `L2338` | `render` | `` | Ejecuta el ciclo de renderizado completo, dibuja medidores `CIRCULAR` vectoriales y respeta oclusión de capas. |
| `L2421` | `pointFromPointer` | `event` | Convierte un evento de ratón/táctil a coordenadas lógicas (0..319, 0..169) del HMI. |
| `L2431` | `drawCADMarquee` | `` | Dibuja el marco de selección rectangular CAD si hay un arrastre activo. |
| `L2435` | `getSelectionBounds` | `fields` | Obtiene el rectángulo contenedor de todos los campos seleccionados. |
| `L2468` | `getSelectionHandles` | `fields` | Calcula las posiciones de los manipuladores de redimensionamiento (esquinas y lados). |
| `L2565` | `hitTestHandle` | `event` | Verifica si el cursor está sobre alguno de los manipuladores de la selección. |
| `L2586` | `isPointInsideSelection` | `point` | Verifica si un punto está dentro del área delimitada por los campos seleccionados. |
| `L2617` | `drawSelectionAndGuides` | `` | Dibuja el recuadro de selección, guías magnéticas inteligentes y manipuladores. |
| `L2766` | `updateCrosshairs` | `pt` | Posiciona las líneas en cruz discontinuas alineadas con las coordenadas lógicas exactas del cursor sobre el canvas. |
| `L2785` | `drawRulers` | `` | Renderiza las 4 regletas perimetrales (superior, inferior, izquierda, derecha) con marcas, cotas e indicador dinámico de posición del cursor. |
| `L3080` | `updateCursor` | `point` | Actualiza la barra de estado inferior, el cuentagotas y sincroniza el cursor en las 4 regletas perimetrales. |
| `L2905` | `syncToolUI` | `` | Sincroniza el botón activo en la barra de herramientas según el modo seleccionado. |
| `L2995` | `updateActiveColorUI` | `` | Actualiza la muestra del color actualmente seleccionado en la interfaz. |
| `L3003` | `buildColorSelect` | `select, selectedName, allowTransparent = false` | Puebla un selector desplegable con los colores estándar de ST77XX. |
| `L3024` | `attachVisualColorPicker` | `select, getValue, setValue` | Asocia el selector visual de color personalizado a un control de entrada. |
| `L3046` | `sync` | `` | Sincroniza todos los paneles, inspectores, canvas y código con el estado actual del proyecto. |
| `L3111` | `syncAllColorControls` | `` | Refresca todos los selectores de color del inspector con sus valores reales. |
| `L3115` | `buildPalette` | `` | Construye la paleta visual de colores predefinidos. |
| `L3147` | `buildShapeCppSnippet` | `field` | Construye el fragmento de código C++ (`TFT_eSPI`) para la pestaña Técnico del inspector de figuras. |
| `L3215` | `syncShapeInspector` | `` | Sincroniza todos los controles del nuevo inspector de figuras por pestañas (`Forma`, `Apariencia`, `Ubicación`, `Técnico`). |
| `L3313` | `syncInputsFromState` | `` | Vuelca los datos del campo seleccionado en los inputs del inspector. |
| `L3434` | `captureSnapshot` | `` | Captura una instantánea completa del estado del proyecto para el historial. |
| `L3449` | `equalPixels` | `a, b` | Función interna de soporte del módulo. |
| `L3455` | `sameSnapshot` | `a, b` | Función interna de soporte del módulo. |
| `L3466` | `commitHistory` | `` | Guarda un snapshot en el historial de deshacer si ha habido cambios. |
| `L3476` | `restoreSnapshot` | `snapshot` | Restaura el estado completo del proyecto a partir de un snapshot del historial. |
| `L3499` | `undo` | `` | Deshace la última acción en el proyecto. |
| `L3505` | `redo` | `` | Rehace la última acción deshecha. |
| `L3511` | `uniqueFieldSymbol` | `base` | Genera un símbolo C++ único incrementando sufijos numéricos. |
| `L3522` | `uniqueVariable` | `base` | Genera un nombre de variable C++ único sin colisiones. |
| `L3533` | `placeNewField` | `field` | Coloca un nuevo campo en la página activa evitando superposición con campos existentes. |
| `L3539` | `selectFirstOnPage` | `` | Selecciona el primer campo disponible en la página actual. |
| `L3545` | `setActivePage` | `page` | Cambia la página activa en el diseñador y refresca la vista. |
| `L3557` | `addPage` | `name` | Crea una nueva página en el proyecto con un nombre específico. |
| `L3575` | `renamePage` | `page, name` | Renombra una página existente y actualiza identificadores asociados. |
| `L3586` | `setPageMetadata` | `page, metadata = {}` | Asigna metadatos personalizados (color, shortName) a una página. |
| `L3618` | `deletePage` | `page` | Elimina una página del proyecto si no es la única. |
| `L3650` | `moveSelectedFieldToPage` | `page` | Mueve el campo seleccionado de la página actual a otra página destino. |
| `L3666` | `addShapeField` | `type, startPoint, endPoint` | Crea y añade un nuevo campo de figura geométrica con `cornerRadius: 0` y `assignNewLayerOrder`. |
| `L3698` | `addRawTextField` | `point` | Crea un campo de texto libre asignándole `assignNewLayerOrder`. |
| `L3730` | `addTextField` | `` | Crea un campo de texto con valores por defecto asignándole `assignNewLayerOrder`. |
| `L3753` | `addValueField` | `` | Crea un campo de valor numérico con valores por defecto asignándole `assignNewLayerOrder`. |
| `L3775` | `duplicateSelectedField` | `` | Duplica el campo seleccionado asignándole `assignNewLayerOrder` al final de la lista. |
| `L3807` | `deleteSelectedField` | `` | Elimina el campo seleccionado del proyecto y guarda historial. |
| `L3821` | `resetProject` | `` | Reinicia el proyecto al estado inicial con campos de demostración. |
| `L3835` | `demoTextField` | `` | Crea el campo de texto de demostración inicial. |
| `L3870` | `demoValueField` | `` | Crea el campo de valor de demostración inicial. |
| `L3909` | `bindRawInput` | `element, handler` | Enlaza un input de texto básico a un manejador de actualización. |
| `L3921` | `bindFieldInput` | `element, handler` | Enlaza un control del inspector a una propiedad del campo con guardado en historial. |
| `L4023` | `updateVariableButtonDisplay` | `tool` | Actualiza el texto/icono del botón selector de variable en el inspector. |
| `L4032` | `updateTextButtonDisplay` | `tool` | Actualiza la visualización de configuración de texto. |
| `L4041` | `closeAllDropdowns` | `` | Cierra cualquier menú desplegable abierto en la barra de herramientas o inspector. |
| `L4153` | `updateScalePxHint` | `` | Actualiza la indicación visual de resolución y píxeles reales según la escala. |
| `L4343` | `distToSegment` | `px, py, x1, y1, x2, y2` | Calcula la distancia mínima euclidiana de un punto a un segmento de recta. |
| `L4353` | `distanceToFieldBorder` | `point, field` | Calcula la distancia de un punto al borde del contorno de un campo. |
| `L4460` | `hitTestField` | `point` | Determina qué campo ha sido pinchado con el ratón en coordenadas del lienzo. |
| `L4480` | `handlePointerDown` | `event` | Manejador del evento de inicio de clic o toque en el canvas. |
| `L4676` | `handlePointerMove` | `event` | Manejador de movimiento del ratón/toque (arrastre, redimensión, dibujo, hover). |
| `L5031` | `segmentsIntersect` | `x1, y1, x2, y2, x3, y3, x4, y4` | Verifica si dos segmentos de recta se cruzan. |
| `L5032` | `ccw` | `ax, ay, bx, by, cx, cy` | Función interna de soporte del módulo. |
| `L5039` | `segmentIntersectsBox` | `x1, y1, x2, y2, L, R, T, B` | Verifica si un segmento de recta interseca una caja delimitadora. |
| `L5051` | `doesBoxTouchContour` | `boxL, boxR, boxT, boxB, field` | Verifica si una caja toca el contorno de un campo para selección por contacto. |
| `L5126` | `endPointer` | `` | Finaliza la interacción del ratón (commit de transformación, creación o selección). |
| `L5256` | `isEditingTarget` | `target` | Verifica si el objetivo del evento es un input o elemento editable que debe ignorar atajos. |
| `L5263` | `nudgeSelection` | `dx, dy` | Mueve los campos seleccionados (dx, dy) píxeles usando las teclas de flecha. |
| `L5326` | `alignSelectedShapes` | `alignType` | Alinea los campos seleccionados (izquierda, derecha, centro, arriba, abajo, distribuir). |
| `L5579` | `syncShapeTabPanes` | `` | Alterna la visibilidad de las pestañas principales (`Diseño`/`Técnico`) y subpestañas (`Forma`/`Apariencia`/`Ubicación`) del inspector de figuras. |
| `L5636` | `applyShapeCornerRadius` | `val, commit = false` | Aplica y restringe el radio de redondeo (`cornerRadius`) de la figura seleccionada según el preset o entrada numérica. |
| `L5767` | `activateShapeEyedropper` | `target` | Abre el cuentagotas (EyeDropper nativo del navegador o muestreador en lienzo) para copiar colores a borde o relleno. |
| `L5832` | `shapeGeomUpdate` | `` | Actualiza las propiedades de geometría de figuras tras editar X/Y/W/H. |
| `L5877` | `triggerColorPicker` | `input` | Abre programáticamente el diálogo nativo o selector de color. |
| `L5938` | `window.JWPLCHMIEditor` | `` | API global expuesta por el editor principal (incluye `getShapeVertices`, `getMaxCornerRadius`, `ensureLayerOrder`, `assignNewLayerOrder`, `reorderPageObjects`, `sortObjectListDOM`, `getFieldBounds`, `getPixelFieldOrder`). |

### `designer-bar.js`

- **Ruta absoluta:** [`designer-bar.js`](file:///c:/Users/Andre/Documents/GitHub/platform-jwplc/tools/jwplc-hmi-designer/poc/designer-bar.js)
- **Líneas totales:** `3221` | **Tamaño:** `171.8 KB`

| Línea Aprox. | Función / Identificador | Parámetros | Descripción / Propósito |
| :---: | :--- | :--- | :--- |
| `L910` | `editor` | `` | Función interna de soporte del módulo. |
| `L914` | `selectedField` | `` | Retorna el primer campo seleccionado o null si no hay ninguno. |
| `L918` | `isBar` | `field = selectedField(` | Función selectora / consulta de estado. |
| `L922` | `activeBars` | `` | Función interna de soporte del módulo. |
| `L926` | `visibleBars` | `` | Función interna de soporte del módulo. |
| `L931` | `serialFor` | `field` | Función interna de soporte del módulo. |
| `L938` | `sanitizeSymbol` | `value, fallback` | Limpia una cadena para que sea un identificador C++ válido (elimina espacios y caracteres especiales). |
| `L944` | `cppString` | `value` | Escapa una cadena para su inserción literal en código C++ ("ejemplo"). |
| `L948` | `cppBool` | `value` | Convierte un booleano de JavaScript a literal true/false de C++. |
| `L952` | `cppFloat` | `value` | Función interna de soporte del módulo. |
| `L958` | `hex565` | `value` | Convierte un color RGB565 numérico a formato hexadecimal C++ (0xXXXX). |
| `L963` | `rgbTo565` | `r, g, b` | Función interna de soporte del módulo. |
| `L967` | `hexCssTo565` | `hex` | Función interna de soporte del módulo. |
| `L983` | `rgb565ToCss` | `value` | Convierte un valor RGB565 a string CSS rgb(r, g, b). |
| `L992` | `rgb565ToHexCss` | `value` | Función interna de soporte del módulo. |
| `L1001` | `nominalTextBounds` | `text, size` | Calcula el tamaño nominal de un texto con fuente clásica por factor de tamaño. |
| `L1008` | `interpolateColor565` | `c1, c2, t` | Función interna de soporte del módulo. |
| `L1018` | `computeSafeCenterTextSize` | `field, text` | Función interna de soporte del módulo. |
| `L1029` | `getCircleCenterText` | `field, norm` | Función selectora / consulta de estado. |
| `L1047` | `ensureBarState` | `field` | Función interna de soporte del módulo. |
| `L1103` | `normalizedFor` | `field` | Función interna de soporte del módulo. |
| `L1112` | `barGeometry` | `field` | Función interna de soporte del módulo. |
| `L1357` | `drawClassicText` | `ctx, text, x, y, size, color, scale` | Renderizado gráfico del elemento correspondiente. |
| `L1381` | `traceRoundedRect` | `ctx, x, y, w, h, r` | Función interna de soporte del módulo. |
| `L1402` | `getRoundedRectRowDx` | `dy, h, rad` | Función selectora / consulta de estado. |
| `L1414` | `pixelFillRoundedRect` | `ctx, x, y, w, h, r, colorCss, scale` | Función interna de soporte del módulo. |
| `L1432` | `pixelBorderRoundedRect` | `ctx, x, y, w, h, r, bThick, colorCss, scale` | Función interna de soporte del módulo. |
| `L1480` | `pixelFillRoundedPortion` | `ctx, x, y, w, h, r, orientation, fillAmount, isGrad, startCol, endCol, solidCss, scale` | Función interna de soporte del módulo. |
| `L1526` | `drawBarField` | `ctx, field, scale` | Renderizado de barra lineal (pixel-art con `trackCss` en `radPx <= 0`) y medidor circular vectorial. |
| `L2037` | `drawBars` | `displayCtx, zoom, previewCtx` | Renderiza medidores `CIRCULAR` directamente a escala `zoom` cuando `integratedBarLayers` está activo. |
| `L2051` | `patchCanvases` | `` | Repinta medidores `CIRCULAR` en `displayCanvas` y `previewCanvas` tras cambios en el inspector. |
| `L2070` | `triggerCoreRender` | `commit = false` | Función interna de soporte del módulo. |
| `L2082` | `switchMainTab` | `tab` | Función interna de soporte del módulo. |
| `L2097` | `switchSubTab` | `subTab` | Función interna de soporte del módulo. |
| `L2126` | `copyToClipboard` | `text, button` | Función utilitaria para copiar al portapapeles con confirmación visual (✓) |
| `L2128` | `onSuccess` | `` | Función interna de soporte del módulo. |
| `L2144` | `fallbackCopy` | `text, cb` | Función interna de soporte del módulo. |
| `L2178` | `updateBarFromControls` | `commit = false` | Función interna de soporte del módulo. |
| `L2303` | `syncSimulationControls` | `val, commit = false` | --- Simulación local (Debajo de Tipo de indicador y en Datos PLC) --- |
| `L2320` | `setupSliderInteraction` | `slider` | Asigna o muta el estado del componente. |
| `L2324` | `updateFromPointer` | `e` | Función interna de soporte del módulo. |
| `L2351` | `stopDrag` | `e` | Función interna de soporte del módulo. |
| `L2825` | `setPreset` | `preset, commit = true` | Selector de Presets de un clic |
| `L2866` | `bindBarTextControl` | `input, property` | Función interna de soporte del módulo. |
| `L2882` | `createBarField` | `` | Función interna de soporte del módulo. |
| `L2953` | `patchObjectList` | `` | Aplica parche/decorador sobre la función o componente correspondiente. |
| `L2966` | `patchInspector` | `field` | Sincroniza controles del inspector BAR preservando valores `0x0000` (negro) mediante `??`. |
| `L3275` | `barFieldBlock` | `field` | Función interna de soporte del módulo. |
| `L3307` | `replaceHelperCall` | `text, id, replacement` | Función interna de soporte del módulo. |
| `L3355` | `activeBarFieldsInCode` | `text` | Función interna de soporte del módulo. |
| `L3359` | `patchGeneratedCode` | `` | Aplica parche/decorador sobre la función o componente correspondiente. |
| `L3382` | `patchStatusText` | `` | Aplica parche/decorador sobre la función o componente correspondiente. |
| `L3414` | `patchUI` | `` | Aplica parche/decorador sobre la función o componente correspondiente. |
| `L3439` | `window.JWPLCHMIBar` | `` | Expone la API del módulo en el objeto global de ventana. |

### `designer-bool.js`

- **Ruta absoluta:** [`designer-bool.js`](file:///c:/Users/Andre/Documents/GitHub/platform-jwplc/tools/jwplc-hmi-designer/poc/designer-bool.js)
- **Líneas totales:** `319` | **Tamaño:** `12.8 KB`

| Línea Aprox. | Función / Identificador | Parámetros | Descripción / Propósito |
| :---: | :--- | :--- | :--- |
| `L53` | `editor` | `` | Función interna de soporte del módulo. |
| `L57` | `selectedField` | `` | Retorna el primer campo seleccionado o null si no hay ninguno. |
| `L61` | `isBool` | `field = selectedField(` | Función selectora / consulta de estado. |
| `L65` | `allBoolFields` | `` | Función interna de soporte del módulo. |
| `L69` | `serialFor` | `field` | Función interna de soporte del módulo. |
| `L74` | `sanitizeSymbol` | `value, fallback` | Limpia una cadena para que sea un identificador C++ válido (elimina espacios y caracteres especiales). |
| `L80` | `cppString` | `value` | Escapa una cadena para su inserción literal en código C++ ("ejemplo"). |
| `L84` | `cppBool` | `value` | Convierte un booleano de JavaScript a literal true/false de C++. |
| `L88` | `hex565` | `value` | Convierte un color RGB565 numérico a formato hexadecimal C++ (0xXXXX). |
| `L92` | `ensureBoolState` | `field` | Función interna de soporte del módulo. |
| `L102` | `forceBoolRender` | `commit = false` | Función interna de soporte del módulo. |
| `L110` | `updateBoolFromControls` | `commit = false` | Función interna de soporte del módulo. |
| `L126` | `createBoolField` | `` | Función interna de soporte del módulo. |
| `L169` | `patchObjectList` | `` | Aplica parche/decorador sobre la función o componente correspondiente. |
| `L182` | `patchInspector` | `field` | Aplica parche/decorador sobre la función o componente correspondiente. |
| `L203` | `boolFieldBlock` | `field` | Función interna de soporte del módulo. |
| `L211` | `replaceHelperCall` | `text, id, replacement` | Función interna de soporte del módulo. |
| `L238` | `activeBoolFieldsInCode` | `text` | Función interna de soporte del módulo. |
| `L245` | `patchGeneratedCode` | `` | Aplica parche/decorador sobre la función o componente correspondiente. |
| `L267` | `patchStatusText` | `` | Aplica parche/decorador sobre la función o componente correspondiente. |
| `L287` | `patchUI` | `` | Aplica parche/decorador sobre la función o componente correspondiente. |
| `L313` | `window.JWPLCHMIBool` | `` | Expone la API del módulo en el objeto global de ventana. |

### `designer-codegen.js`

- **Ruta absoluta:** [`designer-codegen.js`](file:///c:/Users/Andre/Documents/GitHub/platform-jwplc/tools/jwplc-hmi-designer/poc/designer-codegen.js)
- **Líneas totales:** `642` | **Tamaño:** `26.8 KB`

| Línea Aprox. | Función / Identificador | Parámetros | Descripción / Propósito |
| :---: | :--- | :--- | :--- |
| `L17` | `editor` | `` | Función interna de soporte del módulo. |
| `L21` | `allFields` | `` | Función interna de soporte del módulo. |
| `L25` | `pages` | `` | Función interna de soporte del módulo. |
| `L29` | `selectedField` | `` | Retorna el primer campo seleccionado o null si no hay ninguno. |
| `L33` | `sanitizeSymbol` | `value, fallback` | Limpia una cadena para que sea un identificador C++ válido (elimina espacios y caracteres especiales). |
| `L39` | `sanitizePageToken` | `value, fallback` | Función interna de soporte del módulo. |
| `L51` | `pageSymbols` | `` | Función interna de soporte del módulo. |
| `L65` | `serialFor` | `field, fallbackIndex = 0` | Función interna de soporte del módulo. |
| `L70` | `fieldFallbackId` | `field, index` | Genera un ID simbólico C++ por defecto si el campo no tiene uno asignado. |
| `L78` | `variableFallback` | `field, index` | Genera un nombre de variable C++ por defecto si el campo no tiene uno. |
| `L86` | `canonicalFor` | `field, property, value, index` | Función interna de soporte del módulo. |
| `L93` | `pageForField` | `field` | Función interna de soporte del módulo. |
| `L101` | `pageComment` | `page` | Función interna de soporte del módulo. |
| `L108` | `duplicateFor` | `field, property, candidate` | Función interna de soporte del módulo. |
| `L125` | `ensureWarningNode` | `input, kind` | Función interna de soporte del módulo. |
| `L137` | `injectStyles` | `` | Función interna de soporte del módulo. |
| `L158` | `clearWarning` | `input, kind` | Función interna de soporte del módulo. |
| `L165` | `showDuplicateWarning` | `input, kind, duplicate` | Función interna de soporte del módulo. |
| `L176` | `guardUniqueInput` | `input, property, kind` | Función interna de soporte del módulo. |
| `L199` | `validationIssues` | `` | Función interna de soporte del módulo. |
| `L219` | `variableDeclaration` | `field, index` | Genera la línea de declaración de variable C++ (ej. int16_t var = 0;). |
| `L228` | `setterHint` | `field, index` | Genera código C++ para figuras (`fillRoundRect`/`drawRoundRect`, polígonos/triángulos redondeados) y setters en runtime. |
| `L375` | `groupedChunks` | `renderer, commentIndent = ''` | Función interna de soporte del módulo. |
| `L392` | `navBarFunctionBlock` | `` | Genera las funciones C++ de la barra de navegación HMI. |
| `L476` | `generatedUIUpdateBlock` | `` | Función interna de soporte del módulo. |
| `L517` | `pageEnumBlock` | `` | Función interna de soporte del módulo. |
| `L523` | `decorateHeader` | `text` | Función interna de soporte del módulo. |
| `L535` | `groupGeneratedSections` | `text` | Función interna de soporte del módulo. |
| `L596` | `duplicateDiagnostic` | `text, issues` | Función interna de soporte del módulo. |
| `L607` | `patchGeneratedCode` | `` | Aplica parche/decorador sobre la función o componente correspondiente. |
| `L615` | `patchUI` | `` | Aplica parche/decorador sobre la función o componente correspondiente. |
| `L636` | `window.JWPLCHMICodegen` | `` | Expone la API del módulo en el objeto global de ventana. |

### `designer-field-visibility.js`

- **Ruta absoluta:** [`designer-field-visibility.js`](file:///c:/Users/Andre/Documents/GitHub/platform-jwplc/tools/jwplc-hmi-designer/poc/designer-field-visibility.js)
- **Líneas totales:** `148` | **Tamaño:** `6.1 KB`

| Línea Aprox. | Función / Identificador | Parámetros | Descripción / Propósito |
| :---: | :--- | :--- | :--- |
| `L19` | `editor` | `` | Función interna de soporte del módulo. |
| `L23` | `setVisible` | `element, visible` | Asigna o muta el estado del componente. |
| `L29` | `syncFieldVisibility` | `` | Sincroniza el estado interno con la interfaz de usuario. |
| `L88` | `loadInspectorState` | `` | Función interna de soporte del módulo. |
| `L98` | `loadPixelStability` | `` | Función interna de soporte del módulo. |

### `designer-fonts-data.js`

- **Ruta absoluta:** [`designer-fonts-data.js`](file:///c:/Users/Andre/Documents/GitHub/platform-jwplc/tools/jwplc-hmi-designer/poc/designer-fonts-data.js)
- **Líneas totales:** `4` | **Tamaño:** `166.2 KB`

| Línea Aprox. | Función / Identificador | Parámetros | Descripción / Propósito |
| :---: | :--- | :--- | :--- |
| `L2` | `window.JWPLCGfxFontData` | `` | Expone la API del módulo en el objeto global de ventana. |
| `L3` | `window.JWPLCTftBitmapFontData` | `` | Expone la API del módulo en el objeto global de ventana. |

### `designer-fonts.js`

- **Ruta absoluta:** [`designer-fonts.js`](file:///c:/Users/Andre/Documents/GitHub/platform-jwplc/tools/jwplc-hmi-designer/poc/designer-fonts.js)
- **Líneas totales:** `292` | **Tamaño:** `10.2 KB`

| Línea Aprox. | Función / Identificador | Parámetros | Descripción / Propósito |
| :---: | :--- | :--- | :--- |
| `L54` | `get` | `id` | Función selectora / consulta de estado. |
| `L58` | `normalizeId` | `id` | Función interna de soporte del módulo. |
| `L63` | `fieldFont` | `field` | Compatibilidad con proyectos antiguos: sin campo font => GLCD. |
| `L70` | `gfx` | `font` | Función interna de soporte del módulo. |
| `L98` | `bmp` | `font` | Función interna de soporte del módulo. |
| `L111` | `bitmapIndex` | `data, character` | Función interna de soporte del módulo. |
| `L116` | `glyphOf` | `data, character` | Función interna de soporte del módulo. |
| `L124` | `ctx2d` | `` | Función interna de soporte del módulo. |
| `L131` | `cssFont` | `font, size` | Función interna de soporte del módulo. |
| `L137` | `measureWidth` | `font, text, size` | ---- API ------------------------------------------------------------------ |
| `L164` | `lineHeight` | `font, size` | Función interna de soporte del módulo. |
| `L176` | `rasterizeExact` | `font, data, text, size` | Función interna de soporte del módulo. |
| `L213` | `rasterizeBitmap` | `font, data, text, size` | Función interna de soporte del módulo. |
| `L243` | `rasterizeApprox` | `font, text, size` | Función interna de soporte del módulo. |
| `L266` | `rasterize` | `font, text, size` | anclado por la esquina superior izquierda (TL_DATUM de TFT_eSPI). |

### `designer-inspector-state.js`

- **Ruta absoluta:** [`designer-inspector-state.js`](file:///c:/Users/Andre/Documents/GitHub/platform-jwplc/tools/jwplc-hmi-designer/poc/designer-inspector-state.js)
- **Líneas totales:** `93` | **Tamaño:** `3.2 KB`

| Línea Aprox. | Función / Identificador | Parámetros | Descripción / Propósito |
| :---: | :--- | :--- | :--- |
| `L30` | `editor` | `` | Función interna de soporte del módulo. |
| `L34` | `pixels` | `` | Función interna de soporte del módulo. |
| `L38` | `setSectionVisible` | `section, visible` | Asigna o muta el estado del componente. |
| `L45` | `currentMode` | `` | Determina el modo exclusivo del inspector (`PIXELMAP`, `SHAPE`, `FIELDS` o `EMPTY`). |
| `L57` | `syncInspectorState` | `` | Sincroniza la visibilidad de las secciones del inspector evitando que `#textFieldControlsSection` aparezca bajo `#shapeInspectorSection`. |
| `L88` | `window.JWPLCHMIInspectorState` | `` | Expone la API del módulo en el objeto global de ventana. |

### `designer-live.js`

- **Ruta absoluta:** [`designer-live.js`](file:///c:/Users/Andre/Documents/GitHub/platform-jwplc/tools/jwplc-hmi-designer/poc/designer-live.js)
- **Líneas totales:** `710` | **Tamaño:** `26.1 KB`

| Línea Aprox. | Función / Identificador | Parámetros | Descripción / Propósito |
| :---: | :--- | :--- | :--- |
| `L155` | `editor` | `` | Función interna de soporte del módulo. |
| `L159` | `timestamp` | `` | Función interna de soporte del módulo. |
| `L164` | `refreshLog` | `` | Función interna de soporte del módulo. |
| `L170` | `logLine` | `text` | Función interna de soporte del módulo. |
| `L176` | `showDiagnostic` | `` | Función interna de soporte del módulo. |
| `L183` | `showCodeOutput` | `` | Función interna de soporte del módulo. |
| `L213` | `dispatchInput` | `element, value` | Función interna de soporte del módulo. |
| `L221` | `fieldSerial` | `field` | Función interna de soporte del módulo. |
| `L226` | `normalizeSelectedObjectDefaults` | `` | Función interna de soporte del módulo. |
| `L253` | `scheduleNormalize` | `` | Función interna de soporte del módulo. |
| `L263` | `rgb888To565` | `r, g, b` | Función interna de soporte del módulo. |
| `L267` | `captureFrame565` | `` | Función interna de soporte del módulo. |
| `L277` | `computeDirtyRect` | `current, baseline` | Función interna de soporte del módulo. |
| `L305` | `buildRuns` | `frame, rect` | Función interna de soporte del módulo. |
| `L326` | `nextSequence` | `` | Función interna de soporte del módulo. |
| `L331` | `appendRuns` | `packet, offset, runs` | Función interna de soporte del módulo. |
| `L340` | `encodeFullPacket` | `frame` | Función interna de soporte del módulo. |
| `L355` | `encodeRegionPacket` | `frame, rect` | Función interna de soporte del módulo. |
| `L371` | `writePacketChunked` | `packet` | Función interna de soporte del módulo. |
| `L377` | `sendProbe` | `` | Función interna de soporte del módulo. |
| `L386` | `stopProbeTimer` | `` | Función interna de soporte del módulo. |
| `L391` | `stopAckTimer` | `` | Función interna de soporte del módulo. |
| `L396` | `updateDiagForSend` | `meta` | Función interna de soporte del módulo. |
| `L407` | `armAckTimeout` | `expectedSequence` | Función interna de soporte del módulo. |
| `L426` | `requestLiveSync` | `force = false` | Función interna de soporte del módulo. |
| `L436` | `trySendLatest` | `` | Función interna de soporte del módulo. |
| `L491` | `parseStats` | `line` | Función interna de soporte del módulo. |
| `L505` | `handleDeviceLine` | `line` | Manejador de evento de interacción de usuario. |
| `L580` | `readLoop` | `` | Función interna de soporte del módulo. |
| `L599` | `portDescription` | `` | Función interna de soporte del módulo. |
| `L611` | `connectLive` | `` | Función interna de soporte del módulo. |
| `L645` | `disconnectLive` | `` | Función interna de soporte del módulo. |
| `L702` | `window.JWPLCHMILive` | `` | Expone la API del módulo en el objeto global de ventana. |

### `designer-native-project.js`

- **Ruta absoluta:** [`designer-native-project.js`](file:///c:/Users/Andre/Documents/GitHub/platform-jwplc/tools/jwplc-hmi-designer/poc/designer-native-project.js)
- **Líneas totales:** `196` | **Tamaño:** `7.1 KB`

| Línea Aprox. | Función / Identificador | Parámetros | Descripción / Propósito |
| :---: | :--- | :--- | :--- |
| `L10` | `wait` | `ms` | Función interna de soporte del módulo. |
| `L14` | `waitFor` | `predicate, timeoutMs = 3000` | Función interna de soporte del módulo. |
| `L23` | `toast` | `message, kind = ''` | Función interna de soporte del módulo. |
| `L57` | `updateUi` | `` | Función interna de soporte del módulo. |
| `L79` | `linkSketchNative` | `` | Función interna de soporte del módulo. |
| `L93` | `generatedHeaderText` | `` | Función interna de soporte del módulo. |
| `L113` | `updateHeaderNative` | `` | Función interna de soporte del módulo. |
| `L134` | `saveCanonicalProjectNative` | `fallbackSave` | Función interna de soporte del módulo. |
| `L163` | `replaceButton` | `id, handler` | Función interna de soporte del módulo. |
| `L172` | `init` | `` | Inicializador del módulo y vinculación de eventos. |

### `designer-pages.js`

- **Ruta absoluta:** [`designer-pages.js`](file:///c:/Users/Andre/Documents/GitHub/platform-jwplc/tools/jwplc-hmi-designer/poc/designer-pages.js)
- **Líneas totales:** `1706` | **Tamaño:** `62.6 KB`

| Línea Aprox. | Función / Identificador | Parámetros | Descripción / Propósito |
| :---: | :--- | :--- | :--- |
| `L12` | `PRESET_PAGE_COLORS` | `` | Paleta de 8 colores ST7789 para pestañas (azul, blanco, lavanda, amarillo, cian, verde, naranja, rojo). |
| `L55` | `MICRO_FONT_3X5` | `` | Glifos bitmap 3x5 embebidos en celda 4x6 para micro-tipografía ultracompacta. |
| `L98` | `drawMicroText` | `ctx, text, x, y, scale, col` | Renderiza microfuente en canvas HTML5 para pestañas laterales o estrechas. |
| `L115` | `computeSmartShortName` | `pageName, index` | Calcula automáticamente una abreviatura inteligente (ej. `CFG`, `GEN`, `T_CD`) según el nombre. |
| `L145` | `getNavBarConfig` | `` | Retorna la configuración activa de la barra HMI (activación, 6 presets, posición, coordenadas X/Y/W/H, distribución, alineación, fuente, bordes, radio, gap). |
| `L176` | `updateNavBarConfig` | `next` | Actualiza la configuración de la barra de navegación y sincroniza dimensiones horizontal/vertical. |
| `L226` | `syncNavBarConfigFromField` | `field` | Sincroniza la configuración del navbar desde los cambios de arrastre/redimensión CAD del lienzo en `app.js`. |
| `L252` | `computeNavBarLayout` | `pageList, curActivePage` | Algoritmo de distribución adaptativo: calcula posiciones horizontal/vertical, cluster agrupado para `compact-dots`, anchos de botones y flechas ◄ ► ▲ ▼ / contador NN/TT en desbordamiento. |
| `L620` | `drawHMINavigationBar` | `ctx, scale` | Renderizado gráfico de la barra HMI en canvas según el prediseño activo (`industrial-color`, `dark-pill`, `segmented-bar`, `compact-dots`, `edge-tabs`, `numeric-pills`), fondo y bordes. |
| `L940` | `rebuildTopTabs` | `` | Reconstruye las solapas superiores exclusivamente para gestión de páginas (renombrado inline con doble clic, hold-to-delete y `＋`). |
| `L1299` | `switchNavBarMainTab` | `tabName` | Alterna entre las pestañas `Diseño`, `Páginas` y `Técnico` del Inspector de Barra de Navegación homogéneo con `BAR`. |
| `L1493` | `syncNavBarInspector` | `` | Sincroniza tarjetas visuales (6 presets, 4 posiciones, alineación rápida H/V/Full, coordenadas X/Y/W/H, tipografía micro/normal/mediana, acabados), lista de páginas y llamada C++ en el panel derecho. |
| `L1678` | `window.JWPLCHMIPages` | `` | API pública del gestor de páginas y barra HMI expuesta en window (`syncNavBarConfigFromField`, `getNavBarConfig`, `setNavBarConfig`, etc.). |

### `designer-pixelmap-codegen-guard.js`

- **Ruta absoluta:** [`designer-pixelmap-codegen-guard.js`](file:///c:/Users/Andre/Documents/GitHub/platform-jwplc/tools/jwplc-hmi-designer/poc/designer-pixelmap-codegen-guard.js)
- **Líneas totales:** `126` | **Tamaño:** `4.0 KB`

| Línea Aprox. | Función / Identificador | Parámetros | Descripción / Propósito |
| :---: | :--- | :--- | :--- |
| `L13` | `canonicalEnum` | `` | Función interna de soporte del módulo. |
| `L17` | `stripEnums` | `text` | Función interna de soporte del módulo. |
| `L21` | `insertCanonicalEnum` | `text, enumBlock` | Función interna de soporte del módulo. |
| `L41` | `normalizeText` | `text` | Función interna de soporte del módulo. |
| `L57` | `normalizeGenerated` | `result` | Función interna de soporte del módulo. |
| `L63` | `wrapBuildCode` | `` | Función interna de soporte del módulo. |
| `L74` | `applyToOutput` | `` | Función interna de soporte del módulo. |
| `L119` | `window.JWPLCHMIPixelCodegenGuard` | `` | Expone la API del módulo en el objeto global de ventana. |

### `designer-pixelmap-compat.js`

- **Ruta absoluta:** [`designer-pixelmap-compat.js`](file:///c:/Users/Andre/Documents/GitHub/platform-jwplc/tools/jwplc-hmi-designer/poc/designer-pixelmap-compat.js)
- **Líneas totales:** `215` | **Tamaño:** `9.0 KB`

| Línea Aprox. | Función / Identificador | Parámetros | Descripción / Propósito |
| :---: | :--- | :--- | :--- |
| `L21` | `editor` | `` | Función interna de soporte del módulo. |
| `L22` | `activePage` | `` | Función interna de soporte del módulo. |
| `L24` | `textBounds` | `text, size` | Función interna de soporte del módulo. |
| `L31` | `numericSample` | `field` | Función interna de soporte del módulo. |
| `L37` | `barRect` | `field` | Función interna de soporte del módulo. |
| `L65` | `fieldRect` | `field` | Obtiene el rectángulo delimitador del campo delegando primero en `editor().getFieldBounds(field)`. |
| `L92` | `captureBase` | `` | Captura el snapshot base de `displayCanvas` y `previewCanvas`. |
| `L102` | `restoreBase` | `` | Restaura el snapshot base antes de componer las capas `Pixel`. |
| `L122` | `restoreFields` | `` | Redibuja guías de selección y respeta la oclusión por píxel (`getPixelFieldOrder`) y `layerOrder`. |
| `L152` | `queueRestore` | `` | Función interna de soporte del módulo. |
| `L179` | `wrapCodegen` | `` | Función interna de soporte del módulo. |
| `L213` | `window.JWPLCHMIPixelCompat` | `` | Expone la API del módulo en el objeto global de ventana. |

### `designer-pixelmap-optimizer.js`

- **Ruta absoluta:** [`designer-pixelmap-optimizer.js`](file:///c:/Users/Andre/Documents/GitHub/platform-jwplc/tools/jwplc-hmi-designer/poc/designer-pixelmap-optimizer.js)
- **Líneas totales:** `391` | **Tamaño:** `15.0 KB`

| Línea Aprox. | Función / Identificador | Parámetros | Descripción / Propósito |
| :---: | :--- | :--- | :--- |
| `L13` | `waitForPixelCodegen` | `` | Función interna de soporte del módulo. |
| `L29` | `hex565` | `value` | Convierte un color RGB565 numérico a formato hexadecimal C++ (0xXXXX). |
| `L30` | `hex32` | `value` | Función interna de soporte del módulo. |
| `L32` | `cppToken` | `value, fallback` | Función interna de soporte del módulo. |
| `L45` | `absolutePixels` | `map` | Función interna de soporte del módulo. |
| `L56` | `legacyRuns` | `map` | Función interna de soporte del módulo. |
| `L71` | `horizontalSpans` | `points, colorIndex` | Función interna de soporte del módulo. |
| `L85` | `verticalSpans` | `points, colorIndex` | Función interna de soporte del módulo. |
| `L99` | `packedPlanForMap` | `map` | Función interna de soporte del módulo. |
| `L150` | `analyzeProject` | `pm = window.JWPLCHMIPixelMaps` | Función interna de soporte del módulo. |
| `L180` | `packSpan` | `span` | Función interna de soporte del módulo. |
| `L195` | `valuesBlock` | `values, formatter, indent = '    '` | Función interna de soporte del módulo. |
| `L203` | `enumBlockForMaps` | `maps` | Función interna de soporte del módulo. |
| `L214` | `existingEnumBlock` | `block, maps` | Función interna de soporte del módulo. |
| `L219` | `packedCode` | `analysis, originalGenerated` | Función interna de soporte del módulo. |
| `L264` | `wrapBuildCode` | `pm` | Función interna de soporte del módulo. |
| `L291` | `injectInspector` | `` | Función interna de soporte del módulo. |
| `L325` | `formatBytes` | `value` | Función interna de soporte del módulo. |
| `L331` | `refreshInspector` | `pm = window.JWPLCHMIPixelMaps` | Función interna de soporte del módulo. |
| `L360` | `patchGate` | `` | Aplica parche/decorador sobre la función o componente correspondiente. |
| `L370` | `init` | `` | Inicializador del módulo y vinculación de eventos. |
| `L384` | `window.JWPLCHMIPixelOptimizer` | `` | Expone la API del módulo en el objeto global de ventana. |

### `designer-pixelmap-stability.js`

- **Ruta absoluta:** [`designer-pixelmap-stability.js`](file:///c:/Users/Andre/Documents/GitHub/platform-jwplc/tools/jwplc-hmi-designer/poc/designer-pixelmap-stability.js)
- **Líneas totales:** `362` | **Tamaño:** `14.0 KB`

| Línea Aprox. | Función / Identificador | Parámetros | Descripción / Propósito |
| :---: | :--- | :--- | :--- |
| `L19` | `editor` | `` | Función interna de soporte del módulo. |
| `L20` | `pixels` | `` | Función interna de soporte del módulo. |
| `L21` | `compat` | `` | Función interna de soporte del módulo. |
| `L22` | `page` | `` | Función interna de soporte del módulo. |
| `L27` | `clamp` | `value, min, max` | Restringe un valor numérico entre un mínimo y un máximo. |
| `L31` | `rgb565` | `value` | Función interna de soporte del módulo. |
| `L45` | `editorCss565` | `value, opacity` | Calcula color CSS con opacidad visual del Designer. |
| `L51` | `fieldRects` | `forMap` | Retorna rectángulos de campos con `layerOrder < forMap.layerOrder` como respaldo geométrico. |
| `L63` | `mapBounds` | `map` | Función interna de soporte del módulo. |
| `L74` | `drawSelectionGeometry` | `` | Renderizado gráfico del elemento correspondiente. |
| `L101` | `stableRepaint` | `` | Repinta capas `Pixel` ordenadas por `layerOrder` respetando oclusión por píxel (`getPixelFieldOrder`). |
| `L121` | `covered` | `x, y` | Evalúa si un píxel `(x, y)` está cubierto por un campo/figura con `layerOrder < mapOrder`. |
| `L149` | `scheduleStableRepaint` | `` | Registra el repintado estable al final de la cola del mismo frame. |
| `L159` | `isEditingTarget` | `target` | Verifica si el objetivo del evento es un input o elemento editable que debe ignorar atajos. |
| `L166` | `sanitizeToken` | `value, fallback` | Función interna de soporte del módulo. |
| `L180` | `runtimeMaps` | `` | Función interna de soporte del módulo. |
| `L184` | `pixelMapSymbols` | `` | Función interna de soporte del módulo. |
| `L194` | `pixelMapEnumBlock` | `` | Función interna de soporte del módulo. |
| `L200` | `wrapBuildCode` | `` | Función interna de soporte del módulo. |
| `L218` | `pageSymbols` | `` | Función interna de soporte del módulo. |
| `L229` | `pixelOnlyHeader` | `generated` | Función interna de soporte del módulo. |
| `L257` | `insertPixelCodeIntoMixedHeader` | `text, generated` | Función interna de soporte del módulo. |
| `L283` | `ensurePixelCode` | `` | Función interna de soporte del módulo. |
| `L355` | `window.JWPLCHMIPixelStability` | `` | Expone la API del módulo en el objeto global de ventana. |

### `designer-pixelmap-workbench.js`

- **Ruta absoluta:** [`designer-pixelmap-workbench.js`](file:///c:/Users/Andre/Documents/GitHub/platform-jwplc/tools/jwplc-hmi-designer/poc/designer-pixelmap-workbench.js)
- **Líneas totales:** `873` | **Tamaño:** `29.4 KB`

| Línea Aprox. | Función / Identificador | Parámetros | Descripción / Propósito |
| :---: | :--- | :--- | :--- |
| `L11` | `waitForPixelMaps` | `` | Función interna de soporte del módulo. |
| `L26` | `isEditingTarget` | `target` | Verifica si el objetivo del evento es un input o elemento editable que debe ignorar atajos. |
| `L31` | `clamp` | `value, min, max` | Restringe un valor numérico entre un mínimo y un máximo. |
| `L35` | `brushRange` | `size` | Función interna de soporte del módulo. |
| `L41` | `pointFromEvent` | `canvas, event` | Función interna de soporte del módulo. |
| `L52` | `mapBounds` | `map` | Función interna de soporte del módulo. |
| `L70` | `pixelIndex` | `map, gx, gy` | Función interna de soporte del módulo. |
| `L77` | `pixelColor` | `map, gx, gy` | Función interna de soporte del módulo. |
| `L82` | `putPixel` | `map, gx, gy, value` | Función interna de soporte del módulo. |
| `L118` | `putBrush` | `map, gx, gy, value, size` | Función interna de soporte del módulo. |
| `L129` | `rasterLine` | `a, b, fn` | Traza una línea en el búfer de dibujo actual. |
| `L149` | `drawLine` | `map, a, b, color, size` | Renderizado gráfico del elemento correspondiente. |
| `L157` | `drawRect` | `map, a, b, color, size` | Renderizado gráfico del elemento correspondiente. |
| `L171` | `drawEllipse` | `map, a, b, color, size` | Renderizado gráfico del elemento correspondiente. |
| `L193` | `plot` | `px, py` | Función interna de soporte del módulo. |
| `L216` | `drawTriangle` | `map, a, b, color, size` | Renderizado gráfico del elemento correspondiente. |
| `L227` | `drawPolygon` | `map, a, b, sides, color, size` | Renderizado gráfico del elemento correspondiente. |
| `L251` | `floodFill` | `map, gx, gy, replacement` | Rellena un área contigua de píxeles del mismo color (algoritmo de bote de pintura). |
| `L269` | `sameTarget` | `x, y` | Función interna de soporte del módulo. |
| `L311` | `rgb565Css` | `value` | Función interna de soporte del módulo. |
| `L322` | `init` | `` | Inicializador del módulo y vinculación de eventos. |
| `L344` | `selectedMap` | `` | Función interna de soporte del módulo. |
| `L348` | `selectedMapIndex` | `` | Función interna de soporte del módulo. |
| `L353` | `snapshot` | `` | Función interna de soporte del módulo. |
| `L360` | `sameSnapshot` | `a, b` | Función interna de soporte del módulo. |
| `L365` | `resetHistory` | `` | Función interna de soporte del módulo. |
| `L373` | `commitHistoryNow` | `` | Función interna de soporte del módulo. |
| `L389` | `scheduleHistoryCommit` | `` | Función interna de soporte del módulo. |
| `L395` | `rowForIndex` | `index` | Función interna de soporte del módulo. |
| `L402` | `restoreSnapshot` | `state` | Restaura el estado completo del proyecto a partir de un snapshot del historial. |
| `L430` | `undoPixel` | `` | Función interna de soporte del módulo. |
| `L438` | `redoPixel` | `` | Función interna de soporte del módulo. |
| `L446` | `syncHistoryButtons` | `` | Sincroniza el estado interno con la interfaz de usuario. |
| `L458` | `notifyChanged` | `source` | Función interna de soporte del módulo. |
| `L465` | `injectTools` | `` | Función interna de soporte del módulo. |
| `L469` | `injectStyles` | `` | Función interna de soporte del módulo. |
| `L479` | `updateToolButtons` | `` | Función interna de soporte del módulo. |
| `L498` | `syncCursorCanvas` | `` | Sincroniza el estado interno con la interfaz de usuario. |
| `L503` | `activeFootprintSize` | `` | Función interna de soporte del módulo. |
| `L511` | `drawGuideRect` | `point, size, stroke, dashed = true` | Renderizado gráfico del elemento correspondiente. |
| `L526` | `drawPreviewShape` | `` | Renderizado gráfico del elemento correspondiente. |
| `L585` | `drawCursor` | `point` | Renderizado gráfico del elemento correspondiente. |
| `L611` | `pickColorAt` | `point` | Muestrea el color RGB565 del framebuffer en las coordenadas del canvas. |
| `L624` | `activateNativeMode` | `mode` | Función interna de soporte del módulo. |
| `L632` | `activateExtraMode` | `mode` | Función interna de soporte del módulo. |
| `L638` | `snapPoint` | `point, force = false` | Función interna de soporte del módulo. |
| `L651` | `handlePointerDown` | `event` | Manejador del evento de inicio de clic o toque en el canvas. |
| `L694` | `handlePointerMove` | `event` | Manejador de movimiento del ratón/toque (arrastre, redimensión, dibujo, hover). |
| `L707` | `handlePointerUp` | `event` | Manejador de evento de interacción de usuario. |
| `L740` | `handleKeydown` | `event` | Manejador de evento de interacción de usuario. |
| `L779` | `handleToolbarClick` | `event` | Manejador de evento de interacción de usuario. |
| `L859` | `window.JWPLCHMIPixelWorkbench` | `` | Expone la API del módulo en el objeto global de ventana. |

### `designer-pixelmap.js`

- **Ruta absoluta:** [`designer-pixelmap.js`](file:///c:/Users/Andre/Documents/GitHub/platform-jwplc/tools/jwplc-hmi-designer/poc/designer-pixelmap.js)
- **Líneas totales:** `1207` | **Tamaño:** `47.8 KB`

| Línea Aprox. | Función / Identificador | Parámetros | Descripción / Propósito |
| :---: | :--- | :--- | :--- |
| `L52` | `editor` | `` | Función interna de soporte del módulo. |
| `L53` | `page` | `` | Función interna de soporte del módulo. |
| `L54` | `hex565` | `value` | Convierte un color RGB565 numérico a formato hexadecimal C++ (0xXXXX). |
| `L55` | `inside` | `p` | Función interna de soporte del módulo. |
| `L56` | `clamp` | `value, min, max` | Restringe un valor numérico entre un mínimo y un máximo. |
| `L57` | `currentZoom` | `` | Función interna de soporte del módulo. |
| `L59` | `parse565` | `value, fallback = 0` | Función interna de soporte del módulo. |
| `L67` | `rgb565ToRgb` | `value` | Función interna de soporte del módulo. |
| `L79` | `css565` | `value` | Función interna de soporte del módulo. |
| `L84` | `hex888` | `value` | Función interna de soporte del módulo. |
| `L89` | `hex888To565` | `value` | Función interna de soporte del módulo. |
| `L99` | `selected` | `` | Función interna de soporte del módulo. |
| `L103` | `mapsForPage` | `` | Función interna de soporte del módulo. |
| `L107` | `ensureEditorMeta` | `map` | Función interna de soporte del módulo. |
| `L115` | `bounds` | `map` | Función interna de soporte del módulo. |
| `L126` | `pointFromEvent` | `event` | Función interna de soporte del módulo. |
| `L135` | `findPixel` | `map, localX, localY` | Función interna de soporte del módulo. |
| `L139` | `canonicalize` | `map` | Función interna de soporte del módulo. |
| `L153` | `putPixel` | `map, gx, gy, value` | Función interna de soporte del módulo. |
| `L183` | `brushRange` | `size` | Función interna de soporte del módulo. |
| `L189` | `putBrush` | `map, gx, gy, value, size` | Función interna de soporte del módulo. |
| `L200` | `eraseBrush` | `map, gx, gy, size` | Función interna de soporte del módulo. |
| `L218` | `rasterLine` | `a, b, fn` | Traza una línea en el búfer de dibujo actual. |
| `L237` | `clearFieldSelection` | `` | Función interna de soporte del módulo. |
| `L247` | `createMap` | `` | Crea una nueva capa `Pixel` asignándole `assignNewLayerOrder` al final del árbol. |
| `L269` | `duplicateMap` | `` | Duplica la capa `Pixel` seleccionada asignándole `assignNewLayerOrder`. |
| `L290` | `deleteMap` | `` | Elimina la capa `Pixel` seleccionada. |
| `L299` | `hitMap` | `point, tolerance = 2` | Detecta selección sobre una capa `Pixel` respetando `layerOrder` y `getPixelFieldOrder`. |
| `L328` | `organizeLeftPanel` | `` | Función interna de soporte del módulo. |
| `L332` | `injectStyles` | `` | Función interna de soporte del módulo. |
| `L360` | `makeColorControl` | `root, getValue, setValue, compact = false` | Función interna de soporte del módulo. |
| `L394` | `sync` | `` | Sincroniza todos los paneles, inspectores, canvas y código con el estado actual del proyecto. |
| `L422` | `syncMainColor` | `` | Sincroniza el estado interno con la interfaz de usuario. |
| `L424` | `sizeOptions` | `` | Función interna de soporte del módulo. |
| `L428` | `injectInspector` | `` | Función interna de soporte del módulo. |
| `L545` | `syncInspector` | `` | Sincroniza el estado interno con la interfaz de usuario. |
| `L574` | `reorderPageMaps` | `sourceKey, targetKey, insertAbove` | Reordena capas `Pixel` y campos estándar delegando en `editor().reorderPageObjects(...)`. |
| `L594` | `patchObjectList` | `` | Inserta filas `.a11-pixel-row` en `#objectsList` con `data-layer-order` y ejecuta `sortObjectListDOM`. |
| `L708` | `fieldRects` | `forMap` | Obtiene rectángulos de campos con `layerOrder < forMap.layerOrder`. |
| `L721` | `render` | `` | Renderiza las capas `Pixel` ordenadas por `layerOrder` y respeta `getPixelFieldOrder`. |
| `L735` | `covered` | `x, y` | Evalúa si un píxel `(x, y)` está cubierto por un campo/figura con `layerOrder < mapOrder`. |
| `L775` | `drawGeometry` | `` | Renderizado gráfico del elemento correspondiente. |
| `L789` | `refreshBase` | `` | Función interna de soporte del módulo. |
| `L794` | `changed` | `` | Función interna de soporte del módulo. |
| `L800` | `normalizeRuns` | `map` | Función interna de soporte del módulo. |
| `L812` | `cppToken` | `value, fallback` | Función interna de soporte del módulo. |
| `L817` | `buildCode` | `` | Función interna de soporte del módulo. |
| `L834` | `patchCode` | `` | Aplica parche/decorador sobre la función o componente correspondiente. |
| `L850` | `syncTools` | `` | Sincroniza el estado interno con la interfaz de usuario. |
| `L872` | `updateEraserCursor` | `p` | Función interna de soporte del módulo. |
| `L1067` | `endPointer` | `event` | Finaliza la interacción del ratón (commit de transformación, creación o selección). |
| `L1146` | `exportMaps` | `` | Función interna de soporte del módulo. |
| `L1159` | `importMaps` | `input` | Función interna de soporte del módulo. |
| `L1184` | `window.JWPLCHMIPixelMaps` | `` | Expone la API del módulo en el objeto global de ventana. |

### `designer-project-integration-fix.js`

- **Ruta absoluta:** [`designer-project-integration-fix.js`](file:///c:/Users/Andre/Documents/GitHub/platform-jwplc/tools/jwplc-hmi-designer/poc/designer-project-integration-fix.js)
- **Líneas totales:** `320` | **Tamaño:** `11.4 KB`

| Línea Aprox. | Función / Identificador | Parámetros | Descripción / Propósito |
| :---: | :--- | :--- | :--- |
| `L13` | `wait` | `ms` | Función interna de soporte del módulo. |
| `L17` | `waitFor` | `predicate, timeoutMs = 3500` | Función interna de soporte del módulo. |
| `L26` | `toast` | `message, kind = ''` | Función interna de soporte del módulo. |
| `L61` | `validatePixelMaps` | `input` | Función interna de soporte del módulo. |
| `L73` | `installProjectPersistence` | `` | Función interna de soporte del módulo. |
| `L105` | `chooseProjectFile` | `` | Función interna de soporte del módulo. |
| `L127` | `openProjectFixed` | `` | Función interna de soporte del módulo. |
| `L139` | `installOpenButton` | `` | Función interna de soporte del módulo. |
| `L151` | `nativeLinkedInfo` | `` | Función interna de soporte del módulo. |
| `L161` | `syncNativeLinkedUi` | `info` | Sincroniza el estado interno con la interfaz de usuario. |
| `L182` | `isValidGeneratedHeader` | `text` | Función selectora / consulta de estado. |
| `L188` | `stableGeneratedHeaderText` | `` | Función interna de soporte del módulo. |
| `L216` | `linkSketchIfNeeded` | `` | Función interna de soporte del módulo. |
| `L229` | `updateHeaderNativeFixed` | `` | Función interna de soporte del módulo. |
| `L252` | `installNativeUpdateButton` | `` | Función interna de soporte del módulo. |
| `L269` | `restoreNativeState` | `` | Función interna de soporte del módulo. |
| `L275` | `init` | `` | Inicializador del módulo y vinculación de eventos. |
| `L312` | `window.JWPLCHMIProjectIntegrationFix` | `` | Expone la API del módulo en el objeto global de ventana. |

### `designer-project.js`

- **Ruta absoluta:** [`designer-project.js`](file:///c:/Users/Andre/Documents/GitHub/platform-jwplc/tools/jwplc-hmi-designer/poc/designer-project.js)
- **Líneas totales:** `723` | **Tamaño:** `24.7 KB`

| Línea Aprox. | Función / Identificador | Parámetros | Descripción / Propósito |
| :---: | :--- | :--- | :--- |
| `L20` | `editor` | `` | Función interna de soporte del módulo. |
| `L24` | `codegen` | `` | Función interna de soporte del módulo. |
| `L28` | `wait` | `ms` | Función interna de soporte del módulo. |
| `L32` | `waitFor` | `predicate, timeoutMs = 2500` | Función interna de soporte del módulo. |
| `L41` | `toolbar` | `` | Función interna de soporte del módulo. |
| `L45` | `statusbar` | `` | Función interna de soporte del módulo. |
| `L49` | `findToolbarButton` | `label` | Función interna de soporte del módulo. |
| `L54` | `projectStatusNode` | `` | Función interna de soporte del módulo. |
| `L59` | `setProjectStatus` | `` | Asigna o muta el estado del componente. |
| `L65` | `setDirty` | `next = true` | Asigna o muta el estado del componente. |
| `L70` | `injectStyles` | `` | Función interna de soporte del módulo. |
| `L134` | `toast` | `message, kind = ''` | Función interna de soporte del módulo. |
| `L150` | `ensureManifest` | `` | Función interna de soporte del módulo. |
| `L165` | `registerServiceWorker` | `` | Función interna de soporte del módulo. |
| `L172` | `openDatabase` | `` | Función interna de soporte del módulo. |
| `L185` | `storeHandle` | `key, handle` | Función interna de soporte del módulo. |
| `L199` | `loadHandle` | `key` | Función interna de soporte del módulo. |
| `L216` | `ensureDirectoryPermission` | `handle, ask = false` | Función interna de soporte del módulo. |
| `L226` | `sanitizeFileStem` | `value, fallback = 'JWPLC_HMI'` | Función interna de soporte del módulo. |
| `L231` | `canonicalProjectFileName` | `` | Función interna de soporte del módulo. |
| `L236` | `directoryHasFile` | `directory, name` | Función interna de soporte del módulo. |
| `L246` | `serializeProject` | `` | Función interna de soporte del módulo. |
| `L275` | `validateProject` | `data` | Función interna de soporte del módulo. |
| `L294` | `normalizeImportedFields` | `fields` | Normaliza los campos importados (incluyendo `cornerRadius`) al cargar un proyecto `.jwhmi`. |
| `L303` | `applyProject` | `data` | Función interna de soporte del módulo. |
| `L347` | `writeFileHandle` | `handle, text` | Función interna de soporte del módulo. |
| `L353` | `saveProjectBesideSketch` | `data` | Función interna de soporte del módulo. |
| `L379` | `saveProjectAs` | `` | Función interna de soporte del módulo. |
| `L406` | `saveProject` | `` | Función interna de soporte del módulo. |
| `L422` | `openProject` | `` | Función interna de soporte del módulo. |
| `L455` | `directoryContainsSketch` | `directory` | Función interna de soporte del módulo. |
| `L474` | `linkedStatusNode` | `` | Función interna de soporte del módulo. |
| `L478` | `linkButton` | `` | Función interna de soporte del módulo. |
| `L482` | `updateButton` | `` | Función interna de soporte del módulo. |
| `L486` | `updateLinkedUI` | `state = 'none'` | Función interna de soporte del módulo. |
| `L500` | `linkSketch` | `` | Función interna de soporte del módulo. |
| `L529` | `generatedHeaderText` | `` | Función interna de soporte del módulo. |
| `L548` | `updateGeneratedHeader` | `` | Función interna de soporte del módulo. |
| `L586` | `restoreLinkedSketch` | `` | Función interna de soporte del módulo. |
| `L596` | `injectToolbar` | `` | Inyecta los botones agrupados con iconos SVG en la barra superior (`.toolbar`). |
| `L661` | `injectStatus` | `` | Función interna de soporte del módulo. |
| `L673` | `bindDirtyTracking` | `` | Función interna de soporte del módulo. |
| `L710` | `window.JWPLCHMIProject` | `` | Expone la API del módulo en el objeto global de ventana. |

### `desktop-responsive.js`

- **Ruta absoluta:** [`desktop-responsive.js`](file:///c:/Users/Andre/Documents/GitHub/platform-jwplc/tools/jwplc-hmi-designer/poc/desktop-responsive.js)
- **Líneas totales:** `506` | **Tamaño:** `19.4 KB`

| Línea Aprox. | Función / Identificador | Parámetros | Descripción / Propósito |
| :---: | :--- | :--- | :--- |
| `L18` | `$` | `selector, root = document` | Función interna de soporte del módulo. |
| `L19` | `$$` | `selector, root = document` | Función interna de soporte del módulo. |
| `L21` | `toolbar` | `` | Función interna de soporte del módulo. |
| `L22` | `statusbar` | `` | Función interna de soporte del módulo. |
| `L23` | `workspace` | `` | Función interna de soporte del módulo. |
| `L24` | `leftPanel` | `` | Función interna de soporte del módulo. |
| `L25` | `rightPanel` | `` | Función interna de soporte del módulo. |
| `L27` | `screenRatio` | `` | Función interna de soporte del módulo. |
| `L33` | `chooseMode` | `` | Función interna de soporte del módulo. |
| `L40` | `linkedSketchName` | `` | Función interna de soporte del módulo. |
| `L49` | `setTextIfChanged` | `node, text` | Asigna o muta el estado del componente. |
| `L53` | `syncSketchIdentity` | `` | Sincroniza el estado interno con la interfaz de usuario. |
| `L67` | `moveGateToStatusbar` | `` | Función interna de soporte del módulo. |
| `L77` | `moveLiveStatusToStatusbar` | `` | Función interna de soporte del módulo. |
| `L87` | `markToolbarCommands` | `` | Función interna de soporte del módulo. |
| `L97` | `proxyTarget` | `key` | Función interna de soporte del módulo. |
| `L105` | `ensureOverflow` | `` | Función interna de soporte del módulo. |
| `L156` | `syncOverflow` | `` | Sincroniza el estado interno con la interfaz de usuario. |
| `L169` | `ensureRightTabs` | `` | Función interna de soporte del módulo. |
| `L180` | `setRightView` | `view, persist = false` | Asigna o muta el estado del componente. |
| `L188` | `ensureCompactRightButtons` | `` | Función interna de soporte del módulo. |
| `L222` | `leftSections` | `` | Función interna de soporte del módulo. |
| `L233` | `ensureLeftRail` | `` | Función interna de soporte del módulo. |
| `L256` | `ensureScrim` | `` | Función interna de soporte del módulo. |
| `L271` | `syncDrawers` | `` | Sincroniza el estado interno con la interfaz de usuario. |
| `L281` | `bindCollapsibleSections` | `` | Función interna de soporte del módulo. |
| `L297` | `applyLeftDefaults` | `nextMode` | Función interna de soporte del módulo. |
| `L307` | `bindBottomPersistence` | `` | Función interna de soporte del módulo. |
| `L319` | `applyBottomDefault` | `nextMode` | Función interna de soporte del módulo. |
| `L328` | `ensureZoomOne` | `` | Función interna de soporte del módulo. |
| `L337` | `bestFitZoom` | `maxZoom = 8` | Función interna de soporte del módulo. |
| `L350` | `setZoom` | `value, persist = true` | Asigna o muta el estado del componente. |
| `L370` | `applyAdaptiveZoom` | `nextMode` | Función interna de soporte del módulo. |
| `L381` | `bindAdaptiveFit` | `` | Función interna de soporte del módulo. |
| `L405` | `syncToolbarLabels` | `` | Sincroniza el estado interno con la interfaz de usuario. |
| `L414` | `markStatusItems` | `` | Función interna de soporte del módulo. |
| `L431` | `syncLateNodes` | `` | Sincroniza el estado interno con la interfaz de usuario. |
| `L441` | `applyMode` | `force = false` | Función interna de soporte del módulo. |
| `L463` | `init` | `` | Inicializador del módulo y vinculación de eventos. |
| `L494` | `window.JWPLCHMIDesktopResponsive` | `` | Expone la API del módulo en el objeto global de ventana. |

### `gfx-classic-font.js`

- **Ruta absoluta:** [`gfx-classic-font.js`](file:///c:/Users/Andre/Documents/GitHub/platform-jwplc/tools/jwplc-hmi-designer/poc/gfx-classic-font.js)
- **Líneas totales:** `80` | **Tamaño:** `3.4 KB`

| Línea Aprox. | Función / Identificador | Parámetros | Descripción / Propósito |
| :---: | :--- | :--- | :--- |
| `L62` | `glyphFor` | `code` | Función interna de soporte del módulo. |

### `service-worker.js`

- **Ruta absoluta:** [`service-worker.js`](file:///c:/Users/Andre/Documents/GitHub/platform-jwplc/tools/jwplc-hmi-designer/poc/service-worker.js)
- **Líneas totales:** `68` | **Tamaño:** `2.1 KB`

*Este archivo no define funciones directamente (es un archivo de datos, markup, estilos o configuración).* 

### `ux-foundation.js`

- **Ruta absoluta:** [`ux-foundation.js`](file:///c:/Users/Andre/Documents/GitHub/platform-jwplc/tools/jwplc-hmi-designer/poc/ux-foundation.js)
- **Líneas totales:** `270` | **Tamaño:** `10.0 KB`

| Línea Aprox. | Función / Identificador | Parámetros | Descripción / Propósito |
| :---: | :--- | :--- | :--- |
| `L36` | `editor` | `` | Función interna de soporte del módulo. |
| `L40` | `hasFieldSelection` | `` | Función selectora / consulta de estado. |
| `L44` | `selectedField` | `` | Retorna el primer campo seleccionado o null si no hay ninguno. |
| `L48` | `selectedGeometry` | `` | Función interna de soporte del módulo. |
| `L52` | `badge` | `ctx, text, x, y` | Función interna de soporte del módulo. |
| `L72` | `regionRect` | `ctx, x, y, w, h, zoom, color, dash = [4, 3]` | Función interna de soporte del módulo. |
| `L82` | `drawHandles` | `ctx, left, top, width, height` | Renderizado gráfico del elemento correspondiente. |
| `L99` | `syncOverlaySize` | `` | Sincroniza el estado interno con la interfaz de usuario. |
| `L106` | `drawGeometryOverlay` | `` | Renderizado gráfico del elemento correspondiente. |
| `L136` | `updateStatus` | `` | Función interna de soporte del módulo. |
| `L155` | `refreshUX` | `` | Función interna de soporte del módulo. |
| `L160` | `exactFitZoom` | `` | Función interna de soporte del módulo. |
| `L167` | `applyExactFit` | `` | Función interna de soporte del módulo. |
| `L252` | `loadScript` | `src, onDone` | Función interna de soporte del módulo. |


## NAV_BAR (barra de navegación HMI como objeto)
- Tipo de campo global `NAV_BAR` (app.js: fieldsForPage/getFieldBounds/toolForField; se crea desde Tipos de Variable → Barra de Navegación). No genera entrada en HMI_FIELDS ni consume enum ID.
- Render: `JWPLCHMIPages.drawHMINavigationBar` se invoca desde `rebuildLogicalImage` (vive en logicalCanvas: lienzo, preview y minimapa).
- Inspector Homogéneo: `#navBarInspectorSection` estructurado con la arquitectura visual de `BAR` (`.bar-inspector-wrap`, pestañas `[Diseño]`, `[Páginas]`, `[Técnico]`, tarjetas `.bar-card` y selectores visuales `.bar-preset-btn`).
- Renombrar página: doble clic en la solapa de página (`rebuildTopTabs`, actualización no destructiva del DOM, timer de latch para prevenir colisión con clics simples).
- Gestión de páginas limpia: el disparador de configuración "Menú HMI" fue desacoplado de las pestañas superiores; la configuración responde únicamente a la selección del objeto `NAV_BAR`.
