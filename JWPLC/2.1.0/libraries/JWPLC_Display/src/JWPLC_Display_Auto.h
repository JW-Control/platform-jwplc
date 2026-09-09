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
// no inspecciona los fuentes de la libreria para descubrir dependencias.
//
// Para garantizar reproducibilidad, durante discovery se importan primero
// marcadores exclusivos de las copias Adafruit bundled por JWPLC. Esto evita
// que una libreria homonima instalada o modificada en el sketchbook tenga
// prioridad sobre la copia validada con el package. Los marcadores son vacios:
// no expanden API ni agregan codigo al firmware.

#ifndef JWPLC_LIBRARY_DISCOVERY_PHASE
#define JWPLC_LIBRARY_DISCOVERY_PHASE 0
#endif

#include <JWPLC_Display_API.h>
#include <JWPLC_GlobalPeripherals_Auto.h>

#if JWPLC_LIBRARY_DISCOVERY_PHASE
#include <JWPLC_Bundled_Adafruit_ST77xx.h>
#include <JWPLC_Bundled_Adafruit_GFX.h>
#include <JWPLC_Bundled_Adafruit_BusIO.h>
#include <Adafruit_ST7789.h>
#else
// Autoload real también cuando JWPLC_Display se compila desde source.
//
// Con dot_a_linkage=true Arduino agrupa los .o source en JWPLC_Display.a.
// Incluir únicamente JWPLC_Display_API.h hace visible la API, pero un sketch
// que no use explícitamente el objeto global no deja ninguna referencia que
// obligue al linker a extraer JWPLC_Display.cpp.o del archive. En el formato
// precompilado ese papel lo cumple además ldflags=--undefined=JWPLC_Display.
//
// Esta referencia local, marcada used, conserva el contrato de autoload en
// ambos modos sin whole-archive y sin arrastrar los TUs opcionales de HMI.
// La sección puede ser eliminada posteriormente por --gc-sections; la
// resolución del símbolo ocurre antes y fuerza únicamente el miembro base.
namespace JWPLCDisplayAutoload
{
    static JWPLC_DisplayClass *const __attribute__((used)) anchor = &JWPLC_Display;
}
#endif

#endif // JWPLC_DISPLAY_AUTO_H
