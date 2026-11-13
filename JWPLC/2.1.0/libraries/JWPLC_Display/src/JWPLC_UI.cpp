#include "JWPLC_UI.h"
#include "JWPLC_UI_Format.h"
#include "JWPLC_UI_RuntimeHooks.h"
#include <JWPLC_TFT.h>
#include <JWPLC_GlobalPeripherals.h>
#include <JW_SD.h>
#include "JWPLC_Display_API.h"
#include "JWPLC_Display_H3E1_Profile.h"

#include <Arduino.h>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

extern "C"
{
#include "jwplc_peripherals.h"
}

#if defined(ESP32)
#include <soc/soc.h>
#endif

namespace
{
    static constexpr uint8_t VALUE_BUFFER_SIZE = 40;
    static constexpr int16_t FIELD_PADDING = 3;
    static constexpr int16_t FIELD_GAP = 4;
    static constexpr int16_t DEFAULT_BAR_WIDTH = 80;
    static constexpr int16_t DEFAULT_BAR_HEIGHT = 12;

    struct TrendRuntime
    {
        float *samples;
        JWPLCDataLog *sdLog;
        uint32_t lastSampleMs;
        uint32_t lastSdMs;
        float drawnLow;
        float drawnHigh;
        uint16_t capacity;
        uint16_t count;
        uint16_t head;
        uint16_t drawnCount;
        uint8_t channels;
        bool fullRedraw;
        bool headerOnly;
        bool hasDrawnScale;
        bool sdStarted;
        bool sdFailed;
        bool sdInitAttempted;
    };

    struct FieldRuntime
    {
        // Apunta directo a HMI_FIELDS[] en Flash (.rodata) o a g_ownedDefs si
        // setFields() recibió un arreglo temporal en RAM/stack (64 B totales).
        const JWPLC_UIField *def;

        union RuntimeStorage
        {
            char value[VALUE_BUFFER_SIZE];
            struct
            {
                float value;
                int16_t drawnW;
            } bar;
            struct
            {
                TrendRuntime *rt;
                float ch1;
                float ch2;
                float ch3;
            } trend;
            struct
            {
                uint32_t lastAnimTickMs;
                uint8_t state;
                uint8_t animFrame;
            } icon;

            RuntimeStorage()
            {
                memset(this, 0, sizeof(*this));
            }
        } u;

        int16_t fieldX;
        int16_t fieldY;
        int16_t fieldW;
        int16_t fieldH;

        int16_t valueX;
        int16_t valueY;
        int16_t valueW;
        int16_t valueH;

        bool dirty;
        bool hasValue;
        bool boolState;
        // BOOL con fondo por estado: al cambiar de estado se repinta el campo
        // completo (fondo, marco, etiqueta y unidad), no sólo el valor.
        bool staticDirty;

        FieldRuntime()
            : def(nullptr),
              u(),
              fieldX(0),
              fieldY(0),
              fieldW(0),
              fieldH(0),
              valueX(0),
              valueY(0),
              valueW(0),
              valueH(0),
              dirty(true),
              hasValue(false),
              boolState(false),
              staticDirty(false)
        {
        }
    };

    FieldRuntime g_fields[JWPLC_UI_MAX_FIELDS];
    JWPLC_UIField *g_ownedDefs = nullptr;

    inline bool isFieldUsed(const FieldRuntime &field)
    {
        return field.def != nullptr;
    }

    inline bool isFlashFieldArray(const JWPLC_UIField *fields)
    {
#if defined(ESP32) && defined(SOC_DROM_LOW) && defined(SOC_DROM_HIGH)
        const uintptr_t addr = (uintptr_t)fields;
        return addr >= (uintptr_t)SOC_DROM_LOW && addr < (uintptr_t)SOC_DROM_HIGH;
#else
        (void)fields;
        return false;
#endif
    }

    void freeTrendRuntime(FieldRuntime &field)
    {
        if (field.def == nullptr ||
            field.def->meta.type != JWPLC_UI_FIELD_TREND ||
            field.u.trend.rt == nullptr)
        {
            return;
        }
        TrendRuntime *tr = field.u.trend.rt;
        if (tr->sdLog != nullptr)
        {
            tr->sdLog->close(true);
            delete tr->sdLog;
            tr->sdLog = nullptr;
        }
        // samples vive en el mismo bloque contiguo calloc(TrendRuntime + samples).
        tr->samples = nullptr;
        free(tr);
        field.u.trend.rt = nullptr;
    }

    void freeAllTrendRuntimes()
    {
        for (size_t i = 0; i < JWPLC_UI_MAX_FIELDS; ++i)
        {
            freeTrendRuntime(g_fields[i]);
        }
        if (g_ownedDefs != nullptr)
        {
            free(g_ownedDefs);
            g_ownedDefs = nullptr;
        }
    }

    void resetFieldRedrawState(FieldRuntime &field)
    {
        field.dirty = true;
        if (field.def == nullptr)
        {
            return;
        }
        const JWPLC_UIFieldType t = field.def->meta.type;
        if (t == JWPLC_UI_FIELD_BAR || t == JWPLC_UI_FIELD_CIRCULAR)
        {
            field.u.bar.drawnW = -1;
        }
        else if (t == JWPLC_UI_FIELD_TREND && field.u.trend.rt != nullptr)
        {
            field.u.trend.rt->fullRedraw = true;
            field.u.trend.rt->headerOnly = false;
            field.u.trend.rt->drawnCount = 0U;
        }
    }

    struct TrendGeometry
    {
        int16_t x;
        int16_t y;
        int16_t w;
        int16_t h;
        int16_t plotX;
        int16_t plotY;
        int16_t plotW;
        int16_t plotH;
        int16_t baseAxisY;
        int16_t topGuideY;
        int16_t activeH;
        int16_t guide75Y;
        int16_t guide50Y;
        int16_t guide25Y;
        int16_t timeH;
    };

    struct TrendScale
    {
        float low;
        float mid;
        float high;
    };

    float trendNiceNum(float range, bool roundIt)
    {
        if (range <= 0.0f || !std::isfinite(range))
        {
            return 1.0f;
        }
        const float expVal = floorf(log10f(range));
        const float f = range / powf(10.0f, expVal);
        float nf = 1.0f;
        if (roundIt)
        {
            if (f < 1.5f) nf = 1.0f;
            else if (f < 3.0f) nf = 2.0f;
            else if (f < 7.0f) nf = 5.0f;
            else nf = 10.0f;
        }
        else
        {
            if (f <= 1.0f) nf = 1.0f;
            else if (f <= 2.0f) nf = 2.0f;
            else if (f <= 5.0f) nf = 5.0f;
            else nf = 10.0f;
        }
        return nf * powf(10.0f, expVal);
    }

    TrendScale calculateTrendScale(float minVal, float maxVal)
    {
        if (!std::isfinite(minVal) || !std::isfinite(maxVal))
        {
            return {0.0f, 50.0f, 100.0f};
        }
        if (minVal > maxVal)
        {
            const float tmp = minVal;
            minVal = maxVal;
            maxVal = tmp;
        }
        if (fabsf(maxVal - minVal) < 0.0001f)
        {
            const float pad = (fabsf(minVal) < 1.0f) ? 1.0f : fabsf(minVal) * 0.05f;
            minVal -= pad;
            maxVal += pad;
        }
        const float span = maxVal - minVal;
        // Dividir en ~4 pasos finos evita saltos de escala desproporcionados (p. ej. 25->40)
        // manteniendo números limpios e histéresis frente a ruido pequeño.
        const float step = trendNiceNum(span / 4.0f, true);
        float graphMin = floorf(minVal / step) * step;
        float graphMax = ceilf(maxVal / step) * step;
        if (graphMax <= graphMin)
        {
            graphMax = graphMin + step;
        }
        return {graphMin, graphMin + (graphMax - graphMin) * 0.5f, graphMax};
    }

    TrendGeometry computeTrendGeometry(const FieldRuntime &field)
    {
        TrendGeometry g{};
        const JWPLC_UIField &def = *field.def;
        const JWPLC_UITrendConfig &cfg = def.trendConfig;
        g.x = def.rect.x;
        g.y = def.rect.y;
        g.w = (field.fieldW >= 40) ? field.fieldW : 200;
        g.h = (field.fieldH >= 30) ? field.fieldH : 80;

        int8_t dec = (int8_t)cfg.decimals;
        if (dec < 0) dec = 0;
        if (dec > 3) dec = 3;

        int16_t scaleMargin = 2;
        if (cfg.showAxes)
        {
            scaleMargin = (dec > 1) ? 46 : ((dec == 1) ? 38 : 30);
        }

        const int16_t topPad = 12;
        g.timeH = cfg.showTime ? 13 : 4;

        g.plotX = g.x + scaleMargin + 2;
        g.plotY = g.y + topPad;
        g.plotW = g.w - scaleMargin - 6;
        if (g.plotW < 10) g.plotW = 10;
        g.plotH = g.h - topPad - g.timeH;
        if (g.plotH < 10) g.plotH = 10;

        g.baseAxisY = g.plotY + g.plotH;
        g.topGuideY = g.plotY + 4;
        g.activeH = g.baseAxisY - g.topGuideY;
        if (g.activeH < 6) g.activeH = 6;

        g.guide75Y = (int16_t)lroundf((float)g.baseAxisY - 0.75f * (float)g.activeH);
        g.guide50Y = (int16_t)lroundf((float)g.baseAxisY - 0.50f * (float)g.activeH);
        g.guide25Y = (int16_t)lroundf((float)g.baseAxisY - 0.25f * (float)g.activeH);
        return g;
    }

    bool trendValueToY(float val, float minVal, float maxVal, int16_t topGuideY, int16_t activeH, int16_t &outY)
    {
        if (!std::isfinite(val) || !std::isfinite(minVal) || !std::isfinite(maxVal) || maxVal <= minVal)
        {
            outY = topGuideY + activeH / 2;
            return false;
        }
        const float ratio = (val - minVal) / (maxVal - minVal);
        const bool inRange = (ratio >= 0.0f && ratio <= 1.0f);
        float clamped = ratio;
        if (clamped < 0.0f) clamped = 0.0f;
        if (clamped > 1.0f) clamped = 1.0f;
        outY = topGuideY + (int16_t)lroundf((1.0f - clamped) * (float)activeH);
        return inRange;
    }

    TrendRuntime *ensureTrendRuntime(FieldRuntime &field)
    {
        if (field.u.trend.rt != nullptr)
        {
            return field.u.trend.rt;
        }
        uint16_t cap = field.def->trendConfig.visibleSamples;
        if (cap < 20) cap = 20;
        if (cap > 320) cap = 320;
        uint8_t chCount = field.def->trendConfig.channelCount;
        if (chCount < 1U) chCount = 1U;
        if (chCount > 3U) chCount = 3U;

        const size_t totalSamples = (size_t)cap * (size_t)chCount;
        TrendRuntime *tr = (TrendRuntime *)calloc(1, sizeof(TrendRuntime) + totalSamples * sizeof(float));
        if (tr == nullptr)
        {
            return nullptr;
        }
        tr->samples = (float *)(tr + 1);
        tr->capacity = cap;
        tr->channels = chCount;
        tr->drawnCount = 0U;
        tr->fullRedraw = true;
        tr->headerOnly = false;
        field.u.trend.rt = tr;
        return tr;
    }

