#include "JWPLC_UI_EventLog.h"
#include "JWPLC_UI.h"
#include "JWPLC_UI_Pages.h"
#include "JWPLC_TFT.h"
#include <JWPLC_GlobalPeripherals.h>
#include <JW_SD.h>
#include <JW_FRAM.h>
#include <JW_RTC.h>
#include <stdio.h>
#include <string.h>

namespace
{
    static constexpr uint8_t MAX_CAPACITY = 128;
    static constexpr uint8_t VISIBLE_ROWS = 10;
    static constexpr int16_t ROW_HEIGHT = 14;
    static constexpr int16_t HEADER_HEIGHT = 17;
    static constexpr int16_t FOOTER_HEIGHT = 13;

    static JWPLC_UIEventLog g_config;
    static bool g_configured = false;

    // Buffer circular en RAM (FIFO con sobrescritura al llenarse)
    static JWPLC_UIEventRecord g_records[MAX_CAPACITY];
    static size_t g_count = 0;
    static size_t g_head = 0; // Índice donde se inserta el siguiente

    // Estado visual y de navegación
    static int16_t g_selectedIndex = 0;  // 0 = más reciente
    static int16_t g_scrollOffset = 0;   // Primer índice visible
    static bool g_inDetail = false;
    static bool g_dirty = false;
    static bool g_fullRedraw = true;

    // MicroSD DataLog
    static JWPLCDataLog g_datalog;
    static bool g_datalogStarted = false;

    // FRAM
    static uint8_t g_framHeadSlot = 0;

    uint8_t visibleRowsCount()
    {
        const int16_t h = g_config.height > 0 ? g_config.height : 170;
        const int16_t border = g_config.frame ? 1 : 0;
        const int16_t availH = h - 2 * border - HEADER_HEIGHT - FOOTER_HEIGHT;
        if (availH <= 0) return 1;
        const int16_t rows = availH / ROW_HEIGHT;
        return rows > 0 ? (uint8_t)rows : 1;
    }

    const char *severityName(uint8_t sev)
    {
        switch (sev)
        {
        case JWPLC_EVENT_SEV_CRIT: return "CRITICA";
        case JWPLC_EVENT_SEV_WARN: return "AVISO";
        case JWPLC_EVENT_SEV_INFO:
        default:                   return "INFO";
        }
    }

    const char *categoryName(uint8_t cat)
    {
        switch (cat)
        {
        case JWPLC_EVENT_CAT_ALARM:     return "ALARMA";
        case JWPLC_EVENT_CAT_INTERLOCK: return "INTERLOCK";
        case JWPLC_EVENT_CAT_SYSTEM:    return "SISTEMA";
        case JWPLC_EVENT_CAT_PROCESS:
        default:                        return "PROCESO";
        }
    }

    const char *transitionName(uint8_t trans)
    {
        switch (trans)
        {
        case JWPLC_EVENT_TRANS_NORMAL: return "CESO";
        case JWPLC_EVENT_TRANS_ACTIVE: return "ACTIVO";
        case JWPLC_EVENT_TRANS_ACK:    return "RECON";
        default:                       return "---";
        }
    }

    const char *sourceName(uint8_t src)
    {
        switch (src)
        {
        case JWPLC_EVENT_SRC_ETH:    return "ETHERNET";
        case JWPLC_EVENT_SRC_MODBUS: return "MODBUS";
        case JWPLC_EVENT_SRC_SD:     return "SD";
        case JWPLC_EVENT_SRC_RTC:    return "RTC";
        case JWPLC_EVENT_SRC_IO:     return "I/O";
        case JWPLC_EVENT_SRC_USER:
        default:                     return "SKETCH";
        }
    }

    const JWPLC_UIEventDescriptor *findDescriptor(uint16_t code)
    {
        if (g_config.descriptors == nullptr || g_config.descriptorCount == 0)
        {
            return nullptr;
        }
        for (size_t i = 0; i < g_config.descriptorCount; ++i)
        {
            if (g_config.descriptors[i].code == code)
            {
                return &g_config.descriptors[i];
            }
        }
        return nullptr;
    }

