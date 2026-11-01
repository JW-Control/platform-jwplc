#ifndef JWPLC_TFT_H
#define JWPLC_TFT_H

#include <Arduino.h>
#include <stdint.h>
#include <atomic>

#define JWPLC_TFT_VERSION_MAJOR 0
#define JWPLC_TFT_VERSION_MINOR 4
#define JWPLC_TFT_VERSION_PATCH 0

// 0.4.0: recorte, líneas gruesas, polilíneas y sprites (Runtime alpha.16).
#define JWPLC_TFT_HAS_CLIP 1
#define JWPLC_TFT_HAS_WIDE_LINE 1
#define JWPLC_TFT_HAS_SPRITE 1
#define JWPLC_TFT_HAS_FONT16 1

enum class JWPLC_TFTPanel : uint8_t
{
    BASIC_V2_ST7789_170X320 = 0,

    // Target planificado para Basic v3. No se activa en 2.1.x hasta fijar
    // board target, pinout y calificacion fisica de la pantalla 240x320.
    BASIC_V3_ST7789_240X320_RESERVED = 1
};

struct JWPLC_TFTPanelInfo
{
    const char *name;
    uint16_t nativeWidth;
    uint16_t nativeHeight;
    uint16_t logicalWidth;
    uint16_t logicalHeight;
    uint8_t rotation;
    bool bgr;
    bool inverted;
    uint32_t spiHz;
};

// RGB565 publico e independiente del backend.
static constexpr uint16_t JWPLC_TFT_BLACK = 0x0000;
static constexpr uint16_t JWPLC_TFT_WHITE = 0xFFFF;
static constexpr uint16_t JWPLC_TFT_RED = 0xF800;
static constexpr uint16_t JWPLC_TFT_GREEN = 0x07E0;
static constexpr uint16_t JWPLC_TFT_BLUE = 0x001F;
static constexpr uint16_t JWPLC_TFT_YELLOW = 0xFFE0;
static constexpr uint16_t JWPLC_TFT_CYAN = 0x07FF;
static constexpr uint16_t JWPLC_TFT_MAGENTA = 0xF81F;

// Fuentes (0.3.0). Cada fuente es un objeto constante: el enlazador sólo
// incluye en el firmware las fuentes que el sketch nombra.
struct JWPLC_TFTFont
{
    uint8_t kind;      // 0 = GLCD 5x7 clásica, 1 = FreeFont (Adafruit GFX)
    const void *data;  // privado del backend
};

extern const JWPLC_TFTFont JWPLC_FONT_GLCD;
extern const JWPLC_TFTFont JWPLC_FONT_SANS_9;
extern const JWPLC_TFTFont JWPLC_FONT_SANS_BOLD_9;
extern const JWPLC_TFTFont JWPLC_FONT_SANS_12;
extern const JWPLC_TFTFont JWPLC_FONT_SANS_BOLD_12;
extern const JWPLC_TFTFont JWPLC_FONT_SANS_18;
extern const JWPLC_TFTFont JWPLC_FONT_SANS_BOLD_18;
extern const JWPLC_TFTFont JWPLC_FONT_SANS_24;
extern const JWPLC_TFTFont JWPLC_FONT_SANS_BOLD_24;
extern const JWPLC_TFTFont JWPLC_FONT_SERIF_9;
extern const JWPLC_TFTFont JWPLC_FONT_SERIF_BOLD_9;
extern const JWPLC_TFTFont JWPLC_FONT_SERIF_12;
extern const JWPLC_TFTFont JWPLC_FONT_SERIF_BOLD_12;
extern const JWPLC_TFTFont JWPLC_FONT_SERIF_18;
extern const JWPLC_TFTFont JWPLC_FONT_SERIF_BOLD_18;
extern const JWPLC_TFTFont JWPLC_FONT_SERIF_24;
extern const JWPLC_TFTFont JWPLC_FONT_SERIF_BOLD_24;
extern const JWPLC_TFTFont JWPLC_FONT_MONO_9;
extern const JWPLC_TFTFont JWPLC_FONT_MONO_BOLD_9;
extern const JWPLC_TFTFont JWPLC_FONT_MONO_12;
extern const JWPLC_TFTFont JWPLC_FONT_MONO_BOLD_12;
extern const JWPLC_TFTFont JWPLC_FONT_MONO_18;
extern const JWPLC_TFTFont JWPLC_FONT_MONO_BOLD_18;
extern const JWPLC_TFTFont JWPLC_FONT_MONO_24;
extern const JWPLC_TFTFont JWPLC_FONT_MONO_BOLD_24;

