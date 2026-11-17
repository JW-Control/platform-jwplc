#ifndef JWPLC_UI_POPUP_H
#define JWPLC_UI_POPUP_H

#include <Arduino.h>
#include <stddef.h>

// Sistema de ventanas emergentes modales (I5-I6).
// Se dibujan sobre la pantalla actual (IDLE o USER) sin añadir páginas al
// selector, arbitran las teclas por encima de MENU/EVENT_LOG/Selector (I4) y
// soportan cola por prioridad + detección de flanco en triggerVar.

#define JWPLC_UI_HAS_POPUP 1

static constexpr uint8_t JWPLC_UI_POPUP_MAX_COUNT = 12;
static constexpr uint8_t JWPLC_UI_POPUP_QUEUE_SIZE = 4;
static constexpr uint8_t JWPLC_UI_POPUP_NONE = 0xFFU;

enum JWPLC_UIPopupPreset : uint8_t
{
    JWPLC_UI_POPUP_INFO = 0,
    JWPLC_UI_POPUP_WARN = 1,
    JWPLC_UI_POPUP_CRIT = 2,
    JWPLC_UI_POPUP_CONFIRM = 3
};

struct JWPLC_UIPopupColors
{
    uint16_t background;
    uint16_t border;
    uint16_t headerBg;
    uint16_t headerText;
    uint16_t text;
    uint16_t acceptBg;
    uint16_t acceptText;
    uint16_t cancelBg;
    uint16_t cancelText;

    constexpr JWPLC_UIPopupColors(
        uint16_t backgroundValue = 0x1082,
        uint16_t borderValue = 0x2D7F,
        uint16_t headerBgValue = 0x1B9C,
        uint16_t headerTextValue = 0xFFFF,
        uint16_t textValue = 0xFFFF,
        uint16_t acceptBgValue = 0x1B9C,
        uint16_t acceptTextValue = 0xFFFF,
        uint16_t cancelBgValue = 0x3186,
        uint16_t cancelTextValue = 0xFFFF)
        : background(backgroundValue),
          border(borderValue),
          headerBg(headerBgValue),
          headerText(headerTextValue),
          text(textValue),
          acceptBg(acceptBgValue),
          acceptText(acceptTextValue),
          cancelBg(cancelBgValue),
          cancelText(cancelTextValue)
    {
    }
};

constexpr JWPLC_UIPopupColors JWPLC_UIPopupPresetColors(JWPLC_UIPopupPreset preset)
{
    return (preset == JWPLC_UI_POPUP_WARN)
               ? JWPLC_UIPopupColors(0x1082, 0xFD20, 0xFD20, 0x0000, 0xFFFF, 0xFD20, 0x0000, 0x3186, 0xFFFF)
           : (preset == JWPLC_UI_POPUP_CRIT)
               ? JWPLC_UIPopupColors(0x1082, 0xF945, 0xE8E4, 0xFFFF, 0xFFFF, 0xE8E4, 0xFFFF, 0x3186, 0xFFFF)
           : (preset == JWPLC_UI_POPUP_CONFIRM)
               ? JWPLC_UIPopupColors(0x1082, 0x07FF, 0x055A, 0xFFFF, 0xFFFF, 0x0568, 0xFFFF, 0x3186, 0xFFFF)
               : JWPLC_UIPopupColors(0x1082, 0x2D7F, 0x1B9C, 0xFFFF, 0xFFFF, 0x1B9C, 0xFFFF, 0x3186, 0xFFFF);
}

struct JWPLC_UIPopup
{
    uint8_t id = 0;
    JWPLC_UIPopupPreset preset = JWPLC_UI_POPUP_INFO;
    const char *title = nullptr;
    const char *code = nullptr;
    const char *message = nullptr;
    const char *acceptText = nullptr;
    const char *cancelText = nullptr;
    const bool *triggerVar = nullptr;
    bool *confirmOutVar = nullptr;
    const char *errCode = nullptr;
    uint16_t autoCloseMs = 0;
    bool showOverIdle = true;
    bool blocksIdle = false;
    bool latched = false;
    bool logToEventLog = false;
    uint8_t targetPage = 0xFFU;
    uint8_t targetView = 0xFFU;
    int16_t x = 36;
    int16_t y = 27;
    int16_t width = 248;
    int16_t height = 116;
    uint8_t cornerRadius = 6;
    JWPLC_UIPopupColors colors = JWPLC_UIPopupPresetColors(JWPLC_UI_POPUP_INFO);
};

inline JWPLC_UIPopup JWPLC_UIPopupInfo(
    uint8_t id,
    const char *title,
    const char *message,
    const char *code = nullptr)
{
    JWPLC_UIPopup p;
    p.id = id;
    p.preset = JWPLC_UI_POPUP_INFO;
    p.title = title;
    p.message = message;
    p.code = code;
    p.acceptText = "OK";
    p.showOverIdle = true;
    p.blocksIdle = false;
    p.latched = false;
    p.colors = JWPLC_UIPopupPresetColors(JWPLC_UI_POPUP_INFO);
    return p;
}

