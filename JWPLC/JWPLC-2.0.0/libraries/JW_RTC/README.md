# JW_RTC

RTC library for **DS3232M / DS3232** with a clean JW-style API.

## Overview

`JW_RTC` is the RTC library used in the JW Control ecosystem for the **DS3232M / DS3232** family.

### Current scope
Version **1.0.1** is designed for the **JWPLC package** and uses `jwplc_i2c_bridge` internally.

This version already provides:

- clean `DateTime` handling without `TimeLib`
- direct read/write of date and time
- Unix timestamp conversion
- RTC validity check (`OSF`)
- temperature reading
- aging offset read/write
- square-wave and 32kHz output control
- Alarm 1 / Alarm 2 configuration
- battery-backed SRAM access
- friendlier aliases like `JWRTCDateTime`

A future version can add a second backend for `Wire` to support generic Arduino and ESP32 boards outside the JWPLC environment.

---

## Installation

Copy the library into your Arduino libraries folder or include it in your JWPLC package libraries folder.

Main header:

```cpp
#include <JW_RTC.h>
```

---

## Quick start

```cpp
#include <JW_RTC.h>

JW_RTC rtc;

void setup()
{
  Serial.begin(115200);
  delay(300);

  if (!rtc.begin())
  {
    Serial.print("RTC begin failed: ");
    Serial.println(JW_RTC::errorToString(rtc.lastError()));
    return;
  }

  Serial.println("RTC ready");
}

void loop()
{
  JWRTCDateTime dt;

  if (rtc.read(dt))
  {
    Serial.print(dt.year);
    Serial.print('/');
    Serial.print(dt.month);
    Serial.print('/');
    Serial.print(dt.day);
    Serial.print(' ');
    Serial.print(dt.hour);
    Serial.print(':');
    Serial.print(dt.minute);
    Serial.print(':');
    Serial.println(dt.second);
  }

  delay(1000);
}
```

---

## Friendly aliases

Version 1.0.1 adds easier names so user sketches do not need as much `::`.

### Available aliases

```cpp
JWRTCDateTime
JWRTCAlarm1Config
JWRTCAlarm2Config
JWRTCError
JWRTCSquareWaveMode
JWRTCAlarm1Mode
JWRTCAlarm2Mode
```

### Example

```cpp
JWRTCDateTime dt;
dt.year = 2026;
dt.month = 4;
dt.day = 19;
dt.hour = 23;
dt.minute = 45;
dt.second = 0;
dt.dayOfWeek = 0; // optional, auto-calculated on write
```

---

## DateTime structure

`JWRTCDateTime` contains:

```cpp
struct DateTime
{
    uint16_t year;
    uint8_t month;
    uint8_t day;
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
    uint8_t dayOfWeek; // 1=Sunday ... 7=Saturday
    bool valid;
};
```

### Notes

- valid year range: `2000..2099`
- if `dayOfWeek = 0` during `write()`, it is auto-calculated
- `valid` is typically filled when the structure is read from the RTC

---

## Constructors and initialization

### `JW_RTC rtc;`

Creates an RTC object using the default DS3232M / DS3232 address:

```cpp
JW_RTC rtc;
```

### `bool begin()`

Starts the RTC using the JWPLC bridge defaults.

```cpp
if (!rtc.begin())
{
  Serial.println(JW_RTC::errorToString(rtc.lastError()));
}
```

### `bool beginWithPins(uint8_t sdaPin, uint8_t sclPin, uint32_t frequencyHz = 400000UL)`

Useful when you want to initialize the bridge with explicit pins and frequency.

```cpp
if (!rtc.beginWithPins(21, 22, 400000UL))
{
  Serial.println("RTC beginWithPins failed");
}
```

### `bool setClock(uint32_t frequencyHz)`

Changes I2C clock after begin.

```cpp
rtc.setClock(100000UL);   // 100 kHz
rtc.setClock(400000UL);   // 400 kHz
```

---

## Presence and validity

### `bool isPresent()`

Checks whether the device responds.

```cpp
if (rtc.isPresent())
{
  Serial.println("RTC detected");
}
else
{
  Serial.println("RTC not found");
}
```

### `bool isTimeValid()`

Checks whether the oscillator stop flag (`OSF`) indicates valid time.

```cpp
if (rtc.isTimeValid())
{
  Serial.println("RTC time is valid");
}
else
{
  Serial.println("RTC lost power or time is not valid");
}
```

