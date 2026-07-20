#include "RuntimeUIFBDMapUnified.h"

#include "../widgets/RuntimeUIWidgets.h"

using namespace JWPLCLogicRuntimeUIWidgets;

/**
 * @brief Compatibilidad temporal con la llamada existente del núcleo Unified.
 *
 * El marco T-only ya nace correctamente desde drawTonPanelFrame() mediante
 * TON_PANEL_H=14. No debe existir una segunda pasada que borre y redibuje bordes,
 * porque produciría el frame intermedio T/Ta observado físicamente.
 */
void jwplcNormalizeUnifiedTonFrame(bool selected)
{
  (void)selected;
}

void RuntimeUIFBDMapUnified::handleMapInputStable()
{
  if (_model == nullptr)
  {
    return;
  }

  const uint16_t previousSelection = _selectedIndex;
  bool changed = false;

  if (JWPLC_Buttons.pressed(BTN_LEFT))
  {
    changed = selectSource();
  }
  else if (JWPLC_Buttons.pressed(BTN_RIGHT))
  {
    changed = selectConsumer();
  }
  else if (JWPLC_Buttons.pressed(BTN_UP))
  {
    changed = selectVertical(false);
  }
  else if (JWPLC_Buttons.pressed(BTN_DOWN))
  {
    changed = selectVertical(true);
  }

  if (changed)
  {
    JWPLC_Display.notifyActivity();
    const bool viewportChanged = ensureSelectionVisible();
    _headerDirty = true;

    if (viewportChanged)
    {
      // Cuando cambia la ventana, las coordenadas de varios nodos cambian y sí
      // corresponde reconstruir una sola vez el contenido completo.
      _contentDirty = true;
      return;
    }

    // Si la ventana no cambia, la selección solo modifica los dos marcos. No se
    // borra el interior del bloque, por lo que textos y bloques apagados no
    // desaparecen durante unos milisegundos al pulsar la botonera.
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

void RuntimeUIFBDMapUnified::drawMapSelectionFrame(uint16_t blockIndex)
{
  if (_model == nullptr || blockIndex >= _model->blockCount())
  {
    return;
  }

  const bool active = _model->blockValue(blockIndex);
  const bool selected = blockIndex == _selectedIndex;
  const uint16_t border =
      selected ? COLOR_WARNING : (active ? COLOR_OK : COLOR_BORDER);
  Adafruit_ST7789 &tft = JWPLC_Display.tft();

  if (isFullLevel(_levels[blockIndex]))
  {
    const int16_t x = screenX(blockIndex);
    const int16_t y = screenY(blockIndex);
    if (!nodeFullyVisible(x, y))
    {
      return;
    }

    tft.drawRect(x, y, NODE_W, NODE_H, border);
    tft.drawRect(x + 1,
                 y + 1,
                 NODE_W - 2,
                 NODE_H - 2,
                 selected ? COLOR_WARNING : COLOR_BACKGROUND);
    return;
  }

  const GridRange range = visibleGridRange();
  bool leftSide = false;
  if (!shouldDrawEdgeHint(blockIndex, range, leftSide))
  {
    return;
  }

  const int16_t hintX = screenX(blockIndex);
  const int16_t hintY = static_cast<int16_t>(
      screenY(blockIndex) + EDGE_HINT_Y_OFFSET);
  tft.drawRect(hintX, hintY, EDGE_HINT_W, EDGE_HINT_H, border);
}
