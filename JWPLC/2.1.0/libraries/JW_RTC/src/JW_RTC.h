#ifndef JW_RTC_H
#define JW_RTC_H

#include <Arduino.h>
#include <stdint.h>
#include <stddef.h>

extern "C" {
  #include "jwplc_i2c_bridge.h"
}

class JW_RTC
{
public:
    static constexpr uint8_t DEFAULT_ADDRESS = 0x68;
    static constexpr uint8_t NVRAM_SIZE = 236;

    enum class Error : uint8_t
    {
        Ok = 0,
        DeviceNotFound,
        BusInitFailed,
        ReadFailed,
        WriteFailed,
        InvalidArgument,
        OutOfRange,
        NotReady
    };

    enum class SquareWaveMode : uint8_t
    {
        Off = 0,
        Hz1,
        Hz1024,
        Hz4096,
        Hz8192
    };

    enum class Alarm1Mode : uint8_t
    {
        EverySecond = 0,
        MatchSeconds,
        MatchMinutesSeconds,
        MatchHoursMinutesSeconds,
        MatchDateHoursMinutesSeconds,
        MatchDayHoursMinutesSeconds
    };

    enum class Alarm2Mode : uint8_t
    {
        EveryMinute = 0,
        MatchMinutes,
        MatchHoursMinutes,
        MatchDateHoursMinutes,
        MatchDayHoursMinutes
    };

    struct DateTime
    {
        uint16_t year = 2000;
        uint8_t month = 1;
        uint8_t day = 1;
        uint8_t hour = 0;
        uint8_t minute = 0;
        uint8_t second = 0;
        uint8_t dayOfWeek = 1; // 1=Sunday ... 7=Saturday
        bool valid = false;
    };

    struct Alarm1Config
    {
        Alarm1Mode mode = Alarm1Mode::EverySecond;
        uint8_t second = 0;
        uint8_t minute = 0;
        uint8_t hour = 0;
        uint8_t day = 1;       // day of month or day of week
        bool dayOfWeek = false;
    };

    struct Alarm2Config
    {
        Alarm2Mode mode = Alarm2Mode::EveryMinute;
        uint8_t minute = 0;
        uint8_t hour = 0;
        uint8_t day = 1;       // day of month or day of week
        bool dayOfWeek = false;
    };

    explicit JW_RTC(uint8_t address = DEFAULT_ADDRESS);

    bool begin();
    bool beginWithPins(uint8_t sdaPin, uint8_t sclPin, uint32_t frequencyHz = 400000UL);
    bool setClock(uint32_t frequencyHz);

    bool isPresent();

    bool read(DateTime& dt);
    DateTime now();

    bool write(const DateTime& dt);

    bool readUnix(uint32_t& unixTime);
    bool writeUnix(uint32_t unixTime);

    bool isTimeValid();
    bool lostPower();
    bool clearOscillatorStopFlag();

    bool readTemperatureC(float& tempC);
    bool readTemperatureCentiC(int16_t& tempCenti);

    bool getAgingOffset(int8_t& offset);
    bool setAgingOffset(int8_t offset);

    bool forceTemperatureConversion();

    bool setSquareWave(SquareWaveMode mode, bool batteryBacked = false);
    bool set32kHzOutput(bool enable);
    bool get32kHzOutput(bool& enable);

    bool readControl(uint8_t& value);
    bool writeControl(uint8_t value);
    bool updateControl(uint8_t mask, uint8_t value);

    bool readStatus(uint8_t& value);
    bool writeStatus(uint8_t value);
    bool updateStatus(uint8_t mask, uint8_t value);

    bool setAlarm1(const Alarm1Config& cfg);
    bool setAlarm2(const Alarm2Config& cfg);
    bool enableAlarmInterrupts(bool alarm1Enable, bool alarm2Enable);

    bool getAlarm1Flag(bool& fired);
    bool getAlarm2Flag(bool& fired);
    bool clearAlarm1Flag();
    bool clearAlarm2Flag();
    bool clearAlarmFlags();

    bool nvramRead(uint8_t addr, uint8_t* data, size_t len);
    bool nvramWrite(uint8_t addr, const uint8_t* data, size_t len);

    bool nvramReadByte(uint8_t addr, uint8_t& value);
    bool nvramWriteByte(uint8_t addr, uint8_t value);

    template <typename T>
    bool nvramReadObject(uint8_t addr, T& value)
    {
        return nvramRead(addr, reinterpret_cast<uint8_t*>(&value), sizeof(T));
    }