// Fuente de 16 px con trazo de 1 px (Font 2 de TFT_eSPI): para listas y textos
// junto a títulos en GLCD x2. Solo se enlaza si el sketch la usa.
extern const JWPLC_TFTFont JWPLC_FONT_16;

// Punto de anclaje de drawString(). Valores idénticos a TFT_eSPI.
enum class JWPLC_TFTDatum : uint8_t
{
    TOP_LEFT = 0,
    TOP_CENTER = 1,
    TOP_RIGHT = 2,
    MIDDLE_LEFT = 3,
    MIDDLE_CENTER = 4,
    MIDDLE_RIGHT = 5,
    BOTTOM_LEFT = 6,
    BOTTOM_CENTER = 7,
    BOTTOM_RIGHT = 8,
    BASELINE_LEFT = 9,
    BASELINE_CENTER = 10,
    BASELINE_RIGHT = 11
};

// Las mismas posiciones como constantes globales, para escribir en el sketch
// tft.setTextDatum(DATUM_MIDDLE_LEFT) sin «::» (regla del propietario, 01/11).
static constexpr JWPLC_TFTDatum DATUM_TOP_LEFT = JWPLC_TFTDatum::TOP_LEFT;
static constexpr JWPLC_TFTDatum DATUM_TOP_CENTER = JWPLC_TFTDatum::TOP_CENTER;
static constexpr JWPLC_TFTDatum DATUM_TOP_RIGHT = JWPLC_TFTDatum::TOP_RIGHT;
static constexpr JWPLC_TFTDatum DATUM_MIDDLE_LEFT = JWPLC_TFTDatum::MIDDLE_LEFT;
static constexpr JWPLC_TFTDatum DATUM_MIDDLE_CENTER = JWPLC_TFTDatum::MIDDLE_CENTER;
static constexpr JWPLC_TFTDatum DATUM_MIDDLE_RIGHT = JWPLC_TFTDatum::MIDDLE_RIGHT;
static constexpr JWPLC_TFTDatum DATUM_BOTTOM_LEFT = JWPLC_TFTDatum::BOTTOM_LEFT;
static constexpr JWPLC_TFTDatum DATUM_BOTTOM_CENTER = JWPLC_TFTDatum::BOTTOM_CENTER;
static constexpr JWPLC_TFTDatum DATUM_BOTTOM_RIGHT = JWPLC_TFTDatum::BOTTOM_RIGHT;
static constexpr JWPLC_TFTDatum DATUM_BASELINE_LEFT = JWPLC_TFTDatum::BASELINE_LEFT;
static constexpr JWPLC_TFTDatum DATUM_BASELINE_CENTER = JWPLC_TFTDatum::BASELINE_CENTER;
static constexpr JWPLC_TFTDatum DATUM_BASELINE_RIGHT = JWPLC_TFTDatum::BASELINE_RIGHT;

class JWPLC_TFTSprite;

class JWPLC_TFTClass : public Print
{
public:
    JWPLC_TFTClass();

    // Inicializa el ST7789 usando el perfil fisico del board JWPLC actual.
    // Es idempotente.
    bool begin(uint32_t timeoutMs = 100);
    bool isReady() const;

    JWPLC_TFTPanel panel() const;
    JWPLC_TFTPanelInfo panelInfo() const;

    int16_t width() const;
    int16_t height() const;
    uint8_t rotation() const;

    // Agrupa varias primitivas bajo un unico lock/transaccion SPI.
    // JWPLC_Display usara esta ruta para los dirty passes.
    bool beginBatch(uint32_t timeoutMs = 50);
    void endBatch();
    bool batchActive() const;

