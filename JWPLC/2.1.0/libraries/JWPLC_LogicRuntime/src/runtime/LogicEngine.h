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

  // El descriptor se copia por valor. Esto permite cargar descriptores
  // temporales, por ejemplo LogicProgramBuffer::asProgram(), sin conservar
  // un puntero colgante al objeto LogicProgram temporal.
  //
  // Los punteros name y blocks siguen apuntando al almacenamiento externo,
  // por lo que ese buffer debe vivir mientras el motor use el programa.
  LogicProgram _program;
  bool _hasProgram;

  LogicBlockState _states[JWPLC_LOGIC_COMPILED_MAX_BLOCKS];
  LogicValidationError _validationError;
};

#endif
