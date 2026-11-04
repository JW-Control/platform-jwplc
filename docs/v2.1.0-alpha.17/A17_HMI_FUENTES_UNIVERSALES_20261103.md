# Alpha17 · A17-HMI — Tipografía Universal Personalizable en HMI Designer y Firmware

Fecha: `2026-11-03`  
Alcance: `tools/jwplc-hmi-designer` y `JWPLC/2.1.0/libraries/JWPLC_Display`  
Estado: `IMPLEMENTADO Y VERIFICADO` (Commit `09717a38`)

```text
HMI_FONTS_CATALOG=PASS (FONT_16, FONT_26, FONT_7SEG_48, FreeFonts con glifos reales)
HMI_DATA_SPEC_SCHEMA=PASS (labelFont, valueFont, font, headerFont, rowFont)
HMI_CANVAS_RENDERING=PASS (geometría y trazado con fuentes independientes en tiempo real)
HMI_INSPECTORS_UI=PASS (tarjeta Tipografía en VALUE, BOOL, TEXT, MENU y EVENT_LOG con undo/redo)
HMI_FIRMWARE_CPP=PASS (JWPLC_UIStyle, JWPLC_UIMenu, JWPLC_UIEventLog con const JWPLC_TFTFont*)
HMI_CODEGEN_CPP=PASS (emisión &JWPLC_FONT_* manteniendo paridad total con golden GLCD)
HMI_TESTS_PASS=PASS (codegen-core.test.cjs 100% sin regresiones)
```

---

## 1. Contexto y Diagnóstico

En versiones anteriores a Alpha 17, el soporte tipográfico en el HMI Designer presentaba limitaciones críticas:
1. **Selector exclusivo para texto estático (`RAW_TEXT`):** Las fuentes personalizadas solo estaban habilitadas para etiquetas fijas. Todos los campos dinámicos de datos (`VALUE`, `BOOL`, `TEXT`) y los módulos complejos (`MENU`, `EVENT_LOG`) estaban forzados a la fuente clásica matricial por defecto (`GLCD` 5×7).
2. **Falta de independencia entre Etiqueta y Valor:** En instrumentación industrial resulta indispensable jerarquizar visualmente los elementos: por ejemplo, mostrar el valor medido en una tipografía grande o display de 7 segmentos de 48 px, mientras la etiqueta ("Presión", "Nivel") permanece legible en una fuente Bitmap o Sans de menor escala.
3. **Restricción de fuentes alfanuméricas vs. numéricas:** Fuentes especializadas como `FONT_7SEG_48` (display digital de 7 segmentos) carecen de caracteres alfabéticos completos; si un usuario las asignaba a una etiqueta o texto arbitrario, la pantalla no podía representarlo.
4. **Paridad gráfica entre lienzo y display físico:** El dibujo en el canvas HTML5 debía coincidir píxel a píxel con el renderizado nativo de la pantalla TFT ST7789 ejecutada por el firmware C++ (`JWPLC_Display`).

---

## 2. Arquitectura de la Solución

Se implementó una solución de tipografía universal desacoplada en siete capas:

### 2.1 Catálogo y Glifos Reales (`designer-fonts.js` y `designer-fonts-data.js`)
- Se habilitaron formalmente en el catálogo:
  - `FONT_16` (Font 2 de TFT_eSPI, bitmap de 16 px de alto).
  - `FONT_26` (Font 4 de TFT_eSPI, bitmap de 26 px de alto).
  - `FONT_7SEG_48` (Font 7 de TFT_eSPI, display de 7 segmentos de 48 px de alto, con `numericOnly: true`).
  - Familias FreeFonts (`SANS_9`, `SANS_12`, `SERIF_9`, `MONO_9`, etc.).
- Se implementó `availableFontsFor(targetType)`:
  - Para campos numéricos (`VALUE` en su valor): permite todas las fuentes, incluyendo `FONT_7SEG_48`.
  - Para etiquetas, textos (`TEXT`), booleanos (`BOOL`), menús (`MENU`) y registros (`EVENT_LOG`): filtra automáticamente cualquier fuente con `numericOnly: true`, evitando glifos inexistentes.
