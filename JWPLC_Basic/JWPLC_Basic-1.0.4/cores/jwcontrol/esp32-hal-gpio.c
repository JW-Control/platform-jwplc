// Copyright 2015-2016 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at

//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "esp32-hal-gpio.h"
#undef INPUT
#define INPUT             0x01
#undef OUTPUT
#define OUTPUT            0x03

#include "hal/gpio_hal.h"
#include "soc/soc_caps.h"

// It fixes lack of pin definition for S3 and for any future SoC
// this function works for ESP32, ESP32-S2 and ESP32-S3 - including the C3, it will return -1 for any pin
#if SOC_TOUCH_SENSOR_NUM >  0
#include "soc/touch_sensor_periph.h"

int8_t digitalPinToTouchChannel(uint8_t pin) 
{
    int8_t ret = -1;
    if (pin < SOC_GPIO_PIN_COUNT) {
        for (uint8_t i = 0; i < SOC_TOUCH_SENSOR_NUM; i++) {
            if (touch_sensor_channel_io_map[i] == pin) {
                ret = i;
                break;
            }
        }
    }
    return ret;
}
#else
// No Touch Sensor available
int8_t digitalPinToTouchChannel(uint8_t pin) 
{
    return -1;
}
#endif

#ifdef SOC_ADC_SUPPORTED
#include "soc/adc_periph.h"

int8_t digitalPinToAnalogChannel(uint8_t pin) 
{
    uint8_t channel = 0;
    if (pin < SOC_GPIO_PIN_COUNT) {
        for (uint8_t i = 0; i < SOC_ADC_PERIPH_NUM; i++) {
            for (uint8_t j = 0; j < SOC_ADC_MAX_CHANNEL_NUM; j++) {
                if (adc_channel_io_map[i][j] == pin) {
                    return channel;
                }
                channel++;
            }
        }
    }
    return -1;
}

int8_t analogChannelToDigitalPin(uint8_t channel) 
{
    if (channel >= (SOC_ADC_PERIPH_NUM * SOC_ADC_MAX_CHANNEL_NUM)) {
        return -1;
    }
    uint8_t adc_unit = (channel / SOC_ADC_MAX_CHANNEL_NUM);
    uint8_t adc_chan = (channel % SOC_ADC_MAX_CHANNEL_NUM);
    return adc_channel_io_map[adc_unit][adc_chan];
}
#else
// No Analog channels availible
int8_t analogChannelToDigitalPin(uint8_t channel) 
{
    return -1;
}
#endif

typedef void (*voidFuncPtr)(void);
typedef void (*voidFuncPtrArg)(void*);
typedef struct {
    voidFuncPtr fn;
    void* arg;
    bool functional;
} InterruptHandle_t;
static InterruptHandle_t __pinInterruptHandlers[SOC_GPIO_PIN_COUNT] = {0,};

#include "driver/rtc_io.h"

extern void ARDUINO_ISR_ATTR __pinMode(uint32_t pin, uint8_t mode)
{
#ifdef RGB_BUILTIN
    if (pin == RGB_BUILTIN){
        __pinMode(RGB_BUILTIN-SOC_GPIO_PIN_COUNT, mode);
        return;
    }
#endif

    if (!GPIO_IS_VALID_GPIO(pin)) {
        log_e("Invalid pin selected");
        return;
    }
    
    gpio_hal_context_t gpiohal;
    gpiohal.dev = GPIO_LL_GET_HW(GPIO_PORT_0);

    gpio_config_t conf = {
        .pin_bit_mask = (1ULL<<pin),                 /*!< GPIO pin: set with bit mask, each bit maps to a GPIO */
        .mode = GPIO_MODE_DISABLE,                   /*!< GPIO mode: set input/output mode                     */
        .pull_up_en = GPIO_PULLUP_DISABLE,           /*!< GPIO pull-up                                         */
        .pull_down_en = GPIO_PULLDOWN_DISABLE,       /*!< GPIO pull-down                                       */
        .intr_type = gpiohal.dev->pin[pin].int_type  /*!< GPIO interrupt type - previously set                 */
    };
    if (mode < 0x20) {//io
        conf.mode = mode & (INPUT | OUTPUT);
        if (mode & OPEN_DRAIN) {
            conf.mode |= GPIO_MODE_DEF_OD;
        }
        if (mode & PULLUP) {
            conf.pull_up_en = GPIO_PULLUP_ENABLE;
        }
        if (mode & PULLDOWN) {
            conf.pull_down_en = GPIO_PULLDOWN_ENABLE;
        }
    }
    if(gpio_config(&conf) != ESP_OK)
    {
        log_e("GPIO config failed");
        return;
    }
}

