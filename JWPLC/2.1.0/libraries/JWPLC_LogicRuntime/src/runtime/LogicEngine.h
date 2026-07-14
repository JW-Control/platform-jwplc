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
  void unloadProgram();
  void resetStates();
  bool scan(uint32_t nowMs);

  bool hasProgram() const;
  bool blockValue(uint16_t index) const;
  const LogicProgram *program() const;
  LogicValidationError validationError() const;

  /**
   * @brief Tamaño del bitmap de estado para el programa cargado.
   *
   * El bitmap usa el índice de bloque como posición estable. Solo se exportan o
   * restauran SET/RESET marcados como retentivos; los demás bits se ignoran.
   */
  size_t retentiveStateBytes() const;
  uint16_t retentiveBlockCount() const;
  bool exportRetentiveState(uint8_t *destination,
                            size_t destinationCapacity) const;
  bool importRetentiveState(const uint8_t *source,
                            size_t sourceLength);
  void clearRetentiveStates();

private:
  bool sourceValue(uint16_t source) const;
  bool isRetentiveSetReset(uint16_t index) const;

  JWPLCLogicIO *_io;

  // El motor conserva una copia profunda de nombre y bloques. Así el programa
  // puede provenir de un descriptor temporal o de un buffer de almacenamiento
  // que luego sea reutilizado sin dejar punteros colgantes durante el scan.
  char _programName[JWPLC_LOGIC_PROGRAM_NAME_BYTES + 1];
  LogicBlockDefinition _programBlocks[JWPLC_LOGIC_COMPILED_MAX_BLOCKS];
  LogicProgram _program;
  bool _hasProgram;

  LogicBlockState _states[JWPLC_LOGIC_COMPILED_MAX_BLOCKS];
  LogicValidationError _validationError;
};

#endif