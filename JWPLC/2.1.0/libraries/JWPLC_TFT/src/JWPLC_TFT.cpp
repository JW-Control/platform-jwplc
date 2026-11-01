#include "JWPLC_TFT.h"

#include <SPI.h>
#include <new>
#include <TFT_eSPI.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

extern "C"
{
#include "jwplc_spi_bus.h"
}

#ifndef JWPLC_TFT_BACKEND_SETUP
#error "JWPLC_TFT: no se cargo el setup privado del backend"
#endif

#if !defined(ST7789_DRIVER)
#error "JWPLC_TFT: ST7789_DRIVER requerido"
#endif

#if TFT_WIDTH != 170 || TFT_HEIGHT != 320
#error "JWPLC_TFT: H3E.2A requiere panel fisico Basic v2 170x320"
#endif

#if TFT_RGB_ORDER != TFT_BGR
#error "JWPLC_TFT: Basic v2 requiere orden BGR"
#endif

#if TFT_SPI_MODE != SPI_MODE0
#error "JWPLC_TFT: Basic v2 requiere SPI MODE0"
#endif

#if SPI_FREQUENCY != 80000000
#error "JWPLC_TFT: Basic v2 requiere TFT a 80 MHz"
#endif

#ifndef TFT_INVERSION_ON
#error "JWPLC_TFT: Basic v2 requiere inversion ON"
#endif

#ifndef SUPPORT_TRANSACTIONS
#error "JWPLC_TFT: SUPPORT_TRANSACTIONS requerido"
#endif

#if !defined(LOAD_GFXFF)
#error "JWPLC_TFT: LOAD_GFXFF requerido (fuentes 0.3.0)"
#endif

#if defined(LOAD_FONT2) || defined(LOAD_FONT4) || defined(LOAD_FONT6) || defined(LOAD_FONT7) || defined(LOAD_FONT8)
#error "JWPLC_TFT: Font2/4/6/7/8 de TFT_eSPI agregan ~9 KB a todo sketch; no habilitar"
#endif

namespace
{
    TFT_eSPI g_backend = TFT_eSPI();

    static constexpr uint8_t ACTIVE_ROTATION = 1;
    static constexpr uint8_t ST7789_CMD_DISPON = 0x29;
    static constexpr uint16_t NATIVE_WIDTH = 170;
    static constexpr uint16_t NATIVE_HEIGHT = 320;
    static constexpr uint16_t LOGICAL_WIDTH = 320;
    static constexpr uint16_t LOGICAL_HEIGHT = 170;
}

// Estado de texto extendido (0.3.0). Se guarda fuera de la clase para no
// cambiar el layout de JWPLC_TFTClass (ABI de los .a que ya enlazan con ella).
const JWPLC_TFTFont JWPLC_FONT_GLCD = {0, nullptr};
const JWPLC_TFTFont JWPLC_FONT_SANS_9 = {1, &FreeSans9pt7b};
const JWPLC_TFTFont JWPLC_FONT_SANS_BOLD_9 = {1, &FreeSansBold9pt7b};
const JWPLC_TFTFont JWPLC_FONT_SANS_12 = {1, &FreeSans12pt7b};
const JWPLC_TFTFont JWPLC_FONT_SANS_BOLD_12 = {1, &FreeSansBold12pt7b};
const JWPLC_TFTFont JWPLC_FONT_SANS_18 = {1, &FreeSans18pt7b};
const JWPLC_TFTFont JWPLC_FONT_SANS_BOLD_18 = {1, &FreeSansBold18pt7b};
const JWPLC_TFTFont JWPLC_FONT_SANS_24 = {1, &FreeSans24pt7b};
const JWPLC_TFTFont JWPLC_FONT_SANS_BOLD_24 = {1, &FreeSansBold24pt7b};
const JWPLC_TFTFont JWPLC_FONT_SERIF_9 = {1, &FreeSerif9pt7b};
const JWPLC_TFTFont JWPLC_FONT_SERIF_BOLD_9 = {1, &FreeSerifBold9pt7b};
const JWPLC_TFTFont JWPLC_FONT_SERIF_12 = {1, &FreeSerif12pt7b};
const JWPLC_TFTFont JWPLC_FONT_SERIF_BOLD_12 = {1, &FreeSerifBold12pt7b};
const JWPLC_TFTFont JWPLC_FONT_SERIF_18 = {1, &FreeSerif18pt7b};
const JWPLC_TFTFont JWPLC_FONT_SERIF_BOLD_18 = {1, &FreeSerifBold18pt7b};
const JWPLC_TFTFont JWPLC_FONT_SERIF_24 = {1, &FreeSerif24pt7b};
const JWPLC_TFTFont JWPLC_FONT_SERIF_BOLD_24 = {1, &FreeSerifBold24pt7b};
const JWPLC_TFTFont JWPLC_FONT_MONO_9 = {1, &FreeMono9pt7b};
const JWPLC_TFTFont JWPLC_FONT_MONO_BOLD_9 = {1, &FreeMonoBold9pt7b};
const JWPLC_TFTFont JWPLC_FONT_MONO_12 = {1, &FreeMono12pt7b};
const JWPLC_TFTFont JWPLC_FONT_MONO_BOLD_12 = {1, &FreeMonoBold12pt7b};
const JWPLC_TFTFont JWPLC_FONT_MONO_18 = {1, &FreeMono18pt7b};
const JWPLC_TFTFont JWPLC_FONT_MONO_BOLD_18 = {1, &FreeMonoBold18pt7b};
const JWPLC_TFTFont JWPLC_FONT_MONO_24 = {1, &FreeMono24pt7b};
const JWPLC_TFTFont JWPLC_FONT_MONO_BOLD_24 = {1, &FreeMonoBold24pt7b};

