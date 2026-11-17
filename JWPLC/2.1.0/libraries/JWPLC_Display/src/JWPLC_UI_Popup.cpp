#include "JWPLC_UI_Popup.h"
#include "JWPLC_Display_API.h"
#include "JWPLC_UI.h"
#include "JWPLC_UI_EventLog.h"
#include "JWPLC_UI_RuntimeHooks.h"

#include <JWPLC_GlobalPeripherals.h>
#include <JWPLC_TFT.h>
#include <Arduino.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>

// Sistema de ventanas emergentes modales (I5-I6).
// Solo se enlaza si el sketch usa setPopups/showPopup/dismissPopup.

namespace
{
    static constexpr int16_t SCREEN_W = 320;
    static constexpr int16_t SCREEN_H = 170;
    static constexpr uint8_t HEADER_H = 22;
    static constexpr uint8_t CHAR_W = 6;
    static constexpr uint8_t CHAR_H = 8;
    static constexpr uint8_t LINE_H = 11;

    struct ActivePopupEntry
    {
        bool used = false;
        bool isAdHoc = false;
        uint8_t defIndex = JWPLC_UI_POPUP_NONE;
        JWPLC_UIPopup def;
        uint8_t focusIndex = 0; // 0 = cancelar / único botón; 1 = aceptar en CONFIRM
        uint32_t openedMs = 0;
        jwplcUIPopupConfirmCallback_t onAccept = nullptr;
        void *onAcceptCtx = nullptr;
        char runtimeTitle[36] = {};
        char runtimeCode[12] = {};
        char runtimeMessage[128] = {};
    };

    JWPLC_UIPopup g_popups[JWPLC_UI_POPUP_MAX_COUNT];
    bool g_prevTriggers[JWPLC_UI_POPUP_MAX_COUNT] = {};
    bool g_confirmedFlags[JWPLC_UI_POPUP_MAX_COUNT] = {};
    uint8_t g_popupCount = 0;

    ActivePopupEntry g_active;
    ActivePopupEntry g_queue[JWPLC_UI_POPUP_QUEUE_SIZE];
    uint8_t g_queueCount = 0;

    bool g_overlayDirty = false;
    bool g_restoreUnderlying = false;
    bool g_appliedErrLed = false;
    char g_prevErrCode[5] = {};
    bool g_savedPrevErrState = false;
    bool g_savedPrevErrLed = false;

    uint8_t presetPriority(JWPLC_UIPopupPreset preset)
    {
        switch (preset)
        {
        case JWPLC_UI_POPUP_CRIT:
            return 3;
        case JWPLC_UI_POPUP_CONFIRM:
            return 2;
        case JWPLC_UI_POPUP_WARN:
            return 1;
        case JWPLC_UI_POPUP_INFO:
        default:
            return 0;
        }
    }

    int findDefIndexById(uint8_t id)
    {
        for (uint8_t i = 0; i < g_popupCount; ++i)
        {
            if (g_popups[i].id == id)
                return (int)i;
        }
        return -1;
    }

    void copyBounded(char *dst, size_t size, const char *src)
    {
        if (dst == nullptr || size == 0)
            return;
        if (src == nullptr)
        {
            dst[0] = '\0';
            return;
        }
        strncpy(dst, src, size - 1);
        dst[size - 1] = '\0';
    }

    uint16_t parseEventCode(const char *codeStr, uint8_t id)
    {
        if (codeStr != nullptr && codeStr[0] != '\0')
        {
            unsigned long val = 0;
            bool hasDigit = false;
            for (const char *p = codeStr; *p != '\0'; ++p)
            {
                if (*p >= '0' && *p <= '9')
                {
                    hasDigit = true;
                    val = val * 10UL + (unsigned long)(*p - '0');
                }
            }
            if (hasDigit && val > 0 && val <= 65535UL)
                return (uint16_t)val;
        }
        return (uint16_t)(900U + id);
    }

