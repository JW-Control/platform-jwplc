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

static bool isLeapYear(uint16_t year)
{
    return ((year % 4u) == 0u && (year % 100u) != 0u) || ((year % 400u) == 0u);
}

static uint8_t daysInMonth(uint16_t year, uint8_t month)
{
    static const uint8_t table[12] = {31,28,31,30,31,30,31,31,30,31,30,31};

    if (month < 1 || month > 12)
    {
        return 0;
    }

    if (month == 2 && isLeapYear(year))
    {
        return 29;
    }

    return table[month - 1];
}

static uint8_t computeDayOfWeek(uint16_t year, uint8_t month, uint8_t day)
{
    int32_t y = year;
    uint32_t m = month;
    uint32_t d = day;

    y -= (m <= 2);
    const int32_t era = (y >= 0 ? y : y - 399) / 400;
    const uint32_t yoe = (uint32_t)(y - era * 400);
    const uint32_t doy = (153u * (m + (m > 2 ? (uint32_t)-3 : 9u)) + 2u) / 5u + d - 1u;
    const uint32_t doe = yoe * 365u + yoe / 4u - yoe / 100u + doy;
    const int32_t days = era * 146097 + (int32_t)doe - 719468;

    return (uint8_t)(((uint32_t)(days + 4) % 7u) + 1u);
}

bool jwplcRTC_begin(void)
{
    uint8_t status = 0;
    return jwplcRTC_readStatus(&status);
}

bool jwplcRTC_isPresent(void)
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

bool jwplcRTC_lostPower(bool *lostPower)
{
    uint8_t status = 0;

    if (lostPower == 0)
    {
        return false;
    }

    if (!jwplcRTC_readStatus(&status))
    {
        return false;
    }

    *lostPower = ((status & 0x80u) != 0u);
    return true;
}

bool jwplcRTC_readRaw(uint8_t startReg, uint8_t length, uint8_t *data)
{
    if (data == 0 || length == 0)
    {
        return false;
    }

    return (jwplcI2C_readRegs(JWPLC_RTC_DEFAULT_ADDRESS, startReg, length, data) == 0);
}

bool jwplcRTC_readTemperatureCentiC(int16_t *tempCentiC)
{
    uint8_t raw[2] = {0};

    if (tempCentiC == 0)
    {
        return false;
    }

    if (!jwplcRTC_readRaw(0x11, 2, raw))
    {
        return false;
    }

    int16_t quarterC = (int16_t)(((int16_t)((int8_t)raw[0]) << 2) | (raw[1] >> 6));
    *tempCentiC = (int16_t)(quarterC * 25);
    return true;
}

bool jwplcRTC_readTemperatureC(float *tempC)
{
    int16_t tempCentiC = 0;

    if (tempC == 0)
    {
        return false;
    }

    if (!jwplcRTC_readTemperatureCentiC(&tempCentiC))
    {
        return false;
    }

    *tempC = (float)tempCentiC / 100.0f;
    return true;
}

bool jwplcRTC_clearOSF(void)
{
    uint8_t status = 0;

    if (!jwplcRTC_readStatus(&status))
    {
        return false;
    }

    status &= (uint8_t)~0x80u;
    return (jwplcI2C_writeReg8(JWPLC_RTC_DEFAULT_ADDRESS, 0x0F, status) == 0);
}

bool jwplcRTC_setDateTime(const JWPLC_RTCDateTime *dt)
{
    if (dt == 0)
    {
        return false;
    }

    if (dt->second > 59 || dt->minute > 59 || dt->hour > 23 ||
        dt->month < 1 || dt->month > 12 ||
        dt->year < 2000 || dt->year > 2099)
    {
        return false;
    }

    if (dt->day < 1 || dt->day > daysInMonth(dt->year, dt->month))
    {
        return false;
    }

    uint8_t dow = dt->dayOfWeek;
    if (dow == 0)
    {
        dow = computeDayOfWeek(dt->year, dt->month, dt->day);
    }

    if (dow < 1 || dow > 7)
    {
        return false;
    }

    uint8_t raw[7];
    raw[0] = binToBcd(dt->second);
    raw[1] = binToBcd(dt->minute);
    raw[2] = binToBcd(dt->hour);  // modo 24h
    raw[3] = dow & 0x07u;         // día de semana 1..7
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
    dt->lostPower = false;
    dt->status = 0xFF;
    dt->dayOfWeek = 0;
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

    uint8_t status = 0xFF;
    if (jwplcRTC_readStatus(&status))
    {
        dt->status = status;
        dt->lostPower = ((status & 0x80u) != 0u);
    }
    else
    {
        dt->status = 0xFF;
        dt->lostPower = true;
    }

    uint8_t sec = bcdToBin(raw[0] & 0x7Fu);
    uint8_t min = bcdToBin(raw[1] & 0x7Fu);

    uint8_t hourRaw = raw[2];
    uint8_t hour = 0;

    if (hourRaw & 0x40u)
    {
        hour = bcdToBin(hourRaw & 0x1Fu);
        if (hourRaw & 0x20u)
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
        hour = bcdToBin(hourRaw & 0x3Fu);
    }

    uint8_t dayOfWeek = (uint8_t)(raw[3] & 0x07u);
    uint8_t day = bcdToBin(raw[4] & 0x3Fu);
    uint8_t month = bcdToBin(raw[5] & 0x1Fu);
    uint16_t year = (uint16_t)(2000u + bcdToBin(raw[6]));

    dt->dayOfWeek = dayOfWeek;
    dt->second = sec;
    dt->minute = min;
    dt->hour = hour;
    dt->day = day;
    dt->month = month;
    dt->year = year;

    bool plausible =
        (sec <= 59u) &&
        (min <= 59u) &&
        (hour <= 23u) &&
        (month >= 1u && month <= 12u) &&
        (day >= 1u && day <= daysInMonth(year, month));

    dt->valid = plausible && !dt->lostPower;

    return true;
}
