#include <Arduino.h>
#include <stdint.h>

extern "C" void jwplc_pinMode(uint16_t pin, uint8_t mode);
extern "C" void jwplc_digitalWrite(uint16_t pin, uint8_t val);
extern "C" int jwplc_digitalRead(uint16_t pin);

// Las referencias volatiles fuerzan al linker a resolver los tres simbolos,
// pero el sketch no ejecuta operaciones GPIO ni requiere hardware conectado.
static void (*volatile bridgePinMode)(uint16_t, uint8_t) = jwplc_pinMode;
static void (*volatile bridgeDigitalWrite)(uint16_t, uint8_t) = jwplc_digitalWrite;
static int (*volatile bridgeDigitalRead)(uint16_t) = jwplc_digitalRead;

void setup()
{
    (void)bridgePinMode;
    (void)bridgeDigitalWrite;
    (void)bridgeDigitalRead;
}

void loop()
{
}