    void logPopupTransition(const ActivePopupEntry &entry, uint8_t transition)
    {
        if (!entry.def.logToEventLog)
            return;

        uint8_t sev = JWPLC_EVENT_SEV_INFO;
        uint8_t cat = JWPLC_EVENT_CAT_SYSTEM;
        if (entry.def.preset == JWPLC_UI_POPUP_WARN)
        {
            sev = JWPLC_EVENT_SEV_WARN;
            cat = JWPLC_EVENT_CAT_ALARM;
        }
        else if (entry.def.preset == JWPLC_UI_POPUP_CRIT)
        {
            sev = JWPLC_EVENT_SEV_CRIT;
            cat = entry.def.latched ? JWPLC_EVENT_CAT_ALARM : JWPLC_EVENT_CAT_INTERLOCK;
        }
        else if (entry.def.preset == JWPLC_UI_POPUP_CONFIRM)
        {
            sev = JWPLC_EVENT_SEV_INFO;
            cat = JWPLC_EVENT_CAT_PROCESS;
        }

        const char *label = (entry.def.title != nullptr && entry.def.title[0] != '\0')
                                ? entry.def.title
                                : entry.def.message;
        const uint16_t evtCode = parseEventCode(entry.def.code, entry.def.id);
        JWPLC_Display.logEvent(evtCode, transition, cat, sev, label);
    }

    void syncErrIndicatorsForActive()
    {
        if (g_active.used && g_active.def.preset == JWPLC_UI_POPUP_CRIT)
        {
            if (!g_savedPrevErrState)
            {
                g_savedPrevErrState = true;
                g_savedPrevErrLed = JWPLC_Display.errLed();
                copyBounded(g_prevErrCode, sizeof(g_prevErrCode), JWPLC_Display.errCode());
            }
            JWPLC_Display.setErrLed(true);
            g_appliedErrLed = true;
            if (g_active.def.errCode != nullptr && g_active.def.errCode[0] != '\0')
            {
                JWPLC_Display.setErrCode(g_active.def.errCode);
            }
            return;
        }

        if (g_appliedErrLed)
        {
            g_appliedErrLed = false;
            if (g_savedPrevErrState)
            {
                JWPLC_Display.setErrLed(g_savedPrevErrLed);
                if (g_prevErrCode[0] != '\0')
                    JWPLC_Display.setErrCode(g_prevErrCode);
                else
                    JWPLC_Display.setErrCode("");
                g_savedPrevErrState = false;
            }
        }
    }

    bool shouldShowInCurrentMode(const ActivePopupEntry &entry)
    {
        if (!entry.used)
            return false;
        if (JWPLC_Display.isIdleMode())
            return entry.def.showOverIdle || entry.def.preset == JWPLC_UI_POPUP_CRIT;
        return true;
    }

    void makeEntryFromDef(uint8_t defIndex, ActivePopupEntry &out)
    {
        out = ActivePopupEntry();
        out.used = true;
        out.isAdHoc = false;
        out.defIndex = defIndex;
        out.def = g_popups[defIndex];
        out.focusIndex = 0; // CONFIRM inicia en CANCELAR por seguridad (§G.10)
        out.openedMs = millis();
    }

    void makeAdHocEntry(
        JWPLC_UIPopupPreset preset,
        const char *title,
        const char *message,
        const char *code,
        uint16_t autoCloseMs,
        ActivePopupEntry &out)
    {
        out = ActivePopupEntry();
        out.used = true;
        out.isAdHoc = true;
        out.defIndex = JWPLC_UI_POPUP_NONE;
        out.def = JWPLC_UIPopup();
        out.def.id = JWPLC_UI_POPUP_NONE;
        out.def.preset = preset;
        out.def.colors = JWPLC_UIPopupPresetColors(preset);
        out.def.blocksIdle = (preset == JWPLC_UI_POPUP_CRIT || preset == JWPLC_UI_POPUP_CONFIRM);
        out.def.latched = (preset == JWPLC_UI_POPUP_CRIT || preset == JWPLC_UI_POPUP_CONFIRM);
        out.def.showOverIdle = true;
        out.def.autoCloseMs = autoCloseMs;
        out.def.acceptText = (preset == JWPLC_UI_POPUP_CONFIRM) ? "ACEPTAR" : "OK";
        out.def.cancelText = "CANCELAR";
        copyBounded(out.runtimeTitle, sizeof(out.runtimeTitle), title);
        copyBounded(out.runtimeMessage, sizeof(out.runtimeMessage), message);
        copyBounded(out.runtimeCode, sizeof(out.runtimeCode), code);
        out.def.title = out.runtimeTitle;
        out.def.message = out.runtimeMessage;
        out.def.code = out.runtimeCode[0] ? out.runtimeCode : nullptr;
        out.openedMs = millis();
    }

