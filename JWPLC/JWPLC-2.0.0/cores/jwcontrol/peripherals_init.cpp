#include "peripherals_init.h"

#include <stdbool.h>

#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "pins_arduino.h"

#include "jwplc_hardware_config.h"

extern "C"
{
#include "peripheral-tca6424a.h"
#include "jwplc_peripherals.h"
#include "jwplc_i2c_bridge.h"
}

static bool g_jwplc_peripherals_initialized = false;

void initPeripherals(void)
{
    if (g_jwplc_peripherals_initialized)
    {
        return;
    }

#if defined(JWPLC_BASIC)
    gpio_set_direction((gpio_num_t)EN_IO, GPIO_MODE_OUTPUT);

    gpio_set_level((gpio_num_t)EN_IO, 0);
    vTaskDelay(pdMS_TO_TICKS(2));

    jwplcSystemInitState();

    if (jwplcI2C_begin() != 0)
    {
        return;
    }

#if JWPLC_HAS_RTC
    // RTC no es crítico para permitir que el resto del sistema arranque.
    // Si está habilitado por perfil de hardware, se inicializa de forma automática.
    if (jwplcRTCBeginCallback())
    {
        jwplcSystemTickRTC();
    }
#endif

    if (!TCA6424A_init(TCA6424A_DEFAULT_ADDRESS))
    {
        return;
    }

    jwplcSystemClearOutputShadow();

    (void)TCA6424A_writeBank(TCA6424A_DEFAULT_ADDRESS, 1, 0x00);
    (void)TCA6424A_writeBank(TCA6424A_DEFAULT_ADDRESS, 2, 0x00);

    (void)TCA6424A_setBankDirection(TCA6424A_DEFAULT_ADDRESS, 0, 0xFF);
    (void)TCA6424A_setBankDirection(TCA6424A_DEFAULT_ADDRESS, 1, 0x00);
    (void)TCA6424A_setBankDirection(TCA6424A_DEFAULT_ADDRESS, 2, 0xFF);

    gpio_set_level((gpio_num_t)EN_IO, 1);
    vTaskDelay(pdMS_TO_TICKS(2));

    g_jwplc_peripherals_initialized = true;
#else
    jwplcSystemInitState();
    g_jwplc_peripherals_initialized = true;
#endif
}
