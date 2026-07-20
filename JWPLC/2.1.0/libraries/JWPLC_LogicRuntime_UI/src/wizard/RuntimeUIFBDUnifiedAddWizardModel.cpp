#include "RuntimeUIFBDUnifiedAddWizardModel.h"

#include <cstdio>

RuntimeUIFBDUnifiedAddWizardModel::RuntimeUIFBDUnifiedAddWizardModel()
    : _page(Page::Type),
      _type(Type::DigitalInput),
      _field(0),
      _originBlock(0),
      _existingBlockCount(0),
      _sourceA(JWPLC_LOGIC_V2_SOURCE_CONST_FALSE),
      _sourceB(JWPLC_LOGIC_V2_SOURCE_CONST_TRUE),
      _resource(0),
      _major(1),
      _minor(0),
      _timeBase(TimeBase::Seconds)
{
}

void RuntimeUIFBDUnifiedAddWizardModel::reset(uint16_t originBlock,
                                               uint16_t existingBlockCount,
                                               uint8_t firstFreeOutput)
{
  _page = Page::Type;
  _type = Type::DigitalInput;
  _field = 0;
  _originBlock = originBlock;
  _existingBlockCount = existingBlockCount;
  _resource = firstFreeOutput;
  initializeTypeDefaults();
}

RuntimeUIFBDUnifiedAddWizardModel::Page
RuntimeUIFBDUnifiedAddWizardModel::page() const
{
  return _page;
}

void RuntimeUIFBDUnifiedAddWizardModel::setPage(Page page)
{
  _page = page;
  _field = 0;
  if (page == Page::Configure)
  {
    initializeTypeDefaults();
  }
}

RuntimeUIFBDUnifiedAddWizardModel::Type
RuntimeUIFBDUnifiedAddWizardModel::type() const
{
  return _type;
}

void RuntimeUIFBDUnifiedAddWizardModel::moveType(bool forward)
{
  const uint8_t count = static_cast<uint8_t>(Type::Count);
  uint8_t value = static_cast<uint8_t>(_type);
  value = forward
              ? static_cast<uint8_t>((value + 1U) % count)
              : static_cast<uint8_t>(value == 0 ? count - 1U : value - 1U);
  _type = static_cast<Type>(value);
  _field = 0;
}

const char *RuntimeUIFBDUnifiedAddWizardModel::typeName() const
{
  switch (_type)
  {
  case Type::DigitalInput: return "ENTRADA DI";
  case Type::Not: return "NOT";
  case Type::And2: return "AND 2";
  case Type::Ton: return "TON";
  case Type::DigitalOutput: return "SALIDA DO";
  default: return "?";
  }
}

LogicV2BlockType RuntimeUIFBDUnifiedAddWizardModel::logicType() const
{
  switch (_type)
  {
  case Type::DigitalInput: return LogicV2BlockType::DigitalInput;
  case Type::Not: return LogicV2BlockType::Not;
  case Type::And2: return LogicV2BlockType::And;
  case Type::Ton: return LogicV2BlockType::Ton;
  case Type::DigitalOutput: return LogicV2BlockType::DigitalOutput;
  default: return LogicV2BlockType::DigitalInput;
  }
}

uint8_t RuntimeUIFBDUnifiedAddWizardModel::field() const
{
  return _field;
}

uint8_t RuntimeUIFBDUnifiedAddWizardModel::fieldCount() const
{
  switch (_type)
  {
  case Type::DigitalInput: return 1;
  case Type::Not: return 1;
  case Type::And2: return 2;
  case Type::Ton: return 4;
  case Type::DigitalOutput: return 2;
  default: return 0;
  }
}

void RuntimeUIFBDUnifiedAddWizardModel::moveField(bool forward)
{
  const uint8_t count = fieldCount();
  if (count == 0)
  {
    _field = 0;
    return;
  }
  _field = forward
               ? static_cast<uint8_t>((_field + 1U) % count)
               : static_cast<uint8_t>(_field == 0 ? count - 1U : _field - 1U);
}

const char *RuntimeUIFBDUnifiedAddWizardModel::fieldName(uint8_t field) const
{
  switch (_type)
  {
  case Type::DigitalInput:
    return "RECURSO";
  case Type::Not:
    return "FUENTE";
  case Type::And2:
    return field == 0 ? "IN1" : "IN2";
  case Type::Ton:
    if (field == 0) return "FUENTE";
    if (field == 1) return timeBase() == TimeBase::Hours ? "HORA" :
                           (timeBase() == TimeBase::Minutes ? "MIN" : "SEG");
    if (field == 2) return timeBase() == TimeBase::Hours ? "MIN" :
                           (timeBase() == TimeBase::Minutes ? "SEG" : "CENT");
    return "BASE";
  case Type::DigitalOutput:
    return field == 0 ? "FUENTE" : "RECURSO";
  default:
    return "?";
  }
}

