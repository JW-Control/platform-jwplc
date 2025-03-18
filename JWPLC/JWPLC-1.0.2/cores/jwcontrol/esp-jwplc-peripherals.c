#include <stdint.h>
#include <stddef.h>
#include <errno.h>

#include "esp32-hal-gpio.h"

const int I2C_BUS = 0;

const uint8_t NORMAL_GPIO_INPUT = 0x01;
const uint8_t NORMAL_GPIO_OUTPUT = 0x03;

const uint8_t ARRAY_TCA6424A[] = {
#if defined(JWPLC_BASIC)
	0x22,
#endif // JWPLC
};
const size_t NUM_ARRAY_TCA6424A = sizeof(ARRAY_TCA6424A) / sizeof(uint8_t);

#ifdef JWPLC_BASIC
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
	if (!ledcAttachChannel(pin, freq, 12, ch))
	{
		return -1;
	}
	channels_used[ch] = true;
	return 0;
}

static int deinit_pin(uint32_t pin, q_channel_t ch)
{
	if (!ledcDetach(pin))
	{
		return -1;
	}
	channels_used[ch] = false;
	return 0;
}
#endif

int normal_gpio_init(void)
{
#ifdef PLC14IOS
	memset(channels_used, false, sizeof(channels_used));
#endif
	return 0;
}

int normal_gpio_deinit(void)
{
#ifdef PLC14IOS
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
#ifdef JWPLC_BASIC
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
	__pinMode(pin, mode);
	return 0;
}

extern void ARDUINO_ISR_ATTR __digitalWrite(uint32_t pin, uint8_t val);
int normal_gpio_write(uint32_t pin, uint8_t value)
{
#ifdef JWPLC_BASIC
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
	__digitalWrite(pin, value);
	return 0;
}

int normal_gpio_pwm_frequency(uint32_t pin, uint32_t freq)
{
#ifdef JWPLC_BASIC
	q_channel_t ch = return_channel(pin);
	int is_channel_used = is_channel_initialized(pin);
	if (is_channel_used < 0)
	{
		return is_channel_used;
	}

	if (is_channel_used == 1)
	{
		ledcChangeFrequency(pin, freq, 12);
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
#ifdef JWPLC_BASIC
	q_channel_t ch = return_channel(pin);
	int is_channel_used = is_channel_initialized(pin);
	if (is_channel_used < 0)
	{
		return is_channel_used;
	}

	if (is_channel_used == 0)
	{
		// Si no está inicializado, se inicializa con una frecuencia base (500 Hz en este caso)
		if (init_pin(pin, ch, 500) != 0)
		{
			return -1;
		}
	}

	// En la nueva API se utiliza el número de pin, no el canal
	ledcWrite(pin, value);

	return 0;
#else
	errno = ENOTSUP;
	return -1;
#endif
}

extern int ARDUINO_ISR_ATTR __digitalRead(uint32_t pin);
int normal_gpio_read(uint32_t pin, uint8_t *read)
{
#ifdef JWPLC_BASIC
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
	*read = __digitalRead(pin);
	return 0;
}

extern uint16_t __analogRead(uint32_t pin);
int normal_gpio_analog_read(uint32_t pin, uint16_t *read)
{
#ifdef JWPLC_BASIC
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
	*read = sample;
	return 0;
}