    uint32_t currentTimestamp()
    {
        if (JWPLC_RTC.isReady())
        {
            JW_RTC::DateTime dt = JWPLC_RTC.now();
            if (dt.valid)
            {
                return JW_RTC::toUnix(dt);
            }
        }
        // Fallback: uptime en segundos con bit más significativo en 1 para indicar relativo
        return (millis() / 1000UL) | 0x80000000UL;
    }

    void formatTime(uint32_t ts, char *out, size_t outSize)
    {
        if (ts & 0x80000000UL)
        {
            // Uptime relativo
            uint32_t sec = ts & 0x7FFFFFFFUL;
            uint32_t h = (sec / 3600UL) % 24;
            uint32_t m = (sec % 3600UL) / 60;
            uint32_t s = sec % 60;
            snprintf(out, outSize, "+%02u:%02u:%02u", (unsigned)h, (unsigned)m, (unsigned)s);
        }
        else
        {
            // Unix epoch
            JW_RTC::DateTime dt;
            if (JW_RTC::fromUnix(ts, dt))
            {
                snprintf(out, outSize, "%02u:%02u:%02u", dt.hour, dt.minute, dt.second);
            }
            else
            {
                snprintf(out, outSize, "--:--:--");
            }
        }
    }

    void formatDateTime(uint32_t ts, char *out, size_t outSize)
    {
        if (ts & 0x80000000UL)
        {
            uint32_t sec = ts & 0x7FFFFFFFUL;
            uint32_t d = sec / 86400UL;
            uint32_t h = (sec % 86400UL) / 3600UL;
            uint32_t m = (sec % 3600UL) / 60;
            uint32_t s = sec % 60;
            snprintf(out, outSize, "Up %ud %02u:%02u:%02u", (unsigned)d, (unsigned)h, (unsigned)m, (unsigned)s);
        }
        else
        {
            JW_RTC::DateTime dt;
            if (JW_RTC::fromUnix(ts, dt))
            {
                snprintf(out, outSize, "%04u-%02u-%02u %02u:%02u:%02u",
                         dt.year, dt.month, dt.day, dt.hour, dt.minute, dt.second);
            }
            else
            {
                snprintf(out, outSize, "Fecha invalida");
            }
        }
    }

    uint16_t severityColor(uint8_t sev)
    {
        switch (sev)
        {
        case JWPLC_EVENT_SEV_CRIT: return g_config.colors.sevCrit ? g_config.colors.sevCrit : 0xF800; // Rojo
        case JWPLC_EVENT_SEV_WARN: return g_config.colors.sevWarn ? g_config.colors.sevWarn : 0xFD20; // Ámbar
        case JWPLC_EVENT_SEV_INFO:
        default:                   return g_config.colors.sevInfo ? g_config.colors.sevInfo : 0x07E0; // Verde
        }
    }

    // Persistencia en FRAM (últimos 16 eventos)
    void framSaveRecord(const JWPLC_UIEventRecord &rec)
    {
        if (!g_config.enableFram || !JWPLC_FRAM.isReady())
        {
            return;
        }

        const uint32_t slotAddr = JWPLC_EVENT_FRAM_ADDR + 16 + ((uint32_t)g_framHeadSlot * sizeof(JWPLC_UIEventRecord));
        JWPLC_FRAM.write(slotAddr, (const uint8_t *)&rec, sizeof(JWPLC_UIEventRecord));

        g_framHeadSlot = (uint8_t)((g_framHeadSlot + 1) % JWPLC_EVENT_FRAM_MAX);

        // Actualizar cabecera en 0x1E00: [MAGIC 4B][COUNT 1B][HEAD 1B][RESERVED 10B]
        uint8_t header[16] = {0};
        const uint32_t magic = JWPLC_EVENT_FRAM_MAGIC;
        memcpy(header, &magic, 4);
        header[4] = (g_count > JWPLC_EVENT_FRAM_MAX) ? JWPLC_EVENT_FRAM_MAX : (uint8_t)g_count;
        header[5] = g_framHeadSlot;
        JWPLC_FRAM.write(JWPLC_EVENT_FRAM_ADDR, header, sizeof(header));
    }

