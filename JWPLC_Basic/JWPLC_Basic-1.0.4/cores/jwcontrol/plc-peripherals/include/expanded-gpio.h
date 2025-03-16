#ifndef __EXPANDED_GPIO_H__
#define __EXPANDED_GPIO_H__

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#define INPUT 0
#define OUTPUT 1

#define LOW 0
#define HIGH 1

#define PERIPHERALS_NO_I2C_BUS -1

// Macros to extract device address and index from a pin number
#define pinToDeviceAddress(pin) (((pin) >> 8) & 0xff)
#define pinToDeviceIndex(pin) ((pin)&0xff)

#ifdef __cplusplus
extern "C" {
#endif

	extern const int I2C_BUS;

	extern const uint8_t NORMAL_GPIO_INPUT;
	extern const uint8_t NORMAL_GPIO_OUTPUT;

	extern const uint8_t ARRAY_TCA6424A[];
	extern const size_t NUM_ARRAY_TCA6424A;


	extern int normal_gpio_init(void);
	extern int normal_gpio_deinit(void);

	extern int normal_gpio_set_pin_mode(uint32_t pin, uint8_t mode);

	extern int normal_gpio_write(uint32_t pin, uint8_t value);

	extern int normal_gpio_pwm_frequency(uint32_t pin, uint32_t freq);

	extern int normal_gpio_pwm_write(uint32_t pin, uint16_t value);

	extern int normal_gpio_read(uint32_t pin, uint8_t* read);

	extern int normal_gpio_analog_read(uint32_t pin, uint16_t* read);


	#if defined(PLC_ENVIRONMENT) && PLC_ENVIRONMENT == Linux
	void delay(uint32_t milliseconds);

	void delayMicroseconds(uint32_t micros);
	#endif


	// Error codes for error handling
	enum {
		// Initialization and Deinitialization Errors
		I2C_ALREADY_INITIALIZED               = 1,
		I2C_ALREADY_DEINITIALIZED             = 2,
		NORMAL_GPIO_INIT_FAIL                 = 3,
		NORMAL_GPIO_DEINIT_FAIL               = 4,
		ARRAY_TCA6424A_INIT_FAIL              = 5,

		// pinMode Errors
		NORMAL_GPIO_SET_PIN_MODE_FAIL         = 6,
		ARRAY_TCA6424A_SET_PIN_MODE_FAIL      = 7,

		// digitalRead Errors
		NORMAL_GPIO_READ_FAIL                 = 8,
		ARRAY_TCA6424A_READ_FAIL             = 9,

		// digitalWrite Errors
		NORMAL_GPIO_WRITE_FAIL                = 10,
		ARRAY_TCA6424A_WRITE_FAIL             = 11,

		// PWM Errors
		NORMAL_GPIO_PWM_WRITE_FAIL            = 12,

		// PWM Frequency Change Errors
		NORMAL_GPIO_PWM_CHANGE_FREQ_FAIL      = 13,

		// Others
		I2C_PIN_WITHOUT_I2C_BUS               = 32
	};


	// Exported functions, useful for Arduino and other environments

	/**
	 * @brief Initializes the expanded GPIO devices.
	 *
	 * This function initializes all the GPIOs defined in the library.
	 *
	 * @param restart_peripherals Flag indicating whether to restart the peripherals on initialization failure.
	 * @return 0 if successful, appropriate error code otherwise.
	 */
	int initExpandedGPIO(bool restart_peripherals);

	/**
	 * @brief De-initializes the expanded GPIO devices.
	 *
	 * This function de-initializes all the GPIOs defined in the library.
	 *
	 * @param restart_peripherals Flag indicating whether to restart the peripherals on initialization failure.
	 * @return 0 if successful, appropriate error code otherwise.
	 */
	int deinitExpandedGPIO(void);

	/**
	 * @brief Sets the pin mode for a specified pin.
	 *
	 * @param pin The pin number.
	 * @param mode The mode to set (INPUT or OUTPUT).
	 * @return 0 if successful, appropriate error code otherwise.
	 */
	int pinMode(uint32_t pin, uint8_t mode);

	/**
	 * @brief Writes a digital value to the specified pin.
	 *
	 * @param pin The pin number.
	 * @param value The digital value to write (LOW or HIGH).
	 * @return 0 if successful, appropriate error code otherwise.
	 */
	int digitalWrite(uint32_t pin, uint8_t value);

	/**
	 * @brief Reads the digital value from the specified pin.
	 *
	 * @param pin The pin number.
	 * @return The digital value read from the pin (LOW or HIGH).
	 */
	int digitalRead(uint32_t pin);

	/**
	 * @brief Writes an analog value to the specified pin.
	 *
	 * @param pin The pin number.
	 * @param value The analog (or PWM) value to write (0-4095).
	 * @return 0 if successful, appropriate error code otherwise.
	 */
	int analogWrite(uint32_t pin, uint16_t value);

	/**
	 * @brief Sets the PWM frequency for the specified pin.
	 *
	 * @param pin The pin number.
	 * @param desired_freq The desired PWM frequency in Hertz.
	 * @return 0 if successful, appropriate error code otherwise.
	 */
	int analogWriteSetFrequency(uint32_t pin, uint32_t desired_freq);

	/**
	 * @brief Reads the analog value from the specified pin.
	 *
	 * @param pin The pin number.
	 * @return The analog value read from the pin (0-4095).
	 */
	uint16_t analogRead(uint32_t pin);

#ifdef __cplusplus
}
#endif


#endif // __EXPANDED_GPIO_H__
