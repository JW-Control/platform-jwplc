#ifndef JWCONTROL_PERIPHERALS_INIT_H
#define JWCONTROL_PERIPHERALS_INIT_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

// Falla de arranque de las E/S industriales (I0_x / Q0_x). La lee JWPLC_IO.fault().
#define JWPLC_IO_FAULT_NONE       0u
#define JWPLC_IO_FAULT_I2C        1u  // jwplcI2C_begin() fallo: no arranco nada del bus
#define JWPLC_IO_FAULT_TCA_INIT   2u  // el TCA6424A no responde
#define JWPLC_IO_FAULT_TCA_CONFIG 3u  // responde pero no acepto la configuracion

// Periodo del reintento automatico del expansor desde jwplcSystemTask.
#ifndef JWPLC_IO_RETRY_PERIOD_MS
#define JWPLC_IO_RETRY_PERIOD_MS 1000u
#endif

void initPeripherals(void);

// Devuelve JWPLC_IO_FAULT_*.
uint8_t jwplcSystemIOFault(void);

// Reintenta configurar el TCA6424A y habilitar EN_IO si fallo al arrancar.
// No hace nada si las E/S ya estan listas o si la falla fue del bus I2C.
void jwplcPeripheralsRetryIO(void);

#ifdef __cplusplus
}
#endif

#endif