    void framRestore()
    {
        if (!g_config.enableFram || !JWPLC_FRAM.isReady())
        {
            return;
        }

        uint8_t header[16] = {0};
        JWPLC_FRAM.read(JWPLC_EVENT_FRAM_ADDR, header, sizeof(header));

        uint32_t magic = 0;
        memcpy(&magic, header, 4);
        if (magic != JWPLC_EVENT_FRAM_MAGIC)
        {
            return;
        }

        const uint8_t savedCount = (header[4] > JWPLC_EVENT_FRAM_MAX) ? JWPLC_EVENT_FRAM_MAX : header[4];
        const uint8_t headSlot = header[5] % JWPLC_EVENT_FRAM_MAX;

        if (savedCount == 0)
        {
            return;
        }

        // Cargar en orden cronológico inverso hacia el ring buffer
        for (uint8_t i = 0; i < savedCount; ++i)
        {
            const uint8_t slot = (uint8_t)((headSlot + JWPLC_EVENT_FRAM_MAX - savedCount + i) % JWPLC_EVENT_FRAM_MAX);
            const uint32_t slotAddr = JWPLC_EVENT_FRAM_ADDR + 16 + ((uint32_t)slot * sizeof(JWPLC_UIEventRecord));
            JWPLC_UIEventRecord rec;
            if (JWPLC_FRAM.read(slotAddr, (uint8_t *)&rec, sizeof(rec)))
            {
                g_records[g_head] = rec;
                g_head = (g_head + 1) % g_config.ramCapacity;
                if (g_count < g_config.ramCapacity)
                {
                    g_count++;
                }
            }
        }
        g_framHeadSlot = headSlot;
    }

    // Escritura a MicroSD mediante JWPLCDataLog
    void sdLogRecord(const JWPLC_UIEventRecord &rec, const char *customText)
    {
        if (!g_config.enableSd || !JWPLC_SD.isReady())
        {
            return;
        }

        if (!g_datalogStarted)
        {
            const char *path = (g_config.sdFileName != nullptr && g_config.sdFileName[0] != '\0')
                                   ? g_config.sdFileName
                                   : "/EVENTOS.CSV";
            g_datalogStarted = g_datalog.begin(JWPLC_SD, path);
            if (!g_datalogStarted)
            {
                return;
            }
        }

        char timeStr[32];
        formatDateTime(rec.timestamp, timeStr, sizeof(timeStr));

        const JWPLC_UIEventDescriptor *desc = findDescriptor(rec.code);
        const char *text = (customText != nullptr && customText[0] != '\0')
                               ? customText
                               : ((desc != nullptr && desc->label != nullptr) ? desc->label : "Evento sin descripcion");

        char line[160];
        snprintf(line, sizeof(line), "%s,%s,%s,%u,%s,\"%s\"\r\n",
                 timeStr,
                 categoryName(rec.category),
                 severityName(rec.severity),
                 rec.code,
                 transitionName(rec.transition),
                 text);

        g_datalog.write(line);
    }
}

namespace JWPLCUIEventLog
{
    bool begin(const JWPLC_UIEventLog &config)
    {
        g_config = config;
        if (g_config.ramCapacity == 0 || g_config.ramCapacity > MAX_CAPACITY)
        {
            g_config.ramCapacity = JWPLC_EVENT_MAX_RAM_DEFAULT;
        }

        // Colores por defecto si están en 0
        if (g_config.colors.headerBackground == 0) g_config.colors.headerBackground = 0x2124;
        if (g_config.colors.headerText == 0)       g_config.colors.headerText = 0xFFFF;
        if (g_config.colors.rowBackground == 0)    g_config.colors.rowBackground = 0x0000;
        if (g_config.colors.rowBackgroundAlt == 0) g_config.colors.rowBackgroundAlt = 0x10A2;
        if (g_config.colors.rowText == 0)          g_config.colors.rowText = 0xC618;
        if (g_config.colors.selectBackground == 0) g_config.colors.selectBackground = 0x03FF;
        if (g_config.colors.selectText == 0)       g_config.colors.selectText = 0xFFFF;
        if (g_config.colors.sevInfo == 0)          g_config.colors.sevInfo = 0x07E0;
        if (g_config.colors.sevWarn == 0)          g_config.colors.sevWarn = 0xFD20;
        if (g_config.colors.sevCrit == 0)          g_config.colors.sevCrit = 0xF800;

        g_count = 0;
        g_head = 0;
        g_selectedIndex = 0;
        g_scrollOffset = 0;
        g_inDetail = false;
        g_configured = true;
        g_datalogStarted = false;

        framRestore();

        // Registrar evento de arranque del sistema
        logEvent(JWPLC_EVT_SYS_BOOT, JWPLC_EVENT_TRANS_ACTIVE, JWPLC_EVENT_CAT_SYSTEM, JWPLC_EVENT_SEV_INFO, "Arranque de sistema");

        return true;
    }

