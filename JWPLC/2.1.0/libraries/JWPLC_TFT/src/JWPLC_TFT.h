#ifndef JWPLC_TFT_H
#define JWPLC_TFT_H

#include <Arduino.h>
#include <stdint.h>

#define JWPLC_TFT_VERSION_MAJOR 0
#define JWPLC_TFT_VERSION_MINOR 1
#define JWPLC_TFT_VERSION_PATCH 0

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
    bool _ready;
    bool _batchActive;
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

extern JWPLC_TFTClass JWPLC_TFT;

#endif // JWPLC_TFT_H
