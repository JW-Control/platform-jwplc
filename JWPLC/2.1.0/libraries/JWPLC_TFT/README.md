# JWPLC_TFT

API de dibujo directo para la TFT integrada del **JWPLC Basic**.

`JWPLC_TFT` permite dibujar texto, líneas, rectángulos, círculos y píxeles sin
depender públicamente de TFT_eSPI ni de Adafruit.

Para interfaces HMI normales se recomienda usar **JWPLC_Display**. Use
`JWPLC_TFT` cuando necesite dibujo directo o una pantalla personalizada.

---

## ¿JWPLC_Display o JWPLC_TFT?

### Use JWPLC_Display cuando quiera

- páginas HMI;
- campos TEXT / VALUE / BOOL / BAR;
- HMI Designer;
- modo IDLE;
- indicadores RUN / ERR / BUS / ETH;
- refresco y dirty-cache gestionados por el package.

### Use JWPLC_TFT cuando quiera

- dibujar primitivas manualmente;
- posicionar texto libre;
- crear una interfaz gráfica propia;
- acceder al renderer desde `JWPLC_Display.tft()`.

---

## Inicio rápido

```cpp
#include <JWPLC_TFT.h>

void setup()
{
    JWPLC_TFT.begin();

    JWPLC_TFT.fillScreen(JWPLC_TFT_BLACK);

    JWPLC_TFT.setCursor(10, 10);
    JWPLC_TFT.setTextColor(JWPLC_TFT_WHITE);
    JWPLC_TFT.setTextSize(2);

    JWPLC_TFT.println("Hola JWPLC");
}

void loop()
{
}
```

`begin()` es idempotente: puede llamarse aunque el runtime ya haya
inicializado la pantalla.

---

## Estado y dimensiones

```cpp
JWPLC_TFT.begin();
JWPLC_TFT.isReady();

JWPLC_TFT.width();
JWPLC_TFT.height();
JWPLC_TFT.rotation();
```

En JWPLC Basic v2 la geometría lógica normal es:

```text
320 x 170
```

También están disponibles:

```cpp
JWPLC_TFT.panel();
JWPLC_TFT.panelInfo();
```

para consultar información del perfil activo.

---

## Colores básicos

Constantes RGB565 incluidas:

```cpp
JWPLC_TFT_BLACK
JWPLC_TFT_WHITE
JWPLC_TFT_RED
JWPLC_TFT_GREEN
JWPLC_TFT_BLUE
JWPLC_TFT_YELLOW
JWPLC_TFT_CYAN
JWPLC_TFT_MAGENTA
```

Ejemplo:

```cpp
JWPLC_TFT.fillScreen(JWPLC_TFT_BLACK);

JWPLC_TFT.fillRect(
    10, 10,
    80, 40,
    JWPLC_TFT_BLUE);
```

También puede usarse cualquier valor RGB565 propio:

```cpp
uint16_t myColor = 0x7BEF;

JWPLC_TFT.fillScreen(myColor);
```

---

## Limpiar o rellenar la pantalla

```cpp
JWPLC_TFT.fillScreen(color);
```

Ejemplo:

```cpp
JWPLC_TFT.fillScreen(JWPLC_TFT_BLACK);
```

---

# Rectángulos

## Rectángulo relleno

```cpp
JWPLC_TFT.fillRect(
    x,
    y,
    width,
    height,
    color);
```

Ejemplo:

```cpp
JWPLC_TFT.fillRect(
    20, 30,
    100, 50,
    JWPLC_TFT_GREEN);
```

## Contorno de rectángulo

```cpp
JWPLC_TFT.drawRect(
    x,
    y,
    width,
    height,
    color);
```

## Rectángulos redondeados

```cpp
JWPLC_TFT.fillRoundRect(
    x, y,
    width, height,
    radius,
    color);

JWPLC_TFT.drawRoundRect(
    x, y,
    width, height,
    radius,
    color);
```

---

# Círculos

## Círculo relleno

```cpp
JWPLC_TFT.fillCircle(
    x,
    y,
    radius,
    color);
```

## Contorno

```cpp
JWPLC_TFT.drawCircle(
    x,
    y,
    radius,
    color);
```

Ejemplo:

```cpp
JWPLC_TFT.fillCircle(
    160,
    85,
    20,
    JWPLC_TFT_RED);
```

---

# Líneas y píxeles

Línea libre:

```cpp
JWPLC_TFT.drawLine(
    x0, y0,
    x1, y1,
    color);
```

Horizontal:

```cpp
JWPLC_TFT.drawFastHLine(
    x,
    y,
    width,
    color);
```

Vertical:

```cpp
JWPLC_TFT.drawFastVLine(
    x,
    y,
    height,
    color);
```

Píxel:

```cpp
JWPLC_TFT.drawPixel(
    x,
    y,
    color);
```

---

# Texto

## Posición

