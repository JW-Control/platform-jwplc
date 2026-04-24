#include "JWPLC_IdleScreen.h"

#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <cstring>
#include <cstdio>

namespace JWPLCIdleScreen
{
    static Adafruit_ST7789 *tft = nullptr;

    static StatusPanel g_panel;
    static StatusPanel g_lastPanel;

    // Si está activo, el layout IDLE se reconstruye por fases en varias
    // llamadas consecutivas a draw(), evitando una carga visual pesada
    // en un solo tick del runtime.
    static bool g_forceFullRedraw = true;
    static uint8_t g_fullRedrawPhase = 0;

    static uint8_t g_lastDI = 0xFF;
    static uint8_t g_lastDO = 0xFF;

    static bool g_lastRtcValid = false;
    static uint8_t g_lastRtcHour = 0xFF;
    static uint8_t g_lastRtcMinute = 0xFF;
    static uint8_t g_lastRtcSecond = 0xFF;
    static uint8_t g_lastRtcDay = 0xFF;
    static uint8_t g_lastRtcMonth = 0xFF;
    static uint16_t g_lastRtcYear = 0xFFFF;

    static char g_title[32] = "JWPLC Basic";

    // =====================================================
    // Debug temporal de profiling
    // =====================================================
    static bool g_profileIdlePhases = false;
    static bool g_profileBaseFramePendingPrint = false;

    static uint32_t g_profBaseFillScreenUs = 0;
    static uint32_t g_profBaseBorderUs = 0;
    static uint32_t g_profBaseDividersUs = 0;
    static uint32_t g_profBaseTitleUs = 0;
    static uint32_t g_profBaseTotalUs = 0;

    static uint32_t g_profPhase1TotalUs = 0;
    static uint32_t g_profPhase2TotalUs = 0;

    static uint32_t g_profUpdateIoUs = 0;
    static uint32_t g_profUpdateRtcUs = 0;
    static uint32_t g_profPhase3TotalUs = 0;
    static uint32_t g_profNormalPrintMs = 0;

    static uint32_t g_profUpdateRtcTimeUs = 0;
    static uint32_t g_profUpdateRtcDateUs = 0;

    static uint32_t g_profRtcHHUs = 0;
    static uint32_t g_profRtcMMUs = 0;
    static uint32_t g_profRtcSSUs = 0;

    // =====================================================
    // Layout
    // =====================================================
    static constexpr int SCREEN_W = 320;
    static constexpr int SCREEN_H = 170;

    static constexpr int BORDER_X = 0;
    static constexpr int BORDER_Y = 0;
    static constexpr int BORDER_W = 320;
    static constexpr int BORDER_H = 170;

    static constexpr int TITLE_X = 6;
    static constexpr int TITLE_Y = 4;

    static constexpr int LEFT_DIV_X = 122;

    static constexpr int STATUS_LABEL_X = 20;
    static constexpr int STATUS_BOX_X = 62;
    static constexpr int STATUS_Y0 = 20;
    static constexpr int STATUS_STEP_Y = 22;
    static constexpr int STATUS_BOX_W = 36;
    static constexpr int STATUS_BOX_H = 12;

    static constexpr int RTC_TIME_X = 8;
    static constexpr int RTC_TIME_Y = 138;
    static constexpr int RTC_DATE_X = 8;
    static constexpr int RTC_DATE_Y = 156;

    static constexpr int RTC_TIME_CHAR_W = 12; // fuente base Adafruit_GFX con textSize(2)
    static constexpr int RTC_HH_X = RTC_TIME_X;
    static constexpr int RTC_COLON1_X = RTC_HH_X + (2 * RTC_TIME_CHAR_W);
    static constexpr int RTC_MM_X = RTC_COLON1_X + RTC_TIME_CHAR_W;
    static constexpr int RTC_COLON2_X = RTC_MM_X + (2 * RTC_TIME_CHAR_W);
    static constexpr int RTC_SS_X = RTC_COLON2_X + RTC_TIME_CHAR_W;

