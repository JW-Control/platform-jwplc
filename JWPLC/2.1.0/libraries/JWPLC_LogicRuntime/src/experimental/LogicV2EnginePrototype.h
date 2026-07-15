#ifndef JWPLC_LOGIC_V2_ENGINE_PROTOTYPE_H
#define JWPLC_LOGIC_V2_ENGINE_PROTOTYPE_H

#include <Arduino.h>

#include "LogicVariableInputPrototype.h"

/**
 * @brief Estado del motor RAM v2 experimental.
 */
enum class LogicV2EngineState : uint8_t
{
  Empty = 0,
  Ready,
  Running,
  Stopped,
  Fault
};

/**
 * @brief Error operativo del motor RAM v2 experimental.
 */
enum class LogicV2EngineError : uint8_t
{
  None = 0,
  NullArgument,
  InvalidProgram,
  NotReady,
  NotRunning,
  InputProfileMismatch,
  EvaluationFailed
};

/**
 * @brief Motor de ejecución aislado para el modelo de entradas variables v2.
 *
 * Mantiene copias profundas de bloques y enlaces dentro de arreglos de tamaño
 * fijo. No conoce FRAM, codec, retentivos ni salidas físicas. Su objetivo es
 * medir el coste RAM y validar la carga/ejecución antes de sustituir el motor
 * estable v1.
 */
class LogicV2EnginePrototype
{
public:
  LogicV2EnginePrototype();

  bool loadProgram(const LogicV2Program &program,
                   uint8_t digitalInputCount);
  void unloadProgram();

  bool start();
  void stop();
  bool scan(const bool *digitalInputs, uint8_t digitalInputCount);

  bool hasProgram() const;
  LogicV2EngineState state() const;
  LogicV2EngineError lastError() const;
  LogicV2PrototypeError validationError() const;

  uint16_t blockCount() const;
  uint16_t linkCount() const;
  uint8_t digitalInputCount() const;
  uint32_t scanCount() const;

  bool blockValue(uint16_t blockIndex) const;
  const LogicV2BlockRecord *blockDefinition(uint16_t blockIndex) const;
  const LogicV2InputLink *inputLink(uint16_t linkIndex) const;
  const LogicV2Program *program() const;

  static const char *stateName(LogicV2EngineState state);
  static const char *errorName(LogicV2EngineError error);

private:
  void clearValues();
  void clearProgramStorage();
  void setFault(LogicV2EngineError error,
                LogicV2PrototypeError validationError);

  LogicV2BlockRecord _blocks[JWPLC_LOGIC_V2_COMPILED_MAX_BLOCKS];
  LogicV2InputLink _links[JWPLC_LOGIC_V2_COMPILED_MAX_LINKS];
  bool _values[JWPLC_LOGIC_V2_COMPILED_MAX_BLOCKS];
  LogicV2Program _program;
  uint8_t _digitalInputCount;
  LogicV2EngineState _state;
  LogicV2EngineError _lastError;
  LogicV2PrototypeError _validationError;
  uint32_t _scanCount;
};

#endif