#ifndef JWPLC_LOGIC_ENGINE_H
#define JWPLC_LOGIC_ENGINE_H

#include <Arduino.h>

#include "LogicProgram.h"
#include "LogicValidator.h"
#include "../io/JWPLCLogicIO.h"

class LogicEngine
{
public:
  LogicEngine();

  void attachIO(JWPLCLogicIO &io);
  bool loadProgram(const LogicProgram &program, uint16_t maxBlocks);
  void resetStates();
  bool scan(uint32_t nowMs);

  bool hasProgram() const;
  bool blockValue(uint16_t index) const;
  const LogicProgram *program() const;
  LogicValidationError validationError() const;

private:
  bool sourceValue(uint16_t source) const;

  JWPLCLogicIO *_io;
  const LogicProgram *_program;
  LogicBlockState _states[JWPLC_LOGIC_COMPILED_MAX_BLOCKS];
  LogicValidationError _validationError;
};

#endif