namespace
{
    const JWPLC_TFTFont *g_font = &JWPLC_FONT_GLCD;
    // 0.4.0: recorte activo. Fuera de la clase para no tocar su layout (ABI).
    bool g_clipActive = false;
    JWPLC_TFTDatum g_datum = JWPLC_TFTDatum::TOP_LEFT;
    uint16_t g_padding = 0;

    void applyFont(const JWPLC_TFTFont &font)
    {
        if (font.kind == 1 && font.data != nullptr)
        {
            g_backend.setFreeFont(static_cast<const GFXfont *>(font.data));
            return;
        }

        g_backend.setFreeFont(nullptr);
        g_backend.setTextFont(1);
    }
}

JWPLC_TFTClass JWPLC_TFT;

JWPLC_TFTClass::JWPLC_TFTClass()
    : _ready(false),
      _batchOwner(nullptr),
      _textSize(1),
      _textColor(JWPLC_TFT_WHITE),
      _textBackground(JWPLC_TFT_BLACK),
      _textBackgroundEnabled(true),
      _wrapX(true),
      _wrapY(false),
      _cursorX(0),
      _cursorY(0)
{
}

bool JWPLC_TFTClass::begin(uint32_t timeoutMs)
{
    if (_ready)
    {
        return true;
    }

    if (!jwplcSPI_begin())
    {
        return false;
    }

    SPI.begin(
        JWPLC_SPI_SCK,
        JWPLC_SPI_MISO,
        JWPLC_SPI_MOSI);

    if (!jwplcSPI_acquire(timeoutMs))
    {
        return false;
    }

    jwplcSPI_prepareForTFT();

    g_backend.init();
    g_backend.setRotation(ACTIVE_ROTATION);
    g_backend.fillScreen(JWPLC_TFT_BLACK);
    g_backend.writecommand(ST7789_CMD_DISPON);
    delay(120);

    syncTextState();
    syncCursorState();

    jwplcSPI_release();

    _ready =
        g_backend.width() == LOGICAL_WIDTH &&
        g_backend.height() == LOGICAL_HEIGHT;

    return _ready;
}

bool JWPLC_TFTClass::isReady() const
{
    return _ready;
}

JWPLC_TFTPanel JWPLC_TFTClass::panel() const
{
    return JWPLC_TFTPanel::BASIC_V2_ST7789_170X320;
}

JWPLC_TFTPanelInfo JWPLC_TFTClass::panelInfo() const
{
    return {
        "JWPLC Basic v2 ST7789 170x320",
        NATIVE_WIDTH,
        NATIVE_HEIGHT,
        LOGICAL_WIDTH,
        LOGICAL_HEIGHT,
        ACTIVE_ROTATION,
        true,
        true,
        JWPLC_SPI_TFT_HZ};
}

int16_t JWPLC_TFTClass::width() const
{
    return _ready
               ? g_backend.width()
               : (int16_t)LOGICAL_WIDTH;
}