extern void ARDUINO_ISR_ATTR __digitalWrite(uint32_t pin, uint8_t val)
{
    #ifdef RGB_BUILTIN
        if(pin == RGB_BUILTIN){
            //use RMT to set all channels on/off
            const uint8_t comm_val = val != 0 ? RGB_BRIGHTNESS : 0;
            neopixelWrite(RGB_BUILTIN, comm_val, comm_val, comm_val);
            return;
        }
    #endif
	gpio_set_level((gpio_num_t)pin, val);
}

extern int ARDUINO_ISR_ATTR __digitalRead(uint32_t pin)
{
	return gpio_get_level((gpio_num_t)pin);
}

static void ARDUINO_ISR_ATTR __onPinInterrupt(void * arg) {
	InterruptHandle_t * isr = (InterruptHandle_t*)arg;
    if(isr->fn) {
        if(isr->arg){
            ((voidFuncPtrArg)isr->fn)(isr->arg);
        } else {
        	isr->fn();
        }
    }
}

extern void cleanupFunctional(void* arg);

extern void __attachInterruptFunctionalArg(uint32_t pin, voidFuncPtrArg userFunc, void * arg, int intr_type, bool functional)
{
    static bool interrupt_initialized = false;

    if(!interrupt_initialized) {
    	esp_err_t err = gpio_install_isr_service((int)ARDUINO_ISR_FLAG);
    	interrupt_initialized = (err == ESP_OK) || (err == ESP_ERR_INVALID_STATE);
    }
    if(!interrupt_initialized) {
    	log_e("GPIO ISR Service Failed To Start");
    	return;
    }

    // if new attach without detach remove old info
    if (__pinInterruptHandlers[pin].functional && __pinInterruptHandlers[pin].arg)
    {
    	cleanupFunctional(__pinInterruptHandlers[pin].arg);
    }
    __pinInterruptHandlers[pin].fn = (voidFuncPtr)userFunc;
    __pinInterruptHandlers[pin].arg = arg;
    __pinInterruptHandlers[pin].functional = functional;

    gpio_set_intr_type((gpio_num_t)pin, (gpio_int_type_t)(intr_type & 0x7));
    if(intr_type & 0x8){
    	gpio_wakeup_enable((gpio_num_t)pin, (gpio_int_type_t)(intr_type & 0x7));
    }
    gpio_isr_handler_add((gpio_num_t)pin, __onPinInterrupt, &__pinInterruptHandlers[pin]);


    //FIX interrupts on peripherals outputs (eg. LEDC,...)
    //Enable input in GPIO register
    gpio_hal_context_t gpiohal;
    gpiohal.dev = GPIO_LL_GET_HW(GPIO_PORT_0);
    gpio_hal_input_enable(&gpiohal, pin);
}

extern void __attachInterruptArg(uint32_t pin, voidFuncPtrArg userFunc, void * arg, int intr_type)
{
	__attachInterruptFunctionalArg(pin, userFunc, arg, intr_type, false);
}

extern void __attachInterrupt(uint32_t pin, voidFuncPtr userFunc, int intr_type) {
    __attachInterruptFunctionalArg(pin, (voidFuncPtrArg)userFunc, NULL, intr_type, false);
}

extern void __detachInterrupt(uint32_t pin)
{
	gpio_isr_handler_remove((gpio_num_t)pin); //remove handle and disable isr for pin
	gpio_wakeup_disable((gpio_num_t)pin);

    if (__pinInterruptHandlers[pin].functional && __pinInterruptHandlers[pin].arg)
    {
    	cleanupFunctional(__pinInterruptHandlers[pin].arg);
    }
    __pinInterruptHandlers[pin].fn = NULL;
    __pinInterruptHandlers[pin].arg = NULL;
    __pinInterruptHandlers[pin].functional = false;

    gpio_set_intr_type((gpio_num_t)pin, GPIO_INTR_DISABLE);
}


extern void attachInterrupt(uint32_t pin, voidFuncPtr handler, int mode) __attribute__ ((weak, alias("__attachInterrupt")));
extern void attachInterruptArg(uint32_t pin, voidFuncPtrArg handler, void * arg, int mode) __attribute__ ((weak, alias("__attachInterruptArg")));
extern void detachInterrupt(uint32_t pin) __attribute__ ((weak, alias("__detachInterrupt")));
