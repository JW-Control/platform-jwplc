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

enum JWPLC_UIValueAlign : uint8_t
{
    JWPLC_UI_ALIGN_LEFT = 0,
    JWPLC_UI_ALIGN_CENTER,
    JWPLC_UI_ALIGN_RIGHT
};

enum JWPLC_UIRefreshMode : uint8_t
{
    USER_REFRESH_ON_DEMAND = 0,
    USER_REFRESH_PERIODIC
};

// Los grupos usan constructores simples para mantener compatibilidad C++11
// y permitir inicializadores compactos con {...} en sketches Arduino.
struct JWPLC_UIFieldMeta
{
    uint8_t id;
    uint8_t page;
    JWPLC_UIFieldType type;

    JWPLC_UIFieldMeta(
        uint8_t idValue = 0,
        uint8_t pageValue = 0,
        JWPLC_UIFieldType typeValue = JWPLC_UI_FIELD_VALUE)
        : id(idValue), page(pageValue), type(typeValue)
    {
    }
};

struct JWPLC_UIRect
{
    int16_t x;
    int16_t y;
    int16_t width;
    int16_t height;

    JWPLC_UIRect(
        int16_t xValue = 0,
        int16_t yValue = 0,
        int16_t widthValue = JWPLC_UI_AUTO,
        int16_t heightValue = JWPLC_UI_AUTO)
        : x(xValue), y(yValue), width(widthValue), height(heightValue)
    {
    }
};

struct JWPLC_UIText
{
    const char *label;
    const char *unit;

    JWPLC_UIText(
        const char *labelValue = nullptr,
        const char *unitValue = nullptr)
        : label(labelValue), unit(unitValue)
    {
    }
};

struct JWPLC_UIColors
{
    uint16_t label;
    uint16_t value;
    uint16_t background;
    uint16_t frame;

    JWPLC_UIColors(
        uint16_t labelValue = 0xFFFF,
        uint16_t valueValue = 0xFFFF,
        uint16_t backgroundValue = 0x0000,
        uint16_t frameValue = 0xFFFF)
        : label(labelValue),
          value(valueValue),
          background(backgroundValue),
          frame(frameValue)
    {
    }
};

struct JWPLC_UIStyle
{
    uint8_t labelTextSize;
    uint8_t valueTextSize;
    bool frame;
    JWPLC_UIFieldLayout layout;
    JWPLC_UIValueAlign valueAlign;

    // Mirror interno temporal para mantener el motor Alpha8 desacoplado de
    // la forma de inicializacion publica. El usuario configura colors como
    // grupo propio dentro de JWPLC_UIField, no anidado dentro de style.
    JWPLC_UIColors colors;

    JWPLC_UIStyle(
        uint8_t labelSizeValue = 1,
        uint8_t valueSizeValue = 2,
        bool frameValue = false,
        JWPLC_UIFieldLayout layoutValue = JWPLC_UI_LAYOUT_INLINE,
        JWPLC_UIValueAlign alignValue = JWPLC_UI_ALIGN_RIGHT,
        JWPLC_UIColors colorsValue = JWPLC_UIColors())
        : labelTextSize(labelSizeValue),
          valueTextSize(valueSizeValue),
          frame(frameValue),
          layout(layoutValue),
          valueAlign(alignValue),
          colors(colorsValue)
    {
    }
};

// signedValue=true permite negativos y reserva espacio para '-'.
struct JWPLC_UIValueFormat
{
    uint8_t integerDigits;
    uint8_t decimalDigits;
    bool signedValue;
    bool leadingZeros;

    JWPLC_UIValueFormat(
        uint8_t integerDigitsValue = 3,
        uint8_t decimalDigitsValue = 0,
        bool signedValueValue = false,
        bool leadingZerosValue = false)
        : integerDigits(integerDigitsValue),
          decimalDigits(decimalDigitsValue),
          signedValue(signedValueValue),
          leadingZeros(leadingZerosValue)
    {
    }
};

struct JWPLC_UIBoolText
{
    const char *falseText;
    const char *trueText;

    JWPLC_UIBoolText(
        const char *falseTextValue = "OFF",
        const char *trueTextValue = "ON")
        : falseText(falseTextValue), trueText(trueTextValue)
    {
    }
};

struct JWPLC_UIRange
{
    float min;
    float max;

    JWPLC_UIRange(float minValue = 0.0f, float maxValue = 100.0f)
        : min(minValue), max(maxValue)
    {
    }
};