    bool alreadyActiveOrQueuedId(uint8_t id)
    {
        if (id == JWPLC_UI_POPUP_NONE)
            return false;
        if (g_active.used && !g_active.isAdHoc && g_active.def.id == id)
            return true;
        for (uint8_t i = 0; i < g_queueCount; ++i)
        {
            if (g_queue[i].used && !g_queue[i].isAdHoc && g_queue[i].def.id == id)
                return true;
        }
        return false;
    }

    void rebindEntryPointers(ActivePopupEntry &entry)
    {
        if (entry.isAdHoc)
        {
            entry.def.title = entry.runtimeTitle;
            entry.def.message = entry.runtimeMessage;
            entry.def.code = entry.runtimeCode[0] ? entry.runtimeCode : nullptr;
        }
    }

    bool enqueueOrActivate(const ActivePopupEntry &entry)
    {
        if (!entry.used)
            return false;

        if (!entry.isAdHoc && alreadyActiveOrQueuedId(entry.def.id))
            return true;

        if (!g_active.used)
        {
            g_active = entry;
            rebindEntryPointers(g_active);
            g_active.openedMs = millis();
            g_overlayDirty = true;
            logPopupTransition(g_active, JWPLC_EVENT_TRANS_ACTIVE);
            syncErrIndicatorsForActive();
            jwplcUIMenuCancelEdit();
            JWPLCButtons::clearPendingInput();
            jwplcSystemForceDisplayRefresh();
            return true;
        }

        const uint8_t incomingPrio = presetPriority(entry.def.preset);
        const uint8_t activePrio = presetPriority(g_active.def.preset);

        if (incomingPrio > activePrio && g_queueCount < JWPLC_UI_POPUP_QUEUE_SIZE)
        {
            // Desplaza hacia arriba respetando orden de llegada de los ya encolados
            for (uint8_t i = g_queueCount; i > 0; --i)
            {
                g_queue[i] = g_queue[i - 1];
                rebindEntryPointers(g_queue[i]);
            }
            g_queue[0] = g_active;
            rebindEntryPointers(g_queue[0]);
            g_queueCount++;

            g_active = entry;
            rebindEntryPointers(g_active);
            g_active.openedMs = millis();
            g_overlayDirty = true;
            logPopupTransition(g_active, JWPLC_EVENT_TRANS_ACTIVE);
            syncErrIndicatorsForActive();
            jwplcUIMenuCancelEdit();
            JWPLCButtons::clearPendingInput();
            jwplcSystemForceDisplayRefresh();
            return true;
        }

        if (g_queueCount >= JWPLC_UI_POPUP_QUEUE_SIZE)
            return false;

        uint8_t insertAt = g_queueCount;
        for (uint8_t i = 0; i < g_queueCount; ++i)
        {
            if (incomingPrio > presetPriority(g_queue[i].def.preset))
            {
                insertAt = i;
                break;
            }
        }
        for (uint8_t i = g_queueCount; i > insertAt; --i)
        {
            g_queue[i] = g_queue[i - 1];
            rebindEntryPointers(g_queue[i]);
        }
        g_queue[insertAt] = entry;
        rebindEntryPointers(g_queue[insertAt]);
        g_queueCount++;
        return true;
    }

    void closeActive(bool accepted)
    {
        if (!g_active.used)
            return;

        const ActivePopupEntry finished = g_active;

        if (finished.def.preset == JWPLC_UI_POPUP_CONFIRM)
        {
            if (finished.def.confirmOutVar != nullptr)
                *finished.def.confirmOutVar = accepted;
            if (accepted && finished.defIndex < JWPLC_UI_POPUP_MAX_COUNT)
                g_confirmedFlags[finished.defIndex] = true;
            logPopupTransition(finished, accepted ? JWPLC_EVENT_TRANS_ACK : JWPLC_EVENT_TRANS_NORMAL);
        }
        else
        {
            logPopupTransition(finished, JWPLC_EVENT_TRANS_ACK);
        }

        g_active = ActivePopupEntry();

        if (accepted && finished.onAccept != nullptr)
        {
            finished.onAccept(finished.onAcceptCtx);
        }

        if (accepted && finished.def.preset == JWPLC_UI_POPUP_CONFIRM)
        {
            if (finished.def.targetPage < 16U)
            {
                JWPLC_Display.setUserPage(finished.def.targetPage);
                if (finished.def.targetView < 8U)
                    JWPLC_Display.setPageView(finished.def.targetPage, finished.def.targetView);
            }
            else if (finished.def.targetView < 8U)
            {
                JWPLC_Display.setPageView(finished.def.targetView);
            }
        }

        if (g_queueCount > 0)
        {
            g_active = g_queue[0];
            rebindEntryPointers(g_active);
            g_active.openedMs = millis();
            for (uint8_t i = 1; i < g_queueCount; ++i)
            {
                g_queue[i - 1] = g_queue[i];
                rebindEntryPointers(g_queue[i - 1]);
            }
            g_queue[g_queueCount - 1] = ActivePopupEntry();
            g_queueCount--;

            g_overlayDirty = true;
            g_restoreUnderlying = true;
            logPopupTransition(g_active, JWPLC_EVENT_TRANS_ACTIVE);
        }
        else
        {
            g_overlayDirty = false;
            g_restoreUnderlying = true;
        }

        syncErrIndicatorsForActive();
        JWPLCButtons::clearPendingInput();
        jwplcSystemForceDisplayRefresh();
    }

