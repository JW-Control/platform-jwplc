# Alpha11 — Arquitectura de JWPLC HMI Designer

Fecha de inicio: 2026-09-05

## Base exacta

```text
BASE_BRANCH=release/v2.1.x
BASE_SHA=db2f0b2ca2f091aa5fe95dee05ef957b3ebee5aa
WORK_BRANCH=v2.1.0-alpha.11/feature/hmi-designer
ALPHA10_STATUS=CLOSED_PUBLISHED
```

Alpha11 parte del cierre final documentado de Alpha10. No se reabre el alcance de build/library discovery cerrado en Alpha10.

---

## 1. Objetivo

Crear **JWPLC HMI Designer**, una herramienta visual específica para el **JWPLC Basic** que permita diseñar la pantalla TFT/HMI viendo el resultado a nivel de píxel y generar código compatible con la API pública actual de `JWPLC_Display`.

El objetivo de Alpha11 no es reemplazar `JWPLC_Display` ni crear un segundo runtime gráfico. El Designer será el frontend visual de la HMI declarativa ya validada.

Flujo objetivo:

```text
Diseñar visualmente
        ↓
Proyecto .jwhmi
        ↓
Generador JWPLC
        ↓
JWPLC_UIField[] + bindings + callbacks cortos
        ↓
Sketch Arduino
        ↓
JWPLC_Display / ST7789
```

Principio principal:

```text
DESIGNER_FRONTEND_OF_EXISTING_API=YES
SECOND_HMI_RUNTIME=NO
```

---

## 2. Contrato físico de pantalla

La implementación actual de `JWPLC_Display` inicializa:

```cpp
tft.init(170, 320);
tft.setRotation(3);
```

Por tanto, el espacio lógico que debe usar el Designer es:

```text
WIDTH=320
HEIGHT=170
CONTROLLER=ST7789
ROTATION=3
COLOR_FORMAT=RGB565
```

El canvas del Designer debe representar **320 × 170 píxeles reales**.

Los mockups conceptuales previos que usaban 128 × 96 fueron únicamente referencias visuales y no forman parte del contrato técnico.

---

## 3. API HMI heredada y protegida

Alpha11 debe preservar la API pública ya validada de `JWPLC_Display`.

### Tipos de campo soportados actualmente

```cpp
JWPLC_UI_FIELD_VALUE
JWPLC_UI_FIELD_TEXT
JWPLC_UI_FIELD_BOOL
JWPLC_UI_FIELD_BAR
```

### Límite actual

```cpp
JWPLC_UI_MAX_FIELDS = 32
```

### Modelo actual de campo

El Designer debe mapear sus propiedades a:

```text
meta
  id
  page
  type

rect
  x
  y
  width
  height

text
  label
  unit
  capacity

style
  labelTextSize
  valueTextSize
  frame
  layout
  valueAlign

colors
  label
  value
  background
  frame

format
  integerDigits
  decimalDigits
  signedValue
  leadingZeros

boolText
  falseText
  trueText

barRange
  min
  max
```

### Helpers públicos existentes

```cpp
JWPLC_UIValueField(...)
JWPLC_UITextField(...)
JWPLC_UIBoolField(...)
JWPLC_UIBarField(...)
```

### Actualización de valores

```cpp
JWPLC_Display.setValue(...)
JWPLC_Display.setText(...)
JWPLC_Display.setBool(...)
JWPLC_Display.setBar(...)
```

### Páginas y refresh

```cpp
JWPLC_Display.setUserPage(...)
JWPLC_Display.userPage()
JWPLC_Display.setUserRefreshMode(...)
JWPLC_Display.requestUserRefresh()
JWPLC_Display.invalidateField(...)
JWPLC_Display.invalidateAllFields()
```

### Callbacks cortos recomendados

```cpp
extern "C" void jwplcUIEnter();
extern "C" void jwplcUIPageEnter(uint8_t page);
extern "C" void jwplcUIUpdate();
extern "C" void jwplcUIExit();
```

Los callbacks históricos `jwplcUserDisplay*Callback()` permanecen por compatibilidad, pero el codegen nuevo del Designer no debe tomarlos como API principal.

---

## 4. Reglas de render que el Designer debe reproducir

La HMI actual usa estas constantes internas:

```text
FIELD_PADDING=3
FIELD_GAP=4
DEFAULT_BAR_WIDTH=80
DEFAULT_BAR_HEIGHT=12
```

El motor calcula geometría automática a partir de `label`, `unit`, formato, tamaño de texto, layout y tipo de campo.

El Designer debe implementar la misma geometría, no una aproximación visual independiente.

### Fuente V1

La HMI actual usa la fuente clásica fija de Adafruit GFX (`glcdfont.c`) al no seleccionar una `GFXfont` custom.

Para Alpha11 V1:

