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

// Identidad logica del campo.
struct JWPLC_UIFieldMeta
{
    uint8_t id = 0;
    uint8_t page = 0;
    JWPLC_UIFieldType type = JWPLC_UI_FIELD_VALUE;
};

// Geometria del campo. width/height pueden usar JWPLC_UI_AUTO.
struct JWPLC_UIRect
{
    int16_t x = 0;
    int16_t y = 0;
    int16_t width = JWPLC_UI_AUTO;
    int16_t height = JWPLC_UI_AUTO;
};

// Texto estatico asociado al campo.
struct JWPLC_UIText
{
    const char *label = nullptr;
    const char *unit = nullptr;
};

// Paleta del campo.
struct JWPLC_UIColors
{
    uint16_t label = 0xFFFF;
    uint16_t value = 0xFFFF;
    uint16_t background = 0x0000;
    uint16_t frame = 0xFFFF;
};

// Apariencia comun. El valor puede alinearse dentro de su region reservada.
struct JWPLC_UIStyle
{
    uint8_t labelTextSize = 1;
    uint8_t valueTextSize = 2;
    bool frame = false;
    JWPLC_UIFieldLayout layout = JWPLC_UI_LAYOUT_INLINE;
    JWPLC_UIValueAlign valueAlign = JWPLC_UI_ALIGN_RIGHT;
    JWPLC_UIColors colors = {};
};

// Formato numerico y capacidad visual.
// signedValue=true reserva espacio para '-' y permite valores negativos.
struct JWPLC_UIValueFormat
{
    uint8_t integerDigits = 3;
    uint8_t decimalDigits = 0;
    bool signedValue = false;
    bool leadingZeros = false;
};

struct JWPLC_UIBoolText
{
    const char *falseText = "OFF";
    const char *trueText = "ON";
};

struct JWPLC_UIRange
{
    float min = 0.0f;
    float max = 100.0f;
};

// Opciones especificas por tipo. Los helpers rellenan solo lo necesario.
struct JWPLC_UIFieldOptions
{
    uint8_t textCapacity = 12;
    JWPLC_UIBoolText boolText = {};
    JWPLC_UIRange barRange = {};
};

// Definicion HMI agrupada: una linea por grupo en inicializadores directos.
struct JWPLC_UIField
{
    JWPLC_UIFieldMeta meta = {};
    JWPLC_UIRect rect = {};
    JWPLC_UIText text = {};
    JWPLC_UIStyle style = {};
    JWPLC_UIValueFormat format = {};
    JWPLC_UIFieldOptions options = {};
};

inline JWPLC_UIStyle JWPLC_UIValueStyle(
    uint8_t valueTextSize = 2,
    uint8_t labelTextSize = 1,
    bool frame = false,
    JWPLC_UIFieldLayout layout = JWPLC_UI_LAYOUT_INLINE,
    JWPLC_UIValueAlign align = JWPLC_UI_ALIGN_RIGHT,
    JWPLC_UIColors colors = {})
{
    JWPLC_UIStyle style;
    style.labelTextSize = labelTextSize;
    style.valueTextSize = valueTextSize;
    style.frame = frame;
    style.layout = layout;
    style.valueAlign = align;
    style.colors = colors;
    return style;
}

inline JWPLC_UIStyle JWPLC_UITextFieldStyle(
    uint8_t valueTextSize = 1,
    uint8_t labelTextSize = 1,
    bool frame = false,
    JWPLC_UIFieldLayout layout = JWPLC_UI_LAYOUT_INLINE,
    JWPLC_UIValueAlign align = JWPLC_UI_ALIGN_LEFT,
    JWPLC_UIColors colors = {})
{
    return JWPLC_UIValueStyle(
        valueTextSize,
        labelTextSize,
        frame,
        layout,
        align,
        colors);
}

inline JWPLC_UIStyle JWPLC_UIBoolStyle(
    uint8_t valueTextSize = 2,
    uint8_t labelTextSize = 1,
    bool frame = false,
    JWPLC_UIFieldLayout layout = JWPLC_UI_LAYOUT_INLINE,
    JWPLC_UIValueAlign align = JWPLC_UI_ALIGN_CENTER,
    JWPLC_UIColors colors = {})
{
    return JWPLC_UIValueStyle(
        valueTextSize,
        labelTextSize,
        frame,
        layout,
        align,
        colors);
}