    // ------------------------------------------------------------------
    // Geometría y dibujo (paridad exacta 320x170 con designer-popups.js)
    // ------------------------------------------------------------------

    void clampPopupRect(const JWPLC_UIPopup &def, int16_t &x, int16_t &y, int16_t &w, int16_t &h, int16_t &r)
    {
        w = (def.width >= 120 && def.width <= SCREEN_W - 8) ? def.width : 248;
        h = (def.height >= 72 && def.height <= SCREEN_H - 8) ? def.height : 116;
        x = (def.x >= 0 && def.x + w <= SCREEN_W) ? def.x : (int16_t)((SCREEN_W - w) / 2);
        y = (def.y >= 0 && def.y + h <= SCREEN_H) ? def.y : (int16_t)((SCREEN_H - h) / 2);
        r = (def.cornerRadius <= 12) ? (int16_t)def.cornerRadius : 6;
    }

    void drawHeaderIcon(JWPLC_TFTClass &tft, JWPLC_UIPopupPreset preset, int16_t ix, int16_t iy, uint16_t fg, uint16_t bg)
    {
        // Caja 14x14 limpia en la esquina izquierda de la barra de título.
        if (preset == JWPLC_UI_POPUP_INFO)
        {
            tft.drawRect(ix + 1, iy + 1, 12, 12, fg);
            tft.fillRect(ix + 6, iy + 3, 2, 2, fg);
            tft.fillRect(ix + 6, iy + 6, 2, 5, fg);
        }
        else if (preset == JWPLC_UI_POPUP_CONFIRM)
        {
            tft.drawRect(ix + 1, iy + 1, 12, 12, fg);
            tft.fillRect(ix + 5, iy + 3, 4, 2, fg);
            tft.fillRect(ix + 7, iy + 5, 2, 2, fg);
            tft.fillRect(ix + 6, iy + 6, 2, 2, fg);
            tft.fillRect(ix + 6, iy + 9, 2, 2, fg);
        }
        else
        {
            // WARN o CRIT: signo '!' en marco de alerta.
            tft.fillRect(ix + 1, iy + 1, 12, 12, bg);
            tft.drawRect(ix + 1, iy + 1, 12, 12, fg);
            tft.fillRect(ix + 6, iy + 3, 2, 5, fg);
            tft.fillRect(ix + 6, iy + 9, 2, 2, fg);
        }
    }

    void drawCenteredGlcd(JWPLC_TFTClass &tft, int16_t cx, int16_t cy, const char *text, uint16_t fg, uint16_t bg)
    {
        if (text == nullptr)
            return;
        const size_t len = strlen(text);
        const int16_t tw = (int16_t)(len * CHAR_W);
        tft.setTextColor(fg, bg);
        tft.setCursor((int16_t)(cx - tw / 2), (int16_t)(cy - CHAR_H / 2));
        tft.print(text);
    }

