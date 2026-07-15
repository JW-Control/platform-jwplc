#include "LogicV2EnginePrototype.h"

#include <cstring>

LogicV2EnginePrototype::LogicV2EnginePrototype()
    : _blocks{},
      _links{},
      _values{},
      _program{nullptr, 0, nullptr, 0},
      _digitalInputCount(0),
      _state(LogicV2EngineState::Empty),
      _lastError(LogicV2EngineError::None),
      _validationError(LogicV2PrototypeError::None),
      _scanCount(0)
{
}

void LogicV2EnginePrototype::clearValues()
{
  memset(_values, 0, sizeof(_values));
}

void LogicV2EnginePrototype::clearProgramStorage()
{
  memset(_blocks, 0, sizeof(_blocks));
  memset(_links, 0, sizeof(_links));
  clearValues();
  _program = {nullptr, 0, nullptr, 0};
  _digitalInputCount = 0;
  _scanCount = 0;
}

void LogicV2EnginePrototype::setFault(
    LogicV2EngineError error,
    LogicV2PrototypeError validationError)
{
  clearProgramStorage();
  _state = LogicV2EngineState::Fault;
  _lastError = error;
  _validationError = validationError;
}

bool LogicV2EnginePrototype::loadProgram(const LogicV2Program &program,
                                         uint8_t digitalInputCount)
{
  const LogicV2PrototypeError validation =
      LogicVariableInputPrototype::validate(
          program,
          JWPLC_LOGIC_V2_COMPILED_MAX_BLOCKS,
          JWPLC_LOGIC_V2_COMPILED_MAX_LINKS,
          digitalInputCount);

  if (validation != LogicV2PrototypeError::None)
  {
    setFault(LogicV2EngineError::InvalidProgram, validation);
    return false;
  }

  memcpy(_blocks,
         program.blocks,
         static_cast<size_t>(program.blockCount) *
             sizeof(LogicV2BlockRecord));

  if (program.linkCount > 0)
  {
    memcpy(_links,
           program.links,
           static_cast<size_t>(program.linkCount) *
               sizeof(LogicV2InputLink));
  }

  if (program.blockCount < JWPLC_LOGIC_V2_COMPILED_MAX_BLOCKS)
  {
    memset(&_blocks[program.blockCount],
           0,
           static_cast<size_t>(JWPLC_LOGIC_V2_COMPILED_MAX_BLOCKS -
                               program.blockCount) *
               sizeof(LogicV2BlockRecord));
  }

  if (program.linkCount < JWPLC_LOGIC_V2_COMPILED_MAX_LINKS)
  {
    memset(&_links[program.linkCount],
           0,
           static_cast<size_t>(JWPLC_LOGIC_V2_COMPILED_MAX_LINKS -
                               program.linkCount) *
               sizeof(LogicV2InputLink));
  }

  _program = {_blocks, program.blockCount, _links, program.linkCount};
  _digitalInputCount = digitalInputCount;
  clearValues();
  _scanCount = 0;
  _state = LogicV2EngineState::Ready;
  _lastError = LogicV2EngineError::None;
  _validationError = LogicV2PrototypeError::None;
  return true;
}

void LogicV2EnginePrototype::unloadProgram()
{
  clearProgramStorage();
  _state = LogicV2EngineState::Empty;
  _lastError = LogicV2EngineError::None;
  _validationError = LogicV2PrototypeError::None;
}

bool LogicV2EnginePrototype::start()
{
  if (!hasProgram())
  {
    _state = LogicV2EngineState::Fault;
    _lastError = LogicV2EngineError::NotReady;
    return false;
  }

  clearValues();
  _scanCount = 0;
  _state = LogicV2EngineState::Running;
  _lastError = LogicV2EngineError::None;
  return true;
}

void LogicV2EnginePrototype::stop()
{
  clearValues();
  _scanCount = 0;
  _lastError = LogicV2EngineError::None;
  _state = hasProgram() ? LogicV2EngineState::Stopped
                        : LogicV2EngineState::Empty;
}

