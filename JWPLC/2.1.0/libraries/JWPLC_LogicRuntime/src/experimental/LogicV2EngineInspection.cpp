#include "LogicV2EnginePrototype.h"

bool LogicV2EnginePrototype::neutralInputValue(
    LogicV2BlockType consumerType) const
{
  return consumerType == LogicV2BlockType::And ||
         consumerType == LogicV2BlockType::Nand;
}

bool LogicV2EnginePrototype::resolveInputValue(
    const LogicV2InputLink &link,
    LogicV2BlockType consumerType,
    bool &value) const
{
  const uint16_t source = link.source();

  if (source == JWPLC_LOGIC_V2_SOURCE_OPEN)
  {
    value = neutralInputValue(consumerType);
  }
  else if (source == JWPLC_LOGIC_V2_SOURCE_CONST_FALSE)
  {
    value = false;
  }
  else if (source == JWPLC_LOGIC_V2_SOURCE_CONST_TRUE)
  {
    value = true;
  }
  else if (source < _program.blockCount)
  {
    value = _values[source];
  }
  else
  {
    return false;
  }

  if (link.inverted())
  {
    value = !value;
  }

  return true;
}

bool LogicV2EnginePrototype::inputValue(uint16_t blockIndex,
                                        uint8_t inputIndex) const
{
  if (!hasProgram() || blockIndex >= _program.blockCount)
  {
    return false;
  }

  const LogicV2BlockRecord &block = _blocks[blockIndex];
  if (inputIndex >= block.inputCount)
  {
    return false;
  }

  const uint32_t linkIndex =
      static_cast<uint32_t>(block.firstInput) + inputIndex;
  if (linkIndex >= _program.linkCount)
  {
    return false;
  }

  bool value = false;
  return resolveInputValue(_links[linkIndex], block.type, value)
             ? value
             : false;
}
