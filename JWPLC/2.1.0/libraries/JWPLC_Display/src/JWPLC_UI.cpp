#include "JWPLC_UI.h"
#include "JWPLC_Display_API.h"

#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>

#include <cmath>
#include <cstdio>
#include <cstring>

extern "C"
{
#include "jwplc_peripherals.h"
}

namespace
{
    static constexpr uint8_t VALUE_BUFFER_SIZE = 40;
    static constexpr int16_t FIELD_PADDING = 3;
    static constexpr int16_t FIELD_GAP = 4;
    static constexpr int16_t DEFAULT_BAR_WIDTH = 80;
    static constexpr int16_t DEFAULT_BAR_HEIGHT = 12;

    struct FieldRuntime
    {
        JWPLC_UIField def;
        bool used;
        bool dirty;
        bool hasValue;

        char value[VALUE_BUFFER_SIZE];
        float barValue;

        int16_t fieldX;
        int16_t fieldY;
        int16_t fieldW;
        int16_t fieldH;

        int16_t valueX;
        int16_t valueY;
        int16_t valueW;
        int16_t valueH;

        FieldRuntime()
            : def(),
              used(false),
              dirty(true),
              hasValue(false),
              barValue(0.0f),
              fieldX(0),
              fieldY(0),
              fieldW(0),
              fieldH(0),
              valueX(0),
              valueY(0),
              valueW(0),
              valueH(0)
        {
            memset(value, 0, sizeof(value));
        }
    };

    FieldRuntime g_fields[JWPLC_UI_MAX_FIELDS];
    size_t g_fieldCount = 0;

    JWPLC_UIRefreshMode g_refreshMode = USER_REFRESH_ON_DEMAND;
    bool g_refreshRequested = false;

    uint8_t g_page = 0;
    bool g_pageRedrawPending = true;

    bool userModeVisible()
    {
        return !JWPLC_Display.isIdleMode();
    }

    FieldRuntime *findField(uint8_t id)
    {
        for (size_t i = 0; i < g_fieldCount; ++i)
        {
            if (g_fields[i].used && g_fields[i].def.meta.id == id)
            {
                return &g_fields[i];
            }
        }

        return nullptr;
    }

    bool anyDirtyOnCurrentPage()
    {
        for (size_t i = 0; i < g_fieldCount; ++i)
        {
            if (g_fields[i].used &&
                g_fields[i].def.meta.page == g_page &&
                g_fields[i].dirty)
            {
                return true;
            }
        }

        return false;
    }

    void requestCurrentViewRefresh()
    {
        g_refreshRequested = true;

        // Si USER no esta visible, conservar dirty/cache sin tocar la TFT.
        if (userModeVisible())
        {
            jwplcSystemForceDisplayRefresh();
        }
    }

    void markFieldDirty(FieldRuntime &field)
    {
        field.dirty = true;

        if (field.def.meta.page == g_page)
        {
            requestCurrentViewRefresh();
        }
    }

    void textBounds(
        Adafruit_ST7789 &tft,
        const char *text,
        uint8_t textSize,
        uint16_t &w,
        uint16_t &h)
    {
        w = 0;
        h = 0;

        if (text == nullptr || text[0] == '\0')
        {
            return;
        }

        int16_t x1 = 0;
        int16_t y1 = 0;

        const uint8_t scale = (textSize == 0) ? 1 : textSize;
        tft.setTextSize(scale);
        tft.getTextBounds(text, 0, 0, &x1, &y1, &w, &h);

        // HMI declarativa: Adafruit GFX reporta la celda clasica 6x8 completa.
        // Para el layout del field usamos el cuerpo nominal 5x7 y dejamos el
        // spacer final / fila de descenso dentro del padding del contenedor.
        if (w >= scale)
        {
            w -= scale;
        }

        if (h >= scale)
        {
            h -= scale;
        }
    }

