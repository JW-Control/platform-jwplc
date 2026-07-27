#include "RuntimeUIV2EditSession.h"

#include <cstring>

RuntimeUIV2EditSession::RuntimeUIV2EditSession()
    : _engine(nullptr),
      _active(false),
      _dirty(false),
      _blockCount(0),
      _linkCount(0),
      _digitalInputCount(0),
      _digitalOutputCount(0),
      _blocks{},
      _links{}
{
}

void RuntimeUIV2EditSession::attach(LogicV2EnginePrototype &engine)
{
  _engine = &engine;
  clearDraft();
}

void RuntimeUIV2EditSession::detach()
{
  _engine = nullptr;
  clearDraft();
}

bool RuntimeUIV2EditSession::isAttached() const
{
  return _engine != nullptr;
}

bool RuntimeUIV2EditSession::begin()
{
  if (_engine == nullptr || !_engine->hasProgram())
  {
    return false;
  }

  const uint16_t blocks = _engine->blockCount();
  const uint16_t links = _engine->linkCount();
  if (blocks > JWPLC_LOGIC_V2_COMPILED_MAX_BLOCKS ||
      links > JWPLC_LOGIC_V2_COMPILED_MAX_LINKS)
  {
    return false;
  }

  clearDraft();
  _blockCount = blocks;
  _linkCount = links;
  _digitalInputCount = _engine->digitalInputCount();
  _digitalOutputCount = _engine->digitalOutputCount();

  for (uint16_t index = 0; index < _blockCount; ++index)
  {
    const LogicV2BlockRecord *definition = _engine->blockDefinition(index);
    if (definition == nullptr)
    {
      clearDraft();
      return false;
    }
    _blocks[index] = *definition;
  }

  for (uint16_t index = 0; index < _linkCount; ++index)
  {
    const LogicV2InputLink *link = _engine->inputLink(index);
    if (link == nullptr)
    {
      clearDraft();
      return false;
    }
    _links[index] = *link;
  }

  _active = true;
  _dirty = false;
  return true;
}

void RuntimeUIV2EditSession::cancel()
{
  clearDraft();
}

bool RuntimeUIV2EditSession::active() const
{
  return _active;
}

bool RuntimeUIV2EditSession::dirty() const
{
  return _dirty;
}

uint16_t RuntimeUIV2EditSession::blockCount() const
{
  return _active ? _blockCount : 0;
}

uint16_t RuntimeUIV2EditSession::linkCount() const
{
  return _active ? _linkCount : 0;
}

const LogicV2BlockRecord *RuntimeUIV2EditSession::block(uint16_t blockIndex) const
{
  if (!_active || blockIndex >= _blockCount)
  {
    return nullptr;
  }
  return &_blocks[blockIndex];
}

const LogicV2InputLink *RuntimeUIV2EditSession::inputLink(
    uint16_t blockIndex,
    uint8_t inputIndex) const
{
  const uint16_t offset = inputOffset(blockIndex, inputIndex);
  if (!_active || offset >= _linkCount)
  {
    return nullptr;
  }
  return &_links[offset];
}

bool RuntimeUIV2EditSession::setInputSource(uint16_t blockIndex,
                                            uint8_t inputIndex,
                                            uint16_t source,
                                            bool inverted)
{
  const uint16_t offset = inputOffset(blockIndex, inputIndex);
  if (!_active || offset >= _linkCount)
  {
    return false;
  }

  const bool isSpecial = source == JWPLC_LOGIC_V2_SOURCE_OPEN ||
                         source == JWPLC_LOGIC_V2_SOURCE_CONST_TRUE ||
                         source == JWPLC_LOGIC_V2_SOURCE_CONST_FALSE;
  if (!isSpecial &&
      (source > JWPLC_LOGIC_V2_MAX_BLOCK_SOURCE || source >= blockIndex))
  {
    return false;
  }

  _links[offset] = makeLink(source, inverted);
  _dirty = true;
  return true;
}

bool RuntimeUIV2EditSession::setInputInverted(uint16_t blockIndex,
                                              uint8_t inputIndex,
                                              bool inverted)
{
  const LogicV2InputLink *current = inputLink(blockIndex, inputIndex);
  if (current == nullptr)
  {
    return false;
  }
  return setInputSource(blockIndex,
                        inputIndex,
                        current->source(),
                        inverted);
}

bool RuntimeUIV2EditSession::setBlockParameter(uint16_t blockIndex,
                                               uint32_t parameter)
{
  if (!_active || blockIndex >= _blockCount)
  {
    return false;
  }
  _blocks[blockIndex].parameter = parameter;
  _dirty = true;
  return true;
}

