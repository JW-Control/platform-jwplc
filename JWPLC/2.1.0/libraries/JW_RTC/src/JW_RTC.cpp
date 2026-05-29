#include "JW_RTC.h"

#include <stdlib.h>
#include <string.h>

JW_RTC::JW_RTC(uint8_t address)
    : _address(address), _ready(false), _lastError(Error::NotReady)
{
}

void JW_RTC::setError(Error error)
{
    _lastError = error;
}

void JW_RTC::clearError()
{
    _lastError = Error::Ok;
}

JW_RTC::Error JW_RTC::lastError() const
{
    return _lastError;
}

bool JW_RTC::begin()
{
    int ret = jwplcI2C_begin();
    if (ret != 0)
    {
        _ready = false;
        setError(Error::BusInitFailed);
        return false;
    }

    _ready = true;
    return isPresent();
}

bool JW_RTC::beginWithPins(uint8_t sdaPin, uint8_t sclPin, uint32_t frequencyHz)
{
    int ret = jwplcI2C_beginWithPins(sdaPin, sclPin, frequencyHz);
    if (ret != 0)
    {
        _ready = false;
        setError(Error::BusInitFailed);
        return false;
    }

    _ready = true;
    return isPresent();
}

bool JW_RTC::setClock(uint32_t frequencyHz)
{
    if (!_ready)
    {
        setError(Error::NotReady);
        return false;
    }

    int ret = jwplcI2C_setClock(frequencyHz);
    if (ret != 0)
    {
        setError(Error::BusInitFailed);
        return false;
    }

    setError(Error::Ok);
    return true;
}

// =====================================================
// Low-level bridge
// =====================================================
bool JW_RTC::readRegister(uint8_t reg, uint8_t& value)
{
    if (!_ready)
    {
        setError(Error::NotReady);
        return false;
    }

    int ret = jwplcI2C_readReg8(_address, reg, &value);
    if (ret != 0)
    {
        setError(Error::ReadFailed);
        return false;
    }

    setError(Error::Ok);
    return true;
}

bool JW_RTC::writeRegister(uint8_t reg, uint8_t value)
{
    if (!_ready)
    {
        setError(Error::NotReady);
        return false;
    }

    int ret = jwplcI2C_writeReg8(_address, reg, value);
    if (ret != 0)
    {
        setError(Error::WriteFailed);
        return false;
    }

    setError(Error::Ok);
    return true;
}

bool JW_RTC::readBytes(uint8_t reg, uint8_t* data, size_t len)
{
    if (!_ready)
    {
        setError(Error::NotReady);
        return false;
    }

    if ((data == nullptr && len > 0) || len > 255)
    {
        setError(Error::InvalidArgument);
        return false;
    }

    int ret = jwplcI2C_readRegs(_address, reg, static_cast<uint8_t>(len), data);
    if (ret != 0)
    {
        setError(Error::ReadFailed);
        return false;
    }

    setError(Error::Ok);
    return true;
}

bool JW_RTC::writeBytes(uint8_t reg, const uint8_t* data, size_t len)
{
    if (!_ready)
    {
        setError(Error::NotReady);
        return false;
    }

    if ((data == nullptr && len > 0) || len > 255)
    {
        setError(Error::InvalidArgument);
        return false;
    }

    int ret = jwplcI2C_writeRegs(_address, reg, static_cast<uint8_t>(len), data);
    if (ret != 0)
    {
        setError(Error::WriteFailed);
        return false;
    }

    setError(Error::Ok);
    return true;
}

// =====================================================
// Helpers
// =====================================================
uint8_t JW_RTC::bcdEncode(uint8_t value)
{
    return static_cast<uint8_t>(((value / 10U) << 4U) | (value % 10U));
}

uint8_t JW_RTC::bcdDecode(uint8_t value)
{
    return static_cast<uint8_t>(((value >> 4U) * 10U) + (value & 0x0FU));
}