int16_t JWPLC_TFTClass::height() const
{
    return _ready
               ? g_backend.height()
               : (int16_t)LOGICAL_HEIGHT;
}

uint8_t JWPLC_TFTClass::rotation() const
{
    return ACTIVE_ROTATION;
}

bool JWPLC_TFTClass::beginBatch(uint32_t timeoutMs)
{
    if (!_ready)
    {
        return false;
    }

    void *const self = xTaskGetCurrentTaskHandle();
    if (_batchOwner.load(std::memory_order_acquire) == self)
    {
        // Idempotencia previa para la misma tarea: un endBatch() basta.
        return true;
    }

    // Otra tarea tiene el batch: no se permite adoptar su transaccion.
    // Esperar el mutex SPI real respeta timeoutMs (0 = no esperar).
    if (!jwplcSPI_acquire(timeoutMs))
    {
        return false;
    }

    jwplcSPI_prepareForTFT();
    g_backend.startWrite();

    _batchOwner.store(self, std::memory_order_release);
    return true;
}

void JWPLC_TFTClass::endBatch()
{
    void *const self = xTaskGetCurrentTaskHandle();
    if (_batchOwner.load(std::memory_order_acquire) != self)
    {
        // Una tarea ajena nunca cierra ni libera el mutex del propietario.
        return;
    }

    if (g_clipActive)
    {
        // El recorte vive sólo dentro del batch que lo pidió.
        g_backend.resetViewport();
        g_clipActive = false;
    }

    g_backend.endWrite();
    _batchOwner.store(nullptr, std::memory_order_release);
    jwplcSPI_release();
}

bool JWPLC_TFTClass::batchActive() const
{
    return _batchOwner.load(std::memory_order_acquire) != nullptr;
}

bool JWPLC_TFTClass::acquireForOperation(uint32_t timeoutMs)
{
    if (!_ready)
    {
        return false;
    }

    void *const self = xTaskGetCurrentTaskHandle();
    if (_batchOwner.load(std::memory_order_acquire) == self)
    {
        // Las primitivas del propietario participan en su batch.
        return true;
    }

    // Batch de otra tarea: esperar el mutex, nunca saltarselo.
    if (!jwplcSPI_acquire(timeoutMs))
    {
        return false;
    }

    jwplcSPI_prepareForTFT();
    g_backend.startWrite();
    return true;
}

void JWPLC_TFTClass::releaseAfterOperation()
{
    void *const self = xTaskGetCurrentTaskHandle();
    if (_batchOwner.load(std::memory_order_acquire) == self)
    {
        return;
    }

    g_backend.endWrite();
    jwplcSPI_release();
}

bool JWPLC_TFTClass::fillScreen(
    uint16_t color,
    uint32_t timeoutMs)
{
    if (!acquireForOperation(timeoutMs))
    {
        return false;
    }

    g_backend.fillScreen(color);
    releaseAfterOperation();
    return true;
}

bool JWPLC_TFTClass::fillRect(
    int16_t x,
    int16_t y,
    int16_t w,
    int16_t h,
    uint16_t color,
    uint32_t timeoutMs)
{
    if (!acquireForOperation(timeoutMs))
    {
        return false;
    }

    g_backend.fillRect(x, y, w, h, color);
    releaseAfterOperation();
    return true;
}

bool JWPLC_TFTClass::drawRect(
    int16_t x,
    int16_t y,
    int16_t w,
    int16_t h,
    uint16_t color,
    uint32_t timeoutMs)
{
    if (!acquireForOperation(timeoutMs))
    {
        return false;
    }

    g_backend.drawRect(x, y, w, h, color);
    releaseAfterOperation();
    return true;
}

bool JWPLC_TFTClass::fillRoundRect(
    int16_t x,
    int16_t y,
    int16_t w,
    int16_t h,
    int16_t radius,
    uint16_t color,
    uint32_t timeoutMs)
{
    if (!acquireForOperation(timeoutMs))
    {
        return false;
    }

    g_backend.fillRoundRect(
        x,
        y,
        w,
        h,
        radius,
        color);

    releaseAfterOperation();
    return true;
}

bool JWPLC_TFTClass::drawRoundRect(
    int16_t x,
    int16_t y,
    int16_t w,
    int16_t h,
    int16_t radius,
    uint16_t color,
    uint32_t timeoutMs)
{
    if (!acquireForOperation(timeoutMs))
    {
        return false;
    }

    g_backend.drawRoundRect(
        x,
        y,
        w,
        h,
        radius,
        color);

    releaseAfterOperation();
    return true;
}

