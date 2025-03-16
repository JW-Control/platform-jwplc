#ifndef __pins_is_H__
#define __pins_is_H__

#if defined(JWPLC_BASIC_4DI_8DO) || defined(JWPLC_BASIC_4DI_8DO_2MO)
#define PIN_I0_0 (0x2207)
#define PIN_I0_1 (0x2206)
#define PIN_I0_2 (0x2205)
#define PIN_I0_3 (0x2204)

static const uint32_t I0_0 = PIN_I0_0;
static const uint32_t I0_1 = PIN_I0_1;
static const uint32_t I0_2 = PIN_I0_2;
static const uint32_t I0_3 = PIN_I0_3;

#define PIN_Q0_0 (0x2208)
#define PIN_Q0_1 (0x2209)
#define PIN_Q0_2 (0x220a)
#define PIN_Q0_3 (0x220b)
#define PIN_Q0_4 (0x220c)
#define PIN_Q0_5 (0x220d)
#define PIN_Q0_6 (0x220e)
#define PIN_Q0_7 (0x220f)

static const uint32_t Q0_0 = PIN_Q0_0;
static const uint32_t Q0_1 = PIN_Q0_1;
static const uint32_t Q0_2 = PIN_Q0_2;
static const uint32_t Q0_3 = PIN_Q0_3;
static const uint32_t Q0_4 = PIN_Q0_4;
static const uint32_t Q0_5 = PIN_Q0_5;
static const uint32_t Q0_6 = PIN_Q0_6;
static const uint32_t Q0_7 = PIN_Q0_7;
#endif

#if defined(JWPLC_BASIC_4DI_8DO_2MO)
#define PIN_Q0_8 (14)
#define PIN_Q0_9 (12)

static const uint32_t Q0_8 = PIN_Q0_8;
static const uint32_t Q0_9 = PIN_Q0_9;
#endif

static const uint32_t I_VP = 36;
static const uint32_t I_VN = 39;
static const uint32_t EN_IO = 27;

// Arrays defined in esp-plc-peripherals.c

#define HAVE_TCA6424A

#endif // __pins_is_H__
