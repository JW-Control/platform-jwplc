#include "LogicEngine.h"

#include <cstring>

LogicEngine::LogicEngine()
    : _io(nullptr),
      _programName{},
      _programBlocks{},
      _program{nullptr, nullptr, 0},
      _hasProgram(false),
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
    unloadProgram();
    _validationError = LogicValidationError::NullProgram;
    return false;
  }

  const LogicValidationError validationError =
      LogicValidator::validate(program,
                               maxBlocks,
                               _io->digitalInputCount(),
                               _io->digitalOutputCount());

  if (validationError != LogicValidationError::None)
  {
    unloadProgram();
    _validationError = validationError;
    return false;
  }

  memset(_programName, 0, sizeof(_programName));
  if (program.name != nullptr)
  {
    strncpy(_programName,
            program.name,
            JWPLC_LOGIC_PROGRAM_NAME_BYTES);
    _programName[JWPLC_LOGIC_PROGRAM_NAME_BYTES] = '\0';
  }

  memcpy(_programBlocks,
         program.blocks,
         static_cast<size_t>(program.blockCount) *
             sizeof(LogicBlockDefinition));

  _program = {_programName, _programBlocks, program.blockCount};
  _hasProgram = true;
  _validationError = LogicValidationError::None;

  resetStates();
  _io->allOutputsOff();
  return true;
}

void LogicEngine::unloadProgram()
{
  _program = {nullptr, nullptr, 0};
  _hasProgram = false;
  _validationError = LogicValidationError::None;
  resetStates();

  if (_io != nullptr)
  {
    _io->allOutputsOff();
  }
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
  if (!_hasProgram || source >= _program.blockCount)
  {
    return false;
  }

  return _states[source].value;
}

bool LogicEngine::scan(uint32_t nowMs)
{
  if (_io == nullptr || !_hasProgram || _program.blocks == nullptr)
  {
    return false;
  }

  _io->scanInputs();
  _io->beginOutputScan();

  for (uint16_t index = 0; index < _program.blockCount; ++index)
  {
    const LogicBlockDefinition &block = _program.blocks[index];
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

  for (uint16_t index = 0; index < _program.blockCount; ++index)
  {
    const LogicBlockDefinition &block = _program.blocks[index];
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
  return _hasProgram;
}

bool LogicEngine::blockValue(uint16_t index) const
{
  if (!_hasProgram || index >= _program.blockCount)
  {
    return false;
  }

  return _states[index].value;
}

const LogicProgram *LogicEngine::program() const
{
  return _hasProgram ? &_program : nullptr;
}

LogicValidationError LogicEngine::validationError() const
{
  return _validationError;
}