    template <typename T>
    bool nvramWriteObject(uint8_t addr, const T& value)
    {
        return nvramWrite(addr, reinterpret_cast<const uint8_t*>(&value), sizeof(T));
    }

    Error lastError() const;
    void clearError();

    static const __FlashStringHelper* errorToString(Error error);

    static bool isLeapYear(uint16_t year);
    static uint8_t daysInMonth(uint16_t year, uint8_t month);
    static bool isValidDateTime(const DateTime& dt);

    static bool fromUnix(uint32_t unixTime, DateTime& dt);
    static uint32_t toUnix(const DateTime& dt);

    static bool fromBuildTime(const char* buildDate, const char* buildTime, DateTime& dt);

private:
    uint8_t _address;
    bool _ready;
    Error _lastError;

    static constexpr uint8_t REG_SECONDS   = 0x00;
    static constexpr uint8_t REG_MINUTES   = 0x01;
    static constexpr uint8_t REG_HOURS     = 0x02;
    static constexpr uint8_t REG_DAY       = 0x03;
    static constexpr uint8_t REG_DATE      = 0x04;
    static constexpr uint8_t REG_MONTH     = 0x05;
    static constexpr uint8_t REG_YEAR      = 0x06;

    static constexpr uint8_t REG_ALARM1_SEC  = 0x07;
    static constexpr uint8_t REG_ALARM1_MIN  = 0x08;
    static constexpr uint8_t REG_ALARM1_HOUR = 0x09;
    static constexpr uint8_t REG_ALARM1_DAY  = 0x0A;

    static constexpr uint8_t REG_ALARM2_MIN  = 0x0B;
    static constexpr uint8_t REG_ALARM2_HOUR = 0x0C;
    static constexpr uint8_t REG_ALARM2_DAY  = 0x0D;

    static constexpr uint8_t REG_CONTROL   = 0x0E;
    static constexpr uint8_t REG_STATUS    = 0x0F;
    static constexpr uint8_t REG_AGING     = 0x10;
    static constexpr uint8_t REG_TEMP_MSB  = 0x11;
    static constexpr uint8_t REG_TEMP_LSB  = 0x12;
    static constexpr uint8_t REG_NVRAM     = 0x14;

    static constexpr uint8_t CTRL_EOSC   = 0x80;
    static constexpr uint8_t CTRL_BBSQW  = 0x40;
    static constexpr uint8_t CTRL_CONV   = 0x20;
    static constexpr uint8_t CTRL_RS2    = 0x10;
    static constexpr uint8_t CTRL_RS1    = 0x08;
    static constexpr uint8_t CTRL_INTCN  = 0x04;
    static constexpr uint8_t CTRL_A2IE   = 0x02;
    static constexpr uint8_t CTRL_A1IE   = 0x01;

    static constexpr uint8_t STAT_OSF     = 0x80;
    static constexpr uint8_t STAT_BB32KHZ = 0x40;
    static constexpr uint8_t STAT_CRATE1  = 0x20;
    static constexpr uint8_t STAT_CRATE0  = 0x10;
    static constexpr uint8_t STAT_EN32KHZ = 0x08;
    static constexpr uint8_t STAT_BSY     = 0x04;
    static constexpr uint8_t STAT_A2F     = 0x02;
    static constexpr uint8_t STAT_A1F     = 0x01;

    static uint8_t bcdEncode(uint8_t value);
    static uint8_t bcdDecode(uint8_t value);
    static uint8_t computeDayOfWeek(uint16_t year, uint8_t month, uint8_t day);

    bool readRegister(uint8_t reg, uint8_t& value);
    bool writeRegister(uint8_t reg, uint8_t value);

    bool readBytes(uint8_t reg, uint8_t* data, size_t len);
    bool writeBytes(uint8_t reg, const uint8_t* data, size_t len);

    void setError(Error error);
};

// =====================================================
// Aliases amigables tipo Arduino / JW
// =====================================================
using JWRTCDateTime       = JW_RTC::DateTime;
using JWRTCAlarm1Config   = JW_RTC::Alarm1Config;
using JWRTCAlarm2Config   = JW_RTC::Alarm2Config;
using JWRTCError          = JW_RTC::Error;
using JWRTCSquareWaveMode = JW_RTC::SquareWaveMode;
using JWRTCAlarm1Mode     = JW_RTC::Alarm1Mode;
using JWRTCAlarm2Mode     = JW_RTC::Alarm2Mode;

#endif