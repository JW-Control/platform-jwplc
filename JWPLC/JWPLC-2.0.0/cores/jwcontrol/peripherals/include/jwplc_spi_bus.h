#ifndef JWCONTROL_JWPLC_SPI_BUS_H
#define JWCONTROL_JWPLC_SPI_BUS_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// =====================================================
// Pines SPI compartidos JWPLC Basic
// =====================================================
#define JWPLC_SPI_MOSI 23
#define JWPLC_SPI_MISO 19
#define JWPLC_SPI_SCK  18

#define JWPLC_TFT_CS   33
#define JWPLC_TFT_DC   25
#define JWPLC_TFT_RST  14

#define JWPLC_SD_CS    32
#define JWPLC_FRAM_CS  13
#define JWPLC_ETH_CS   5

// =====================================================
// Frecuencias SPI sugeridas
// =====================================================
// Estas constantes quedan disponibles para las librerías que sí usen SPI.h.
#define JWPLC_SPI_TFT_HZ   80000000UL
#define JWPLC_SPI_ETH_HZ   20000000UL
#define JWPLC_SPI_SD_HZ    20000000UL
#define JWPLC_SPI_FRAM_HZ  10000000UL

// Inicialización base de pines CS y mutex.
// No llama a SPI.begin() porque este archivo pertenece al core.
bool jwplcSPI_begin(void);
bool jwplcSPI_isReady(void);

// Mutex general del bus SPI.
// timeoutMs = 0 no espera.
// timeoutMs = 0xFFFFFFFF espera indefinidamente.
bool jwplcSPI_acquire(uint32_t timeoutMs);
void jwplcSPI_release(void);

// CS seguros
void jwplcSPI_deselectAll(void);
void jwplcSPI_prepareForTFT(void);

#ifdef __cplusplus
}
#endif

#endif // JWCONTROL_JWPLC_SPI_BUS_H