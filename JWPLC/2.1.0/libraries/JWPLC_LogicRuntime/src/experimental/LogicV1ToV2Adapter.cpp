#include "LogicV1ToV2Adapter.h"

namespace
{
  void clearDestination(LogicV2Program &destination)
  {
    destination = {nullptr, 0, nullptr, 0};
  }
}

bool LogicV1ToV2Adapter::sourceIsValid(uint16_t source,
                                       uint16_t currentBlockIndex)
{
  return source != JWPLC_LOGIC_NO_SOURCE && source < currentBlockIndex;
}

uint8_t LogicV1ToV2Adapter::inputCountForType(LogicBlockType type)
{
  switch (type)
  {
  case LogicBlockType::DigitalInput:
    return 0;
  case LogicBlockType::DigitalOutput:
  case LogicBlockType::Not:
    return 1;
  case LogicBlockType::And:
  case LogicBlockType::Or:
  case LogicBlockType::SetReset:
    return 2;
  default:
    return 0xFFU;
  }
}

bool LogicV1ToV2Adapter::mapType(LogicBlockType sourceType,
                                 LogicV2BlockType &destinationType)
{
  switch (sourceType)
  {
  case LogicBlockType::DigitalInput:
    destinationType = LogicV2BlockType::DigitalInput;
    return true;
  case LogicBlockType::DigitalOutput:
    destinationType = LogicV2BlockType::DigitalOutput;
    return true;
  case LogicBlockType::Not:
    destinationType = LogicV2BlockType::Not;
    return true;
  case LogicBlockType::And:
    destinationType = LogicV2BlockType::And;
    return true;
  case LogicBlockType::Or:
    destinationType = LogicV2BlockType::Or;
    return true;
  case LogicBlockType::SetReset:
    destinationType = LogicV2BlockType::SetReset;
    return true;
  default:
    return false;
  }
}

bool LogicV1ToV2Adapter::requiredLinkCount(
    const LogicProgram &source,
    size_t &requiredLinks,
    LogicV1ToV2AdapterError &error)
{
  requiredLinks = 0;
  error = LogicV1ToV2AdapterError::None;

  if (source.blocks == nullptr)
  {
    error = LogicV1ToV2AdapterError::NullArgument;
    return false;
  }

  if (source.blockCount == 0)
  {
    error = LogicV1ToV2AdapterError::EmptyProgram;
    return false;
  }

  if (source.blockCount > JWPLC_LOGIC_V2_COMPILED_MAX_BLOCKS)
  {
    error = LogicV1ToV2AdapterError::TooManyBlocks;
    return false;
  }

  for (uint16_t index = 0; index < source.blockCount; ++index)
  {
    const LogicBlockDefinition &block = source.blocks[index];

    // La retención persistente v2 todavía no está habilitada. SET/RESET con
    // flags cero sí puede convertirse y mantener su estado durante RUN.
    if (block.flags != JWPLC_LOGIC_BLOCK_FLAG_NONE)
    {
      error = LogicV1ToV2AdapterError::UnsupportedFlags;
      return false;
    }

    const uint8_t inputCount = inputCountForType(block.type);
    if (inputCount == 0xFFU)
    {
      error = LogicV1ToV2AdapterError::UnsupportedBlockType;
      return false;
    }

    if (inputCount >= 1U)
    {
      if (block.sourceA == JWPLC_LOGIC_NO_SOURCE)
      {
        error = LogicV1ToV2AdapterError::MissingSource;
        return false;
      }

      if (!sourceIsValid(block.sourceA, index))
      {
        error = LogicV1ToV2AdapterError::SourceNotPrevious;
        return false;
      }
    }

    if (inputCount >= 2U)
    {
      if (block.sourceB == JWPLC_LOGIC_NO_SOURCE)
      {
        error = LogicV1ToV2AdapterError::MissingSource;
        return false;
      }

      if (!sourceIsValid(block.sourceB, index))
      {
        error = LogicV1ToV2AdapterError::SourceNotPrevious;
        return false;
      }
    }

    requiredLinks += inputCount;
    if (requiredLinks > JWPLC_LOGIC_V2_COMPILED_MAX_LINKS)
    {
      error = LogicV1ToV2AdapterError::LinkBufferTooSmall;
      return false;
    }
  }

  return true;
}

