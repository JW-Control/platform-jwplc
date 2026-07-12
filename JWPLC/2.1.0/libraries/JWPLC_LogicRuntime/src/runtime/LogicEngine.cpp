#include "LogicEngine.h"

LogicEngine::LogicEngine()
    : _io(nullptr),
      _program(nullptr),
      _states{},
      _validationError(LogicValidationError::None)
{
}

void LogicEngine::attachIO(JWPLCLogicIO &io)
{
  _io = &io;
}

bool LogicEngine::loadProgram(const LogicProgram &program, uint16_t maxBlocks)
{
  if (_io == nullptr)
  {
    _program = nullptr;
    _validationError = LogicValidationError::NullProgram;
    return false;
  }

  _validationError = LogicValidator::validate(program,
                                               maxBlocks,
                                               _io->digitalInputCount(),
                                               _io->digitalOutputCount());

  if (_validationError != LogicValidationError::None)
  {
    _program = nullptr;
    _io->allOutputsOff();
    return false;
  }

  _program = &program;
  resetStates();
  _io->allOutputsOff();
  return true;
}

void LogicEngine::resetStates()
{
  for (uint16_t index = 0; index < JWPLC_LOGIC_COMPILED_MAX_BLOCKS; ++index)
  {
    _states[index].value = false;
    _states[index].timing = false;
    _states[index].startedAtMs = 0;
  }
}

bool LogicEngine::sourceValue(uint16_t source) const
{
  if (_program == nullptr || source >= _program->blockCount)
  {
    return false;
  }

  return _states[source].value;
}

bool LogicEngine::scan(uint32_t nowMs)
{
  if (_io == nullptr || _program == nullptr)
  {
    return false;
  }

  _io->scanInputs();
  _io->beginOutputScan();

  for (uint16_t index = 0; index < _program->blockCount; ++index)
  {
    const LogicBlockDefinition &block = _program->blocks[index];
    LogicBlockState &state = _states[index];

    switch (block.type)
    {
    case LogicBlockType::DigitalInput:
      state.value = _io->digitalInput(static_cast<uint8_t>(block.resource));
      state.timing = false;
      break;

    case LogicBlockType::DigitalOutput:
      state.value = sourceValue(block.sourceA);
      state.timing = false;
      break;

    case LogicBlockType::Not:
      state.value = !sourceValue(block.sourceA);
      state.timing = false;
      break;

    case LogicBlockType::And:
      state.value = sourceValue(block.sourceA) && sourceValue(block.sourceB);
      state.timing = false;
      break;

    case LogicBlockType::Or:
      state.value = sourceValue(block.sourceA) || sourceValue(block.sourceB);
      state.timing = false;
      break;

    case LogicBlockType::SetReset:
      if (sourceValue(block.sourceB))
      {
        state.value = false;
      }
      else if (sourceValue(block.sourceA))
      {
        state.value = true;
      }
      state.timing = false;
      break;

    case LogicBlockType::Ton:
    {
      const bool input = sourceValue(block.sourceA);

      if (!input)
      {
        state.value = false;
        state.timing = false;
        state.startedAtMs = 0;
      }
      else if (!state.value)
      {
        if (!state.timing)
        {
          state.timing = true;
          state.startedAtMs = nowMs;
        }

        if (static_cast<uint32_t>(nowMs - state.startedAtMs) >= block.parameter)
        {
          state.value = true;
          state.timing = false;
        }
      }
      break;
    }

    default:
      _io->allOutputsOff();
      return false;
    }
  }

  for (uint16_t index = 0; index < _program->blockCount; ++index)
  {
    const LogicBlockDefinition &block = _program->blocks[index];
    if (block.type == LogicBlockType::DigitalOutput)
    {
      if (!_io->setDigitalOutput(static_cast<uint8_t>(block.resource),
                                 _states[index].value))
      {
        _io->allOutputsOff();
        return false;
      }
    }
  }

  _io->commitOutputs();
  return true;
}

bool LogicEngine::hasProgram() const
{
  return _program != nullptr;
}

bool LogicEngine::blockValue(uint16_t index) const
{
  if (_program == nullptr || index >= _program->blockCount)
  {
    return false;
  }

  return _states[index].value;
}

const LogicProgram *LogicEngine::program() const
{
  return _program;
}

LogicValidationError LogicEngine::validationError() const
{
  return _validationError;
}
