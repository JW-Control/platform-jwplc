#include "LogicVariableInputPrototype.h"

bool LogicVariableInputPrototype::isVariableGate(LogicV2BlockType type)
{
  return type == LogicV2BlockType::And ||
         type == LogicV2BlockType::Or ||
         type == LogicV2BlockType::Nand ||
         type == LogicV2BlockType::Nor ||
         type == LogicV2BlockType::Xor;
}

bool LogicVariableInputPrototype::isKnownType(LogicV2BlockType type)
{
  return type >= LogicV2BlockType::DigitalInput &&
         type <= LogicV2BlockType::DigitalOutput;
}

bool LogicVariableInputPrototype::neutralValue(LogicV2BlockType type)
{
  return type == LogicV2BlockType::And ||
         type == LogicV2BlockType::Nand;
}

LogicV2PrototypeError LogicVariableInputPrototype::validate(
    const LogicV2Program &program,
    uint16_t maxBlocks,
    uint16_t maxLinks,
    uint8_t digitalInputCount,
    uint8_t digitalOutputCount)
{
  if (program.blocks == nullptr ||
      (program.linkCount > 0 && program.links == nullptr))
  {
    return LogicV2PrototypeError::NullArgument;
  }

  if (program.blockCount == 0)
  {
    return LogicV2PrototypeError::EmptyProgram;
  }

  if (program.blockCount > maxBlocks)
  {
    return LogicV2PrototypeError::TooManyBlocks;
  }

  if (program.linkCount > maxLinks)
  {
    return LogicV2PrototypeError::TooManyLinks;
  }

  uint32_t usedDigitalOutputs = 0;

  for (uint16_t blockIndex = 0;
       blockIndex < program.blockCount;
       ++blockIndex)
  {
    const LogicV2BlockRecord &block = program.blocks[blockIndex];

    if (!isKnownType(block.type))
    {
      return LogicV2PrototypeError::InvalidBlockType;
    }

    if (block.flags != 0 || block.reserved != 0)
    {
      return LogicV2PrototypeError::InvalidBlockFlags;
    }

    uint8_t minimumInputs = 0;
    uint8_t maximumInputs = 0;
    bool openAllowed = false;

    switch (block.type)
    {
    case LogicV2BlockType::DigitalInput:
      if (block.resource >= digitalInputCount)
      {
        return LogicV2PrototypeError::ResourceOutOfRange;
      }
      break;

    case LogicV2BlockType::DigitalOutput:
      minimumInputs = 1;
      maximumInputs = 1;
      if (block.resource >= digitalOutputCount || block.resource >= 32)
      {
        return LogicV2PrototypeError::ResourceOutOfRange;
      }
      if ((usedDigitalOutputs & (1UL << block.resource)) != 0)
      {
        return LogicV2PrototypeError::DuplicateDigitalOutput;
      }
      usedDigitalOutputs |= (1UL << block.resource);
      break;

    case LogicV2BlockType::ConstantFalse:
    case LogicV2BlockType::ConstantTrue:
      break;

    case LogicV2BlockType::Not:
      minimumInputs = 1;
      maximumInputs = 1;
      break;

    case LogicV2BlockType::And:
    case LogicV2BlockType::Or:
    case LogicV2BlockType::Nand:
    case LogicV2BlockType::Nor:
    case LogicV2BlockType::Xor:
      minimumInputs = 2;
      maximumInputs = JWPLC_LOGIC_V2_MAX_INPUTS_PER_BLOCK;
      openAllowed = true;
      break;

    default:
      return LogicV2PrototypeError::InvalidBlockType;
    }

    if (block.inputCount < minimumInputs ||
        block.inputCount > maximumInputs)
    {
      return LogicV2PrototypeError::InvalidInputCount;
    }

    if (block.inputCount == 0)
    {
      continue;
    }

    const uint32_t endInput =
        static_cast<uint32_t>(block.firstInput) + block.inputCount;
    if (endInput > program.linkCount)
    {
      return LogicV2PrototypeError::InputRangeOutOfBounds;
    }

    for (uint8_t input = 0; input < block.inputCount; ++input)
    {
      const LogicV2InputLink &link =
          program.links[block.firstInput + input];
      const uint16_t source = link.source();

      if (source == JWPLC_LOGIC_V2_SOURCE_OPEN)
      {
        if (!openAllowed)
        {
          return LogicV2PrototypeError::OpenInputNotAllowed;
        }
        continue;
      }

      if (source == JWPLC_LOGIC_V2_SOURCE_CONST_FALSE ||
          source == JWPLC_LOGIC_V2_SOURCE_CONST_TRUE)
      {
        continue;
      }

      if (source > JWPLC_LOGIC_V2_MAX_BLOCK_SOURCE)
      {
        return LogicV2PrototypeError::InvalidSourceEncoding;
      }

      if (source >= blockIndex)
      {
        return LogicV2PrototypeError::SourceNotPrevious;
      }
    }
  }

  return LogicV2PrototypeError::None;
}

