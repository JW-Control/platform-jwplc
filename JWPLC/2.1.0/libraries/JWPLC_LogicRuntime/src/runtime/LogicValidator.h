#ifndef JWPLC_LOGIC_VALIDATOR_H
#define JWPLC_LOGIC_VALIDATOR_H

#include <Arduino.h>

#include "LogicProgram.h"

enum class LogicValidationError : uint8_t
{
  None = 0,
  NullProgram,
  EmptyProgram,
  TooManyBlocks,
  InvalidBlockType,
  MissingSource,
  SourceOutOfRange,
  SourceNotPrevious,
  ResourceOutOfRange,
  DuplicateDigitalOutput,
  InvalidBlockFlags
};

class LogicValidator
{
public:
  static LogicValidationError validate(const LogicProgram &program,
                                       uint16_t maxBlocks,
                                       uint8_t digitalInputCount,
                                       uint8_t digitalOutputCount);

  static const char *errorName(LogicValidationError error);

private:
  static LogicValidationError validateSource(uint16_t source,
                                             uint16_t currentBlockIndex);
};

#endif