```text
FONT_ENGINE=ADAFRUIT_GFX_CLASSIC
FONT_CELL=6x8 por carácter a textSize=1
GLYPH_BITMAP=5x7 clásico + spacing de GFX
TEXT_SCALE=integer 1x, 2x, 3x...
```

El Designer debe generar los píxeles a partir del mismo bitmap, de forma que el preview pueda compararse con la TFT física.

### Dirty refresh

El Designer no modifica el modelo de refresh del runtime:

```text
STATIC -> se dibuja al entrar/cambiar página
DYNAMIC -> sólo región VALUE dirty
```

El codegen debe favorecer:

```cpp
USER_REFRESH_ON_DEMAND
```

cuando la interfaz no requiera refresco periódico forzado.

---

## 5. Alcance funcional Alpha11 V1

Alpha11 V1 debe soportar visualmente y en codegen:

- pantalla lógica 320 × 170;
- zoom del canvas;
- cuadrícula de píxel opcional;
- coordenadas X/Y;
- páginas USER;
- selección, movimiento y edición de objetos;
- `VALUE`;
- `TEXT`;
- `BOOL`;
- `BAR`;
- label;
- unit;
- formatos numéricos;
- textos booleanos;
- rango de barra;
- `INLINE` / `STACKED`;
- alineación LEFT/CENTER/RIGHT;
- frame;
- colores RGB565;
- bindings de variables/expresiones;
- preview de valores de diseño;
- codegen C++ determinista;
- exportación a archivos generados separados del código manual.

### Fuera de alcance inicial

No bloquear Alpha11 V1 por:

- gauge;
- imágenes BMP;
- iconos arbitrarios;
- animaciones;
- touch;
- fuentes custom;
- LVGL;
- OpenPLC/Ladder bindings;
- integración profunda con Arduino IDE;
- OTA;
- modificación del runtime para aumentar el máximo de 32 campos.

Estos puntos pueden evaluarse después de cerrar el núcleo visual y la paridad de codegen.

---

## 6. Arquitectura del Designer

Ruta inicial:

```text
tools/jwplc-hmi-designer/
```

Capas previstas:

```text
JWPLC HMI Designer
├── app/
│   ├── workspace
│   ├── canvas
│   ├── pages
│   ├── components
│   ├── inspector
│   └── preview
│
├── core/
│   ├── project-model
│   ├── field-model
│   ├── validation
│   ├── rgb565
│   └── gfx-classic-font
│
├── renderer/
│   ├── framebuffer-320x170
│   ├── field-geometry
│   └── field-renderer
│
├── generator/
│   ├── cpp-fields
│   ├── cpp-bindings
│   ├── callbacks
│   └── generated-files
│
└── integration/
    └── sketch-adapter
```

El primer PoC puede ejecutarse como herramienta web local. La integración con Arduino IDE será una capa posterior.

Regla:

```text
ARDUINO_IDE_INTEGRATION_LAYER=OPTIONAL
DESIGNER_CORE_DEPENDS_ON_ARDUINO_IDE=NO
```

---

## 7. Formato de proyecto `.jwhmi`

Se adopta provisionalmente un documento JSON versionado y legible.

Ejemplo conceptual:

```json
{
  "format": "jwplc-hmi",
  "version": 1,
  "target": {
    "device": "jwplc-basic-v2",
    "width": 320,
    "height": 170,
    "rotation": 3,
    "color": "rgb565"
  },
  "settings": {
    "refreshMode": "on-demand",
    "initialPage": 0
  },
  "pages": [
    {
      "id": 0,
      "name": "Principal"
    }
  ],
  "fields": [
    {
      "id": 1,
      "symbol": "FIELD_TEMP",
      "type": "VALUE",
      "page": 0,
      "rect": { "x": 20, "y": 40, "width": "AUTO", "height": "AUTO" },
      "text": { "label": "Temp", "unit": "C", "capacity": 12 },
      "style": {
        "labelTextSize": 1,
        "valueTextSize": 2,
        "frame": true,
        "layout": "INLINE",
        "valueAlign": "RIGHT"
      },
      "colors": {
        "label": "0xFFFF",
        "value": "0xFFFF",
        "background": "0x0000",
        "frame": "0xFFFF"
      },
      "format": {
        "integerDigits": 3,
        "decimalDigits": 1,
        "signedValue": true,
        "leadingZeros": false
      },
      "binding": {
        "expression": "temperature"
      },
      "preview": {
        "value": 25.6
      }
    }
  ]
}
```

El campo `preview` pertenece al Designer y no debe afectar el runtime generado.

---

## 8. Modelo de bindings

V1 no intentará parsear todo C++ automáticamente.

El usuario podrá escribir una expresión C++ válida:

```text
temperature
motorOn
nivelTanque
JWPLC_IO.inputs()
JWPLC_Time.hour()
```

El Designer genera dentro de `jwplcUIUpdate()` la llamada correspondiente al tipo:

```cpp
JWPLC_Display.setValue(FIELD_TEMP, temperature);
JWPLC_Display.setBool(FIELD_MOTOR, motorOn);
JWPLC_Display.setBar(FIELD_LEVEL, nivelTanque);
```

Regla de seguridad:

- el Designer no genera lecturas físicas lentas adicionales por sí mismo;
- bindings directos a fachadas cacheadas como `JWPLC_IO` / `JWPLC_Time` son válidos;
- el usuario sigue siendo dueño de la lógica de adquisición de sensores/comunicaciones.

---

## 9. Archivos generados

Objetivo:

```text
MiSketch/
├── MiSketch.ino
├── MiInterfaz.jwhmi
├── jwplc_hmi_generated.h
└── jwplc_hmi_generated.cpp
```

El Designer es dueño únicamente de los archivos `jwplc_hmi_generated.*`.

No debe sobrescribir arbitrariamente el `.ino`.

Integración mínima esperada:

```cpp
#include "jwplc_hmi_generated.h"
```

El adaptador de sketch podrá insertar esa línea sólo mediante una acción explícita y detectable.

---

## 10. Gates de Alpha11

### A11-0 — Arquitectura y contrato

PASS cuando:

- branch exacto creado desde cierre Alpha10;
- resolución física fijada en 320 × 170;
- API actual auditada;
- alcance V1 cerrado;
- formato `.jwhmi` definido;
- política de archivos generados definida.

### A11-1 — Pixel canvas

PASS cuando:

- canvas lógico 320 × 170;
- zoom permite inspeccionar píxeles individuales;
- grid opcional;
- cursor informa coordenadas exactas;
- framebuffer no usa interpolación visual.

### A11-2 — GFX text parity

PASS cuando:

- fuente clásica Adafruit GFX reproducida;
- texto se visualiza píxel por píxel;
- textSize entero reproduce escalado del runtime;
- muestras seleccionadas coinciden con captura/TFT física.

### A11-3 — Fields actuales

PASS cuando los cuatro tipos actuales se pueden crear, mover, configurar y visualizar:

```text
VALUE
TEXT
BOOL
BAR
```

### A11-4 — Codegen

PASS cuando:

- proyecto `.jwhmi` genera C++ determinista;
- helpers `JWPLC_UI*Field()` correctos;
- IDs únicos;
- páginas correctas;
- bindings correctos;
- código compilable en JWPLC Basic.

### A11-5 — Paridad física

PASS cuando una HMI creada visualmente:

1. se exporta;
2. compila con Arduino CLI/IDE;
3. se sube a JWPLC Basic;
4. coincide en posición, texto, colores y dimensiones con el Designer dentro de tolerancias documentadas.

### A11-6 — Integración de sketch

PASS cuando:

- exportación no destruye código manual;
- regeneración es idempotente;
- existe flujo claro `Diseñar -> Generar -> Compilar -> Subir`.

La integración profunda como extensión Arduino IDE puede continuar después del gate A11-6 si la API pública del IDE no ofrece todavía un contrato suficientemente estable.

---

## 11. Criterios de estabilidad

Alpha11 no debe:

- retirar periféricos del autoload;
- modificar Ethernet/RTU/TCA/RTC/FRAM/SD por necesidades del Designer;
- romper la HMI declarativa Alpha8/Alpha10;
- forzar LVGL;
- introducir memoria dinámica innecesaria en el runtime embebido;
- aumentar tráfico SPI por el hecho de usar el Designer;
- generar redibujado full-screen periódico cuando el runtime puede usar dirty regions.

---

## 12. Decisiones iniciales

```text
ALPHA11_SCOPE=JWPLC_HMI_DESIGNER_V1
TARGET_DISPLAY=ST7789_320x170_ROT3
EXISTING_DISPLAY_API=PROTECTED
EXISTING_FIELD_TYPES=VALUE_TEXT_BOOL_BAR
PROJECT_FORMAT=JWHMI_JSON_V1
PIXEL_PREVIEW=REQUIRED
ADAFRUIT_CLASSIC_FONT_PARITY=REQUIRED
CODEGEN=REQUIRED
SKETCH_MANUAL_CODE_OWNERSHIP=PRESERVED
ARDUINO_IDE_DEEP_INTEGRATION=DEFER_UNTIL_CORE_STABLE
OPENPLC_BINDINGS=OUT_OF_SCOPE_V1
```

## Estado

```text
A11_0_ARCHITECTURE=DEFINED
A11_1_PIXEL_CANVAS=PENDING
A11_2_GFX_TEXT_PARITY=PENDING
A11_3_FIELDS=PENDING
A11_4_CODEGEN=PENDING
A11_5_PHYSICAL_PARITY=PENDING
A11_6_SKETCH_INTEGRATION=PENDING
ALPHA11_STATUS=IN_PROGRESS
```
