#include <Arduino.h>
#include <JWPLC_Bundled_Ethernet_W5x00.h>
#include <Ethernet.h>

// Gate de enlace Alpha5 para el archive precompilado del backend W5x00.
// La referencia a EthernetClass::init fuerza al linker a extraer el backend
// sin ejecutar accesos SPI ni GPIO durante la prueba.
static void (*volatile ethernetInitFn)(uint8_t) = &EthernetClass::init;

void setup()
{
    (void)ethernetInitFn;
}

void loop()
{
}
