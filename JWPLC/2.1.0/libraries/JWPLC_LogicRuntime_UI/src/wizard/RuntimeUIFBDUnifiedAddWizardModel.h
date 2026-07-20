#ifndef JWPLC_LOGIC_RUNTIME_UI_FBD_UNIFIED_ADD_WIZARD_MODEL_H
#define JWPLC_LOGIC_RUNTIME_UI_FBD_UNIFIED_ADD_WIZARD_MODEL_H

#include <Arduino.h>
#include <JWPLC_LogicRuntime_V2.h>

/**
 * @brief Modelo puro RAM del asistente NUEVO BLOQUE de la UI Unified.
 *
 * No dibuja, no lee botones, no aplica el motor y no depende de ninguna Vx.
 * Conserva únicamente la selección y los valores necesarios para construir un
 * bloque al final del orden topológico.
 */
class RuntimeUIFBDUnifiedAddWizardModel
{
public:
  enum class Page : uint8_t
  {
    Type = 0,
    Configure
  };

  enum class Type : uint8_t
  {
    DigitalInput = 0,
    Not,
    And2,
    Ton,
    DigitalOutput,
    Count
  };

  enum class TimeBase : uint8_t
  {
    Seconds = 0,
    Minutes,
    Hours
  };

  RuntimeUIFBDUnifiedAddWizardModel();

  void reset(uint16_t originBlock,
             uint16_t existingBlockCount,
             uint8_t firstFreeOutput);

  Page page() const;
  void setPage(Page page);

  Type type() const;
  void moveType(bool forward);
  const char *typeName() const;
  LogicV2BlockType logicType() const;

  uint8_t field() const;
  uint8_t fieldCount() const;
  void moveField(bool forward);
  const char *fieldName(uint8_t field) const;

  uint16_t sourceA() const;
  uint16_t sourceB() const;
  void moveSourceA(bool forward, bool allowOpen = false);
  void moveSourceB(bool forward, bool allowOpen = true);

  uint8_t resource() const;
  void moveResource(bool forward, uint8_t count);

  uint32_t major() const;
  uint32_t minor() const;
  TimeBase timeBase() const;
  void moveMajor(bool increase);
  void moveMinor(bool increase);
  void moveTimeBase(bool forward);

  uint32_t timeMilliseconds() const;
  uint16_t timeResource() const;

  uint8_t inputCount() const;
  void buildInputs(LogicV2InputLink *destination,
                   uint8_t capacity) const;

  void formatSource(uint16_t source,
                    char *destination,
                    size_t capacity) const;
  void formatFieldValue(uint8_t field,
                        char *destination,
                        size_t capacity) const;

private:
  uint16_t sourceCandidateCount(bool allowOpen) const;
  uint16_t sourceCandidateAt(uint16_t candidate, bool allowOpen) const;
  uint16_t sourceCandidateIndex(uint16_t source, bool allowOpen) const;
  void moveSource(uint16_t &source, bool forward, bool allowOpen);
  void initializeTypeDefaults();
  uint32_t minorMaximum() const;

  Page _page;
  Type _type;
  uint8_t _field;
  uint16_t _originBlock;
  uint16_t _existingBlockCount;
  uint16_t _sourceA;
  uint16_t _sourceB;
  uint8_t _resource;
  uint32_t _major;
  uint32_t _minor;
  TimeBase _timeBase;
};

#endif