    int16_t effectiveFieldPadding(const JWPLC_UIField &def)
    {
        uint8_t maxTextSize = (def.style.labelTextSize == 0)
                                  ? 1
                                  : def.style.labelTextSize;

        if (def.meta.type != JWPLC_UI_FIELD_BAR)
        {
            const uint8_t valueSize = (def.style.valueTextSize == 0)
                                          ? 1
                                          : def.style.valueTextSize;
            if (valueSize > maxTextSize)
            {
                maxTextSize = valueSize;
            }
        }

        // FIELD_PADDING sigue siendo el margen visual minimo. Para escalas
        // mayores reservamos al menos una celda de escala para contener la
        // posible fila 8 (descenders) sin salir del rectangulo del field.
        return (maxTextSize > FIELD_PADDING)
                   ? (int16_t)maxTextSize
                   : FIELD_PADDING;
    }

    void makeNumericSample(const JWPLC_UIField &def, char *out, size_t outSize)
    {
        if (out == nullptr || outSize == 0)
        {
            return;
        }

        const uint8_t integerDigits =
            (def.format.integerDigits == 0) ? 1 : def.format.integerDigits;

        size_t pos = 0;

        if (def.format.signedValue && pos + 1 < outSize)
        {
            out[pos++] = '-';
        }

        for (uint8_t i = 0; i < integerDigits && pos + 1 < outSize; ++i)
        {
            out[pos++] = '8';
        }

        if (def.format.decimalDigits > 0 && pos + 1 < outSize)
        {
            out[pos++] = '.';

            for (uint8_t i = 0;
                 i < def.format.decimalDigits && pos + 1 < outSize;
                 ++i)
            {
                out[pos++] = '8';
            }
        }

        out[pos] = '\0';
    }

    void makeTextSample(uint8_t capacity, char *out, size_t outSize)
    {
        if (out == nullptr || outSize == 0)
        {
            return;
        }

        const uint8_t count = (capacity == 0) ? 1 : capacity;
        size_t pos = 0;

        for (uint8_t i = 0; i < count && pos + 1 < outSize; ++i)
        {
            out[pos++] = 'W';
        }

        out[pos] = '\0';
    }

    void makeOverflowText(const JWPLC_UIField &def, char *out, size_t outSize)
    {
        if (out == nullptr || outSize == 0)
        {
            return;
        }

        size_t slots =
            (def.format.integerDigits == 0) ? 1 : def.format.integerDigits;

        if (def.format.decimalDigits > 0)
        {
            slots += 1 + def.format.decimalDigits;
        }

        if (def.format.signedValue)
        {
            slots += 1;
        }

        if (slots > outSize - 1)
        {
            slots = outSize - 1;
        }

        for (size_t i = 0; i < slots; ++i)
        {
            out[i] = '#';
        }

        out[slots] = '\0';
    }

    bool formatNumeric(
        const JWPLC_UIField &def,
        double value,
        char *out,
        size_t outSize)
    {
        if (out == nullptr || outSize == 0)
        {
            return false;
        }

        if (!std::isfinite(value))
        {
            makeOverflowText(def, out, outSize);
            return false;
        }

        const bool negative = value < 0.0;

        if (negative && !def.format.signedValue)
        {
            makeOverflowText(def, out, outSize);
            return false;
        }

        const uint8_t decimals = def.format.decimalDigits;

        if (def.format.leadingZeros)
        {
            const uint8_t integerDigits =
                (def.format.integerDigits == 0) ? 1 : def.format.integerDigits;

            const int magnitudeWidth =
                integerDigits + ((decimals > 0) ? (1 + decimals) : 0);

            char magnitude[VALUE_BUFFER_SIZE] = {};
            snprintf(
                magnitude,
                sizeof(magnitude),
                "%0*.*f",
                magnitudeWidth,
                decimals,
                std::fabs(value));

            if (negative)
            {
                snprintf(out, outSize, "-%s", magnitude);
            }
            else
            {
                strncpy(out, magnitude, outSize - 1);
                out[outSize - 1] = '\0';
            }
        }
        else
        {
            snprintf(out, outSize, "%.*f", decimals, value);
        }

        const char *p = out;

        if (*p == '-' || *p == '+')
        {
            ++p;
        }

        uint8_t integerCount = 0;

        while (*p != '\0' && *p != '.')
        {
            if (*p >= '0' && *p <= '9')
            {
                ++integerCount;
            }

            ++p;
        }

        const uint8_t allowed =
            (def.format.integerDigits == 0) ? 1 : def.format.integerDigits;

        if (integerCount > allowed)
        {
            makeOverflowText(def, out, outSize);
            return false;
        }

        return true;
    }

