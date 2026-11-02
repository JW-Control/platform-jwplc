// JWPLC_TFT_Mirror.h: copia en RAM de lo que se dibuja en la TFT (uso interno).
//
// La usa JWPLC_WebScreen para mostrar la pantalla del equipo en un navegador.
// No es API del sketch: el sketch solo llama JWPLC_WebScreen.begin().
//
// Cómo funciona: las macros de escritura SPI de TFT_eSPI (TFT_WRITE_BITS,
// tft_Write_16N, DC_C/DC_D) y pushBlock/pushPixels avisan aquí de cada byte
// que va a la TFT. Este módulo decodifica CASET (0x2A), RASET (0x2B) y
// RAMWR (0x2C) igual que el ST7789 y escribe los píxeles en un búfer de
// 8 bits por píxel con paleta (hasta 256 colores RGB565 exactos; 320x170 =
// 54 400 B). Cada fila guarda la versión en la que cambió, para enviar solo
// las filas nuevas. Sin JWPLC_WebScreen no se reserva memoria y el costo es
// una comparación por escritura.
#pragma once

#include <stddef.h>
#include <stdint.h>

extern volatile bool jwplcMirrorOn;
extern volatile uint8_t jwplcMirrorDC; // 0 = comando, 1 = datos

// Ganchos de TFT_eSPI (solo con jwplcMirrorOn).
void jwplcMirrorWrite(uint32_t wireBits, uint8_t bits);
void jwplcMirrorBlock(uint16_t color, uint32_t len);
void jwplcMirrorPixels(const void *data, uint32_t len, bool hostOrder);
void jwplcMirrorGeometry(int32_t colstart, int32_t rowstart, int32_t width, int32_t height);

// Reserva el búfer (idempotente). false si no hay memoria.
bool jwplcMirrorStart();
bool jwplcMirrorActive();

// Lectura para el servidor web.
uint16_t jwplcMirrorWidth();
uint16_t jwplcMirrorHeight();
uint32_t jwplcMirrorSnapshot();              // nueva versión; lo dibujado después queda >= ella
uint32_t jwplcMirrorRowVersion(uint16_t y);
const uint8_t *jwplcMirrorRow(uint16_t y);
uint16_t jwplcMirrorPaletteCount();
uint16_t jwplcMirrorPaletteColor(uint16_t index);
uint32_t jwplcMirrorWrites(); // crece con cada tramo escrito (para esperar a que termine un redibujo)
