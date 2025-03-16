/*
 * Copyright (c) 2024 Industrial Shields. All rights reserved
 *
 * This file is part of plc-peripherals.
 *
 * plc-peripherals is free software: you can redistribute
 * it and/or modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation, either version
 * 3 of the License, or (at your option) any later version.
 *
 * plc-peripherals is distributed in the hope that it will
 * be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "expanded-gpio.h"
#include "WireC.h"
#include "peripheral-tca6424a.h"


/**
 * @brief Checks if an address exists in an array of addresses.
 *
 * @param addr The address to check.
 * @param arr Pointer to the array of addresses.
 * @param len Length of the array.
 * @return 0 if the address is found, -1 otherwise.
 */
static int isAddressIntoArray(uint8_t addr, const uint8_t *arr, uint8_t len){
	while (len--)
	{
		if (*arr++ == addr)
		{
			return 0;
		}
	}

	return -1;
}


int initExpandedGPIO(bool restart_peripherals){
	int ret = -1;

	ret = normal_gpio_init();
    if (ret != 0) {
        return NORMAL_GPIO_INIT_FAIL;
    }

    if (I2C_BUS != PERIPHERALS_NO_I2C_BUS) {
        // Inicializar WireC
        if (WireC_begin() != 0) {
            return I2C_ALREADY_INITIALIZED; // Error al inicializar I2C
        }

        // Inicializar TCA6424A
        if (!TCA6424A_init(TCA6424A_DEFAULT_ADDRESS)) {
            return ARRAY_TCA6424A_INIT_FAIL;
        }
    }

    return 0; // Éxito
}

int deinitExpandedGPIO(void){
	int ret = -1;
	ret = normal_gpio_deinit();
    return ret == 0 ? 0 : NORMAL_GPIO_DEINIT_FAIL;
}

int pinMode(uint32_t pin, uint8_t mode){
	int ret = -1;

	uint8_t addr = pinToDeviceAddress(pin);
	uint8_t index = pinToDeviceIndex(pin);

	if (addr == 0)	{
		ret = normal_gpio_set_pin_mode(index, mode == OUTPUT ? NORMAL_GPIO_OUTPUT : NORMAL_GPIO_INPUT);
		return ret == 0 ? 0 : NORMAL_GPIO_SET_PIN_MODE_FAIL;
	}

	else if (I2C_BUS == PERIPHERALS_NO_I2C_BUS)	{
		return I2C_PIN_WITHOUT_I2C_BUS;
	}

	if (isAddressIntoArray(addr, ARRAY_TCA6424A, NUM_ARRAY_TCA6424A) == 0)	{
		ret = TCA6424A_setPinDirection(addr, index, mode == OUTPUT ? TCA6424A_OUTPUT : TCA6424A_INPUT);
		return ret? 0 : ARRAY_TCA6424A_SET_PIN_MODE_FAIL;
	}
}

int digitalWrite(uint32_t pin, uint8_t value){
	int ret = -1;

	uint8_t addr = pinToDeviceAddress(pin);
	uint8_t index = pinToDeviceIndex(pin);

	if (addr == 0)	{
		ret = normal_gpio_write(index, value);
		return ret == 0 ? 0 : NORMAL_GPIO_WRITE_FAIL;
	}

	else if (I2C_BUS == PERIPHERALS_NO_I2C_BUS)	{
		return I2C_PIN_WITHOUT_I2C_BUS;
	}

	else if (isAddressIntoArray(addr, ARRAY_TCA6424A, NUM_ARRAY_TCA6424A) == 0)	{
		ret = TCA6424A_writePin(addr, index, value);
		return ret? 0 : ARRAY_TCA6424A_WRITE_FAIL;
	}

	return 0;
}

int digitalRead(uint32_t pin){
	uint8_t addr = pinToDeviceAddress(pin);
	uint8_t index = pinToDeviceIndex(pin);
	uint16_t value = 0;

	int ret = -1;

	if (addr == 0)	{
		ret = normal_gpio_read(index, (uint8_t *)&value);
		return ret == 0 ? (int)value : NORMAL_GPIO_READ_FAIL;
	}

	else if (I2C_BUS == PERIPHERALS_NO_I2C_BUS)	{
		return I2C_PIN_WITHOUT_I2C_BUS;
	}

    if (isAddressIntoArray(addr, ARRAY_TCA6424A, NUM_ARRAY_TCA6424A) == 0)    {
        return TCA6424A_readPin(addr, index, &value) ? value : ARRAY_TCA6424A_READ_FAIL;
    }
}

int analogWrite(uint32_t pin, uint16_t value){
	uint8_t addr = pinToDeviceAddress(pin);
	uint8_t index = pinToDeviceIndex(pin);

	int ret = -1;

	if (addr == 0)	{
		ret = normal_gpio_pwm_write(index, value);
		return ret == 0 ? 0 : NORMAL_GPIO_PWM_WRITE_FAIL;
	}

	else if (I2C_BUS == PERIPHERALS_NO_I2C_BUS)	{
		return I2C_PIN_WITHOUT_I2C_BUS;
	}

	return NORMAL_GPIO_PWM_WRITE_FAIL; // Para expansión, no implementado aún
}

int analogWriteSetFrequency(uint32_t pin, uint32_t desired_freq){
	uint8_t addr = pinToDeviceAddress(pin);
	uint8_t index = pinToDeviceIndex(pin);

	int ret = -1;

	if (addr == 0)	{
		ret = normal_gpio_pwm_frequency(index, desired_freq);
		return ret == 0 ? 0 : NORMAL_GPIO_PWM_CHANGE_FREQ_FAIL;
	}

	else if (I2C_BUS == PERIPHERALS_NO_I2C_BUS)	{
		return I2C_PIN_WITHOUT_I2C_BUS;
	}

	return NORMAL_GPIO_PWM_CHANGE_FREQ_FAIL; // Para expansión, no implementado aún
}

uint16_t analogRead(uint32_t pin){
	uint8_t addr = pinToDeviceAddress(pin);
	uint8_t index = pinToDeviceIndex(pin);
	uint16_t value = 0;

	int ret = -1;

	if (addr == 0)	{
		ret = normal_gpio_analog_read(index, &value);
		return ret == 0 ? value : 0;
	}

	else if (I2C_BUS == PERIPHERALS_NO_I2C_BUS)	{
		return I2C_PIN_WITHOUT_I2C_BUS;
	}

	return 0;
}