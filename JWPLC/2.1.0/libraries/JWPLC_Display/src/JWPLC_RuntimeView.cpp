#include "JWPLC_RuntimeView.h"

JWPLC_IOView JWPLC_IO;
JWPLC_TimeView JWPLC_Time;

uint8_t JWPLC_IOView::inputs() const
{
    const JWPLC_IOState *state = jwplcGetIOState();
    return (state != nullptr) ? state->di_logical_bank0 : 0;
}

uint8_t JWPLC_IOView::outputs() const
{
    const JWPLC_IOState *state = jwplcGetIOState();
    return (state != nullptr) ? state->do_bank1 : 0;
}

bool JWPLC_IOView::input(uint8_t index) const
{
    if (index >= 8)
    {
        return false;
    }

    return (inputs() & (uint8_t)(1u << index)) != 0;
}

bool JWPLC_IOView::output(uint8_t index) const
{
    if (index >= 8)
    {
        return false;
    }

    return (outputs() & (uint8_t)(1u << index)) != 0;
}

bool JWPLC_IOView::ready() const
{
    const JWPLC_IOState *state = jwplcGetIOState();
    return state != nullptr && state->initialized;
}

uint32_t JWPLC_IOView::lastScanMs() const
{
    const JWPLC_IOState *state = jwplcGetIOState();
    return (state != nullptr) ? state->last_scan_ms : 0;
}

bool JWPLC_TimeView::present() const
{
    const JWPLC_RTCState *state = jwplcGetRTCState();
    return state != nullptr && state->present;
}

bool JWPLC_TimeView::valid() const
{
    const JWPLC_RTCState *state = jwplcGetRTCState();
    return state != nullptr && state->valid;
}

bool JWPLC_TimeView::lostPower() const
{
    const JWPLC_RTCState *state = jwplcGetRTCState();
    return state != nullptr && state->lost_power;
}

uint8_t JWPLC_TimeView::second() const
{
    const JWPLC_RTCState *state = jwplcGetRTCState();
    return (state != nullptr) ? state->second : 0;
}

uint8_t JWPLC_TimeView::minute() const
{
    const JWPLC_RTCState *state = jwplcGetRTCState();
    return (state != nullptr) ? state->minute : 0;
}

uint8_t JWPLC_TimeView::hour() const
{
    const JWPLC_RTCState *state = jwplcGetRTCState();
    return (state != nullptr) ? state->hour : 0;
}

uint8_t JWPLC_TimeView::day() const
{
    const JWPLC_RTCState *state = jwplcGetRTCState();
    return (state != nullptr) ? state->day : 0;
}

uint8_t JWPLC_TimeView::month() const
{
    const JWPLC_RTCState *state = jwplcGetRTCState();
    return (state != nullptr) ? state->month : 0;
}

uint16_t JWPLC_TimeView::year() const
{
    const JWPLC_RTCState *state = jwplcGetRTCState();
    return (state != nullptr) ? state->year : 0;
}

uint8_t JWPLC_TimeView::dayOfWeek() const
{
    const JWPLC_RTCState *state = jwplcGetRTCState();
    return (state != nullptr) ? state->day_of_week : 0;
}

uint32_t JWPLC_TimeView::lastUpdateMs() const
{
    const JWPLC_RTCState *state = jwplcGetRTCState();
    return (state != nullptr) ? state->last_update_ms : 0;
}