    void logEvent(uint16_t code, uint8_t transition, uint8_t category, uint8_t severity, const char *customText)
    {
        if (!g_configured)
        {
            return;
        }

        // Si hay descriptor registrado, adoptar su severidad y categoría por defecto
        const JWPLC_UIEventDescriptor *desc = findDescriptor(code);
        if (desc != nullptr)
        {
            if (severity == 0 && desc->severity != 0) severity = desc->severity;
            if (category == 3 && desc->category != 3) category = desc->category;
        }

        JWPLC_UIEventRecord rec;
        memset(&rec, 0, sizeof(rec));
        rec.timestamp = currentTimestamp();
        rec.code = code;
        rec.category = category;
        rec.transition = transition;
        rec.source = JWPLC_EVENT_SRC_USER;
        rec.severity = severity;

        // Inserción en buffer circular RAM
        g_records[g_head] = rec;
        g_head = (g_head + 1) % g_config.ramCapacity;
        if (g_count < g_config.ramCapacity)
        {
            g_count++;
        }

        // Persistencia inmediata no volátil
        framSaveRecord(rec);

        // Volcado a microSD
        sdLogRecord(rec, customText);

        g_dirty = true;
        if (isPageActive())
        {
            JWPLCUI::requestRefresh();
        }
    }

    size_t count()
    {
        return g_count;
    }

    bool getRecord(size_t index, JWPLC_UIEventRecord &outRecord)
    {
        if (index >= g_count)
        {
            return false;
        }
        // 0 = más reciente (g_head - 1)
        const size_t pos = (g_head + g_config.ramCapacity - 1 - index) % g_config.ramCapacity;
        outRecord = g_records[pos];
        return true;
    }

    void clear()
    {
        g_count = 0;
        g_head = 0;
        g_selectedIndex = 0;
        g_scrollOffset = 0;
        g_inDetail = false;
        g_dirty = true;
        if (isPageActive())
        {
            JWPLCUI::requestRefresh();
        }
    }

    bool isPageActive()
    {
        return g_configured && (JWPLCUI::currentPage() == g_config.page);
    }

    bool inDetail()
    {
        return g_inDetail;
    }

    void exitDetail()
    {
        if (g_inDetail)
        {
            g_inDetail = false;
            g_fullRedraw = true;
            g_dirty = true;
            JWPLCUI::requestRefresh();
        }
    }

    void service()
    {
        if (!isPageActive() || !JWPLCButtons::isReady())
        {
            return;
        }

        if (g_inDetail)
        {
            if (JWPLC_Buttons.pressed(BTN_OK) || JWPLC_Buttons.pressed(BTN_ESC))
            {
                exitDetail();
                JWPLCButtons::clearPendingInput();
            }
            return;
        }

        // Navegación en la lista
        const uint8_t visRows = visibleRowsCount();
        if (JWPLC_Buttons.pressed(BTN_DOWN))
        {
            if (g_selectedIndex + 1 < (int16_t)g_count)
            {
                g_selectedIndex++;
                if (g_selectedIndex >= g_scrollOffset + visRows)
                {
                    g_scrollOffset = g_selectedIndex - visRows + 1;
                }
                g_dirty = true;
                JWPLCUI::requestRefresh();
            }
        }
        else if (JWPLC_Buttons.pressed(BTN_UP))
        {
            if (g_selectedIndex > 0)
            {
                g_selectedIndex--;
                if (g_selectedIndex < g_scrollOffset)
                {
                    g_scrollOffset = g_selectedIndex;
                }
                g_dirty = true;
                JWPLCUI::requestRefresh();
            }
        }
        else if (JWPLC_Buttons.pressed(BTN_OK))
        {
            if (g_count > 0)
            {
                g_inDetail = true;
                g_fullRedraw = true;
                g_dirty = true;
                JWPLCButtons::clearPendingInput();
                JWPLCUI::requestRefresh();
            }
        }
    }

