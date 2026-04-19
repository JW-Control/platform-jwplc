#ifndef JWPLC_RTC_H
#define JWPLC_RTC_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define JWPLC_RTC_DEFAULT_ADDRESS 0x68

typedef struct
{
    bool present;
    bool valid;
    bool lostPower;
    uint8_t status;
    uint8_t dayOfWeek;   // 1=Sunday ... 7=Saturday
    uint8_t second;
    uint8_t minute;
    uint8_t hour;
    uint8_t day;
    uint8_t month;
    uint16_t year;
} JWPLC_RTCDateTime;

bool jwplcRTC_begin(void);
bool jwplcRTC_isPresent(void);

bool jwplcRTC_readStatus(uint8_t *status);
bool jwplcRTC_lostPower(bool *lostPower);

bool jwplcRTC_readDateTime(JWPLC_RTCDateTime *dt);
bool jwplcRTC_readRaw(uint8_t startReg, uint8_t length, uint8_t *data);

bool jwplcRTC_readTemperatureCentiC(int16_t *tempCentiC);
bool jwplcRTC_readTemperatureC(float *tempC);

bool jwplcRTC_clearOSF(void);
bool jwplcRTC_setDateTime(const JWPLC_RTCDateTime *dt);

#ifdef __cplusplus
}
#endif

#endif
