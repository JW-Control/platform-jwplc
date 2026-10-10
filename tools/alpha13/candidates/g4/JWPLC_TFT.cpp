#include "JWPLC_TFT.h"

#include <SPI.h>
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