    static constexpr int IN_HDR_X = 156;
    static constexpr int OUT_HDR_X = 252;
    static constexpr int IO_HDR_Y = 8;

    static constexpr int IO_ROW_Y0 = 33;
    static constexpr int IO_ROW_STEP = 16;

    static constexpr int IN_TEXT_X = 138;
    static constexpr int IN_BOX_X = 167;

    static constexpr int MID_DIV_X = 220;

    static constexpr int OUT_BOX_X = 238;
    static constexpr int OUT_TEXT_X = 280;

    static constexpr int IO_BOX_W = 36;
    static constexpr int IO_BOX_H = 12;

    // =====================================================
    // Colores
    // =====================================================
    static constexpr uint16_t C_BG = ST77XX_BLACK;
    static constexpr uint16_t C_TEXT = ST77XX_WHITE;
    static constexpr uint16_t C_BORDER = ST77XX_WHITE;
    static constexpr uint16_t C_DIVIDER = 0x39E7;
    static constexpr uint16_t C_TITLE = 0x7DFF;
    static constexpr uint16_t C_IN_ACTIVE = 0x867D;
    static constexpr uint16_t C_OK_GREEN = 0x5FE0;
    static constexpr uint16_t C_ERR_RED = ST77XX_RED;

    // =====================================================
    // Prototipos internos
    // =====================================================
    static void drawBox(int x, int y, int w, int h, bool on, uint16_t onColor);
    static void drawStatusItemStatic(uint8_t idx, const char *label);
    static void fillStatusItemState(uint8_t idx, bool on, uint16_t onColor);
    static void drawIoCellStatic(uint8_t index, bool isInput);
    static void fillIoCellState(uint8_t index, bool isInput, bool active);
    static void drawDividers();
    static void drawBaseFramePhase();
    static void drawStatusAndHeadersPhase();
    static void drawIORangePhase(uint8_t first, uint8_t last);
    static void drawRtcTimeStaticDecorations();
    static uint32_t printRtcTimeToken(int x, const char *token);
    static void updateStatusPanel();
    static void updateIO(const JWPLC_IOState *io);
    static void formatTime(const JWPLC_RTCState *rtc, char *out, size_t len);
    static void formatDate(const JWPLC_RTCState *rtc, char *out, size_t len);
    static void updateRTC(const JWPLC_RTCState *rtc);

    // =====================================================
    // Helpers básicos de dibujo
    // =====================================================
    static void drawBox(int x, int y, int w, int h, bool on, uint16_t onColor)
    {
        if (!tft)
            return;

        tft->fillRect(x, y, w, h, on ? onColor : C_BG);
        tft->drawRect(x, y, w, h, C_BORDER);
    }

    static void drawStatusItemStatic(uint8_t idx, const char *label)
    {
        if (!tft)
            return;

        int y_label = STATUS_Y0 + idx * STATUS_STEP_Y;

        tft->setTextSize(2);
        tft->setTextColor(C_TEXT, C_BG);
        tft->setCursor(STATUS_LABEL_X, y_label);
        tft->print(label);

        tft->drawRect(STATUS_BOX_X, y_label + 1, STATUS_BOX_W, STATUS_BOX_H, C_BORDER);
    }

    static void fillStatusItemState(uint8_t idx, bool on, uint16_t onColor)
    {
        if (!tft)
            return;

        int y_label = STATUS_Y0 + idx * STATUS_STEP_Y;
        uint16_t color = on ? onColor : C_BG;

        tft->fillRect(STATUS_BOX_X + 1, y_label + 2, STATUS_BOX_W - 2, STATUS_BOX_H - 2, color);
    }