bool LogicVariableInputPrototype::resolveInput(
    const LogicV2InputLink &link,
    LogicV2BlockType consumerType,
    const bool *blockValues,
    bool &value)
{
  const uint16_t source = link.source();

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
  else if (source <= JWPLC_LOGIC_V2_MAX_BLOCK_SOURCE)
  {
    value = blockValues[source];
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

bool LogicVariableInputPrototype::evaluate(
    const LogicV2Program &program,
    const bool *digitalInputs,
    uint8_t digitalInputCount,
    bool *blockValues,
    size_t blockValueCapacity,
    LogicV2PrototypeError &error,
    uint8_t digitalOutputCount)
{
  error = validate(program,
                   program.blockCount,
                   program.linkCount,
                   digitalInputCount,
                   digitalOutputCount);
  if (error != LogicV2PrototypeError::None)
  {
    return false;
  }

  return evaluateValidated(program,
                           digitalInputs,
                           digitalInputCount,
                           blockValues,
                           blockValueCapacity,
                           error);
}

bool LogicVariableInputPrototype::evaluateValidated(
    const LogicV2Program &program,
    const bool *digitalInputs,
    uint8_t digitalInputCount,
    bool *blockValues,
    size_t blockValueCapacity,
    LogicV2PrototypeError &error)
{
  if (program.blocks == nullptr ||
      (program.linkCount > 0 && program.links == nullptr) ||
      blockValues == nullptr)
  {
    error = LogicV2PrototypeError::NullArgument;
    return false;
  }

  if (blockValueCapacity < program.blockCount)
  {
    error = LogicV2PrototypeError::OutputBufferTooSmall;
    return false;
  }

  if (digitalInputCount > 0 && digitalInputs == nullptr)
  {
    error = LogicV2PrototypeError::NullDigitalInputs;
    return false;
  }

  for (uint16_t blockIndex = 0;
       blockIndex < program.blockCount;
       ++blockIndex)
  {
    const LogicV2BlockRecord &block = program.blocks[blockIndex];
    bool result = false;

    switch (block.type)
    {
    case LogicV2BlockType::DigitalInput:
      if (block.resource >= digitalInputCount)
      {
        error = LogicV2PrototypeError::ResourceOutOfRange;
        return false;
      }
      result = digitalInputs[block.resource];
      break;

    case LogicV2BlockType::ConstantFalse:
      result = false;
      break;

    case LogicV2BlockType::ConstantTrue:
      result = true;
      break;

    case LogicV2BlockType::DigitalOutput:
    {
      bool inputValue = false;
      if (!resolveInput(program.links[block.firstInput],
                        block.type,
                        blockValues,
                        inputValue))
      {
        error = LogicV2PrototypeError::InvalidSourceEncoding;
        return false;
      }
      result = inputValue;
      break;
    }

    case LogicV2BlockType::Not:
    {
      bool inputValue = false;
      if (!resolveInput(program.links[block.firstInput],
                        block.type,
                        blockValues,
                        inputValue))
      {
        error = LogicV2PrototypeError::InvalidSourceEncoding;
        return false;
      }
      result = !inputValue;
      break;
    }

    case LogicV2BlockType::And:
    case LogicV2BlockType::Nand:
      result = true;
      for (uint8_t input = 0; input < block.inputCount; ++input)
      {
        bool inputValue = true;
        if (!resolveInput(program.links[block.firstInput + input],
                          block.type,
                          blockValues,
                          inputValue))
        {
          error = LogicV2PrototypeError::InvalidSourceEncoding;
          return false;
        }
        result = result && inputValue;
      }
      if (block.type == LogicV2BlockType::Nand)
      {
        result = !result;
      }
      break;

    case LogicV2BlockType::Or:
    case LogicV2BlockType::Nor:
    case LogicV2BlockType::Xor:
      result = false;
      for (uint8_t input = 0; input < block.inputCount; ++input)
      {
        bool inputValue = false;
        if (!resolveInput(program.links[block.firstInput + input],
                          block.type,
                          blockValues,
                          inputValue))
        {
          error = LogicV2PrototypeError::InvalidSourceEncoding;
          return false;
        }

        if (block.type == LogicV2BlockType::Xor)
        {
          result = result != inputValue;
        }
        else
        {
          result = result || inputValue;
        }
      }
      if (block.type == LogicV2BlockType::Nor)
      {
        result = !result;
      }
      break;

    default:
      error = LogicV2PrototypeError::InvalidBlockType;
      return false;
    }

    blockValues[blockIndex] = result;
  }

  error = LogicV2PrototypeError::None;
  return true;
}

const char *LogicVariableInputPrototype::errorName(
    LogicV2PrototypeError error)
{
  switch (error)
  {
  case LogicV2PrototypeError::None:
    return "NONE";
  case LogicV2PrototypeError::NullArgument:
    return "NULL_ARGUMENT";
  case LogicV2PrototypeError::EmptyProgram:
    return "EMPTY_PROGRAM";
  case LogicV2PrototypeError::TooManyBlocks:
    return "TOO_MANY_BLOCKS";
  case LogicV2PrototypeError::TooManyLinks:
    return "TOO_MANY_LINKS";
  case LogicV2PrototypeError::InvalidBlockType:
    return "INVALID_BLOCK_TYPE";
  case LogicV2PrototypeError::InvalidBlockFlags:
    return "INVALID_BLOCK_FLAGS";
  case LogicV2PrototypeError::InvalidInputCount:
    return "INVALID_INPUT_COUNT";
  case LogicV2PrototypeError::InputRangeOutOfBounds:
    return "INPUT_RANGE_OUT_OF_BOUNDS";
  case LogicV2PrototypeError::InvalidSourceEncoding:
    return "INVALID_SOURCE_ENCODING";
  case LogicV2PrototypeError::SourceNotPrevious:
    return "SOURCE_NOT_PREVIOUS";
  case LogicV2PrototypeError::ResourceOutOfRange:
    return "RESOURCE_OUT_OF_RANGE";
  case LogicV2PrototypeError::OpenInputNotAllowed:
    return "OPEN_INPUT_NOT_ALLOWED";
  case LogicV2PrototypeError::NullDigitalInputs:
    return "NULL_DIGITAL_INPUTS";
  case LogicV2PrototypeError::OutputBufferTooSmall:
    return "OUTPUT_BUFFER_TOO_SMALL";
  case LogicV2PrototypeError::DuplicateDigitalOutput:
    return "DUPLICATE_DIGITAL_OUTPUT";
  default:
    return "UNKNOWN";
  }
}