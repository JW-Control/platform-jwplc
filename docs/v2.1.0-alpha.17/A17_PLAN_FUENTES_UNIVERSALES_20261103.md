# Plan de Implementación: Tipografía Universal Personalizable en HMI Designer y Firmware

> **Para agentes y desarrolladores:** Sub-skill requerida: `superpowers:executing-plans` o `superpowers:subagent-driven-development`.
> Cada paso usa la sintaxis de lista de tareas (`- [ ]`).

**Meta:** Implementar selección y renderizado de tipografías personalizadas (`FONT_16`, `FONT_26`, `FONT_7SEG_48` y FreeFonts) de forma universal e independiente para etiquetas (`labelFont`), valores/estados (`valueFont`), menús de configuración (`MENU`), historial de eventos (`EVENT_LOG`) y pestañas (`NAV_BAR`), garantizando paridad exacta entre el canvas del Designer y la pantalla física ST7789.

**Arquitectura:** 
1. Frontend: Actualizar el catálogo `designer-fonts.js` con las fuentes nativas de `JWPLC_TFT` 0.5.0 y renderizado real de glifos en `designer-fonts-data.js`.
2. Inspectores y Modelo: Agregar `labelFont` y `valueFont` independientes a `VALUE`, `BOOL`, `TEXT`, y `font` a `MENU` y `EVENT_LOG`.
3. Canvas: Adaptar `computeFieldGeometry()` y `drawField()` en `app.js`, `designer-menu.js` y `designer-event-log.js`.
4. Firmware: Ampliar `JWPLC_UIStyle`, `JWPLC_UI_Menu` y `JWPLC_UI_EventLog` con punteros `const JWPLC_TFTFont *` y enrutamiento en `tft.setFont()`.
5. Codegen: Emitir `&JWPLC_FONT_*` en `JWPLC_HMI_Generated.h` con retrocompatibilidad absoluta (fallback a GLCD si es nulo).

**Archivos involucrados:**
- `tools/jwplc-hmi-designer/poc/designer-fonts.js`
- `tools/jwplc-hmi-designer/poc/designer-fonts-data.js`
- `tools/jwplc-hmi-designer/poc/app.js`
- `tools/jwplc-hmi-designer/poc/designer-field-inspector.js`
- `tools/jwplc-hmi-designer/poc/designer-menu.js`
- `tools/jwplc-hmi-designer/poc/designer-event-log.js`
- `tools/jwplc-hmi-designer/poc/designer-codegen.js`
- `tools/jwplc-hmi-designer/cli/lib/spec.cjs`
- `tools/jwplc-hmi-designer/schema/jwhmi.schema.json`
- `JWPLC/2.1.0/libraries/JWPLC_Display/src/JWPLC_UI.h`
- `JWPLC/2.1.0/libraries/JWPLC_Display/src/JWPLC_UI.cpp`
- `JWPLC/2.1.0/libraries/JWPLC_Display/src/JWPLC_UI_Menu.h`
- `JWPLC/2.1.0/libraries/JWPLC_Display/src/JWPLC_UI_Menu.cpp`
- `JWPLC/2.1.0/libraries/JWPLC_Display/src/JWPLC_UI_EventLog.h`
- `JWPLC/2.1.0/libraries/JWPLC_Display/src/JWPLC_UI_EventLog.cpp`

---

## Tareas de Implementación

### Tarea 1: Catálogo y Bitmaps de Fuentes en el Frontend
**Archivos:**
- Modificar: `tools/jwplc-hmi-designer/poc/designer-fonts.js`
- Modificar: `tools/jwplc-hmi-designer/poc/designer-fonts-data.js`