bool JWPLC_TFTClass::fillCircle(
    int16_t x,
    int16_t y,
    int16_t radius,
    uint16_t color,
    uint32_t timeoutMs)
{
    if (!acquireForOperation(timeoutMs))
    {
        return false;
    }

    g_backend.fillCircle(
        x,
        y,
        radius,
        color);

    releaseAfterOperation();
    return true;
}

bool JWPLC_TFTClass::drawCircle(
    int16_t x,
    int16_t y,
    int16_t radius,
    uint16_t color,
    uint32_t timeoutMs)
{
    if (!acquireForOperation(timeoutMs))
    {
        return false;
    }

    g_backend.drawCircle(
        x,
        y,
        radius,
        color);

    releaseAfterOperation();
    return true;
}

bool JWPLC_TFTClass::fillEllipse(
    int16_t x,
    int16_t y,
    int16_t rx,
    int16_t ry,
    uint16_t color,
    uint32_t timeoutMs)
{
    if (!acquireForOperation(timeoutMs))
    {
        return false;
    }

    g_backend.fillEllipse(
        x,
        y,
        rx,
        ry,
        color);

    releaseAfterOperation();
    return true;
}

bool JWPLC_TFTClass::drawEllipse(
    int16_t x,
    int16_t y,
    int16_t rx,
    int16_t ry,
    uint16_t color,
    uint32_t timeoutMs)
{
    if (!acquireForOperation(timeoutMs))
    {
        return false;
    }

    g_backend.drawEllipse(
        x,
        y,
        rx,
        ry,
        color);

    releaseAfterOperation();
    return true;
}

bool JWPLC_TFTClass::fillTriangle(
    int16_t x0,
    int16_t y0,
    int16_t x1,
    int16_t y1,
    int16_t x2,
    int16_t y2,
    uint16_t color,
    uint32_t timeoutMs)
{
    if (!acquireForOperation(timeoutMs))
    {
        return false;
    }

    g_backend.fillTriangle(
        x0,
        y0,
        x1,
        y1,
        x2,
        y2,
        color);

    releaseAfterOperation();
    return true;
}

bool JWPLC_TFTClass::drawTriangle(
    int16_t x0,
    int16_t y0,
    int16_t x1,
    int16_t y1,
    int16_t x2,
    int16_t y2,
    uint16_t color,
    uint32_t timeoutMs)
{
    if (!acquireForOperation(timeoutMs))
    {
        return false;
    }

    g_backend.drawTriangle(
        x0,
        y0,
        x1,
        y1,
        x2,
        y2,
        color);

    releaseAfterOperation();
    return true;
}

bool JWPLC_TFTClass::drawFastHLine(
    int16_t x,
    int16_t y,
    int16_t w,
    uint16_t color,
    uint32_t timeoutMs)
{
    if (!acquireForOperation(timeoutMs))
    {
        return false;
    }

    g_backend.drawFastHLine(x, y, w, color);
    releaseAfterOperation();
    return true;
}

bool JWPLC_TFTClass::drawFastVLine(
    int16_t x,
    int16_t y,
    int16_t h,
    uint16_t color,
    uint32_t timeoutMs)
{
    if (!acquireForOperation(timeoutMs))
    {
        return false;
    }

    g_backend.drawFastVLine(x, y, h, color);
    releaseAfterOperation();
    return true;
}

bool JWPLC_TFTClass::drawLine(
    int16_t x0,
    int16_t y0,
    int16_t x1,
    int16_t y1,
    uint16_t color,
    uint32_t timeoutMs)
{
    if (!acquireForOperation(timeoutMs))
    {
        return false;
    }

    g_backend.drawLine(x0, y0, x1, y1, color);
    releaseAfterOperation();
    return true;
}

bool JWPLC_TFTClass::drawPixel(
    int16_t x,
    int16_t y,
    uint16_t color,
    uint32_t timeoutMs)
{
    if (!acquireForOperation(timeoutMs))
    {
        return false;
    }

    g_backend.drawPixel(x, y, color);
    releaseAfterOperation();
    return true;
}

