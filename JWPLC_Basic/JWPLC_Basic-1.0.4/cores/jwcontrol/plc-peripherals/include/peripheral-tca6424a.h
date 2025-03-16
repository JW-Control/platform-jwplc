// I2Cdev library collection - TCA6424A I2C device class header file
// Based on Texas Instruments TCA6424A datasheet, 9/2010 (document SCPS193B)
// 7/31/2011 by Jeff Rowberg <jeff@rowberg.net>
// Updates should (hopefully) always be available at https://github.com/jrowberg/i2cdevlib
//
// Changelog:
//     2011-07-31 - initial release

/* ============================================
I2Cdev device library code is placed under the MIT license
Copyright (c) 2011 Jeff Rowberg

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
===============================================
*/

#ifndef _PERIPHERAL_TCA6424A_H_
#define _PERIPHERAL_TCA6424A_H_

#include <stdint.h>
#include <stdbool.h>

#define TCA6424A_NUM_IO 24

#define TCA6424A_ADDRESS_ADDR_LOW   0x22 // address pin low (GND)
#define TCA6424A_ADDRESS_ADDR_HIGH  0x23 // address pin high (VCC)
#define TCA6424A_DEFAULT_ADDRESS    TCA6424A_ADDRESS_ADDR_LOW

#define TCA6424A_RA_INPUT0          0x00
#define TCA6424A_RA_INPUT1          0x01
#define TCA6424A_RA_INPUT2          0x02
#define TCA6424A_RA_OUTPUT0         0x04
#define TCA6424A_RA_OUTPUT1         0x05
#define TCA6424A_RA_OUTPUT2         0x06
#define TCA6424A_RA_POLARITY0       0x08
#define TCA6424A_RA_POLARITY1       0x09
#define TCA6424A_RA_POLARITY2       0x0A
#define TCA6424A_RA_CONFIG0         0x0C
#define TCA6424A_RA_CONFIG1         0x0D
#define TCA6424A_RA_CONFIG2         0x0E

#define TCA6424A_AUTO_INCREMENT     0x80

#define TCA6424A_LOW                0
#define TCA6424A_HIGH               1

#define TCA6424A_POLARITY_NORMAL    0
#define TCA6424A_POLARITY_INVERTED  1

#define TCA6424A_OUTPUT             0
#define TCA6424A_INPUT              1

#define TCA6424A_P00                0
#define TCA6424A_P01                1
#define TCA6424A_P02                2
#define TCA6424A_P03                3
#define TCA6424A_P04                4
#define TCA6424A_P05                5
#define TCA6424A_P06                6
#define TCA6424A_P07                7
#define TCA6424A_P10                8
#define TCA6424A_P11                9
#define TCA6424A_P12                10
#define TCA6424A_P13                11
#define TCA6424A_P14                12
#define TCA6424A_P15                13
#define TCA6424A_P16                14
#define TCA6424A_P17                15
#define TCA6424A_P20                16
#define TCA6424A_P21                17
#define TCA6424A_P22                18
#define TCA6424A_P23                19
#define TCA6424A_P24                20
#define TCA6424A_P25                21
#define TCA6424A_P26                22
#define TCA6424A_P27                23

// Prototipos de funciones
bool TCA6424A_init(uint8_t address);
bool TCA6424A_testConnection(uint8_t address);

bool TCA6424A_readPin(uint8_t address, uint16_t pin, uint8_t *state);
bool TCA6424A_writePin(uint8_t address, uint16_t pin, bool state);

uint8_t TCA6424A_readBank(uint8_t address, uint8_t bank, uint8_t *state);
bool TCA6424A_writeBank(uint8_t address, uint8_t bank, uint8_t state);

bool TCA6424A_setPinDirection(uint8_t address, uint16_t pin, uint8_t direction);
bool TCA6424A_setBankDirection(uint8_t address, uint8_t bank, uint8_t direction);

#endif /* _PERIPHERAL_TCA6424A_H_ */