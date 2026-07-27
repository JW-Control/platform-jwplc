#include "RuntimeUIFBDMap.h"

extern "C" void jwplcUnifiedWizardLinkAnchor();
extern "C" void jwplcUnifiedU4HeaderAnchor();
extern "C" void jwplcUnifiedU4MapAnchor();
extern "C" void jwplcUnifiedU4InputAnchor();
extern "C" void jwplcUnifiedU4ResetForAttach(
    const RuntimeUIFBDMapUnified *owner,
    RuntimeUIV2ReadModel *model);

RuntimeUIFBDMap::RuntimeUIFBDMap()
    : _unifiedPreview(false),
      _appliedRefreshPeriodMs(0),
      _legacy(),
      _unified()
{
  // Fuerza la extracción de todos los objetos U4 desde el archive Arduino. Las
  // funciones activas de Unified sustituyen símbolos débiles del núcleo base.
  jwplcUnifiedWizardLinkAnchor();
  jwplcUnifiedU4HeaderAnchor();
  jwplcUnifiedU4MapAnchor();
  jwplcUnifiedU4InputAnchor();
}

void RuntimeUIFBDMap::setUnifiedPreview(bool enabled)
{
  _unifiedPreview = enabled;
  _appliedRefreshPeriodMs = 0;
}

bool RuntimeUIFBDMap::unifiedPreviewEnabled() const
{
  return _unifiedPreview;
}

void RuntimeUIFBDMap::attach(RuntimeUIV2ReadModel &model)
{
  if (_unifiedPreview)
  {
    _legacy.detach();
    jwplcUnifiedU4ResetForAttach(&_unified, &model);
    _unified.attach(model);
  }
  else
  {
    _unified.detach();
    jwplcUnifiedU4ResetForAttach(&_unified, nullptr);
    _legacy.attach(model);
  }
  _appliedRefreshPeriodMs = 0;
}

void RuntimeUIFBDMap::detach()
{
  _legacy.detach();
  _unified.detach();
  jwplcUnifiedU4ResetForAttach(&_unified, nullptr);
  _unifiedPreview = false;
  _appliedRefreshPeriodMs = 0;
}

uint32_t RuntimeUIFBDMap::desiredRefreshPeriodMs() const
{
  return _unifiedPreview
             ? _unified.requestedRefreshPeriodMs()
             : _legacy.recommendedRefreshPeriodMs();
}

void RuntimeUIFBDMap::syncRefreshPeriod()
{
  const uint32_t desired = desiredRefreshPeriodMs();
  if (_appliedRefreshPeriodMs == desired)
  {
    return;
  }

  JWPLC_Display.setUserRefreshPeriodMs(desired);
  _appliedRefreshPeriodMs = desired;
}

bool RuntimeUIFBDMap::needsTftRefresh() const
{
  return _unifiedPreview
             ? _unified.needsTftRefresh()
             : true;
}

void RuntimeUIFBDMap::enter()
{
  if (_unifiedPreview)
  {
    _unified.enter();
  }
  else
  {
    _legacy.enter();
  }

  _appliedRefreshPeriodMs = 0;
  syncRefreshPeriod();
}

void RuntimeUIFBDMap::refresh(const JWPLC_IOState *io,
                              const JWPLC_RTCState *rtc)
{
  if (_unifiedPreview)
  {
    _unified.refresh(io, rtc);
  }
  else
  {
    _legacy.refresh(io, rtc);
  }
  syncRefreshPeriod();
}

void RuntimeUIFBDMap::exit()
{
  if (_unifiedPreview)
  {
    _unified.exit();
  }
  else
  {
    _legacy.exit();
  }
}

void RuntimeUIFBDMap::forceRedraw()
{
  if (_unifiedPreview)
  {
    _unified.forceRedraw();
  }
  else
  {
    _legacy.forceRedraw();
  }

  _appliedRefreshPeriodMs = 0;
  syncRefreshPeriod();
}

void RuntimeUIFBDMap::processPendingEdit()
{
  if (_unifiedPreview)
  {
    _unified.processPendingEdit();
  }
  else
  {
    _legacy.processPendingEdit();
  }
}