    static void drawIoCell(uint8_t index, bool isInput, bool active)
    {
        if (!tft)
            return;

        int y = IO_ROW_Y0 + index * IO_ROW_STEP;

        tft->setTextSize(1);
        tft->setTextColor(C_TEXT, C_BG);

        if (isInput)
        {
            tft->setCursor(IN_TEXT_X, y + 2);
            tft->print("I0.");
            tft->print(index);

            drawBox(IN_BOX_X, y, IO_BOX_W, IO_BOX_H, active, C_IN_ACTIVE);
        }
        else
        {
            drawBox(OUT_BOX_X, y, IO_BOX_W, IO_BOX_H, active, C_OK_GREEN);

            tft->setCursor(OUT_TEXT_X, y + 2);
            tft->print("Q0.");
            tft->print(index);
        }
    }

    static void drawIoCellStatic(uint8_t index, bool isInput)
    {
        if (!tft)
            return;

        int y = IO_ROW_Y0 + index * IO_ROW_STEP;

        tft->setTextSize(1);
        tft->setTextColor(C_TEXT, C_BG);

        if (isInput)
        {
            tft->setCursor(IN_TEXT_X, y + 2);
            tft->print("I0.");
            tft->print(index);

            tft->drawRect(IN_BOX_X, y, IO_BOX_W, IO_BOX_H, C_BORDER);
        }
        else
        {
            tft->setCursor(OUT_TEXT_X, y + 2);
            tft->print("Q0.");
            tft->print(index);

            tft->drawRect(OUT_BOX_X, y, IO_BOX_W, IO_BOX_H, C_BORDER);
        }
    }

    static void fillIoCellState(uint8_t index, bool isInput, bool active)
    {
        if (!tft)
            return;

        int y = IO_ROW_Y0 + index * IO_ROW_STEP;
        uint16_t color = active ? (isInput ? C_IN_ACTIVE : C_OK_GREEN) : C_BG;

        if (isInput)
        {
            tft->fillRect(IN_BOX_X + 1, y + 1, IO_BOX_W - 2, IO_BOX_H - 2, color);
        }
        else
        {
            tft->fillRect(OUT_BOX_X + 1, y + 1, IO_BOX_W - 2, IO_BOX_H - 2, color);
        }
    }

    static void drawDividers()
    {
        if (!tft)
            return;

        tft->drawFastVLine(LEFT_DIV_X, 2, SCREEN_H - 4, C_DIVIDER);
        tft->drawFastVLine(MID_DIV_X, 2, 166, C_DIVIDER);
        tft->drawFastHLine(RTC_TIME_X - 2, RTC_TIME_Y - 6, LEFT_DIV_X - 12, C_DIVIDER);
    }

    // =====================================================
    // Fases de construcción del layout IDLE
    // =====================================================
    static void drawBaseFramePhase()
    {
        if (!tft)
            return;

        uint32_t tStart = micros();
        uint32_t t0 = 0;

        t0 = micros();
        tft->fillScreen(C_BG);
        g_profBaseFillScreenUs = micros() - t0;

        t0 = micros();
        tft->drawRect(BORDER_X, BORDER_Y, BORDER_W, BORDER_H, C_BORDER);
        g_profBaseBorderUs = micros() - t0;

        t0 = micros();
        drawDividers();
        g_profBaseDividersUs = micros() - t0;

        t0 = micros();
        tft->setTextSize(1);
        tft->setTextColor(C_TITLE, C_BG);
        tft->setCursor(TITLE_X, TITLE_Y);
        tft->print(g_title);

        drawRtcTimeStaticDecorations();

        g_profBaseTitleUs = micros() - t0;

        g_profBaseTotalUs = micros() - tStart;

        if (g_profileIdlePhases)
        {
            g_profileBaseFramePendingPrint = true;
        }
    }

