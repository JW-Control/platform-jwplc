#include "jwplc_spi_bus.h"

#include <Arduino.h>

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include "jwplc_hardware_config.h"

static SemaphoreHandle_t g_spiMutex = nullptr;
static bool g_spiReady = false;

static TickType_t jwplcSPI_timeoutTicks(uint32_t timeoutMs)
{
    if (timeoutMs == 0)
    {
        return 0;
    }

    if (timeoutMs == 0xFFFFFFFFUL)
    {
        return portMAX_DELAY;
    }

    return pdMS_TO_TICKS(timeoutMs);
}

static void jwplcSPI_configureCSPins()
{
    pinMode(JWPLC_TFT_CS, OUTPUT);
    digitalWrite(JWPLC_TFT_CS, HIGH);

#if JWPLC_HAS_SD
    pinMode(JWPLC_SD_CS, OUTPUT);
    digitalWrite(JWPLC_SD_CS, HIGH);
#endif

#if JWPLC_HAS_FRAM
    pinMode(JWPLC_FRAM_CS, OUTPUT);
    digitalWrite(JWPLC_FRAM_CS, HIGH);
#endif

#if JWPLC_HAS_ETHERNET
    pinMode(JWPLC_ETH_CS, OUTPUT);
    digitalWrite(JWPLC_ETH_CS, HIGH);
#endif
}

bool jwplcSPI_begin(void)
{
    if (g_spiMutex == nullptr)
    {
        g_spiMutex = xSemaphoreCreateMutex();

        if (g_spiMutex == nullptr)
        {
            return false;
        }
    }

    if (!g_spiReady)
    {
        jwplcSPI_configureCSPins();
        jwplcSPI_deselectAll();
        g_spiReady = true;
    }

    return true;
}

bool jwplcSPI_isReady(void)
{
    return g_spiReady;
}

bool jwplcSPI_acquire(uint32_t timeoutMs)
{
    if (!jwplcSPI_begin())
    {
        return false;
    }

    return xSemaphoreTake(g_spiMutex, jwplcSPI_timeoutTicks(timeoutMs)) == pdTRUE;
}

void jwplcSPI_release(void)
{
    if (g_spiMutex != nullptr)
    {
        xSemaphoreGive(g_spiMutex);
    }
}

void jwplcSPI_deselectAll(void)
{
    digitalWrite(JWPLC_TFT_CS, HIGH);

#if JWPLC_HAS_SD
    digitalWrite(JWPLC_SD_CS, HIGH);
#endif

#if JWPLC_HAS_FRAM
    digitalWrite(JWPLC_FRAM_CS, HIGH);
#endif

#if JWPLC_HAS_ETHERNET
    digitalWrite(JWPLC_ETH_CS, HIGH);
#endif
}

void jwplcSPI_prepareForTFT(void)
{
#if JWPLC_HAS_SD
    digitalWrite(JWPLC_SD_CS, HIGH);
#endif

#if JWPLC_HAS_FRAM
    digitalWrite(JWPLC_FRAM_CS, HIGH);
#endif

#if JWPLC_HAS_ETHERNET
    digitalWrite(JWPLC_ETH_CS, HIGH);
#endif

    digitalWrite(JWPLC_TFT_CS, HIGH);
}