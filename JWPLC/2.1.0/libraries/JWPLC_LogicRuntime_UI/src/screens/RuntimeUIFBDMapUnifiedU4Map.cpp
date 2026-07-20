#define JWPLC_UNIFIED_STRONG_IMPLEMENTATION
#include "RuntimeUIFBDMapUnifiedU4Internal.h"

#include <cstdio>
#include <cstring>

#include "../widgets/RuntimeUIWidgets.h"

using namespace JWPLCLogicRuntimeUIWidgets;
using namespace JWPLCUnifiedU4;

extern "C" void jwplcUnifiedU4MapAnchor() {}

void RuntimeUIFBDMapUnified::handleMapInputStable()
{
  ensureU4(this, _model);
  if (_model == nullptr)
    return;

  if (gU4.addSelected)
  {
    const bool left = JWPLC_Buttons.pressed(BTN_LEFT);
    const bool escape = !left && JWPLC_Buttons.pressed(BTN_ESC);
    if (left || escape)
    {
      gU4.addSelected = false;
      _selectedIndex = gU4.addOrigin < _model->blockCount()
                           ? gU4.addOrigin
                           : static_cast<uint16_t>(_model->blockCount() - 1U);
      gU4.previewDrawn = false;
      invalidateMapCache();
      _contentDirty = true;
      _headerDirty = true;
      JWPLC_Display.notifyActivity();
      gateInputUntilRelease();
      return;
    }

    if (JWPLC_Buttons.pressed(BTN_OK))
    {
      gU4.type = U4Type::DigitalInput;
      gU4.error = false;
      transitionTo(View::AddType);
      JWPLC_Display.notifyActivity();
    }
    return;
  }

  const uint16_t previousSelection = _selectedIndex;
  bool changed = false;
  const bool left = JWPLC_Buttons.pressed(BTN_LEFT);
  const bool right = !left && JWPLC_Buttons.pressed(BTN_RIGHT);
  const bool up = !left && !right && JWPLC_Buttons.pressed(BTN_UP);
  const bool down = !left && !right && !up && JWPLC_Buttons.pressed(BTN_DOWN);

  if (left)
    changed = selectSource();
  else if (right)
  {
    changed = selectConsumer();
    const bool canAdd = _model->blockCount() > 0 &&
                        _model->blockCount() < MAX_BLOCKS &&
                        _maxLevel < MAX_LEVELS - 1U;
    if (!changed && canAdd && _levels[_selectedIndex] == _maxLevel)
    {
      gU4.addOrigin = _selectedIndex;
      gU4.addSelected = true;
      gU4.previewDrawn = false;
      invalidateMapCache();
      _contentDirty = true;
      _headerDirty = true;
      JWPLC_Display.notifyActivity();
      gateInputUntilRelease();
      return;
    }
  }
  else if (up)
    changed = selectVertical(false);
  else if (down)
    changed = selectVertical(true);

  if (changed)
  {
    JWPLC_Display.notifyActivity();
    const bool viewportChanged = ensureSelectionVisible();
    _headerDirty = true;
    if (viewportChanged)
    {
      gU4.previewDrawn = false;
      _contentDirty = true;
      return;
    }

    drawMapSelectionFrame(previousSelection);
    drawMapSelectionFrame(_selectedIndex);
    _mapSelectionCache = _selectedIndex;
    _mapSelectionCacheValid = true;
  }

  if (JWPLC_Buttons.pressed(BTN_OK) && _model->blockCount() > 0)
  {
    JWPLC_Display.notifyActivity();
    transitionTo(View::Detail);
  }
}