bool RuntimeUIV2EditSession::setBlockResource(uint16_t blockIndex,
                                              uint16_t resource)
{
  if (!_active || blockIndex >= _blockCount)
  {
    return false;
  }
  _blocks[blockIndex].resource = resource;
  _dirty = true;
  return true;
}

bool RuntimeUIV2EditSession::appendBlock(
    LogicV2BlockType type,
    const LogicV2InputLink *inputs,
    uint8_t inputCount,
    uint16_t resource,
    uint32_t parameter,
    uint16_t *newBlockIndex)
{
  if (!_active ||
      validate() != LogicV2PrototypeError::None ||
      _blockCount >= JWPLC_LOGIC_V2_COMPILED_MAX_BLOCKS ||
      inputCount > JWPLC_LOGIC_V2_MAX_INPUTS_PER_BLOCK ||
      static_cast<uint32_t>(_linkCount) + inputCount >
          JWPLC_LOGIC_V2_COMPILED_MAX_LINKS ||
      (inputCount > 0 && inputs == nullptr))
  {
    return false;
  }

  const uint16_t oldBlockCount = _blockCount;
  const uint16_t oldLinkCount = _linkCount;
  const bool oldDirty = _dirty;
  const uint16_t appendedIndex = oldBlockCount;

  _blocks[appendedIndex] = LogicV2BlockRecord(
      type,
      oldLinkCount,
      inputCount,
      resource,
      parameter,
      0);

  for (uint8_t input = 0; input < inputCount; ++input)
  {
    _links[oldLinkCount + input] = inputs[input];
  }

  _blockCount = static_cast<uint16_t>(oldBlockCount + 1U);
  _linkCount = static_cast<uint16_t>(oldLinkCount + inputCount);

  if (validate() != LogicV2PrototypeError::None)
  {
    _blocks[appendedIndex] = LogicV2BlockRecord();
    for (uint16_t link = oldLinkCount; link < _linkCount; ++link)
    {
      _links[link] = LogicV2InputLink();
    }
    _blockCount = oldBlockCount;
    _linkCount = oldLinkCount;
    _dirty = oldDirty;
    return false;
  }

  _dirty = true;
  if (newBlockIndex != nullptr)
  {
    *newBlockIndex = appendedIndex;
  }
  return true;
}

uint16_t RuntimeUIV2EditSession::consumerCount(uint16_t blockIndex) const
{
  if (!_active || blockIndex >= _blockCount)
  {
    return 0;
  }

  uint16_t consumers = 0;
  for (uint16_t link = 0; link < _linkCount; ++link)
  {
    if (_links[link].source() == blockIndex)
    {
      ++consumers;
    }
  }
  return consumers;
}

bool RuntimeUIV2EditSession::hasConsumers(uint16_t blockIndex) const
{
  return consumerCount(blockIndex) > 0;
}

bool RuntimeUIV2EditSession::removeBlock(uint16_t blockIndex)
{
  if (!_active ||
      blockIndex >= _blockCount ||
      _blockCount <= 1 ||
      hasConsumers(blockIndex) ||
      validate() != LogicV2PrototypeError::None)
  {
    return false;
  }

  // La eliminación debe ser completamente transaccional. El respaldo temporal
  // ocupa ~2.2 KiB con la configuración normal de 100 bloques/512 enlaces y
  // solo existe durante esta operación estructural, nunca durante cada scan.
  LogicV2BlockRecord blockBackup[JWPLC_LOGIC_V2_COMPILED_MAX_BLOCKS];
  LogicV2InputLink linkBackup[JWPLC_LOGIC_V2_COMPILED_MAX_LINKS];
  std::memcpy(blockBackup, _blocks, sizeof(_blocks));
  std::memcpy(linkBackup, _links, sizeof(_links));

  const uint16_t oldBlockCount = _blockCount;
  const uint16_t oldLinkCount = _linkCount;
  const bool oldDirty = _dirty;
  const uint16_t removedFirstInput = _blocks[blockIndex].firstInput;
  const uint8_t removedInputCount = _blocks[blockIndex].inputCount;
  const uint16_t removedEndInput = static_cast<uint16_t>(
      removedFirstInput + removedInputCount);

  if (removedInputCount > 0)
  {
    const uint16_t linksAfter = static_cast<uint16_t>(
        oldLinkCount - removedEndInput);
    if (linksAfter > 0)
    {
      std::memmove(&_links[removedFirstInput],
                   &_links[removedEndInput],
                   static_cast<size_t>(linksAfter) * sizeof(LogicV2InputLink));
    }
  }

  const uint16_t newLinkCount = static_cast<uint16_t>(
      oldLinkCount - removedInputCount);
  for (uint16_t link = newLinkCount; link < oldLinkCount; ++link)
  {
    _links[link] = LogicV2InputLink();
  }

  const uint16_t blocksAfter = static_cast<uint16_t>(
      oldBlockCount - blockIndex - 1U);
  if (blocksAfter > 0)
  {
    std::memmove(&_blocks[blockIndex],
                 &_blocks[blockIndex + 1U],
                 static_cast<size_t>(blocksAfter) * sizeof(LogicV2BlockRecord));
  }

  const uint16_t newBlockCount = static_cast<uint16_t>(oldBlockCount - 1U);
  _blocks[newBlockCount] = LogicV2BlockRecord();
  _blockCount = newBlockCount;
  _linkCount = newLinkCount;

  for (uint16_t block = blockIndex; block < _blockCount; ++block)
  {
    if (_blocks[block].firstInput >= removedEndInput)
    {
      _blocks[block].firstInput = static_cast<uint16_t>(
          _blocks[block].firstInput - removedInputCount);
    }
  }

  for (uint16_t link = 0; link < _linkCount; ++link)
  {
    const uint16_t source = _links[link].source();
    if (source <= JWPLC_LOGIC_V2_MAX_BLOCK_SOURCE && source > blockIndex)
    {
      _links[link] = makeLink(
          static_cast<uint16_t>(source - 1U),
          _links[link].inverted());
    }
  }

  if (validate() != LogicV2PrototypeError::None)
  {
    std::memcpy(_blocks, blockBackup, sizeof(_blocks));
    std::memcpy(_links, linkBackup, sizeof(_links));
    _blockCount = oldBlockCount;
    _linkCount = oldLinkCount;
    _dirty = oldDirty;
    return false;
  }

  _dirty = true;
  return true;
}

