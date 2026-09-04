#ifndef JWPLC_RUNTIME_VIEW_H
#define JWPLC_RUNTIME_VIEW_H

#include <Arduino.h>

// Fachadas livianas sobre snapshots ya mantenidos por el runtime.
// No realizan transacciones I2C/SPI adicionales.
class JWPLC_IOView
{
public:
    uint8_t inputs() const;
    uint8_t outputs() const;

    bool input(uint8_t index) const;
    bool output(uint8_t index) const;

    bool ready() const;
    uint32_t lastScanMs() const;
};

class JWPLC_TimeView
{
public:
    bool present() const;
    bool valid() const;
    bool lostPower() const;

    uint8_t second() const;
    uint8_t minute() const;
    uint8_t hour() const;

    uint8_t day() const;
    uint8_t month() const;
    uint16_t year() const;
    uint8_t dayOfWeek() const;

    uint32_t lastUpdateMs() const;
};

extern JWPLC_IOView JWPLC_IO;
extern JWPLC_TimeView JWPLC_Time;

#endif // JWPLC_RUNTIME_VIEW_H
