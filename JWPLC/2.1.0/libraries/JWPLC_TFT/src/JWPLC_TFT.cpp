#include "JWPLC_TFT.h"

#include <SPI.h>
#include <TFT_eSPI.h>

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

#if !defined(LOAD_FONT2) || !defined(LOAD_FONT4) || !defined(LOAD_GFXFF)
#error "JWPLC_TFT: LOAD_FONT2, LOAD_FONT4 y LOAD_GFXFF requeridos"
#endif

namespace
{
    TFT_eSPI g_backend = TFT_eSPI();

    static constexpr uint8_t ACTIVE_ROTATION = 1;
    static constexpr uint16_t NATIVE_WIDTH = 170;
    static constexpr uint16_t NATIVE_HEIGHT = 320;
    static constexpr uint16_t LOGICAL_WIDTH = 320;
    static constexpr uint16_t LOGICAL_HEIGHT = 170;

    // Devuelve la FreeFont asociada o nullptr para fuentes bitmap.
    const GFXfont *freeFontFor(JWPLC_TFTFont font)
    {
        switch (font)
        {
        case JWPLC_TFTFont::SANS_9:        return &FreeSans9pt7b;
        case JWPLC_TFTFont::SANS_BOLD_9:   return &FreeSansBold9pt7b;
        case JWPLC_TFTFont::SANS_12:       return &FreeSans12pt7b;
        case JWPLC_TFTFont::SANS_BOLD_12:  return &FreeSansBold12pt7b;
        case JWPLC_TFTFont::SANS_18:       return &FreeSans18pt7b;
        case JWPLC_TFTFont::SANS_BOLD_18:  return &FreeSansBold18pt7b;
        case JWPLC_TFTFont::SANS_24:       return &FreeSans24pt7b;
        case JWPLC_TFTFont::SANS_BOLD_24:  return &FreeSansBold24pt7b;
        case JWPLC_TFTFont::SERIF_9:       return &FreeSerif9pt7b;
        case JWPLC_TFTFont::SERIF_BOLD_9:  return &FreeSerifBold9pt7b;
        case JWPLC_TFTFont::SERIF_12:      return &FreeSerif12pt7b;
        case JWPLC_TFTFont::SERIF_BOLD_12: return &FreeSerifBold12pt7b;
        case JWPLC_TFTFont::SERIF_18:      return &FreeSerif18pt7b;
        case JWPLC_TFTFont::SERIF_BOLD_18: return &FreeSerifBold18pt7b;
        case JWPLC_TFTFont::SERIF_24:      return &FreeSerif24pt7b;
        case JWPLC_TFTFont::SERIF_BOLD_24: return &FreeSerifBold24pt7b;
        case JWPLC_TFTFont::MONO_9:        return &FreeMono9pt7b;
        case JWPLC_TFTFont::MONO_BOLD_9:   return &FreeMonoBold9pt7b;
        case JWPLC_TFTFont::MONO_12:       return &FreeMono12pt7b;
        case JWPLC_TFTFont::MONO_BOLD_12:  return &FreeMonoBold12pt7b;
        case JWPLC_TFTFont::MONO_18:       return &FreeMono18pt7b;
        case JWPLC_TFTFont::MONO_BOLD_18:  return &FreeMonoBold18pt7b;
        case JWPLC_TFTFont::MONO_24:       return &FreeMono24pt7b;
        case JWPLC_TFTFont::MONO_BOLD_24:  return &FreeMonoBold24pt7b;
        default:                           return nullptr;
        }
    }

    void applyFont(JWPLC_TFTFont font)
    {
        const GFXfont *gfx = freeFontFor(font);

        if (gfx != nullptr)
        {
            g_backend.setFreeFont(gfx);
            return;
        }

        // Limpia cualquier FreeFont previa antes de elegir una bitmap.
        g_backend.setFreeFont(nullptr);

        switch (font)
        {
        case JWPLC_TFTFont::FONT2: g_backend.setTextFont(2); break;
        case JWPLC_TFTFont::FONT4: g_backend.setTextFont(4); break;
        default:                   g_backend.setTextFont(1); break;
        }
    }
}

JWPLC_TFTClass JWPLC_TFT;

JWPLC_TFTClass::JWPLC_TFTClass()
    : _ready(false),
      _batchActive(false),
      _textSize(1),
      _textColor(JWPLC_TFT_WHITE),
      _textBackground(JWPLC_TFT_BLACK),
      _textBackgroundEnabled(true),
      _wrapX(true),
      _wrapY(false),
      _font(JWPLC_TFTFont::GLCD),
      _datum(JWPLC_TFTDatum::TOP_LEFT),
      _padding(0),
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

    if (_batchActive)
    {
        return true;
    }

    if (!jwplcSPI_acquire(timeoutMs))
    {
        return false;
    }

    jwplcSPI_prepareForTFT();
    g_backend.startWrite();

    _batchActive = true;
    return true;
}

void JWPLC_TFTClass::endBatch()
{
    if (!_batchActive)
    {
        return;
    }

    g_backend.endWrite();
    _batchActive = false;
    jwplcSPI_release();
}

bool JWPLC_TFTClass::batchActive() const
{
    return _batchActive;
}

bool JWPLC_TFTClass::acquireForOperation(uint32_t timeoutMs)
{
    if (!_ready)
    {
        return false;
    }

    if (_batchActive)
    {
        return true;
    }

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
    if (_batchActive)
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

    applyFont(_font);
    g_backend.setTextDatum(static_cast<uint8_t>(_datum));
    g_backend.setTextPadding(_padding);
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

void JWPLC_TFTClass::setFont(JWPLC_TFTFont font)
{
    _font = font;
    applyFont(_font);
}

JWPLC_TFTFont JWPLC_TFTClass::font() const
{
    return _font;
}

void JWPLC_TFTClass::setTextDatum(JWPLC_TFTDatum datum)
{
    _datum = datum;
    g_backend.setTextDatum(static_cast<uint8_t>(_datum));
}

JWPLC_TFTDatum JWPLC_TFTClass::textDatum() const
{
    return _datum;
}

void JWPLC_TFTClass::setTextPadding(uint16_t width)
{
    _padding = width;
    g_backend.setTextPadding(_padding);
}

uint16_t JWPLC_TFTClass::textPadding() const
{
    return _padding;
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
