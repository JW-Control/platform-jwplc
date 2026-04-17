#ifndef JWCONTROL_JWPLC_PERIPHERALS_H
#define JWCONTROL_JWPLC_PERIPHERALS_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
    volatile uint8_t di_raw_bank0;      // Lectura cruda P00..P07
    volatile uint8_t di_logical_bank0;  // Reordenada I0_0..I0_7
    volatile uint8_t do_bank1;          // Estado sombra Q0_0..Q0_7
    volatile uint8_t do_bank2;          // Reservado para futuras expansiones
    volatile bool initialized;
    volatile uint32_t last_scan_ms;
} JWPLC_IOState;

typedef struct
{
    volatile bool valid;
    volatile uint8_t second;
    volatile uint8_t minute;
    volatile uint8_t hour;
    volatile uint8_t day;
    volatile uint8_t month;
    volatile uint16_t year;
    volatile uint32_t last_update_ms;
} JWPLC_RTCState;

// Wrappers Arduino redirigidos
void jwplc_pinMode(uint16_t pin, uint8_t mode);
void jwplc_digitalWrite(uint16_t pin, uint8_t val);
int  jwplc_digitalRead(uint16_t pin);

// Estado/cache del sistema
const JWPLC_IOState* jwplcGetIOState(void);
const JWPLC_RTCState* jwplcGetRTCState(void);

// Runtime automático JWPLC
void jwplcSystemInitState(void);
void jwplcSystemScanIO(void);
void jwplcSystemTickRTC(void);      // Stub por ahora
void jwplcSystemDisplayHook(void);  // Stub por ahora

// Shadow de salidas
void jwplcSystemSetOutputShadow(uint8_t bank1, uint8_t bank2);
void jwplcSystemClearOutputShadow(void);

#ifdef __cplusplus
}
#endif

#endif