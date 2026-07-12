#include "LogicProgramCodec.h"

#include <string.h>

namespace
{
constexpr size_t OFFSET_MAGIC = 0;
constexpr size_t OFFSET_VERSION = 4;
constexpr size_t OFFSET_HEADER_SIZE = 6;
constexpr size_t OFFSET_PROGRAM_ID = 8;
constexpr size_t OFFSET_GENERATION = 12;
constexpr size_t OFFSET_BLOCK_COUNT = 16;
constexpr size_t OFFSET_BLOCK_SIZE = 18;
constexpr size_t OFFSET_PAYLOAD_LENGTH = 20;
constexpr size_t OFFSET_PAYLOAD_CRC = 24;
constexpr size_t OFFSET_HEADER_CRC = 28;
constexpr size_t OFFSET_FLAGS = 32;
constexpr size_t OFFSET_NAME = 36;

bool isValidBlockType(LogicBlockType type)
{
  return static_cast<uint8_t>(type) <=
         static_cast<uint8_t>(LogicBlockType::Ton);
}
} // namespace

size_t LogicProgramCodec::requiredSize(uint16_t blockCount)
{
  return static_cast<size_t>(JWPLC_LOGIC_IMAGE_HEADER_SIZE) +
         static_cast<size_t>(blockCount) * JWPLC_LOGIC_IMAGE_BLOCK_SIZE;
}

LogicProgramCodecError LogicProgramCodec::serialize(const LogicProgram &program,
                                                    uint32_t programId,
                                                    uint32_t generation,
                                                    uint32_t flags,
                                                    uint8_t *destination,
                                                    size_t destinationCapacity,
                                                    size_t &writtenBytes)
{
  writtenBytes = 0;

  if (destination == nullptr || program.blocks == nullptr || program.name == nullptr)
  {
    return LogicProgramCodecError::NullArgument;
  }

  if (program.blockCount == 0)
  {
    return LogicProgramCodecError::EmptyProgram;
  }

  if (program.blockCount > JWPLC_LOGIC_COMPILED_MAX_BLOCKS)
  {
    return LogicProgramCodecError::TooManyBlocks;
  }

  const size_t nameLength = strlen(program.name);
  if (nameLength > JWPLC_LOGIC_IMAGE_NAME_BYTES)
  {
    return LogicProgramCodecError::NameTooLong;
  }

  for (uint16_t index = 0; index < program.blockCount; ++index)
  {
    if (!isValidBlockType(program.blocks[index].type))
    {
      return LogicProgramCodecError::InvalidBlockType;
    }
  }

  const size_t totalSize = requiredSize(program.blockCount);
  if (destinationCapacity < totalSize)
  {
    return LogicProgramCodecError::BufferTooSmall;
  }

  memset(destination, 0, totalSize);

  const uint32_t payloadLength =
      static_cast<uint32_t>(program.blockCount) * JWPLC_LOGIC_IMAGE_BLOCK_SIZE;

  writeU32LE(destination + OFFSET_MAGIC, JWPLC_LOGIC_IMAGE_MAGIC);
  writeU16LE(destination + OFFSET_VERSION, JWPLC_LOGIC_IMAGE_FORMAT_VERSION);
  writeU16LE(destination + OFFSET_HEADER_SIZE, JWPLC_LOGIC_IMAGE_HEADER_SIZE);
  writeU32LE(destination + OFFSET_PROGRAM_ID, programId);
  writeU32LE(destination + OFFSET_GENERATION, generation);
  writeU16LE(destination + OFFSET_BLOCK_COUNT, program.blockCount);
  writeU16LE(destination + OFFSET_BLOCK_SIZE, JWPLC_LOGIC_IMAGE_BLOCK_SIZE);
  writeU32LE(destination + OFFSET_PAYLOAD_LENGTH, payloadLength);
  writeU32LE(destination + OFFSET_FLAGS, flags);
  memcpy(destination + OFFSET_NAME, program.name, nameLength);

  uint8_t *payload = destination + JWPLC_LOGIC_IMAGE_HEADER_SIZE;
  for (uint16_t index = 0; index < program.blockCount; ++index)
  {
    const LogicBlockDefinition &block = program.blocks[index];
    uint8_t *record = payload +
                      static_cast<size_t>(index) * JWPLC_LOGIC_IMAGE_BLOCK_SIZE;

    record[0] = static_cast<uint8_t>(block.type);
    record[1] = 0; // reservado para flags por bloque
    writeU16LE(record + 2, block.resource);
    writeU16LE(record + 4, block.sourceA);
    writeU16LE(record + 6, block.sourceB);
    writeU32LE(record + 8, block.parameter);
  }

  const uint32_t payloadCrc = crc32(payload, payloadLength);
  writeU32LE(destination + OFFSET_PAYLOAD_CRC, payloadCrc);

  // El campo header CRC permanece en cero durante el cálculo.
  const uint32_t headerCrc = crc32(destination, JWPLC_LOGIC_IMAGE_HEADER_SIZE);
  writeU32LE(destination + OFFSET_HEADER_CRC, headerCrc);

  writtenBytes = totalSize;
  return LogicProgramCodecError::None;
}