    bool fillScreen(uint16_t color, uint32_t timeoutMs = 50);
    bool fillRect(
        int16_t x,
        int16_t y,
        int16_t w,
        int16_t h,
        uint16_t color,
        uint32_t timeoutMs = 50);
    bool drawRect(
        int16_t x,
        int16_t y,
        int16_t w,
        int16_t h,
        uint16_t color,
        uint32_t timeoutMs = 50);
    bool fillRoundRect(
        int16_t x,
        int16_t y,
        int16_t w,
        int16_t h,
        int16_t radius,
        uint16_t color,
        uint32_t timeoutMs = 50);
    bool drawRoundRect(
        int16_t x,
        int16_t y,
        int16_t w,
        int16_t h,
        int16_t radius,
        uint16_t color,
        uint32_t timeoutMs = 50);
    bool fillCircle(
        int16_t x,
        int16_t y,
        int16_t radius,
        uint16_t color,
        uint32_t timeoutMs = 50);
    bool drawCircle(
        int16_t x,
        int16_t y,
        int16_t radius,
        uint16_t color,
        uint32_t timeoutMs = 50);
    // Elipse centrada en (x, y) con semiejes rx/ry. Alpha15: usada por el
    // codegen del HMI Designer para figuras estáticas.
    bool fillEllipse(
        int16_t x,
        int16_t y,
        int16_t rx,
        int16_t ry,
        uint16_t color,
        uint32_t timeoutMs = 50);
    bool drawEllipse(
        int16_t x,
        int16_t y,
        int16_t rx,
        int16_t ry,
        uint16_t color,
        uint32_t timeoutMs = 50);
    bool fillTriangle(
        int16_t x0,
        int16_t y0,
        int16_t x1,
        int16_t y1,
        int16_t x2,
        int16_t y2,
        uint16_t color,
        uint32_t timeoutMs = 50);
    bool drawTriangle(
        int16_t x0,
        int16_t y0,
        int16_t x1,
        int16_t y1,
        int16_t x2,
        int16_t y2,
        uint16_t color,
        uint32_t timeoutMs = 50);
    bool drawFastHLine(
        int16_t x,
        int16_t y,
        int16_t w,
        uint16_t color,
        uint32_t timeoutMs = 50);
    bool drawFastVLine(
        int16_t x,
        int16_t y,
        int16_t h,
        uint16_t color,
        uint32_t timeoutMs = 50);
    bool drawLine(
        int16_t x0,
        int16_t y0,
        int16_t x1,
        int16_t y1,
        uint16_t color,
        uint32_t timeoutMs = 50);
    bool drawPixel(
        int16_t x,
        int16_t y,
        uint16_t color,
        uint32_t timeoutMs = 50);

    // Envía un bloque w x h de píxeles RGB565 (valores nativos, como los de
    // drawPixel) en una sola ventana SPI. Es la forma rápida de pintar una
    // imagen o un framebuffer parcial: una transferencia en vez de un
    // drawPixel por píxel.
    bool pushImage(
        int16_t x,
        int16_t y,
        int16_t w,
        int16_t h,
        const uint16_t *pixels,
        uint32_t timeoutMs = 50);

    // --- 0.4.0: recorte -------------------------------------------------
    // Limita el dibujo al rectángulo dado (coordenadas de pantalla). Sólo se
    // acepta dentro de beginBatch() de la misma tarea, y endBatch() lo quita
    // siempre: así JWPLC_Display u otra tarea nunca heredan un recorte ajeno.
    bool setClipRect(int16_t x, int16_t y, int16_t w, int16_t h);
    void resetClip();
    bool clipActive() const;

    // --- 0.4.0: líneas gruesas -----------------------------------------
    // Línea con grosor y bordes suavizados. `background` es el color sobre
    // el que se suaviza (la TFT no se lee de vuelta).
    bool drawWideLine(
        int16_t x0,
        int16_t y0,
        int16_t x1,
        int16_t y1,
        uint8_t width,
        uint16_t color,
        uint16_t background = JWPLC_TFT_BLACK,
        uint32_t timeoutMs = 50);

    // Polilínea de `points` vértices en `xy` = {x0, y0, x1, y1, ...}. Con
    // width <= 1 usa líneas finas; si no, drawWideLine. Un solo lock SPI.
    bool drawPolyline(
        const int16_t *xy,
        size_t points,
        uint8_t width,
        uint16_t color,
        uint16_t background = JWPLC_TFT_BLACK,
        uint32_t timeoutMs = 50);

    void setCursor(int16_t x, int16_t y);
    int16_t cursorX() const;
    int16_t cursorY() const;

    void setTextSize(uint8_t size);
    uint8_t textSize() const;

    void setTextColor(uint16_t foreground);
    void setTextColor(uint16_t foreground, uint16_t background);
    uint16_t textColor() const;
    uint16_t textBackground() const;

    void setTextWrap(bool wrapX, bool wrapY = false);

    // Fuente activa para print() y drawString(). Con FreeFonts el origen de
    // print()/setCursor() es la línea base; drawString() con datum TOP_* no.
    // Restaurar JWPLC_FONT_GLCD al terminar (JWPLC_Display dibuja en GLCD).
    void setFont(const JWPLC_TFTFont &font);
    const JWPLC_TFTFont &font() const;

    void setTextDatum(JWPLC_TFTDatum datum);
    JWPLC_TFTDatum textDatum() const;

    // Ancho mínimo que drawString() rellena con el color de fondo.
    void setTextPadding(uint16_t width);
    uint16_t textPadding() const;

    // Dibuja texto anclado en (x, y) según el datum. Devuelve el ancho
    // dibujado o -1 si no se pudo adquirir el bus.
    int16_t drawString(
        const char *text,
        int16_t x,
        int16_t y,
        uint32_t timeoutMs = 50);

    int16_t textWidth(const char *text) const;
    int16_t fontHeight() const;

