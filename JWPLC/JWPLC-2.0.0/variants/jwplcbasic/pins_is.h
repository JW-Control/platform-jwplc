#ifndef JWPLC_BASIC_PINS_IS_H
#define JWPLC_BASIC_PINS_IS_H
/**
 * Este archivo define los pines de entrada/salida extendidos para JWPLC Basic,
 * aprovechando el TCA6424A para expandir I/O.
 *
 * Para compilar estas definiciones, en boards.txt usamos:
 *    jwplcbasic.build.defines=-DJWPLC_BASIC
 */

#if defined(JWPLC_BASIC)

// Entradas digitales expandidas (TCA6424A)
static const uint16_t I0_0 = 0x2207;
static const uint16_t I0_1 = 0x2206;
static const uint16_t I0_2 = 0x2205;
static const uint16_t I0_3 = 0x2204;
static const uint16_t I0_4 = 0x2203;
static const uint16_t I0_5 = 0x2202;
static const uint16_t I0_6 = 0x2201;
static const uint16_t I0_7 = 0x2200;

static const uint8_t I0_COUNT = 8;
static const uint16_t I0_X[8] = {
    I0_0, I0_1, I0_2, I0_3,
    I0_4, I0_5, I0_6, I0_7
};

// Salidas digitales tipo relé (TCA6424A)
static const uint16_t Q0_0 = 0x2208;
static const uint16_t Q0_1 = 0x2209;
static const uint16_t Q0_2 = 0x220A;
static const uint16_t Q0_3 = 0x220B;
static const uint16_t Q0_4 = 0x220C;
static const uint16_t Q0_5 = 0x220D;
static const uint16_t Q0_6 = 0x220E;
static const uint16_t Q0_7 = 0x220F;

static const uint8_t Q0_COUNT = 8;
static const uint16_t Q0_X[8] = {
    Q0_0, Q0_1, Q0_2, Q0_3,
    Q0_4, Q0_5, Q0_6, Q0_7
};

// ADC / referencias internas
static const uint16_t I_VP = 36;
static const uint16_t I_VN = 39;

// Habilitación del expansor
static const uint16_t EN_IO = 27;

#endif // JWPLC_BASIC

#endif // JWPLC_BASIC_PINS_IS_H