// Stub host.
#pragma once
#include <stdint.h>

// Copia de JWPLC_GlobalPeripherals/src/JWPLC_GlobalPeripherals.h (IDs de la
// botonera): el header generado los usa en la configuración de la IDLE.
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