    void writeTrendSdSample(FieldRuntime &field, TrendRuntime &tr, uint32_t now)
    {
        const JWPLC_UITrendConfig &cfg = field.def->trendConfig;
        if (!cfg.enableSD || tr.sdFailed)
        {
            return;
        }
        if (!JWPLC_SD.isReady())
        {
            if (tr.sdInitAttempted)
            {
                return;
            }
            tr.sdInitAttempted = true;
            if (!JWPLCSD::isEnabled() || !JWPLCSD::begin() || !JWPLC_SD.isReady())
            {
                return;
            }
        }
        const uint32_t sdInt = (cfg.sdIntervalMs >= 100U) ? cfg.sdIntervalMs : 100U;
        if (tr.sdStarted && (uint32_t)(now - tr.lastSdMs) < sdInt)
        {
            return;
        }

        if (!tr.sdStarted)
        {
            if (tr.sdLog == nullptr)
            {
                tr.sdLog = new JWPLCDataLog();
            }
            if (tr.sdLog == nullptr)
            {
                tr.sdFailed = true;
                return;
            }
            char path[32];
            snprintf(path, sizeof(path), "/TREND_%u.CSV", (unsigned)field.def->meta.id);
            if (!tr.sdLog->begin(JWPLC_SD, path))
            {
                tr.sdFailed = true;
                return;
            }
            tr.sdStarted = true;
            if (cfg.channelCount >= 3)
            {
                tr.sdLog->writeLine("ms,ch1,ch2,ch3");
            }
            else if (cfg.channelCount == 2)
            {
                tr.sdLog->writeLine("ms,ch1,ch2");
            }
            else
            {
                tr.sdLog->writeLine("ms,ch1");
            }
        }

        tr.lastSdMs = now;
        char line[96];
        if (cfg.channelCount >= 3)
        {
            snprintf(line, sizeof(line), "%lu,%.3f,%.3f,%.3f",
                     (unsigned long)now,
                     (double)field.u.trend.ch1,
                     (double)field.u.trend.ch2,
                     (double)field.u.trend.ch3);
        }
        else if (cfg.channelCount == 2)
        {
            snprintf(line, sizeof(line), "%lu,%.3f,%.3f",
                     (unsigned long)now,
                     (double)field.u.trend.ch1,
                     (double)field.u.trend.ch2);
        }
        else
        {
            snprintf(line, sizeof(line), "%lu,%.3f",
                     (unsigned long)now,
                     (double)field.u.trend.ch1);
        }
        tr.sdLog->writeLine(line);
    }

    void pushTrendSample(FieldRuntime &field, TrendRuntime &tr, uint32_t now)
    {
        const uint8_t chStride = (tr.channels >= 1U && tr.channels <= 3U) ? tr.channels : 1U;
        const size_t slot = (size_t)tr.head * (size_t)chStride;
        tr.samples[slot + 0U] = field.u.trend.ch1;
        if (chStride >= 2U) tr.samples[slot + 1U] = field.u.trend.ch2;
        if (chStride >= 3U) tr.samples[slot + 2U] = field.u.trend.ch3;
        tr.head = (uint16_t)((tr.head + 1U) % tr.capacity);
        if (tr.count < tr.capacity)
        {
            tr.count++;
        }
        tr.lastSampleMs = now;
        writeTrendSdSample(field, tr, now);
    }

    bool tickTrendField(FieldRuntime &field, uint32_t now, bool forceFirstSample)
    {
        if (field.def->meta.type != JWPLC_UI_FIELD_TREND || !field.hasValue)
        {
            return false;
        }
        TrendRuntime *tr = ensureTrendRuntime(field);
        if (tr == nullptr)
        {
            return false;
        }
        const uint32_t sampleMs = (field.def->trendConfig.sampleIntervalMs >= 50U)
                                      ? field.def->trendConfig.sampleIntervalMs
                                      : 1000U;
        if (forceFirstSample && tr->count == 0U)
        {
            pushTrendSample(field, *tr, now);
            return true;
        }
        if (tr->count > 0U && (uint32_t)(now - tr->lastSampleMs) >= sampleMs)
        {
            pushTrendSample(field, *tr, now);
            return true;
        }
        return false;
    }

