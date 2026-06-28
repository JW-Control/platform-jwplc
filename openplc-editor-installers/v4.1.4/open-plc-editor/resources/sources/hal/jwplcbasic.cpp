#include <stdlib.h>

extern "C"
{
#include "openplc.h"
}

#include "Arduino.h"
#include "../examples/Baremetal/defines.h"

// JWPLC Basic v2.x usa pines virtuales uint16_t:
// I0_0 = 0x2207, Q0_0 = 0x2208, etc.
// No usar uint8_t porque truncaría los pines.
uint16_t pinMask_DIN[] = {PINMASK_DIN};
uint16_t pinMask_AIN[] = {PINMASK_AIN};
uint16_t pinMask_DOUT[] = {PINMASK_DOUT};
uint16_t pinMask_AOUT[] = {PINMASK_AOUT};

static inline bool jwplcValidPin(uint16_t pin)
{
    // OpenPLC suele usar -1 o 99 en otros targets para pines no usados.
    // En uint16_t, -1 se convierte en 0xFFFF.
    return pin != 0xFFFF && pin != 99;
}

void hardwareInit()
{
    // JWPLC Basic v2.x:
    // La inicialización de E/S industriales ya la realiza el core jwcontrol
    // mediante initPeripherals(), antes de que se ejecute setup().
    //
    // No tocar EN_IO aquí.
    // No repetir pinMode() aquí.
    // No reinicializar TCA6424A aquí.
}

void updateInputBuffers()
{
    for (int i = 0; i < NUM_DISCRETE_INPUT; i++)
    {
        uint16_t pin = pinMask_DIN[i];

        if (bool_input[i / 8][i % 8] != NULL && jwplcValidPin(pin))
        {
            *bool_input[i / 8][i % 8] = digitalRead(pin);
        }
    }
}

void updateOutputBuffers()
{
    for (int i = 0; i < NUM_DISCRETE_OUTPUT; i++)
    {
        uint16_t pin = pinMask_DOUT[i];

        if (bool_output[i / 8][i % 8] != NULL && jwplcValidPin(pin))
        {
            digitalWrite(pin, *bool_output[i / 8][i % 8]);
        }
    }
}
