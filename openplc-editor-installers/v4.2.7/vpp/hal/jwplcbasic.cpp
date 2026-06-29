#include <stdlib.h>

extern "C"
{
#include "openplc.h"
}

#include "Arduino.h"

// JWPLC Basic v2.x usa pines virtuales uint16_t:
// I0_0 = 0x2207, Q0_0 = 0x2208, etc.
// No usar uint8_t porque truncaria los pines.
//
// Para el VPP de JWPLC Basic el mapa físico es fijo:
//   I0_0..I0_7 -> entradas digitales
//   Q0_0..Q0_7 -> salidas digitales
//
// No depender de ../examples/Baremetal/defines.h porque OpenPLC 4.2.7
// no genera ese archivo dentro del flujo VPP.
uint16_t pinMask_DIN[] = {
    I0_0, I0_1, I0_2, I0_3,
    I0_4, I0_5, I0_6, I0_7
};

uint16_t pinMask_AIN[] = {
    0xFFFF
};

uint16_t pinMask_DOUT[] = {
    Q0_0, Q0_1, Q0_2, Q0_3,
    Q0_4, Q0_5, Q0_6, Q0_7
};

uint16_t pinMask_AOUT[] = {
    0xFFFF
};

static inline bool jwplcValidPin(uint16_t pin)
{
    // OpenPLC suele usar -1 o 99 en otros targets para pines no usados.
    // En uint16_t, -1 se convierte en 0xFFFF.
    return pin != 0xFFFF && pin != 99;
}

void hardwareInit()
{
    // JWPLC Basic v2.x:
    // La inicializacion de E/S industriales ya la realiza el core jwcontrol
    // mediante initPeripherals(), antes de que se ejecute setup().
    //
    // No tocar EN_IO aqui.
    // No repetir pinMode() aqui.
    // No reinicializar TCA6424A aqui.
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
