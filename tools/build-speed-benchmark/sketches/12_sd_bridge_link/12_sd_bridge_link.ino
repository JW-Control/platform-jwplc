#include <Arduino.h>
#include <SD.h>

// Alpha5: gate de enlace para el archive precompilado de SD nativa.
// Tomar la direccion de SDFS::begin obliga al linker a extraer codigo real
// de libSD.a sin ejecutar accesos SPI ni GPIO durante esta prueba.
using SDBeginFn = bool (fs::SDFS::*)(uint8_t, SPIClass &, uint32_t, const char *, uint8_t, bool);
static SDBeginFn volatile sdBeginFn = static_cast<SDBeginFn>(&fs::SDFS::begin);

void setup()
{
    (void)sdBeginFn;
}

void loop()
{
}