    void draw(JWPLC_TFTClass &tft)
    {
        if (!isPageActive())
        {
            return;
        }

        const JWPLC_UIEventLogColors &c = g_config.colors;
        const int16_t x0 = g_config.width > 0 ? g_config.x : 0;
        const int16_t y0 = g_config.height > 0 ? g_config.y : 0;
        const int16_t w = g_config.width > 0 ? g_config.width : 320;
        const int16_t h = g_config.height > 0 ? g_config.height : 170;
        const int16_t border = g_config.frame ? 1 : 0;
        const uint8_t visRows = visibleRowsCount();

        if (g_inDetail)
        {
            // Ventana modal de detalle centrado
            int16_t bw = (w >= 280) ? 280 : (w > 16 ? (w - 8) : w);
            int16_t bh = (h >= 130) ? 130 : (h > 16 ? (h - 8) : h);
            int16_t bx = x0 + (w - bw) / 2;
            int16_t by = y0 + (h - bh) / 2;

            tft.fillRect(bx, by, bw, bh, 0x10A2); // Fondo oscuro
            tft.drawRect(bx, by, bw, bh, 0xFFFF); // Borde blanco
            tft.fillRect(bx + 1, by + 1, bw - 2, 16, c.headerBackground);

            tft.setTextSize(1);
            tft.setTextColor(c.headerText, c.headerBackground);
            tft.setCursor(bx + 8, by + 4);
            tft.print("DETALLE DEL EVENTO");

            JWPLC_UIEventRecord rec;
            if (getRecord((size_t)g_selectedIndex, rec))
            {
                char dtStr[32];
                formatDateTime(rec.timestamp, dtStr, sizeof(dtStr));

                const JWPLC_UIEventDescriptor *desc = findDescriptor(rec.code);
                const char *label = (desc != nullptr && desc->label != nullptr) ? desc->label : "---";

                tft.setTextColor(0xFFFF, 0x10A2);
                tft.setCursor(bx + 10, by + 24);
                tft.printf("Momento:   %s", dtStr);

                tft.setCursor(bx + 10, by + 40);
                tft.printf("Codigo:    #%u", rec.code);

                tft.setCursor(bx + 140, by + 40);
                tft.setTextColor(severityColor(rec.severity), 0x10A2);
                tft.printf("Severidad: %s", severityName(rec.severity));

                tft.setTextColor(0xFFFF, 0x10A2);
                tft.setCursor(bx + 10, by + 56);
                tft.printf("Categoria: %s", categoryName(rec.category));

                tft.setCursor(bx + 140, by + 56);
                tft.printf("Estado:    %s", transitionName(rec.transition));

                tft.setCursor(bx + 10, by + 72);
                tft.printf("Fuente:    %s", sourceName(rec.source));

                tft.setCursor(bx + 10, by + 88);
                tft.printf("Desc:      %.28s", label);
            }

            // Botón de cierre
            tft.fillRect(bx + 80, by + 108, 120, 16, c.selectBackground);
            tft.setTextColor(c.selectText, c.selectBackground);
            tft.setCursor(bx + 92, by + 112);
            tft.print("[ OK / ESC: CERRAR ]");
            return;
        }

        // Marco exterior opcional
        if (g_config.frame)
        {
            tft.drawRect(x0, y0, w, h, c.headerBackground);
        }

        // Encabezado
        tft.fillRect(x0 + border, y0 + border, w - 2 * border, HEADER_HEIGHT, c.headerBackground);
        tft.setTextSize(1);
        tft.setTextColor(c.headerText, c.headerBackground);
        tft.setCursor(x0 + border + 6, y0 + border + 4);
        tft.print(w >= 220 ? "HISTORIAL DE EVENTOS" : "HISTORIAL");

        // Contador e indicador de SD
        if (w >= 180)
        {
            char countBuf[16];
            snprintf(countBuf, sizeof(countBuf), "%02d/%02d", (int)(g_count > 0 ? (g_selectedIndex + 1) : 0), (int)g_count);
            tft.setCursor(x0 + border + w - (w >= 240 ? 110 : 70), y0 + border + 4);
            tft.print(countBuf);
        }

        if (w >= 240)
        {
            const bool sdOk = JWPLC_SD.isReady();
            tft.setTextColor(sdOk ? 0x07E0 : 0x632C, c.headerBackground);
            tft.setCursor(x0 + border + w - 50, y0 + border + 4);
            tft.print(sdOk ? "SD:OK" : "SD:--");
        }

        // Filas visibles
        for (uint8_t r = 0; r < visRows; ++r)
        {
            const int16_t y = y0 + border + HEADER_HEIGHT + (r * ROW_HEIGHT);
            const size_t recIdx = (size_t)(g_scrollOffset + r);
            const bool isSelected = (recIdx == (size_t)g_selectedIndex) && (g_count > 0);

            const uint16_t rowBg = isSelected ? c.selectBackground : ((r % 2 == 0) ? c.rowBackground : c.rowBackgroundAlt);
            const uint16_t rowFg = isSelected ? c.selectText : c.rowText;

            tft.fillRect(x0 + border, y, w - 2 * border, ROW_HEIGHT, rowBg);

            JWPLC_UIEventRecord rec;
            if (getRecord(recIdx, rec))
            {
                // Barra lateral de severidad (ancho 3px)
                tft.fillRect(x0 + border, y, 3, ROW_HEIGHT, severityColor(rec.severity));

                // Hora
                char timeBuf[16];
                formatTime(rec.timestamp, timeBuf, sizeof(timeBuf));
                tft.setTextColor(rowFg, rowBg);
                tft.setCursor(x0 + border + 6, y + 3);
                tft.print(timeBuf);

                if (w >= 120)
                {
                    // Código
                    char codeBuf[10];
                    snprintf(codeBuf, sizeof(codeBuf), "#%03u", rec.code);
                    tft.setCursor(x0 + border + 60, y + 3);
                    tft.print(codeBuf);
                }

                if (w >= 165)
                {
                    // Estado de transición
                    tft.setCursor(x0 + border + 98, y + 3);
                    tft.print(transitionName(rec.transition));
                }

                if (w >= 210)
                {
                    // Descripción
                    const JWPLC_UIEventDescriptor *desc = findDescriptor(rec.code);
                    const char *label = (desc != nullptr && desc->label != nullptr) ? desc->label : "---";
                    tft.setCursor(x0 + border + 138, y + 3);
                    char truncDesc[32];
                    int maxChars = (w - 145) / 6;
                    if (maxChars > 26) maxChars = 26;
                    if (maxChars < 1) maxChars = 1;
                    snprintf(truncDesc, sizeof(truncDesc), "%.*s", maxChars, label);
                    tft.print(truncDesc);
                }
            }
        }

        // Pie de página con ayuda de teclas
        const int16_t footerY = y0 + h - border - FOOTER_HEIGHT;
        tft.fillRect(x0 + border, footerY, w - 2 * border, FOOTER_HEIGHT, 0x10A2);
        tft.setTextColor(0xFFFF, 0x10A2);
        tft.setCursor(x0 + border + 8, footerY + 3);
        if (w >= 260)
        {
            tft.print("[OK] Detalle     [\x1E\x1F] Desplazar     [ESC] Salir");
        }
        else
        {
            tft.print("[OK] [\x1E\x1F] [ESC]");
        }
    }
}

// ----------------------------------------------------------------------
// Hooks strong para JWPLC_UI_API.cpp
// ----------------------------------------------------------------------

extern "C" void jwplcUIEventLogServiceInput(void)
{
    JWPLCUIEventLog::service();
}

extern "C" void jwplcUIEventLogPrepareEnter(void)
{
    if (JWPLCUIEventLog::isPageActive())
    {
        JWPLCButtons::clearPendingInput();
    }
}

extern "C" void jwplcUIEventLogInvalidateAll(void)
{
    g_dirty = true;
}

extern "C" void jwplcUIEventLogDrawStatic(JWPLC_TFTClass *tft)
{
    if (tft != nullptr && JWPLCUIEventLog::isPageActive())
    {
        JWPLCUIEventLog::draw(*tft);
    }
}

extern "C" void jwplcUIEventLogDrawDirty(JWPLC_TFTClass *tft)
{
    if (tft != nullptr && JWPLCUIEventLog::isPageActive() && g_dirty)
    {
        g_dirty = false;
        JWPLCUIEventLog::draw(*tft);
    }
}