    bool setValueString(FieldRuntime &field, const char *value)
    {
        if (value == nullptr)
        {
            value = "";
        }

        if (field.hasValue &&
            strncmp(field.value, value, sizeof(field.value)) == 0)
        {
            return false;
        }

        strncpy(field.value, value, sizeof(field.value) - 1);
        field.value[sizeof(field.value) - 1] = '\0';

        field.hasValue = true;
        markFieldDirty(field);
        return true;
    }

    int16_t alignedValueX(
        Adafruit_ST7789 &tft,
        const FieldRuntime &field)
    {
        if (field.def.meta.type == JWPLC_UI_FIELD_BAR)
        {
            return field.valueX;
        }

        uint16_t currentW = 0;
        uint16_t currentH = 0;
        textBounds(
            tft,
            field.value,
            field.def.style.valueTextSize,
            currentW,
            currentH);

        if ((int16_t)currentW >= field.valueW)
        {
            return field.valueX;
        }

        const int16_t freeW = field.valueW - (int16_t)currentW;

        switch (field.def.style.valueAlign)
        {
        case JWPLC_UI_ALIGN_CENTER:
            return field.valueX + freeW / 2;

        case JWPLC_UI_ALIGN_RIGHT:
            return field.valueX + freeW;

        case JWPLC_UI_ALIGN_LEFT:
        default:
            return field.valueX;
        }
    }