LogicProgramCodecError LogicProgramCodec::deserialize(const uint8_t *source,
                                                      size_t sourceLength,
                                                      LogicProgramBuffer &destination)
{
  if (source == nullptr)
  {
    return LogicProgramCodecError::NullArgument;
  }

  if (sourceLength < JWPLC_LOGIC_IMAGE_HEADER_SIZE)
  {
    return LogicProgramCodecError::InvalidLength;
  }

  if (readU32LE(source + OFFSET_MAGIC) != JWPLC_LOGIC_IMAGE_MAGIC)
  {
    return LogicProgramCodecError::InvalidMagic;
  }

  if (readU16LE(source + OFFSET_VERSION) != JWPLC_LOGIC_IMAGE_FORMAT_VERSION)
  {
    return LogicProgramCodecError::UnsupportedVersion;
  }

  if (readU16LE(source + OFFSET_HEADER_SIZE) != JWPLC_LOGIC_IMAGE_HEADER_SIZE)
  {
    return LogicProgramCodecError::InvalidHeaderSize;
  }

  if (readU16LE(source + OFFSET_BLOCK_SIZE) != JWPLC_LOGIC_IMAGE_BLOCK_SIZE)
  {
    return LogicProgramCodecError::InvalidBlockRecordSize;
  }

  uint8_t headerCopy[JWPLC_LOGIC_IMAGE_HEADER_SIZE];
  memcpy(headerCopy, source, JWPLC_LOGIC_IMAGE_HEADER_SIZE);
  const uint32_t storedHeaderCrc = readU32LE(headerCopy + OFFSET_HEADER_CRC);
  writeU32LE(headerCopy + OFFSET_HEADER_CRC, 0);

  if (crc32(headerCopy, sizeof(headerCopy)) != storedHeaderCrc)
  {
    return LogicProgramCodecError::HeaderCrcMismatch;
  }

  const uint16_t blockCount = readU16LE(source + OFFSET_BLOCK_COUNT);
  if (blockCount == 0)
  {
    return LogicProgramCodecError::EmptyProgram;
  }

  if (blockCount > JWPLC_LOGIC_COMPILED_MAX_BLOCKS)
  {
    return LogicProgramCodecError::TooManyBlocks;
  }

  const uint32_t payloadLength = readU32LE(source + OFFSET_PAYLOAD_LENGTH);
  const uint32_t expectedPayloadLength =
      static_cast<uint32_t>(blockCount) * JWPLC_LOGIC_IMAGE_BLOCK_SIZE;

  if (payloadLength != expectedPayloadLength ||
      sourceLength < static_cast<size_t>(JWPLC_LOGIC_IMAGE_HEADER_SIZE) + payloadLength)
  {
    return LogicProgramCodecError::InvalidLength;
  }

  const uint8_t *payload = source + JWPLC_LOGIC_IMAGE_HEADER_SIZE;
  const uint32_t storedPayloadCrc = readU32LE(source + OFFSET_PAYLOAD_CRC);
  if (crc32(payload, payloadLength) != storedPayloadCrc)
  {
    return LogicProgramCodecError::PayloadCrcMismatch;
  }

  memset(&destination, 0, sizeof(destination));
  memcpy(destination.name, source + OFFSET_NAME, JWPLC_LOGIC_IMAGE_NAME_BYTES);
  destination.name[JWPLC_LOGIC_IMAGE_NAME_BYTES] = '\0';
  destination.blockCount = blockCount;
  destination.metadata.programId = readU32LE(source + OFFSET_PROGRAM_ID);
  destination.metadata.generation = readU32LE(source + OFFSET_GENERATION);
  destination.metadata.flags = readU32LE(source + OFFSET_FLAGS);
  destination.metadata.payloadLength = payloadLength;
  destination.metadata.payloadCrc32 = storedPayloadCrc;
  destination.metadata.blockCount = blockCount;

  for (uint16_t index = 0; index < blockCount; ++index)
  {
    const uint8_t *record = payload +
                            static_cast<size_t>(index) * JWPLC_LOGIC_IMAGE_BLOCK_SIZE;
    LogicBlockDefinition &block = destination.blocks[index];

    block.type = static_cast<LogicBlockType>(record[0]);
    if (!isValidBlockType(block.type))
    {
      memset(&destination, 0, sizeof(destination));
      return LogicProgramCodecError::InvalidBlockType;
    }

    block.resource = readU16LE(record + 2);
    block.sourceA = readU16LE(record + 4);
    block.sourceB = readU16LE(record + 6);
    block.parameter = readU32LE(record + 8);
  }

  return LogicProgramCodecError::None;
}

