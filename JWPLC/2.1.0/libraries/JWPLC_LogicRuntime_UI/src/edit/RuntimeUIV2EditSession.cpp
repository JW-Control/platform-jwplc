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

  // El prototipo v2 es acíclico: una entrada solo puede usar bloques previos.
  if (source < JWPLC_LOGIC_V2_SOURCE_CONST_FALSE && source >= blockIndex)
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
                                              uint8_t resource)
{
  if (!_active || blockIndex >= _blockCount)
  {
    return false;
  }
  _blocks[blockIndex].resource = resource;
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
  if (!_active || blockIndex >= _blockCount)
  {
    return JWPLC_LOGIC_V2_COMPILED_MAX_LINKS;
  }

  uint16_t offset = 0;
  for (uint16_t index = 0; index < blockIndex; ++index)
  {
    offset = static_cast<uint16_t>(offset + _blocks[index].inputCount);
  }

  if (inputIndex >= _blocks[blockIndex].inputCount)
  {
    return JWPLC_LOGIC_V2_COMPILED_MAX_LINKS;
  }
  return static_cast<uint16_t>(offset + inputIndex);
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