    void computeFieldGeometry(Adafruit_ST7789 &tft, FieldRuntime &field)
    {
        const JWPLC_UIField &def = field.def;
        const int16_t pad = effectiveFieldPadding(def);

        uint16_t labelW = 0;
        uint16_t labelH = 0;
        textBounds(
            tft,
            def.text.label,
            def.style.labelTextSize,
            labelW,
            labelH);

        uint16_t unitW = 0;
        uint16_t unitH = 0;
        textBounds(
            tft,
            def.text.unit,
            def.style.labelTextSize,
            unitW,
            unitH);

        uint16_t valueW = 0;
        uint16_t valueH = 0;
        char sample[VALUE_BUFFER_SIZE] = {};

        switch (def.meta.type)
        {
        case JWPLC_UI_FIELD_VALUE:
            makeNumericSample(def, sample, sizeof(sample));
            textBounds(
                tft,
                sample,
                def.style.valueTextSize,
                valueW,
                valueH);
            break;

        case JWPLC_UI_FIELD_TEXT:
            makeTextSample(
                def.options.textCapacity,
                sample,
                sizeof(sample));
            textBounds(
                tft,
                sample,
                def.style.valueTextSize,
                valueW,
                valueH);
            break;

        case JWPLC_UI_FIELD_BOOL:
        {
            uint16_t falseW = 0;
            uint16_t falseH = 0;
            uint16_t trueW = 0;
            uint16_t trueH = 0;

            textBounds(
                tft,
                (def.options.boolText.falseText != nullptr)
                    ? def.options.boolText.falseText
                    : "OFF",
                def.style.valueTextSize,
                falseW,
                falseH);

            textBounds(
                tft,
                (def.options.boolText.trueText != nullptr)
                    ? def.options.boolText.trueText
                    : "ON",
                def.style.valueTextSize,
                trueW,
                trueH);

            valueW = (falseW > trueW) ? falseW : trueW;
            valueH = (falseH > trueH) ? falseH : trueH;
            break;
        }

        case JWPLC_UI_FIELD_BAR:
        default:
            valueW =
                (def.rect.width == JWPLC_UI_AUTO)
                    ? DEFAULT_BAR_WIDTH
                    : (uint16_t)((def.rect.width > 2 * pad)
                                     ? def.rect.width - 2 * pad
                                     : DEFAULT_BAR_WIDTH);
            valueH = DEFAULT_BAR_HEIGHT;
            break;
        }

        if (def.meta.type == JWPLC_UI_FIELD_BAR &&
            def.rect.width != JWPLC_UI_AUTO)
        {
            int16_t available = def.rect.width - 2 * pad;

            if (def.style.layout == JWPLC_UI_LAYOUT_INLINE && labelW > 0)
            {
                available -= (int16_t)labelW + FIELD_GAP;
            }

            if (unitW > 0)
            {
                available -= (int16_t)unitW + FIELD_GAP;
            }

            if (available < 1)
            {
                available = 1;
            }

            valueW = (uint16_t)available;
        }

        int16_t autoW = 0;
        int16_t autoH = 0;

        if (def.style.layout == JWPLC_UI_LAYOUT_STACKED)
        {
            const int16_t valueAndUnitW =
                (int16_t)valueW +
                ((unitW > 0) ? FIELD_GAP + (int16_t)unitW : 0);

            autoW =
                2 * pad +
                (((int16_t)labelW > valueAndUnitW)
                     ? (int16_t)labelW
                     : valueAndUnitW);

            autoH =
                2 * pad +
                (int16_t)labelH +
                ((labelH > 0) ? FIELD_GAP : 0) +
                (int16_t)((valueH > unitH) ? valueH : unitH);

            field.valueX = def.rect.x + pad;
            field.valueY =
                def.rect.y + pad +
                (int16_t)labelH +
                ((labelH > 0) ? FIELD_GAP : 0);
        }
        else
        {
            autoW =
                2 * pad +
                (int16_t)labelW +
                ((labelW > 0) ? FIELD_GAP : 0) +
                (int16_t)valueW +
                ((unitW > 0) ? FIELD_GAP + (int16_t)unitW : 0);

            autoH =
                2 * pad +
                (int16_t)((labelH > valueH)
                              ? ((labelH > unitH) ? labelH : unitH)
                              : ((valueH > unitH) ? valueH : unitH));

            field.valueX =
                def.rect.x + pad +
                (int16_t)labelW +
                ((labelW > 0) ? FIELD_GAP : 0);

            field.valueY = def.rect.y + pad;
        }

        field.fieldX = def.rect.x;
        field.fieldY = def.rect.y;
        field.fieldW =
            (def.rect.width == JWPLC_UI_AUTO) ? autoW : def.rect.width;
        field.fieldH =
            (def.rect.height == JWPLC_UI_AUTO) ? autoH : def.rect.height;

        field.valueW = (int16_t)valueW;
        field.valueH =
            (def.meta.type == JWPLC_UI_FIELD_BAR)
                ? (int16_t)valueH
                : (int16_t)((valueH == 0)
                                 ? (7 * ((def.style.valueTextSize == 0)
                                            ? 1
                                            : def.style.valueTextSize))
                                 : valueH);
    }