bool JWPLC_TFTClass::pushImage(
    int16_t x,
    int16_t y,
    int16_t w,
    int16_t h,
    const uint16_t *pixels,
    uint32_t timeoutMs)
{
    if (pixels == nullptr || w <= 0 || h <= 0)
    {
        return false;
    }

    if (!acquireForOperation(timeoutMs))
    {
        return false;
    }

    // Los colores llegan en el orden de la CPU (little-endian), igual que en
    // drawPixel; el ST7789 los espera con el byte alto primero.
    const bool swap = g_backend.getSwapBytes();
    g_backend.setSwapBytes(true);
    g_backend.pushImage(x, y, w, h, pixels);
    g_backend.setSwapBytes(swap);

    releaseAfterOperation();
    return true;
}

void JWPLC_TFTClass::syncTextState()
{
    g_backend.setTextSize(_textSize);

    if (_textBackgroundEnabled)
    {
        g_backend.setTextColor(
            _textColor,
            _textBackground);
    }
    else
    {
        g_backend.setTextColor(
            _textColor);
    }

    g_backend.setTextWrap(
        _wrapX,
        _wrapY);

    applyFont(*g_font);
    g_backend.setTextDatum(static_cast<uint8_t>(g_datum));
    g_backend.setTextPadding(g_padding);
}

void JWPLC_TFTClass::setFont(const JWPLC_TFTFont &font)
{
    g_font = &font;
    applyFont(font);
}

const JWPLC_TFTFont &JWPLC_TFTClass::font() const
{
    return *g_font;
}

void JWPLC_TFTClass::setTextDatum(JWPLC_TFTDatum datum)
{
    g_datum = datum;
    g_backend.setTextDatum(static_cast<uint8_t>(datum));
}

JWPLC_TFTDatum JWPLC_TFTClass::textDatum() const
{
    return g_datum;
}

void JWPLC_TFTClass::setTextPadding(uint16_t width)
{
    g_padding = width;
    g_backend.setTextPadding(width);
}

uint16_t JWPLC_TFTClass::textPadding() const
{
    return g_padding;
}

int16_t JWPLC_TFTClass::drawString(
    const char *text,
    int16_t x,
    int16_t y,
    uint32_t timeoutMs)
{
    if (text == nullptr)
    {
        return 0;
    }

    if (!acquireForOperation(timeoutMs))
    {
        return -1;
    }

    const int16_t drawn = g_backend.drawString(text, x, y);
    releaseAfterOperation();
    return drawn;
}

void JWPLC_TFTClass::syncCursorState()
{
    g_backend.setCursor(
        _cursorX,
        _cursorY);
}

void JWPLC_TFTClass::setCursor(
    int16_t x,
    int16_t y)
{
    _cursorX = x;
    _cursorY = y;
    syncCursorState();
}

int16_t JWPLC_TFTClass::cursorX() const
{
    return _cursorX;
}

int16_t JWPLC_TFTClass::cursorY() const
{
    return _cursorY;
}

void JWPLC_TFTClass::setTextSize(uint8_t size)
{
    _textSize =
        (size == 0)
            ? 1
            : size;

    g_backend.setTextSize(
        _textSize);
}

uint8_t JWPLC_TFTClass::textSize() const
{
    return _textSize;
}

void JWPLC_TFTClass::setTextColor(uint16_t foreground)
{
    _textColor = foreground;
    _textBackgroundEnabled = false;

    g_backend.setTextColor(
        _textColor);
}

void JWPLC_TFTClass::setTextColor(
    uint16_t foreground,
    uint16_t background)
{
    _textColor = foreground;
    _textBackground = background;
    _textBackgroundEnabled = true;

    g_backend.setTextColor(
        _textColor,
        _textBackground);
}

uint16_t JWPLC_TFTClass::textColor() const
{
    return _textColor;
}

uint16_t JWPLC_TFTClass::textBackground() const
{
    return _textBackground;
}

void JWPLC_TFTClass::setTextWrap(
    bool wrapX,
    bool wrapY)
{
    _wrapX = wrapX;
    _wrapY = wrapY;

    g_backend.setTextWrap(
        _wrapX,
        _wrapY);
}

int16_t JWPLC_TFTClass::textWidth(
    const char *text) const
{
    if (text == nullptr)
    {
        return 0;
    }

    return g_backend.textWidth(text);
}

int16_t JWPLC_TFTClass::fontHeight() const
{
    return g_backend.fontHeight();
}