    void drawWrappedBody(JWPLC_TFTClass &tft, const char *msg, int16_t bx, int16_t by, int16_t bw, int16_t bh, uint16_t fg, uint16_t bg)
    {
        if (msg == nullptr || msg[0] == '\0')
            return;

        const int16_t maxChars = (bw / CHAR_W > 4) ? (int16_t)(bw / CHAR_W) : 4;
        const int16_t maxLines = (bh / LINE_H > 1) ? (int16_t)(bh / LINE_H) : 1;

        char lines[6][48];
        int16_t lineCount = 0;

        const char *p = msg;
        while (*p != '\0' && lineCount < maxLines && lineCount < 6)
        {
            while (*p == ' ')
                p++;
            if (*p == '\0')
                break;

            if (*p == '\n')
            {
                lines[lineCount][0] = '\0';
                lineCount++;
                p++;
                continue;
            }

            int16_t take = 0;
            int16_t lastSpace = -1;
            while (p[take] != '\0' && p[take] != '\n' && take < maxChars && take < 46)
            {
                if (p[take] == ' ')
                    lastSpace = take;
                take++;
            }

            if (p[take] != '\0' && p[take] != '\n' && lastSpace > 0)
                take = lastSpace;

            if (take <= 0)
                take = 1;

            memcpy(lines[lineCount], p, (size_t)take);
            lines[lineCount][take] = '\0';
            lineCount++;

            p += take;
            if (*p == '\n')
                p++;
        }

        const int16_t totalH = (int16_t)(lineCount * LINE_H);
        const int16_t startY = (totalH < bh) ? (int16_t)(by + (bh - totalH) / 2 + 1) : by;

        tft.setTextColor(fg, bg);
        for (int16_t i = 0; i < lineCount; ++i)
        {
            const int16_t lw = (int16_t)(strlen(lines[i]) * CHAR_W);
            int16_t lx = (int16_t)(bx + (bw - lw) / 2);
            if (lx < bx)
                lx = bx;
            tft.setCursor(lx, (int16_t)(startY + i * LINE_H));
            tft.print(lines[i]);
        }
    }

    void drawActionButton(
        JWPLC_TFTClass &tft,
        int16_t bx,
        int16_t by,
        int16_t bw,
        int16_t bh,
        const char *label,
        uint16_t fill,
        uint16_t textColor,
        bool focused)
    {
        tft.fillRoundRect(bx, by, bw, bh, 4, fill);
        if (focused)
        {
            tft.drawRoundRect(bx, by, bw, bh, 4, JWPLC_TFT_WHITE);
            tft.drawRoundRect((int16_t)(bx + 1), (int16_t)(by + 1), (int16_t)(bw - 2), (int16_t)(bh - 2), 3, JWPLC_TFT_WHITE);
        }
        else
        {
            tft.drawRoundRect(bx, by, bw, bh, 4, textColor);
        }
        drawCenteredGlcd(tft, (int16_t)(bx + bw / 2), (int16_t)(by + bh / 2), label, textColor, fill);
    }

