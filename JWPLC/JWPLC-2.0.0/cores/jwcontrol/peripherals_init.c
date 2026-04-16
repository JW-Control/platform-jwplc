#include "peripherals_init.h"

#include <stdbool.h>

#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "pins_arduino.h"

#include "WireC.h"
#include "peripheral-tca6424a.h"

static bool g_jwplc_peripherals_initialized = false;

void initPeripherals(void)
{
    if (g_jwplc_peripherals_initialized)
    {
        return;
    }

#if defined(JWPLC_BASIC)
    // 1) Mantener las salidas aisladas al arrancar
    gpio_set_direction((gpio_num_t)EN_IO, GPIO_MODE_OUTPUT);
    gpio_set_level((gpio_num_t)EN_IO, 0);
    vTaskDelay(pdMS_TO_TICKS(2));

    // 2) Iniciar I2C
    if (WireC_begin() != 0)
    {
        return;
    }

    // 3) Verificar que el TCA responda
    if (!TCA6424A_init(TCA6424A_DEFAULT_ADDRESS))
    {
        return;
    }

    // 4) Estado seguro de salidas
    // Banco 1 = P10..P17 = DO0..DO7
    (void)TCA6424A_writeBank(TCA6424A_DEFAULT_ADDRESS, 1, 0x00);

    // Banco 2 no usado por ahora
    (void)TCA6424A_writeBank(TCA6424A_DEFAULT_ADDRESS, 2, 0x00);

    // 5) Direcciones
    // Banco 0 = entradas
    // Banco 1 = salidas
    // Banco 2 = no usado -> entrada por seguridad
    (void)TCA6424A_setBankDirection(TCA6424A_DEFAULT_ADDRESS, 0, 0xFF);
    (void)TCA6424A_setBankDirection(TCA6424A_DEFAULT_ADDRESS, 1, 0x00);
    (void)TCA6424A_setBankDirection(TCA6424A_DEFAULT_ADDRESS, 2, 0xFF);

    // 6) Dar un pequeño margen y habilitar salidas
    vTaskDelay(pdMS_TO_TICKS(2));
    gpio_set_level((gpio_num_t)EN_IO, 1);

    g_jwplc_peripherals_initialized = true;
#else
    g_jwplc_peripherals_initialized = true;
#endif
}