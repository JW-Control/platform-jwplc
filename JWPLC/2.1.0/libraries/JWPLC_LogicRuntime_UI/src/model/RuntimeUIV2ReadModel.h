#ifndef JWPLC_LOGIC_RUNTIME_UI_V2_READ_MODEL_H
#define JWPLC_LOGIC_RUNTIME_UI_V2_READ_MODEL_H

#include <Arduino.h>
#include <JWPLC_LogicRuntime_V2.h>

/**
 * @brief Puente de inspección entre el motor RAM v2 y la UI.
 *
 * Las consultas normales siguen siendo de solo lectura. `mutableEngine()` se
 * reserva exclusivamente para la sesión transaccional RAM del editor, que
 * valida una copia completa antes de recargar el motor.
 */
class RuntimeUIV2ReadModel
{
public:
  RuntimeUIV2ReadModel();

  void attach(const LogicV2EnginePrototype &engine);
  void detach();
  bool isAttached() const;

  const LogicV2EnginePrototype *engine() const;

  LogicV2EnginePrototype *mutableEngine() const
  {
    return const_cast<LogicV2EnginePrototype *>(_engine);
  }

  LogicV2EngineState state() const;
  uint16_t blockCount() const;
  uint16_t linkCount() const;

  const LogicV2BlockRecord *block(uint16_t blockIndex) const;
  const LogicV2InputLink *link(uint16_t linkIndex) const;
  const LogicV2InputLink *inputLink(uint16_t blockIndex,
                                    uint8_t inputIndex) const;

  bool blockValue(uint16_t blockIndex) const;
  bool inputValue(uint16_t blockIndex, uint8_t inputIndex) const;

  bool isTon(uint16_t blockIndex) const;
  bool tonTiming(uint16_t blockIndex) const;
  uint32_t tonElapsedMs(uint16_t blockIndex, uint32_t nowMs) const;
  uint32_t tonRemainingMs(uint16_t blockIndex, uint32_t nowMs) const;

  uint8_t collectSources(uint16_t blockIndex,
                         uint16_t *destination,
                         uint8_t destinationCapacity) const;
  uint8_t collectConsumers(uint16_t sourceIndex,
                           uint16_t *destination,
                           uint8_t destinationCapacity) const;
  bool blockConsumes(uint16_t blockIndex, uint16_t sourceIndex) const;

  const char *typeName(LogicV2BlockType type) const;
  const char *typeShort(LogicV2BlockType type) const;
  const char *inputRole(LogicV2BlockType type,
                        uint8_t inputIndex) const;

private:
  const LogicV2EnginePrototype *_engine;
};

#endif