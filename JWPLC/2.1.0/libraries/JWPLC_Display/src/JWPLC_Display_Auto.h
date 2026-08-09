#ifndef JWPLC_DISPLAY_AUTO_H
#define JWPLC_DISPLAY_AUTO_H

// =====================================================
// JWPLC Display autoload marker
// =====================================================
//
// Punto de entrada automatico del ecosistema JWPLC.
// Durante library discovery debe permanecer liviano para evitar expandir
// repetidamente los headers pesados del runtime y del display.
//
// JWPLC_Display puede distribuirse como precompiled=full. En ese modo Arduino
// no inspecciona los fuentes de la libreria para descubrir dependencias, por
// lo que declaramos explicitamente solo el header grafico necesario durante
// discovery. Se deja que Arduino aplique su resolucion normal de librerias;
// no se fuerzan copias bundled por marcadores para evitar colisiones entre
// librerias homonimas del core ESP32 y del sketchbook.

#ifndef JWPLC_LIBRARY_DISCOVERY_PHASE
#define JWPLC_LIBRARY_DISCOVERY_PHASE 0
#endif

#include <JWPLC_Display_API.h>
#include <JWPLC_GlobalPeripherals_Auto.h>

#if JWPLC_LIBRARY_DISCOVERY_PHASE
#include <Adafruit_ST7789.h>
#endif

#endif // JWPLC_DISPLAY_AUTO_H
