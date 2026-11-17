#include "peripherals_init.h"

#include <stdbool.h>

#include "driver/gpio.h"
#include "rom/ets_sys.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "pins_arduino.h"

#include "jwplc_hardware_config.h"
#include "jwplc_spi_bus.h"

extern "C"
{
#include "peripheral-tca6424a.h"
#include "jwplc_peripherals.h"
#include "jwplc_i2c_bridge.h"
}

static bool g_jwplc_peripherals_initialized = false;
static volatile uint8_t g_jwplc_io_fault = JWPLC_IO_FAULT_NONE;

#if defined(JWPLC_BASIC)
static const char *jwplcIOFaultText(uint8_t fault)
{
    switch (fault)
    {
    case JWPLC_IO_FAULT_I2C:
        return "el bus I2C no arranco";
    case JWPLC_IO_FAULT_TCA_INIT:
        return "el TCA6424A no responde en I2C";
    case JWPLC_IO_FAULT_TCA_CONFIG:
        return "no se pudo configurar el TCA6424A";
    default:
        return "sin falla";
    }
}

static void jwplcReportIOFault(uint8_t fault)
{
    g_jwplc_io_fault = fault;
    // Se escribe directo en la UART0 con la ROM: sale aunque el sketch todavia
    // no haya llamado a Serial.begin() (la UART0 queda a 115200 desde el arranque).
    ets_printf("\r\n[JWPLC] E/S NO DISPONIBLES: %s. EN_IO (GPIO%d) queda en 0 y los reles"
                   " no se activan.\r\n",
                   jwplcIOFaultText(fault), (int)EN_IO);
}

// Inicializa el TCA6424A, deja las salidas apagadas y recien entonces habilita
// EN_IO. Devuelve la falla (JWPLC_IO_FAULT_NONE si todo salio bien). Si falla,
// EN_IO queda en 0: sin el expansor configurado los reles deben seguir apagados.
static uint8_t jwplcStartExpander(void)
{
    if (!TCA6424A_init(TCA6424A_DEFAULT_ADDRESS))
    {
        return JWPLC_IO_FAULT_TCA_INIT;
    }

    jwplcSystemClearOutputShadow();

    if (!TCA6424A_writeBank(TCA6424A_DEFAULT_ADDRESS, 1, 0x00) ||
        !TCA6424A_writeBank(TCA6424A_DEFAULT_ADDRESS, 2, 0x00) ||
        !TCA6424A_setBankDirection(TCA6424A_DEFAULT_ADDRESS, 0, 0xFF) ||
        !TCA6424A_setBankDirection(TCA6424A_DEFAULT_ADDRESS, 1, 0x00) ||
        !TCA6424A_setBankDirection(TCA6424A_DEFAULT_ADDRESS, 2, 0xFF))
    {
        return JWPLC_IO_FAULT_TCA_CONFIG;
    }

    gpio_set_level((gpio_num_t)EN_IO, 1);
    vTaskDelay(pdMS_TO_TICKS(2));
    return JWPLC_IO_FAULT_NONE;
}
#endif

uint8_t jwplcSystemIOFault(void)
{
    return g_jwplc_io_fault;
}

void jwplcPeripheralsRetryIO(void)
{
#if defined(JWPLC_BASIC)
    // Solo se reintenta el expansor. Si fallo el I2C tampoco arrancaron RTC,
    // FRAM, SD ni TFT, y no es seguro rehacer ese arranque con el sketch corriendo.
    if (g_jwplc_peripherals_initialized ||
        (g_jwplc_io_fault != JWPLC_IO_FAULT_TCA_INIT &&
         g_jwplc_io_fault != JWPLC_IO_FAULT_TCA_CONFIG))
    {
        return;
    }

    if (jwplcI2C_transactionBegin() != 0)
    {
        return;
    }

    const uint8_t fault = jwplcStartExpander();

    jwplcI2C_transactionEnd();

    if (fault != JWPLC_IO_FAULT_NONE)
    {
        g_jwplc_io_fault = fault;
        return;
    }

    g_jwplc_io_fault = JWPLC_IO_FAULT_NONE;
    g_jwplc_peripherals_initialized = true;
    jwplcSystemSetIOReady(true);
    jwplcSystemMarkDisplayDirty();
    ets_printf("\r\n[JWPLC] E/S recuperadas: TCA6424A configurado y EN_IO (GPIO%d) en 1.\r\n",
                   (int)EN_IO);
#endif
}

void initPeripherals(void)
{
    if (g_jwplc_peripherals_initialized)
    {
        return;
    }

#if defined(JWPLC_BASIC)
    gpio_set_direction((gpio_num_t)EN_IO, GPIO_MODE_OUTPUT);

    // Mantener la TFT en un estado eléctrico conocido desde el primer instante
    // del autoload. El backlight del JWPLC Basic no tiene control por software,
    // así que dejar el ST7789 fuera de reset mientras se inicializan RTC/FRAM/SD
    // puede hacer visible contenido aleatorio de GRAM antes de tft.init().
    gpio_set_direction((gpio_num_t)JWPLC_TFT_CS, GPIO_MODE_OUTPUT);
    gpio_set_level((gpio_num_t)JWPLC_TFT_CS, 1);
    gpio_set_direction((gpio_num_t)JWPLC_TFT_RST, GPIO_MODE_OUTPUT);
    gpio_set_level((gpio_num_t)JWPLC_TFT_RST, 0);

    gpio_set_level((gpio_num_t)EN_IO, 0);
    vTaskDelay(pdMS_TO_TICKS(2));

    jwplcSystemInitState();

    if (jwplcI2C_begin() != 0)
    {
        jwplcReportIOFault(JWPLC_IO_FAULT_I2C);
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

#if JWPLC_HAS_FRAM
    // FRAM no es crítica para permitir que el resto del sistema arranque.
    // Si está habilitada por perfil de hardware, se inicializa automáticamente.
    (void)jwplcFRAMBeginCallback();
#endif

    // microSD no es crítica para permitir que el resto del sistema arranque.
    // El callback también se llama en perfiles sin SD para marcar JWPLC_SD como deshabilitada.
    (void)jwplcSDBeginCallback();

    // La botonera matricial es parte base del ecosistema JWPLC.
    // No es crítica para bloquear el arranque si por algún motivo falla.
    (void)jwplcButtonsBeginCallback();

    // Alpha5 PILOTO: inicializar la TFT antes de setup(), mientras
    // el arranque de periféricos sigue serializado. tft.init()
    // contiene esperas obligatorias del ST7789 y aquí no compite
    // con accesos concurrentes del sketch a FRAM o microSD.
    //
    // Alpha11: si la inicialización física termina correctamente, dibujar el
    // primer frame IDLE aquí mismo. Así la pantalla no depende del primer turno
    // de jwplcSystemTask para abandonar el contenido indeterminado de GRAM.
    if (jwplcDisplayBeginCallback())
    {
        jwplcDisplayRefreshCallback(jwplcGetIOState(), jwplcGetRTCState());
    }

    const uint8_t fault = jwplcStartExpander();

    if (fault != JWPLC_IO_FAULT_NONE)
    {
        jwplcReportIOFault(fault);
        return;
    }

    g_jwplc_peripherals_initialized = true;
    jwplcSystemSetIOReady(true);
#else
    jwplcSystemInitState();
    g_jwplc_peripherals_initialized = true;
    jwplcSystemSetIOReady(true);
#endif
}
