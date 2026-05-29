#ifndef __pins_is_H__
#define __pins_is_H__
/**
 * Este archivo define los pines de entrada/salida extendidos para JWPLC Basic,
 * aprovechando el TCA6424A para expandir I/O.
 *
 * Para compilar estas definiciones, en boards.txt usamos:
 *    jwplcbasic.build.defines=-DJWPLC_BASIC
 */

#if defined(JWPLC_BASIC)

// Definición de 6 entradas digitales (DI):
// Se asume que siguen la secuencia 0x2207..0x2202
// Ajusta estos valores si tu hardware difiere.
#define PIN_I0_0 (0x2207)
#define PIN_I0_1 (0x2206)
#define PIN_I0_2 (0x2205)
#define PIN_I0_3 (0x2204)
#define PIN_I0_4 (0x2203)
#define PIN_I0_5 (0x2202)

static const uint32_t I0_0 = PIN_I0_0;
static const uint32_t I0_1 = PIN_I0_1;
static const uint32_t I0_2 = PIN_I0_2;
static const uint32_t I0_3 = PIN_I0_3;
static const uint32_t I0_4 = PIN_I0_4;
static const uint32_t I0_5 = PIN_I0_5;

// Definición de 8 salidas digitales (DO):
// Se asume 0x2208..0x220f en el TCA6424A
#define PIN_Q0_0 (0x2208)
#define PIN_Q0_1 (0x2209)
#define PIN_Q0_2 (0x220a)
#define PIN_Q0_3 (0x220b)
#define PIN_Q0_4 (0x220c)
#define PIN_Q0_5 (0x220d)
#define PIN_Q0_6 (0x220e)
#define PIN_Q0_7 (0x220f)

static const uint32_t Q0_0 = PIN_Q0_0;  //RELE Q0.0
static const uint32_t Q0_1 = PIN_Q0_1;  //RELE Q0.1
static const uint32_t Q0_2 = PIN_Q0_2;  //RELE Q0.2
static const uint32_t Q0_3 = PIN_Q0_3;  //RELE Q0.3
static const uint32_t Q0_4 = PIN_Q0_4;  //RELE Q0.4
static const uint32_t Q0_5 = PIN_Q0_5;  //RELE Q0.5
static const uint32_t Q0_6 = PIN_Q0_6;  //RELE Q0.6
static const uint32_t Q0_7 = PIN_Q0_7;  //RELE Q0.7

// Definición de 2 salidas MO (por hardware real se conectan a GPIO14 y GPIO12):
#define PIN_Q0_8 (14)
#define PIN_Q0_9 (12)

static const uint32_t Q0_8 = PIN_Q0_8;  //TRANSISTOR Q0.8
static const uint32_t Q0_9 = PIN_Q0_9;  //TRANSISTOR Q0.9

// Activamos la definición para indicar que tenemos TCA6424A
#define HAVE_TCA6424A

#endif // JWPLC_BASIC

// Pines analógicos o referencias internas (por ejemplo, para medición interna)
static const uint32_t I_VP = 36; // Canal ADC VP
static const uint32_t I_VN = 39; // Canal ADC VN

// Pin para habilitar el IO expander (si aplica)
static const uint32_t EN_IO = 27;

#endif // __pins_is_H__