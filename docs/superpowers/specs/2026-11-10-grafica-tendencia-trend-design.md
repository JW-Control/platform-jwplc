# Especificación de Diseño: Widget de Gráfica de Tendencia (`TREND`) para HMI Designer

**Fecha:** 2026-11-10  
**Línea / Versión:** v2.1.0-alpha.17 (`tools/jwplc-hmi-designer` y firmware `JWPLC_UI_Widgets` / `JWPLC_Display`)  
**Estado:** Especificación de Diseño  
**Referencia:** Proyecto Sensor COD [`SENSOR_COD_WIFI_REVISION_25_08.ino`](file:///D:/SENSOR_COD_WIFI_REVISION_24_07/SENSOR_COD_WIFI_REVISION_25_08/SENSOR_COD_WIFI_REVISION_25_08.ino)

---

## 1. Intención y Justificación

### 1.1 Contexto
En sistemas de automatización e instrumentación industrial (temperatura en hornos, presión en tuberías, nivel en tanques, pH/turbidez en tratamiento de agua), monitorear una variable solo con un valor numérico (`VALUE`) o una barra de nivel (`BAR`) es insuficiente: el operador necesita observar la **tendencia temporal** (si el valor está subiendo, bajando o estabilizado), la velocidad de cambio y los picos históricos.

En el proyecto de referencia ([`SENSOR_COD_WIFI_REVISION_25_08.ino`](file:///D:/SENSOR_COD_WIFI_REVISION_24_07/SENSOR_COD_WIFI_REVISION_25_08/SENSOR_COD_WIFI_REVISION_25_08.ino)) se implementó con éxito una gráfica de instrumentación con auto-escala dinámica de 3 guías, borrado diferencial sin parpadeo y trazo grueso de 2 px. Sin embargo, su historial en pantalla estaba limitado rígidamente a 30 minutos (1800 muestras) en RAM y no estaba integrado en el HMI Designer.

### 1.2 Objetivo
Crear un nuevo tipo de campo visual **`TREND` (Gráfica de Tendencia)** en `tools/jwplc-hmi-designer`:
1. **Widget flexible (Enfoque A):** Ubicable en cualquier posición `(x, y)` y dimensiones `(w, h)`, pudiendo ser un recuadro compacto o abarcar la pantalla completa.
2. **Muestreo configurable:** Periodo de muestreo ajustable (en milisegundos o muestras por segundo / Hz) y capacidad de muestras visibles en vivo.
3. **Escala dual:** Auto-escala dinámica (con algoritmo de 3 guías y margen de resguardo) o Rango Fijo (`minValue` a `maxValue`).
4. **Almacenamiento y Consulta en MicroSD:** Soporte para registro continuo en la tarjeta SD y consulta histórica ("Ver última hora / último turno") mediante diezmado (decimation) en un buffer efímero de ~1 KB de RAM, sin saturar la DRAM ni desgastar la Flash interna.
5. **Anti-parpadeo y trazo:** Soporte de trazo de 1 px o 2 px y redibujado diferencial de alta velocidad.

---

## 2. Requisitos y Especificaciones Técnicas

### 2.1 Modelo de Datos del Campo `TREND` (`.jwhmi` y `jwhmi.schema.json`)

```json
{
  "type": "TREND",
  "id": "trend_temp",
  "name": "Tendencia Temperatura",
  "x": 10,
  "y": 25,
  "w": 300,
  "h": 130,
  "page": 0,
  "layerOrder": 1,
  "variable": "temperaturaHorno",
  "unit": "°C",
  "title": "TEMP",
  "sampleIntervalMs": 1000,
  "visibleSamples": 150,
  "scaleMode": "AUTO",
  "minValue": 0.0,
  "maxValue": 100.0,
  "decimals": 1,
  "lineWidth": 2,
  "showAxes": true,
  "showTimeAxis": true,
  "enableSD": true,
  "sdIntervalMs": 1000,
  "curveColor": "0xFF7A18",
  "axesColor": "0x4208",
  "gridColor": "0x2104",
  "textColor": "0xDCE9F1",
  "backgroundColor": "0x000000"
}
```

### 2.2 Configuración de Muestreo (Tiempo y Frecuencia)
El Inspector ofrecerá selección directa mediante chips táctiles y steppers:
* **Frecuencia / Periodo:**
  * Alta velocidad: `10 Hz` (100 ms), `5 Hz` (200 ms), `2 Hz` (500 ms).
  * Estándar de proceso: `1 Hz` (1000 ms / 1 muestra por segundo).
  * Lento / Ambiental: `2 s`, `5 s`, `10 s`, `30 s`, `60 s`.
* **Ventana visible en pantalla (`visibleSamples`):**
  * Auto-ajustado al ancho útil en píxeles del widget o fijado a un número de puntos (ej. 60, 120, 150, 200, 300 puntos).
  * Consumo en RAM: 1 muestra float por columna visible ($\approx 600\text{ bytes}$ a 1.2 KB por canal en vivo).

### 2.3 Algoritmo de Auto-Escala Dinámica (`calculateGraphScale`)
Replicado del proyecto de referencia:
* Encuentra `dataMin` y `dataMax` dentro de la ventana de muestras visibles.
* Margen de resguardo: aplica un margen superior e inferior (`guideMarginPx = 8..10 px`) para que la curva nunca toque los bordes superior ni inferior.
* Tres líneas guía:
  * Guía Superior (`upperGuideY`): representa `scale.high`.
  * Guía Media (`middleGuideY`): representa `scale.mid = (high + low) / 2`.
  * Guía Inferior (`lowerGuideY`): representa `scale.low`.
* Protección de datos planos: si `|dataMax - dataMin| < 0.0001`, expande un $\pm 5\%$ artificial para evitar divisiones entre cero y líneas colapsadas.
* Etiquetas del eje Y: formateadas según el paso con `0`, `1`, `2` o `3` decimales.

### 2.4 Almacenamiento y Consulta en MicroSD

1. **Escritura en segundo plano (Streaming Append):**
   * El firmware acumula muestras en un buffer interno de 512 bytes (sector SD estándar).
   * Al completarse, realiza una escritura en bloque en la MicroSD utilizando el candado/mutex del bus SPI de `JWPLC_Display` para no interferir con el refresco de la pantalla.
   * Formato: Archivo binario compacto `/trend/<var>.dat` (timestamp `uint32_t` + valor `float32`) o CSV `/trend/<var>.csv`.
2. **Consulta de rangos mayores ("Ver última hora / último turno"):**
   * **Problema:** 1 hora a 1 muestra/s = 3,600 muestras. No caben en los 250 píxeles de ancho del widget ni se deben cargar en DRAM.
   * **Solución (Diezmado / Submuestreo por bloque):**
     * Para un widget de ancho $W$ píxeles (ej. 240 px), se asigna un buffer temporal efímero de **solo 240 floats** ($\approx \mathbf{960\text{ bytes}}$ de RAM).
     * El firmware calcula el paso de salto en el archivo: $\text{paso} = \frac{\text{total muestras}}{W} = \frac{3600}{240} = 15\text{ muestras por píxel}$.
     * Realiza un barrido lineal por sectores, extrayendo una muestra representativa (o par Min/Max) por cada intervalo.
     * Calcula la auto-escala con esos 240 puntos, dibuja la gráfica histórica y libera el buffer de 1 KB.

---

## 3. Arquitectura del Inspector Visual (UI/UX)

Siguiendo el estándar de diseño de `tools/jwplc-hmi-designer`:

### 3.1 Pestaña Diseño (`design:shape`, `design:content`, `design:appearance`, `design:placement`)
* **Forma:**
  * Ancho, alto, grosor de trazo (`1 px` / `2 px`).
  * Toggles visuales: *Escala y Guías Y* (On/Off), *Eje de Tiempo X* (On/Off).
  * Tarjeta de Presets rápidos en 1 clic:
    * `[ Pantalla Completa ]` (ajusta automáticamente a 320×170 px con márgenes de instrumento).
    * `[ Recuadro Compacto ]` (ajusta a 160×90 px para cuadros de mando mixtos).
* **Contenido:**
  * Input de Título (`TEMP`), Unidad (`°C`), Decimales de visualización (`0..3`).
* **Apariencia:**
  * Editor de color completo (`JWPLCColorEditor`): Color de Curva, Color de Guías/Ejes, Color de Fondo y Color de Texto.

### 3.2 Pestaña Datos (`data`)
* **Variable PLC:** Selector de variable vinculada o nombre en sketch.
* **Escala:** Chips visuales `[ Auto-escala Dinámica ● ]` o `[ Rango Fijo ]`. Si es fijo, muestra inputs `Mínimo` y `Máximo`.
* **Muestreo:**
  * Chips de Frecuencia: `[ 10 Hz ] [ 5 Hz ] [ 2 Hz ] [ 1 Hz ● ] [ 2 s ] [ 5 s ] [ 10 s ] [ 60 s ]`.
  * Input de Muestras en Vivo (ancho de ventana, por defecto igual al ancho útil de la gráfica).
* **Almacenamiento MicroSD:**
  * Toggle `[ Habilitar Registro en MicroSD ]`.
  * Intervalo de registro (chips o stepper).

---

## 4. Generación de Código C++ (`JWPLC_HMI_Generated.h`)

El generador emitirá la configuración estructurada del widget contra `JWPLC_UI_Widgets`:

```cpp
// Configuración de la gráfica de tendencia
JWPLC_UITrendConfig cfg_trend_temp = {
    /* x */ 10, /* y */ 25, /* w */ 300, /* h */ 130,
    /* sampleIntervalMs */ 1000,
    /* visibleSamples */ 150,
    /* scaleMode */ JWPLC_TREND_SCALE_AUTO,
    /* minVal */ 0.0f, /* maxVal */ 100.0f,
    /* decimals */ 1,
    /* lineWidth */ 2,
    /* showAxes */ true,
    /* showTime */ true,
    /* curveColor */ 0xFD20, // Naranja
    /* axesColor */ 0x4208,  // Gris tenue
    /* enableSD */ true,
    /* sdIntervalMs */ 1000
};
```

En `jwplcUIUpdate()`:
```cpp
jwplcTrendPush(trend_temp, temperaturaHorno);
```

---

## 5. Criterios de Aceptación y Verificación

1. **Especificación y Esquema:** `spec.cjs` y `jwhmi.schema.json` validan campos de tipo `TREND` sin errores.
2. **Lienzo del Designer:** Simula el avance continuo de la curva con datos ficticios o estáticos sin desbordar el contenedor.
3. **Inspector sin scroll:** Pestañas Diseño, Datos y Apariencia organizadas con tokens de diseño semánticos (`var(--tab-bg)`, `var(--orange)`, etc.) compatibles con modo claro y oscuro.
4. **Generación C++:** Emisión limpia en el header C++ con paridad byte a byte en pruebas golden.
5. **Pruebas unitarias:** Nueva suite `tests/trend-smoke.cjs` pasando al 100%.
