#ifndef JWPLC_LOGIC_RUNTIME_UI_V2_EDIT_SESSION_H
#define JWPLC_LOGIC_RUNTIME_UI_V2_EDIT_SESSION_H

#include <Arduino.h>
#include <JWPLC_LogicRuntime.h>

/**
 * @brief Borrador RAM transaccional para editar un programa Logic Runtime v2.
 *
 * Copia bloques y enlaces del motor, permite modificar el borrador y solo
 * reemplaza el programa activo después de validar la copia completa. No escribe
 * FRAM ni conmuta salidas físicas.
 */
class RuntimeUIV2EditSession
{
public:
  RuntimeUIV2EditSession();

  void attach(LogicV2EnginePrototype &engine);
  void detach();
  bool isAttached() const;

  bool begin();
  void cancel();
  bool active() const;
  bool dirty() const;

  uint16_t blockCount() const;
  uint16_t linkCount() const;
  const LogicV2BlockRecord *block(uint16_t blockIndex) const;
  const LogicV2InputLink *inputLink(uint16_t blockIndex,
                                    uint8_t inputIndex) const;

  bool setInputSource(uint16_t blockIndex,
                      uint8_t inputIndex,
                      uint16_t source,
                      bool inverted);
  bool setInputInverted(uint16_t blockIndex,
                        uint8_t inputIndex,
                        bool inverted);
  bool setBlockParameter(uint16_t blockIndex, uint32_t parameter);
  bool setBlockResource(uint16_t blockIndex, uint8_t resource);

  LogicV2PrototypeError validate() const;
  bool apply(bool restartIfPreviouslyRunning = true);

private:
  uint16_t inputOffset(uint16_t blockIndex, uint8_t inputIndex) const;
  LogicV2InputLink makeLink(uint16_t source, bool inverted) const;
  void clearDraft();

  LogicV2EnginePrototype *_engine;
  bool _active;
  bool _dirty;
  uint16_t _blockCount;
  uint16_t _linkCount;
  uint8_t _digitalInputCount;
  uint8_t _digitalOutputCount;

  LogicV2BlockRecord _blocks[JWPLC_LOGIC_V2_COMPILED_MAX_BLOCKS];
  LogicV2InputLink _links[JWPLC_LOGIC_V2_COMPILED_MAX_LINKS];
};

#endif