inline JWPLC_UIStyle JWPLC_UIBarStyle(
    uint8_t labelTextSize = 1,
    bool frame = false,
    JWPLC_UIFieldLayout layout = JWPLC_UI_LAYOUT_STACKED,
    JWPLC_UIColors colors = {})
{
    return JWPLC_UIValueStyle(
        1,
        labelTextSize,
        frame,
        layout,
        JWPLC_UI_ALIGN_LEFT,
        colors);
}

// ---------------------------------------------------------------------
// Helpers agrupados: utiles cuando se quiere controlar rect/style.
// ---------------------------------------------------------------------

inline JWPLC_UIField JWPLC_UIValueField(
    uint8_t id,
    JWPLC_UIRect rect,
    JWPLC_UIText text = {},
    JWPLC_UIValueFormat format = {},
    JWPLC_UIStyle style = JWPLC_UIValueStyle(),
    uint8_t page = 0)
{
    JWPLC_UIField field;
    field.meta = {id, page, JWPLC_UI_FIELD_VALUE};
    field.rect = rect;
    field.text = text;
    field.style = style;
    field.format = format;
    return field;
}

inline JWPLC_UIField JWPLC_UITextField(
    uint8_t id,
    JWPLC_UIRect rect,
    JWPLC_UIText text = {},
    uint8_t textCapacity = 12,
    JWPLC_UIStyle style = JWPLC_UITextFieldStyle(),
    uint8_t page = 0)
{
    JWPLC_UIField field;
    field.meta = {id, page, JWPLC_UI_FIELD_TEXT};
    field.rect = rect;
    field.text = text;
    field.style = style;
    field.options.textCapacity = textCapacity;
    return field;
}

inline JWPLC_UIField JWPLC_UIBoolField(
    uint8_t id,
    JWPLC_UIRect rect,
    JWPLC_UIText text = {},
    JWPLC_UIBoolText boolText = {},
    JWPLC_UIStyle style = JWPLC_UIBoolStyle(),
    uint8_t page = 0)
{
    JWPLC_UIField field;
    field.meta = {id, page, JWPLC_UI_FIELD_BOOL};
    field.rect = rect;
    field.text = text;
    field.style = style;
    field.options.boolText = boolText;
    return field;
}

inline JWPLC_UIField JWPLC_UIBarField(
    uint8_t id,
    JWPLC_UIRect rect,
    JWPLC_UIText text = {},
    JWPLC_UIRange range = {},
    JWPLC_UIStyle style = JWPLC_UIBarStyle(),
    uint8_t page = 0)
{
    JWPLC_UIField field;
    field.meta = {id, page, JWPLC_UI_FIELD_BAR};
    field.rect = rect;
    field.text = text;
    field.style = style;
    field.options.barRange = range;
    return field;
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
    JWPLC_UIValueFormat format = {},
    uint8_t page = 0)
{
    return JWPLC_UIValueField(
        id,
        {x, y, JWPLC_UI_AUTO, JWPLC_UI_AUTO},
        {label, unit},
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
        {x, y, JWPLC_UI_AUTO, JWPLC_UI_AUTO},
        {label, nullptr},
        textCapacity,
        JWPLC_UITextFieldStyle(),
        page);
}

inline JWPLC_UIField JWPLC_UIBoolField(
    uint8_t id,
    int16_t x,
    int16_t y,
    const char *label = nullptr,
    JWPLC_UIBoolText boolText = {},
    uint8_t page = 0)
{
    return JWPLC_UIBoolField(
        id,
        {x, y, JWPLC_UI_AUTO, JWPLC_UI_AUTO},
        {label, nullptr},
        boolText,
        JWPLC_UIBoolStyle(),
        page);
}

inline JWPLC_UIField JWPLC_UIBarField(
    uint8_t id,
    int16_t x,
    int16_t y,
    const char *label = nullptr,
    JWPLC_UIRange range = {},
    int16_t width = 100,
    int16_t height = 28,
    uint8_t page = 0)
{
    return JWPLC_UIBarField(
        id,
        {x, y, width, height},
        {label, nullptr},
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