struct JWPLC_UIFieldOptions
{
    uint8_t textCapacity;
    JWPLC_UIBoolText boolText;
    JWPLC_UIRange barRange;

    JWPLC_UIFieldOptions(
        uint8_t textCapacityValue = 12,
        JWPLC_UIBoolText boolTextValue = JWPLC_UIBoolText(),
        JWPLC_UIRange barRangeValue = JWPLC_UIRange())
        : textCapacity(textCapacityValue),
          boolText(boolTextValue),
          barRange(barRangeValue)
    {
    }
};

// Definicion HMI agrupada. En inicializacion directa queda una fila por grupo:
// { meta }, { rect }, { text }, { style }, { colors }, { format }, { options }.
struct JWPLC_UIField
{
    JWPLC_UIFieldMeta meta;
    JWPLC_UIRect rect;
    JWPLC_UIText text;
    JWPLC_UIStyle style;
    JWPLC_UIColors colors;
    JWPLC_UIValueFormat format;
    JWPLC_UIFieldOptions options;

    JWPLC_UIField(
        JWPLC_UIFieldMeta metaValue = JWPLC_UIFieldMeta(),
        JWPLC_UIRect rectValue = JWPLC_UIRect(),
        JWPLC_UIText textValue = JWPLC_UIText(),
        JWPLC_UIStyle styleValue = JWPLC_UIStyle(),
        JWPLC_UIColors colorsValue = JWPLC_UIColors(),
        JWPLC_UIValueFormat formatValue = JWPLC_UIValueFormat(),
        JWPLC_UIFieldOptions optionsValue = JWPLC_UIFieldOptions())
        : meta(metaValue),
          rect(rectValue),
          text(textValue),
          style(styleValue),
          colors(colorsValue),
          format(formatValue),
          options(optionsValue)
    {
        // El motor actual lee style.colors. Se sincroniza una sola vez al
        // registrar la definicion; el API publico mantiene colors separado.
        style.colors = colors;
    }
};

inline JWPLC_UIStyle JWPLC_UIValueStyle(
    uint8_t valueTextSize = 2,
    uint8_t labelTextSize = 1,
    bool frame = false,
    JWPLC_UIFieldLayout layout = JWPLC_UI_LAYOUT_INLINE,
    JWPLC_UIValueAlign align = JWPLC_UI_ALIGN_RIGHT)
{
    return JWPLC_UIStyle(
        labelTextSize,
        valueTextSize,
        frame,
        layout,
        align);
}

inline JWPLC_UIStyle JWPLC_UITextFieldStyle(
    uint8_t valueTextSize = 1,
    uint8_t labelTextSize = 1,
    bool frame = false,
    JWPLC_UIFieldLayout layout = JWPLC_UI_LAYOUT_INLINE,
    JWPLC_UIValueAlign align = JWPLC_UI_ALIGN_LEFT)
{
    return JWPLC_UIValueStyle(
        valueTextSize,
        labelTextSize,
        frame,
        layout,
        align);
}

inline JWPLC_UIStyle JWPLC_UIBoolStyle(
    uint8_t valueTextSize = 2,
    uint8_t labelTextSize = 1,
    bool frame = false,
    JWPLC_UIFieldLayout layout = JWPLC_UI_LAYOUT_INLINE,
    JWPLC_UIValueAlign align = JWPLC_UI_ALIGN_CENTER)
{
    return JWPLC_UIValueStyle(
        valueTextSize,
        labelTextSize,
        frame,
        layout,
        align);
}

inline JWPLC_UIStyle JWPLC_UIBarStyle(
    uint8_t labelTextSize = 1,
    bool frame = false,
    JWPLC_UIFieldLayout layout = JWPLC_UI_LAYOUT_STACKED)
{
    return JWPLC_UIValueStyle(
        1,
        labelTextSize,
        frame,
        layout,
        JWPLC_UI_ALIGN_LEFT);
}

// ---------------------------------------------------------------------
// Helpers agrupados: control completo sin llenar el struct miembro a miembro.
// colors queda separado de style para que cada grupo sea una sola fila clara.
// ---------------------------------------------------------------------
inline JWPLC_UIField JWPLC_UIValueField(
    uint8_t id,
    JWPLC_UIRect rect,
    JWPLC_UIText text = JWPLC_UIText(),
    JWPLC_UIValueFormat format = JWPLC_UIValueFormat(),
    JWPLC_UIStyle style = JWPLC_UIValueStyle(),
    uint8_t page = 0,
    JWPLC_UIColors colors = JWPLC_UIColors())
{
    return JWPLC_UIField(
        JWPLC_UIFieldMeta(id, page, JWPLC_UI_FIELD_VALUE),
        rect,
        text,
        style,
        colors,
        format,
        JWPLC_UIFieldOptions());
}

