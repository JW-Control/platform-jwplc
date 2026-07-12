#ifndef JWPLC_LOGIC_PROGRAM_H
#define JWPLC_LOGIC_PROGRAM_H

#include <Arduino.h>

#include "LogicBlock.h"

struct LogicProgram
{
  const char *name;
  const LogicBlockDefinition *blocks;
  uint16_t blockCount;
};

#endif
