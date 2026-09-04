#ifndef JWPLC_UI_H
#define JWPLC_UI_H

#include <Arduino.h>
#include <stddef.h>

class Adafruit_ST7789;

static constexpr int16_t JWPLC_UI_AUTO = -1;
static constexpr uint8_t JWPLC_UI_MAX_FIELDS = 32;

enum JWPLC_UIFieldType : uint8_t
{
    JWPLC_UI_FIELD_VALUE = 0,
    JWPLC_UI_FIELD_TEXT,
    JWPLC_UI_FIELD_BOOL,
    JWPLC_UI_FIELD_BAR
};

enum JWPLC_UIFieldLayout : uint8_t
{
    JWPLC_UI_LAYOUT_INLINE = 0,
    JWPLC_UI_LAYOUT_STACKED
};

enum JWPLC_UIRefreshMode : uint8_t
{
    USER_REFRESH_ON_DEMAND = 0,
    USER_REFRESH_PERIODIC
};

struct JWPLC_UIValueFormat
{
    uint8_t integerDigits = 3;
    uint8_t decimalDigits = 0;
    bool reserveSign = false;
    bool leadingZeros = false;
};

struct JWPLC_UIField
{
    uint8_t id = 0;
    uint8_t page = 0;
    JWPLC_UIFieldType type = JWPLC_UI_FIELD_VALUE;

    int16_t x = 0;
    int16_t y = 0;
    int16_t width = JWPLC_UI_AUTO;
    int16_t height = JWPLC_UI_AUTO;

    const char *label = nullptr;
    const char *unit = nullptr;

    uint8_t labelTextSize = 1;
    uint8_t valueTextSize = 2;

    bool frame = false;

    uint16_t labelColor = 0xFFFF;
    uint16_t valueColor = 0xFFFF;
    uint16_t backgroundColor = 0x0000;
    uint16_t frameColor = 0xFFFF;

    JWPLC_UIFieldLayout layout = JWPLC_UI_LAYOUT_INLINE;
    JWPLC_UIValueFormat format = {};

    uint8_t textCapacity = 12;

    const char *falseText = "OFF";
    const char *trueText = "ON";

    float barMin = 0.0f;
    float barMax = 100.0f;
};

inline JWPLC_UIField JWPLC_UIValueField(
    uint8_t id,
    int16_t x,
    int16_t y,
    const char *label = nullptr,
    const char *unit = nullptr,
    uint8_t integerDigits = 3,
    uint8_t decimalDigits = 0,
    bool reserveSign = false,
    bool leadingZeros = false,
    uint8_t valueTextSize = 2,
    uint8_t labelTextSize = 1,
    bool frame = false,
    int16_t width = JWPLC_UI_AUTO,
    int16_t height = JWPLC_UI_AUTO,
    uint8_t page = 0,
    JWPLC_UIFieldLayout layout = JWPLC_UI_LAYOUT_INLINE)
{
    JWPLC_UIField field;
    field.id = id;
    field.page = page;
    field.type = JWPLC_UI_FIELD_VALUE;
    field.x = x;
    field.y = y;
    field.width = width;
    field.height = height;
    field.label = label;
    field.unit = unit;
    field.labelTextSize = labelTextSize;
    field.valueTextSize = valueTextSize;
    field.frame = frame;
    field.layout = layout;
    field.format.integerDigits = integerDigits;
    field.format.decimalDigits = decimalDigits;
    field.format.reserveSign = reserveSign;
    field.format.leadingZeros = leadingZeros;
    return field;
}

inline JWPLC_UIField JWPLC_UITextField(
    uint8_t id,
    int16_t x,
    int16_t y,
    const char *label = nullptr,
    uint8_t textCapacity = 12,
    uint8_t valueTextSize = 1,
    uint8_t labelTextSize = 1,
    bool frame = false,
    int16_t width = JWPLC_UI_AUTO,
    int16_t height = JWPLC_UI_AUTO,
    uint8_t page = 0,
    JWPLC_UIFieldLayout layout = JWPLC_UI_LAYOUT_INLINE)
{
    JWPLC_UIField field;
    field.id = id;
    field.page = page;
    field.type = JWPLC_UI_FIELD_TEXT;
    field.x = x;
    field.y = y;
    field.width = width;
    field.height = height;
    field.label = label;
    field.labelTextSize = labelTextSize;
    field.valueTextSize = valueTextSize;
    field.frame = frame;
    field.layout = layout;
    field.textCapacity = textCapacity;
    return field;
}

inline JWPLC_UIField JWPLC_UIBoolField(
    uint8_t id,
    int16_t x,
    int16_t y,
    const char *label = nullptr,
    const char *falseText = "OFF",
    const char *trueText = "ON",
    uint8_t valueTextSize = 2,
    uint8_t labelTextSize = 1,
    bool frame = false,
    int16_t width = JWPLC_UI_AUTO,
    int16_t height = JWPLC_UI_AUTO,
    uint8_t page = 0,
    JWPLC_UIFieldLayout layout = JWPLC_UI_LAYOUT_INLINE)
{
    JWPLC_UIField field;
    field.id = id;
    field.page = page;
    field.type = JWPLC_UI_FIELD_BOOL;
    field.x = x;
    field.y = y;
    field.width = width;
    field.height = height;
    field.label = label;
    field.labelTextSize = labelTextSize;
    field.valueTextSize = valueTextSize;
    field.frame = frame;
    field.layout = layout;
    field.falseText = falseText;
    field.trueText = trueText;
    return field;
}

inline JWPLC_UIField JWPLC_UIBarField(
    uint8_t id,
    int16_t x,
    int16_t y,
    const char *label = nullptr,
    float minValue = 0.0f,
    float maxValue = 100.0f,
    int16_t width = 100,
    int16_t height = 18,
    uint8_t labelTextSize = 1,
    bool frame = false,
    uint8_t page = 0,
    JWPLC_UIFieldLayout layout = JWPLC_UI_LAYOUT_STACKED)
{
    JWPLC_UIField field;
    field.id = id;
    field.page = page;
    field.type = JWPLC_UI_FIELD_BAR;
    field.x = x;
    field.y = y;
    field.width = width;
    field.height = height;
    field.label = label;
    field.labelTextSize = labelTextSize;
    field.valueTextSize = 1;
    field.frame = frame;
    field.layout = layout;
    field.barMin = minValue;
    field.barMax = maxValue;
    return field;
}

namespace JWPLCUI
{
    bool setFields(const JWPLC_UIField *fields, size_t count);
    void clearFields();
    size_t fieldCount();

    bool setNumericValue(uint8_t fieldId, double value);
    bool setText(uint8_t fieldId, const char *value);
    bool setBool(uint8_t fieldId, bool value);
    bool setBar(uint8_t fieldId, float value);

    void setRefreshMode(JWPLC_UIRefreshMode mode);
    JWPLC_UIRefreshMode refreshMode();

    void requestRefresh();
    bool refreshNeeded();
    void consumeRefreshRequest();

    void setPage(uint8_t page);
    uint8_t currentPage();

    bool pageRedrawPending();
    void consumePageRedrawPending();

    void invalidateField(uint8_t fieldId);
    void invalidateAll(bool redrawStatic);

    void prepareEnter();

    void drawStatic(Adafruit_ST7789 &tft);
    void drawDirty(Adafruit_ST7789 &tft);
}

#endif // JWPLC_UI_H