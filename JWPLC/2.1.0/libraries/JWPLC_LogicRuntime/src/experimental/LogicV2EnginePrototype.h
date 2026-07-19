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
  TimeRequired,
  EvaluationFailed
};

/**
 * @brief Motor de ejecución aislado para el modelo de entradas variables v2.
 *
 * Mantiene copias profundas de bloques y enlaces dentro de arreglos de tamaño
 * fijo. No conoce FRAM, codec, retentivos ni salidas físicas. DigitalOutput se
 * conserva como valor lógico consultable, pero todavía no conmuta Q0.
 */
class LogicV2EnginePrototype
{
public:
  LogicV2EnginePrototype();

  bool loadProgram(const LogicV2Program &program,
                   uint8_t digitalInputCount,
                   uint8_t digitalOutputCount = 0);
  void unloadProgram();

  bool start();
  void stop();

  /**
   * @brief Scan sin tiempo, reservado para programas sin bloques TON.
   *
   * Cuando existe al menos un TON devuelve false y TIME_REQUIRED para evitar
   * que un temporizador quede congelado silenciosamente.
   */
  bool scan(const bool *digitalInputs, uint8_t digitalInputCount);

  /** @brief Scan determinista con tiempo explícito en milisegundos. */
  bool scan(const bool *digitalInputs,
            uint8_t digitalInputCount,
            uint32_t nowMs);

  bool hasProgram() const;
  LogicV2EngineState state() const;
  LogicV2EngineError lastError() const;
  LogicV2PrototypeError validationError() const;

  uint16_t blockCount() const;
  uint16_t linkCount() const;
  uint8_t digitalInputCount() const;
  uint8_t digitalOutputCount() const;
  uint32_t scanCount() const;

  bool blockValue(uint16_t blockIndex) const;

  /**
   * @brief Valor lógico resuelto de una entrada de bloque.
   *
   * Centraliza la semántica de OPEN, HI, LO y negación por pin. La UI y otras
   * vistas de inspección deben usar esta API en vez de duplicar reglas del
   * evaluador.
   */
  bool inputValue(uint16_t blockIndex, uint8_t inputIndex) const;

  /**
   * @brief Valor lógico asociado a un recurso Q0.x del programa cargado.
   *
   * Devuelve false cuando no existe un bloque DigitalOutput para el recurso.
   * El validador garantiza que no haya recursos de salida duplicados.
   */
  bool digitalOutputValue(uint8_t outputIndex) const;

  /** @brief Indica si el TON seleccionado está contando actualmente. */
  bool tonTiming(uint16_t blockIndex) const;

  /**
   * @brief Tiempo transcurrido del TON para un nowMs explícito.
   *
   * Devuelve 0 para bloques que no son TON o que todavía no iniciaron. Cuando
   * la salida ya está activa devuelve el parámetro completo.
   */
  uint32_t tonElapsedMs(uint16_t blockIndex, uint32_t nowMs) const;

  /** @brief Tiempo restante del TON, saturado entre 0 y el parámetro. */
  uint32_t tonRemainingMs(uint16_t blockIndex, uint32_t nowMs) const;

  const LogicV2BlockRecord *blockDefinition(uint16_t blockIndex) const;
  const LogicV2InputLink *inputLink(uint16_t linkIndex) const;
  const LogicV2Program *program() const;

  static const char *stateName(LogicV2EngineState state);
  static const char *errorName(LogicV2EngineError error);

private:
  bool containsTimedBlocks() const;
  bool neutralInputValue(LogicV2BlockType consumerType) const;
  bool resolveInputValue(const LogicV2InputLink &link,
                         LogicV2BlockType consumerType,
                         bool &value) const;
  void clearRuntimeState();
  void clearProgramStorage();
  void setFault(LogicV2EngineError error,
                LogicV2PrototypeError validationError);

  LogicV2BlockRecord _blocks[JWPLC_LOGIC_V2_COMPILED_MAX_BLOCKS];
  LogicV2InputLink _links[JWPLC_LOGIC_V2_COMPILED_MAX_LINKS];
  bool _values[JWPLC_LOGIC_V2_COMPILED_MAX_BLOCKS];
  bool _timing[JWPLC_LOGIC_V2_COMPILED_MAX_BLOCKS];
  uint32_t _startedAtMs[JWPLC_LOGIC_V2_COMPILED_MAX_BLOCKS];
  LogicV2Program _program;
  uint8_t _digitalInputCount;
  uint8_t _digitalOutputCount;
  LogicV2EngineState _state;
  LogicV2EngineError _lastError;
  LogicV2PrototypeError _validationError;
  uint32_t _scanCount;
};

#endif