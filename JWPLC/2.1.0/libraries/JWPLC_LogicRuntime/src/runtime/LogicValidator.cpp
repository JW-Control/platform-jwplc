#include "LogicValidator.h"

LogicValidationError LogicValidator::validateSource(uint16_t source,
                                                    uint16_t currentBlockIndex)
{
  if (source == JWPLC_LOGIC_NO_SOURCE)
  {
    return LogicValidationError::MissingSource;
  }

  if (source >= currentBlockIndex)
  {
    return source == currentBlockIndex
               ? LogicValidationError::SourceNotPrevious
               : LogicValidationError::SourceOutOfRange;
  }

  return LogicValidationError::None;
}

LogicValidationError LogicValidator::validate(const LogicProgram &program,
                                               uint16_t maxBlocks,
                                               uint8_t digitalInputCount,
                                               uint8_t digitalOutputCount)
{
  if (program.blocks == nullptr)
  {
    return LogicValidationError::NullProgram;
  }

  if (program.blockCount == 0)
  {
    return LogicValidationError::EmptyProgram;
  }

  if (program.blockCount > maxBlocks ||
      program.blockCount > JWPLC_LOGIC_COMPILED_MAX_BLOCKS)
  {
    return LogicValidationError::TooManyBlocks;
  }

  uint32_t usedDigitalOutputs = 0;

  for (uint16_t index = 0; index < program.blockCount; ++index)
  {
    const LogicBlockDefinition &block = program.blocks[index];
    LogicValidationError error = LogicValidationError::None;

    switch (block.type)
    {
    case LogicBlockType::DigitalInput:
      if (block.resource >= digitalInputCount)
      {
        return LogicValidationError::ResourceOutOfRange;
      }
      break;

    case LogicBlockType::DigitalOutput:
      error = validateSource(block.sourceA, index);
      if (error != LogicValidationError::None)
      {
        return error;
      }

      if (block.resource >= digitalOutputCount || block.resource >= 32)
      {
        return LogicValidationError::ResourceOutOfRange;
      }

      if ((usedDigitalOutputs & (1UL << block.resource)) != 0)
      {
        return LogicValidationError::DuplicateDigitalOutput;
      }

      usedDigitalOutputs |= (1UL << block.resource);
      break;

    case LogicBlockType::Not:
    case LogicBlockType::Ton:
      error = validateSource(block.sourceA, index);
      if (error != LogicValidationError::None)
      {
        return error;
      }
      break;

    case LogicBlockType::And:
    case LogicBlockType::Or:
    case LogicBlockType::SetReset:
      error = validateSource(block.sourceA, index);
      if (error != LogicValidationError::None)
      {
        return error;
      }

      error = validateSource(block.sourceB, index);
      if (error != LogicValidationError::None)
      {
        return error;
      }
      break;

    default:
      return LogicValidationError::InvalidBlockType;
    }
  }

  return LogicValidationError::None;
}

const char *LogicValidator::errorName(LogicValidationError error)
{
  switch (error)
  {
  case LogicValidationError::None:
    return "NONE";
  case LogicValidationError::NullProgram:
    return "NULL_PROGRAM";
  case LogicValidationError::EmptyProgram:
    return "EMPTY_PROGRAM";
  case LogicValidationError::TooManyBlocks:
    return "TOO_MANY_BLOCKS";
  case LogicValidationError::InvalidBlockType:
    return "INVALID_BLOCK_TYPE";
  case LogicValidationError::MissingSource:
    return "MISSING_SOURCE";
  case LogicValidationError::SourceOutOfRange:
    return "SOURCE_OUT_OF_RANGE";
  case LogicValidationError::SourceNotPrevious:
    return "SOURCE_NOT_PREVIOUS";
  case LogicValidationError::ResourceOutOfRange:
    return "RESOURCE_OUT_OF_RANGE";
  case LogicValidationError::DuplicateDigitalOutput:
    return "DUPLICATE_DIGITAL_OUTPUT";
  default:
    return "UNKNOWN";
  }
}