    // Buffer de bandas RGB565 compartido entre ICON y TREND anti-parpadeo
    // (320 * 8 = 2560 píxeles = 5 KB fijos para toda la HMI, sin fragmentar heap).
    static constexpr size_t BAND_BUFFER_PIXELS = 320U * 8U;
    uint16_t g_bandRows[BAND_BUFFER_PIXELS];
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
            if (isFieldUsed(g_fields[i]) && g_fields[i].def->meta.id == id)
            {
                return &g_fields[i];
            }
        }

        return nullptr;
    }

    bool isTrendSampleDue(const FieldRuntime &f, uint32_t now)
    {
        if (!isFieldUsed(f) || f.def->meta.type != JWPLC_UI_FIELD_TREND)
        {
            return false;
        }
        if (!f.hasValue || f.u.trend.rt == nullptr || f.u.trend.rt->count == 0U)
        {
            return true;
        }
        const uint32_t sampleMs = (f.def->trendConfig.sampleIntervalMs >= 50U)
                                      ? f.def->trendConfig.sampleIntervalMs
                                      : 1000U;
        return (uint32_t)(now - f.u.trend.rt->lastSampleMs) >= sampleMs;
    }

    bool anyTrendSampleDueImpl()
    {
        const uint32_t now = millis();
        for (size_t i = 0; i < g_fieldCount; ++i)
        {
            if (isTrendSampleDue(g_fields[i], now))
            {
                return true;
            }
        }
        return false;
    }

    void tickDueTrendsImpl()
    {
        const uint32_t now = millis();
        for (size_t i = 0; i < g_fieldCount; ++i)
        {
            FieldRuntime &f = g_fields[i];
            if (!isFieldUsed(f) || f.def->meta.type != JWPLC_UI_FIELD_TREND || !f.hasValue)
            {
                continue;
            }
            if (tickTrendField(f, now, false))
            {
                if (f.u.trend.rt != nullptr)
                {
                    f.u.trend.rt->headerOnly = false;
                }
                f.dirty = true;
            }
        }
    }

    bool anyDirtyOnCurrentPage()
    {
        const uint32_t now = millis();
        bool dirtyFound = false;

        for (size_t i = 0; i < g_fieldCount; ++i)
        {
            FieldRuntime &f = g_fields[i];
            if (!isFieldUsed(f))
            {
                continue;
            }

            if (f.def->meta.page != g_page)
            {
                continue;
            }

            if (f.def->meta.type == JWPLC_UI_FIELD_TREND && isTrendSampleDue(f, now))
            {
                dirtyFound = true;
                continue;
            }

            if (f.dirty)
            {
                dirtyFound = true;
                continue;
            }

            // Animación autónoma de fotogramas en iconos
            if (f.def->meta.type == JWPLC_UI_FIELD_ICON)
            {
                const JWPLC_UIIcon *icon = f.def->icon;
                if (icon != nullptr && icon->states != nullptr && icon->stateCount > 0)
                {
                    const uint8_t stIdx = (f.u.icon.state < icon->stateCount)
                                              ? f.u.icon.state
                                              : (uint8_t)(icon->stateCount - 1);
                    const JWPLC_UIIconState &st = icon->states[stIdx];
                    if (st.frameCount > 1 && st.frameIntervalMs > 0 && st.frames != nullptr)
                    {
                        if ((uint32_t)(now - f.u.icon.lastAnimTickMs) >= st.frameIntervalMs)
                        {
                            f.u.icon.lastAnimTickMs = now;
                            f.u.icon.animFrame = (uint8_t)((f.u.icon.animFrame + 1) % st.frameCount);
                            f.dirty = true;
                            dirtyFound = true;
                        }
                    }
                }
            }
        }

        return dirtyFound;
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

        if (field.def->meta.page == g_page)
        {
            requestCurrentViewRefresh();
        }
    }

    void textBounds(
        JWPLC_TFTClass &tft,
        const char *text,
        uint8_t textSize,
        uint16_t &w,
        uint16_t &h,
        const JWPLC_TFTFont *font = nullptr)
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

        if (font != nullptr && font->kind != 0)
        {
            tft.setFont(*font);
            tft.getTextBounds(text, 0, 0, &x1, &y1, &w, &h);
            tft.setFont(JWPLC_FONT_GLCD);
            return;
        }

        tft.setFont(JWPLC_FONT_GLCD);
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

        if (def.meta.type != JWPLC_UI_FIELD_BAR && def.meta.type != JWPLC_UI_FIELD_CIRCULAR)
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
            strncmp(field.u.value, value, sizeof(field.u.value)) == 0)
        {
            jwplcH3E1RecordSetter(
                field.def->meta.id,
                false);
            return false;
        }

        strncpy(field.u.value, value, sizeof(field.u.value) - 1);
        field.u.value[sizeof(field.u.value) - 1] = '\0';

        field.hasValue = true;
        markFieldDirty(field);

        jwplcH3E1RecordSetter(
            field.def->meta.id,
            true);
        return true;
    }

    int16_t alignedValueX(
        JWPLC_TFTClass &tft,
        const FieldRuntime &field)
    {
        if (field.def->meta.type == JWPLC_UI_FIELD_BAR || field.def->meta.type == JWPLC_UI_FIELD_CIRCULAR)
        {
            return field.valueX;
        }

        uint16_t currentW = 0;
        uint16_t currentH = 0;
        textBounds(
            tft,
            field.u.value,
            field.def->style.valueTextSize,
            currentW,
            currentH,
            field.def->style.valueFont);

        if ((int16_t)currentW >= field.valueW)
        {
            return field.valueX;
        }

        const int16_t freeW = field.valueW - (int16_t)currentW;

        switch (field.def->style.valueAlign)
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
        const JWPLC_UIField &def = *field.def;
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
            labelH,
            def.style.labelFont);

        uint16_t unitW = 0;
        uint16_t unitH = 0;
        textBounds(
            tft,
            def.text.unit,
            def.style.labelTextSize,
            unitW,
            unitH,
            def.style.labelFont);

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
                valueH,
                def.style.valueFont);
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
                valueH,
                def.style.valueFont);
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
                falseH,
                def.style.valueFont);

            textBounds(
                tft,
                (def.options.boolText.trueText != nullptr)
                    ? def.options.boolText.trueText
                    : "ON",
                def.style.valueTextSize,
                trueW,
                trueH,
                def.style.valueFont);

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

        case JWPLC_UI_FIELD_TREND:
        {
            int16_t tw = (def.rect.width != JWPLC_UI_AUTO) ? def.rect.width : 200;
            int16_t th = (def.rect.height != JWPLC_UI_AUTO) ? def.rect.height : 80;
            if (tw < 40) tw = 40;
            if (th < 30) th = 30;
            field.fieldX = def.rect.x;
            field.fieldY = def.rect.y;
            field.fieldW = tw;
            field.fieldH = th;
            field.valueX = def.rect.x;
            field.valueY = def.rect.y;
            field.valueW = tw;
            field.valueH = th;
            return;
        }

        case JWPLC_UI_FIELD_CIRCULAR:
        {
            uint16_t diameter = (def.rect.width != JWPLC_UI_AUTO) ? def.rect.width : 52;
            if (diameter < 20) diameter = 20;
            valueW = diameter;
            valueH = diameter;
            break;
        }

        case JWPLC_UI_FIELD_BAR:
        default:
            if (def.meta.type == JWPLC_UI_FIELD_BAR && def.barConfig.orientation == JWPLC_UI_BAR_VERTICAL)
            {
                int16_t w = (def.rect.width != JWPLC_UI_AUTO && def.rect.width > lead + pad)
                                ? (def.rect.width - lead - pad)
                                : 14;
                int16_t h = (def.rect.height != JWPLC_UI_AUTO && def.rect.height > lead + pad)
                                ? (def.rect.height - lead - pad)
                                : 60;
                if (def.style.layout == JWPLC_UI_LAYOUT_STACKED && labelH > 0)
                {
                    if (h > (int16_t)labelH + FIELD_GAP) h -= (int16_t)labelH + FIELD_GAP;
                }
                if (def.barConfig.thickness > 0) w = def.barConfig.thickness;
                if (def.barConfig.length > 0) h = def.barConfig.length;
                valueW = w > 0 ? (uint16_t)w : 14;
                valueH = h > 0 ? (uint16_t)h : 60;
            }
            else
            {
                valueW =
                    (def.rect.width == JWPLC_UI_AUTO)
                        ? DEFAULT_BAR_WIDTH
                        : (uint16_t)((def.rect.width > lead + pad)
                                         ? def.rect.width - lead - pad
                                         : DEFAULT_BAR_WIDTH);
                valueH = (def.meta.type == JWPLC_UI_FIELD_BAR && def.barConfig.thickness > 0)
                             ? def.barConfig.thickness
                             : DEFAULT_BAR_HEIGHT;
            }
            break;
        }

        if (def.meta.type == JWPLC_UI_FIELD_BAR &&
            def.barConfig.orientation != JWPLC_UI_BAR_VERTICAL &&
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

        if (def.meta.type == JWPLC_UI_FIELD_BAR && def.barConfig.orientation == JWPLC_UI_BAR_VERTICAL)
        {
            if (def.style.layout == JWPLC_UI_LAYOUT_STACKED)
            {
                int16_t contentW = (int16_t)valueW;
                if ((int16_t)labelW > contentW) contentW = (int16_t)labelW;
                if ((int16_t)unitW > contentW) contentW = (int16_t)unitW;

                autoW = lead + pad + contentW;
                autoH = lead + pad +
                        (labelH > 0 ? (int16_t)labelH + FIELD_GAP : 0) +
                        (int16_t)valueH +
                        (unitW > 0 ? FIELD_GAP + (int16_t)unitH : 0);

                field.fieldW = (def.rect.width == JWPLC_UI_AUTO) ? autoW : def.rect.width;
                field.fieldH = (def.rect.height == JWPLC_UI_AUTO) ? autoH : def.rect.height;

                int16_t availW = field.fieldW - lead - pad;
                if (availW < (int16_t)valueW) availW = (int16_t)valueW;

                field.valueX = def.rect.x + lead + (availW - (int16_t)valueW) / 2;
                field.valueY = def.rect.y + lead + (labelH > 0 ? (int16_t)labelH + FIELD_GAP : 0);
            }
            else
            {
                int16_t maxH = (int16_t)valueH;
                if ((int16_t)labelH > maxH) maxH = (int16_t)labelH;
                if ((int16_t)unitH > maxH) maxH = (int16_t)unitH;

                autoW = lead + pad +
                        (labelW > 0 ? (int16_t)labelW + FIELD_GAP : 0) +
                        (int16_t)valueW +
                        (unitW > 0 ? FIELD_GAP + (int16_t)unitW : 0);
                autoH = lead + pad + maxH;

                field.fieldW = (def.rect.width == JWPLC_UI_AUTO) ? autoW : def.rect.width;
                field.fieldH = (def.rect.height == JWPLC_UI_AUTO) ? autoH : def.rect.height;

                field.valueX = def.rect.x + lead + (labelW > 0 ? (int16_t)labelW + FIELD_GAP : 0);
                field.valueY = def.rect.y + lead + (maxH - (int16_t)valueH) / 2;
            }
        }
        else if (def.style.layout == JWPLC_UI_LAYOUT_STACKED)
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
        if (!(def.meta.type == JWPLC_UI_FIELD_BAR && def.barConfig.orientation == JWPLC_UI_BAR_VERTICAL))
        {
            field.fieldW =
                (def.rect.width == JWPLC_UI_AUTO) ? autoW : def.rect.width;
            field.fieldH =
                (def.rect.height == JWPLC_UI_AUTO) ? autoH : def.rect.height;
        }

        field.valueW = (int16_t)valueW;
        field.valueH =
            (def.meta.type == JWPLC_UI_FIELD_BAR || def.meta.type == JWPLC_UI_FIELD_CIRCULAR)
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
        const JWPLC_UIField &def = *field.def;

        if (def.meta.type == JWPLC_UI_FIELD_BOOL &&
            (def.options.boolText.stateColors & JWPLC_UI_BOOL_STATE_BACKGROUND))
        {
            return field.boolState
                       ? def.options.boolText.trueBackground
                       : def.options.boolText.falseBackground;
        }

        return def.style.colors.background;
    }

    static inline uint16_t interpolate565(uint16_t c1, uint16_t c2, float t)
    {
        if (t <= 0.0f) return c1;
        if (t >= 1.0f) return c2;
        uint32_t r1 = (c1 >> 11) & 0x1F;
        uint32_t g1 = (c1 >> 5) & 0x3F;
        uint32_t b1 = c1 & 0x1F;
        uint32_t r2 = (c2 >> 11) & 0x1F;
        uint32_t g2 = (c2 >> 5) & 0x3F;
        uint32_t b2 = c2 & 0x1F;
        uint32_t r = (uint32_t)(r1 + (int32_t)(r2 - r1) * t);
        uint32_t g = (uint32_t)(g1 + (int32_t)(g2 - g1) * t);
        uint32_t b = (uint32_t)(b1 + (int32_t)(b2 - b1) * t);
        return (uint16_t)((r << 11) | (g << 5) | b);
    }

    int16_t barBorderInset(const JWPLC_UIField &def)
    {
        return (def.barConfig.borderEnabled && def.barConfig.borderWidth > 0)
                   ? (int16_t)def.barConfig.borderWidth
                   : 0;
    }

    // Margen de esquina de una fila: misma fórmula que getRoundedRectRowDx del Designer.
    int16_t roundedRowInset(int16_t row, int16_t height, int16_t radius)
    {
        int16_t d;
        if (radius <= 0) return 0;
        if (row < radius) d = radius - 1 - row;
        else if (row >= height - radius) d = row - (height - radius);
        else return 0;
        return (int16_t)lroundf((float)radius - sqrtf((float)(radius * radius - d * d)));
    }

    // Una línea del relleno (columna en horizontal, fila en vertical) recortada
    // por las esquinas. position cuenta desde el inicio de la barra.
    void paintBarLine(
        JWPLC_TFTClass &tft,
        int16_t x,
        int16_t y,
        int16_t w,
        int16_t h,
        int16_t radius,
        bool vertical,
        int16_t position,
        uint16_t color)
    {
        if (vertical)
        {
            const int16_t row = h - 1 - position;
            const int16_t dx = roundedRowInset(row, h, radius);
            if (w - 2 * dx > 0) tft.drawFastHLine(x + dx, y + row, w - 2 * dx, color);
            return;
        }

        const int16_t edge = (position < w - 1 - position) ? position : (w - 1 - position);
        int16_t top = 0;
        while (top < radius && roundedRowInset(top, h, radius) > edge) ++top;
        if (h - 2 * top > 0) tft.drawFastVLine(x + position, y + top, h - 2 * top, color);
    }

    int16_t barFillLevel(const FieldRuntime &field)
    {
        float minValue = field.def->options.barRange.min;
        float maxValue = field.def->options.barRange.max;

        if (maxValue <= minValue)
        {
            maxValue = minValue + 1.0f;
        }

        float normalized =
            (field.u.bar.value - minValue) / (maxValue - minValue);

        if (normalized < 0.0f)
        {
            normalized = 0.0f;
        }
        else if (normalized > 1.0f)
        {
            normalized = 1.0f;
        }

        const bool isVert = (field.def->barConfig.orientation == JWPLC_UI_BAR_VERTICAL);
        const int16_t inset = 2 * barBorderInset(*field.def);
        const int16_t totalLen = (isVert ? field.valueH : field.valueW) - inset;
        if (totalLen <= 0) return 0;
        return (int16_t)lroundf(normalized * (float)totalLen);
    }

    // Repinta sólo lo que cambió entre dos niveles, dentro del borde.
    void drawBarDelta(
        JWPLC_TFTClass &tft,
        FieldRuntime &field,
        int16_t oldLevel,
        int16_t newLevel)
    {
        const JWPLC_UIField &def = *field.def;
        const bool isVert = (def.barConfig.orientation == JWPLC_UI_BAR_VERTICAL);
        const bool isBlocks = (def.barConfig.style == JWPLC_UI_BAR_STYLE_BLOCKS);
        const uint16_t colStart = def.style.colors.value;
        const uint16_t colEnd = def.barConfig.colorEnd;
        const bool isGrad = (colEnd != 0 && colEnd != colStart);
        const uint16_t bg = fieldBackground(field);

        const int16_t b = barBorderInset(def);
        const int16_t vx = field.valueX + b;
        const int16_t vy = field.valueY + b;
        const int16_t vw = field.valueW - 2 * b;
        const int16_t vh = field.valueH - 2 * b;
        if (vw <= 0 || vh <= 0) return;

        const int16_t totalLen = isVert ? vh : vw;

        if (oldLevel < 0) oldLevel = 0;
        if (newLevel < 0) newLevel = 0;
        if (newLevel > totalLen) newLevel = totalLen;
        if (oldLevel > totalLen) oldLevel = totalLen;

        if (newLevel == oldLevel) return;

        const int16_t outerMin = (field.valueW < field.valueH) ? field.valueW : field.valueH;
        int16_t radius = (def.barConfig.cornerRadius == 255)
                             ? (outerMin / 2)
                             : (int16_t)def.barConfig.cornerRadius;
        if (radius > outerMin / 2) radius = outerMin / 2;
        radius -= b;
        const int16_t innerMin = (vw < vh) ? vw : vh;
        if (radius > innerMin / 2) radius = innerMin / 2;
        if (radius < 0) radius = 0;

        if (isBlocks)
        {
            // Bloques de 4 px con 1 px de separación, como el lienzo.
            const int16_t blockPitch = 5;
            const int16_t blockSize = 4;
            const int16_t numBlocks = (totalLen + 1) / blockPitch;
            if (numBlocks <= 0) return;

            const int16_t oldBlocks = (int16_t)lroundf((float)oldLevel * numBlocks / totalLen);
            const int16_t newBlocks = (int16_t)lroundf((float)newLevel * numBlocks / totalLen);
            const int16_t first = (newBlocks < oldBlocks) ? newBlocks : oldBlocks;
            const int16_t last = (newBlocks > oldBlocks) ? newBlocks : oldBlocks;

            if (radius > 0) tft.beginBatch();
            for (int16_t k = first; k < last; k++)
            {
                uint16_t c = bg;
                if (k < newBlocks)
                {
                    c = isGrad
                            ? interpolate565(colStart, colEnd, (float)k / (float)(numBlocks > 1 ? numBlocks - 1 : 1))
                            : colStart;
                }

                const int16_t start = k * blockPitch;
                if (radius > 0)
                {
                    for (int16_t p = start; p < start + blockSize && p < totalLen; p++)
                    {
                        paintBarLine(tft, vx, vy, vw, vh, radius, isVert, p, c);
                    }
                }
                else if (isVert)
                {
                    tft.fillRect(vx, vy + vh - start - blockSize, vw, blockSize, c);
                }
                else
                {
                    tft.fillRect(vx + start, vy, blockSize, vh, c);
                }
            }
            if (radius > 0) tft.endBatch();
            return;
        }

        const bool growing = (newLevel > oldLevel);
        const int16_t first = growing ? oldLevel : newLevel;
        const int16_t last = growing ? newLevel : oldLevel;
        const uint16_t flatColor = growing ? colStart : bg;

        if (radius <= 0 && (!growing || !isGrad))
        {
            if (isVert)
            {
                tft.fillRect(vx, vy + vh - last, vw, last - first, flatColor);
            }
            else
            {
                tft.fillRect(vx + first, vy, last - first, vh, flatColor);
            }
            return;
        }

        // Degradado o esquinas: línea a línea bajo un solo lock SPI.
        tft.beginBatch();
        for (int16_t p = first; p < last; p++)
        {
            const uint16_t c = (growing && isGrad)
                                   ? interpolate565(colStart, colEnd, (float)p / (float)totalLen)
                                   : flatColor;
            paintBarLine(tft, vx, vy, vw, vh, radius, isVert, p, c);
        }
        tft.endBatch();
    }

    // ICON sin fondo: true si el píxel (x, y) es transparente en todos los
    // estados y cuadros (nunca se pinta, así se ve la gráfica estática de debajo).
    bool iconAlwaysTransparent(const JWPLC_UIIcon &icon, size_t stride, int16_t x, int16_t y)
    {
        for (uint8_t s = 0; s < icon.stateCount; ++s)
        {
            const JWPLC_UIIconState &st = icon.states[s];
            if (st.frames != nullptr && st.frameCount > 1)
            {
                for (uint8_t f = 0; f < st.frameCount; ++f)
                {
                    const uint8_t *pixels = st.frames[f].pixels;
                    if (pixels == nullptr) continue;
                    const uint8_t packed = pixels[(size_t)y * stride + ((size_t)x >> 1)];
                    const uint8_t index = (x & 1) ? (packed & 0x0F) : (packed >> 4);
                    if (index != icon.transparentIndex) return false;
                }
            }
            else
            {
                const uint8_t *pixels = st.pixels;
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
        }
        return true;
    }

    // ICON sin fondo: cada fila se envía en tramos que saltan los píxeles
    // transparentes en todos los estados.
    void drawIconTransparent(JWPLC_TFTClass &tft, const FieldRuntime &field,
                             const JWPLC_UIIcon &icon, const uint8_t *pixels,
                             const uint16_t *palette, uint16_t background)
    {
        const int16_t w = icon.width;
        const size_t stride = ((size_t)w + 1U) / 2U;

        for (int16_t y = 0; y < icon.height; ++y)
        {
            const uint8_t *row = pixels + (size_t)y * stride;
            int16_t x = 0;

            while (x < w)
            {
                while (x < w && iconAlwaysTransparent(icon, stride, x, y))
                {
                    ++x;
                }
                const int16_t start = x;
                uint16_t *out = g_bandRows;
                while (x < w && !iconAlwaysTransparent(icon, stride, x, y))
                {
                    const uint8_t packed = row[x >> 1];
                    const uint8_t index = (x & 1) ? (packed & 0x0F) : (packed >> 4);
                    *out++ = (index == icon.transparentIndex) ? background : palette[index];
                    ++x;
                }
                if (x > start)
                {
                    tft.pushImage(field.valueX + start, field.valueY + y, x - start, 1, g_bandRows);
                }
            }
        }
    }

    // ICON: decodifica el estado actual (4 bpp + paleta) por bloques de filas
    // y lo envía con pushImage. El índice transparente toma el fondo del campo.
    void drawIcon(JWPLC_TFTClass &tft, const FieldRuntime &field, uint16_t background)
    {
        const JWPLC_UIIcon *icon = field.def->icon;

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
            (field.u.icon.state < icon->stateCount)
                ? field.u.icon.state
                : (uint8_t)(icon->stateCount - 1);
        const JWPLC_UIIconState &state = icon->states[stateIndex];

        const uint8_t *pixels = state.pixels;
        const uint16_t *palette = state.palette;
        if (state.frames != nullptr && state.frameCount > 1)
        {
            const uint8_t fIdx = (uint8_t)(field.u.icon.animFrame % state.frameCount);
            pixels = state.frames[fIdx].pixels;
            palette = state.frames[fIdx].palette;
        }

        if (pixels == nullptr || palette == nullptr)
        {
            return;
        }

        if (field.def->iconTransparent && icon->transparentIndex != JWPLC_UI_ICON_NO_TRANSPARENT)
        {
            drawIconTransparent(tft, field, *icon, pixels, palette, background);
            return;
        }

        const int16_t w = icon->width;
        const int16_t h = icon->height;
        const size_t stride = ((size_t)w + 1U) / 2U;
        const int16_t rowsPerPush =
            (int16_t)((sizeof(g_bandRows) / sizeof(g_bandRows[0])) / (size_t)w);

        for (int16_t y0 = 0; y0 < h; y0 += rowsPerPush)
        {
            const int16_t rows = (h - y0 < rowsPerPush) ? (h - y0) : rowsPerPush;
            uint16_t *out = g_bandRows;

            for (int16_t r = 0; r < rows; ++r)
            {
                const uint8_t *row = pixels + (size_t)(y0 + r) * stride;

                for (int16_t x = 0; x < w; ++x)
                {
                    const uint8_t packed = row[x >> 1];
                    const uint8_t index = (x & 1) ? (packed & 0x0F) : (packed >> 4);
                    *out++ = (index == icon->transparentIndex)
                                 ? background
                                 : palette[index];
                }
            }

            tft.pushImage(
                field.valueX,
                field.valueY + y0,
                w,
                rows,
                g_bandRows);
        }
    }

    void drawCircularGauge(
        JWPLC_TFTClass &tft,
        FieldRuntime &field,
        uint16_t background)
    {
        const JWPLC_UIField &def = *field.def;
        float minValue = def.options.barRange.min;
        float maxValue = def.options.barRange.max;
        if (maxValue <= minValue) maxValue = minValue + 1.0f;
        float normalized = (field.u.bar.value - minValue) / (maxValue - minValue);
        if (normalized < 0.0f) normalized = 0.0f;
        if (normalized > 1.0f) normalized = 1.0f;

        const int16_t cx = field.valueX + field.valueW / 2;
        const int16_t cy = field.valueY + field.valueH / 2;
        const int16_t R = field.valueW / 2;
        int16_t thick = def.circularStyle.thickness;
        if (thick < 2) thick = 2;
        if (thick > R - 2) thick = R - 2;
        const int16_t Rin = R - thick;

        uint16_t sweepDeg = def.circularStyle.sweep;
        if (sweepDeg == 0 || sweepDeg > 360) sweepDeg = 360;
        const uint8_t quad = def.circularStyle.startQuadrant;
        float quadAngle = -90.0f; // TOP
        if (quad == 1) quadAngle = 0.0f; // RIGHT
        else if (quad == 2) quadAngle = 90.0f; // BOTTOM
        else if (quad == 3) quadAngle = 180.0f; // LEFT

        float startAngle = quadAngle;
        if (sweepDeg < 360) {
            startAngle = quadAngle - (float)sweepDeg / 2.0f;
        }

        const bool isCW = def.circularStyle.clockwise;
        const bool isSeg = def.circularStyle.segmented;
        const float activeSweep = normalized * (float)sweepDeg;
        const uint16_t activeCol = def.style.colors.value;
        const uint16_t trackCol = def.circularStyle.trackColor ? def.circularStyle.trackColor : background;

        const int16_t R2 = R * R;
        const int16_t Rin2 = Rin * Rin;

        tft.beginBatch();

        for (int16_t dy = -R; dy <= R; dy++)
        {
            int16_t py = cy + dy;
            if (py < 0 || py >= tft.height()) continue;
            int16_t dy2 = dy * dy;
            if (dy2 > R2) continue;
            int16_t dxOut = (int16_t)sqrtf((float)(R2 - dy2));
            int16_t dxIn = (Rin > 0 && dy2 < Rin2) ? (int16_t)sqrtf((float)(Rin2 - dy2)) : -1;

            if (dxIn >= 0)
            {
                tft.drawFastHLine(cx - dxIn, py, 2 * dxIn + 1, background);
            }

            for (int16_t side = 0; side < 2; side++)
            {
                int16_t xStart = (side == 0) ? -dxOut : (dxIn >= 0 ? dxIn + 1 : 0);
                int16_t xEnd = (side == 0) ? (dxIn >= 0 ? -dxIn - 1 : 0) : dxOut;
                if (xStart > xEnd) continue;

                for (int16_t dx = xStart; dx <= xEnd; dx++)
                {
                    int16_t px = cx + dx;
                    if (px < 0 || px >= tft.width()) continue;

                    float deg = atan2f((float)dy, (float)dx) * 180.0f / 3.14159265f;
                    float delta = isCW ? (deg - startAngle) : ((startAngle + (float)sweepDeg) - deg);
                    while (delta < 0.0f) delta += 360.0f;
                    while (delta >= 360.0f) delta -= 360.0f;

                    if (delta < (float)sweepDeg)
                    {
                        if (isSeg)
                        {
                            int segCount = (sweepDeg >= 360) ? 16 : ((sweepDeg >= 270) ? 12 : ((sweepDeg >= 240) ? 10 : 8));
                            float segStep = (float)sweepDeg / segCount;
                            float phase = fmodf(delta, segStep);
                            if (phase > segStep * 0.82f)
                            {
                                tft.drawPixel(px, py, background);
                                continue;
                            }
                        }

                        if (delta <= activeSweep && activeSweep > 0.5f)
                        {
                            tft.drawPixel(px, py, activeCol);
                        }
                        else
                        {
                            tft.drawPixel(px, py, trackCol);
                        }
                    }
                    else
                    {
                        tft.drawPixel(px, py, background);
                    }
                }
            }
        }

        if (def.circularStyle.centerMode == 1 || def.circularStyle.centerMode == 2) // PERCENT / VALUE
        {
            char buf[12];
            if (def.circularStyle.centerMode == 1)
            {
                int pct = (int)roundf(normalized * 100.0f);
                if (pct < 0) pct = 0;
                if (pct > 100) pct = 100;
                snprintf(buf, sizeof(buf), "%d%%", pct);
            }
            else
            {
                // Como el Designer: entero si no tiene decimales, si no un decimal.
                const float v = field.u.bar.value;
                if (v == (float)(long)v) snprintf(buf, sizeof(buf), "%ld", (long)v);
                else snprintf(buf, sizeof(buf), "%.1f", (double)v);
            }

            uint8_t sz = def.circularStyle.centerTextSize > 0 ? def.circularStyle.centerTextSize : 1;
            int16_t tw = (int16_t)(strlen(buf) * 6 * sz);
            int16_t th = (int16_t)(8 * sz);
            int16_t tx = cx - tw / 2;
            int16_t ty = cy - th / 2;
            uint16_t txtColor = def.circularStyle.centerTextColor ? def.circularStyle.centerTextColor : def.style.colors.label;

            tft.setTextSize(sz);
            tft.setTextColor(txtColor, background);
            tft.setCursor(tx, ty);
            tft.print(buf);
        }

        tft.endBatch();
    }

    void formatTrendValue(float val, uint8_t decimals, char *buf, size_t bufSize)
    {
        if (buf == nullptr || bufSize == 0)
        {
            return;
        }
        if (!std::isfinite(val))
        {
            snprintf(buf, bufSize, "--");
            return;
        }
        int dec = (int)decimals;
        if (dec < 0) dec = 0;
        if (dec > 3) dec = 3;
        snprintf(buf, bufSize, "%.*f", dec, (double)val);
    }

    void formatTrendTimeSpan(uint32_t totalSec, char *buf, size_t bufSize)
    {
        if (totalSec >= 3600U)
        {
            snprintf(buf, bufSize, "%luh", (unsigned long)(totalSec / 3600U));
        }
        else if (totalSec >= 60U)
        {
            snprintf(buf, bufSize, "%lum", (unsigned long)(totalSec / 60U));
        }
        else
        {
            snprintf(buf, bufSize, "%lus", (unsigned long)totalSec);
        }
    }

    TrendScale computeTrendDisplayedScale(const FieldRuntime &field, const TrendRuntime &tr)
    {
        const JWPLC_UIField &def = *field.def;
        const JWPLC_UITrendConfig &cfg = def.trendConfig;
        const bool manual = (cfg.scaleMode == 1);

        float rMin = def.options.barRange.min;
        float rMax = def.options.barRange.max;
        if (!std::isfinite(rMin) || !std::isfinite(rMax) || rMax <= rMin)
        {
            rMin = 0.0f;
            rMax = 100.0f;
        }

        if (manual)
        {
            return {rMin, rMin + (rMax - rMin) * 0.5f, rMax};
        }

        if (tr.count == 0U)
        {
            return calculateTrendScale(rMin, rMax);
        }

        uint8_t chCount = cfg.channelCount;
        if (chCount < 1U) chCount = 1U;
        if (chCount > 3U) chCount = 3U;
        const uint8_t chStride = (tr.channels >= 1U && tr.channels <= 3U) ? tr.channels : 1U;
        if (chCount > chStride) chCount = chStride;

        bool found = false;
        float dataMin = 0.0f;
        float dataMax = 0.0f;
        const uint16_t oldestIdx = (tr.count < tr.capacity) ? 0U : tr.head;
        for (uint16_t i = 0U; i < tr.count; ++i)
        {
            const uint16_t slotIdx = (uint16_t)((oldestIdx + i) % tr.capacity);
            const size_t base = (size_t)slotIdx * (size_t)chStride;
            for (uint8_t ch = 0U; ch < chCount; ++ch)
            {
                const float v = tr.samples[base + ch];
                if (!std::isfinite(v))
                {
                    continue;
                }
                if (!found)
                {
                    dataMin = v;
                    dataMax = v;
                    found = true;
                }
                else
                {
                    if (v < dataMin) dataMin = v;
                    if (v > dataMax) dataMax = v;
                }
            }
        }

        if (!found)
        {
            return calculateTrendScale(rMin, rMax);
        }
        return calculateTrendScale(dataMin, dataMax);
    }

    void drawTrendGuidesAndThresholds(
        JWPLC_TFTClass &tft,
        const JWPLC_UITrendConfig &cfg,
        const TrendGeometry &g,
        const TrendScale &scale)
    {
        const int16_t xStart = g.plotX + (cfg.showAxes ? 1 : 0);
        const int16_t lineW = g.plotW - (cfg.showAxes ? 1 : 0);
        if (lineW <= 0)
        {
            return;
        }

        if (cfg.showGuides)
        {
            tft.drawFastHLine(xStart, g.guide75Y, lineW, cfg.gridColor);
            tft.drawFastHLine(xStart, g.guide50Y, lineW, cfg.gridColor);
            tft.drawFastHLine(xStart, g.guide25Y, lineW, cfg.gridColor);
        }

        uint8_t thrCount = cfg.thresholdCount;
        if (thrCount > 2U) thrCount = 2U;
        const float thrVals[2] = {cfg.thr1Value, cfg.thr2Value};
        const uint16_t thrCols[2] = {cfg.thr1Color, cfg.thr2Color};
        const int16_t maxY = cfg.showAxes ? (g.baseAxisY - 1) : g.baseAxisY;

        for (uint8_t i = 0U; i < thrCount; ++i)
        {
            int16_t ty = 0;
            if (!trendValueToY(thrVals[i], scale.low, scale.high, g.topGuideY, g.activeH, ty))
            {
                continue;
            }
            if (ty < g.plotY || ty > maxY)
            {
                continue;
            }
            for (int16_t dx = 0; dx < lineW; ++dx)
            {
                if ((dx % 6) < 3)
                {
                    tft.drawPixel(xStart + dx, ty, thrCols[i]);
                }
            }
        }
    }

    void formatTrendChannelTag(const char *raw, const char *fallback, char *out, size_t cap)
    {
        const char *src = (raw != nullptr && raw[0] != '\0') ? raw : fallback;
        char glcd[16];
        JWPLCUIFormat::utf8ToGlcd(src, glcd, sizeof(glcd));
        size_t n = strlen(glcd);
        if (n > 4U) n = 4U;
        if (n >= cap) n = cap - 1U;
        memcpy(out, glcd, n);
        out[n] = '\0';
    }

    int16_t drawOrMeasureTrendLegend(
        JWPLC_TFTClass *tft,
        const JWPLC_UITrendConfig &cfg,
        const TrendGeometry &g,
        int16_t minLeftX)
    {
        uint8_t chCount = cfg.channelCount;
        if (chCount < 1U) chCount = 1U;
        if (chCount > 3U) chCount = 3U;
        int16_t rightLimit = g.x + g.w - 3;
        if (chCount <= 1U || rightLimit - minLeftX < 18)
        {
            return rightLimit;
        }

        char lbl2[6];
        char lbl3[6];
        formatTrendChannelTag(cfg.ch2Label, "SP", lbl2, sizeof(lbl2));
        formatTrendChannelTag(cfg.ch3Label, "CH3", lbl3, sizeof(lbl3));
        const int16_t item2W = 5 + 2 + (int16_t)strlen(lbl2) * 6;
        const int16_t item3W = (chCount >= 3U) ? (5 + 2 + (int16_t)strlen(lbl3) * 6 + 5) : 0;
        const int16_t fullW = item2W + item3W;

        if (rightLimit - fullW >= minLeftX)
        {
            if (tft != nullptr)
            {
                tft->setFont(JWPLC_FONT_GLCD);
                tft->setTextSize(1);
            }
            if (chCount >= 3U)
            {
                const int16_t x3 = rightLimit - (5 + 2 + (int16_t)strlen(lbl3) * 6);
                if (tft != nullptr)
                {
                    tft->fillRect(x3, g.y + 3, 5, 5, cfg.ch3Color);
                    tft->setTextColor(cfg.ch3Color, cfg.backgroundColor);
                    tft->setCursor(x3 + 7, g.y + 2);
                    tft->print(lbl3);
                }
                rightLimit = x3 - 5;
            }
            const int16_t x2 = rightLimit - item2W;
            if (tft != nullptr)
            {
                tft->fillRect(x2, g.y + 3, 5, 5, cfg.ch2Color);
                tft->setTextColor(cfg.ch2Color, cfg.backgroundColor);
                tft->setCursor(x2 + 7, g.y + 2);
                tft->print(lbl2);
            }
            return x2 - 4;
        }

        // Respaldo compacto: solo cuadros de color si el encabezado es muy estrecho
        if (chCount >= 3U)
        {
            rightLimit -= 8;
            if (tft != nullptr) tft->fillRect(rightLimit, g.y + 3, 6, 6, cfg.ch3Color);
            rightLimit -= 2;
        }
        rightLimit -= 8;
        if (tft != nullptr) tft->fillRect(rightLimit, g.y + 3, 6, 6, cfg.ch2Color);
        return rightLimit - 3;
    }

    void drawTrendStaticChrome(
        JWPLC_TFTClass &tft,
        const FieldRuntime &field,
        const TrendGeometry &g,
        const TrendScale &scale)
    {
        const JWPLC_UIField &def = *field.def;
        const JWPLC_UITrendConfig &cfg = def.trendConfig;

        tft.fillRect(g.x, g.y, g.w, g.h, cfg.backgroundColor);

        if (def.style.frame && g.w > 1 && g.h > 1)
        {
            const uint16_t frameCol = (def.style.colors.frame != 0U)
                                          ? def.style.colors.frame
                                          : cfg.gridColor;
            tft.drawRect(g.x, g.y, g.w, g.h, frameCol);
        }

        tft.setFont(JWPLC_FONT_GLCD);
        tft.setTextSize(1);

        int16_t titleEnd = 3;
        if (cfg.showLiveValue && def.text.label != nullptr && def.text.label[0] != '\0')
        {
            char glcdTitle[40];
            JWPLCUIFormat::utf8ToGlcd(def.text.label, glcdTitle, sizeof(glcdTitle));
            const int16_t maxChars = (g.w > 24) ? ((g.w / 2 - 4) / 6) : 3;
            int16_t len = (int16_t)strlen(glcdTitle);
            if (maxChars > 0 && len > maxChars)
            {
                len = maxChars;
                glcdTitle[maxChars] = '\0';
            }
            tft.setTextColor(cfg.curveColor, cfg.backgroundColor);
            tft.setCursor(g.x + 3, g.y + 2);
            tft.print(glcdTitle);
            titleEnd = 3 + len * 6 + 6;
        }

        drawOrMeasureTrendLegend(&tft, cfg, g, g.x + titleEnd + (cfg.showLiveValue ? 36 : 0));

        if (cfg.showAxes)
        {
            const int16_t axisH = g.baseAxisY - g.plotY + 1;
            if (axisH > 0)
            {
                tft.drawFastVLine(g.plotX, g.plotY, axisH, cfg.axesColor);
            }
            tft.drawFastHLine(g.plotX, g.baseAxisY, g.plotW, cfg.axesColor);
            tft.drawFastHLine(g.plotX - 2, g.topGuideY, 2, cfg.axesColor);
            tft.drawFastHLine(g.plotX - 2, g.guide50Y, 2, cfg.axesColor);
            tft.drawFastHLine(g.plotX - 2, g.baseAxisY, 2, cfg.axesColor);

            const float scaleVals[3] = {scale.high, scale.mid, scale.low};
            int16_t labelYs[5] = {
                (int16_t)(g.topGuideY - 3),
                (int16_t)(g.guide50Y - 3),
                (int16_t)(g.baseAxisY - 7),
                0,
                0};
            uint8_t placedCount = 3U;

            for (int i = 0; i < 3; ++i)
            {
                if (labelYs[i] < g.plotY) labelYs[i] = g.plotY;
                if (labelYs[i] > g.baseAxisY - 7) labelYs[i] = g.baseAxisY - 7;
                char numBuf[16];
                formatTrendValue(scaleVals[i], cfg.decimals, numBuf, sizeof(numBuf));
                const int16_t tw = (int16_t)strlen(numBuf) * 6;
                int16_t lx = g.plotX - 3 - tw;
                if (lx < g.x + 2) lx = g.x + 2;
                tft.setTextColor(cfg.textColor, cfg.backgroundColor);
                tft.setCursor(lx, labelYs[i]);
                tft.print(numBuf);
            }

            uint8_t thrCount = cfg.thresholdCount;
            if (thrCount > 2U) thrCount = 2U;
            const float thrVals[2] = {cfg.thr1Value, cfg.thr2Value};
            const uint16_t thrCols[2] = {cfg.thr1Color, cfg.thr2Color};
            for (uint8_t i = 0U; i < thrCount; ++i)
            {
                int16_t ty = 0;
                if (!trendValueToY(thrVals[i], scale.low, scale.high, g.topGuideY, g.activeH, ty))
                {
                    continue;
                }
                int16_t ly = ty - 3;
                if (ly < g.plotY) ly = g.plotY;
                if (ly > g.baseAxisY - 7) ly = g.baseAxisY - 7;

                bool collides = false;
                for (uint8_t p = 0U; p < placedCount; ++p)
                {
                    if (abs((int)(ly - labelYs[p])) < 9)
                    {
                        collides = true;
                        break;
                    }
                }
                if (!collides && placedCount < 5U)
                {
                    char numBuf[16];
                    formatTrendValue(thrVals[i], cfg.decimals, numBuf, sizeof(numBuf));
                    const int16_t tw = (int16_t)strlen(numBuf) * 6;
                    int16_t lx = g.plotX - 3 - tw;
                    if (lx < g.x + 2) lx = g.x + 2;
                    tft.setTextColor(thrCols[i], cfg.backgroundColor);
                    tft.setCursor(lx, ly);
                    tft.print(numBuf);
                    labelYs[placedCount++] = ly;
                }
            }
        }

        if (cfg.showTime)
        {
            const uint32_t sampleMs = (cfg.sampleIntervalMs >= 50U) ? cfg.sampleIntervalMs : 1000U;
            const uint16_t cap = (cfg.visibleSamples >= 20U) ? ((cfg.visibleSamples <= 320U) ? cfg.visibleSamples : 320U) : 20U;
            const uint32_t spanSec = (sampleMs * (uint32_t)(cap - 1U)) / 1000U;
            char spanBuf[16];
            formatTrendTimeSpan((spanSec > 0U) ? spanSec : 1U, spanBuf, sizeof(spanBuf));

            const int16_t timeY = g.baseAxisY + 3;
            tft.setTextColor(cfg.textColor, cfg.backgroundColor);
            tft.setCursor(g.plotX, timeY);
            tft.print("0s");

            const int16_t spanW = (int16_t)strlen(spanBuf) * 6;
            int16_t spanX = g.plotX + g.plotW - spanW;
            if (spanX < g.plotX + 18) spanX = g.plotX + 18;
            tft.setCursor(spanX, timeY);
            tft.print(spanBuf);
        }
    }

    void drawTrendHeaderLive(
        JWPLC_TFTClass &tft,
        const FieldRuntime &field,
        const TrendGeometry &g)
    {
        const JWPLC_UIField &def = *field.def;
        const JWPLC_UITrendConfig &cfg = def.trendConfig;
        if (!cfg.showLiveValue)
        {
            return;
        }

        int16_t titleEnd = 3;
        if (def.text.label != nullptr && def.text.label[0] != '\0')
        {
            char glcdTitle[40];
            JWPLCUIFormat::utf8ToGlcd(def.text.label, glcdTitle, sizeof(glcdTitle));
            const int16_t maxChars = (g.w > 24) ? ((g.w / 2 - 4) / 6) : 3;
            int16_t len = (int16_t)strlen(glcdTitle);
            if (maxChars > 0 && len > maxChars) len = maxChars;
            titleEnd = 3 + len * 6 + 6;
        }

        const int16_t clearX = g.x + titleEnd;
        const int16_t rightLimit = drawOrMeasureTrendLegend(nullptr, cfg, g, clearX + 36);
        const int16_t clearW = rightLimit - clearX;
        if (clearW <= 6)
        {
            return;
        }

        tft.setFont(JWPLC_FONT_GLCD);
        tft.setTextSize(1);

        char valText[32];
        if (!field.hasValue)
        {
            snprintf(valText, sizeof(valText), "--");
        }
        else
        {
            char numBuf[16];
            formatTrendValue(field.u.trend.ch1, cfg.decimals, numBuf, sizeof(numBuf));
            if (def.text.unit != nullptr && def.text.unit[0] != '\0')
            {
                char glcdUnit[16];
                JWPLCUIFormat::utf8ToGlcd(def.text.unit, glcdUnit, sizeof(glcdUnit));
                snprintf(valText, sizeof(valText), "%s %s", numBuf, glcdUnit);
            }
            else
            {
                snprintf(valText, sizeof(valText), "%s", numBuf);
            }
        }

        const int16_t availChars = (rightLimit - clearX) / 6;
        if (availChars > 0 && (int16_t)strlen(valText) > availChars)
        {
            valText[availChars] = '\0';
        }
        int16_t printedW = 0;
        if (availChars > 0)
        {
            tft.setTextColor(field.hasValue ? cfg.curveColor : cfg.textColor, cfg.backgroundColor);
            tft.setCursor(clearX, g.y + 2);
            tft.print(valText);
            printedW = (int16_t)strlen(valText) * 6;
        }
        // Limpiar solo el sobrante derecho si el texto nuevo es más corto (evita parpadeo en dígitos).
        if (printedW < clearW)
        {
            tft.fillRect(clearX + printedW, g.y + 1, clearW - printedW, 10, cfg.backgroundColor);
        }
    }

    static inline void plotBandPixel(
        uint16_t *band,
        int16_t bandW,
        int16_t bandX0,
        int16_t bandX1,
        int16_t bandY0,
        int16_t bandY1,
        int16_t x,
        int16_t y,
        uint16_t color)
    {
        if (x >= bandX0 && x <= bandX1 && y >= bandY0 && y <= bandY1)
        {
            band[(size_t)(y - bandY0) * (size_t)bandW + (size_t)(x - bandX0)] = color;
        }
    }

    void rasterizeTrendBandLine(
        uint16_t *band,
        int16_t bandW,
        int16_t bandX0,
        int16_t bandY0,
        int16_t bandY1,
        int16_t x0,
        int16_t y0,
        int16_t x1,
        int16_t y1,
        uint16_t color)
    {
        const int16_t bandX1 = bandX0 + bandW - 1;
        if ((y0 < bandY0 && y1 < bandY0) || (y0 > bandY1 && y1 > bandY1))
        {
            return;
        }
        if ((x0 < bandX0 && x1 < bandX0) || (x0 > bandX1 && x1 > bandX1))
        {
            return;
        }

        int16_t dx = abs(x1 - x0);
        int16_t sx = (x0 < x1) ? 1 : -1;
        int16_t dy = -abs(y1 - y0);
        int16_t sy = (y0 < y1) ? 1 : -1;
        int16_t err = dx + dy;

        for (;;)
        {
            plotBandPixel(band, bandW, bandX0, bandX1, bandY0, bandY1, x0, y0, color);
            if (x0 == x1 && y0 == y1)
            {
                break;
            }
            const int16_t e2 = (int16_t)(2 * err);
            if (e2 >= dy)
            {
                err += dy;
                x0 += sx;
            }
            if (e2 <= dx)
            {
                err += dx;
                y0 += sy;
            }
        }
    }

    bool computeTrendSamplePoint(
        uint16_t sampleIndex,
        uint16_t capacity,
        float val,
        int16_t startX,
        int16_t availW,
        int16_t maxX,
        int16_t minY,
        int16_t maxY,
        const TrendGeometry &g,
        const TrendScale &scale,
        int16_t &outX,
        int16_t &outY)
    {
        if (!std::isfinite(val))
        {
            return false;
        }
        int16_t px = startX + (int16_t)lroundf(
                                  ((float)sampleIndex / (float)(capacity - 1U)) * (float)(availW - 1));
        if (px < startX) px = startX;
        if (px > maxX) px = maxX;

        int16_t py = 0;
        trendValueToY(val, scale.low, scale.high, g.topGuideY, g.activeH, py);
        if (py < minY) py = minY;
        if (py > maxY) py = maxY;

        outX = px;
        outY = py;
        return true;
    }

    void drawTrendCurveRange(
        JWPLC_TFTClass &tft,
        const JWPLC_UITrendConfig &cfg,
        const TrendRuntime &tr,
        const TrendGeometry &g,
        const TrendScale &scale,
        uint16_t fromSample,
        uint16_t toSample)
    {
        if (tr.count == 0U || g.plotW <= 2 || fromSample > toSample || toSample >= tr.count)
        {
            return;
        }

        uint8_t chCount = cfg.channelCount;
        if (chCount < 1U) chCount = 1U;
        if (chCount > 3U) chCount = 3U;
        const uint8_t chStride = (tr.channels >= 1U && tr.channels <= 3U) ? tr.channels : 1U;
        if (chCount > chStride) chCount = chStride;

        const uint16_t chColors[3] = {cfg.curveColor, cfg.ch2Color, cfg.ch3Color};
        const uint16_t oldestIdx = (tr.count < tr.capacity) ? 0U : tr.head;
        const int16_t startX = g.plotX + (cfg.showAxes ? 1 : 0);
        const int16_t availW = g.plotW - (cfg.showAxes ? 1 : 0);
        const int16_t maxX = startX + availW - 1;
        const int16_t minY = g.plotY;
        const int16_t maxY = cfg.showAxes ? (g.baseAxisY - 1) : g.baseAxisY;
        const bool thick = (cfg.lineWidth >= 2U);

        if (availW <= 1 || maxY <= minY)
        {
            return;
        }

        for (int8_t ch = (int8_t)chCount - 1; ch >= 0; --ch)
        {
            const uint16_t col = chColors[ch];
            int16_t prevX = 0;
            int16_t prevY = 0;
            bool hasPrev = false;

            for (uint16_t i = fromSample; i <= toSample; ++i)
            {
                const uint16_t ringIdx = (uint16_t)((oldestIdx + i) % tr.capacity);
                const float val = tr.samples[(size_t)ringIdx * (size_t)chStride + (size_t)ch];
                int16_t px = 0;
                int16_t py = 0;
                if (!computeTrendSamplePoint(
                        i, tr.capacity, val, startX, availW, maxX, minY, maxY, g, scale, px, py))
                {
                    hasPrev = false;
                    continue;
                }

                if (!hasPrev)
                {
                    if (tr.count == 1U || i > fromSample)
                    {
                        tft.drawPixel(px, py, col);
                        if (thick && py + 1 <= maxY)
                        {
                            tft.drawPixel(px, py + 1, col);
                        }
                    }
                    prevX = px;
                    prevY = py;
                    hasPrev = true;
                    continue;
                }

                tft.drawLine(prevX, prevY, px, py, col);
                if (thick)
                {
                    const int16_t yOff = (prevY + 1 <= maxY && py + 1 <= maxY) ? 1 : -1;
                    if (prevY + yOff >= minY && py + yOff >= minY)
                    {
                        tft.drawLine(prevX, prevY + yOff, px, py + yOff, col);
                    }
                }
                prevX = px;
                prevY = py;
            }
        }
    }

    void drawTrendCurves(
        JWPLC_TFTClass &tft,
        const JWPLC_UITrendConfig &cfg,
        const TrendRuntime &tr,
        const TrendGeometry &g,
        const TrendScale &scale)
    {
        if (tr.count == 0U)
        {
            return;
        }
        drawTrendCurveRange(tft, cfg, tr, g, scale, 0U, (uint16_t)(tr.count - 1U));
    }

    // Compone fondo, guías, umbrales y curvas en franjas RGB565 en RAM y las
    // empuja con pushImage sin borrar previamente la pantalla (cero parpadeo).
    void drawTrendPlotBands(
        JWPLC_TFTClass &tft,
        const JWPLC_UITrendConfig &cfg,
        const TrendRuntime &tr,
        const TrendGeometry &g,
        const TrendScale &scale)
    {
        const int16_t clearX = g.plotX + (cfg.showAxes ? 1 : 0);
        const int16_t clearW = g.plotW - (cfg.showAxes ? 1 : 0);
        const int16_t minY = g.plotY;
        const int16_t maxY = cfg.showAxes ? (g.baseAxisY - 1) : g.baseAxisY;
        const int16_t clearH = maxY - minY + 1;

        if (clearW <= 0 || clearH <= 0)
        {
            return;
        }

        const int16_t rowsPerBand = (int16_t)(BAND_BUFFER_PIXELS / (size_t)clearW);
        if (rowsPerBand < 1)
        {
            tft.fillRect(clearX, minY, clearW, clearH, cfg.backgroundColor);
            drawTrendGuidesAndThresholds(tft, cfg, g, scale);
            drawTrendCurves(tft, cfg, tr, g, scale);
            return;
        }

        uint8_t chCount = cfg.channelCount;
        if (chCount < 1U) chCount = 1U;
        if (chCount > 3U) chCount = 3U;
        const uint8_t chStride = (tr.channels >= 1U && tr.channels <= 3U) ? tr.channels : 1U;
        if (chCount > chStride) chCount = chStride;

        const uint16_t chColors[3] = {cfg.curveColor, cfg.ch2Color, cfg.ch3Color};
        const uint16_t oldestIdx = (tr.count < tr.capacity) ? 0U : tr.head;
        const int16_t maxX = clearX + clearW - 1;
        const bool thick = (cfg.lineWidth >= 2U);

        uint8_t thrCount = cfg.thresholdCount;
        if (thrCount > 2U) thrCount = 2U;
        const float thrVals[2] = {cfg.thr1Value, cfg.thr2Value};
        const uint16_t thrCols[2] = {cfg.thr1Color, cfg.thr2Color};
        int16_t thrYs[2] = {-1, -1};
        for (uint8_t i = 0U; i < thrCount; ++i)
        {
            int16_t ty = 0;
            if (trendValueToY(thrVals[i], scale.low, scale.high, g.topGuideY, g.activeH, ty) &&
                ty >= minY && ty <= maxY)
            {
                thrYs[i] = ty;
            }
        }

        const int16_t guidesY[3] = {g.guide75Y, g.guide50Y, g.guide25Y};

        for (int16_t y0 = minY; y0 <= maxY; y0 += rowsPerBand)
        {
            const int16_t rem = (int16_t)(maxY - y0 + 1);
            const int16_t rows = (rem < rowsPerBand) ? rem : rowsPerBand;
            const int16_t bandY1 = y0 + rows - 1;
            const size_t bandPixels = (size_t)clearW * (size_t)rows;

            for (size_t p = 0; p < bandPixels; ++p)
            {
                g_bandRows[p] = cfg.backgroundColor;
            }

            if (cfg.showGuides)
            {
                for (uint8_t gi = 0U; gi < 3U; ++gi)
                {
                    const int16_t gy = guidesY[gi];
                    if (gy >= y0 && gy <= bandY1)
                    {
                        uint16_t *rowPtr = g_bandRows + (size_t)(gy - y0) * (size_t)clearW;
                        for (int16_t dx = 0; dx < clearW; ++dx)
                        {
                            rowPtr[dx] = cfg.gridColor;
                        }
                    }
                }
            }

            for (uint8_t ti = 0U; ti < thrCount; ++ti)
            {
                const int16_t ty = thrYs[ti];
                if (ty >= y0 && ty <= bandY1)
                {
                    uint16_t *rowPtr = g_bandRows + (size_t)(ty - y0) * (size_t)clearW;
                    for (int16_t dx = 0; dx < clearW; ++dx)
                    {
                        if ((dx % 6) < 3)
                        {
                            rowPtr[dx] = thrCols[ti];
                        }
                    }
                }
            }

            if (tr.count > 0U && clearW > 1 && maxY > minY)
            {
                for (int8_t ch = (int8_t)chCount - 1; ch >= 0; --ch)
                {
                    const uint16_t col = chColors[ch];
                    int16_t prevX = 0;
                    int16_t prevY = 0;
                    bool hasPrev = false;

                    for (uint16_t i = 0U; i < tr.count; ++i)
                    {
                        const uint16_t ringIdx = (uint16_t)((oldestIdx + i) % tr.capacity);
                        const float val = tr.samples[(size_t)ringIdx * (size_t)chStride + (size_t)ch];
                        int16_t px = 0;
                        int16_t py = 0;
                        if (!computeTrendSamplePoint(
                                i, tr.capacity, val, clearX, clearW, maxX, minY, maxY, g, scale, px, py))
                        {
                            hasPrev = false;
                            continue;
                        }

                        if (!hasPrev)
                        {
                            if (tr.count == 1U)
                            {
                                plotBandPixel(g_bandRows, clearW, clearX, maxX, y0, bandY1, px, py, col);
                                if (thick && py + 1 <= maxY)
                                {
                                    plotBandPixel(g_bandRows, clearW, clearX, maxX, y0, bandY1, px, py + 1, col);
                                }
                            }
                            prevX = px;
                            prevY = py;
                            hasPrev = true;
                            continue;
                        }

                        rasterizeTrendBandLine(g_bandRows, clearW, clearX, y0, bandY1, prevX, prevY, px, py, col);
                        if (thick)
                        {
                            const int16_t yOff = (prevY + 1 <= maxY && py + 1 <= maxY) ? 1 : -1;
                            if (prevY + yOff >= minY && py + yOff >= minY)
                            {
                                rasterizeTrendBandLine(
                                    g_bandRows, clearW, clearX, y0, bandY1,
                                    prevX, (int16_t)(prevY + yOff), px, (int16_t)(py + yOff), col);
                            }
                        }
                        prevX = px;
                        prevY = py;
                    }
                }
            }

            tft.pushImage(clearX, y0, clearW, rows, g_bandRows);
        }
    }

    void drawTrendField(JWPLC_TFTClass &tft, FieldRuntime &field)
    {
        const JWPLC_UITrendConfig &cfg = field.def->trendConfig;
        const TrendGeometry g = computeTrendGeometry(field);
        TrendRuntime *tr = ensureTrendRuntime(field);
        if (tr == nullptr)
        {
            tft.fillRect(g.x, g.y, g.w, g.h, cfg.backgroundColor);
            field.dirty = false;
            return;
        }

        if (tr->headerOnly && !tr->fullRedraw && tr->hasDrawnScale)
        {
            tft.beginBatch();
            drawTrendHeaderLive(tft, field, g);
            tft.endBatch();
            tr->headerOnly = false;
            field.dirty = false;
            return;
        }
        tr->headerOnly = false;

        const TrendScale scale = computeTrendDisplayedScale(field, *tr);
        const bool scaleChanged = !tr->hasDrawnScale ||
                                  fabsf(scale.low - tr->drawnLow) > 1e-4f ||
                                  fabsf(scale.high - tr->drawnHigh) > 1e-4f;
        const bool pureAppend = !tr->fullRedraw &&
                                !scaleChanged &&
                                tr->drawnCount >= 1U &&
                                tr->count > tr->drawnCount &&
                                (tr->count < tr->capacity || tr->head == 0U);

        tft.beginBatch();
        if (tr->fullRedraw || scaleChanged)
        {
            drawTrendStaticChrome(tft, field, g, scale);
            drawTrendHeaderLive(tft, field, g);
            drawTrendPlotBands(tft, cfg, *tr, g, scale);
            tr->drawnLow = scale.low;
            tr->drawnHigh = scale.high;
            tr->hasDrawnScale = true;
            tr->fullRedraw = false;
            tr->drawnCount = tr->count;
        }
        else if (pureAppend)
        {
            drawTrendHeaderLive(tft, field, g);
            drawTrendCurveRange(tft, cfg, *tr, g, scale, (uint16_t)(tr->drawnCount - 1U), (uint16_t)(tr->count - 1U));
            tr->drawnCount = tr->count;
        }
        else
        {
            drawTrendHeaderLive(tft, field, g);
            drawTrendPlotBands(tft, cfg, *tr, g, scale);
            tr->drawnCount = tr->count;
        }
        tft.endBatch();

        field.dirty = false;
    }

    void drawFieldStatic(JWPLC_TFTClass &tft, FieldRuntime &field)
    {
        computeFieldGeometry(tft, field);

        const JWPLC_UIField &def = *field.def;
        field.staticDirty = false;

        // TREND dibuja su propio fondo, marco, ejes y encabezado en drawTrendField().
        if (def.meta.type == JWPLC_UI_FIELD_TREND)
        {
            TrendRuntime *tr = ensureTrendRuntime(field);
            if (tr != nullptr)
            {
                tr->fullRedraw = true;
                tr->headerOnly = false;
                tr->drawnCount = 0U;
            }
            field.dirty = true;
            return;
        }

        const uint16_t background = fieldBackground(field);
        // ICON sin fondo: ni recuadro ni fondo detrás de la etiqueta.
        const bool transparent = def.meta.type == JWPLC_UI_FIELD_ICON && def.iconTransparent;

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
        if (def.meta.type == JWPLC_UI_FIELD_BAR || def.meta.type == JWPLC_UI_FIELD_CIRCULAR)
        {
            field.u.bar.drawnW = 0;
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

        if (def.meta.type == JWPLC_UI_FIELD_BAR)
        {
            const int16_t minDim = (field.valueW < field.valueH) ? field.valueW : field.valueH;
            int16_t r = (def.barConfig.cornerRadius == 255)
                            ? (minDim / 2)
                            : (int16_t)def.barConfig.cornerRadius;
            if (r > minDim / 2) r = minDim / 2;

            if (r > 0)
            {
                tft.fillRoundRect(
                    field.valueX,
                    field.valueY,
                    field.valueW,
                    field.valueH,
                    r,
                    def.style.colors.background);
            }

            if (def.barConfig.borderEnabled && def.barConfig.borderWidth > 0)
            {
                const uint16_t bCol = def.barConfig.borderColor;
                const int16_t bThick = (int16_t)def.barConfig.borderWidth;
                for (int16_t i = 0; i < bThick; ++i)
                {
                    const int16_t curW = field.valueW - 2 * i;
                    const int16_t curH = field.valueH - 2 * i;
                    if (curW <= 0 || curH <= 0) break;
                    if (r > 0)
                    {
                        const int16_t curR = (r - i > 0) ? (r - i) : 1;
                        tft.drawRoundRect(field.valueX + i, field.valueY + i, curW, curH, curR, bCol);
                    }
                    else
                    {
                        tft.drawRect(field.valueX + i, field.valueY + i, curW, curH, bCol);
                    }
                }
            }
        }

        if (def.text.label != nullptr && def.text.label[0] != '\0')
        {
            uint16_t labelW = 0;
            uint16_t labelH = 0;
            textBounds(
                tft,
                def.text.label,
                def.style.labelTextSize,
                labelW,
                labelH,
                def.style.labelFont);
            if (def.style.labelFont != nullptr && def.style.labelFont->kind != 0)
            {
                tft.setFont(*def.style.labelFont);
            }
            else
            {
                tft.setFont(JWPLC_FONT_GLCD);
            }
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
            int16_t lx = def.rect.x + FIELD_PADDING;
            int16_t ly = def.rect.y + FIELD_PADDING;
            if (def.meta.type == JWPLC_UI_FIELD_BAR &&
                def.barConfig.orientation == JWPLC_UI_BAR_VERTICAL &&
                def.style.layout == JWPLC_UI_LAYOUT_STACKED)
            {
                const int16_t lead = FIELD_PADDING;
                const int16_t pad = effectiveFieldPadding(def);
                const int16_t availW = field.fieldW - lead - pad;
                if (availW > (int16_t)labelW)
                {
                    lx = def.rect.x + lead + (availW - (int16_t)labelW) / 2;
                }
            }
            tft.setCursor(
                lx,
                ly);
            tft.print(def.text.label);
            tft.setFont(JWPLC_FONT_GLCD);
        }

        if (def.text.unit != nullptr && def.text.unit[0] != '\0')
        {
            uint16_t unitW = 0;
            uint16_t unitH = 0;
            textBounds(
                tft,
                def.text.unit,
                def.style.labelTextSize,
                unitW,
                unitH,
                def.style.labelFont);
            if (def.style.labelFont != nullptr && def.style.labelFont->kind != 0)
            {
                tft.setFont(*def.style.labelFont);
            }
            else
            {
                tft.setFont(JWPLC_FONT_GLCD);
            }
            tft.setTextSize(
                (def.style.labelTextSize == 0)
                    ? 1
                    : def.style.labelTextSize);
            tft.setTextColor(
                def.style.colors.label,
                background);
            int16_t ux = field.valueX + field.valueW + FIELD_GAP;
            int16_t uy = field.valueY;
            if (def.meta.type == JWPLC_UI_FIELD_BAR &&
                def.barConfig.orientation == JWPLC_UI_BAR_VERTICAL &&
                def.style.layout == JWPLC_UI_LAYOUT_STACKED)
            {
                const int16_t lead = FIELD_PADDING;
                const int16_t pad = effectiveFieldPadding(def);
                const int16_t availW = field.fieldW - lead - pad;
                ux = (availW > (int16_t)unitW)
                         ? (def.rect.x + lead + (availW - (int16_t)unitW) / 2)
                         : (def.rect.x + lead);
                uy = field.valueY + field.valueH + FIELD_GAP;
            }
            tft.setCursor(
                ux,
                uy);
            tft.print(def.text.unit);
            tft.setFont(JWPLC_FONT_GLCD);
        }

        field.dirty = true;
    }

    void drawFieldValue(JWPLC_TFTClass &tft, FieldRuntime &field)
    {
        const JWPLC_UIField &def = *field.def;
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
            field.u.bar.drawnW >= 0)
        {
            const int16_t fillLevel = barFillLevel(field);
            const int16_t oldLevel = field.u.bar.drawnW;
            const uint32_t printStartUs = profile ? micros() : 0U;

            drawBarDelta(tft, field, oldLevel, fillLevel);

            field.u.bar.drawnW = fillLevel;
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

        // CIRCULAR: dibuja arco, pista y porcentaje central.
        if (def.meta.type == JWPLC_UI_FIELD_CIRCULAR)
        {
            const uint32_t printStartUs = profile ? micros() : 0U;

            drawCircularGauge(tft, field, fieldBackground(field));
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

        // TREND: dibuja gráfica completa o actualización incremental de trazos.
        if (def.meta.type == JWPLC_UI_FIELD_TREND)
        {
            const uint32_t printStartUs = profile ? micros() : 0U;

            drawTrendField(tft, field);

            if (profile)
            {
                jwplcH3E1RecordFieldDraw(
                    def.meta.id,
                    (uint8_t)def.meta.type,
                    (uint16_t)field.fieldW,
                    (uint16_t)field.fieldH,
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

        if (def.meta.type == JWPLC_UI_FIELD_BAR)
        {
            const int16_t minDim = (field.valueW < field.valueH) ? field.valueW : field.valueH;
            int16_t r = (def.barConfig.cornerRadius == 255)
                            ? (minDim / 2)
                            : (int16_t)def.barConfig.cornerRadius;
            if (r > minDim / 2) r = minDim / 2;

            if (r > 0)
            {
                tft.fillRoundRect(
                    field.valueX,
                    field.valueY,
                    field.valueW,
                    field.valueH,
                    r,
                    valueBackground);
            }
            else
            {
                tft.fillRect(
                    field.valueX,
                    field.valueY,
                    field.valueW,
                    field.valueH,
                    valueBackground);
            }

            if (def.barConfig.borderEnabled && def.barConfig.borderWidth > 0)
            {
                const uint16_t bCol = def.barConfig.borderColor;
                const int16_t bThick = (int16_t)def.barConfig.borderWidth;
                for (int16_t i = 0; i < bThick; ++i)
                {
                    const int16_t curW = field.valueW - 2 * i;
                    const int16_t curH = field.valueH - 2 * i;
                    if (curW <= 0 || curH <= 0) break;
                    if (r > 0)
                    {
                        const int16_t curR = (r - i > 0) ? (r - i) : 1;
                        tft.drawRoundRect(field.valueX + i, field.valueY + i, curW, curH, curR, bCol);
                    }
                    else
                    {
                        tft.drawRect(field.valueX + i, field.valueY + i, curW, curH, bCol);
                    }
                }
            }
        }
        else
        {
            tft.fillRect(
                field.valueX,
                field.valueY,
                field.valueW,
                clearH,
                valueBackground);
        }

        if (profile)
        {
            clearUs =
                (uint32_t)(micros() - clearStartUs);
        }

        if (def.meta.type == JWPLC_UI_FIELD_BAR)
        {
            field.u.bar.drawnW = 0;
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
            const int16_t fillLevel = barFillLevel(field);

            const uint32_t printStartUs =
                profile ? micros() : 0U;

            drawBarDelta(tft, field, 0, fillLevel);

            field.u.bar.drawnW = fillLevel;

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

        if (def.style.valueFont != nullptr && def.style.valueFont->kind != 0)
        {
            tft.setFont(*def.style.valueFont);
        }
        else
        {
            tft.setFont(JWPLC_FONT_GLCD);
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

        tft.print(field.u.value);
        tft.setFont(JWPLC_FONT_GLCD);

        if (profile)
        {
            printUs =
                (uint32_t)(micros() - printStartUs);
        }

        field.dirty = false;

        if (profile)
        {
            const size_t length =
                strlen(field.u.value);

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
        jwplcUIRuntimeEnsureLinked();

        if ((fields == nullptr && count != 0) ||
            count > JWPLC_UI_MAX_FIELDS)
        {
            return false;
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
        }

        freeAllTrendRuntimes();
        g_fieldCount = 0;

        for (size_t i = 0; i < JWPLC_UI_MAX_FIELDS; ++i)
        {
            g_fields[i] = FieldRuntime();
        }

        const JWPLC_UIField *source = fields;
        if (count > 0 && !isFlashFieldArray(fields))
        {
            g_ownedDefs = (JWPLC_UIField *)malloc(count * sizeof(JWPLC_UIField));
            if (g_ownedDefs == nullptr)
            {
                return false;
            }
            memcpy(g_ownedDefs, fields, count * sizeof(JWPLC_UIField));
            source = g_ownedDefs;
        }

        for (size_t i = 0; i < count; ++i)
        {
            g_fields[i].def = &source[i];
            g_fields[i].dirty = true;
            if (source[i].meta.type == JWPLC_UI_FIELD_BAR ||
                source[i].meta.type == JWPLC_UI_FIELD_CIRCULAR)
            {
                g_fields[i].u.bar.drawnW = -1;
            }
            g_fieldCount++;
        }

        g_pageRedrawPending = true;
        requestCurrentViewRefresh();
        return true;
    }

    void clearFields()
    {
        freeAllTrendRuntimes();
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
            field->def->meta.type != JWPLC_UI_FIELD_VALUE ||
            field->def->format.kind == JWPLC_UI_VALUE_DATETIME)
        {
            return false;
        }

        char formatted[VALUE_BUFFER_SIZE] = {};
        const bool valid =
            JWPLCUIFormat::value(
                field->def->format,
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
            field->def->meta.type != JWPLC_UI_FIELD_TEXT)
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
            (field->def->options.textCapacity == 0)
                ? (VALUE_BUFFER_SIZE - 1)
                : field->def->options.textCapacity;

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
            field->def->meta.type == JWPLC_UI_FIELD_ICON)
        {
            return setIconState(fieldId, value ? 1 : 0);
        }

        if (field == nullptr ||
            field->def->meta.type != JWPLC_UI_FIELD_BOOL)
        {
            return false;
        }

        const char *text =
            value
                ? ((field->def->options.boolText.trueText != nullptr)
                       ? field->def->options.boolText.trueText
                       : "ON")
                : ((field->def->options.boolText.falseText != nullptr)
                       ? field->def->options.boolText.falseText
                       : "OFF");

        const bool stateChanged = field->boolState != value;
        field->boolState = value;

        if (stateChanged &&
            (field->def->options.boolText.stateColors & JWPLC_UI_BOOL_STATE_BACKGROUND))
        {
            field->staticDirty = true;
        }

        // Con color por estado, el mismo texto en ambos estados (p. ej. "●")
        // también debe repintarse al cambiar de estado.
        if (!setValueString(*field, text) &&
            stateChanged &&
            field->def->options.boolText.stateColors != 0)
        {
            markFieldDirty(*field);
        }
        return true;
    }

    bool setIconState(uint8_t fieldId, uint8_t state)
    {
        FieldRuntime *field = findField(fieldId);

        if (field == nullptr ||
            field->def->meta.type != JWPLC_UI_FIELD_ICON)
        {
            return false;
        }

        const uint8_t count =
            (field->def->icon != nullptr) ? field->def->icon->stateCount : 0;
        const bool valid = state < count;

        // Fuera de rango se ve el último estado (drawIcon recorta).
        if (field->hasValue && field->u.icon.state == state)
        {
            jwplcH3E1RecordSetter(fieldId, false);
            return valid;
        }

        field->u.icon.state = state;
        field->u.icon.animFrame = 0;
        field->u.icon.lastAnimTickMs = millis();
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

            if (!isFieldUsed(field) ||
                field.def->meta.type != JWPLC_UI_FIELD_VALUE ||
                field.def->format.kind != JWPLC_UI_VALUE_DATETIME)
            {
                continue;
            }

            // setValueString sólo marca dirty si el texto cambió: con HH:MM
            // la TFT se toca una vez por minuto.
            char text[VALUE_BUFFER_SIZE] = {};
            JWPLCUIFormat::dateTime(field.def->format, clock, text, sizeof(text));
            if (field.hasValue && strncmp(field.u.value, text, sizeof(field.u.value)) == 0)
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
            (field->def->meta.type != JWPLC_UI_FIELD_BAR && field->def->meta.type != JWPLC_UI_FIELD_CIRCULAR))
        {
            return false;
        }

        if (field->hasValue && field->u.bar.value == value)
        {
            jwplcH3E1RecordSetter(
                fieldId,
                false);
            return true;
        }

        field->u.bar.value = value;
        field->hasValue = true;
        markFieldDirty(*field);

        jwplcH3E1RecordSetter(
            fieldId,
            true);
        return true;
    }

    static bool setTrendChannels(uint8_t fieldId, float v1, float v2, float v3, uint8_t count)
    {
        FieldRuntime *field = findField(fieldId);

        if (field == nullptr || field->def->meta.type != JWPLC_UI_FIELD_TREND)
        {
            return false;
        }

        const bool unchanged = field->hasValue &&
                               field->u.trend.ch1 == v1 &&
                               (count < 2 || field->u.trend.ch2 == v2) &&
                               (count < 3 || field->u.trend.ch3 == v3);
        if (unchanged)
        {
            if (tickTrendField(*field, millis(), false))
            {
                if (field->u.trend.rt != nullptr)
                {
                    field->u.trend.rt->headerOnly = false;
                }
                markFieldDirty(*field);
                jwplcH3E1RecordSetter(fieldId, true);
            }
            else
            {
                jwplcH3E1RecordSetter(fieldId, false);
            }
            return true;
        }

        field->u.trend.ch1 = v1;
        if (count >= 2) field->u.trend.ch2 = v2;
        if (count >= 3) field->u.trend.ch3 = v3;
        field->hasValue = true;
        const bool sampled = tickTrendField(*field, millis(), true);
        if (field->u.trend.rt != nullptr)
        {
            if (sampled)
            {
                field->u.trend.rt->headerOnly = false;
            }
            else if (!field->dirty && !field->u.trend.rt->fullRedraw && field->u.trend.rt->hasDrawnScale)
            {
                field->u.trend.rt->headerOnly = true;
            }
        }
        markFieldDirty(*field);

        jwplcH3E1RecordSetter(fieldId, true);
        return true;
    }

    bool setTrend(uint8_t fieldId, float value)
    {
        return setTrendChannels(fieldId, value, 0.0f, 0.0f, 1);
    }

    bool setTrend(uint8_t fieldId, float v1, float v2)
    {
        return setTrendChannels(fieldId, v1, v2, 0.0f, 2);
    }

    bool setTrend(uint8_t fieldId, float v1, float v2, float v3)
    {
        return setTrendChannels(fieldId, v1, v2, v3, 3);
    }

    void setRefreshMode(JWPLC_UIRefreshMode mode)
    {
        jwplcUIRuntimeEnsureLinked();
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

    bool anyTrendSampleDue()
    {
        return anyTrendSampleDueImpl();
    }

    void tickDueTrends()
    {
        tickDueTrendsImpl();
    }

    void setPage(uint8_t page)
    {
        jwplcUIRuntimeEnsureLinked();

        if (g_page == page)
        {
            return;
        }

        g_page = page;
        g_pageRedrawPending = true;

        for (size_t i = 0; i < g_fieldCount; ++i)
        {
            if (isFieldUsed(g_fields[i]) &&
                g_fields[i].def->meta.page == g_page)
            {
                resetFieldRedrawState(g_fields[i]);
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
        resetFieldRedrawState(*field);
        markFieldDirty(*field);
    }

    void invalidateAll(bool redrawStatic)
    {
        for (size_t i = 0; i < g_fieldCount; ++i)
        {
            if (isFieldUsed(g_fields[i]))
            {
                resetFieldRedrawState(g_fields[i]);
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
            if (isFieldUsed(g_fields[i]) &&
                g_fields[i].def->meta.page == g_page)
            {
                resetFieldRedrawState(g_fields[i]);
            }
        }

        g_refreshRequested = true;
    }

    void drawStatic(JWPLC_TFTClass &tft)
    {
        for (size_t i = 0; i < g_fieldCount; ++i)
        {
            FieldRuntime &field = g_fields[i];

            if (!isFieldUsed(field) || field.def->meta.page != g_page)
            {
                continue;
            }

            drawFieldStatic(tft, field);
        }
    }

    void drawDirty(JWPLC_TFTClass &tft)
    {
        tickDueTrends();

        const bool profile =
            jwplcH3E1ProfilerEnabled();

        const uint32_t startUs =
            profile ? micros() : 0U;

        uint32_t dirtyCount = 0U;

        for (size_t i = 0; i < g_fieldCount; ++i)
        {
            FieldRuntime &field = g_fields[i];

            if (!isFieldUsed(field) ||
                field.def->meta.page != g_page ||
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

bool JWPLC_DisplayClass::setTrend(uint8_t fieldId, float value)
{
    return JWPLCUI::setTrend(fieldId, value);
}

bool JWPLC_DisplayClass::setTrend(uint8_t fieldId, float v1, float v2)
{
    return JWPLCUI::setTrend(fieldId, v1, v2);
}

bool JWPLC_DisplayClass::setTrend(uint8_t fieldId, float v1, float v2, float v3)
{
    return JWPLCUI::setTrend(fieldId, v1, v2, v3);
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
