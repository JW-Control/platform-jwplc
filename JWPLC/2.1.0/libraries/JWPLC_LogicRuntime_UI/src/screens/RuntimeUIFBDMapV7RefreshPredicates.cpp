#include "RuntimeUIFBDMapV7.h"

bool RuntimeUIFBDMapV7::mapNeedsTftRefreshForExtensionV7() const
{
  if (_model == nullptr ||
      !_model->isAttached() ||
      _mode != Mode::Map)
  {
    return false;
  }

  if (_fullRedraw ||
      !_layoutValid ||
      !_valueCacheValid ||
      !_mapSelectionCacheValid ||
      !_headerStateValid ||
      _layoutBlockCount != _model->blockCount() ||
      _layoutLinkCount != _model->linkCount() ||
      _lastHeaderState != _model->state() ||
      _mapSelectionCache != _selectedIndex)
  {
    return true;
  }

  const uint16_t count = _model->blockCount();
  for (uint16_t block = 0; block < count; ++block)
  {
    if (_valueCache[block] != _model->blockValue(block))
    {
      return true;
    }
  }

  return false;
}

bool RuntimeUIFBDMapV7::detailNeedsTftRefreshForExtensionV7() const
{
  if (_model == nullptr ||
      !_model->isAttached() ||
      _mode != Mode::Detail ||
      _parameterEditorActive)
  {
    return false;
  }

  if (_fullRedraw ||
      !_layoutValid ||
      !_detailCacheValid ||
      !_headerStateValid ||
      _layoutBlockCount != _model->blockCount() ||
      _layoutLinkCount != _model->linkCount() ||
      _lastHeaderState != _model->state() ||
      _detailCacheBlock != _selectedIndex ||
      _detailCachePage != detailPageStart())
  {
    return true;
  }

  const LogicV2BlockRecord *definition = _model->block(_selectedIndex);
  if (definition == nullptr)
  {
    return false;
  }

  if (_detailCacheBlockValue != _model->blockValue(_selectedIndex))
  {
    return true;
  }

  const uint8_t page = detailPageStart();
  for (uint8_t row = 0; row < DETAIL_INPUTS_PER_PAGE; ++row)
  {
    const uint8_t input = static_cast<uint8_t>(page + row);
    if (input >= definition->inputCount)
    {
      continue;
    }

    if (_detailCacheInputs[row] != _model->inputValue(_selectedIndex, input))
    {
      return true;
    }
  }

  if (definition->type == LogicV2BlockType::Ton)
  {
    const bool timing = _model->tonTiming(_selectedIndex);
    const bool active = _model->blockValue(_selectedIndex);

    // Mientras cuenta, Ta necesita una actualización periódica. Cuando queda
    // inactivo o completado, la caché puede mantener la pantalla completamente
    // quieta hasta que cambie otro estado visible.
    if (timing ||
        timing != _detailCacheTonTiming ||
        active != _detailCacheTonActive)
    {
      return true;
    }
  }

  return false;
}