- Se implementó `cppSymbol(id)`: mapea de forma segura a constantes de C++ (`JWPLC_FONT_GLCD`, `JWPLC_FONT_16`, `JWPLC_FONT_26`, `JWPLC_FONT_7SEG_48`).
- En `designer-fonts-data.js`, se inyectaron 95 glifos con su mapa de bits real extraído de `JWPLC_TFT_Font7Seg48.cpp`.
- Se adaptó `designer-fonts.js` con UMD / CommonJS para cargar limpiamente tanto en navegadores como en entornos Node.js y sandboxes `vm` sin DOM.

### 2.2 Modelo de Datos y Validación (`spec.cjs`, `validate.cjs`, `jwhmi.schema.json`)
- **Campos dinámicos (`VALUE`, `BOOL`, `TEXT`):**
  - `labelFont`: ID de fuente para la etiqueta (fallback: `'GLCD'`).
  - `valueFont`: ID de fuente para el valor mostrado (fallback: `'GLCD'`).
- **Menú de configuración (`MENU`):**
  - `font`: ID de fuente para títulos y nodos del menú (fallback: `'GLCD'`).
- **Registro de eventos (`EVENT_LOG`):**
  - `headerFont`: ID de fuente para la cabecera (fallback: `'GLCD'`).
  - `rowFont`: ID de fuente para las filas de eventos y footer (fallback: `'GLCD'`).
- Se validaron reglas en `validate.cjs` y se regeneró el esquema oficial mediante `node tools/jwplc-hmi-designer/cli/jwhmi.cjs schema`.

### 2.3 Motor de Geometría y Pintor en Lienzo (`app.js`, `designer-menu.js`, `designer-event-log.js`)
- `computeFieldGeometry(field)` y `alignedValueX()` en `app.js`:
  - Miden la etiqueta y unidad con `rawTextBounds(..., field.labelFont)`.
  - Miden el valor numérico/texto/booleano con `rawTextBounds(..., field.valueFont)`.
  - Calculan los paddings, offsets y dimensiones automáticas asegurando que los textos no se desborden ni colisionen.
- `drawField(field)` en `app.js`:
  - Dibuja la etiqueta y unidad con `field.labelFont`.
  - Dibuja el valor con `field.valueFont`.
- `designer-menu.js`: calcula la altura de filas con `lineHeight(font)` y dibuja con `drawMenuText(...)`.
- `designer-event-log.js`: dibuja el encabezado con `field.headerFont` y las filas del log con `field.rowFont`.

### 2.4 Inspectores de Usuario / UI (`designer-field-inspector.js`, `designer-menu.js`, `designer-event-log.js`)
- **Inspector de Campos Dinámicos (`designer-field-inspector.js`):**
  - Tarjeta colapsable **Tipografía** con dos selectores independientes:
    - *Fuente de Etiqueta*: poblado mediante `availableFontsFor('label')` (oculta `FONT_7SEG_48`).
    - *Fuente de Valor*: poblado mediante `availableFontsFor(field.type)` (incluye `FONT_7SEG_48` en `VALUE`).
  - Integrado al ciclo de vida del inspector con persistencia en el proyecto y soporte completo del historial Undo/Redo.
- **Inspector de Menú (`designer-menu.js`):**
  - Tarjeta **Tipografía** en la pestaña de Forma con selector `data-menu-font`.
- **Inspector de Historial (`designer-event-log.js`):**
  - Tarjeta **Tipografía** con selectores para `headerFont` y `rowFont`.

---

## 3. Firmware C++ (`JWPLC_Display`)

Se actualizaron las estructuras públicas y los motores de renderizado manteniendo compatibilidad binaria y de código fuente total:

### 3.1 Estilos de Campos (`JWPLC_UI.h` y `JWPLC_UI_API.cpp`)
```cpp
struct JWPLC_UIStyle
{
    uint8_t labelTextSize;
    uint8_t valueTextSize;
    bool frame;
    JWPLC_UIFieldLayout layout;
    JWPLC_UIValueAlign valueAlign;
    JWPLC_UIColors colors;

    const JWPLC_TFTFont *labelFont = nullptr;
    const JWPLC_TFTFont *valueFont = nullptr;

    JWPLC_UIStyle(
        uint8_t labelSizeValue = 1,
        uint8_t valueSizeValue = 2,
        bool frameValue = false,
        JWPLC_UIFieldLayout layoutValue = JWPLC_UI_LAYOUT_INLINE,
        JWPLC_UIValueAlign alignValue = JWPLC_UI_ALIGN_RIGHT,
        JWPLC_UIColors colorsValue = JWPLC_UIColors(),
        const JWPLC_TFTFont *labelFontValue = nullptr,
        const JWPLC_TFTFont *valueFontValue = nullptr);
};
```
Los constructores auxiliares `JWPLC_UIValueStyle(...)`, `JWPLC_UITextFieldStyle(...)` y `JWPLC_UIBoolStyle(...)` incorporaron los parámetros opcionales al final con valor por defecto `nullptr`.

### 3.2 Renderizado en Display (`JWPLC_UI.cpp`)
- `textBounds()` y `computeFieldGeometry()` conmutan temporalmente `tft.setFont(*style.labelFont)` o `tft.setFont(*style.valueFont)`.
- `drawFieldStatic()` renderiza la etiqueta con `labelFont` y restaura inmediatamente `tft.setFont(JWPLC_FONT_GLCD)`.
- `drawFieldValue()` renderiza el valor con `valueFont` y restaura `JWPLC_FONT_GLCD`.

### 3.3 Menús y Registro de Eventos (`JWPLC_UI_Menu.*`, `JWPLC_UI_EventLog.*`)
- `JWPLC_UIMenu` y `JWPLC_UIMenuField()` incorporan `const JWPLC_TFTFont *font = nullptr`.
- `JWPLC_UIEventLog` incorpora `const JWPLC_TFTFont *headerFont = nullptr;` y `const JWPLC_TFTFont *rowFont = nullptr;`.
- Los métodos `drawText()` de ambos módulos aplican la fuente correspondiente y retornan el estado de la TFT a GLCD.

---

## 4. Generador de Código C++

En `designer-codegen-core.js`, `designer-menu.js` y `designer-event-log.js`:
1. **Emisión condicional limpia:**
   - Si un campo usa `labelFont` o `valueFont` distinto a `'GLCD'`, se emite el puntero `&JWPLC_FONT_*` (ej. `&JWPLC_FONT_16`, `&JWPLC_FONT_7SEG_48`).
   - Si no se configuran fuentes personalizadas (o se usa `'GLCD'`), los constructores omiten los parámetros finales.
2. **Ejemplo de código generado:**
```cpp
    // Campo VALUE con fuentes personalizadas
    JWPLC_UIValueField(
        FIELD_TEMP,
        JWPLC_UIRect(10, 20),
        JWPLC_UIText("Temp", "C"),
        JWPLC_UIValueFormat(3, 1, true, false),
        JWPLC_UIValueStyle(
            2,
            1,
            false,
            JWPLC_UI_LAYOUT_INLINE,
            JWPLC_UI_ALIGN_RIGHT,
            &JWPLC_FONT_16,
            &JWPLC_FONT_7SEG_48),
        0,
        JWPLC_UIColors(0xFFFF, 0x07FF, 0x0000, 0xFFFF))
```

---

## 5. Pruebas y Verificación

Se agregaron pruebas unitarias completas a `tools/jwplc-hmi-designer/tests/codegen-core.test.cjs`:
- `VALUE` con `labelFont: 'FONT_16'` y `valueFont: 'FONT_7SEG_48'`: PASS.
- `VALUE` con sólo `valueFont` (emite `nullptr` en `labelFont`): PASS.
- `TEXT` con `labelFont: 'FONT_26'` y `valueFont: 'FONT_16'`: PASS.
- `BOOL` con `labelFont: 'FONT_16'` y `valueFont: 'FONT_26'`: PASS.
- `MENU` con `font: 'FONT_16'`: PASS.
- `EVENT_LOG` con `headerFont: 'FONT_16'` y `rowFont: 'FONT_26'`: PASS.
- Paridad exacta con proyectos GLCD preexistentes sin alteración de fixtures: PASS.
- **Resultado:** 100% de las pruebas del núcleo y de componentes pasaron exitosamente.
