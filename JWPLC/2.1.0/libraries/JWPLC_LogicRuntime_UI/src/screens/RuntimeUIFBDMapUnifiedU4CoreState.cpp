#define JWPLC_UNIFIED_STRONG_IMPLEMENTATION
#include "RuntimeUIFBDMapUnifiedU4Internal.h"

#include <cstdio>
#include <cstring>

#include "../widgets/RuntimeUIWidgets.h"

using namespace JWPLCLogicRuntimeUIWidgets;

namespace JWPLCUnifiedU4
{
U4State gU4;
void resetU4(const RuntimeUIFBDMapUnified *owner,
             RuntimeUIV2ReadModel *model)
{
  gU4 = U4State{};
  gU4.owner = owner;
  gU4.model = model;
}

void ensureU4(const RuntimeUIFBDMapUnified *owner,
              RuntimeUIV2ReadModel *model)
{
  if (gU4.owner != owner || gU4.model != model)
  {
    resetU4(owner, model);
  }
}

const char *typeName(U4Type type)
{
  switch (type)
  {
  case U4Type::DigitalInput:
    return "ENTRADA DI";
  case U4Type::Not:
    return "NOT";
  case U4Type::And2:
    return "AND 2";
  case U4Type::Ton:
    return "TON";
  case U4Type::DigitalOutput:
    return "SALIDA DO";
  default:
    return "?";
  }
}

const char *typeShort(U4Type type)
{
  switch (type)
  {
  case U4Type::DigitalInput:
    return "DI";
  case U4Type::Not:
    return "NOT";
  case U4Type::And2:
    return "AND";
  case U4Type::Ton:
    return "TON";
  case U4Type::DigitalOutput:
    return "DO";
  default:
    return "?";
  }
}

LogicV2BlockType logicType(U4Type type)
{
  switch (type)
  {
  case U4Type::DigitalInput:
    return LogicV2BlockType::DigitalInput;
  case U4Type::Not:
    return LogicV2BlockType::Not;
  case U4Type::And2:
    return LogicV2BlockType::And;
  case U4Type::Ton:
    return LogicV2BlockType::Ton;
  case U4Type::DigitalOutput:
    return LogicV2BlockType::DigitalOutput;
  default:
    return LogicV2BlockType::DigitalInput;
  }
}

uint8_t sourceCountForType(U4Type type)
{
  switch (type)
  {
  case U4Type::Not:
  case U4Type::Ton:
  case U4Type::DigitalOutput:
    return 1;
  case U4Type::And2:
    return 2;
  default:
    return 0;
  }
}

uint8_t parameterCountForType(U4Type type)
{
  return (type == U4Type::DigitalInput ||
          type == U4Type::Ton ||
          type == U4Type::DigitalOutput)
             ? 1
             : 0;
}

const char *sourceRole(U4Type type, uint8_t input)
{
  if (type == U4Type::Ton)
  {
    return "Trg";
  }
  if (type == U4Type::And2)
  {
    return input == 0 ? "IN1" : "IN2";
  }
  return "IN";
}

const char *parameterName(U4Type type)
{
  return type == U4Type::Ton ? "T" : "RECURSO";
}

uint16_t &sourceReference(uint8_t input)
{
  return input == 0 ? gU4.sourceA : gU4.sourceB;
}

uint16_t sourceValue(uint8_t input)
{
  return input == 0 ? gU4.sourceA : gU4.sourceB;
}

uint16_t sourceCandidateCount(const RuntimeUIV2ReadModel *model)
{
  return static_cast<uint16_t>(
      (model != nullptr ? model->blockCount() : 0U) + 3U);
}

uint16_t sourceCandidateAt(const RuntimeUIV2ReadModel *model,
                           uint16_t candidate)
{
  (void)model;
  if (candidate == 0)
    return JWPLC_LOGIC_V2_SOURCE_OPEN;
  if (candidate == 1)
    return JWPLC_LOGIC_V2_SOURCE_CONST_TRUE;
  if (candidate == 2)
    return JWPLC_LOGIC_V2_SOURCE_CONST_FALSE;
  return static_cast<uint16_t>(candidate - 3U);
}

uint16_t sourceCandidateIndex(const RuntimeUIV2ReadModel *model,
                              uint16_t source)
{
  if (source == JWPLC_LOGIC_V2_SOURCE_OPEN)
    return 0;
  if (source == JWPLC_LOGIC_V2_SOURCE_CONST_TRUE)
    return 1;
  if (source == JWPLC_LOGIC_V2_SOURCE_CONST_FALSE)
    return 2;
  if (model != nullptr && source < model->blockCount())
    return static_cast<uint16_t>(source + 3U);
  return 0;
}

void moveSource(RuntimeUIV2ReadModel *model,
                uint16_t &source,
                bool forward)
{
  const uint16_t count = sourceCandidateCount(model);
  if (count == 0)
    return;

  uint16_t index = sourceCandidateIndex(model, source);
  index = forward
              ? static_cast<uint16_t>((index + 1U) % count)
              : (index == 0 ? static_cast<uint16_t>(count - 1U)
                            : static_cast<uint16_t>(index - 1U));
  source = sourceCandidateAt(model, index);
}

void formatSource(const RuntimeUIV2ReadModel *model,
                  uint16_t source,
                  char *destination,
                  size_t capacity)
{
  if (destination == nullptr || capacity == 0)
    return;

  if (source == JWPLC_LOGIC_V2_SOURCE_OPEN)
  {
    std::snprintf(destination, capacity, "ABIERTO");
    return;
  }
  if (source == JWPLC_LOGIC_V2_SOURCE_CONST_TRUE)
  {
    std::snprintf(destination, capacity, "HI CONST 1");
    return;
  }
  if (source == JWPLC_LOGIC_V2_SOURCE_CONST_FALSE)
  {
    std::snprintf(destination, capacity, "LO CONST 0");
    return;
  }

  const LogicV2BlockRecord *definition =
      model != nullptr ? model->block(source) : nullptr;
  std::snprintf(destination,
                capacity,
                "B%02u %s",
                static_cast<unsigned>(source),
                definition != nullptr ? model->typeShort(definition->type) : "?");
}

uint8_t digitalInputCount(const RuntimeUIV2ReadModel *model)
{
  const LogicV2EnginePrototype *engine = model != nullptr ? model->engine() : nullptr;
  return engine != nullptr ? engine->digitalInputCount() : 0;
}

uint8_t digitalOutputCount(const RuntimeUIV2ReadModel *model)
{
  const LogicV2EnginePrototype *engine = model != nullptr ? model->engine() : nullptr;
  return engine != nullptr ? engine->digitalOutputCount() : 0;
}

bool outputResourceUsed(const RuntimeUIV2ReadModel *model, uint8_t resource)
{
  if (model == nullptr)
    return false;

  const uint16_t count = model->blockCount();
  for (uint16_t block = 0; block < count; ++block)
  {
    const LogicV2BlockRecord *definition = model->block(block);
    if (definition != nullptr &&
        definition->type == LogicV2BlockType::DigitalOutput &&
        definition->resource == resource)
    {
      return true;
    }
  }
  return false;
}

uint8_t firstAvailableOutput(const RuntimeUIV2ReadModel *model)
{
  const uint8_t count = digitalOutputCount(model);
  for (uint8_t resource = 0; resource < count; ++resource)
  {
    if (!outputResourceUsed(model, resource))
      return resource;
  }
  return 0xFFU;
}

void moveResource(RuntimeUIV2ReadModel *model, bool forward)
{
  if (gU4.type == U4Type::DigitalInput)
  {
    const uint8_t count = digitalInputCount(model);
    if (count == 0)
    {
      gU4.resource = 0xFFU;
      return;
    }
    if (gU4.resource >= count)
      gU4.resource = 0;
    else
      gU4.resource = forward
                         ? static_cast<uint8_t>((gU4.resource + 1U) % count)
                         : (gU4.resource == 0
                                ? static_cast<uint8_t>(count - 1U)
                                : static_cast<uint8_t>(gU4.resource - 1U));
    return;
  }

  const uint8_t count = digitalOutputCount(model);
  if (count == 0)
  {
    gU4.resource = 0xFFU;
    return;
  }

  uint8_t candidate = gU4.resource < count
                          ? gU4.resource
                          : firstAvailableOutput(model);
  for (uint8_t step = 0; step < count; ++step)
  {
    candidate = forward
                    ? static_cast<uint8_t>((candidate + 1U) % count)
                    : (candidate == 0
                           ? static_cast<uint8_t>(count - 1U)
                           : static_cast<uint8_t>(candidate - 1U));
    if (!outputResourceUsed(model, candidate))
    {
      gU4.resource = candidate;
      return;
    }
  }
  gU4.resource = 0xFFU;
}

} // namespace JWPLCUnifiedU4