    // Helper compatible con la necesidad actual de JWPLC_Display.
    // En H3E.3 se validara su paridad pixel a pixel con el layout existente.
    void getTextBounds(
        const char *text,
        int16_t x,
        int16_t y,
        int16_t *x1,
        int16_t *y1,
        uint16_t *w,
        uint16_t *h) const;

    size_t write(uint8_t value) override;
    size_t write(const uint8_t *buffer, size_t size) override;
    using Print::write;

private:
    // JWPLC_TFTSprite::push() comparte el lock/batch de la TFT global.
    friend class JWPLC_TFTSprite;

    bool _ready;
    // NULL = sin batch. La tarea propietaria se publica de forma atomica.
    // No se expone al sketch ni cambia la API publica.
    std::atomic<void *> _batchOwner;
    uint8_t _textSize;
    uint16_t _textColor;
    uint16_t _textBackground;
    bool _textBackgroundEnabled;
    bool _wrapX;
    bool _wrapY;
    int16_t _cursorX;
    int16_t _cursorY;

    bool acquireForOperation(uint32_t timeoutMs);
    void releaseAfterOperation();
    void syncTextState();
    void syncCursorState();
};

// --- 0.4.0: sprites ------------------------------------------------------
// Lienzo en RAM: se dibuja sin tocar el bus SPI y se envía a la TFT de una
// vez con push(). Evita parpadeos y barridos al redibujar zonas que cambian
// (mapa FBD, listas con scroll, tendencias).
//
// Profundidad de color y RAM aproximada para un área de 320x140:
//   16 bits: RGB565 directo           ~87.5 KB  (sólo zonas pequeñas)
//    8 bits: RGB332 (colores RGB565 convertidos)  ~43.8 KB
//    4 bits: paleta de 16 colores (los colores son ÍNDICES 0..15)  ~21.9 KB
//    1 bit : monocromo (0 = fondo, 1 = tinta)  ~5.5 KB
// La memoria se reserva en create() y se libera en destroy() o al destruir
// el objeto. Dibujar en el sprite no necesita el bus; push() sí.
class JWPLC_TFTSprite
{
public:
    JWPLC_TFTSprite();
    ~JWPLC_TFTSprite();

    JWPLC_TFTSprite(const JWPLC_TFTSprite &) = delete;
    JWPLC_TFTSprite &operator=(const JWPLC_TFTSprite &) = delete;

    bool create(int16_t width, int16_t height, uint8_t colorDepth = 16);
    void destroy();
    bool created() const;
    int16_t width() const;
    int16_t height() const;
    uint8_t colorDepth() const;

    // Sólo con colorDepth 4: asigna el color RGB565 del índice 0..15.
    void setPaletteColor(uint8_t index, uint16_t color);

    void fill(uint16_t color);
    void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color);
    void drawRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color);
    void fillRoundRect(int16_t x, int16_t y, int16_t w, int16_t h, int16_t radius, uint16_t color);
    void drawRoundRect(int16_t x, int16_t y, int16_t w, int16_t h, int16_t radius, uint16_t color);
    void fillCircle(int16_t x, int16_t y, int16_t radius, uint16_t color);
    void drawCircle(int16_t x, int16_t y, int16_t radius, uint16_t color);
    void fillTriangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint16_t color);
    void drawFastHLine(int16_t x, int16_t y, int16_t w, uint16_t color);
    void drawFastVLine(int16_t x, int16_t y, int16_t h, uint16_t color);
    void drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color);
    // En sprites de 16 y 8 bits el borde se suaviza contra `background`; en 4
    // y 1 bit se dibuja sin suavizado (los colores son índices).
    void drawWideLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint8_t width, uint16_t color, uint16_t background = JWPLC_TFT_BLACK);
    void drawPixel(int16_t x, int16_t y, uint16_t color);

    void setFont(const JWPLC_TFTFont &font);
    void setTextColor(uint16_t foreground, uint16_t background);
    void setTextColor(uint16_t foreground);   // fondo transparente
    void setTextDatum(JWPLC_TFTDatum datum);
    void setTextSize(uint8_t size);
    int16_t drawString(const char *text, int16_t x, int16_t y);
    int16_t textWidth(const char *text);
    int16_t fontHeight();

    // Envía el sprite a la TFT con su esquina en (x, y). Respeta el recorte
    // activo. La variante con `transparent` no pinta ese color.
    bool push(int16_t x, int16_t y, uint32_t timeoutMs = 50);
    bool push(int16_t x, int16_t y, uint16_t transparent, uint32_t timeoutMs);

private:
    void *_impl;
    uint8_t _depth;
};

extern JWPLC_TFTClass JWPLC_TFT;

#endif // JWPLC_TFT_H