inline JWPLC_UIField JWPLC_UITextField(
    uint8_t id,
    JWPLC_UIRect rect,
    JWPLC_UIText text = JWPLC_UIText(),
    uint8_t textCapacity = 12,
    JWPLC_UIStyle style = JWPLC_UITextFieldStyle(),
    uint8_t page = 0,
    JWPLC_UIColors colors = JWPLC_UIColors())
{
    return JWPLC_UIField(
        JWPLC_UIFieldMeta(id, page, JWPLC_UI_FIELD_TEXT),
        rect,
        text,
        style,
        colors,
        JWPLC_UIValueFormat(),
        JWPLC_UIFieldOptions(textCapacity));
}

inline JWPLC_UIField JWPLC_UIBoolField(
    uint8_t id,
    JWPLC_UIRect rect,
    JWPLC_UIText text = JWPLC_UIText(),
    JWPLC_UIBoolText boolText = JWPLC_UIBoolText(),
    JWPLC_UIStyle style = JWPLC_UIBoolStyle(),
    uint8_t page = 0,
    JWPLC_UIColors colors = JWPLC_UIColors())
{
    return JWPLC_UIField(
        JWPLC_UIFieldMeta(id, page, JWPLC_UI_FIELD_BOOL),
        rect,
        text,
        style,
        colors,
        JWPLC_UIValueFormat(),
        JWPLC_UIFieldOptions(12, boolText));
}

inline JWPLC_UIField JWPLC_UIBarField(
    uint8_t id,
    JWPLC_UIRect rect,
    JWPLC_UIText text = JWPLC_UIText(),
    JWPLC_UIRange range = JWPLC_UIRange(),
    JWPLC_UIStyle style = JWPLC_UIBarStyle(),
    uint8_t page = 0,
    JWPLC_UIColors colors = JWPLC_UIColors())
{
    return JWPLC_UIField(
        JWPLC_UIFieldMeta(id, page, JWPLC_UI_FIELD_BAR),
        rect,
        text,
        style,
        colors,
        JWPLC_UIValueFormat(),
        JWPLC_UIFieldOptions(12, JWPLC_UIBoolText(), range));
}

// ---------------------------------------------------------------------
// Helpers cortos para el caso comun.
// ---------------------------------------------------------------------
inline JWPLC_UIField JWPLC_UIValueField(
    uint8_t id,
    int16_t x,
    int16_t y,
    const char *label = nullptr,
    const char *unit = nullptr,
    JWPLC_UIValueFormat format = JWPLC_UIValueFormat(),
    uint8_t page = 0)
{
    return JWPLC_UIValueField(
        id,
        JWPLC_UIRect(x, y),
        JWPLC_UIText(label, unit),
        format,
        JWPLC_UIValueStyle(),
        page);
}

inline JWPLC_UIField JWPLC_UITextField(
    uint8_t id,
    int16_t x,
    int16_t y,
    const char *label = nullptr,
    uint8_t textCapacity = 12,
    uint8_t page = 0)
{
    return JWPLC_UITextField(
        id,
        JWPLC_UIRect(x, y),
        JWPLC_UIText(label),
        textCapacity,
        JWPLC_UITextFieldStyle(),
        page);
}

inline JWPLC_UIField JWPLC_UIBoolField(
    uint8_t id,
    int16_t x,
    int16_t y,
    const char *label = nullptr,
    JWPLC_UIBoolText boolText = JWPLC_UIBoolText(),
    uint8_t page = 0)
{
    return JWPLC_UIBoolField(
        id,
        JWPLC_UIRect(x, y),
        JWPLC_UIText(label),
        boolText,
        JWPLC_UIBoolStyle(),
        page);
}

inline JWPLC_UIField JWPLC_UIBarField(
    uint8_t id,
    int16_t x,
    int16_t y,
    const char *label = nullptr,
    JWPLC_UIRange range = JWPLC_UIRange(),
    int16_t width = 100,
    int16_t height = 28,
    uint8_t page = 0)
{
    return JWPLC_UIBarField(
        id,
        JWPLC_UIRect(x, y, width, height),
        JWPLC_UIText(label),
        range,
        JWPLC_UIBarStyle(),
        page);
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
