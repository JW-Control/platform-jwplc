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

    static bool g_forceFullRedraw = true;

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
    // Layout
    // =====================================================
    static constexpr int SCREEN_W = 320;
    static constexpr int SCREEN_H = 170;

    // Borde general
    static constexpr int BORDER_X = 0;
    static constexpr int BORDER_Y = 0;
    static constexpr int BORDER_W = 320;
    static constexpr int BORDER_H = 170;

    // Título pequeño
    static constexpr int TITLE_X = 6;
    static constexpr int TITLE_Y = 4;

    // Bloque izquierdo: estados + RTC
    static constexpr int LEFT_DIV_X = 122;

    static constexpr int STATUS_LABEL_X = 20;
    static constexpr int STATUS_BOX_X = 62;
    static constexpr int STATUS_Y0 = 20;
    static constexpr int STATUS_STEP_Y = 22;
    static constexpr int STATUS_BOX_W = 36;
    static constexpr int STATUS_BOX_H = 12;

    // RTC abajo a la izquierda
    static constexpr int RTC_TIME_X = 8;
    static constexpr int RTC_TIME_Y = 138;
    static constexpr int RTC_DATE_X = 8;
    static constexpr int RTC_DATE_Y = 156;

    // Bloque derecho: IN / OUT
    static constexpr int IN_HDR_X = 156;  // POSICION HORIZONTAL DEL TEXTO "IN" (el "OUT" se alinea a la derecha del bloque)
    static constexpr int OUT_HDR_X = 252; // POSICION HORIZONTAL DEL TEXTO "OUT" (el "IN" se alinea a la izquierda del bloque)
    static constexpr int IO_HDR_Y = 8;    // POSICION VERTICAL DE LOS TEXTOS "IN" y "OUT"

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
    static constexpr uint16_t C_DIVIDER = 0x39E7;   // gris/azulado tenue
    static constexpr uint16_t C_TITLE = 0x7DFF;     // cian claro
    static constexpr uint16_t C_IN_ACTIVE = 0x867D; // azul claro
    static constexpr uint16_t C_OK_GREEN = 0x5FE0;  // verde brillante
    static constexpr uint16_t C_ERR_RED = ST77XX_RED;

    // =====================================================
    // Helpers
    // =====================================================
    static void drawBox(int x, int y, int w, int h, bool on, uint16_t onColor)
    {
        if (!tft)
            return;

        tft->fillRect(x, y, w, h, on ? onColor : C_BG);
        tft->drawRect(x, y, w, h, C_BORDER);
    }

    static void drawStatusItem(uint8_t idx, const char *label, bool on, uint16_t onColor)
    {
        if (!tft)
            return;

        int y_label = STATUS_Y0 + idx * STATUS_STEP_Y;

        tft->setTextSize(2);
        tft->setTextColor(C_TEXT, C_BG);
        tft->setCursor(STATUS_LABEL_X, y_label);
        tft->print(label);

        drawBox(STATUS_BOX_X, y_label + 1, STATUS_BOX_W, STATUS_BOX_H, on, onColor);
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

    static void drawDividers()
    {
        if (!tft)
            return;

        // Divisor entre estados/RTC y E/S
        tft->drawFastVLine(LEFT_DIV_X, 2, SCREEN_H - 4, C_DIVIDER);

        // Divisor entre IN y OUT
        tft->drawFastVLine(MID_DIV_X, 2, 166, C_DIVIDER);

        // Línea horizontal tenue sobre el RTC
        tft->drawFastHLine(RTC_TIME_X - 2, RTC_TIME_Y - 6, LEFT_DIV_X - 12, C_DIVIDER);
    }

    static void drawStaticLayout()
    {
        if (!tft)
            return;

        tft->fillScreen(C_BG);
        tft->drawRect(BORDER_X, BORDER_Y, BORDER_W, BORDER_H, C_BORDER);

        drawDividers();

        // Título pequeño
        tft->setTextSize(1);
        tft->setTextColor(C_TITLE, C_BG);
        tft->setCursor(TITLE_X, TITLE_Y);
        tft->print(g_title);

        // Indicadores laterales
        drawStatusItem(0, "PWR", g_panel.pwr, C_OK_GREEN);
        drawStatusItem(1, "RUN", g_panel.run, C_OK_GREEN);
        drawStatusItem(2, "ERR", g_panel.err, C_ERR_RED);
        drawStatusItem(3, "BUS", g_panel.bus, C_OK_GREEN);
        drawStatusItem(4, "ETH", g_panel.eth, C_OK_GREEN);

        // Encabezados IN / OUT
        tft->setTextSize(2);
        tft->setTextColor(C_TEXT, C_BG);
        tft->setCursor(IN_HDR_X, IO_HDR_Y);
        tft->print("IN");

        tft->setCursor(OUT_HDR_X, IO_HDR_Y);
        tft->print("OUT");

        // Base de celdas de IO
        for (uint8_t i = 0; i < 8; i++)
        {
            drawIoCell(i, true, false);
            drawIoCell(i, false, false);
        }

        g_lastDI = 0xFF;
        g_lastDO = 0xFF;
    }

    static void updateStatusPanel()
    {
        if (!tft)
            return;

        if (g_forceFullRedraw || g_panel.pwr != g_lastPanel.pwr)
            drawStatusItem(0, "PWR", g_panel.pwr, C_OK_GREEN);

        if (g_forceFullRedraw || g_panel.run != g_lastPanel.run)
            drawStatusItem(1, "RUN", g_panel.run, C_OK_GREEN);

        if (g_forceFullRedraw || g_panel.err != g_lastPanel.err)
            drawStatusItem(2, "ERR", g_panel.err, C_ERR_RED);

        if (g_forceFullRedraw || g_panel.bus != g_lastPanel.bus)
            drawStatusItem(3, "BUS", g_panel.bus, C_OK_GREEN);

        if (g_forceFullRedraw || g_panel.eth != g_lastPanel.eth)
            drawStatusItem(4, "ETH", g_panel.eth, C_OK_GREEN);

        g_lastPanel = g_panel;
    }

    static void updateIO(const JWPLC_IOState *io)
    {
        if (!tft || !io)
            return;

        uint8_t di = io->di_logical_bank0;
        uint8_t doo = io->do_bank1;

        if (g_forceFullRedraw)
        {
            for (uint8_t i = 0; i < 8; i++)
            {
                drawIoCell(i, true, (di >> i) & 0x01);
                drawIoCell(i, false, (doo >> i) & 0x01);
            }

            g_lastDI = di;
            g_lastDO = doo;
            return;
        }

        uint8_t diffDI = g_lastDI ^ di;
        uint8_t diffDO = g_lastDO ^ doo;

        for (uint8_t i = 0; i < 8; i++)
        {
            if (diffDI & (1 << i))
            {
                drawIoCell(i, true, (di >> i) & 0x01);
            }

            if (diffDO & (1 << i))
            {
                drawIoCell(i, false, (doo >> i) & 0x01);
            }
        }

        g_lastDI = di;
        g_lastDO = doo;
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

    static void updateRTC(const JWPLC_RTCState *rtc)
    {
        if (!tft || !rtc)
            return;

        bool changed =
            g_forceFullRedraw ||
            (rtc->valid != g_lastRtcValid) ||
            (rtc->hour != g_lastRtcHour) ||
            (rtc->minute != g_lastRtcMinute) ||
            (rtc->second != g_lastRtcSecond) ||
            (rtc->day != g_lastRtcDay) ||
            (rtc->month != g_lastRtcMonth) ||
            (rtc->year != g_lastRtcYear);

        if (!changed)
            return;

        char timeBuf[16];
        char dateBuf[16];
        formatTime(rtc, timeBuf, sizeof(timeBuf));
        formatDate(rtc, dateBuf, sizeof(dateBuf));

        // Limpiar solo zona RTC
        tft->fillRect(RTC_TIME_X - 2, RTC_TIME_Y - 1, LEFT_DIV_X - 12, 28, C_BG);

        // Hora pequeña-mediana
        tft->setTextSize(2);
        tft->setTextColor(C_TEXT, C_BG);
        tft->setCursor(RTC_TIME_X, RTC_TIME_Y);
        tft->print(timeBuf);

        // Fecha pequeña
        tft->setTextSize(1);
        tft->setCursor(RTC_DATE_X, RTC_DATE_Y);
        tft->print(dateBuf);

        g_lastRtcValid = rtc->valid;
        g_lastRtcHour = rtc->hour;
        g_lastRtcMinute = rtc->minute;
        g_lastRtcSecond = rtc->second;
        g_lastRtcDay = rtc->day;
        g_lastRtcMonth = rtc->month;
        g_lastRtcYear = rtc->year;
    }

    // =====================================================
    // API
    // =====================================================
    void begin(Adafruit_ST7789 *display)
    {
        tft = display;
        g_forceFullRedraw = true;
    }

    void setTitle(const char *title)
    {
        if (!title)
            return;

        strncpy(g_title, title, sizeof(g_title) - 1);
        g_title[sizeof(g_title) - 1] = '\0';
        g_forceFullRedraw = true;
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
    }

    void draw(const JWPLC_IOState *io, const JWPLC_RTCState *rtc)
    {
        if (!tft)
            return;

        if (g_forceFullRedraw)
        {
            drawStaticLayout();
        }

        updateStatusPanel();
        updateIO(io);
        updateRTC(rtc);

        g_forceFullRedraw = false;
    }
}