    void renderPopup(JWPLC_TFTClass &tft, const ActivePopupEntry &entry)
    {
        const JWPLC_UIPopup &def = entry.def;
        const JWPLC_UIPopupColors &c = def.colors;

        int16_t x = 0, y = 0, w = 0, h = 0, r = 0;
        clampPopupRect(def, x, y, w, h, r);

        tft.setFont(JWPLC_FONT_GLCD);
        tft.setTextSize(1);

        // Marco exterior oscuro para contraste sobre widgets claros.
        if (r > 0)
        {
            tft.drawRoundRect((int16_t)(x - 1), (int16_t)(y - 1), (int16_t)(w + 2), (int16_t)(h + 2), (int16_t)(r + 1), JWPLC_TFT_BLACK);
            tft.fillRoundRect(x, y, w, h, r, c.background);
            tft.drawRoundRect(x, y, w, h, r, c.border);
            tft.drawRoundRect((int16_t)(x + 1), (int16_t)(y + 1), (int16_t)(w - 2), (int16_t)(h - 2), (r > 1) ? (int16_t)(r - 1) : 0, c.border);
        }
        else
        {
            tft.drawRect((int16_t)(x - 1), (int16_t)(y - 1), (int16_t)(w + 2), (int16_t)(h + 2), JWPLC_TFT_BLACK);
            tft.fillRect(x, y, w, h, c.background);
            tft.drawRect(x, y, w, h, c.border);
            tft.drawRect((int16_t)(x + 1), (int16_t)(y + 1), (int16_t)(w - 2), (int16_t)(h - 2), c.border);
        }

        // Cabecera
        tft.fillRect((int16_t)(x + 2), (int16_t)(y + 2), (int16_t)(w - 4), HEADER_H, c.headerBg);
        drawHeaderIcon(tft, def.preset, (int16_t)(x + 6), (int16_t)(y + 6), c.headerText, c.headerBg);

        char codeBuf[10] = {};
        if (def.code != nullptr && def.code[0] != '\0')
        {
            copyBounded(codeBuf, sizeof(codeBuf), def.code);
        }
        const int16_t codeW = (codeBuf[0] != '\0') ? (int16_t)(strlen(codeBuf) * CHAR_W) : 0;
        if (codeW > 0)
        {
            tft.setTextColor(c.headerText, c.headerBg);
            tft.setCursor((int16_t)(x + w - 8 - codeW), (int16_t)(y + 9));
            tft.print(codeBuf);
        }

        char titleBuf[36] = {};
        const char *rawTitle = (def.title != nullptr && def.title[0] != '\0') ? def.title : "AVISO";
        copyBounded(titleBuf, sizeof(titleBuf), rawTitle);
        const int16_t titleSpace = (int16_t)(w - 30 - (codeW > 0 ? codeW + 10 : 8));
        const int16_t maxTitleChars = (titleSpace / CHAR_W > 1) ? (int16_t)(titleSpace / CHAR_W) : 1;
        if ((int16_t)strlen(titleBuf) > maxTitleChars)
            titleBuf[maxTitleChars] = '\0';

        tft.setTextColor(c.headerText, c.headerBg);
        tft.setCursor((int16_t)(x + 24), (int16_t)(y + 9));
        tft.print(titleBuf);

        // Cuerpo centrado
        const int16_t bodyX = (int16_t)(x + 10);
        const int16_t bodyY = (int16_t)(y + 2 + HEADER_H + 4);
        const int16_t bodyW = (int16_t)(w - 20);
        const int16_t bodyH = (int16_t)(h - HEADER_H - 34);
        drawWrappedBody(tft, def.message, bodyX, bodyY, bodyW, bodyH, c.text, c.background);

        // Botonera inferior
        const int16_t btnY = (int16_t)(y + h - 25);
        const int16_t btnH = 18;
        if (def.preset == JWPLC_UI_POPUP_CONFIRM)
        {
            const int16_t btnW = 94;
            const int16_t gap = 12;
            const int16_t totalW = (int16_t)(btnW * 2 + gap);
            const int16_t startX = (int16_t)(x + (w - totalW) / 2);
            const char *cancelLbl = (def.cancelText != nullptr && def.cancelText[0] != '\0') ? def.cancelText : "CANCELAR";
            const char *acceptLbl = (def.acceptText != nullptr && def.acceptText[0] != '\0') ? def.acceptText : "ACEPTAR";
            drawActionButton(tft, startX, btnY, btnW, btnH, cancelLbl, c.cancelBg, c.cancelText, entry.focusIndex == 0);
            drawActionButton(tft, (int16_t)(startX + btnW + gap), btnY, btnW, btnH, acceptLbl, c.acceptBg, c.acceptText, entry.focusIndex == 1);
        }
        else
        {
            const int16_t btnW = 86;
            const int16_t bx = (int16_t)(x + (w - btnW) / 2);
            const char *okLbl = (def.acceptText != nullptr && def.acceptText[0] != '\0') ? def.acceptText : "OK";
            drawActionButton(tft, bx, btnY, btnW, btnH, okLbl, c.acceptBg, c.acceptText, true);
        }

        tft.setFont(JWPLC_FONT_GLCD);
    }
}

namespace JWPLCUIPopup
{
    bool setPopups(const JWPLC_UIPopup *popups, size_t count)
    {
        jwplcUIRuntimeEnsureLinked();

        if ((popups == nullptr && count != 0) || count > JWPLC_UI_POPUP_MAX_COUNT)
            return false;

        g_popupCount = 0;
        memset(g_prevTriggers, 0, sizeof(g_prevTriggers));
        memset(g_confirmedFlags, 0, sizeof(g_confirmedFlags));

        for (size_t i = 0; i < count; ++i)
        {
            g_popups[i] = popups[i];
            g_prevTriggers[i] = (popups[i].triggerVar != nullptr) ? (*popups[i].triggerVar) : false;
            g_popupCount++;
        }

        return true;
    }

    void clearPopups()
    {
        g_popupCount = 0;
        dismissPopup(JWPLC_UI_POPUP_NONE);
    }

    size_t popupCount()
    {
        return g_popupCount;
    }

