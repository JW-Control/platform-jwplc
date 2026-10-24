# Índice de Validación y Gates Físicos: `gates/`

Este directorio contiene los **Gates de Aceptación (Acceptance Gates)**, sketches de verificación física para Arduino/ESP32 y scripts de prueba para la versión **Alpha11** del **JWPLC HMI Designer**.

Cada subcarpeta corresponde a un hito de validación técnica que garantiza la paridad pixel-perfect entre lo que se diseña en pantalla y lo que se compila y ejecuta en el hardware **JWPLC Basic** con pantalla ST7789 (320 × 170 px).

---

## 1. Resumen de Gates y Subdirectorios

| Subdirectorio / Gate | Archivos Contenidos | Propósito y Criterio de Aceptación |
| :--- | :--- | :--- |
| [`A11_2_GFX_Text_Parity/`](file:///c:/Users/Andre/Documents/GitHub/platform-jwplc/tools/jwplc-hmi-designer/gates/A11_2_GFX_Text_Parity/) | `A11_2_GFX_Text_Parity.ino`<br>`README.md` | Compara el renderizado de texto clásico Adafruit GFX RAW (tamaño 2, colores, coordenadas) entre el simulador web y la TFT física. |
| [`A11_2B_Public_API_Text_Field/`](file:///c:/Users/Andre/Documents/GitHub/platform-jwplc/tools/jwplc-hmi-designer/gates/A11_2B_Public_API_Text_Field/) | `A11_2B_Public_API_Text_Field.ino` | Valida que los campos de texto (`TEXT`) se compilen y muestren correctamente usando exclusivamente la API pública (`JWPLC_UITextField`). |
| [`A11_2C_Balanced_Public_API/`](file:///c:/Users/Andre/Documents/GitHub/platform-jwplc/tools/jwplc-hmi-designer/gates/A11_2C_Balanced_Public_API/) | `Apply-A11-2C-TightMetrics.ps1` | Script de automatización en PowerShell que aplica métricas ajustadas de fuentes en `JWPLC_UI.cpp` de la biblioteca para coincidir con la web. |
| [`A11_3A_TEXT_Field_Parity/`](file:///c:/Users/Andre/Documents/GitHub/platform-jwplc/tools/jwplc-hmi-designer/gates/A11_3A_TEXT_Field_Parity/) | `A11_3A_TEXT_Field_Parity.ino` | Prueba física de paridad de cajas delimitadoras, padding y borrado de fondo sin artefactos gráficos. |
| [`A11_7A_Inspector_Exclusivo/`](file:///c:/Users/Andre/Documents/GitHub/platform-jwplc/tools/jwplc-hmi-designer/gates/A11_7A_Inspector_Exclusivo/) | `README.md` | Documentación del gate de UI: garantiza que el inspector derecho oculte controles de fields cuando se edita un PixelMap y viceversa. |
| [`A11_7B_Pixel_Layer_Editing/`](file:///c:/Users/Andre/Documents/GitHub/platform-jwplc/tools/jwplc-hmi-designer/gates/A11_7B_Pixel_Layer_Editing/) | `README.md` | Especificación de edición de píxeles por capas, previsualización de transparencia y soporte para frames de animación. |
| [`A11_7C_Pixel_Workbench/`](file:///c:/Users/Andre/Documents/GitHub/platform-jwplc/tools/jwplc-hmi-designer/gates/A11_7C_Pixel_Workbench/) | `README.md` | Especificación de herramientas pixel art: cursor con huella real, rectángulos guía e historial independiente de `Ctrl+Z` para dibujo. |
| [`A11_7D_Pixel_Codegen_Optimizer/`](file:///c:/Users/Andre/Documents/GitHub/platform-jwplc/tools/jwplc-hmi-designer/gates/A11_7D_Pixel_Codegen_Optimizer/) | `README.md`<br>`ENUM_DEDUP_FIX.md` | Documentación del optimizador de código C++: compresión en tramos RLE horizontales/verticales y corrección de enums duplicados. |
| [`A11_7E_Project_Link_Persistence/`](file:///c:/Users/Andre/Documents/GitHub/platform-jwplc/tools/jwplc-hmi-designer/gates/A11_7E_Project_Link_Persistence/) | `README.md` | Especificación del contrato de persistencia de archivos (`.jwhmi` + `.ino` + `JWPLC_HMI_Generated.h`) sin pérdida de PixelMaps. |
| [`A11_LivePreview_WebSerial/`](file:///c:/Users/Andre/Documents/GitHub/platform-jwplc/tools/jwplc-hmi-designer/gates/A11_LivePreview_WebSerial/) | `JWPLC_HMI_Live_Bridge.ino` | **Firmware de puente Live Preview**: corre en el hardware receptor escuchando en WebSerial a 921600 baud para renderizar el lienzo en vivo en la pantalla física. |
| [`A11_Pages_Navigation/`](file:///c:/Users/Andre/Documents/GitHub/platform-jwplc/tools/jwplc-hmi-designer/gates/A11_Pages_Navigation/) | `JWPLC_HMI_Pages_Gate.ino` | Sketch de prueba en hardware para navegación de múltiples páginas mediante los botones físicos (UP, DOWN, LEFT, RIGHT, OK). |

---

## 2. Detalle de Archivos Destacados

### `A11_LivePreview_WebSerial/JWPLC_HMI_Live_Bridge/JWPLC_HMI_Live_Bridge.ino`
- **Líneas:** 562
- **Propósito:** Actúa como firmware de desarrollo para Live Preview.
- **Protocolo:** Web Serial a `921600` baud.
- **Formato:** Paquetes binarios con cabecera de sincronización, comandos de frame completo o rectángulos sucios (*dirty rects*) con compresión RLE de 16 bits (RGB565).
- **Funciones clave:**
  - `processPacket()`: Decodifica comandos de frame o región.
  - `drawRleRegion()`: Vuelca píxeles descomprimidos directamente vía SPI al controlador ST7789.
  - `sendStats()`: Informa al navegador de FPS recibidos y latencia.

### `A11_Pages_Navigation/JWPLC_HMI_Pages_Gate/JWPLC_HMI_Pages_Gate.ino`
- **Líneas:** 162
- **Propósito:** Demuestra la máquina de estados de páginas en C++:
  - Modo `PAGE_SELECT`: Flechas cambian de página, `OK` entra al contenido.
  - Modo `PAGE_CONTENT`: Flechas interactúan con campos, `OK` mantenido regresa al selector.