bool JW_RTC::isLeapYear(uint16_t year)
{
    return ((year % 4U) == 0U && (year % 100U) != 0U) || ((year % 400U) == 0U);
}

uint8_t JW_RTC::daysInMonth(uint16_t year, uint8_t month)
{
    static const uint8_t table[12] = {31,28,31,30,31,30,31,31,30,31,30,31};
    if (month < 1 || month > 12) return 0;
    if (month == 2 && isLeapYear(year)) return 29;
    return table[month - 1];
}

bool JW_RTC::isValidDateTime(const DateTime& dt)
{
    if (dt.year < 2000 || dt.year > 2099) return false;
    if (dt.month < 1 || dt.month > 12) return false;
    if (dt.day < 1 || dt.day > daysInMonth(dt.year, dt.month)) return false;
    if (dt.hour > 23) return false;
    if (dt.minute > 59) return false;
    if (dt.second > 59) return false;
    if (dt.dayOfWeek != 0 && (dt.dayOfWeek < 1 || dt.dayOfWeek > 7)) return false;
    return true;
}

uint8_t JW_RTC::computeDayOfWeek(uint16_t year, uint8_t month, uint8_t day)
{
    DateTime dt;
    dt.year = year;
    dt.month = month;
    dt.day = day;
    dt.hour = 0;
    dt.minute = 0;
    dt.second = 0;

    uint32_t unixTime = toUnix(dt);
    uint32_t days = unixTime / 86400UL;

    return static_cast<uint8_t>(((days + 4UL) % 7UL) + 1UL);
}

// =====================================================
// Presence / validity
// =====================================================
bool JW_RTC::isPresent()
{
    uint8_t v = 0;
    bool ok = readRegister(REG_SECONDS, v);
    if (!ok)
    {
        setError(Error::DeviceNotFound);
        return false;
    }

    setError(Error::Ok);
    return true;
}

bool JW_RTC::isTimeValid()
{
    uint8_t status = 0;
    if (!readStatus(status)) return false;
    return (status & STAT_OSF) == 0;
}

bool JW_RTC::lostPower()
{
    return !isTimeValid();
}

bool JW_RTC::clearOscillatorStopFlag()
{
    return updateStatus(STAT_OSF, 0x00);
}

// =====================================================
// DateTime read/write
// =====================================================
bool JW_RTC::read(DateTime& dt)
{
    uint8_t raw[7];
    if (!readBytes(REG_SECONDS, raw, sizeof(raw)))
    {
        dt.valid = false;
        return false;
    }

    uint8_t sec = raw[0] & 0x7F;
    uint8_t min = raw[1] & 0x7F;
    uint8_t hourReg = raw[2];
    uint8_t dow = raw[3] & 0x07;
    uint8_t date = raw[4] & 0x3F;
    uint8_t monthReg = raw[5] & 0x1F;
    uint8_t yearReg = raw[6];

    dt.second = bcdDecode(sec);
    dt.minute = bcdDecode(min);

    if (hourReg & 0x40)
    {
        uint8_t hour12 = bcdDecode(hourReg & 0x1F);
        bool pm = (hourReg & 0x20) != 0;

        if (hour12 == 12)
            dt.hour = pm ? 12 : 0;
        else
            dt.hour = pm ? static_cast<uint8_t>(hour12 + 12) : hour12;
    }
    else
    {
        dt.hour = bcdDecode(hourReg & 0x3F);
    }

    dt.dayOfWeek = dow;
    dt.day = bcdDecode(date);
    dt.month = bcdDecode(monthReg);
    dt.year = static_cast<uint16_t>(2000 + bcdDecode(yearReg));
    dt.valid = isValidDateTime(dt) && isTimeValid();

    return dt.valid || lastError() == Error::Ok;
}

JW_RTC::DateTime JW_RTC::now()
{
    DateTime dt;
    read(dt);
    return dt;
}

