#ifndef JWCONTROL_JWPLC_PERIPHERALS_H
#define JWCONTROL_JWPLC_PERIPHERALS_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
    volatile uint8_t di_raw_bank0;          // Lectura cruda P00..P07
    volatile uint8_t di_logical_bank0;      // Reordenada I0_0..I0_7
    volatile uint8_t do_bank1;              // Shadow Q0_0..Q0_7
    volatile uint8_t do_bank2;              // Reservado
    volatile bool initialized;
    volatile bool display_dirty;            // Hay cambios que la TFT debe reflejar
    volatile uint32_t last_scan_ms;
    volatile uint32_t last_display_refresh_ms;
} JWPLC_IOState;

typedef struct
{
    volatile bool present;
    volatile bool valid;
    volatile bool lost_power;
    volatile uint8_t status;
    volatile uint8_t day_of_week;
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
void jwplcSystemTickRTC(void);
void jwplcSystemDisplayHook(void);

// Shadow de salidas
void jwplcSystemSetOutputShadow(uint8_t bank1, uint8_t bank2);
void jwplcSystemClearOutputShadow(void);

// Control de refresco de display
void jwplcSystemMarkDisplayDirty(void);
void jwplcSystemForceDisplayRefresh(void);
bool jwplcSystemConsumeDisplayDirty(void);

// Hooks de display para implementar desde el sketch si se desea
bool jwplcDisplayBeginCallback(void);
void jwplcDisplayRefreshCallback(const JWPLC_IOState* io, const JWPLC_RTCState* rtc);

// Hooks de RTC para implementar desde el sketch si se desea
bool jwplcRTCBeginCallback(void);
bool jwplcRTCReadCallback(JWPLC_RTCState* rtc);

// Hooks de FRAM para proveedor interno del ecosistema
bool jwplcFRAMBeginCallback(void);

// Hooks de microSD para proveedor interno del ecosistema
bool jwplcSDBeginCallback(void);

// Hooks de botonera para proveedor interno del ecosistema
bool jwplcButtonsBeginCallback(void);

// Hook de Ethernet para proveedor interno del ecosistema
void jwplcEthernetTickCallback(void);

#ifdef __cplusplus
}
#endif

#endif
