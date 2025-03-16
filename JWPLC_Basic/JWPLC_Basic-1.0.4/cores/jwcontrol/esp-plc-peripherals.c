#include <stdint.h>
#include <stddef.h>
#include <errno.h>
#include "esp32-hal-gpio.h"

const int I2C_BUS = 0;

const uint8_t NORMAL_GPIO_INPUT = 0x01;
const uint8_t NORMAL_GPIO_OUTPUT = 0x03;

const uint8_t ARRAY_TCA6424A[] = {
#if defined(JWPLC)
	0x22,
#endif // JWPLC
};
const size_t NUM_ARRAY_TCA6424A = sizeof(ARRAY_TCA6424A) / sizeof(uint8_t);

#ifdef JWPLC_BASIC_4DI_8DO_2MO
static bool channels_used[16];
typedef enum
{
	ch_q0_8 = 0,
	ch_q0_9 = 2,
	NO_CHANNEL,
	ERR = -1
} q_channel_t;

static q_channel_t return_channel(uint32_t pin)
{
	q_channel_t channel;
	switch (pin)
	{
	case Q0_8:
		channel = ch_q0_8;
		break;
	case Q0_9:
		channel = ch_q0_9;
		break;
	default:
		channel = NO_CHANNEL;
		break;
	}

	return channel;
}

static uint32_t return_pin(uint8_t channel)
{
	uint32_t pin;
	switch (channel)
	{
	case ch_q0_8:
		pin = Q0_8;
		break;
	case ch_q0_9:
		pin = Q0_9;
		break;
	default:
		pin = 255;
		errno = EINVAL;
		break;
	}

	return pin;
}

static int is_channel_initialized(uint32_t pin)
{
	q_channel_t ch = return_channel(pin);
	if (ch == ERR)
	{
		return -1;
	}

	return channels_used[ch] ? 1 : 0;
}

static int init_pin(uint32_t pin, q_channel_t ch, uint32_t freq)
{
	uint32_t new_freq = ledcSetup(ch, freq, 12);
	if (new_freq != freq)
		return -1;

	ledcAttachPin(pin, ch);

	channels_used[ch] = true;

	return 0;
}

static int deinit_pin(uint32_t pin, q_channel_t ch)
{
	ledcDetachPin(pin);

	channels_used[ch] = false;

	return 0;
}
#endif

int normal_gpio_init(void)
{
#ifdef JWPLC_BASIC_4DI_8DO_2MO
	memset(channels_used, false, sizeof(channels_used));
#endif
	return 0;
}

int normal_gpio_deinit(void)
{
#ifdef JWPLC_BASIC_4DI_8DO_2MO
	for (size_t c = 0; c < sizeof(channels_used) / sizeof(bool); c++)
	{
		if (channels_used[c])
		{
			uint32_t pin = return_pin(c);
			if (pin == 255 || deinit_pin(pin, c) != 0)
			{
				errno = EFAULT;
				return -1;
			}
		}
	}
#endif
	return 0;
}

extern void ARDUINO_ISR_ATTR __pinMode(uint32_t pin, uint8_t mode);
int normal_gpio_set_pin_mode(uint32_t pin, uint8_t mode)
{
	__pinMode(pin, mode);
	return 0;
}

extern void ARDUINO_ISR_ATTR __digitalWrite(uint32_t pin, uint8_t val);
int normal_gpio_write(uint32_t pin, uint8_t value)
{
	__digitalWrite(pin, value);
	return 0;
}

int normal_gpio_pwm_frequency(uint32_t pin, uint32_t freq)
{
#ifdef JWPLC_BASIC_4DI_8DO_2MO
	q_channel_t ch = return_channel(pin);
	int is_channel_used = is_channel_initialized(pin);
	if (is_channel_used < 0)
	{
		return is_channel_used;
	}

	if (is_channel_used == 1)
	{
		ledcChangeFrequency(ch, freq, 12);
	}
	else
	{
		init_pin(pin, ch, freq);
	}

	return 0;
#else
	errno = ENOTSUP;
	return -1;
#endif
}

int normal_gpio_pwm_write(uint32_t pin, uint16_t value)
{
#ifdef JWPLC_BASIC_4DI_8DO_2MO
	q_channel_t ch = return_channel(pin);
	int is_channel_used = is_channel_initialized(pin);
	if (is_channel_used < 0)
	{
		return is_channel_used;
	}

	if (is_channel_used == 0)
	{
		init_pin(pin, ch, 500);
	}

	ledcWrite(ch, value);

	return 0;
#else
	errno = ENOTSUP;
	return -1;
#endif
}

extern int ARDUINO_ISR_ATTR __digitalRead(uint32_t pin);
extern uint16_t __analogRead(uint32_t pin);
int normal_gpio_read(uint32_t pin, uint8_t *read)
{
#ifdef JWPLC_BASIC_4DI_8DO_2MO
	q_channel_t ch = return_channel(pin);
	int is_channel_used = is_channel_initialized(pin);
	if (is_channel_used < 0)
	{
		return is_channel_used;
	}

	if (is_channel_used == 1)
	{
		deinit_pin(pin, ch);
	}
#endif

#ifdef JWPLC_BASIC_4DI_8DO_2MO
	// If we use 14 IOs we should be able to use, we should be able
	// to use ESP32 ADCs as digital pins
	uint16_t analog_read;
	switch (pin)
	{
	case 32:
	case 33:
		// Up to ~2.7V
		normal_gpio_analog_read(pin, &analog_read); // Applying correction facto
		*read = analog_read > 3500 ? 1 : 0;
		break;
	case 34:
	case 35:
		// Up to ~3.3V
		normal_gpio_analog_read(pin, &analog_read);
		*read = analog_read > 1200 ? 1 : 0;
		break;
	default:
		*read = __digitalRead(pin);
		break;
	}
#else
	*read = __digitalRead(pin);
#endif

	return 0;
}

int normal_gpio_analog_read(uint32_t pin, uint16_t *read)
{
#ifdef JWPLC_BASIC_4DI_8DO_2MO
	q_channel_t ch = return_channel(pin);
	int is_channel_used = is_channel_initialized(pin);
	if (is_channel_used < 0)
	{
		return is_channel_used;
	}

	if (is_channel_used == 1)
	{
		deinit_pin(pin, ch);
	}
#endif
	uint16_t sample = __analogRead(pin);

/*
 * The analog IOs of the ESP32 aren't able to read the full range of values.
 * Based off our tests, the analog inputs at 10V read as 2992 to 2957, which is around 11.5 bits of precision.
 * To use all the 12 bits of precision, we'll have to scale it up later.
 * Voltages below 0.5V aren't detected. Calculating a regression line we can tell
 * the values have an offset of aproximately -171.
 * Combining both values we can add SAMPLE + 171 and scale the value to obtain the maximum range.
 * The scaling factor is obtained with the following: 4096/(2992+171) = ~ 1.2950
 *
 * The final operation would be (SAMPLE + 171)* 1.2950
 * This value correction is performed by the function:
 *      correctionFactor(int sample,float correction_factor)
 * Limitation: Values below 0.5V still won't be detected correctly.
 * There will be an abrupt jump between 0 and 222.
 *
 */
#if defined(JWPLC_BASIC_4DI_8DO_2MO)
	const float CORRECTION_FACTOR = (4096.0 / (2992 + 171));
	if (sample > 0)
		sample += 171;
	sample = (sample * CORRECTION_FACTOR);
	if (sample > 4095)
	{
		sample = 4095;
	}
#endif

	*read = sample;
	return 0;
}