bool JW_RTC::write(const DateTime& input)
{
    DateTime dt = input;

    if (!isValidDateTime(dt))
    {
        setError(Error::InvalidArgument);
        return false;
    }

    if (dt.dayOfWeek == 0)
    {
        dt.dayOfWeek = computeDayOfWeek(dt.year, dt.month, dt.day);
    }

    uint8_t raw[7];
    raw[0] = bcdEncode(dt.second);
    raw[1] = bcdEncode(dt.minute);
    raw[2] = bcdEncode(dt.hour);
    raw[3] = dt.dayOfWeek;
    raw[4] = bcdEncode(dt.day);
    raw[5] = bcdEncode(dt.month);
    raw[6] = bcdEncode(static_cast<uint8_t>(dt.year - 2000));

    if (!writeBytes(REG_SECONDS, raw, sizeof(raw)))
    {
        return false;
    }

    clearOscillatorStopFlag();
    return true;
}

// =====================================================
// Unix conversion
// =====================================================
uint32_t JW_RTC::toUnix(const DateTime& dt)
{
    int32_t y = dt.year;
    uint32_t m = dt.month;
    uint32_t d = dt.day;

    y -= (m <= 2);
    const int32_t era = (y >= 0 ? y : y - 399) / 400;
    const uint32_t yoe = static_cast<uint32_t>(y - era * 400);
    const uint32_t doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    const uint32_t doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    const int32_t days = era * 146097 + static_cast<int32_t>(doe) - 719468;

    return static_cast<uint32_t>(days) * 86400UL +
           static_cast<uint32_t>(dt.hour) * 3600UL +
           static_cast<uint32_t>(dt.minute) * 60UL +
           static_cast<uint32_t>(dt.second);
}

bool JW_RTC::fromUnix(uint32_t unixTime, DateTime& dt)
{
    int32_t z = static_cast<int32_t>(unixTime / 86400UL) + 719468;
    uint32_t rem = unixTime % 86400UL;

    const int32_t era = (z >= 0 ? z : z - 146096) / 146097;
    const uint32_t doe = static_cast<uint32_t>(z - era * 146097);
    const uint32_t yoe = (doe - doe/1460 + doe/36524 - doe/146096) / 365;
    int32_t y = static_cast<int32_t>(yoe) + era * 400;
    const uint32_t doy = doe - (365 * yoe + yoe/4 - yoe/100);
    const uint32_t mp = (5 * doy + 2) / 153;
    const uint32_t d = doy - (153 * mp + 2) / 5 + 1;
    const uint32_t m = mp + (mp < 10 ? 3 : -9);
    y += (m <= 2);

    dt.year = static_cast<uint16_t>(y);
    dt.month = static_cast<uint8_t>(m);
    dt.day = static_cast<uint8_t>(d);
    dt.hour = static_cast<uint8_t>(rem / 3600UL);
    rem %= 3600UL;
    dt.minute = static_cast<uint8_t>(rem / 60UL);
    dt.second = static_cast<uint8_t>(rem % 60UL);
    dt.dayOfWeek = computeDayOfWeek(dt.year, dt.month, dt.day);
    dt.valid = isValidDateTime(dt);

    return dt.valid;
}

bool JW_RTC::readUnix(uint32_t& unixTime)
{
    DateTime dt;
    if (!read(dt) || !dt.valid)
    {
        return false;
    }

    unixTime = toUnix(dt);
    return true;
}

bool JW_RTC::writeUnix(uint32_t unixTime)
{
    DateTime dt;
    if (!fromUnix(unixTime, dt))
    {
        setError(Error::InvalidArgument);
        return false;
    }

    return write(dt);
}