    bool showPopup(uint8_t id)
    {
        jwplcUIRuntimeEnsureLinked();
        const int idx = findDefIndexById(id);
        if (idx < 0)
            return false;

        ActivePopupEntry entry;
        makeEntryFromDef((uint8_t)idx, entry);
        return enqueueOrActivate(entry);
    }

    bool showPopupMessage(
        JWPLC_UIPopupPreset preset,
        const char *title,
        const char *message,
        const char *code,
        uint16_t autoCloseMs)
    {
        jwplcUIRuntimeEnsureLinked();
        ActivePopupEntry entry;
        makeAdHocEntry(preset, title, message, code, autoCloseMs, entry);
        return enqueueOrActivate(entry);
    }

    void dismissPopup(uint8_t id)
    {
        if (id == JWPLC_UI_POPUP_NONE)
        {
            g_queueCount = 0;
            closeActive(false);
            return;
        }

        // Quitar de la cola si estaba pendiente
        for (uint8_t i = 0; i < g_queueCount;)
        {
            if (!g_queue[i].isAdHoc && g_queue[i].def.id == id)
            {
                for (uint8_t j = i + 1; j < g_queueCount; ++j)
                {
                    g_queue[j - 1] = g_queue[j];
                    rebindEntryPointers(g_queue[j - 1]);
                }
                g_queue[g_queueCount - 1] = ActivePopupEntry();
                g_queueCount--;
            }
            else
            {
                i++;
            }
        }

        if (g_active.used && !g_active.isAdHoc && g_active.def.id == id)
        {
            closeActive(false);
        }
    }

    bool popupActive()
    {
        return g_active.used;
    }

    bool popupConfirmed(uint8_t id)
    {
        const int idx = findDefIndexById(id);
        if (idx < 0)
            return false;
        if (!g_confirmedFlags[idx])
            return false;
        g_confirmedFlags[idx] = false;
        return true;
    }
}

// ======================================================================
// Puente público JWPLC_DisplayClass (sólo se enlaza si se usa Popups)
// ======================================================================

bool JWPLC_DisplayClass::setPopups(const JWPLC_UIPopup *popups, size_t count)
{
    return JWPLCUIPopup::setPopups(popups, count);
}

void JWPLC_DisplayClass::clearPopups()
{
    JWPLCUIPopup::clearPopups();
}

bool JWPLC_DisplayClass::showPopup(uint8_t id)
{
    return JWPLCUIPopup::showPopup(id);
}

bool JWPLC_DisplayClass::showPopup(
    JWPLC_UIPopupPreset preset,
    const char *title,
    const char *message,
    const char *code,
    uint16_t autoCloseMs)
{
    return JWPLCUIPopup::showPopupMessage(preset, title, message, code, autoCloseMs);
}

void JWPLC_DisplayClass::dismissPopup(uint8_t id)
{
    JWPLCUIPopup::dismissPopup(id);
}

bool JWPLC_DisplayClass::popupActive() const
{
    return JWPLCUIPopup::popupActive();
}

bool JWPLC_DisplayClass::popupConfirmed(uint8_t id)
{
    return JWPLCUIPopup::popupConfirmed(id);
}

// ======================================================================
// Hooks strong para JWPLC_Display / JWPLC_UI_API / JWPLC_UI_Menu
// ======================================================================

extern "C" void jwplcUIOverlayPoll(void)
{
    // 1. Triggers por flanco ascendente (0 -> 1)
    for (uint8_t i = 0; i < g_popupCount; ++i)
    {
        const bool *var = g_popups[i].triggerVar;
        if (var == nullptr)
            continue;
        const bool curr = *var;
        if (!g_prevTriggers[i] && curr)
        {
            ActivePopupEntry entry;
            makeEntryFromDef(i, entry);
            (void)enqueueOrActivate(entry);
        }
        g_prevTriggers[i] = curr;
    }

    if (!g_active.used)
        return;

    // 2. Auto-cierre si !latched y el trigger volvió a false
    if (!g_active.def.latched &&
        !g_active.isAdHoc &&
        g_active.def.triggerVar != nullptr &&
        !(*g_active.def.triggerVar))
    {
        closeActive(false);
        return;
    }

    // 3. Temporizador autoCloseMs (no aplica a CRIT)
    if (g_active.def.autoCloseMs > 0 &&
        g_active.def.preset != JWPLC_UI_POPUP_CRIT &&
        (uint32_t)(millis() - g_active.openedMs) >= (uint32_t)g_active.def.autoCloseMs)
    {
        closeActive(false);
    }
}