    static void drawStatusAndHeadersPhase()
    {
        if (!tft)
            return;

        uint32_t tStart = micros();

        drawStatusItemStatic(0, "PWR");
        drawStatusItemStatic(1, "RUN");
        drawStatusItemStatic(2, "ERR");
        drawStatusItemStatic(3, "BUS");
        drawStatusItemStatic(4, "ETH");

        tft->setTextSize(2);
        tft->setTextColor(C_TEXT, C_BG);
        tft->setCursor(IN_HDR_X, IO_HDR_Y);
        tft->print("IN");

        tft->setCursor(OUT_HDR_X, IO_HDR_Y);
        tft->print("OUT");

        g_profPhase1TotalUs = micros() - tStart;
    }

    static void drawIORangePhase(uint8_t first, uint8_t last)
    {
        if (!tft)
            return;

        uint32_t tStart = micros();

        for (uint8_t i = first; i <= last; i++)
        {
            drawIoCellStatic(i, true);
            drawIoCellStatic(i, false);
        }

        g_profPhase2TotalUs = micros() - tStart;
    }

    // =====================================================
    // Actualizaciones parciales
    // =====================================================
    static uint32_t g_profUpdateStatusUs = 0;

    static void updateStatusPanel()
    {
        if (!tft)
            return;

        uint32_t tStart = micros();

        if (g_forceFullRedraw || g_panel.pwr != g_lastPanel.pwr)
            fillStatusItemState(0, g_panel.pwr, C_OK_GREEN);

        if (g_forceFullRedraw || g_panel.run != g_lastPanel.run)
            fillStatusItemState(1, g_panel.run, C_OK_GREEN);

        if (g_forceFullRedraw || g_panel.err != g_lastPanel.err)
            fillStatusItemState(2, g_panel.err, C_ERR_RED);

        if (g_forceFullRedraw || g_panel.bus != g_lastPanel.bus)
            fillStatusItemState(3, g_panel.bus, C_OK_GREEN);

        if (g_forceFullRedraw || g_panel.eth != g_lastPanel.eth)
            fillStatusItemState(4, g_panel.eth, C_OK_GREEN);

        g_lastPanel = g_panel;
        g_profUpdateStatusUs = micros() - tStart;
    }

    static void updateIO(const JWPLC_IOState *io)
    {
        if (!tft || !io)
            return;

        uint32_t tStart = micros();

        uint8_t di = io->di_logical_bank0;
        uint8_t doo = io->do_bank1;

        if (g_forceFullRedraw)
        {
            for (uint8_t i = 0; i < 8; i++)
            {
                fillIoCellState(i, true, (di >> i) & 0x01);
                fillIoCellState(i, false, (doo >> i) & 0x01);
            }

            g_lastDI = di;
            g_lastDO = doo;
            g_profUpdateIoUs = micros() - tStart;
            return;
        }

        uint8_t diffDI = g_lastDI ^ di;
        uint8_t diffDO = g_lastDO ^ doo;

        for (uint8_t i = 0; i < 8; i++)
        {
            if (diffDI & (1 << i))
            {
                fillIoCellState(i, true, (di >> i) & 0x01);
            }

            if (diffDO & (1 << i))
            {
                fillIoCellState(i, false, (doo >> i) & 0x01);
            }
        }

        g_lastDI = di;
        g_lastDO = doo;
        g_profUpdateIoUs = micros() - tStart;
    }

    static void formatTime(const JWPLC_RTCState *rtc, char *out, size_t len)
    {
        if (!rtc || !rtc->valid)
        {
            snprintf(out, len, "--:--:--");
            return;
        }

        snprintf(out, len, "%02u:%02u:%02u", rtc->hour, rtc->minute, rtc->second);
    }

    static void formatDate(const JWPLC_RTCState *rtc, char *out, size_t len)
    {
        if (!rtc || !rtc->valid)
        {
            snprintf(out, len, "----/--/--");
            return;
        }

        snprintf(out, len, "%04u/%02u/%02u", rtc->year, rtc->month, rtc->day);
    }

