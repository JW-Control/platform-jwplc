#include <Arduino.h>

// Marcador exclusivo del package JWPLC.
// Fuerza a Arduino Builder a seleccionar la copia vendorizada de BusIO
// antes de resolver Adafruit_SPIDevice.h.
#include <JWPLC_Bundled_Adafruit_BusIO.h>
#include <Adafruit_SPIDevice.h>

// Gate Alpha5:
// tomar la direccion de begin() obliga al linker a extraer
// Adafruit_SPIDevice.cpp.o del archive precompilado.
// No se ejecuta begin(), por lo que este gate no toca GPIO ni SPI.
using BusIOBeginFn = bool (Adafruit_SPIDevice::*)();
static BusIOBeginFn volatile busioBeginFn = &Adafruit_SPIDevice::begin;

void setup()
{
    (void)busioBeginFn;
}

void loop()
{
}