void RuntimeUIFBDMapUnified::renderMap(bool force)
{
  ensureU4(this, _model);
  if (_model == nullptr)
    return;

  const bool canAdd = _model->blockCount() > 0 &&
                      _model->blockCount() < MAX_BLOCKS &&
                      _maxLevel < MAX_LEVELS - 1U;

  if (gU4.addSelected)
  {
    bool valueChanged = force || !_mapValueCacheValid || !_mapSelectionCacheValid;
    if (!valueChanged)
    {
      for (uint16_t block = 0; block < _model->blockCount(); ++block)
      {
        if (_mapValueCache[block] != _model->blockValue(block))
        {
          valueChanged = true;
          break;
        }
      }
    }
    if (!valueChanged)
      return;

    const uint16_t count = _model->blockCount();
    const uint16_t savedSelection = _selectedIndex;
    const uint8_t savedMax = _maxLevel;
    const HorizontalWindowMode savedMode = _horizontalMode;
    const uint8_t savedStart = _centralStartLevel;

    const uint8_t virtualLevel = static_cast<uint8_t>(savedMax + 1U);
    _maxLevel = virtualLevel;
    _horizontalMode = virtualLevel <= 4
                          ? HorizontalWindowMode::LeftEdge
                          : HorizontalWindowMode::RightEdge;
    _centralStartLevel = virtualLevel <= 4
                             ? 1
                             : static_cast<uint8_t>(virtualLevel - 3U);
    _selectedIndex = count;

    clearContentArea();
    drawWires();
    drawNodes();
    drawEdgeHints();

    const int8_t slot = slotForLevel(virtualLevel);
    if (slot >= 0 && slot < static_cast<int8_t>(SLOT_COUNT))
    {
      int16_t y = static_cast<int16_t>(MAP_Y + (MAP_H - NODE_H) / 2);
      if (gU4.addOrigin < count)
        y = static_cast<int16_t>(MAP_Y + _nodeY[gU4.addOrigin] - _viewportY);
      if (y < MAP_Y + 2) y = MAP_Y + 2;
      if (y > MAP_Y + MAP_H - NODE_H - 2)
        y = static_cast<int16_t>(MAP_Y + MAP_H - NODE_H - 2);
      const int16_t x = static_cast<int16_t>(SLOT_X0 + slot * SLOT_STEP);
      Adafruit_ST7789 &tft = JWPLC_Display.tft();
      tft.fillRect(x, y, NODE_W, NODE_H, COLOR_BACKGROUND);
      tft.drawRect(x, y, NODE_W, NODE_H, COLOR_WARNING);
      tft.drawRect(x + 1, y + 1, NODE_W - 2, NODE_H - 2, COLOR_WARNING);
      tft.setTextWrap(false);
      tft.setTextSize(3);
      tft.setTextColor(COLOR_WARNING, COLOR_BACKGROUND);
      tft.setCursor(x + 15, y + 3);
      tft.print('+');
    }

    for (uint16_t block = 0; block < count; ++block)
      _mapValueCache[block] = _model->blockValue(block);
    _mapValueCacheValid = true;
    _mapSelectionCache = count;
    _mapSelectionCacheValid = true;

    _selectedIndex = savedSelection;
    _maxLevel = savedMax;
    _horizontalMode = savedMode;
    _centralStartLevel = savedStart;
    return;
  }

  if (force)
  {
    clearContentArea();
    drawMapFull();
    gU4.previewDrawn = false;
  }
  else
  {
    drawMapLive();
  }

  if (!gU4.previewDrawn && canAdd && _maxLevel <= 3)
  {
    uint16_t origin = _selectedIndex;
    if (_levels[origin] != _maxLevel)
    {
      for (uint16_t block = 0; block < _model->blockCount(); ++block)
      {
        if (_levels[block] == _maxLevel)
        {
          origin = block;
          break;
        }
      }
    }

    const int16_t x = static_cast<int16_t>(
        SLOT_X0 + static_cast<int16_t>(_maxLevel + 1U) * SLOT_STEP + 10);
    int16_t y = static_cast<int16_t>(screenY(origin) + 3);
    if (y < MAP_Y + 2) y = MAP_Y + 2;
    if (y > MAP_Y + MAP_H - 26)
      y = static_cast<int16_t>(MAP_Y + MAP_H - 26);

    Adafruit_ST7789 &tft = JWPLC_Display.tft();
    tft.fillRect(x, y, 28, 24, COLOR_BACKGROUND);
    tft.drawRect(x, y, 28, 24, COLOR_BORDER);
    tft.setTextWrap(false);
    tft.setTextSize(2);
    tft.setTextColor(COLOR_ACCENT, COLOR_BACKGROUND);
    tft.setCursor(x + 8, y + 4);
    tft.print('+');
    gU4.previewDrawn = true;
  }
}