    static void drawRtcTimeStaticDecorations()
    {
        if (!tft)
            return;

        tft->setTextSize(2);
        tft->setTextColor(C_TEXT, C_BG);

        tft->setCursor(RTC_COLON1_X, RTC_TIME_Y);
        tft->print(":");

        tft->setCursor(RTC_COLON2_X, RTC_TIME_Y);
        tft->print(":");
    }

    static uint32_t printRtcTimeToken(int x, const char *token)
    {
        if (!tft)
            return 0;

        uint32_t tStart = micros();

        tft->setTextSize(2);
        tft->setTextColor(C_TEXT, C_BG);
        tft->setCursor(x, RTC_TIME_Y);
        tft->print(token);

        return micros() - tStart;
    }

    static void updateRTC(const JWPLC_RTCState *rtc)
    {
        if (!tft || !rtc)
            return;

        uint32_t tStart = micros();

        bool hhChanged =
            g_forceFullRedraw ||
            (rtc->valid != g_lastRtcValid) ||
            (rtc->hour != g_lastRtcHour);

        bool mmChanged =
            g_forceFullRedraw ||
            (rtc->valid != g_lastRtcValid) ||
            (rtc->minute != g_lastRtcMinute);

        bool ssChanged =
            g_forceFullRedraw ||
            (rtc->valid != g_lastRtcValid) ||
            (rtc->second != g_lastRtcSecond);

        bool dateChanged =
            g_forceFullRedraw ||
            (rtc->valid != g_lastRtcValid) ||
            (rtc->day != g_lastRtcDay) ||
            (rtc->month != g_lastRtcMonth) ||
            (rtc->year != g_lastRtcYear);

        uint32_t tTimeStart = micros();

        if (hhChanged)
        {
            char hhBuf[3];
            if (rtc->valid)
                snprintf(hhBuf, sizeof(hhBuf), "%02u", rtc->hour);
            else
                snprintf(hhBuf, sizeof(hhBuf), "--");

            g_profRtcHHUs = printRtcTimeToken(RTC_HH_X, hhBuf);
        }
        else
        {
            g_profRtcHHUs = 0;
        }

        if (mmChanged)
        {
            char mmBuf[3];
            if (rtc->valid)
                snprintf(mmBuf, sizeof(mmBuf), "%02u", rtc->minute);
            else
                snprintf(mmBuf, sizeof(mmBuf), "--");

            g_profRtcMMUs = printRtcTimeToken(RTC_MM_X, mmBuf);
        }
        else
        {
            g_profRtcMMUs = 0;
        }

        if (ssChanged)
        {
            char ssBuf[3];
            if (rtc->valid)
                snprintf(ssBuf, sizeof(ssBuf), "%02u", rtc->second);
            else
                snprintf(ssBuf, sizeof(ssBuf), "--");

            g_profRtcSSUs = printRtcTimeToken(RTC_SS_X, ssBuf);
        }
        else
        {
            g_profRtcSSUs = 0;
        }

        g_profUpdateRtcTimeUs = (hhChanged || mmChanged || ssChanged)
                                    ? (micros() - tTimeStart)
                                    : 0;

        uint32_t tDateStart = micros();

        if (dateChanged)
        {
            char dateBuf[16];
            formatDate(rtc, dateBuf, sizeof(dateBuf));

            tft->setTextSize(1);
            tft->setTextColor(C_TEXT, C_BG);
            tft->setCursor(RTC_DATE_X, RTC_DATE_Y);
            tft->print(dateBuf);

            g_profUpdateRtcDateUs = micros() - tDateStart;
        }
        else
        {
            g_profUpdateRtcDateUs = 0;
        }

        g_lastRtcValid = rtc->valid;
        g_lastRtcHour = rtc->hour;
        g_lastRtcMinute = rtc->minute;
        g_lastRtcSecond = rtc->second;
        g_lastRtcDay = rtc->day;
        g_lastRtcMonth = rtc->month;
        g_lastRtcYear = rtc->year;

        g_profUpdateRtcUs = micros() - tStart;
    }