- [x] **Paso 1.1:** Habilitar `FONT_16` y `FONT_26` en `FONTS`, marcándolas como soportadas en firmware (`JWPLC_TFT >= 0.5.0`).
- [x] **Paso 1.2:** Agregar la entrada `FONT_7SEG_48` (48 px de 7 segmentos) con la propiedad `numericOnly: true` y símbolos válidos `-.0123456789:`.
- [x] **Paso 1.3:** Añadir función clasificadora `availableFontsFor(targetType)` para filtrar `FONT_7SEG_48` únicamente cuando el campo sea numérico o reloj (`VALUE`), ocultándola en campos de texto alfabético (`TEXT`, `label`, `MENU`).
- [x] **Paso 1.4:** Incorporar los datos de glifos y anchos de celda de `Font16`, `Font26` y `Font7Seg48` a `designer-fonts-data.js` para renderizado en canvas HTML5.
- [x] **Paso 1.5:** Verificar sintaxis y carga limpia con `node --check tools/jwplc-hmi-designer/poc/designer-fonts.js`.

---

### Tarea 2: Modelo de Datos y Esquema JSON
**Archivos:**
- Modificar: `tools/jwplc-hmi-designer/cli/lib/spec.cjs`
- Modificar: `tools/jwplc-hmi-designer/schema/jwhmi.schema.json`

- [x] **Paso 2.1:** Extender los tipos `VALUE`, `BOOL` y `TEXT` en `spec.cjs` con `labelFont` (string, default `'GLCD'`) y `valueFont` (string, default `'GLCD'`).
- [x] **Paso 2.2:** Extender el componente `MENU` con `font` (string, default `'GLCD'`).
- [x] **Paso 2.3:** Extender el componente `EVENT_LOG` con `rowFont` y `headerFont` (string, default `'GLCD'`).
- [x] **Paso 2.4:** Regenerar y validar el esquema con `node tools/jwplc-hmi-designer/cli/jwhmi.cjs schema`.

---

### Tarea 3: Motor de Geometría y Pintor en Canvas
**Archivos:**
- Modificar: `tools/jwplc-hmi-designer/poc/app.js`
- Modificar: `tools/jwplc-hmi-designer/poc/designer-menu.js`
- Modificar: `tools/jwplc-hmi-designer/poc/designer-event-log.js`

- [x] **Paso 3.1:** En `app.js:computeFieldGeometry(field)`: medir `labelBounds` usando la fuente `field.labelFont` y `valueBounds` usando `field.valueFont`, adaptando el ancho total (`fieldW`) y alto total (`fieldH`) a las dimensiones reales del texto.
- [x] **Paso 3.2:** En `app.js:drawField(buffer, field)`: dibujar la etiqueta (`field.label`) y unidad con `field.labelFont`, y el valor (`previewTextForField`) con `field.valueFont`.
- [x] **Paso 3.3:** En `designer-menu.js`: consultar `menu.font`. Si es distinto de GLCD, calcular la altura de fila según la fuente y trazar los textos de cada nodo con los glifos de dicha fuente.
- [x] **Paso 3.4:** En `designer-event-log.js`: trazar cabeceras con `headerFont` y registros con `rowFont`.
- [x] **Paso 3.5:** Comprobar que en el lienzo los campos existentes en GLCD no sufren desplazamientos ni alteraciones visuales.

---

### Tarea 4: Inspectores de Usuario (UI)
**Archivos:**
- Modificar: `tools/jwplc-hmi-designer/poc/designer-field-inspector.js`
- Modificar: `tools/jwplc-hmi-designer/poc/designer-menu.js`
- Modificar: `tools/jwplc-hmi-designer/poc/designer-event-log.js`

- [x] **Paso 4.1:** En `designer-field-inspector.js` para `VALUE`, `BOOL` y `TEXT`:
  - Agregar en la pestaña *Forma / Diseño* el bloque **Tipografía**:
    - Selector visual de **Fuente de Etiqueta** (`labelFont`).
    - Selector visual de **Fuente del Valor / Estado** (`valueFont`).
  - Mostrar la opción `7-Segmentos 48 px` únicamente en campos de tipo `VALUE` (numérico, duración, fecha/hora).
- [x] **Paso 4.2:** En el inspector de `MENU`: agregar selector visual **Tipografía del Menú** en la sección de diseño.
- [x] **Paso 4.3:** En el inspector de `EVENT_LOG`: agregar selector visual **Tipografía de Filas** en la pestaña de diseño.
- [x] **Paso 4.4:** Conectar los eventos de cambio para actualizar el modelo, registrar paso en el historial (undo/redo) y refrescar el mini-modelo de Apariencia.