    void drawFieldStatic(Adafruit_ST7789 &tft, FieldRuntime &field)
    {
        computeFieldGeometry(tft, field);

        const JWPLC_UIField &def = field.def;
        const int16_t pad = effectiveFieldPadding(def);

        if (field.fieldW > 0 && field.fieldH > 0)
        {
            tft.fillRect(
                field.fieldX,
                field.fieldY,
                field.fieldW,
                field.fieldH,
                def.style.colors.background);
        }

        if (def.style.frame && field.fieldW > 1 && field.fieldH > 1)
        {
            tft.drawRect(
                field.fieldX,
                field.fieldY,
                field.fieldW,
                field.fieldH,
                def.style.colors.frame);
        }

        if (def.text.label != nullptr && def.text.label[0] != '\0')
        {
            tft.setTextSize(
                (def.style.labelTextSize == 0)
                    ? 1
                    : def.style.labelTextSize);
            tft.setTextColor(
                def.style.colors.label,
                def.style.colors.background);
            tft.setCursor(
                def.rect.x + pad,
                def.rect.y + pad);
            tft.print(def.text.label);
        }

        if (def.text.unit != nullptr && def.text.unit[0] != '\0')
        {
            tft.setTextSize(
                (def.style.labelTextSize == 0)
                    ? 1
                    : def.style.labelTextSize);
            tft.setTextColor(
                def.style.colors.label,
                def.style.colors.background);
            tft.setCursor(
                field.valueX + field.valueW + FIELD_GAP,
                field.valueY);
            tft.print(def.text.unit);
        }

        field.dirty = true;
    }

    void drawFieldValue(Adafruit_ST7789 &tft, FieldRuntime &field)
    {
        const JWPLC_UIField &def = field.def;

        if (field.valueW <= 0 || field.valueH <= 0)
        {
            field.dirty = false;
            return;
        }

        // Al cambiar una variable solo se limpia su region VALUE. El layout
        // usa el cuerpo nominal 5x7, pero la fuente clasica puede usar la fila
        // 8 (descenders), por eso la limpieza vertical cubre una escala extra.
        int16_t clearH = field.valueH;

        if (def.meta.type != JWPLC_UI_FIELD_BAR)
        {
            const int16_t scale = (def.style.valueTextSize == 0)
                                      ? 1
                                      : (int16_t)def.style.valueTextSize;
            clearH += scale;

            const int16_t maxClearH =
                (field.fieldY + field.fieldH) - field.valueY;
            if (clearH > maxClearH)
            {
                clearH = maxClearH;
            }
        }

        tft.fillRect(
            field.valueX,
            field.valueY,
            field.valueW,
            clearH,
            def.style.colors.background);

        if (!field.hasValue)
        {
            field.dirty = false;
            return;
        }

        if (def.meta.type == JWPLC_UI_FIELD_BAR)
        {
            float minValue = def.options.barRange.min;
            float maxValue = def.options.barRange.max;

            if (maxValue <= minValue)
            {
                maxValue = minValue + 1.0f;
            }

            float normalized =
                (field.barValue - minValue) / (maxValue - minValue);

            if (normalized < 0.0f)
            {
                normalized = 0.0f;
            }
            else if (normalized > 1.0f)
            {
                normalized = 1.0f;
            }

            const int16_t fillW =
                (int16_t)lroundf(normalized * field.valueW);

            if (fillW > 0)
            {
                tft.fillRect(
                    field.valueX,
                    field.valueY,
                    fillW,
                    field.valueH,
                    def.style.colors.value);
            }

            field.dirty = false;
            return;
        }

        tft.setTextSize(
            (def.style.valueTextSize == 0)
                ? 1
                : def.style.valueTextSize);
        tft.setTextColor(
            def.style.colors.value,
            def.style.colors.background);
        tft.setCursor(alignedValueX(tft, field), field.valueY);
        tft.print(field.value);

        field.dirty = false;
    }
}

namespace JWPLCUI
{
    bool setFields(const JWPLC_UIField *fields, size_t count)
    {
        if ((fields == nullptr && count != 0) ||
            count > JWPLC_UI_MAX_FIELDS)
        {
            return false;
        }

        g_fieldCount = 0;

        for (size_t i = 0; i < JWPLC_UI_MAX_FIELDS; ++i)
        {
            g_fields[i] = FieldRuntime();
        }

        for (size_t i = 0; i < count; ++i)
        {
            for (size_t j = 0; j < i; ++j)
            {
                if (fields[i].meta.id == fields[j].meta.id)
                {
                    return false;
                }
            }

            g_fields[i].def = fields[i];
            g_fields[i].used = true;
            g_fields[i].dirty = true;
            g_fieldCount++;
        }

        g_pageRedrawPending = true;
        requestCurrentViewRefresh();
        return true;
    }

