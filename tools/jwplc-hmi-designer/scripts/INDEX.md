# Índice de Scripts y Utilidades: `scripts/`

Este directorio contiene herramientas de automatización y preprocesamiento utilizadas durante el desarrollo y compilación de recursos del **JWPLC HMI Designer**.

---

## 1. Estructura de Archivos

| Archivo | Tipo | Tamaño / Líneas | Propósito Principal |
| :--- | :---: | :---: | :--- |
| [`generate-gfx-fonts.js`](file:///c:/Users/Andre/Documents/GitHub/platform-jwplc/tools/jwplc-hmi-designer/scripts/generate-gfx-fonts.js) | JavaScript (Node.js) | 134 líneas (~6.5 KB) | Generador de fuentes: descarga/lee las fuentes originales de `TFT_eSPI` y produce el archivo `poc/designer-fonts-data.js`. |

---

## 2. Detalle Exhaustivo de Funciones (`generate-gfx-fonts.js`)

| Línea Aprox. | Función | Parámetros | Descripción / Propósito |
| :---: | :--- | :--- | :--- |
| `L16` | `readSource` | `relativePath, dir` | Lee un archivo de cabecera C/C++ desde el disco local si se pasa una ruta; si no, lo descarga directamente desde el repositorio oficial de GitHub de `Bodmer/TFT_eSPI`. |
| `L23` | `parseGfxHeader` | `name, source` | Analiza el código de una cabecera de Adafruit GFX (`GFXfont`), extrayendo el rango de glifos (`first`, `last`), avance vertical (`yAdvance`), tabla de glifos y el mapa de bits empaquetado en Base64. |
| `L40` | `preprocess` | `source, defined` | Emulador ligero del preprocesador de C (`#ifdef`, `#ifndef`, `#else`, `#endif`, `#define`) para filtrar bloques condicionales en fuentes de TFT_eSPI. |
| `L61` | `parseTftBitmapFont` | `source, prefix, height, rle, defined` | Decodifica fuentes bitmap clásicas de `TFT_eSPI` (Font 2 en `Font16.c` y Font 4 en `Font32rle.c`), descomprimiendo el RLE si aplica y empaquetándolas en formato binario continuo MSB-first para el canvas web. |
| `L108` | *IIFE Principal* | Ninguno | Itera sobre las familias FreeSans, FreeSerif, FreeMono (tamaños 9, 12, 18, 24 regular y negrita) y las fuentes bitmap TFT, y escribe el resultado estructurado en `poc/designer-fonts-data.js`. |

---

## 3. Modo de Uso

```powershell
# Ejecución estándar (descarga desde GitHub automáticamente)
node tools/jwplc-hmi-designer/scripts/generate-gfx-fonts.js

# Ejecución con carpeta local de TFT_eSPI Fonts
node tools/jwplc-hmi-designer/scripts/generate-gfx-fonts.js "C:\ruta\a\TFT_eSPI\Fonts"
```