bool JW_RTC::fromBuildTime(const char* buildDate, const char* buildTime, DateTime& dt)
{
    if (!buildDate || !buildTime)
    {
        return false;
    }

    static const char* months[12] = {
        "Jan","Feb","Mar","Apr","May","Jun",
        "Jul","Aug","Sep","Oct","Nov","Dec"
    };

    char mon[4] = {buildDate[0], buildDate[1], buildDate[2], '\0'};
    uint8_t month = 0;
    for (uint8_t i = 0; i < 12; i++)
    {
        if (strcmp(mon, months[i]) == 0)
        {
            month = i + 1;
            break;
        }
    }

    if (month == 0) return false;

    dt.month = month;
    dt.day = static_cast<uint8_t>(atoi(buildDate + 4));
    dt.year = static_cast<uint16_t>(atoi(buildDate + 7));
    dt.hour = static_cast<uint8_t>(atoi(buildTime + 0));
    dt.minute = static_cast<uint8_t>(atoi(buildTime + 3));
    dt.second = static_cast<uint8_t>(atoi(buildTime + 6));
    dt.dayOfWeek = computeDayOfWeek(dt.year, dt.month, dt.day);
    dt.valid = isValidDateTime(dt);

    return dt.valid;
}

// =====================================================
// Temperature / aging
// =====================================================
bool JW_RTC::readTemperatureCentiC(int16_t& tempCenti)
{
    uint8_t raw[2];
    if (!readBytes(REG_TEMP_MSB, raw, 2))
    {
        return false;
    }

    int16_t quarterC = static_cast<int16_t>((static_cast<int8_t>(raw[0]) << 2) | (raw[1] >> 6));
    tempCenti = static_cast<int16_t>(quarterC * 25);
    return true;
}

bool JW_RTC::readTemperatureC(float& tempC)
{
    int16_t centi = 0;
    if (!readTemperatureCentiC(centi))
    {
        return false;
    }

    tempC = static_cast<float>(centi) / 100.0f;
    return true;
}

bool JW_RTC::getAgingOffset(int8_t& offset)
{
    uint8_t v = 0;
    if (!readRegister(REG_AGING, v))
    {
        return false;
    }

    offset = static_cast<int8_t>(v);
    return true;
}

bool JW_RTC::setAgingOffset(int8_t offset)
{
    return writeRegister(REG_AGING, static_cast<uint8_t>(offset));
}

bool JW_RTC::forceTemperatureConversion()
{
    return updateControl(CTRL_CONV, CTRL_CONV);
}

// =====================================================
// Control / status
// =====================================================
bool JW_RTC::readControl(uint8_t& value)
{
    return readRegister(REG_CONTROL, value);
}

bool JW_RTC::writeControl(uint8_t value)
{
    return writeRegister(REG_CONTROL, value);
}

bool JW_RTC::updateControl(uint8_t mask, uint8_t value)
{
    uint8_t reg = 0;
    if (!readControl(reg)) return false;
    reg = static_cast<uint8_t>((reg & ~mask) | (value & mask));
    return writeControl(reg);
}

bool JW_RTC::readStatus(uint8_t& value)
{
    return readRegister(REG_STATUS, value);
}

bool JW_RTC::writeStatus(uint8_t value)
{
    return writeRegister(REG_STATUS, value);
}

bool JW_RTC::updateStatus(uint8_t mask, uint8_t value)
{
    uint8_t reg = 0;
    if (!readStatus(reg)) return false;
    reg = static_cast<uint8_t>((reg & ~mask) | (value & mask));
    return writeStatus(reg);
}

bool JW_RTC::setSquareWave(SquareWaveMode mode, bool batteryBacked)
{
    uint8_t rsBits = 0;
    bool enableSqw = true;

    switch (mode)
    {
        case SquareWaveMode::Off:
            enableSqw = false;
            break;
        case SquareWaveMode::Hz1:
            rsBits = 0x00;
            break;
        case SquareWaveMode::Hz1024:
            rsBits = CTRL_RS1;
            break;
        case SquareWaveMode::Hz4096:
            rsBits = CTRL_RS2;
            break;
        case SquareWaveMode::Hz8192:
            rsBits = CTRL_RS2 | CTRL_RS1;
            break;
    }

    uint8_t value = batteryBacked ? CTRL_BBSQW : 0x00;
    if (!enableSqw)
    {
        value |= CTRL_INTCN;
    }
    else
    {
        value |= rsBits;
    }

    return updateControl(static_cast<uint8_t>(CTRL_BBSQW | CTRL_RS2 | CTRL_RS1 | CTRL_INTCN), value);
}

