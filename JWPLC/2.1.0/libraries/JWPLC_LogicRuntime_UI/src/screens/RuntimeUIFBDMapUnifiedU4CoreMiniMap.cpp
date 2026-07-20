#define JWPLC_UNIFIED_STRONG_IMPLEMENTATION
#include "RuntimeUIFBDMapUnifiedU4Internal.h"

#include <cstdio>
#include <cstring>

#include "../widgets/RuntimeUIWidgets.h"

using namespace JWPLCLogicRuntimeUIWidgets;

namespace JWPLCUnifiedU4
{
void drawMiniMap(const RuntimeUIV2ReadModel *model,
                 uint16_t selectedSource,
                 const uint8_t *levels,
                 const uint8_t *lanes,
                 uint8_t maxLevel,
                 int16_t x,
                 int16_t y,
                 int16_t width,
                 int16_t height)
{
  const uint16_t count = model != nullptr ? model->blockCount() : 0;
  if (count == 0 || selectedSource >= count || width < 20 || height < 8)
    return;

  uint8_t maxLane = 0;
  for (uint16_t block = 0; block < count; ++block)
    if (lanes[block] > maxLane) maxLane = lanes[block];

  uint8_t visibleColumns = static_cast<uint8_t>(width / 34);
  if (visibleColumns < 1) visibleColumns = 1;
  if (visibleColumns > 6) visibleColumns = 6;
  uint8_t visibleRows = static_cast<uint8_t>(height / 9);
  if (visibleRows < 1) visibleRows = 1;
  if (visibleRows > 4) visibleRows = 4;

  const uint8_t totalColumns = static_cast<uint8_t>(maxLevel + 1U);
  const uint8_t totalRows = static_cast<uint8_t>(maxLane + 1U);
  if (visibleColumns > totalColumns) visibleColumns = totalColumns;
  if (visibleRows > totalRows) visibleRows = totalRows;

  uint8_t firstColumn = levels[selectedSource] > visibleColumns / 2U
                            ? static_cast<uint8_t>(levels[selectedSource] - visibleColumns / 2U)
                            : 0;
  if (static_cast<uint16_t>(firstColumn) + visibleColumns > totalColumns)
    firstColumn = static_cast<uint8_t>(totalColumns - visibleColumns);

  uint8_t firstRow = lanes[selectedSource] > visibleRows / 2U
                         ? static_cast<uint8_t>(lanes[selectedSource] - visibleRows / 2U)
                         : 0;
  if (static_cast<uint16_t>(firstRow) + visibleRows > totalRows)
    firstRow = static_cast<uint8_t>(totalRows - visibleRows);

  const int16_t cellW = static_cast<int16_t>(width / visibleColumns);
  const int16_t cellH = static_cast<int16_t>(height / visibleRows);
  int16_t miniW = static_cast<int16_t>(cellW - 5);
  int16_t miniH = static_cast<int16_t>(cellH - 3);
  if (miniW > 18) miniW = 18;
  if (miniH > 8) miniH = 8;
  if (miniW < 5) miniW = 5;
  if (miniH < 4) miniH = 4;

  const auto visible = [&](uint16_t block) -> bool
  {
    return levels[block] >= firstColumn &&
           levels[block] < static_cast<uint8_t>(firstColumn + visibleColumns) &&
           lanes[block] >= firstRow &&
           lanes[block] < static_cast<uint8_t>(firstRow + visibleRows);
  };
  const auto bx = [&](uint16_t block) -> int16_t
  {
    return static_cast<int16_t>(
        x + (levels[block] - firstColumn) * cellW + (cellW - miniW) / 2);
  };
  const auto by = [&](uint16_t block) -> int16_t
  {
    return static_cast<int16_t>(
        y + (lanes[block] - firstRow) * cellH + (cellH - miniH) / 2);
  };

  Adafruit_ST7789 &tft = JWPLC_Display.tft();
  for (uint16_t consumer = 0; consumer < count; ++consumer)
  {
    if (!visible(consumer)) continue;
    const LogicV2BlockRecord *definition = model->block(consumer);
    if (definition == nullptr) continue;
    for (uint8_t input = 0; input < definition->inputCount; ++input)
    {
      const LogicV2InputLink *link = model->inputLink(consumer, input);
      if (link == nullptr) continue;
      const uint16_t source = link->source();
      if (source >= count || !visible(source)) continue;
      tft.drawLine(static_cast<int16_t>(bx(source) + miniW),
                   static_cast<int16_t>(by(source) + miniH / 2),
                   bx(consumer),
                   static_cast<int16_t>(by(consumer) + miniH / 2),
                   COLOR_MUTED);
    }
  }

  for (uint16_t block = 0; block < count; ++block)
  {
    if (!visible(block)) continue;
    const bool selected = block == selectedSource;
    const bool active = model->blockValue(block);
    const uint16_t border = selected ? COLOR_WARNING : (active ? COLOR_OK : COLOR_BORDER);
    tft.fillRect(bx(block), by(block), miniW, miniH, COLOR_BACKGROUND);
    tft.drawRect(bx(block), by(block), miniW, miniH, border);
  }
}

} // namespace JWPLCUnifiedU4
