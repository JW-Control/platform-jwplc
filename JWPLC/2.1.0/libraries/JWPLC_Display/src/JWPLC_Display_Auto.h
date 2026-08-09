#ifndef JWPLC_DISPLAY_AUTO_H
#define JWPLC_DISPLAY_AUTO_H

// =====================================================
// JWPLC Display autoload marker
// =====================================================
//
// Este header es el punto de entrada automatico del ecosistema JWPLC.
// Durante library discovery debe permanecer liviano para evitar expandir
// repetidamente los headers pesados del runtime y del display.
//
// Con librerias precompiladas Arduino omite la deteccion de dependencias de
// sus fuentes. Por eso se usan marcadores vacios y exclusivos del package
// para descubrir explicitamente las copias bundled requeridas sin incluir
// sus APIs pesadas en el sketch.

#ifndef JWPLC_LIBRARY_DISCOVERY_PHASE
#define JWPLC_LIBRARY_DISCOVERY_PHASE 0
#endif

#include <JWPLC_Display_API.h>
#include <JWPLC_GlobalPeripherals_Auto.h>

#if JWPLC_LIBRARY_DISCOVERY_PHASE
#include <JWPLC_Bundled_ST77xx_Marker.h>
#include <JWPLC_Bundled_GFX_Marker.h>
#include <JWPLC_Bundled_BusIO_Marker.h>
#include <JWPLC_Bundled_Ethernet_Marker.h>
#include <JWPLC_Bundled_SD_Marker.h>
#endif

#endif // JWPLC_DISPLAY_AUTO_H
