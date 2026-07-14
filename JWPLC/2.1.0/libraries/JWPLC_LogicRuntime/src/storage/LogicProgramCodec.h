#ifndef JWPLC_LOGIC_PROGRAM_CODEC_H
#define JWPLC_LOGIC_PROGRAM_CODEC_H

#include <Arduino.h>

#include "LogicProgramImage.h"

enum class LogicProgramCodecError : uint8_t
{
  None = 0,
  NullArgument,
  EmptyProgram,
  TooManyBlocks,
  NameTooLong,
  InvalidBlockType,
  BufferTooSmall,
  InvalidMagic,
  UnsupportedVersion,
  InvalidHeaderSize,
  InvalidBlockRecordSize,
  InvalidLength,
  HeaderCrcMismatch,
  PayloadCrcMismatch,
  InvalidBlockFlags
};

class LogicProgramCodec
{
public:
  static size_t requiredSize(uint16_t blockCount);

  static LogicProgramCodecError serialize(const LogicProgram &program,
                                           uint32_t programId,
                                           uint32_t generation,
                                           uint32_t flags,
                                           uint8_t *destination,
                                           size_t destinationCapacity,
                                           size_t &writtenBytes);

  static LogicProgramCodecError deserialize(const uint8_t *source,
                                             size_t sourceLength,
                                             LogicProgramBuffer &destination);

  static uint32_t crc32(const uint8_t *data, size_t length);
  static const char *errorName(LogicProgramCodecError error);

private:
  static void writeU16LE(uint8_t *destination, uint16_t value);
  static void writeU32LE(uint8_t *destination, uint32_t value);
  static uint16_t readU16LE(const uint8_t *source);
  static uint32_t readU32LE(const uint8_t *source);
};

#endif