extern "C" bool jwplcUIOverlayServiceInput(void)
{
    if (!shouldShowInCurrentMode(g_active))
        return false;

    if (!JWPLCButtons::isReady())
        return true;

    const bool ok = JWPLC_Buttons.pressed(BTN_OK);
    const bool esc = JWPLC_Buttons.pressed(BTN_ESC);
    const bool left = JWPLC_Buttons.pressed(BTN_LEFT);
    const bool right = JWPLC_Buttons.pressed(BTN_RIGHT);
    const bool up = JWPLC_Buttons.pressed(BTN_UP);
    const bool down = JWPLC_Buttons.pressed(BTN_DOWN);

    if (g_active.def.preset == JWPLC_UI_POPUP_CONFIRM)
    {
        if (esc)
        {
            closeActive(false);
            return true;
        }
        if (ok)
        {
            const bool accepted = (g_active.focusIndex == 1);
            closeActive(accepted);
            return true;
        }
        if (left || right || up || down)
        {
            g_active.focusIndex = (g_active.focusIndex == 0) ? 1 : 0;
            g_overlayDirty = true;
            JWPLCButtons::clearPendingInput();
            jwplcSystemForceDisplayRefresh();
        }
        return true;
    }

    // CRIT solo permite ACK con OK (ESC no descarta alarmas críticas).
    if (g_active.def.preset == JWPLC_UI_POPUP_CRIT)
    {
        if (ok)
        {
            closeActive(true);
        }
        else if (esc || left || right || up || down)
        {
            JWPLCButtons::clearPendingInput();
        }
        return true;
    }

    // INFO / WARN: OK o ESC cierran el modal.
    if (ok || esc)
    {
        closeActive(ok);
        return true;
    }

    // Consumir cualquier otra tecla mientras hay modal visible (único dueño).
    if (left || right || up || down)
        JWPLCButtons::clearPendingInput();

    return true;
}

extern "C" bool jwplcUIOverlayActive(void)
{
    return shouldShowInCurrentMode(g_active);
}

extern "C" bool jwplcUIOverlayBlocksIdle(void)
{
    return g_active.used &&
           (g_active.def.blocksIdle ||
            g_active.def.preset == JWPLC_UI_POPUP_CRIT ||
            g_active.def.preset == JWPLC_UI_POPUP_CONFIRM);
}

extern "C" bool jwplcUIOverlayBlocksIdleWake(void)
{
    return g_active.used &&
           g_active.def.preset == JWPLC_UI_POPUP_CRIT;
}

extern "C" bool jwplcUIOverlayNeedsRefresh(void)
{
    return g_overlayDirty || g_restoreUnderlying;
}

extern "C" bool jwplcUIOverlayTakeRestoreUnderlying(void)
{
    if (!g_restoreUnderlying)
        return false;
    g_restoreUnderlying = false;
    return true;
}

extern "C" void jwplcUIOverlayNotifyUnderlyingDraw(void)
{
    if (shouldShowInCurrentMode(g_active))
    {
        g_overlayDirty = true;
    }
}

extern "C" void jwplcUIOverlayDraw(JWPLC_TFTClass *tft, bool forceRedraw)
{
    if (tft == nullptr || !shouldShowInCurrentMode(g_active))
        return;

    if (forceRedraw || g_overlayDirty)
    {
        renderPopup(*tft, g_active);
        g_overlayDirty = false;
    }
}

extern "C" bool jwplcUIOverlayRequestConfirm(
    uint8_t popupId,
    jwplcUIPopupConfirmCallback_t onAccept,
    void *userCtx)
{
    jwplcUIRuntimeEnsureLinked();
    const int idx = findDefIndexById(popupId);
    if (idx < 0)
        return false;

    ActivePopupEntry entry;
    makeEntryFromDef((uint8_t)idx, entry);
    if (entry.def.preset != JWPLC_UI_POPUP_CONFIRM)
    {
        entry.def.preset = JWPLC_UI_POPUP_CONFIRM;
        entry.def.cancelText = "CANCELAR";
        if (entry.def.acceptText == nullptr || strcmp(entry.def.acceptText, "OK") == 0)
            entry.def.acceptText = "ACEPTAR";
    }
    entry.def.blocksIdle = true;
    entry.def.latched = true;
    entry.onAccept = onAccept;
    entry.onAcceptCtx = userCtx;
    return enqueueOrActivate(entry);
}