    void clearFields()
    {
        g_fieldCount = 0;

        for (size_t i = 0; i < JWPLC_UI_MAX_FIELDS; ++i)
        {
            g_fields[i] = FieldRuntime();
        }

        g_pageRedrawPending = true;
        requestCurrentViewRefresh();
    }

    size_t fieldCount()
    {
        return g_fieldCount;
    }

    bool setNumericValue(uint8_t fieldId, double value)
    {
        FieldRuntime *field = findField(fieldId);

        if (field == nullptr ||
            field->def.meta.type != JWPLC_UI_FIELD_VALUE)
        {
            return false;
        }

        char formatted[VALUE_BUFFER_SIZE] = {};
        const bool valid =
            formatNumeric(
                field->def,
                value,
                formatted,
                sizeof(formatted));

        setValueString(*field, formatted);
        return valid;
    }

    bool setText(uint8_t fieldId, const char *value)
    {
        FieldRuntime *field = findField(fieldId);

        if (field == nullptr ||
            field->def.meta.type != JWPLC_UI_FIELD_TEXT)
        {
            return false;
        }

        if (value == nullptr)
        {
            value = "";
        }

        char clipped[VALUE_BUFFER_SIZE] = {};

        const uint8_t capacity =
            (field->def.options.textCapacity == 0)
                ? (VALUE_BUFFER_SIZE - 1)
                : field->def.options.textCapacity;

        const size_t allowed =
            (capacity < VALUE_BUFFER_SIZE - 1)
                ? capacity
                : VALUE_BUFFER_SIZE - 1;

        strncpy(clipped, value, allowed);
        clipped[allowed] = '\0';

        const bool complete = strlen(value) <= allowed;
        setValueString(*field, clipped);
        return complete;
    }

    bool setBool(uint8_t fieldId, bool value)
    {
        FieldRuntime *field = findField(fieldId);

        if (field == nullptr ||
            field->def.meta.type != JWPLC_UI_FIELD_BOOL)
        {
            return false;
        }

        const char *text =
            value
                ? ((field->def.options.boolText.trueText != nullptr)
                       ? field->def.options.boolText.trueText
                       : "ON")
                : ((field->def.options.boolText.falseText != nullptr)
                       ? field->def.options.boolText.falseText
                       : "OFF");

        setValueString(*field, text);
        return true;
    }

    bool setBar(uint8_t fieldId, float value)
    {
        FieldRuntime *field = findField(fieldId);

        if (field == nullptr ||
            field->def.meta.type != JWPLC_UI_FIELD_BAR)
        {
            return false;
        }

        if (field->hasValue && field->barValue == value)
        {
            return true;
        }

        field->barValue = value;
        field->hasValue = true;
        markFieldDirty(*field);
        return true;
    }

    void setRefreshMode(JWPLC_UIRefreshMode mode)
    {
        g_refreshMode = mode;
        requestCurrentViewRefresh();
    }

    JWPLC_UIRefreshMode refreshMode()
    {
        return g_refreshMode;
    }

    void requestRefresh()
    {
        requestCurrentViewRefresh();
    }

    bool refreshNeeded()
    {
        return (g_refreshMode == USER_REFRESH_PERIODIC) ||
               g_refreshRequested ||
               g_pageRedrawPending ||
               anyDirtyOnCurrentPage();
    }

    void consumeRefreshRequest()
    {
        g_refreshRequested = false;
    }

    void setPage(uint8_t page)
    {
        if (g_page == page)
        {
            return;
        }

        g_page = page;
        g_pageRedrawPending = true;

        for (size_t i = 0; i < g_fieldCount; ++i)
        {
            if (g_fields[i].used &&
                g_fields[i].def.meta.page == g_page)
            {
                g_fields[i].dirty = true;
            }
        }

        requestCurrentViewRefresh();
    }