bool LogicV1ToV2Adapter::convert(
    const LogicProgram &source,
    LogicV2BlockRecord *destinationBlocks,
    size_t destinationBlockCapacity,
    LogicV2InputLink *destinationLinks,
    size_t destinationLinkCapacity,
    LogicV2Program &destination,
    LogicV1ToV2AdapterError &error)
{
  clearDestination(destination);
  error = LogicV1ToV2AdapterError::None;

  size_t requiredLinks = 0;
  if (!requiredLinkCount(source, requiredLinks, error))
  {
    return false;
  }

  if (destinationBlocks == nullptr ||
      (requiredLinks > 0 && destinationLinks == nullptr))
  {
    error = LogicV1ToV2AdapterError::NullArgument;
    return false;
  }

  if (destinationBlockCapacity < source.blockCount)
  {
    error = LogicV1ToV2AdapterError::BlockBufferTooSmall;
    return false;
  }

  if (destinationLinkCapacity < requiredLinks)
  {
    error = LogicV1ToV2AdapterError::LinkBufferTooSmall;
    return false;
  }

  uint16_t linkCursor = 0;

  for (uint16_t index = 0; index < source.blockCount; ++index)
  {
    const LogicBlockDefinition &sourceBlock = source.blocks[index];
    LogicV2BlockType destinationType = LogicV2BlockType::DigitalInput;

    if (!mapType(sourceBlock.type, destinationType))
    {
      clearDestination(destination);
      error = LogicV1ToV2AdapterError::UnsupportedBlockType;
      return false;
    }

    const uint8_t inputCount = inputCountForType(sourceBlock.type);
    destinationBlocks[index] = LogicV2BlockRecord(
        destinationType,
        linkCursor,
        inputCount,
        sourceBlock.resource,
        sourceBlock.parameter,
        0);

    if (inputCount >= 1U)
    {
      destinationLinks[linkCursor++] =
          LogicV2InputLink::block(sourceBlock.sourceA);
    }

    if (inputCount >= 2U)
    {
      destinationLinks[linkCursor++] =
          LogicV2InputLink::block(sourceBlock.sourceB);
    }
  }

  destination = {
      destinationBlocks,
      source.blockCount,
      destinationLinks,
      linkCursor};

  error = LogicV1ToV2AdapterError::None;
  return true;
}

const char *LogicV1ToV2Adapter::errorName(
    LogicV1ToV2AdapterError error)
{
  switch (error)
  {
  case LogicV1ToV2AdapterError::None:
    return "NONE";
  case LogicV1ToV2AdapterError::NullArgument:
    return "NULL_ARGUMENT";
  case LogicV1ToV2AdapterError::EmptyProgram:
    return "EMPTY_PROGRAM";
  case LogicV1ToV2AdapterError::TooManyBlocks:
    return "TOO_MANY_BLOCKS";
  case LogicV1ToV2AdapterError::BlockBufferTooSmall:
    return "BLOCK_BUFFER_TOO_SMALL";
  case LogicV1ToV2AdapterError::LinkBufferTooSmall:
    return "LINK_BUFFER_TOO_SMALL";
  case LogicV1ToV2AdapterError::UnsupportedBlockType:
    return "UNSUPPORTED_BLOCK_TYPE";
  case LogicV1ToV2AdapterError::UnsupportedFlags:
    return "UNSUPPORTED_FLAGS";
  case LogicV1ToV2AdapterError::MissingSource:
    return "MISSING_SOURCE";
  case LogicV1ToV2AdapterError::SourceNotPrevious:
    return "SOURCE_NOT_PREVIOUS";
  default:
    return "UNKNOWN";
  }
}