### `bool lostPower()`

Convenience helper equivalent to "time is not valid".

```cpp
if (rtc.lostPower())
{
  Serial.println("RTC lost power");
}
```

### `bool clearOscillatorStopFlag()`

Clears the `OSF` bit after setting or validating time.

```cpp
rtc.clearOscillatorStopFlag();
```

---

## Read date and time

### `bool read(JWRTCDateTime& dt)`

Reads current date and time into a structure.

```cpp
JWRTCDateTime dt;

if (rtc.read(dt))
{
  Serial.print(dt.year);
  Serial.print('/');
  Serial.print(dt.month);
  Serial.print('/');
  Serial.println(dt.day);
}
```

### `JWRTCDateTime now()`

Returns a `DateTime` by value.

```cpp
JWRTCDateTime dt = rtc.now();
```

---

## Write date and time

### `bool write(const JWRTCDateTime& dt)`

Writes date and time to the RTC.

```cpp
JWRTCDateTime dt;
dt.year = 2026;
dt.month = 4;
dt.day = 19;
dt.hour = 23;
dt.minute = 59;
dt.second = 0;
dt.dayOfWeek = 0; // auto-calculate

if (!rtc.write(dt))
{
  Serial.println(JW_RTC::errorToString(rtc.lastError()));
}
```

---

## Build-time helper

### `static bool fromBuildTime(const char* buildDate, const char* buildTime, JWRTCDateTime& dt)`

Parses `__DATE__` and `__TIME__`.

```cpp
JWRTCDateTime dt;

if (JW_RTC::fromBuildTime(__DATE__, __TIME__, dt))
{
  rtc.write(dt);
}
```

This is useful for setting the RTC at upload time.

---

## Unix timestamp conversion

### `bool readUnix(uint32_t& unixTime)`

Reads RTC and converts to Unix timestamp.

```cpp
uint32_t ts = 0;

if (rtc.readUnix(ts))
{
  Serial.print("Unix: ");
  Serial.println(ts);
}
```

### `bool writeUnix(uint32_t unixTime)`

Writes RTC from Unix timestamp.

```cpp
rtc.writeUnix(1776542400UL);
```

### `static uint32_t toUnix(const JWRTCDateTime& dt)`

Converts a `DateTime` structure to Unix timestamp.

```cpp
JWRTCDateTime dt;
dt.year = 2026;
dt.month = 4;
dt.day = 19;
dt.hour = 12;
dt.minute = 0;
dt.second = 0;

uint32_t ts = JW_RTC::toUnix(dt);
```

### `static bool fromUnix(uint32_t unixTime, JWRTCDateTime& dt)`

Converts Unix timestamp to `DateTime`.

```cpp
JWRTCDateTime dt;
JW_RTC::fromUnix(1776542400UL, dt);
```

---

## Temperature

The DS3232M / DS3232 provides internal temperature information.

### `bool readTemperatureC(float& tempC)`

Returns temperature in Celsius as float.

```cpp
float tempC = 0.0f;

if (rtc.readTemperatureC(tempC))
{
  Serial.print("Temp C: ");
  Serial.println(tempC, 2);
}
```

### `bool readTemperatureCentiC(int16_t& tempCenti)`

Returns temperature in hundredths of a degree.

```cpp
int16_t tempCenti = 0;

if (rtc.readTemperatureCentiC(tempCenti))
{
  Serial.print("Temp x100: ");
  Serial.println(tempCenti);
}
```

Example: `3725` means `37.25 °C`.

---

## Aging offset

### `bool getAgingOffset(int8_t& offset)`

Reads the aging offset register.

```cpp
int8_t offset = 0;
rtc.getAgingOffset(offset);
Serial.println(offset);
```

### `bool setAgingOffset(int8_t offset)`

Writes the aging offset register.

```cpp
rtc.setAgingOffset(-2);
```

---

## Force temperature conversion

### `bool forceTemperatureConversion()`

Requests a manual temperature conversion.

```cpp
if (rtc.forceTemperatureConversion())
{
  Serial.println("Conversion requested");
}
```

---

## Square-wave output

### `bool setSquareWave(JWRTCSquareWaveMode mode, bool batteryBacked = false)`

Configures the SQW output.

Supported modes:

- `JW_RTC::SquareWaveMode::Off`
- `JW_RTC::SquareWaveMode::Hz1`
- `JW_RTC::SquareWaveMode::Hz1024`
- `JW_RTC::SquareWaveMode::Hz4096`
- `JW_RTC::SquareWaveMode::Hz8192`

Example:

```cpp
rtc.setSquareWave(JW_RTC::SquareWaveMode::Hz1);
```

Battery-backed example:

```cpp
rtc.setSquareWave(JW_RTC::SquareWaveMode::Hz1, true);
```

---

## 32kHz output

### `bool set32kHzOutput(bool enable)`

Enables or disables the 32kHz output.

```cpp
rtc.set32kHzOutput(true);
```

### `bool get32kHzOutput(bool& enable)`

Reads the 32kHz output state.

```cpp
bool en = false;
rtc.get32kHzOutput(en);
Serial.println(en);
```

---

## Raw control/status registers

These helpers are useful for advanced users.

### `bool readControl(uint8_t& value)`

```cpp
uint8_t ctrl = 0;
rtc.readControl(ctrl);
```

### `bool writeControl(uint8_t value)`

```cpp
rtc.writeControl(0x04);
```

### `bool updateControl(uint8_t mask, uint8_t value)`

```cpp
rtc.updateControl(0x04, 0x04);
```

### `bool readStatus(uint8_t& value)`

```cpp
uint8_t status = 0;
rtc.readStatus(status);
```

### `bool writeStatus(uint8_t value)`

```cpp
rtc.writeStatus(0x00);
```

### `bool updateStatus(uint8_t mask, uint8_t value)`

```cpp
rtc.updateStatus(0x80, 0x00); // clear OSF
```

---

## Alarm 1

### `bool setAlarm1(const JWRTCAlarm1Config& cfg)`

Alarm 1 supports:

- every second
- seconds match
- minutes/seconds match
- hours/minutes/seconds match
- date/hours/minutes/seconds match
- day/hours/minutes/seconds match

Example: daily at 08:30:00

```cpp
JWRTCAlarm1Config a1;
a1.mode = JW_RTC::Alarm1Mode::MatchDateHoursMinutesSeconds;
a1.second = 0;
a1.minute = 30;
a1.hour = 8;
a1.day = 1;         // ignored unless mode uses date/day
a1.dayOfWeek = false;

rtc.setAlarm1(a1);
```

Example: weekly by day-of-week

```cpp
JWRTCAlarm1Config a1;
a1.mode = JW_RTC::Alarm1Mode::MatchDayHoursMinutesSeconds;
a1.second = 0;
a1.minute = 0;
a1.hour = 7;
a1.day = 2;         // Monday if 1=Sunday
a1.dayOfWeek = true;

rtc.setAlarm1(a1);
```

---

## Alarm 2

### `bool setAlarm2(const JWRTCAlarm2Config& cfg)`

Alarm 2 supports:

- every minute
- minute match
- hour/minute match
- date/hour/minute match
- day/hour/minute match

Example: daily at 18:45

```cpp
JWRTCAlarm2Config a2;
a2.mode = JW_RTC::Alarm2Mode::MatchHoursMinutes;
a2.minute = 45;
a2.hour = 18;
a2.day = 1;
a2.dayOfWeek = false;

rtc.setAlarm2(a2);
```

---

## Alarm interrupts and flags

### `bool enableAlarmInterrupts(bool alarm1Enable, bool alarm2Enable)`

```cpp
rtc.enableAlarmInterrupts(true, false);   // only alarm 1
```

### `bool getAlarm1Flag(bool& fired)`

```cpp
bool fired = false;
rtc.getAlarm1Flag(fired);
```

### `bool getAlarm2Flag(bool& fired)`

```cpp
bool fired = false;
rtc.getAlarm2Flag(fired);
```

### `bool clearAlarm1Flag()`

```cpp
rtc.clearAlarm1Flag();
```

### `bool clearAlarm2Flag()`

```cpp
rtc.clearAlarm2Flag();
```

### `bool clearAlarmFlags()`

```cpp
rtc.clearAlarmFlags();
```

---

## Battery-backed NVRAM

The DS3232M / DS3232 provides `236 bytes` of battery-backed SRAM.

### `bool nvramRead(uint8_t addr, uint8_t* data, size_t len)`

```cpp
uint8_t buf[8];
rtc.nvramRead(0, buf, sizeof(buf));
```

### `bool nvramWrite(uint8_t addr, const uint8_t* data, size_t len)`