    uint8_t currentPage()
    {
        return g_page;
    }

    bool pageRedrawPending()
    {
        return g_pageRedrawPending;
    }

    void consumePageRedrawPending()
    {
        g_pageRedrawPending = false;
    }

    void invalidateField(uint8_t fieldId)
    {
        FieldRuntime *field = findField(fieldId);

        if (field == nullptr)
        {
            return;
        }

        markFieldDirty(*field);
    }

    void invalidateAll(bool redrawStatic)
    {
        for (size_t i = 0; i < g_fieldCount; ++i)
        {
            if (g_fields[i].used)
            {
                g_fields[i].dirty = true;
            }
        }

        if (redrawStatic)
        {
            g_pageRedrawPending = true;
        }

        requestCurrentViewRefresh();
    }

    void prepareEnter()
    {
        g_pageRedrawPending = true;

        for (size_t i = 0; i < g_fieldCount; ++i)
        {
            if (g_fields[i].used &&
                g_fields[i].def.meta.page == g_page)
            {
                g_fields[i].dirty = true;
            }
        }

        g_refreshRequested = true;
    }

    void drawStatic(Adafruit_ST7789 &tft)
    {
        for (size_t i = 0; i < g_fieldCount; ++i)
        {
            FieldRuntime &field = g_fields[i];

            if (!field.used || field.def.meta.page != g_page)
            {
                continue;
            }

            drawFieldStatic(tft, field);
        }
    }

    void drawDirty(Adafruit_ST7789 &tft)
    {
        for (size_t i = 0; i < g_fieldCount; ++i)
        {
            FieldRuntime &field = g_fields[i];

            if (!field.used ||
                field.def.meta.page != g_page ||
                !field.dirty)
            {
                continue;
            }

            drawFieldValue(tft, field);
        }
    }
}

// =====================================================
// Puente API publica JWPLC_DisplayClass -> motor HMI Alpha8
// =====================================================

void JWPLC_DisplayClass::setUserRefreshMode(JWPLC_UIRefreshMode mode)
{
    JWPLCUI::setRefreshMode(mode);
}

JWPLC_UIRefreshMode JWPLC_DisplayClass::userRefreshMode() const
{
    return JWPLCUI::refreshMode();
}

void JWPLC_DisplayClass::requestUserRefresh()
{
    JWPLCUI::requestRefresh();
}

void JWPLC_DisplayClass::setUserPage(uint8_t page)
{
    JWPLCUI::setPage(page);
}

uint8_t JWPLC_DisplayClass::userPage() const
{
    return JWPLCUI::currentPage();
}

bool JWPLC_DisplayClass::setFields(const JWPLC_UIField *fields, size_t count)
{
    return JWPLCUI::setFields(fields, count);
}

void JWPLC_DisplayClass::clearFields()
{
    JWPLCUI::clearFields();
}

size_t JWPLC_DisplayClass::fieldCount() const
{
    return JWPLCUI::fieldCount();
}

bool JWPLC_DisplayClass::setNumericValue(uint8_t fieldId, double value)
{
    return JWPLCUI::setNumericValue(fieldId, value);
}

bool JWPLC_DisplayClass::setText(uint8_t fieldId, const char *value)
{
    return JWPLCUI::setText(fieldId, value);
}

bool JWPLC_DisplayClass::setBool(uint8_t fieldId, bool value)
{
    return JWPLCUI::setBool(fieldId, value);
}

bool JWPLC_DisplayClass::setBar(uint8_t fieldId, float value)
{
    return JWPLCUI::setBar(fieldId, value);
}

void JWPLC_DisplayClass::invalidateField(uint8_t fieldId)
{
    JWPLCUI::invalidateField(fieldId);
}

void JWPLC_DisplayClass::invalidateAllFields()
{
    JWPLCUI::invalidateAll(false);
}