void JWPLC_TFTClass::getTextBounds(
    const char *text,
    int16_t x,
    int16_t y,
    int16_t *x1,
    int16_t *y1,
    uint16_t *w,
    uint16_t *h) const
{
    if (x1 != nullptr)
    {
        *x1 = x;
    }

    if (y1 != nullptr)
    {
        *y1 = y;
    }

    if (w != nullptr)
    {
        const int16_t measured =
            textWidth(text);

        *w =
            (measured > 0)
                ? (uint16_t)measured
                : 0U;
    }

    if (h != nullptr)
    {
        const int16_t measured =
            (text == nullptr || text[0] == '\0')
                ? 0
                : fontHeight();

        *h =
            (measured > 0)
                ? (uint16_t)measured
                : 0U;
    }
}

size_t JWPLC_TFTClass::write(uint8_t value)
{
    if (!acquireForOperation(50))
    {
        return 0;
    }

    const size_t written =
        g_backend.write(value);

    _cursorX = g_backend.getCursorX();
    _cursorY = g_backend.getCursorY();

    releaseAfterOperation();
    return written;
}

size_t JWPLC_TFTClass::write(
    const uint8_t *buffer,
    size_t size)
{
    if (buffer == nullptr || size == 0)
    {
        return 0;
    }

    if (!acquireForOperation(50))
    {
        return 0;
    }

    const size_t written =
        static_cast<Print &>(g_backend).write(
            buffer,
            size);

    _cursorX = g_backend.getCursorX();
    _cursorY = g_backend.getCursorY();

    releaseAfterOperation();
    return written;
}

// ---------------------------------------------------------------------------
// 0.4.0: recorte, líneas gruesas y polilíneas
// ---------------------------------------------------------------------------

bool JWPLC_TFTClass::setClipRect(
    int16_t x,
    int16_t y,
    int16_t w,
    int16_t h)
{
    void *const self = xTaskGetCurrentTaskHandle();
    if (!_ready || _batchOwner.load(std::memory_order_acquire) != self)
    {
        // Fuera de un batch propio el recorte podría quedar activo para otra
        // tarea (por ejemplo JWPLC_Display): se rechaza.
        return false;
    }

    if (w <= 0 || h <= 0)
    {
        return false;
    }

    // vpDatum=false: las coordenadas siguen siendo de pantalla; sólo recorta.
    g_backend.setViewport(x, y, w, h, false);
    g_clipActive = true;
    return true;
}

void JWPLC_TFTClass::resetClip()
{
    void *const self = xTaskGetCurrentTaskHandle();
    if (_batchOwner.load(std::memory_order_acquire) != self)
    {
        return;
    }

    if (g_clipActive)
    {
        g_backend.resetViewport();
        g_clipActive = false;
    }
}

bool JWPLC_TFTClass::clipActive() const
{
    return g_clipActive;
}

bool JWPLC_TFTClass::drawWideLine(
    int16_t x0,
    int16_t y0,
    int16_t x1,
    int16_t y1,
    uint8_t width,
    uint16_t color,
    uint16_t background,
    uint32_t timeoutMs)
{
    if (!acquireForOperation(timeoutMs))
    {
        return false;
    }

    if (width <= 1)
    {
        g_backend.drawLine(x0, y0, x1, y1, color);
    }
    else
    {
        // Siempre con fondo explícito: el valor por defecto de TFT_eSPI
        // (0x00FFFFFF) leería la GRAM y el bus compartido no lo garantiza.
        g_backend.drawWideLine(
            static_cast<float>(x0),
            static_cast<float>(y0),
            static_cast<float>(x1),
            static_cast<float>(y1),
            static_cast<float>(width),
            color,
            background);
    }

    releaseAfterOperation();
    return true;
}

bool JWPLC_TFTClass::drawPolyline(
    const int16_t *xy,
    size_t points,
    uint8_t width,
    uint16_t color,
    uint16_t background,
    uint32_t timeoutMs)
{
    if (xy == nullptr || points < 2)
    {
        return false;
    }

    if (!acquireForOperation(timeoutMs))
    {
        return false;
    }

    for (size_t i = 1; i < points; ++i)
    {
        const int16_t ax = xy[(i - 1) * 2];
        const int16_t ay = xy[(i - 1) * 2 + 1];
        const int16_t bx = xy[i * 2];
        const int16_t by = xy[i * 2 + 1];

        if (width <= 1)
        {
            g_backend.drawLine(ax, ay, bx, by, color);
        }
        else
        {
            g_backend.drawWideLine(
                static_cast<float>(ax),
                static_cast<float>(ay),
                static_cast<float>(bx),
                static_cast<float>(by),
                static_cast<float>(width),
                color,
                background);
        }
    }

    releaseAfterOperation();
    return true;
}