uint16_t RuntimeUIFBDUnifiedAddWizardModel::sourceA() const { return _sourceA; }
uint16_t RuntimeUIFBDUnifiedAddWizardModel::sourceB() const { return _sourceB; }

void RuntimeUIFBDUnifiedAddWizardModel::moveSourceA(bool forward, bool allowOpen)
{
  moveSource(_sourceA, forward, allowOpen);
}

void RuntimeUIFBDUnifiedAddWizardModel::moveSourceB(bool forward, bool allowOpen)
{
  moveSource(_sourceB, forward, allowOpen);
}

uint8_t RuntimeUIFBDUnifiedAddWizardModel::resource() const { return _resource; }

void RuntimeUIFBDUnifiedAddWizardModel::moveResource(bool forward, uint8_t count)
{
  if (count == 0)
  {
    _resource = 0;
    return;
  }
  _resource = forward
                  ? static_cast<uint8_t>((_resource + 1U) % count)
                  : static_cast<uint8_t>(_resource == 0 ? count - 1U : _resource - 1U);
}

uint32_t RuntimeUIFBDUnifiedAddWizardModel::major() const { return _major; }
uint32_t RuntimeUIFBDUnifiedAddWizardModel::minor() const { return _minor; }
RuntimeUIFBDUnifiedAddWizardModel::TimeBase
RuntimeUIFBDUnifiedAddWizardModel::timeBase() const { return _timeBase; }

void RuntimeUIFBDUnifiedAddWizardModel::moveMajor(bool increase)
{
  _major = increase ? (_major >= 99 ? 0 : _major + 1U)
                    : (_major == 0 ? 99 : _major - 1U);
}

void RuntimeUIFBDUnifiedAddWizardModel::moveMinor(bool increase)
{
  const uint32_t maximum = minorMaximum();
  _minor = increase ? (_minor >= maximum ? 0 : _minor + 1U)
                    : (_minor == 0 ? maximum : _minor - 1U);
}

void RuntimeUIFBDUnifiedAddWizardModel::moveTimeBase(bool forward)
{
  uint8_t value = static_cast<uint8_t>(_timeBase);
  value = forward
              ? static_cast<uint8_t>((value + 1U) % 3U)
              : static_cast<uint8_t>(value == 0 ? 2U : value - 1U);
  _timeBase = static_cast<TimeBase>(value);
  const uint32_t maximum = minorMaximum();
  if (_minor > maximum)
  {
    _minor = maximum;
  }
}

uint32_t RuntimeUIFBDUnifiedAddWizardModel::timeMilliseconds() const
{
  switch (_timeBase)
  {
  case TimeBase::Minutes:
    return (_major * 60UL + _minor) * 1000UL;
  case TimeBase::Hours:
    return (_major * 60UL + _minor) * 60000UL;
  case TimeBase::Seconds:
  default:
    return (_major * 100UL + _minor) * 10UL;
  }
}

uint16_t RuntimeUIFBDUnifiedAddWizardModel::timeResource() const
{
  return static_cast<uint16_t>(_timeBase);
}

uint8_t RuntimeUIFBDUnifiedAddWizardModel::inputCount() const
{
  switch (_type)
  {
  case Type::Not:
  case Type::Ton:
  case Type::DigitalOutput:
    return 1;
  case Type::And2:
    return 2;
  default:
    return 0;
  }
}

void RuntimeUIFBDUnifiedAddWizardModel::buildInputs(LogicV2InputLink *destination,
                                                     uint8_t capacity) const
{
  const uint8_t count = inputCount();
  if (destination == nullptr || capacity < count)
  {
    return;
  }

  if (count > 0)
  {
    destination[0] = LogicV2InputLink(_sourceA);
  }
  if (count > 1)
  {
    destination[1] = LogicV2InputLink(_sourceB);
  }
}

void RuntimeUIFBDUnifiedAddWizardModel::formatSource(uint16_t source,
                                                      char *destination,
                                                      size_t capacity) const
{
  if (destination == nullptr || capacity == 0)
  {
    return;
  }
  if (source == JWPLC_LOGIC_V2_SOURCE_OPEN)
  {
    std::snprintf(destination, capacity, "X ABIERTO");
  }
  else if (source == JWPLC_LOGIC_V2_SOURCE_CONST_TRUE)
  {
    std::snprintf(destination, capacity, "HI CONST 1");
  }
  else if (source == JWPLC_LOGIC_V2_SOURCE_CONST_FALSE)
  {
    std::snprintf(destination, capacity, "LO CONST 0");
  }
  else
  {
    std::snprintf(destination, capacity, "B%02u", static_cast<unsigned>(source));
  }
}