    // =====================================================
    // API pública
    // =====================================================
    void begin(Adafruit_ST7789 *display)
    {
        tft = display;
        g_forceFullRedraw = true;
        g_fullRedrawPhase = 0;
    }

    void setTitle(const char *title)
    {
        if (!title)
            return;

        strncpy(g_title, title, sizeof(g_title) - 1);
        g_title[sizeof(g_title) - 1] = '\0';
        g_forceFullRedraw = true;
        g_fullRedrawPhase = 0;
    }

    void setStatusPanel(const StatusPanel &panel)
    {
        g_panel = panel;
    }

    const StatusPanel &statusPanel()
    {
        return g_panel;
    }

    void forceFullRedraw()
    {
        g_forceFullRedraw = true;
        g_fullRedrawPhase = 0;
    }

    void draw(const JWPLC_IOState *io, const JWPLC_RTCState *rtc)
    {
        if (!tft)
            return;

        // Construcción escalonada del layout completo.
        // Cada llamada dibuja una parte y retorna rápido para no
        // bloquear tanto el resto del sistema.
        if (g_forceFullRedraw)
        {
            switch (g_fullRedrawPhase)
            {
            case 0:
                drawBaseFramePhase();
                g_fullRedrawPhase++;
                jwplcSystemMarkDisplayDirty();
                return;

            case 1:
                drawStatusAndHeadersPhase();
                g_fullRedrawPhase++;
                jwplcSystemMarkDisplayDirty();
                return;

            case 2:
                drawIORangePhase(0, 7);
                g_lastDI = 0xFF;
                g_lastDO = 0xFF;
                g_fullRedrawPhase++;
                jwplcSystemMarkDisplayDirty();
                return;

            case 3:
            {
                uint32_t tStart = micros();

                updateStatusPanel();
                updateIO(io);
                updateRTC(rtc);

                g_profPhase3TotalUs = micros() - tStart;

                if (g_profileIdlePhases && g_profileBaseFramePendingPrint)
                {
                    Serial.printf(
                        "[IDLE PROFILE] phase0 total=%lu us | fillScreen=%lu us | border=%lu us | dividers=%lu us | title=%lu us | phase1=%lu us | phase2=%lu us | phase3=%lu us\r\n",
                        (unsigned long)g_profBaseTotalUs,
                        (unsigned long)g_profBaseFillScreenUs,
                        (unsigned long)g_profBaseBorderUs,
                        (unsigned long)g_profBaseDividersUs,
                        (unsigned long)g_profBaseTitleUs,
                        (unsigned long)g_profPhase1TotalUs,
                        (unsigned long)g_profPhase2TotalUs,
                        (unsigned long)g_profPhase3TotalUs);

                    g_profileBaseFramePendingPrint = false;
                    g_profileIdlePhases = false;
                }

                g_forceFullRedraw = false;
                g_fullRedrawPhase = 0;
                return;
            }
            }
        }

        updateStatusPanel();
        updateIO(io);
        updateRTC(rtc);

        uint32_t nowMs = millis();
        if ((uint32_t)(nowMs - g_profNormalPrintMs) >= 500)
        {
            g_profNormalPrintMs = nowMs;
            Serial.printf(
                "[IDLE NORMAL] status=%lu us | io=%lu us | rtc=%lu us (time=%lu us, date=%lu us | hh=%lu us, mm=%lu us, ss=%lu us)\r\n",
                (unsigned long)g_profUpdateStatusUs,
                (unsigned long)g_profUpdateIoUs,
                (unsigned long)g_profUpdateRtcUs,
                (unsigned long)g_profUpdateRtcTimeUs,
                (unsigned long)g_profUpdateRtcDateUs,
                (unsigned long)g_profRtcHHUs,
                (unsigned long)g_profRtcMMUs,
                (unsigned long)g_profRtcSSUs
            );
        }
    }
}
