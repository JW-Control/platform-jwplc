#include "RuntimeUIV2ReadModel.h"

RuntimeUIV2ReadModel::RuntimeUIV2ReadModel()
    : _engine(nullptr)
{
}

void RuntimeUIV2ReadModel::attach(const LogicV2EnginePrototype &engine)
{
  _engine = &engine;
}

void RuntimeUIV2ReadModel::detach()
{
  _engine = nullptr;
}

bool RuntimeUIV2ReadModel::isAttached() const
{
  return _engine != nullptr;
}

const LogicV2EnginePrototype *RuntimeUIV2ReadModel::engine() const
{
  return _engine;
}

LogicV2EngineState RuntimeUIV2ReadModel::state() const
{
  return _engine ? _engine->state() : LogicV2EngineState::Empty;
}

uint16_t RuntimeUIV2ReadModel::blockCount() const
{
  return _engine ? _engine->blockCount() : 0;
}

uint16_t RuntimeUIV2ReadModel::linkCount() const
{
  return _engine ? _engine->linkCount() : 0;
}

const LogicV2BlockRecord *RuntimeUIV2ReadModel::block(
    uint16_t blockIndex) const
{
  return _engine ? _engine->blockDefinition(blockIndex) : nullptr;
}

const LogicV2InputLink *RuntimeUIV2ReadModel::link(
    uint16_t linkIndex) const
{
  return _engine ? _engine->inputLink(linkIndex) : nullptr;
}

const LogicV2InputLink *RuntimeUIV2ReadModel::inputLink(
    uint16_t blockIndex,
    uint8_t inputIndex) const
{
  const LogicV2BlockRecord *definition = block(blockIndex);
  if (definition == nullptr || inputIndex >= definition->inputCount)
  {
    return nullptr;
  }

  const uint32_t linkIndex =
      static_cast<uint32_t>(definition->firstInput) + inputIndex;
  if (linkIndex >= linkCount())
  {
    return nullptr;
  }

  return link(static_cast<uint16_t>(linkIndex));
}

bool RuntimeUIV2ReadModel::blockValue(uint16_t blockIndex) const
{
  return _engine ? _engine->blockValue(blockIndex) : false;
}

bool RuntimeUIV2ReadModel::neutralValue(LogicV2BlockType type) const
{
  return type == LogicV2BlockType::And ||
         type == LogicV2BlockType::Nand;
}

bool RuntimeUIV2ReadModel::resolveLinkValue(
    const LogicV2InputLink &input,
    LogicV2BlockType consumerType,
    bool &value) const
{
  const uint16_t source = input.source();

  if (source == JWPLC_LOGIC_V2_SOURCE_OPEN)
  {
    value = neutralValue(consumerType);
  }
  else if (source == JWPLC_LOGIC_V2_SOURCE_CONST_FALSE)
  {
    value = false;
  }
  else if (source == JWPLC_LOGIC_V2_SOURCE_CONST_TRUE)
  {
    value = true;
  }
  else if (source < blockCount())
  {
    value = blockValue(source);
  }
  else
  {
    return false;
  }

  if (input.inverted())
  {
    value = !value;
  }

  return true;
}

bool RuntimeUIV2ReadModel::inputValue(uint16_t blockIndex,
                                      uint8_t inputIndex) const
{
  const LogicV2BlockRecord *definition = block(blockIndex);
  const LogicV2InputLink *input = inputLink(blockIndex, inputIndex);
  if (definition == nullptr || input == nullptr)
  {
    return false;
  }

  bool value = false;
  return resolveLinkValue(*input, definition->type, value) ? value : false;
}

bool RuntimeUIV2ReadModel::isTon(uint16_t blockIndex) const
{
  const LogicV2BlockRecord *definition = block(blockIndex);
  return definition != nullptr && definition->type == LogicV2BlockType::Ton;
}

bool RuntimeUIV2ReadModel::tonTiming(uint16_t blockIndex) const
{
  return _engine && _engine->tonTiming(blockIndex);
}

uint32_t RuntimeUIV2ReadModel::tonElapsedMs(uint16_t blockIndex,
                                            uint32_t nowMs) const
{
  return _engine ? _engine->tonElapsedMs(blockIndex, nowMs) : 0;
}

uint32_t RuntimeUIV2ReadModel::tonRemainingMs(uint16_t blockIndex,
                                              uint32_t nowMs) const
{
  return _engine ? _engine->tonRemainingMs(blockIndex, nowMs) : 0;
}