void RuntimeUIFBDUnifiedAddWizardModel::formatFieldValue(uint8_t field,
                                                          char *destination,
                                                          size_t capacity) const
{
  if (destination == nullptr || capacity == 0)
  {
    return;
  }

  if (_type == Type::DigitalInput ||
      (_type == Type::DigitalOutput && field == 1))
  {
    std::snprintf(destination, capacity, "%s%u",
                  _type == Type::DigitalInput ? "I0." : "Q0.",
                  static_cast<unsigned>(_resource));
    return;
  }

  if (_type == Type::Not ||
      (_type == Type::Ton && field == 0) ||
      (_type == Type::DigitalOutput && field == 0))
  {
    formatSource(_sourceA, destination, capacity);
    return;
  }

  if (_type == Type::And2)
  {
    formatSource(field == 0 ? _sourceA : _sourceB, destination, capacity);
    return;
  }

  if (_type == Type::Ton)
  {
    if (field == 1)
    {
      std::snprintf(destination, capacity, "<%02lu>",
                    static_cast<unsigned long>(_major));
    }
    else if (field == 2)
    {
      std::snprintf(destination, capacity, "<%02lu>",
                    static_cast<unsigned long>(_minor));
    }
    else
    {
      const char unit = _timeBase == TimeBase::Seconds ? 's' :
                        (_timeBase == TimeBase::Minutes ? 'm' : 'h');
      std::snprintf(destination, capacity, "<%c>", unit);
    }
    return;
  }

  destination[0] = '\0';
}

uint16_t RuntimeUIFBDUnifiedAddWizardModel::sourceCandidateCount(bool allowOpen) const
{
  return static_cast<uint16_t>(_existingBlockCount + (allowOpen ? 3U : 2U));
}

uint16_t RuntimeUIFBDUnifiedAddWizardModel::sourceCandidateAt(uint16_t candidate,
                                                               bool allowOpen) const
{
  if (allowOpen)
  {
    if (candidate == 0) return JWPLC_LOGIC_V2_SOURCE_OPEN;
    --candidate;
  }
  if (candidate == 0) return JWPLC_LOGIC_V2_SOURCE_CONST_TRUE;
  if (candidate == 1) return JWPLC_LOGIC_V2_SOURCE_CONST_FALSE;
  return static_cast<uint16_t>(candidate - 2U);
}

uint16_t RuntimeUIFBDUnifiedAddWizardModel::sourceCandidateIndex(uint16_t source,
                                                                  bool allowOpen) const
{
  const uint16_t offset = allowOpen ? 1U : 0U;
  if (allowOpen && source == JWPLC_LOGIC_V2_SOURCE_OPEN) return 0;
  if (source == JWPLC_LOGIC_V2_SOURCE_CONST_TRUE) return offset;
  if (source == JWPLC_LOGIC_V2_SOURCE_CONST_FALSE) return static_cast<uint16_t>(offset + 1U);
  return static_cast<uint16_t>(offset + 2U + source);
}

void RuntimeUIFBDUnifiedAddWizardModel::moveSource(uint16_t &source,
                                                    bool forward,
                                                    bool allowOpen)
{
  const uint16_t count = sourceCandidateCount(allowOpen);
  if (count == 0) return;
  uint16_t index = sourceCandidateIndex(source, allowOpen);
  if (index >= count) index = 0;
  index = forward ? static_cast<uint16_t>((index + 1U) % count)
                  : static_cast<uint16_t>(index == 0 ? count - 1U : index - 1U);
  source = sourceCandidateAt(index, allowOpen);
}

void RuntimeUIFBDUnifiedAddWizardModel::initializeTypeDefaults()
{
  _field = 0;
  _sourceA = _originBlock < _existingBlockCount
                 ? _originBlock
                 : JWPLC_LOGIC_V2_SOURCE_CONST_FALSE;
  _sourceB = JWPLC_LOGIC_V2_SOURCE_CONST_TRUE;
  _major = 1;
  _minor = 0;
  _timeBase = TimeBase::Seconds;
}

uint32_t RuntimeUIFBDUnifiedAddWizardModel::minorMaximum() const
{
  return _timeBase == TimeBase::Seconds ? 99UL : 59UL;
}
