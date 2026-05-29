#include "WireC.h"

#include <stdbool.h>

#include "driver/i2c.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "esp_err.h"

#define I2C_MASTER_NUM            I2C_NUM_0
#define I2C_MASTER_FREQ_HZ        100000
#define I2C_MASTER_TX_BUF_DISABLE 0
#define I2C_MASTER_RX_BUF_DISABLE 0
#define ACK_CHECK_EN              0x1
#define WIREC_TIMEOUT_MS          1000

#define WIREC_SDA_PIN             GPIO_NUM_21
#define WIREC_SCL_PIN             GPIO_NUM_22

static i2c_port_t i2c_master_port = I2C_MASTER_NUM;
static bool g_wirec_initialized = false;

static int WireC_cmd_begin(i2c_cmd_handle_t cmd) {
    int ret = i2c_master_cmd_begin(i2c_master_port, cmd, pdMS_TO_TICKS(WIREC_TIMEOUT_MS));
    i2c_cmd_link_delete(cmd);
    return ret;
}

int WireC_begin(void) {
    if (g_wirec_initialized) {
        return 0;
    }

    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = WIREC_SDA_PIN,
        .scl_io_num = WIREC_SCL_PIN,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_MASTER_FREQ_HZ,
        .clk_flags = 0
    };

    esp_err_t err = i2c_param_config(i2c_master_port, &conf);
    if (err != ESP_OK) {
        return err;
    }

    err = i2c_driver_install(
        i2c_master_port,
        conf.mode,
        I2C_MASTER_RX_BUF_DISABLE,
        I2C_MASTER_TX_BUF_DISABLE,
        0
    );

    if (err == ESP_ERR_INVALID_STATE) {
        g_wirec_initialized = true;
        return 0;
    }

    if (err != ESP_OK) {
        return err;
    }

    g_wirec_initialized = true;
    return 0;
}

int WireC_readByte(uint8_t devAddr, uint8_t regAddr, uint8_t *data) {
    if (data == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (devAddr << 1) | I2C_MASTER_WRITE, ACK_CHECK_EN);
    i2c_master_write_byte(cmd, regAddr, ACK_CHECK_EN);
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (devAddr << 1) | I2C_MASTER_READ, ACK_CHECK_EN);
    i2c_master_read_byte(cmd, data, I2C_MASTER_NACK);
    i2c_master_stop(cmd);
    return WireC_cmd_begin(cmd);
}

int WireC_readBytes(uint8_t devAddr, uint8_t regAddr, uint8_t length, uint8_t *data) {
    if (data == NULL || length == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (devAddr << 1) | I2C_MASTER_WRITE, ACK_CHECK_EN);
    i2c_master_write_byte(cmd, regAddr, ACK_CHECK_EN);
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (devAddr << 1) | I2C_MASTER_READ, ACK_CHECK_EN);
    i2c_master_read(cmd, data, length, I2C_MASTER_LAST_NACK);
    i2c_master_stop(cmd);
    return WireC_cmd_begin(cmd);
}

int WireC_readBit(uint8_t devAddr, uint8_t regAddr, uint8_t bitNum, uint8_t *data) {
    uint8_t byteData = 0;
    int ret = WireC_readByte(devAddr, regAddr, &byteData);
    if (ret != 0) {
        return ret;
    }

    *data = (byteData >> bitNum) & 0x01;
    return 0;
}

int WireC_writeByte(uint8_t devAddr, uint8_t regAddr, uint8_t data) {
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (devAddr << 1) | I2C_MASTER_WRITE, ACK_CHECK_EN);
    i2c_master_write_byte(cmd, regAddr, ACK_CHECK_EN);
    i2c_master_write_byte(cmd, data, ACK_CHECK_EN);
    i2c_master_stop(cmd);
    return WireC_cmd_begin(cmd);
}

int WireC_writeBytes(uint8_t devAddr, uint8_t regAddr, uint8_t length, uint8_t *data) {
    if (data == NULL || length == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (devAddr << 1) | I2C_MASTER_WRITE, ACK_CHECK_EN);
    i2c_master_write_byte(cmd, regAddr, ACK_CHECK_EN);
    i2c_master_write(cmd, data, length, ACK_CHECK_EN);
    i2c_master_stop(cmd);
    return WireC_cmd_begin(cmd);
}

int WireC_writeBit(uint8_t devAddr, uint8_t regAddr, uint8_t bitNum, uint8_t data) {
    uint8_t byteData = 0;
    int ret = WireC_readByte(devAddr, regAddr, &byteData);
    if (ret != 0) {
        return ret;
    }

    if (data != 0) {
        byteData |= (1 << bitNum);
    } else {
        byteData &= ~(1 << bitNum);
    }

    return WireC_writeByte(devAddr, regAddr, byteData);
}