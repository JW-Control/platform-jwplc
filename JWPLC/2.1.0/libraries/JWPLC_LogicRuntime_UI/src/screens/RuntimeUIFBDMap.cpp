#include "RuntimeUIFBDMap.h"

RuntimeUIFBDMap::RuntimeUIFBDMap()
    : RuntimeUIFBDMapV14(),
      _appliedRefreshPeriodMs(0)
{
}

void RuntimeUIFBDMap::syncRefreshPeriod()
{
  const uint32_t desired = recommendedRefreshPeriodMs();
  if (_appliedRefreshPeriodMs == desired)
  {
    return;
  }

  JWPLC_Display.setUserRefreshPeriodMs(desired);
  _appliedRefreshPeriodMs = desired;
}

void RuntimeUIFBDMap::enter()
{
  RuntimeUIFBDMapV14::enter();
  _appliedRefreshPeriodMs = 0;
  syncRefreshPeriod();
}

void RuntimeUIFBDMap::refresh(const JWPLC_IOState *io,
                              const JWPLC_RTCState *rtc)
{
  RuntimeUIFBDMapV14::refresh(io, rtc);
  syncRefreshPeriod();
}

void RuntimeUIFBDMap::forceRedraw()
{
  RuntimeUIFBDMapV14::forceRedraw();
  _appliedRefreshPeriodMs = 0;
  syncRefreshPeriod();
}
