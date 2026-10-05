# JWPLC_TFT

`JWPLC_TFT` es la API de dibujo directo de la pantalla integrada del
**JWPLC Basic**.

Úsala cuando necesitas colocar texto, líneas, rectángulos, círculos o píxeles
manualmente. Para una HMI industrial normal, páginas y Fields, normalmente es
más sencillo usar `JWPLC_Display`.

## ¿Para qué sirve?

Con `JWPLC_TFT` puedes:

- limpiar la pantalla;
- dibujar figuras geométricas;
- mostrar texto en posiciones libres;
- medir texto para centrarlo;
- crear indicadores gráficos personalizados;
- construir una interfaz que no encaja en los Fields de `JWPLC_Display`.

Es una API de usuario **avanzada**, pero no requiere conocer el controlador
físico de la TFT.

## Qué hace automáticamente el JWPLC

No necesitas:

- crear un objeto ST7789;
- configurar pines;
- elegir frecuencia del bus;
- controlar el chip select;
- instalar o configurar otro backend gráfico.

La pantalla ya forma parte del JWPLC.

Si usas `JWPLC_Display`, puedes obtener el mismo objeto gráfico con:

```cpp
auto &tft =
    JWPLC_Display.tft();
```

## Inicio rápido

Este ejemplo muestra un título y un círculo.

```cpp
#include <JWPLC_TFT.h>

void setup()
{
    JWPLC_TFT.begin();

    JWPLC_TFT.fillScreen(
        JWPLC_TFT_BLACK);

    JWPLC_TFT.setCursor(
        20, 20);

    JWPLC_TFT.setTextColor(
        JWPLC_TFT_WHITE);

    JWPLC_TFT.setTextSize(2);

    JWPLC_TFT.println(
        "Hola JWPLC");

    JWPLC_TFT.fillCircle(
        160,
        100,
        20,
        JWPLC_TFT_GREEN);
}

void loop()
{
}
```

## Conceptos básicos

### Coordenadas

La esquina superior izquierda es:

```text
x = 0
y = 0
```

- `x` aumenta hacia la derecha;
- `y` aumenta hacia abajo.

Puedes consultar el tamaño lógico actual con:

```cpp
JWPLC_TFT.width();
JWPLC_TFT.height();
```

### Colores

La librería incluye colores RGB565 listos para usar:

```text
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
JWPLC_TFT.fillScreen(
    JWPLC_TFT_BLUE);
```

### Relleno y contorno

Muchas figuras tienen dos variantes:

```text
fill...  -> figura rellena
draw...  -> sólo contorno
```

Por ejemplo:

```cpp
JWPLC_TFT.fillCircle(...);
JWPLC_TFT.drawCircle(...);
```

### Texto

Antes de imprimir puedes elegir:

- posición;
- tamaño;
- color.

```cpp
JWPLC_TFT.setCursor(20, 20);
JWPLC_TFT.setTextSize(2);
JWPLC_TFT.setTextColor(JWPLC_TFT_WHITE);

JWPLC_TFT.println("RUN");
```

## Ejemplo 1 — Básico: texto y estado

Este ejemplo muestra `RUN` o `STOP` según `I0_0`.

```cpp
#include <JWPLC_TFT.h>

bool ultimoEstado = false;
bool primeraVez = true;

void setup()
{
    pinMode(I0_0, INPUT);

    JWPLC_TFT.begin();

    JWPLC_TFT.fillScreen(
        JWPLC_TFT_BLACK);
}

void loop()
{
    const bool estado =
        digitalRead(I0_0);

    if (primeraVez ||
        estado != ultimoEstado)
    {
        primeraVez = false;
        ultimoEstado = estado;

        JWPLC_TFT.fillRect(
            20, 40,
            180, 50,
            JWPLC_TFT_BLACK);

        JWPLC_TFT.setCursor(
            20, 50);

        JWPLC_TFT.setTextSize(3);

        JWPLC_TFT.setTextColor(
            estado
                ? JWPLC_TFT_GREEN
                : JWPLC_TFT_RED);

        JWPLC_TFT.println(
            estado ? "RUN" : "STOP");
    }
}
```

Sólo redibujamos cuando cambia el estado.

## Ejemplo 2 — Intermedio: figuras e indicador de nivel

```cpp
#include <JWPLC_TFT.h>

uint8_t nivel = 0;
uint32_t ultimaActualizacion = 0;

void dibujarNivel(uint8_t valor)
{
    JWPLC_TFT.drawRect(
        20, 80,
        200, 30,
        JWPLC_TFT_WHITE);

    JWPLC_TFT.fillRect(
        22, 82,
        196, 26,
        JWPLC_TFT_BLACK);

    const int16_t ancho =
        (196 * valor) / 100;

    JWPLC_TFT.fillRect(
        22, 82,
        ancho, 26,
        JWPLC_TFT_GREEN);
}

void setup()
{
    JWPLC_TFT.begin();

    JWPLC_TFT.fillScreen(
        JWPLC_TFT_BLACK);

    JWPLC_TFT.setCursor(
        20, 30);

    JWPLC_TFT.setTextColor(
        JWPLC_TFT_WHITE);

    JWPLC_TFT.setTextSize(2);

    JWPLC_TFT.println("Nivel");

    dibujarNivel(nivel);
}

void loop()
{
    if (millis() - ultimaActualizacion >= 500)
    {
        ultimaActualizacion = millis();

        nivel += 10;

        if (nivel > 100)
        {
            nivel = 0;
        }

        dibujarNivel(nivel);
    }
}
```

