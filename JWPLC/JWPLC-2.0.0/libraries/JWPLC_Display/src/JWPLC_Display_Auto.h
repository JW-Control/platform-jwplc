#ifndef JWPLC_DISPLAY_AUTO_H
#define JWPLC_DISPLAY_AUTO_H

// =====================================================
// JWPLC Display autoload marker
// =====================================================
//
// Este header existe para que Arduino Builder detecte y compile
// automaticamente la libreria JWPLC_Display cuando se usa una placa JWPLC.
//
// Importante:
// - No debe incluir JWPLC_Display.h.
// - No debe incluir Adafruit_ST7789.h ni headers pesados del display.
// - Si se incluye desde Arduino.h, un include pesado puede generar ciclos.
//
// Pero si debe exponer los perifericos globales livianos del ecosistema,
// para que el sketch pueda usar JWPLC_RTC, JWPLC_FRAM y JWPLC_Buttons
// sin includes manuales.

#include <JWPLC_GlobalPeripherals.h>
#include <JWPLC_Display_API.h>

#endif // JWPLC_DISPLAY_AUTO_H