---

### Tarea 5: Firmware C++ en `JWPLC_Display`
**Archivos:**
- Modificar: `JWPLC/2.1.0/libraries/JWPLC_Display/src/JWPLC_UI.h`
- Modificar: `JWPLC/2.1.0/libraries/JWPLC_Display/src/JWPLC_UI.cpp`
- Modificar: `JWPLC/2.1.0/libraries/JWPLC_Display/src/JWPLC_UI_Menu.h`
- Modificar: `JWPLC/2.1.0/libraries/JWPLC_Display/src/JWPLC_UI_Menu.cpp`
- Modificar: `JWPLC/2.1.0/libraries/JWPLC_Display/src/JWPLC_UI_EventLog.h`
- Modificar: `JWPLC/2.1.0/libraries/JWPLC_Display/src/JWPLC_UI_EventLog.cpp`

- [x] **Paso 5.1:** En `JWPLC_UI.h`, extender `JWPLC_UIStyle`:
  ```cpp
  const JWPLC_TFTFont *labelFont = nullptr;
  const JWPLC_TFTFont *valueFont = nullptr;
  ```
  Actualizar los constructores de conveniencia `JWPLC_UIValueStyle`, `JWPLC_UITextFieldStyle`, etc., permitiendo pasar estos punteros como argumentos opcionales finales.
- [x] **Paso 5.2:** En `JWPLC_UI.cpp:computeFieldGeometry()`: activar `tft.setFont(*def.style.labelFont)` al medir etiqueta/unidad, y `tft.setFont(*def.style.valueFont)` al medir el valor.
- [x] **Paso 5.3:** En `JWPLC_UI.cpp:drawFieldStatic()` y `drawFieldValue()`: activar la fuente antes de imprimir y asegurar que la altura del borrado (`fillRect`) limpie exactamente el área de la fuente asignada.
- [x] **Paso 5.4:** En `JWPLC_UI_Menu.h/.cpp`: añadir miembro `const JWPLC_TFTFont *font = nullptr;` y usarlo al trazar filas.
- [x] **Paso 5.5:** En `JWPLC_UI_EventLog.h/.cpp`: añadir `const JWPLC_TFTFont *rowFont = nullptr;` y usarlo en la lista de eventos.

---

### Tarea 6: Generador de Código C++ (`designer-codegen.js`)
**Archivos:**
- Modificar: `tools/jwplc-hmi-designer/poc/designer-codegen.js`
- Modificar: `tools/jwplc-hmi-designer/poc/designer-codegen-core.js`

- [x] **Paso 6.1:** Al emitir la definición de `HMI_FIELDS`, si `field.labelFont` o `field.valueFont` difieren de `GLCD`, pasar sus símbolos (`&JWPLC_FONT_16`, `&JWPLC_FONT_7SEG_48`, etc.) al inicializador de estilo.
- [x] **Paso 6.2:** Al emitir la inicialización de `MENU` o `EVENT_LOG`, incluir la llamada de asignación de fuente correspondiente.
- [x] **Paso 6.3:** Si el proyecto usa fuentes personalizadas, asegurar la inclusión de `#include <JWPLC_TFT.h>` y la guarda `#if JWPLC_HMI_TFT_FONTS`.
- [x] **Paso 6.4:** Verificar que un proyecto que solo usa `GLCD` genera un header 100% idéntico a los golden existentes.

---

### Tarea 7: Validación, Tests y Regresión
**Archivos:**
- Ejecutar pruebas en `tools/jwplc-hmi-designer/tests/`

- [x] **Paso 7.1:** Ejecutar `node tools/jwplc-hmi-designer/tests/codegen-core.test.cjs` para verificar que el codegen puro no rompe.
- [x] **Paso 7.2:** Comprobar paridad visual en el diseñador (lienzo HTML5 vs renderizado esperado).
- [x] **Paso 7.3:** Ejecutar suite completa `node tools/jwplc-hmi-designer/tests/run-all.cjs`.
