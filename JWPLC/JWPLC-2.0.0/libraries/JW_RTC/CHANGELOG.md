# Changelog

## 1.0.1
- Added friendlier public aliases:
  - `JWRTCDateTime`
  - `JWRTCAlarm1Config`
  - `JWRTCAlarm2Config`
  - `JWRTCError`
  - `JWRTCSquareWaveMode`
  - `JWRTCAlarm1Mode`
  - `JWRTCAlarm2Mode`
- No backend changes
- No functional changes to RTC access logic

## 1.0.0
- Initial public release
- Clean JW-style API for DS3232M / DS3232
- Bridge backend based on `jwplc_i2c_bridge`
- DateTime read/write
- Unix conversion
- Temperature read
- Aging offset read/write
- Control/status helpers
- Alarm 1 / Alarm 2 support
- 32kHz and square-wave helpers
- Battery-backed NVRAM read/write
