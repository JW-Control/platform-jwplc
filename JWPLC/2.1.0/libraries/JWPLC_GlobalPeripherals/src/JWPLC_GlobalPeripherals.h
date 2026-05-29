#ifndef JWPLC_GLOBAL_PERIPHERALS_H
#define JWPLC_GLOBAL_PERIPHERALS_H

#include <Arduino.h>
#include <JW_RTC.h>
#include <JW_FRAM.h>
#include <JW_SD.h>
#include <JW_MatrixButtons.h>
#include <JWPLC_Ethernet.h>
#include <JWPLC_RS485.h>
#include <JWPLC_ModbusRTU.h>

// =====================================================
// IDs físicos de la botonera JWPLC
// =====================================================
// La botonera ya no pertenece al display. Es un periférico base
// del ecosistema JWPLC.

enum JWPLCButtonId : uint8_t
{
    BTN_LEFT = 0,
    BTN_UP,
    BTN_RIGHT,
    BTN_ESC,
    BTN_OK,
    BTN_DOWN,
    BTN_COUNT
};

// =====================================================
// Objetos globales del ecosistema JWPLC
// =====================================================
// Estas instancias quedan disponibles para el sketch del usuario.
// La inicialización automática la realiza el runtime del package.

extern JW_RTC JWPLC_RTC;
extern JW_FRAM JWPLC_FRAM;
extern JW_MatrixButtons JWPLC_Buttons;
extern JW_SD JWPLC_SD;


// =====================================================
// Helpers globales de botonera
// =====================================================

namespace JWPLCButtons
{
    bool begin();
    bool isReady();

    bool anyPressed();
    bool escPressed();
    bool anyPressedOrRepeated();

    void clearPendingInput();
}

// =====================================================
// Helpers globales de microSD
// =====================================================

namespace JWPLCSD
{
    bool begin();
    bool isEnabled();
    bool isReady();
    bool isCardPresent();
    const char *lastErrorString();
}

#endif // JWPLC_GLOBAL_PERIPHERALS_H