bool JW_RTC::set32kHzOutput(bool enable)
{
    return updateStatus(STAT_EN32KHZ, enable ? STAT_EN32KHZ : 0x00);
}

bool JW_RTC::get32kHzOutput(bool& enable)
{
    uint8_t status = 0;
    if (!readStatus(status)) return false;
    enable = (status & STAT_EN32KHZ) != 0;
    return true;
}

// =====================================================
// Alarms
// =====================================================
bool JW_RTC::setAlarm1(const Alarm1Config& cfg)
{
    if (cfg.second > 59 || cfg.minute > 59 || cfg.hour > 23)
    {
        setError(Error::InvalidArgument);
        return false;
    }

    if (cfg.dayOfWeek)
    {
        if (cfg.day < 1 || cfg.day > 7)
        {
            setError(Error::InvalidArgument);
            return false;
        }
    }
    else
    {
        if (cfg.day < 1 || cfg.day > 31)
        {
            setError(Error::InvalidArgument);
            return false;
        }
    }

    bool m1 = false, m2 = false, m3 = false, m4 = false;
    bool dayOfWeek = cfg.dayOfWeek;

    switch (cfg.mode)
    {
        case Alarm1Mode::EverySecond:
            m1 = m2 = m3 = m4 = true;
            break;
        case Alarm1Mode::MatchSeconds:
            m1 = false; m2 = m3 = m4 = true;
            break;
        case Alarm1Mode::MatchMinutesSeconds:
            m1 = false; m2 = false; m3 = m4 = true;
            break;
        case Alarm1Mode::MatchHoursMinutesSeconds:
            m1 = false; m2 = false; m3 = false; m4 = true;
            break;
        case Alarm1Mode::MatchDateHoursMinutesSeconds:
            m1 = m2 = m3 = m4 = false;
            dayOfWeek = false;
            break;
        case Alarm1Mode::MatchDayHoursMinutesSeconds:
            m1 = m2 = m3 = m4 = false;
            dayOfWeek = true;
            break;
    }

    uint8_t raw[4];
    raw[0] = static_cast<uint8_t>(bcdEncode(cfg.second) | (m1 ? 0x80 : 0x00));
    raw[1] = static_cast<uint8_t>(bcdEncode(cfg.minute) | (m2 ? 0x80 : 0x00));
    raw[2] = static_cast<uint8_t>(bcdEncode(cfg.hour)   | (m3 ? 0x80 : 0x00));
    raw[3] = static_cast<uint8_t>(bcdEncode(cfg.day)    | (m4 ? 0x80 : 0x00) | (dayOfWeek ? 0x40 : 0x00));

    return writeBytes(REG_ALARM1_SEC, raw, sizeof(raw));
}

bool JW_RTC::setAlarm2(const Alarm2Config& cfg)
{
    if (cfg.minute > 59 || cfg.hour > 23)
    {
        setError(Error::InvalidArgument);
        return false;
    }

    if (cfg.dayOfWeek)
    {
        if (cfg.day < 1 || cfg.day > 7)
        {
            setError(Error::InvalidArgument);
            return false;
        }
    }
    else
    {
        if (cfg.day < 1 || cfg.day > 31)
        {
            setError(Error::InvalidArgument);
            return false;
        }
    }

    bool m2 = false, m3 = false, m4 = false;
    bool dayOfWeek = cfg.dayOfWeek;

    switch (cfg.mode)
    {
        case Alarm2Mode::EveryMinute:
            m2 = m3 = m4 = true;
            break;
        case Alarm2Mode::MatchMinutes:
            m2 = false; m3 = m4 = true;
            break;
        case Alarm2Mode::MatchHoursMinutes:
            m2 = false; m3 = false; m4 = true;
            break;
        case Alarm2Mode::MatchDateHoursMinutes:
            m2 = m3 = m4 = false;
            dayOfWeek = false;
            break;
        case Alarm2Mode::MatchDayHoursMinutes:
            m2 = m3 = m4 = false;
            dayOfWeek = true;
            break;
    }

    uint8_t raw[3];
    raw[0] = static_cast<uint8_t>(bcdEncode(cfg.minute) | (m2 ? 0x80 : 0x00));
    raw[1] = static_cast<uint8_t>(bcdEncode(cfg.hour)   | (m3 ? 0x80 : 0x00));
    raw[2] = static_cast<uint8_t>(bcdEncode(cfg.day)    | (m4 ? 0x80 : 0x00) | (dayOfWeek ? 0x40 : 0x00));

    return writeBytes(REG_ALARM2_MIN, raw, sizeof(raw));
}

