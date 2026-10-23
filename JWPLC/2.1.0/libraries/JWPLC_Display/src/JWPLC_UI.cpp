#include "JWPLC_UI.h"
#include "JWPLC_UI_Format.h"
#include <JWPLC_TFT.h>
#include "JWPLC_Display_API.h"
#include "JWPLC_Display_H3E1_Profile.h"

#include <Arduino.h>

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
        bool boolState;
        // BOOL con fondo por estado: al cambiar de estado se repinta el campo
        // completo (fondo, marco, etiqueta y unidad), no sólo el valor.
        bool staticDirty;

        char value[VALUE_BUFFER_SIZE];
        float barValue;
        // BAR: ancho de relleno que hay en pantalla; -1 = desconocido
        // (repintado completo). Con él sólo se pinta o borra el tramo que cambió.
        int16_t barDrawnW;
        // ICON: estado pedido (0..stateCount-1); se dibuja aunque no haya setter.
        uint8_t iconState;

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
              boolState(false),
              staticDirty(false),
              barValue(0.0f),
              barDrawnW(-1),
              iconState(0),
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

    // ICON: filas RGB565 decodificadas que van juntas en un pushImage (1 KB,
    // 8 filas de un icono de 64 px). Fuera de la pila del loop.
    uint16_t g_iconRows[JWPLC_UI_ICON_MAX_SIZE * 8];
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
        JWPLC_TFTClass &tft,
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

        // FIELD_PADDING sigue siendo el margen visual minimo. El texto con
        // fondo pinta la celda clasica completa (6x8 por escala): una columna
        // y una fila de `escala` px mas que el cuerpo nominal 5x7 del layout.
        // Ese exceso, mas la linea del marco (1 px), debe caber en el margen
        // derecho/inferior: pad >= escala + 1. Si no, en 3x y 4x el fondo del
        // texto borraba el marco.
        // Alpha15: este margen se aplica sólo a la derecha y abajo (trailing);
        // arriba/izquierda el margen es siempre FIELD_PADDING para que la
        // etiqueta y el valor no se desplacen al cambiar de escala.
        const int16_t needed = (int16_t)maxTextSize + 1;
        return (needed > FIELD_PADDING) ? needed : FIELD_PADDING;
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

    bool setValueString(FieldRuntime &field, const char *value)
    {
        if (value == nullptr)
        {
            value = "";
        }

        if (field.hasValue &&
            strncmp(field.value, value, sizeof(field.value)) == 0)
        {
            jwplcH3E1RecordSetter(
                field.def.meta.id,
                false);
            return false;
        }

        strncpy(field.value, value, sizeof(field.value) - 1);
        field.value[sizeof(field.value) - 1] = '\0';

        field.hasValue = true;
        markFieldDirty(field);

        jwplcH3E1RecordSetter(
            field.def.meta.id,
            true);
        return true;
    }

    int16_t alignedValueX(
        JWPLC_TFTClass &tft,
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

    void computeFieldGeometry(JWPLC_TFTClass &tft, FieldRuntime &field)
    {
        const JWPLC_UIField &def = field.def;
        // lead: margen superior/izquierdo fijo; pad: margen derecho/inferior.
        const int16_t lead = FIELD_PADDING;
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
            JWPLCUIFormat::sample(def.format, sample, sizeof(sample));
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
            if (def.options.boolText.representation == JWPLC_UI_BOOL_AS_PILOT)
            {
                // Piloto: caja cuadrada de la altura del cuerpo del texto.
                const uint8_t scale = (def.style.valueTextSize == 0) ? 1 : def.style.valueTextSize;
                valueW = (uint16_t)(7 * scale);
                valueH = valueW;
                break;
            }

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

        case JWPLC_UI_FIELD_ICON:
            if (def.icon != nullptr &&
                def.icon->width <= JWPLC_UI_ICON_MAX_SIZE &&
                def.icon->height <= JWPLC_UI_ICON_MAX_SIZE)
            {
                valueW = def.icon->width;
                valueH = def.icon->height;
            }
            break;

        case JWPLC_UI_FIELD_BAR:
        default:
            valueW =
                (def.rect.width == JWPLC_UI_AUTO)
                    ? DEFAULT_BAR_WIDTH
                    : (uint16_t)((def.rect.width > lead + pad)
                                     ? def.rect.width - lead - pad
                                     : DEFAULT_BAR_WIDTH);
            valueH = DEFAULT_BAR_HEIGHT;
            break;
        }

        if (def.meta.type == JWPLC_UI_FIELD_BAR &&
            def.rect.width != JWPLC_UI_AUTO)
        {
            int16_t available = def.rect.width - lead - pad;

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
                lead + pad +
                (((int16_t)labelW > valueAndUnitW)
                     ? (int16_t)labelW
                     : valueAndUnitW);

            autoH =
                lead + pad +
                (int16_t)labelH +
                ((labelH > 0) ? FIELD_GAP : 0) +
                (int16_t)((valueH > unitH) ? valueH : unitH);

            field.valueX = def.rect.x + lead;
            field.valueY =
                def.rect.y + lead +
                (int16_t)labelH +
                ((labelH > 0) ? FIELD_GAP : 0);
        }
        else
        {
            autoW =
                lead + pad +
                (int16_t)labelW +
                ((labelW > 0) ? FIELD_GAP : 0) +
                (int16_t)valueW +
                ((unitW > 0) ? FIELD_GAP + (int16_t)unitW : 0);

            autoH =
                lead + pad +
                (int16_t)((labelH > valueH)
                              ? ((labelH > unitH) ? labelH : unitH)
                              : ((valueH > unitH) ? valueH : unitH));

            field.valueX =
                def.rect.x + lead +
                (int16_t)labelW +
                ((labelW > 0) ? FIELD_GAP : 0);

            field.valueY = def.rect.y + lead;
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

    // Fondo del campo: el del estado actual en BOOL con fondo por estado.
    uint16_t fieldBackground(const FieldRuntime &field)
    {
        const JWPLC_UIField &def = field.def;

        if (def.meta.type == JWPLC_UI_FIELD_BOOL &&
            (def.options.boolText.stateColors & JWPLC_UI_BOOL_STATE_BACKGROUND))
        {
            return field.boolState
                       ? def.options.boolText.trueBackground
                       : def.options.boolText.falseBackground;
        }

        return def.style.colors.background;
    }

    int16_t barFillWidth(const FieldRuntime &field)
    {
        float minValue = field.def.options.barRange.min;
        float maxValue = field.def.options.barRange.max;

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

        return (int16_t)lroundf(normalized * field.valueW);
    }

    // ICON sin fondo: true si el píxel (x, y) es transparente en todos los
    // estados (nunca se pinta, así se ve la gráfica estática de debajo).
    bool iconAlwaysTransparent(const JWPLC_UIIcon &icon, size_t stride, int16_t x, int16_t y)
    {
        for (uint8_t s = 0; s < icon.stateCount; ++s)
        {
            const uint8_t *pixels = icon.states[s].pixels;
            if (pixels == nullptr)
            {
                continue;
            }
            const uint8_t packed = pixels[(size_t)y * stride + ((size_t)x >> 1)];
            const uint8_t index = (x & 1) ? (packed & 0x0F) : (packed >> 4);
            if (index != icon.transparentIndex)
            {
                return false;
            }
        }
        return true;
    }

    // ICON sin fondo: cada fila se envía en tramos que saltan los píxeles
    // transparentes en todos los estados.
    void drawIconTransparent(JWPLC_TFTClass &tft, const FieldRuntime &field,
                             const JWPLC_UIIcon &icon, const JWPLC_UIIconState &state,
                             uint16_t background)
    {
        const int16_t w = icon.width;
        const size_t stride = ((size_t)w + 1U) / 2U;

        for (int16_t y = 0; y < icon.height; ++y)
        {
            const uint8_t *row = state.pixels + (size_t)y * stride;
            int16_t x = 0;

            while (x < w)
            {
                while (x < w && iconAlwaysTransparent(icon, stride, x, y))
                {
                    ++x;
                }
                const int16_t start = x;
                uint16_t *out = g_iconRows;
                while (x < w && !iconAlwaysTransparent(icon, stride, x, y))
                {
                    const uint8_t packed = row[x >> 1];
                    const uint8_t index = (x & 1) ? (packed & 0x0F) : (packed >> 4);
                    *out++ = (index == icon.transparentIndex) ? background : state.palette[index];
                    ++x;
                }
                if (x > start)
                {
                    tft.pushImage(field.valueX + start, field.valueY + y, x - start, 1, g_iconRows);
                }
            }
        }
    }

    // ICON: decodifica el estado actual (4 bpp + paleta) por bloques de filas
    // y lo envía con pushImage. El índice transparente toma el fondo del campo.
    void drawIcon(JWPLC_TFTClass &tft, const FieldRuntime &field, uint16_t background)
    {
        const JWPLC_UIIcon *icon = field.def.icon;

        if (icon == nullptr ||
            icon->states == nullptr ||
            icon->stateCount == 0 ||
            icon->width == 0 ||
            icon->height == 0 ||
            icon->width > JWPLC_UI_ICON_MAX_SIZE ||
            icon->height > JWPLC_UI_ICON_MAX_SIZE)
        {
            return;
        }

        const uint8_t stateIndex =
            (field.iconState < icon->stateCount)
                ? field.iconState
                : (uint8_t)(icon->stateCount - 1);
        const JWPLC_UIIconState &state = icon->states[stateIndex];

        if (state.pixels == nullptr || state.palette == nullptr)
        {
            return;
        }

        if (field.def.iconTransparent && icon->transparentIndex != JWPLC_UI_ICON_NO_TRANSPARENT)
        {
            drawIconTransparent(tft, field, *icon, state, background);
            return;
        }

        const int16_t w = icon->width;
        const int16_t h = icon->height;
        const size_t stride = ((size_t)w + 1U) / 2U;
        const int16_t rowsPerPush =
            (int16_t)((sizeof(g_iconRows) / sizeof(g_iconRows[0])) / (size_t)w);

        for (int16_t y0 = 0; y0 < h; y0 += rowsPerPush)
        {
            const int16_t rows = (h - y0 < rowsPerPush) ? (h - y0) : rowsPerPush;
            uint16_t *out = g_iconRows;

            for (int16_t r = 0; r < rows; ++r)
            {
                const uint8_t *row = state.pixels + (size_t)(y0 + r) * stride;

                for (int16_t x = 0; x < w; ++x)
                {
                    const uint8_t packed = row[x >> 1];
                    const uint8_t index = (x & 1) ? (packed & 0x0F) : (packed >> 4);
                    *out++ = (index == icon->transparentIndex)
                                 ? background
                                 : state.palette[index];
                }
            }

            tft.pushImage(
                field.valueX,
                field.valueY + y0,
                w,
                rows,
                g_iconRows);
        }
    }

    void drawFieldStatic(JWPLC_TFTClass &tft, FieldRuntime &field)
    {
        computeFieldGeometry(tft, field);

        const JWPLC_UIField &def = field.def;
        const uint16_t background = fieldBackground(field);
        // ICON sin fondo: ni recuadro ni fondo detrás de la etiqueta.
        const bool transparent = def.meta.type == JWPLC_UI_FIELD_ICON && def.iconTransparent;
        field.staticDirty = false;

        if (!transparent && field.fieldW > 0 && field.fieldH > 0)
        {
            tft.fillRect(
                field.fieldX,
                field.fieldY,
                field.fieldW,
                field.fieldH,
                background);
        }

        // El fondo del campo dejó la barra vacía.
        field.barDrawnW = 0;

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
            if (transparent)
            {
                tft.setTextColor(def.style.colors.label);
            }
            else
            {
                tft.setTextColor(
                    def.style.colors.label,
                    background);
            }
            tft.setCursor(
                def.rect.x + FIELD_PADDING,
                def.rect.y + FIELD_PADDING);
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
                background);
            tft.setCursor(
                field.valueX + field.valueW + FIELD_GAP,
                field.valueY);
            tft.print(def.text.unit);
        }

        field.dirty = true;
    }

    void drawFieldValue(JWPLC_TFTClass &tft, FieldRuntime &field)
    {
        const JWPLC_UIField &def = field.def;
        const bool profile = jwplcH3E1ProfilerEnabled();
        const uint32_t totalStartUs = profile ? micros() : 0U;

        uint32_t clearUs = 0U;
        uint32_t alignUs = 0U;
        uint32_t printUs = 0U;

        if (field.valueW <= 0 || field.valueH <= 0)
        {
            field.dirty = false;

            if (profile)
            {
                jwplcH3E1RecordFieldDraw(
                    def.meta.id,
                    (uint8_t)def.meta.type,
                    0U,
                    0U,
                    0U,
                    (uint32_t)(micros() - totalStartUs),
                    0U,
                    0U,
                    0U);
            }

            return;
        }

        // BAR con relleno conocido en pantalla: sólo el tramo que cambió
        // (añade al subir, borra el sobrante al bajar), sin limpiar la barra.
        if (def.meta.type == JWPLC_UI_FIELD_BAR &&
            field.hasValue &&
            field.barDrawnW >= 0)
        {
            const int16_t fillW = barFillWidth(field);
            const int16_t oldW = field.barDrawnW;
            const uint32_t printStartUs = profile ? micros() : 0U;

            if (fillW > oldW)
            {
                tft.fillRect(
                    field.valueX + oldW,
                    field.valueY,
                    fillW - oldW,
                    field.valueH,
                    def.style.colors.value);
            }
            else if (fillW < oldW)
            {
                tft.fillRect(
                    field.valueX + fillW,
                    field.valueY,
                    oldW - fillW,
                    field.valueH,
                    fieldBackground(field));
            }

            field.barDrawnW = fillW;
            field.dirty = false;

            if (profile)
            {
                jwplcH3E1RecordFieldDraw(
                    def.meta.id,
                    (uint8_t)def.meta.type,
                    (uint16_t)field.valueW,
                    (uint16_t)field.valueH,
                    0U,
                    (uint32_t)(micros() - totalStartUs),
                    0U,
                    0U,
                    (uint32_t)(micros() - printStartUs));
            }

            return;
        }

        // ICON: la imagen cubre todo su recuadro, no hace falta limpiar antes.
        if (def.meta.type == JWPLC_UI_FIELD_ICON)
        {
            const uint32_t printStartUs = profile ? micros() : 0U;

            drawIcon(tft, field, fieldBackground(field));
            field.dirty = false;

            if (profile)
            {
                jwplcH3E1RecordFieldDraw(
                    def.meta.id,
                    (uint8_t)def.meta.type,
                    (uint16_t)field.valueW,
                    (uint16_t)field.valueH,
                    0U,
                    (uint32_t)(micros() - totalStartUs),
                    0U,
                    0U,
                    (uint32_t)(micros() - printStartUs));
            }

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

            // Nunca sobre la línea inferior del marco.
            const int16_t maxClearH =
                (field.fieldY + field.fieldH - (def.style.frame ? 1 : 0)) - field.valueY;
            if (clearH > maxClearH)
            {
                clearH = maxClearH;
            }
        }

        // BOOL con colores por estado: color del texto del valor; el fondo
        // por estado es el de todo el campo (fieldBackground).
        uint16_t valueColor = def.style.colors.value;
        const uint16_t valueBackground = fieldBackground(field);

        if (def.meta.type == JWPLC_UI_FIELD_BOOL &&
            (def.options.boolText.stateColors & JWPLC_UI_BOOL_STATE_TEXT))
        {
            valueColor = field.boolState
                             ? def.options.boolText.trueColor
                             : def.options.boolText.falseColor;
        }

        const uint32_t clearStartUs = profile ? micros() : 0U;

        tft.fillRect(
            field.valueX,
            field.valueY,
            field.valueW,
            clearH,
            valueBackground);

        if (profile)
        {
            clearUs =
                (uint32_t)(micros() - clearStartUs);
        }

        if (def.meta.type == JWPLC_UI_FIELD_BAR)
        {
            field.barDrawnW = 0;
        }

        if (!field.hasValue)
        {
            field.dirty = false;

            if (profile)
            {
                jwplcH3E1RecordFieldDraw(
                    def.meta.id,
                    (uint8_t)def.meta.type,
                    (uint16_t)field.valueW,
                    (uint16_t)field.valueH,
                    0U,
                    (uint32_t)(micros() - totalStartUs),
                    clearUs,
                    0U,
                    0U);
            }

            return;
        }

        if (def.meta.type == JWPLC_UI_FIELD_BAR)
        {
            const int16_t fillW = barFillWidth(field);

            const uint32_t printStartUs =
                profile ? micros() : 0U;

            if (fillW > 0)
            {
                tft.fillRect(
                    field.valueX,
                    field.valueY,
                    fillW,
                    field.valueH,
                    def.style.colors.value);
            }

            field.barDrawnW = fillW;

            if (profile)
            {
                printUs =
                    (uint32_t)(micros() - printStartUs);
            }

            field.dirty = false;

            if (profile)
            {
                jwplcH3E1RecordFieldDraw(
                    def.meta.id,
                    (uint8_t)def.meta.type,
                    (uint16_t)field.valueW,
                    (uint16_t)field.valueH,
                    0U,
                    (uint32_t)(micros() - totalStartUs),
                    clearUs,
                    0U,
                    printUs);
            }

            return;
        }

        if (def.meta.type == JWPLC_UI_FIELD_BOOL &&
            def.options.boolText.representation == JWPLC_UI_BOOL_AS_PILOT)
        {
            // Aro (color de la etiqueta) y relleno del estado. Diámetro impar
            // 2r+1 <= 7 × escala para que el centro caiga en un píxel.
            const int16_t r = (field.valueH - 1) / 2;
            const int16_t cx = field.valueX + r;
            const int16_t cy = field.valueY + r;
            const uint16_t stateColor = field.boolState
                                            ? def.options.boolText.trueColor
                                            : def.options.boolText.falseColor;
            const uint32_t printStartUs = profile ? micros() : 0U;

            tft.fillCircle(cx, cy, r, def.style.colors.label);
            if (r > 1)
            {
                tft.fillCircle(cx, cy, r - 1, stateColor);
            }

            if (profile)
            {
                printUs = (uint32_t)(micros() - printStartUs);
            }

            field.dirty = false;

            if (profile)
            {
                jwplcH3E1RecordFieldDraw(
                    def.meta.id,
                    (uint8_t)def.meta.type,
                    (uint16_t)field.valueW,
                    (uint16_t)field.valueH,
                    0U,
                    (uint32_t)(micros() - totalStartUs),
                    clearUs,
                    0U,
                    printUs);
            }

            return;
        }

        tft.setTextSize(
            (def.style.valueTextSize == 0)
                ? 1
                : def.style.valueTextSize);
        tft.setTextColor(
            valueColor,
            valueBackground);

        const uint32_t alignStartUs =
            profile ? micros() : 0U;

        const int16_t valueX =
            alignedValueX(tft, field);

        if (profile)
        {
            alignUs =
                (uint32_t)(micros() - alignStartUs);
        }

        tft.setCursor(valueX, field.valueY);

        const uint32_t printStartUs =
            profile ? micros() : 0U;

        tft.print(field.value);

        if (profile)
        {
            printUs =
                (uint32_t)(micros() - printStartUs);
        }

        field.dirty = false;

        if (profile)
        {
            const size_t length =
                strlen(field.value);

            jwplcH3E1RecordFieldDraw(
                def.meta.id,
                (uint8_t)def.meta.type,
                (uint16_t)field.valueW,
                (uint16_t)field.valueH,
                (uint8_t)(
                    (length > 255U)
                        ? 255U
                        : length),
                (uint32_t)(micros() - totalStartUs),
                clearUs,
                alignUs,
                printUs);
        }
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

        // Un reloj (fecha/hora) lo llena pollClockFields(): un setValue() del
        // sketch lo haría parpadear con guiones, así que se ignora.
        if (field == nullptr ||
            field->def.meta.type != JWPLC_UI_FIELD_VALUE ||
            field->def.format.kind == JWPLC_UI_VALUE_DATETIME)
        {
            return false;
        }

        char formatted[VALUE_BUFFER_SIZE] = {};
        const bool valid =
            JWPLCUIFormat::value(
                field->def.format,
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

        // Alpha15 I3: "Presión 25°C" escrito en el sketch (UTF-8) se ve con
        // tildes; la capacidad cuenta caracteres de la TFT, no bytes UTF-8.
        char glcd[VALUE_BUFFER_SIZE] = {};
        JWPLCUIFormat::utf8ToGlcd(value, glcd, sizeof(glcd));
        value = glcd;

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

        if (field != nullptr &&
            field->def.meta.type == JWPLC_UI_FIELD_ICON)
        {
            return setIconState(fieldId, value ? 1 : 0);
        }

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

        const bool stateChanged = field->boolState != value;
        field->boolState = value;

        if (stateChanged &&
            (field->def.options.boolText.stateColors & JWPLC_UI_BOOL_STATE_BACKGROUND))
        {
            field->staticDirty = true;
        }

        // Con color por estado, el mismo texto en ambos estados (p. ej. "●")
        // también debe repintarse al cambiar de estado.
        if (!setValueString(*field, text) &&
            stateChanged &&
            field->def.options.boolText.stateColors != 0)
        {
            markFieldDirty(*field);
        }
        return true;
    }

    bool setIconState(uint8_t fieldId, uint8_t state)
    {
        FieldRuntime *field = findField(fieldId);

        if (field == nullptr ||
            field->def.meta.type != JWPLC_UI_FIELD_ICON)
        {
            return false;
        }

        const uint8_t count =
            (field->def.icon != nullptr) ? field->def.icon->stateCount : 0;
        const bool valid = state < count;

        // Fuera de rango se ve el último estado (drawIcon recorta).
        if (field->hasValue && field->iconState == state)
        {
            jwplcH3E1RecordSetter(fieldId, false);
            return valid;
        }

        field->iconState = state;
        field->hasValue = true;
        markFieldDirty(*field);

        jwplcH3E1RecordSetter(fieldId, true);
        return valid;
    }

    void pollClockFields()
    {
        const JWPLC_RTCState *rtc = jwplcGetRTCState();
        JWPLCUIFormat::Clock clock = {};

        if (rtc != nullptr)
        {
            clock.valid = rtc->valid;
            clock.year = rtc->year;
            clock.month = rtc->month;
            clock.day = rtc->day;
            clock.hour = rtc->hour;
            clock.minute = rtc->minute;
            clock.second = rtc->second;
        }

        for (size_t i = 0; i < g_fieldCount; ++i)
        {
            FieldRuntime &field = g_fields[i];

            if (!field.used ||
                field.def.meta.type != JWPLC_UI_FIELD_VALUE ||
                field.def.format.kind != JWPLC_UI_VALUE_DATETIME)
            {
                continue;
            }

            // setValueString sólo marca dirty si el texto cambió: con HH:MM
            // la TFT se toca una vez por minuto.
            char text[VALUE_BUFFER_SIZE] = {};
            JWPLCUIFormat::dateTime(field.def.format, clock, text, sizeof(text));
            if (field.hasValue && strncmp(field.value, text, sizeof(field.value)) == 0)
            {
                continue; // sin contar un setter en el perfil H3E1
            }
            setValueString(field, text);
        }
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
            jwplcH3E1RecordSetter(
                fieldId,
                false);
            return true;
        }

        field->barValue = value;
        field->hasValue = true;
        markFieldDirty(*field);

        jwplcH3E1RecordSetter(
            fieldId,
            true);
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
                g_fields[i].barDrawnW = -1;
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

        // Invalidación explícita: la pantalla pudo cambiar, barra completa.
        field->barDrawnW = -1;
        markFieldDirty(*field);
    }

    void invalidateAll(bool redrawStatic)
    {
        for (size_t i = 0; i < g_fieldCount; ++i)
        {
            if (g_fields[i].used)
            {
                g_fields[i].dirty = true;
                g_fields[i].barDrawnW = -1;
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
                g_fields[i].barDrawnW = -1;
            }
        }

        g_refreshRequested = true;
    }

    void drawStatic(JWPLC_TFTClass &tft)
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

    void drawDirty(JWPLC_TFTClass &tft)
    {
        const bool profile =
            jwplcH3E1ProfilerEnabled();

        const uint32_t startUs =
            profile ? micros() : 0U;

        uint32_t dirtyCount = 0U;

        for (size_t i = 0; i < g_fieldCount; ++i)
        {
            FieldRuntime &field = g_fields[i];

            if (!field.used ||
                field.def.meta.page != g_page ||
                !field.dirty)
            {
                continue;
            }

            ++dirtyCount;

            if (field.staticDirty)
            {
                drawFieldStatic(tft, field);
            }

            drawFieldValue(tft, field);
        }

        if (profile)
        {
            jwplcH3E1RecordDirtyPass(
                dirtyCount);

            jwplcH3E1RecordStage(
                JWPLC_H3E1_STAGE_UI_DIRTY_CORE,
                (uint32_t)(micros() - startUs));
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

bool JWPLC_DisplayClass::setIconState(uint8_t fieldId, uint8_t state)
{
    return JWPLCUI::setIconState(fieldId, state);
}

void JWPLC_DisplayClass::invalidateField(uint8_t fieldId)
{
    JWPLCUI::invalidateField(fieldId);
}

void JWPLC_DisplayClass::invalidateAllFields()
{
    JWPLCUI::invalidateAll(false);
}