inline JWPLC_UIPopup JWPLC_UIPopupWarn(
    uint8_t id,
    const char *title,
    const char *message,
    const char *code = nullptr)
{
    JWPLC_UIPopup p;
    p.id = id;
    p.preset = JWPLC_UI_POPUP_WARN;
    p.title = title;
    p.message = message;
    p.code = code;
    p.acceptText = "OK";
    p.showOverIdle = true;
    p.blocksIdle = false;
    p.latched = false;
    p.colors = JWPLC_UIPopupPresetColors(JWPLC_UI_POPUP_WARN);
    return p;
}

inline JWPLC_UIPopup JWPLC_UIPopupCrit(
    uint8_t id,
    const char *title,
    const char *message,
    const char *code = nullptr,
    const char *errCode = nullptr)
{
    JWPLC_UIPopup p;
    p.id = id;
    p.preset = JWPLC_UI_POPUP_CRIT;
    p.title = title;
    p.message = message;
    p.code = code;
    p.errCode = errCode;
    p.acceptText = "OK";
    p.showOverIdle = true;
    p.blocksIdle = true;
    p.latched = true;
    p.logToEventLog = true;
    p.colors = JWPLC_UIPopupPresetColors(JWPLC_UI_POPUP_CRIT);
    return p;
}

inline JWPLC_UIPopup JWPLC_UIPopupConfirm(
    uint8_t id,
    const char *title,
    const char *message,
    bool *confirmOutVar = nullptr,
    const char *cancelText = "CANCELAR",
    const char *acceptText = "ACEPTAR")
{
    JWPLC_UIPopup p;
    p.id = id;
    p.preset = JWPLC_UI_POPUP_CONFIRM;
    p.title = title;
    p.message = message;
    p.confirmOutVar = confirmOutVar;
    p.cancelText = cancelText;
    p.acceptText = acceptText;
    p.showOverIdle = true;
    p.blocksIdle = true;
    p.latched = true;
    p.colors = JWPLC_UIPopupPresetColors(JWPLC_UI_POPUP_CONFIRM);
    return p;
}

inline JWPLC_UIPopup JWPLC_UIPopupTrigger(
    JWPLC_UIPopup p,
    const bool *triggerVar,
    bool latched = false,
    bool showOverIdle = true)
{
    p.triggerVar = triggerVar;
    p.latched = latched;
    p.showOverIdle = showOverIdle;
    return p;
}

inline JWPLC_UIPopup JWPLC_UIPopupColorsOverride(
    JWPLC_UIPopup p,
    JWPLC_UIPopupColors colors)
{
    p.colors = colors;
    return p;
}

inline JWPLC_UIPopup JWPLC_UIPopupOptions(
    JWPLC_UIPopup p,
    uint16_t autoCloseMs = 0,
    bool logToEventLog = false,
    bool blocksIdle = false)
{
    p.autoCloseMs = autoCloseMs;
    p.logToEventLog = logToEventLog;
    p.blocksIdle = blocksIdle;
    return p;
}

inline JWPLC_UIPopup JWPLC_UIPopupButtons(
    JWPLC_UIPopup p,
    const char *acceptText,
    const char *cancelText = nullptr)
{
    if (acceptText != nullptr)
        p.acceptText = acceptText;
    if (cancelText != nullptr)
        p.cancelText = cancelText;
    return p;
}

inline JWPLC_UIPopup JWPLC_UIPopupRect(
    JWPLC_UIPopup p,
    int16_t x,
    int16_t y,
    int16_t width,
    int16_t height,
    uint8_t cornerRadius = 6)
{
    p.x = x;
    p.y = y;
    p.width = width;
    p.height = height;
    p.cornerRadius = cornerRadius;
    return p;
}

inline JWPLC_UIPopup JWPLC_UIPopupNavTarget(
    JWPLC_UIPopup p,
    uint8_t targetPage,
    uint8_t targetView = 0xFFU)
{
    p.targetPage = targetPage;
    p.targetView = targetView;
    return p;
}

namespace JWPLCUIPopup
{
    bool setPopups(const JWPLC_UIPopup *popups, size_t count);
    void clearPopups();
    size_t popupCount();

    bool showPopup(uint8_t id);
    bool showPopupMessage(
        JWPLC_UIPopupPreset preset,
        const char *title,
        const char *message,
        const char *code = nullptr,
        uint16_t autoCloseMs = 0);
    void dismissPopup(uint8_t id = JWPLC_UI_POPUP_NONE);
    bool popupActive();
    bool popupConfirmed(uint8_t id);
}

#endif // JWPLC_UI_POPUP_H