bool JW_RTC::enableAlarmInterrupts(bool alarm1Enable, bool alarm2Enable)
{
    uint8_t value = CTRL_INTCN;
    if (alarm1Enable) value |= CTRL_A1IE;
    if (alarm2Enable) value |= CTRL_A2IE;

    return updateControl(static_cast<uint8_t>(CTRL_INTCN | CTRL_A1IE | CTRL_A2IE), value);
}

bool JW_RTC::getAlarm1Flag(bool& fired)
{
    uint8_t status = 0;
    if (!readStatus(status)) return false;
    fired = (status & STAT_A1F) != 0;
    return true;
}

bool JW_RTC::getAlarm2Flag(bool& fired)
{
    uint8_t status = 0;
    if (!readStatus(status)) return false;
    fired = (status & STAT_A2F) != 0;
    return true;
}

bool JW_RTC::clearAlarm1Flag()
{
    return updateStatus(STAT_A1F, 0x00);
}

bool JW_RTC::clearAlarm2Flag()
{
    return updateStatus(STAT_A2F, 0x00);
}

bool JW_RTC::clearAlarmFlags()
{
    return updateStatus(static_cast<uint8_t>(STAT_A1F | STAT_A2F), 0x00);
}

// =====================================================
// NVRAM
// =====================================================
bool JW_RTC::nvramRead(uint8_t addr, uint8_t* data, size_t len)
{
    if (!data && len > 0)
    {
        setError(Error::InvalidArgument);
        return false;
    }

    if ((uint16_t)addr + (uint16_t)len > NVRAM_SIZE)
    {
        setError(Error::OutOfRange);
        return false;
    }

    return readBytes(static_cast<uint8_t>(REG_NVRAM + addr), data, len);
}

bool JW_RTC::nvramWrite(uint8_t addr, const uint8_t* data, size_t len)
{
    if (!data && len > 0)
    {
        setError(Error::InvalidArgument);
        return false;
    }

    if ((uint16_t)addr + (uint16_t)len > NVRAM_SIZE)
    {
        setError(Error::OutOfRange);
        return false;
    }

    return writeBytes(static_cast<uint8_t>(REG_NVRAM + addr), data, len);
}

bool JW_RTC::nvramReadByte(uint8_t addr, uint8_t& value)
{
    return nvramRead(addr, &value, 1);
}

bool JW_RTC::nvramWriteByte(uint8_t addr, uint8_t value)
{
    return nvramWrite(addr, &value, 1);
}

// =====================================================
// Error strings
// =====================================================
const __FlashStringHelper* JW_RTC::errorToString(Error error)
{
    switch (error)
    {
        case Error::Ok:              return F("Ok");
        case Error::DeviceNotFound:  return F("DeviceNotFound");
        case Error::BusInitFailed:   return F("BusInitFailed");
        case Error::ReadFailed:      return F("ReadFailed");
        case Error::WriteFailed:     return F("WriteFailed");
        case Error::InvalidArgument: return F("InvalidArgument");
        case Error::OutOfRange:      return F("OutOfRange");
        case Error::NotReady:        return F("NotReady");
        default:                     return F("Unknown");
    }
}