uint8_t RuntimeUIV2ReadModel::collectSources(
    uint16_t blockIndex,
    uint16_t *destination,
    uint8_t destinationCapacity) const
{
  if (destination == nullptr || destinationCapacity == 0)
  {
    return 0;
  }

  const LogicV2BlockRecord *definition = block(blockIndex);
  if (definition == nullptr)
  {
    return 0;
  }

  uint8_t count = 0;
  for (uint8_t inputIndex = 0;
       inputIndex < definition->inputCount && count < destinationCapacity;
       ++inputIndex)
  {
    const LogicV2InputLink *input = inputLink(blockIndex, inputIndex);
    if (input == nullptr)
    {
      continue;
    }

    const uint16_t source = input->source();
    if (source < blockCount())
    {
      destination[count++] = source;
    }
  }

  return count;
}

bool RuntimeUIV2ReadModel::blockConsumes(uint16_t blockIndex,
                                         uint16_t sourceIndex) const
{
  const LogicV2BlockRecord *definition = block(blockIndex);
  if (definition == nullptr)
  {
    return false;
  }

  for (uint8_t inputIndex = 0;
       inputIndex < definition->inputCount;
       ++inputIndex)
  {
    const LogicV2InputLink *input = inputLink(blockIndex, inputIndex);
    if (input != nullptr && input->source() == sourceIndex)
    {
      return true;
    }
  }

  return false;
}

uint8_t RuntimeUIV2ReadModel::collectConsumers(
    uint16_t sourceIndex,
    uint16_t *destination,
    uint8_t destinationCapacity) const
{
  if (destination == nullptr || destinationCapacity == 0 ||
      sourceIndex >= blockCount())
  {
    return 0;
  }

  uint8_t count = 0;
  for (uint16_t blockIndex = static_cast<uint16_t>(sourceIndex + 1U);
       blockIndex < blockCount() && count < destinationCapacity;
       ++blockIndex)
  {
    if (blockConsumes(blockIndex, sourceIndex))
    {
      destination[count++] = blockIndex;
    }
  }

  return count;
}

const char *RuntimeUIV2ReadModel::typeName(LogicV2BlockType type) const
{
  switch (type)
  {
  case LogicV2BlockType::DigitalInput:
    return "ENTRADA DIGITAL";
  case LogicV2BlockType::DigitalOutput:
    return "SALIDA DIGITAL";
  case LogicV2BlockType::ConstantFalse:
    return "CONSTANTE LO";
  case LogicV2BlockType::ConstantTrue:
    return "CONSTANTE HI";
  case LogicV2BlockType::Not:
    return "NOT";
  case LogicV2BlockType::And:
    return "AND";
  case LogicV2BlockType::Or:
    return "OR";
  case LogicV2BlockType::Nand:
    return "NAND";
  case LogicV2BlockType::Nor:
    return "NOR";
  case LogicV2BlockType::Xor:
    return "XOR";
  case LogicV2BlockType::SetReset:
    return "SET/RESET";
  case LogicV2BlockType::Ton:
    return "TON";
  default:
    return "DESCONOCIDO";
  }
}

const char *RuntimeUIV2ReadModel::typeShort(LogicV2BlockType type) const
{
  switch (type)
  {
  case LogicV2BlockType::DigitalInput:
    return "I";
  case LogicV2BlockType::DigitalOutput:
    return "Q";
  case LogicV2BlockType::ConstantFalse:
    return "LO";
  case LogicV2BlockType::ConstantTrue:
    return "HI";
  case LogicV2BlockType::Not:
    return "NOT";
  case LogicV2BlockType::And:
    return "AND";
  case LogicV2BlockType::Or:
    return "OR";
  case LogicV2BlockType::Nand:
    return "NAND";
  case LogicV2BlockType::Nor:
    return "NOR";
  case LogicV2BlockType::Xor:
    return "XOR";
  case LogicV2BlockType::SetReset:
    return "SR";
  case LogicV2BlockType::Ton:
    return "TON";
  default:
    return "?";
  }
}

const char *RuntimeUIV2ReadModel::inputRole(LogicV2BlockType type,
                                            uint8_t inputIndex) const
{
  if (type == LogicV2BlockType::SetReset)
  {
    return inputIndex == 0 ? "S" : "R";
  }

  if (type == LogicV2BlockType::Ton)
  {
    return "Trg";
  }

  return "";
}