uint32_t LogicProgramCodec::crc32(const uint8_t *data, size_t length)
{
  if (data == nullptr && length != 0)
  {
    return 0;
  }

  uint32_t crc = 0xFFFFFFFFUL;
  for (size_t index = 0; index < length; ++index)
  {
    crc ^= data[index];
    for (uint8_t bit = 0; bit < 8; ++bit)
    {
      const uint32_t mask = static_cast<uint32_t>(
          -static_cast<int32_t>(crc & 1UL));
      crc = (crc >> 1) ^ (0xEDB88320UL & mask);
    }
  }

  return ~crc;
}

const char *LogicProgramCodec::errorName(LogicProgramCodecError error)
{
  switch (error)
  {
  case LogicProgramCodecError::None:
    return "NONE";
  case LogicProgramCodecError::NullArgument:
    return "NULL_ARGUMENT";
  case LogicProgramCodecError::EmptyProgram:
    return "EMPTY_PROGRAM";
  case LogicProgramCodecError::TooManyBlocks:
    return "TOO_MANY_BLOCKS";
  case LogicProgramCodecError::NameTooLong:
    return "NAME_TOO_LONG";
  case LogicProgramCodecError::InvalidBlockType:
    return "INVALID_BLOCK_TYPE";
  case LogicProgramCodecError::BufferTooSmall:
    return "BUFFER_TOO_SMALL";
  case LogicProgramCodecError::InvalidMagic:
    return "INVALID_MAGIC";
  case LogicProgramCodecError::UnsupportedVersion:
    return "UNSUPPORTED_VERSION";
  case LogicProgramCodecError::InvalidHeaderSize:
    return "INVALID_HEADER_SIZE";
  case LogicProgramCodecError::InvalidBlockRecordSize:
    return "INVALID_BLOCK_RECORD_SIZE";
  case LogicProgramCodecError::InvalidLength:
    return "INVALID_LENGTH";
  case LogicProgramCodecError::HeaderCrcMismatch:
    return "HEADER_CRC_MISMATCH";
  case LogicProgramCodecError::PayloadCrcMismatch:
    return "PAYLOAD_CRC_MISMATCH";
  default:
    return "UNKNOWN";
  }
}

void LogicProgramCodec::writeU16LE(uint8_t *destination, uint16_t value)
{
  destination[0] = static_cast<uint8_t>(value & 0xFFU);
  destination[1] = static_cast<uint8_t>((value >> 8) & 0xFFU);
}

void LogicProgramCodec::writeU32LE(uint8_t *destination, uint32_t value)
{
  destination[0] = static_cast<uint8_t>(value & 0xFFUL);
  destination[1] = static_cast<uint8_t>((value >> 8) & 0xFFUL);
  destination[2] = static_cast<uint8_t>((value >> 16) & 0xFFUL);
  destination[3] = static_cast<uint8_t>((value >> 24) & 0xFFUL);
}

uint16_t LogicProgramCodec::readU16LE(const uint8_t *source)
{
  return static_cast<uint16_t>(source[0]) |
         static_cast<uint16_t>(source[1] << 8);
}

uint32_t LogicProgramCodec::readU32LE(const uint8_t *source)
{
  return static_cast<uint32_t>(source[0]) |
         (static_cast<uint32_t>(source[1]) << 8) |
         (static_cast<uint32_t>(source[2]) << 16) |
         (static_cast<uint32_t>(source[3]) << 24);
}
