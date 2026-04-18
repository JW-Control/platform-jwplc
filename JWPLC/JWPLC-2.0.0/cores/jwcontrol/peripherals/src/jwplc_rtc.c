#include "jwplc_rtc.h"
#include "jwplc_i2c_bridge.h"

static uint8_t bcdToBin(uint8_t val)
{
    return (uint8_t)((val & 0x0F) + ((val >> 4) * 10));
}

static uint8_t binToBcd(uint8_t val)
{
    return (uint8_t)(((val / 10) << 4) | (val % 10));
}

bool jwplcRTC_begin(void)
{
    uint8_t status = 0;
    return jwplcRTC_readStatus(&status);
}

bool jwplcRTC_readStatus(uint8_t *status)
{
    if (status == 0)
    {
        return false;
    }

    return (jwplcI2C_readReg8(JWPLC_RTC_DEFAULT_ADDRESS, 0x0F, status) == 0);
}

bool jwplcRTC_readRaw(uint8_t startReg, uint8_t length, uint8_t *data)
{
    if (data == 0 || length == 0)
    {
        return false;
    }

    return (jwplcI2C_readRegs(JWPLC_RTC_DEFAULT_ADDRESS, startReg, length, data) == 0);
}

bool jwplcRTC_clearOSF(void)
{
    uint8_t status = 0;

    if (!jwplcRTC_readStatus(&status))
    {
        return false;
    }

    status &= (uint8_t)~0x80;   // limpiar OSF (bit 7)

    return (jwplcI2C_writeReg8(JWPLC_RTC_DEFAULT_ADDRESS, 0x0F, status) == 0);
}

bool jwplcRTC_setDateTime(const JWPLC_RTCDateTime *dt)
{
    if (dt == 0)
    {
        return false;
    }

    if (dt->second > 59 || dt->minute > 59 || dt->hour > 23 ||
        dt->day < 1 || dt->day > 31 || dt->month < 1 || dt->month > 12 ||
        dt->year < 2000 || dt->year > 2099)
    {
        return false;
    }

    uint8_t raw[7];

    raw[0] = binToBcd(dt->second);
    raw[1] = binToBcd(dt->minute);
    raw[2] = binToBcd(dt->hour);      // modo 24h
    raw[3] = 0x01;                    // día de semana placeholder
    raw[4] = binToBcd(dt->day);
    raw[5] = binToBcd(dt->month);
    raw[6] = binToBcd((uint8_t)(dt->year - 2000));

    if (jwplcI2C_writeRegs(JWPLC_RTC_DEFAULT_ADDRESS, 0x00, 7, raw) != 0)
    {
        return false;
    }

    return jwplcRTC_clearOSF();
}

bool jwplcRTC_readDateTime(JWPLC_RTCDateTime *dt)
{
    if (dt == 0)
    {
        return false;
    }

    dt->present = false;
    dt->valid = false;
    dt->status = 0xFF;
    dt->second = 0;
    dt->minute = 0;
    dt->hour = 0;
    dt->day = 0;
    dt->month = 0;
    dt->year = 0;

    uint8_t raw[7] = {0};
    if (!jwplcRTC_readRaw(0x00, 7, raw))
    {
        return false;
    }

    dt->present = true;

    uint8_t status = 0x00;
    if (jwplcRTC_readStatus(&status))
    {
        dt->status = status;
    }

    uint8_t sec = bcdToBin(raw[0] & 0x7F);
    uint8_t min = bcdToBin(raw[1] & 0x7F);

    uint8_t hourRaw = raw[2];
    uint8_t hour = 0;

    if (hourRaw & 0x40)
    {
        hour = bcdToBin(hourRaw & 0x1F);
        if (hourRaw & 0x20)
        {
            if (hour < 12) hour = (uint8_t)(hour + 12);
        }
        else
        {
            if (hour == 12) hour = 0;
        }
    }
    else
    {
        hour = bcdToBin(hourRaw & 0x3F);
    }

    uint8_t day = bcdToBin(raw[4] & 0x3F);
    uint8_t month = bcdToBin(raw[5] & 0x1F);
    uint16_t year = (uint16_t)(2000 + bcdToBin(raw[6]));

    dt->second = sec;
    dt->minute = min;
    dt->hour = hour;
    dt->day = day;
    dt->month = month;
    dt->year = year;

    bool plausible =
        (sec <= 59) &&
        (min <= 59) &&
        (hour <= 23) &&
        (day >= 1 && day <= 31) &&
        (month >= 1 && month <= 12);

    bool osf = ((dt->status & 0x80) != 0);

    dt->valid = plausible && !osf;

    return true;
}