// ---------------------------------------------------------------------------
// 0.4.0: JWPLC_TFTSprite (envoltorio de TFT_eSprite; no se expone al sketch)
// ---------------------------------------------------------------------------

namespace
{
    inline TFT_eSprite *spr(void *impl)
    {
        return static_cast<TFT_eSprite *>(impl);
    }

    void applySpriteFont(TFT_eSprite *s, const JWPLC_TFTFont &font)
    {
        if (font.kind == 1 && font.data != nullptr)
        {
            s->setFreeFont(static_cast<const GFXfont *>(font.data));
            return;
        }

        s->setFreeFont(nullptr);
        s->setTextFont(1);
    }
}

JWPLC_TFTSprite::JWPLC_TFTSprite()
    : _impl(nullptr),
      _depth(16)
{
}

JWPLC_TFTSprite::~JWPLC_TFTSprite()
{
    destroy();
}

bool JWPLC_TFTSprite::create(int16_t width, int16_t height, uint8_t colorDepth)
{
    destroy();

    if (width <= 0 || height <= 0)
    {
        return false;
    }

    if (colorDepth != 16 && colorDepth != 8 && colorDepth != 4 && colorDepth != 1)
    {
        return false;
    }

    TFT_eSprite *s = new (std::nothrow) TFT_eSprite(&g_backend);
    if (s == nullptr)
    {
        return false;
    }

    s->setColorDepth(colorDepth);
    if (s->createSprite(width, height) == nullptr)
    {
        delete s;
        return false;
    }

    applySpriteFont(s, JWPLC_FONT_GLCD);
    s->setTextDatum(TL_DATUM);
    _impl = s;
    _depth = colorDepth;
    return true;
}

void JWPLC_TFTSprite::destroy()
{
    if (_impl == nullptr)
    {
        return;
    }

    spr(_impl)->deleteSprite();
    delete spr(_impl);
    _impl = nullptr;
}

bool JWPLC_TFTSprite::created() const { return _impl != nullptr; }
int16_t JWPLC_TFTSprite::width() const { return _impl ? spr(_impl)->width() : 0; }
int16_t JWPLC_TFTSprite::height() const { return _impl ? spr(_impl)->height() : 0; }
uint8_t JWPLC_TFTSprite::colorDepth() const { return _depth; }

void JWPLC_TFTSprite::setPaletteColor(uint8_t index, uint16_t color)
{
    if (_impl && _depth == 4 && index < 16)
    {
        spr(_impl)->setPaletteColor(index, color);
    }
}

void JWPLC_TFTSprite::fill(uint16_t color)
{
    if (_impl) spr(_impl)->fillSprite(color);
}

void JWPLC_TFTSprite::fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color)
{
    if (_impl) spr(_impl)->fillRect(x, y, w, h, color);
}

void JWPLC_TFTSprite::drawRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color)
{
    if (_impl) spr(_impl)->drawRect(x, y, w, h, color);
}

void JWPLC_TFTSprite::fillRoundRect(int16_t x, int16_t y, int16_t w, int16_t h, int16_t radius, uint16_t color)
{
    if (_impl) spr(_impl)->fillRoundRect(x, y, w, h, radius, color);
}

void JWPLC_TFTSprite::drawRoundRect(int16_t x, int16_t y, int16_t w, int16_t h, int16_t radius, uint16_t color)
{
    if (_impl) spr(_impl)->drawRoundRect(x, y, w, h, radius, color);
}

void JWPLC_TFTSprite::fillCircle(int16_t x, int16_t y, int16_t radius, uint16_t color)
{
    if (_impl) spr(_impl)->fillCircle(x, y, radius, color);
}

void JWPLC_TFTSprite::drawCircle(int16_t x, int16_t y, int16_t radius, uint16_t color)
{
    if (_impl) spr(_impl)->drawCircle(x, y, radius, color);
}

void JWPLC_TFTSprite::fillTriangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint16_t color)
{
    if (_impl) spr(_impl)->fillTriangle(x0, y0, x1, y1, x2, y2, color);
}

void JWPLC_TFTSprite::drawFastHLine(int16_t x, int16_t y, int16_t w, uint16_t color)
{
    if (_impl) spr(_impl)->drawFastHLine(x, y, w, color);
}