bool LogicV2EnginePrototype::scan(const bool *digitalInputs,
                                  uint8_t digitalInputCount)
{
  if (_state != LogicV2EngineState::Running || !hasProgram())
  {
    _lastError = LogicV2EngineError::NotRunning;
    return false;
  }

  if (digitalInputCount < _digitalInputCount ||
      (_digitalInputCount > 0 && digitalInputs == nullptr))
  {
    _lastError = LogicV2EngineError::InputProfileMismatch;
    return false;
  }

  LogicV2PrototypeError evaluationError = LogicV2PrototypeError::None;
  if (!LogicVariableInputPrototype::evaluateValidated(
          _program,
          digitalInputs,
          digitalInputCount,
          _values,
          JWPLC_LOGIC_V2_COMPILED_MAX_BLOCKS,
          evaluationError))
  {
    _state = LogicV2EngineState::Fault;
    _lastError = LogicV2EngineError::EvaluationFailed;
    _validationError = evaluationError;
    clearValues();
    return false;
  }

  ++_scanCount;
  _lastError = LogicV2EngineError::None;
  _validationError = LogicV2PrototypeError::None;
  return true;
}

bool LogicV2EnginePrototype::hasProgram() const
{
  return _program.blocks != nullptr && _program.blockCount > 0;
}

LogicV2EngineState LogicV2EnginePrototype::state() const
{
  return _state;
}

LogicV2EngineError LogicV2EnginePrototype::lastError() const
{
  return _lastError;
}

LogicV2PrototypeError LogicV2EnginePrototype::validationError() const
{
  return _validationError;
}

uint16_t LogicV2EnginePrototype::blockCount() const
{
  return _program.blockCount;
}

uint16_t LogicV2EnginePrototype::linkCount() const
{
  return _program.linkCount;
}

uint8_t LogicV2EnginePrototype::digitalInputCount() const
{
  return _digitalInputCount;
}

uint32_t LogicV2EnginePrototype::scanCount() const
{
  return _scanCount;
}

bool LogicV2EnginePrototype::blockValue(uint16_t blockIndex) const
{
  if (!hasProgram() || blockIndex >= _program.blockCount)
  {
    return false;
  }

  return _values[blockIndex];
}

const LogicV2BlockRecord *LogicV2EnginePrototype::blockDefinition(
    uint16_t blockIndex) const
{
  if (!hasProgram() || blockIndex >= _program.blockCount)
  {
    return nullptr;
  }

  return &_blocks[blockIndex];
}

const LogicV2InputLink *LogicV2EnginePrototype::inputLink(
    uint16_t linkIndex) const
{
  if (!hasProgram() || linkIndex >= _program.linkCount)
  {
    return nullptr;
  }

  return &_links[linkIndex];
}

const LogicV2Program *LogicV2EnginePrototype::program() const
{
  return hasProgram() ? &_program : nullptr;
}

const char *LogicV2EnginePrototype::stateName(LogicV2EngineState state)
{
  switch (state)
  {
  case LogicV2EngineState::Empty:
    return "EMPTY";
  case LogicV2EngineState::Ready:
    return "READY";
  case LogicV2EngineState::Running:
    return "RUNNING";
  case LogicV2EngineState::Stopped:
    return "STOPPED";
  case LogicV2EngineState::Fault:
    return "FAULT";
  default:
    return "UNKNOWN";
  }
}

const char *LogicV2EnginePrototype::errorName(LogicV2EngineError error)
{
  switch (error)
  {
  case LogicV2EngineError::None:
    return "NONE";
  case LogicV2EngineError::NullArgument:
    return "NULL_ARGUMENT";
  case LogicV2EngineError::InvalidProgram:
    return "INVALID_PROGRAM";
  case LogicV2EngineError::NotReady:
    return "NOT_READY";
  case LogicV2EngineError::NotRunning:
    return "NOT_RUNNING";
  case LogicV2EngineError::InputProfileMismatch:
    return "INPUT_PROFILE_MISMATCH";
  case LogicV2EngineError::EvaluationFailed:
    return "EVALUATION_FAILED";
  default:
    return "UNKNOWN";
  }
}