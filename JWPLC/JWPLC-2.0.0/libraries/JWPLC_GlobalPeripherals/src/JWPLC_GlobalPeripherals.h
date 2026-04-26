#ifndef JWPLC_GLOBAL_PERIPHERALS_H
#define JWPLC_GLOBAL_PERIPHERALS_H

#include <Arduino.h>
#include <JW_RTC.h>
#include <JW_FRAM.h>

// =====================================================
// Objetos globales del ecosistema JWPLC
// =====================================================
// Estas instancias quedan disponibles para el sketch del usuario.
// La inicialización automática la realiza el runtime del package.

extern JW_RTC JWPLC_RTC;
extern JW_FRAM JWPLC_FRAM;

#endif // JWPLC_GLOBAL_PERIPHERALS_H