LogicV2PrototypeError RuntimeUIV2EditSession::validate() const
{
  if (!_active)
  {
    return LogicV2PrototypeError::NullArgument;
  }

  const LogicV2Program program = {
      _blocks,
      _blockCount,
      _links,
      _linkCount};

  return LogicVariableInputPrototype::validate(
      program,
      JWPLC_LOGIC_V2_COMPILED_MAX_BLOCKS,
      JWPLC_LOGIC_V2_COMPILED_MAX_LINKS,
      _digitalInputCount,
      _digitalOutputCount);
}

bool RuntimeUIV2EditSession::apply(bool restartIfPreviouslyRunning)
{
  if (_engine == nullptr || !_active || validate() != LogicV2PrototypeError::None)
  {
    return false;
  }

  const bool wasRunning = _engine->state() == LogicV2EngineState::Running;
  if (wasRunning)
  {
    _engine->stop();
  }

  const LogicV2Program program = {
      _blocks,
      _blockCount,
      _links,
      _linkCount};

  if (!_engine->loadProgram(program,
                            _digitalInputCount,
                            _digitalOutputCount))
  {
    return false;
  }

  if (wasRunning && restartIfPreviouslyRunning && !_engine->start())
  {
    return false;
  }

  _dirty = false;
  return begin();
}

uint16_t RuntimeUIV2EditSession::inputOffset(uint16_t blockIndex,
                                             uint8_t inputIndex) const
{
  if (!_active || blockIndex >= _blockCount ||
      inputIndex >= _blocks[blockIndex].inputCount)
  {
    return JWPLC_LOGIC_V2_COMPILED_MAX_LINKS;
  }

  const uint32_t offset = static_cast<uint32_t>(_blocks[blockIndex].firstInput) +
                          static_cast<uint32_t>(inputIndex);
  if (offset >= _linkCount)
  {
    return JWPLC_LOGIC_V2_COMPILED_MAX_LINKS;
  }
  return static_cast<uint16_t>(offset);
}

LogicV2InputLink RuntimeUIV2EditSession::makeLink(uint16_t source,
                                                  bool inverted) const
{
  if (source == JWPLC_LOGIC_V2_SOURCE_OPEN)
  {
    return LogicV2InputLink::open(inverted);
  }
  if (source == JWPLC_LOGIC_V2_SOURCE_CONST_TRUE)
  {
    return LogicV2InputLink::constantTrue(inverted);
  }
  if (source == JWPLC_LOGIC_V2_SOURCE_CONST_FALSE)
  {
    return LogicV2InputLink::constantFalse(inverted);
  }
  return LogicV2InputLink::block(source, inverted);
}

void RuntimeUIV2EditSession::clearDraft()
{
  _active = false;
  _dirty = false;
  _blockCount = 0;
  _linkCount = 0;
  _digitalInputCount = 0;
  _digitalOutputCount = 0;
  std::memset(_blocks, 0, sizeof(_blocks));
  std::memset(_links, 0, sizeof(_links));
}
