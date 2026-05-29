#include "jwplc_i2c_bridge.h"

#include <Arduino.h>
#include <stdlib.h>
#include <string.h>

#include "esp_err.h"
#include "esp32-hal-i2c.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

static constexpr uint8_t  JWPLC_I2C_PORT        = 0;
static constexpr uint8_t  JWPLC_I2C_DEFAULT_SDA = SDA;
static constexpr uint8_t  JWPLC_I2C_DEFAULT_SCL = SCL;
static constexpr uint32_t JWPLC_I2C_DEFAULT_HZ  = 100000;
static constexpr uint16_t JWPLC_I2C_TIMEOUT_MS  = 50;

static SemaphoreHandle_t g_jwplcI2CMutex = NULL;
static bool g_jwplcI2CInitialized = false;
static uint8_t g_jwplcI2CSda = JWPLC_I2C_DEFAULT_SDA;
static uint8_t g_jwplcI2CScl = JWPLC_I2C_DEFAULT_SCL;
static uint32_t g_jwplcI2CFreq = JWPLC_I2C_DEFAULT_HZ;

static bool jwplcI2CEnsureMutex(void)
{
    if (g_jwplcI2CMutex != NULL)
    {
        return true;
    }

    g_jwplcI2CMutex = xSemaphoreCreateRecursiveMutex();
    return (g_jwplcI2CMutex != NULL);
}

static bool jwplcI2CLock(void)
{
    if (!jwplcI2CEnsureMutex())
    {
        return false;
    }

    return (xSemaphoreTakeRecursive(g_jwplcI2CMutex, portMAX_DELAY) == pdTRUE);
}

static void jwplcI2CUnlock(void)
{
    if (g_jwplcI2CMutex != NULL)
    {
        xSemaphoreGiveRecursive(g_jwplcI2CMutex);
    }
}

static int jwplcI2CEnsureStartedLocked(void)
{
    if (!i2cIsInit(JWPLC_I2C_PORT))
    {
        esp_err_t err = i2cInit(JWPLC_I2C_PORT, g_jwplcI2CSda, g_jwplcI2CScl, g_jwplcI2CFreq);
        if (err != ESP_OK)
        {
            g_jwplcI2CInitialized = false;
            return err;
        }
    }

    esp_err_t clkErr = i2cSetClock(JWPLC_I2C_PORT, g_jwplcI2CFreq);
    if (clkErr != ESP_OK)
    {
        g_jwplcI2CInitialized = false;
        return clkErr;
    }

    g_jwplcI2CInitialized = true;
    return ESP_OK;
}

int jwplcI2C_begin(void)
{
    return jwplcI2C_beginWithPins(JWPLC_I2C_DEFAULT_SDA, JWPLC_I2C_DEFAULT_SCL, JWPLC_I2C_DEFAULT_HZ);
}

int jwplcI2C_beginWithPins(uint8_t sdaPin, uint8_t sclPin, uint32_t frequencyHz)
{
    if (!jwplcI2CLock())
    {
        return ESP_FAIL;
    }

    g_jwplcI2CSda  = sdaPin;
    g_jwplcI2CScl  = sclPin;
    g_jwplcI2CFreq = (frequencyHz == 0) ? JWPLC_I2C_DEFAULT_HZ : frequencyHz;

    int ret = jwplcI2CEnsureStartedLocked();

    jwplcI2CUnlock();
    return ret;
}

int jwplcI2C_setClock(uint32_t frequencyHz)
{
    if (frequencyHz == 0)
    {
        return ESP_ERR_INVALID_ARG;
    }

    if (!jwplcI2CLock())
    {
        return ESP_FAIL;
    }

    g_jwplcI2CFreq = frequencyHz;
    int ret = jwplcI2CEnsureStartedLocked();

    jwplcI2CUnlock();
    return ret;
}

int jwplcI2C_probe(uint8_t address)
{
    uint8_t value = 0;
    return jwplcI2C_readReg8(address, 0x00, &value);
}