## Ejemplo 3 — Aplicación real: pequeño panel de proceso

El ejemplo muestra:

- entrada I0_0;
- salida Q0_0;
- un indicador circular;
- un contador de segundos.

```cpp
#include <JWPLC_TFT.h>

bool ultimoInput = false;
bool primeraVez = true;
uint32_t ultimoSegundo = 0;

void dibujarEstado(bool estado)
{
    JWPLC_TFT.fillCircle(
        40, 90,
        15,
        estado
            ? JWPLC_TFT_GREEN
            : JWPLC_TFT_RED);

    JWPLC_TFT.fillRect(
        70, 75,
        120, 30,
        JWPLC_TFT_BLACK);

    JWPLC_TFT.setCursor(
        70, 82);

    JWPLC_TFT.setTextColor(
        JWPLC_TFT_WHITE);

    JWPLC_TFT.setTextSize(2);

    JWPLC_TFT.print(
        estado ? "ACTIVO" : "PARADO");
}

void setup()
{
    pinMode(I0_0, INPUT);
    pinMode(Q0_0, OUTPUT);

    JWPLC_TFT.begin();

    JWPLC_TFT.fillScreen(
        JWPLC_TFT_BLACK);

    JWPLC_TFT.setCursor(
        20, 20);

    JWPLC_TFT.setTextColor(
        JWPLC_TFT_CYAN);

    JWPLC_TFT.setTextSize(2);

    JWPLC_TFT.println(
        "Panel de proceso");
}

void loop()
{
    const bool entrada =
        digitalRead(I0_0);

    digitalWrite(
        Q0_0,
        entrada ? HIGH : LOW);

    if (primeraVez ||
        entrada != ultimoInput)
    {
        primeraVez = false;
        ultimoInput = entrada;

        dibujarEstado(entrada);
    }

    const uint32_t segundos =
        millis() / 1000;

    if (segundos != ultimoSegundo)
    {
        ultimoSegundo = segundos;

        JWPLC_TFT.fillRect(
            20, 125,
            200, 25,
            JWPLC_TFT_BLACK);

        JWPLC_TFT.setCursor(
            20, 130);

        JWPLC_TFT.setTextSize(1);
        JWPLC_TFT.setTextColor(
            JWPLC_TFT_YELLOW);

        JWPLC_TFT.print("Tiempo: ");
        JWPLC_TFT.print(segundos);
        JWPLC_TFT.print(" s");
    }
}
```

## Ejemplo 4 — Avanzado de usuario: batching y texto centrado

Un **batch** agrupa varias operaciones gráficas.

Es útil cuando quieres dibujar una vista completa con varias primitivas
consecutivas.

```cpp
#include <JWPLC_TFT.h>

void setup()
{
    JWPLC_TFT.begin();

    const char *titulo =
        "JWPLC";

    const int16_t anchoTexto =
        JWPLC_TFT.textWidth(titulo);

    const int16_t x =
        (JWPLC_TFT.width() -
         anchoTexto) / 2;

    if (JWPLC_TFT.beginBatch())
    {
        JWPLC_TFT.fillScreen(
            JWPLC_TFT_BLACK);

        JWPLC_TFT.drawRoundRect(
            20, 20,
            280, 120,
            10,
            JWPLC_TFT_BLUE);

        JWPLC_TFT.setCursor(
            x, 65);

        JWPLC_TFT.setTextColor(
            JWPLC_TFT_WHITE);

        JWPLC_TFT.print(titulo);

        JWPLC_TFT.endBatch();
    }
}

void loop()
{
}
```

No necesitas batching para dibujos aislados.

## API de usuario

### Inicialización y estado — Básico

| Función | Qué hace | Nivel |
|---|---|---|
| `begin(timeoutMs)` | Prepara la API gráfica | Básico |
| `isReady()` | Indica si la TFT está lista | Básico |
| `width()` | Ancho lógico | Básico |
| `height()` | Alto lógico | Básico |
| `rotation()` | Rotación actual | Intermedio |

`begin()` puede llamarse aunque la plataforma ya haya inicializado la
pantalla.

### Pantalla y rectángulos — Básico

```cpp
JWPLC_TFT.fillScreen(color);

JWPLC_TFT.fillRect(
    x, y,
    width, height,
    color);

JWPLC_TFT.drawRect(
    x, y,
    width, height,
    color);
```