```cpp
JWPLC_TFT.setCursor(20, 20);
```

Consulta:

```cpp
JWPLC_TFT.cursorX();
JWPLC_TFT.cursorY();
```

## Tamaño

```cpp
JWPLC_TFT.setTextSize(2);
```

Consulta:

```cpp
JWPLC_TFT.textSize();
```

## Color

Sólo foreground:

```cpp
JWPLC_TFT.setTextColor(
    JWPLC_TFT_WHITE);
```

Foreground + background:

```cpp
JWPLC_TFT.setTextColor(
    JWPLC_TFT_WHITE,
    JWPLC_TFT_BLUE);
```

## Imprimir texto

`JWPLC_TFT` hereda de `Print`, por lo que puede usarse:

```cpp
JWPLC_TFT.print("Temperatura: ");
JWPLC_TFT.println(25.4);
```

Ejemplo completo:

```cpp
JWPLC_TFT.setCursor(20, 20);
JWPLC_TFT.setTextSize(2);
JWPLC_TFT.setTextColor(JWPLC_TFT_YELLOW);

JWPLC_TFT.print("T = ");
JWPLC_TFT.print(24.8);
JWPLC_TFT.println(" C");
```

---

## Ajuste de línea

```cpp
JWPLC_TFT.setTextWrap(
    true,
    false);
```

Parámetros:

```text
wrapX
wrapY
```

---

## Medir texto

Ancho:

```cpp
int16_t w =
    JWPLC_TFT.textWidth("RUN");
```

Altura de fuente:

```cpp
int16_t h =
    JWPLC_TFT.fontHeight();
```

Bounds completos:

```cpp
int16_t x1;
int16_t y1;
uint16_t w;
uint16_t h;

JWPLC_TFT.getTextBounds(
    "JWPLC",
    10,
    10,
    &x1,
    &y1,
    &w,
    &h);
```

Esto es útil para centrar texto o construir layouts propios.

---

# Batching

Para varias operaciones consecutivas puede agruparse el acceso a la pantalla:

```cpp
if (JWPLC_TFT.beginBatch())
{
    JWPLC_TFT.fillRect(
        0, 0,
        100, 50,
        JWPLC_TFT_BLUE);

    JWPLC_TFT.drawCircle(
        50, 25,
        15,
        JWPLC_TFT_WHITE);

    JWPLC_TFT.endBatch();
}
```

Consulta:

```cpp
JWPLC_TFT.batchActive();
```

El batching es opcional. Para operaciones aisladas no es necesario usarlo.

---

# Acceder a la TFT desde JWPLC_Display

Cuando ya se está usando `JWPLC_Display`, puede obtenerse el renderer:

```cpp
JWPLC_TFTClass &tft =
    JWPLC_Display.tft();

tft.fillCircle(
    160,
    85,
    10,
    JWPLC_TFT_GREEN);
```

Alias equivalente:

```cpp
JWPLC_TFTClass &tft =
    JWPLC_Display.display();
```

Para código que quiera evitar depender del tipo explícito:

```cpp
auto &tft = JWPLC_Display.tft();
```

Este último patrón es especialmente útil para mantener el código desacoplado
del backend gráfico.

---

## Compatibilidad con código anterior

El backend actual ya no expone `Adafruit_ST7789&` como tipo público.

Código anterior como:

```cpp
Adafruit_ST7789 &tft =
    JWPLC_Display.raw();
```

debe migrarse.

Patrón recomendado:

```cpp
auto &tft =
    JWPLC_Display.tft();
```

o usar directamente `JWPLC_TFT`.

---

## Qué no necesita configurar el usuario

El package gestiona internamente:

- controlador ST7789;
- orientación y offsets del panel;
- frecuencia SPI;
- arbitraje del bus compartido;
- backend TFT_eSPI incorporado al archive.

No es necesario instalar ni configurar TFT_eSPI.

---

## Relación con el SPI compartido

La TFT comparte bus con otros periféricos del JWPLC Basic. La librería utiliza
el arbitraje del package para convivir con Ethernet, microSD y otros dispositivos
del mismo bus.

El sketch no debe manipular manualmente el CS o el SPI de la pantalla para
usar la API normal.

---

## Estado Alpha12

```text
JWPLC_TFT 0.1.0
JWPLC_DISPLAY_BACKEND_MIGRATION=COMPLETE
TFT_ESPI_USER_DEPENDENCY=NO
PRECOMPILED_RELEASE_LIKE=ACTIVE
FULL_RUNTIME_TFT=VALIDATED
```

Archive cualificado:

```text
Bytes  : 1091098
SHA256 : 5d860a131811dd9a7eb6fa55f5674b1d78b0de7dfaf8748ce18a60ceed2d3738
```

Alpha13 continuará la evolución funcional de TFT/Display sobre esta API sin
volver a exponer TFT_eSPI como dependencia pública.