```cpp
uint8_t buf[4] = {1,2,3,4};
rtc.nvramWrite(0, buf, sizeof(buf));
```

### `bool nvramReadByte(uint8_t addr, uint8_t& value)`

```cpp
uint8_t value = 0;
rtc.nvramReadByte(0, value);
```

### `bool nvramWriteByte(uint8_t addr, uint8_t value)`

```cpp
rtc.nvramWriteByte(0, 42);
```

### `template <typename T> bool nvramReadObject(uint8_t addr, T& value)`

```cpp
struct ConfigData
{
  uint16_t magic;
  uint32_t counter;
};

ConfigData data;
rtc.nvramReadObject(0, data);
```

### `template <typename T> bool nvramWriteObject(uint8_t addr, const T& value)`

```cpp
ConfigData data;
data.magic = 0x55AA;
data.counter = 123;
rtc.nvramWriteObject(0, data);
```

---

## Error handling

### `JWRTCError lastError() const`

Returns the last error code.

```cpp
JWRTCError err = rtc.lastError();
```

### `void clearError()`

```cpp
rtc.clearError();
```

### `static const __FlashStringHelper* errorToString(JWRTCError error)`

```cpp
Serial.println(JW_RTC::errorToString(rtc.lastError()));
```

Typical errors:

- `Ok`
- `DeviceNotFound`
- `BusInitFailed`
- `ReadFailed`
- `WriteFailed`
- `InvalidArgument`
- `OutOfRange`
- `NotReady`

---

## Utility helpers

### `static bool isLeapYear(uint16_t year)`

```cpp
bool leap = JW_RTC::isLeapYear(2028);
```

### `static uint8_t daysInMonth(uint16_t year, uint8_t month)`

```cpp
uint8_t days = JW_RTC::daysInMonth(2026, 2);
```

### `static bool isValidDateTime(const JWRTCDateTime& dt)`

```cpp
if (JW_RTC::isValidDateTime(dt))
{
  Serial.println("DateTime is valid");
}
```

---

## Complete example

```cpp
#include <JW_RTC.h>

JW_RTC rtc;

void printDateTime(const JWRTCDateTime& dt)
{
  Serial.print(dt.year);
  Serial.print('/');
  if (dt.month < 10) Serial.print('0');
  Serial.print(dt.month);
  Serial.print('/');
  if (dt.day < 10) Serial.print('0');
  Serial.print(dt.day);
  Serial.print(' ');

  if (dt.hour < 10) Serial.print('0');
  Serial.print(dt.hour);
  Serial.print(':');
  if (dt.minute < 10) Serial.print('0');
  Serial.print(dt.minute);
  Serial.print(':');
  if (dt.second < 10) Serial.print('0');
  Serial.println(dt.second);
}

void setup()
{
  Serial.begin(115200);
  delay(300);

  if (!rtc.begin())
  {
    Serial.print("RTC begin failed: ");
    Serial.println(JW_RTC::errorToString(rtc.lastError()));
    return;
  }

  Serial.println("RTC ready");

  if (rtc.lostPower())
  {
    JWRTCDateTime buildDt;
    if (JW_RTC::fromBuildTime(__DATE__, __TIME__, buildDt))
    {
      rtc.write(buildDt);
      Serial.println("RTC set from build time");
    }
  }
}

void loop()
{
  JWRTCDateTime dt;
  float tempC = 0.0f;

  if (rtc.read(dt))
  {
    printDateTime(dt);
  }
  else
  {
    Serial.print("Read failed: ");
    Serial.println(JW_RTC::errorToString(rtc.lastError()));
  }

  if (rtc.readTemperatureC(tempC))
  {
    Serial.print("Temp C: ");
    Serial.println(tempC, 2);
  }

  delay(1000);
}
```

---

## Current backend note

Version **1.0.1** uses:

- `jwplc_i2c_bridge`

That makes it ideal for the current **JWPLC Basic** package.

A future release can keep the same API and add a second backend for:

- `Wire`

to support generic Arduino and ESP32 boards outside JWPLC.

---

## Changelog summary

### 1.0.1
- added friendlier aliases like `JWRTCDateTime`
- no backend changes
- no functional changes to RTC access logic

### 1.0.0
- initial public release
- clean JW-style API for DS3232M / DS3232
- bridge backend based on `jwplc_i2c_bridge`
- temperature, alarms, NVRAM, square-wave and 32kHz support