Rectángulos redondeados:

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

### Círculos — Básico

```cpp
JWPLC_TFT.fillCircle(
    x, y,
    radius,
    color);

JWPLC_TFT.drawCircle(
    x, y,
    radius,
    color);
```

### Líneas y píxeles — Básico / Intermedio

```cpp
JWPLC_TFT.drawLine(
    x0, y0,
    x1, y1,
    color);

JWPLC_TFT.drawFastHLine(
    x, y,
    width,
    color);

JWPLC_TFT.drawFastVLine(
    x, y,
    height,
    color);

JWPLC_TFT.drawPixel(
    x, y,
    color);
```

### Cursor y texto — Básico

```cpp
JWPLC_TFT.setCursor(x, y);
JWPLC_TFT.cursorX();
JWPLC_TFT.cursorY();

JWPLC_TFT.setTextSize(2);
JWPLC_TFT.textSize();

JWPLC_TFT.setTextColor(
    JWPLC_TFT_WHITE);
```

Color de texto con fondo:

```cpp
JWPLC_TFT.setTextColor(
    JWPLC_TFT_WHITE,
    JWPLC_TFT_BLUE);
```

Consultar:

```cpp
JWPLC_TFT.textColor();
JWPLC_TFT.textBackground();
```

Ajuste de línea:

```cpp
JWPLC_TFT.setTextWrap(
    true,
    false);
```

### Imprimir — Básico

Como `JWPLC_TFT` hereda de `Print`:

```cpp
JWPLC_TFT.print("Temperatura: ");
JWPLC_TFT.println(25.4);
```

También están disponibles los overloads de `write()`:

```cpp
JWPLC_TFT.write(
    (uint8_t)'A');

JWPLC_TFT.write(
    buffer,
    length);
```

### Medir texto — Intermedio

```cpp
int16_t ancho =
    JWPLC_TFT.textWidth("RUN");

int16_t alto =
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

### Batching — Avanzado de usuario

```cpp
if (JWPLC_TFT.beginBatch())
{
    // varias operaciones
    JWPLC_TFT.endBatch();
}
```

Consultar:

```cpp
JWPLC_TFT.batchActive();
```

`beginBatch(timeoutMs)` devuelve `false` si no puede comenzar dentro del
timeout indicado.

### Información del panel — Avanzado de usuario

```cpp
JWPLC_TFTPanel panel =
    JWPLC_TFT.panel();

JWPLC_TFTPanelInfo info =
    JWPLC_TFT.panelInfo();
```

`panelInfo()` permite consultar datos como dimensiones lógicas del panel.

No uses esa información para reconfigurar físicamente la pantalla.

## Errores comunes

### Redibujar toda la pantalla en cada `loop()`

Evita:

```cpp
void loop()
{
    JWPLC_TFT.fillScreen(
        JWPLC_TFT_BLACK);

    // volver a dibujar todo siempre
}
```

Actualiza sólo cuando cambia el dato o sólo la región necesaria.

### Usar `delay()` para animaciones o indicadores

Prefiere `millis()` para programar actualizaciones periódicas.

### Mezclar `JWPLC_Display` y dibujo directo sin una estrategia

Si tu proyecto usa una HMI con Fields, deja a `JWPLC_Display` gestionar la
interfaz principal. Usa dibujo directo sólo donde realmente lo necesites.

### Crear otro driver de pantalla

No crees otra instancia para el mismo panel integrado.

### Dibujar fuera de las dimensiones

Consulta:

```cpp
JWPLC_TFT.width();
JWPLC_TFT.height();
```

antes de calcular layouts dinámicos.

## API avanzada

### Acceso desde JWPLC_Display

```cpp
auto &tft =
    JWPLC_Display.tft();
```

Es el camino recomendado cuando tu aplicación ya usa `JWPLC_Display` y sólo
necesita algunas primitivas gráficas adicionales.

### Colores personalizados

Las funciones reciben un `uint16_t` RGB565.

Puedes usar un valor propio:

```cpp
uint16_t miColor = 0x7BEF;

JWPLC_TFT.fillCircle(
    100, 80,
    15,
    miColor);
```

## Compatibilidad

`JWPLC_Display.display()` sigue devolviendo la misma API gráfica como alias
histórico:

```cpp
auto &tft =
    JWPLC_Display.display();
```

Para código nuevo se recomienda:

```cpp
JWPLC_Display.tft();
```

Código antiguo que esperaba explícitamente otro tipo gráfico debe migrar a
`JWPLC_TFTClass&` o, preferiblemente:

```cpp
auto &tft =
    JWPLC_Display.tft();
```

## Versión

Documentado para:

```text
JWPLC ESP32 v2.1.0-alpha.12
JWPLC_TFT 0.1.0
```

Esta guía documenta la API gráfica de usuario. Los detalles del controlador
físico y de la arquitectura interna pertenecen a la documentación del package.
