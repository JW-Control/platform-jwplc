#ifndef JWPLC_LOGIC_PROGRAM_H
#define JWPLC_LOGIC_PROGRAM_H

#include <Arduino.h>

#include "LogicBlock.h"

static constexpr uint8_t JWPLC_LOGIC_PROGRAM_NAME_BYTES = 24;

struct LogicProgram
{
  const char *name;
  const LogicBlockDefinition *blocks;
  uint16_t blockCount;
};

#endif
