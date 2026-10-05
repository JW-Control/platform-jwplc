#ifndef JWPLC_DISPLAY_AUTO_H
#define JWPLC_DISPLAY_AUTO_H

// =====================================================
// JWPLC Display autoload
// =====================================================
//
// JWPLC_Display se apoya exclusivamente en la API JWPLC_TFT.
// El usuario no necesita instalar, configurar ni descubrir Adafruit GFX,
// Adafruit ST7789 o TFT_eSPI para la pantalla integrada.
//
// Durante library discovery este header conserva la API liviana y deja que
// JWPLC_Display_API.h descubra JWPLC_TFT como dependencia publica del package.
// JWPLC_GlobalPeripherals_Auto.h mantiene el resto del autoload normal.

#ifndef JWPLC_LIBRARY_DISCOVERY_PHASE
#define JWPLC_LIBRARY_DISCOVERY_PHASE 0
#endif

#include <JWPLC_Display_API.h>
#include <JWPLC_GlobalPeripherals_Auto.h>

#if !JWPLC_LIBRARY_DISCOVERY_PHASE
// Con dot_a_linkage=true / precompiled=full, esta referencia fuerza la
// extraccion del miembro base JWPLC_Display.cpp.o sin usar whole-archive.
// Los TUs HMI opcionales siguen entrando solo cuando la API los requiere.
namespace JWPLCDisplayAutoload
{
    static JWPLC_DisplayClass *const __attribute__((used)) anchor = &JWPLC_Display;
}
#endif

#endif // JWPLC_DISPLAY_AUTO_H