void JWPLC_TFTSprite::drawFastVLine(int16_t x, int16_t y, int16_t h, uint16_t color)
{
    if (_impl) spr(_impl)->drawFastVLine(x, y, h, color);
}

void JWPLC_TFTSprite::drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color)
{
    if (_impl) spr(_impl)->drawLine(x0, y0, x1, y1, color);
}

void JWPLC_TFTSprite::drawWideLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint8_t width, uint16_t color, uint16_t background)
{
    if (_impl == nullptr)
    {
        return;
    }

    TFT_eSprite *s = spr(_impl);
    if (width <= 1)
    {
        s->drawLine(x0, y0, x1, y1, color);
        return;
    }

    if (_depth == 4 || _depth == 1)
    {
        // Con índices de paleta no hay mezcla de colores: trazo sólido
        // formado por líneas paralelas desplazadas.
        const int16_t half = width / 2;
        const bool mostlyHorizontal = abs(x1 - x0) >= abs(y1 - y0);
        for (int16_t d = -half; d <= half - ((width % 2) ? 0 : 1); ++d)
        {
            if (mostlyHorizontal)
            {
                s->drawLine(x0, y0 + d, x1, y1 + d, color);
            }
            else
            {
                s->drawLine(x0 + d, y0, x1 + d, y1, color);
            }
        }
        return;
    }

    s->drawWideLine(
        static_cast<float>(x0),
        static_cast<float>(y0),
        static_cast<float>(x1),
        static_cast<float>(y1),
        static_cast<float>(width),
        color,
        background);
}

void JWPLC_TFTSprite::drawPixel(int16_t x, int16_t y, uint16_t color)
{
    if (_impl) spr(_impl)->drawPixel(x, y, color);
}

void JWPLC_TFTSprite::setFont(const JWPLC_TFTFont &font)
{
    if (_impl) applySpriteFont(spr(_impl), font);
}

void JWPLC_TFTSprite::setTextColor(uint16_t foreground, uint16_t background)
{
    if (_impl) spr(_impl)->setTextColor(foreground, background, true);
}

void JWPLC_TFTSprite::setTextColor(uint16_t foreground)
{
    if (_impl) spr(_impl)->setTextColor(foreground);
}

void JWPLC_TFTSprite::setTextDatum(JWPLC_TFTDatum datum)
{
    if (_impl) spr(_impl)->setTextDatum(static_cast<uint8_t>(datum));
}

void JWPLC_TFTSprite::setTextSize(uint8_t size)
{
    if (_impl) spr(_impl)->setTextSize(size == 0 ? 1 : size);
}

int16_t JWPLC_TFTSprite::drawString(const char *text, int16_t x, int16_t y)
{
    if (_impl == nullptr || text == nullptr)
    {
        return 0;
    }

    return spr(_impl)->drawString(text, x, y);
}

int16_t JWPLC_TFTSprite::textWidth(const char *text)
{
    if (_impl == nullptr || text == nullptr)
    {
        return 0;
    }

    return spr(_impl)->textWidth(text);
}

int16_t JWPLC_TFTSprite::fontHeight()
{
    return _impl ? spr(_impl)->fontHeight() : 0;
}

bool JWPLC_TFTSprite::push(int16_t x, int16_t y, uint32_t timeoutMs)
{
    if (_impl == nullptr)
    {
        return false;
    }

    // Mismo lock que las primitivas: si la tarea ya tiene un batch abierto
    // el envío participa en él (y en su recorte); si no, toma y suelta el bus.
    if (!JWPLC_TFT.acquireForOperation(timeoutMs))
    {
        return false;
    }

    spr(_impl)->pushSprite(x, y);
    JWPLC_TFT.releaseAfterOperation();
    return true;
}

bool JWPLC_TFTSprite::push(int16_t x, int16_t y, uint16_t transparent, uint32_t timeoutMs)
{
    if (_impl == nullptr)
    {
        return false;
    }

    // Mismo lock que las primitivas: si la tarea ya tiene un batch abierto
    // el envío participa en él (y en su recorte); si no, toma y suelta el bus.
    if (!JWPLC_TFT.acquireForOperation(timeoutMs))
    {
        return false;
    }

    spr(_impl)->pushSprite(x, y, transparent);
    JWPLC_TFT.releaseAfterOperation();
    return true;
}