int jwplcI2C_readReg8(uint8_t address, uint8_t reg, uint8_t *data)
{
    if (data == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    if (!jwplcI2CLock())
    {
        return ESP_FAIL;
    }

    int ret = jwplcI2CEnsureStartedLocked();
    if (ret != ESP_OK)
    {
        jwplcI2CUnlock();
        return ret;
    }

    size_t rxLength = 0;
    uint8_t regAddr = reg;
    esp_err_t err = i2cWriteReadNonStop(
        JWPLC_I2C_PORT,
        address,
        &regAddr,
        1,
        data,
        1,
        JWPLC_I2C_TIMEOUT_MS,
        &rxLength
    );

    jwplcI2CUnlock();

    if (err != ESP_OK)
    {
        return err;
    }

    return (rxLength == 1) ? ESP_OK : ESP_FAIL;
}

int jwplcI2C_readRegs(uint8_t address, uint8_t startReg, uint8_t length, uint8_t *data)
{
    if (data == NULL || length == 0)
    {
        return ESP_ERR_INVALID_ARG;
    }

    if (!jwplcI2CLock())
    {
        return ESP_FAIL;
    }

    int ret = jwplcI2CEnsureStartedLocked();
    if (ret != ESP_OK)
    {
        jwplcI2CUnlock();
        return ret;
    }

    size_t rxLength = 0;
    uint8_t regAddr = startReg;
    esp_err_t err = i2cWriteReadNonStop(
        JWPLC_I2C_PORT,
        address,
        &regAddr,
        1,
        data,
        length,
        JWPLC_I2C_TIMEOUT_MS,
        &rxLength
    );

    jwplcI2CUnlock();

    if (err != ESP_OK)
    {
        return err;
    }

    return (rxLength == length) ? ESP_OK : ESP_FAIL;
}

int jwplcI2C_writeReg8(uint8_t address, uint8_t reg, uint8_t data)
{
    if (!jwplcI2CLock())
    {
        return ESP_FAIL;
    }

    int ret = jwplcI2CEnsureStartedLocked();
    if (ret != ESP_OK)
    {
        jwplcI2CUnlock();
        return ret;
    }

    uint8_t tx[2];
    tx[0] = reg;
    tx[1] = data;

    esp_err_t err = i2cWrite(
        JWPLC_I2C_PORT,
        address,
        tx,
        sizeof(tx),
        JWPLC_I2C_TIMEOUT_MS
    );

    jwplcI2CUnlock();
    return err;
}

int jwplcI2C_writeRegs(uint8_t address, uint8_t startReg, uint8_t length, const uint8_t *data)
{
    if (data == NULL || length == 0)
    {
        return ESP_ERR_INVALID_ARG;
    }

    if (!jwplcI2CLock())
    {
        return ESP_FAIL;
    }

    int ret = jwplcI2CEnsureStartedLocked();
    if (ret != ESP_OK)
    {
        jwplcI2CUnlock();
        return ret;
    }

    uint8_t *tx = (uint8_t *)malloc((size_t)length + 1);
    if (tx == NULL)
    {
        jwplcI2CUnlock();
        return ESP_ERR_NO_MEM;
    }

    tx[0] = startReg;
    memcpy(&tx[1], data, length);

    esp_err_t err = i2cWrite(
        JWPLC_I2C_PORT,
        address,
        tx,
        (size_t)length + 1,
        JWPLC_I2C_TIMEOUT_MS
    );

    free(tx);
    jwplcI2CUnlock();

    return err;
}

int jwplcI2C_updateBit(uint8_t address, uint8_t reg, uint8_t bitNum, uint8_t bitValue)
{
    if (bitNum > 7)
    {
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t value = 0;
    int ret = jwplcI2C_readReg8(address, reg, &value);
    if (ret != ESP_OK)
    {
        return ret;
    }

    if (bitValue)
    {
        value |= (uint8_t)(1u << bitNum);
    }
    else
    {
        value &= (uint8_t)~(1u << bitNum);
    }

    return jwplcI2C_writeReg8(address, reg, value);
}