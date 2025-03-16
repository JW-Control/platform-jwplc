#ifndef Pins_Arduino_h
#define Pins_Arduino_h

#include <stdint.h>

#include "pins_is.h"

// JW Control by JW Control S.A.C.
// Define pinout of JWPLC v1.4

#define NUM_DIGITAL_PINS        40 // REVISADO
#define NUM_ANALOG_INPUTS       16 // REVISADO

static const uint8_t TX = 1; // REVISADO
static const uint8_t RX = 3; // REVISADO

static const uint8_t SDA = 21; // REVISADO
static const uint8_t SCL = 22; // REVISADO

static const uint8_t SS = 5;    // REVISADO
static const uint8_t MOSI = 23; // REVISADO
static const uint8_t MISO = 19; // REVISADO
static const uint8_t SCK = 18;  // REVISADO

static const uint8_t A0 = 36;
static const uint8_t A3 = 39;
static const uint8_t A4 = 32;
static const uint8_t A5 = 33;
static const uint8_t A6 = 34;
static const uint8_t A7 = 35;
static const uint8_t A10 = 4;
static const uint8_t A11 = 0;
static const uint8_t A12 = 2;
static const uint8_t A13 = 15;
static const uint8_t A14 = 13;
static const uint8_t A15 = 12;
static const uint8_t A16 = 14;
static const uint8_t A17 = 27;
static const uint8_t A18 = 25;
static const uint8_t A19 = 26;

static const uint8_t T0 = 4;
static const uint8_t T1 = 0;
static const uint8_t T2 = 2;
static const uint8_t T3 = 15;
static const uint8_t T4 = 13;
static const uint8_t T5 = 12;
static const uint8_t T6 = 14;
static const uint8_t T7 = 27;
static const uint8_t T8 = 33;
static const uint8_t T9 = 32;

static const uint8_t DAC1 = 25;
static const uint8_t DAC2 = 26;

#endif /* Pins_Arduino_h */
