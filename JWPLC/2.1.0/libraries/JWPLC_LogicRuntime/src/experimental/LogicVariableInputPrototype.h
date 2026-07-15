#ifndef JWPLC_LOGIC_VARIABLE_INPUT_PROTOTYPE_H
#define JWPLC_LOGIC_VARIABLE_INPUT_PROTOTYPE_H

#include <Arduino.h>

#include "../runtime/LogicBlock.h"

/**
 * @brief Prototipo aislado del modelo de bloques v2.
 *
 * Esta API todavía no reemplaza LogicBlockDefinition ni el codec v1. Sirve
 * para validar en RAM entradas variables, negación por pin y compuertas
 * compatibles con el modelo mental de LOGO! antes de modificar el runtime
 * estable o la FRAM.
 */

static constexpr uint8_t JWPLC_LOGIC_V2_MAX_INPUTS_PER_BLOCK = 8;
static constexpr uint16_t JWPLC_LOGIC_V2_LINK_INVERTED = 0x8000U;
static constexpr uint16_t JWPLC_LOGIC_V2_LINK_SOURCE_MASK = 0x7FFFU;
static constexpr uint16_t JWPLC_LOGIC_V2_SOURCE_CONST_TRUE = 0x7FFDU;
static constexpr uint16_t JWPLC_LOGIC_V2_SOURCE_CONST_FALSE = 0x7FFEU;
static constexpr uint16_t JWPLC_LOGIC_V2_SOURCE_OPEN = 0x7FFFU;
static constexpr uint16_t JWPLC_LOGIC_V2_MAX_BLOCK_SOURCE = 0x7FFCU;

#ifndef JWPLC_LOGIC_V2_COMPILED_MAX_LINKS_CONFIG
#if JWPLC_LOGIC_COMPILED_MAX_BLOCKS_CONFIG > 100
#define JWPLC_LOGIC_V2_COMPILED_MAX_LINKS_CONFIG 2048
#else
#define JWPLC_LOGIC_V2_COMPILED_MAX_LINKS_CONFIG 512
#endif
#endif

static_assert(JWPLC_LOGIC_V2_COMPILED_MAX_LINKS_CONFIG > 0,
              "El motor v2 requiere al menos un enlace compilado");
static_assert(JWPLC_LOGIC_V2_COMPILED_MAX_LINKS_CONFIG <= 2048,
              "El prototipo v2 admite como maximo 2048 enlaces compilados");

static constexpr uint16_t JWPLC_LOGIC_V2_COMPILED_MAX_BLOCKS =
    JWPLC_LOGIC_COMPILED_MAX_BLOCKS;
static constexpr uint16_t JWPLC_LOGIC_V2_COMPILED_MAX_LINKS =
    static_cast<uint16_t>(JWPLC_LOGIC_V2_COMPILED_MAX_LINKS_CONFIG);

enum class LogicV2BlockType : uint8_t
{
  DigitalInput = 0,
  ConstantFalse,
  ConstantTrue,
  Not,
  And,
  Or,
  Nand,
  Nor,
  Xor
};

struct LogicV2BlockRecord
{
  LogicV2BlockType type;
  uint8_t flags;
  uint16_t firstInput;
  uint8_t inputCount;
  uint8_t reserved;
  uint16_t resource;
  uint32_t parameter;

  constexpr LogicV2BlockRecord(
      LogicV2BlockType typeValue = LogicV2BlockType::DigitalInput,
      uint16_t firstInputValue = 0,
      uint8_t inputCountValue = 0,
      uint16_t resourceValue = 0,
      uint32_t parameterValue = 0,
      uint8_t flagsValue = 0)
      : type(typeValue),
        flags(flagsValue),
        firstInput(firstInputValue),
        inputCount(inputCountValue),
        reserved(0),
        resource(resourceValue),
        parameter(parameterValue)
  {
  }
};

static_assert(sizeof(LogicV2BlockRecord) == 12,
              "LogicV2BlockRecord debe conservar 12 bytes");

struct LogicV2InputLink
{
  uint16_t encodedSource;

  constexpr explicit LogicV2InputLink(uint16_t encodedValue =
                                          JWPLC_LOGIC_V2_SOURCE_OPEN)
      : encodedSource(encodedValue)
  {
  }

  static constexpr LogicV2InputLink block(uint16_t blockIndex,
                                           bool inverted = false)
  {
    return LogicV2InputLink(
        static_cast<uint16_t>((blockIndex & JWPLC_LOGIC_V2_LINK_SOURCE_MASK) |
                              (inverted ? JWPLC_LOGIC_V2_LINK_INVERTED : 0U)));
  }

  static constexpr LogicV2InputLink open(bool inverted = false)
  {
    return LogicV2InputLink(
        static_cast<uint16_t>(JWPLC_LOGIC_V2_SOURCE_OPEN |
                              (inverted ? JWPLC_LOGIC_V2_LINK_INVERTED : 0U)));
  }

  static constexpr LogicV2InputLink constantFalse(bool inverted = false)
  {
    return LogicV2InputLink(
        static_cast<uint16_t>(JWPLC_LOGIC_V2_SOURCE_CONST_FALSE |
                              (inverted ? JWPLC_LOGIC_V2_LINK_INVERTED : 0U)));
  }

  static constexpr LogicV2InputLink constantTrue(bool inverted = false)
  {
    return LogicV2InputLink(
        static_cast<uint16_t>(JWPLC_LOGIC_V2_SOURCE_CONST_TRUE |
                              (inverted ? JWPLC_LOGIC_V2_LINK_INVERTED : 0U)));
  }

  constexpr uint16_t source() const
  {
    return static_cast<uint16_t>(encodedSource &
                                 JWPLC_LOGIC_V2_LINK_SOURCE_MASK);
  }

  constexpr bool inverted() const
  {
    return (encodedSource & JWPLC_LOGIC_V2_LINK_INVERTED) != 0;
  }
};

static_assert(sizeof(LogicV2InputLink) == 2,
              "LogicV2InputLink debe ocupar 2 bytes");

struct LogicV2Program
{
  const LogicV2BlockRecord *blocks;
  uint16_t blockCount;
  const LogicV2InputLink *links;
  uint16_t linkCount;
};

enum class LogicV2PrototypeError : uint8_t
{
  None = 0,
  NullArgument,
  EmptyProgram,
  TooManyBlocks,
  TooManyLinks,
  InvalidBlockType,
  InvalidBlockFlags,
  InvalidInputCount,
  InputRangeOutOfBounds,
  InvalidSourceEncoding,
  SourceNotPrevious,
  ResourceOutOfRange,
  OpenInputNotAllowed,
  NullDigitalInputs,
  OutputBufferTooSmall
};

class LogicVariableInputPrototype
{
public:
  static LogicV2PrototypeError validate(const LogicV2Program &program,
                                        uint16_t maxBlocks,
                                        uint16_t maxLinks,
                                        uint8_t digitalInputCount);

  /**
   * @brief Valida y evalua un programa en una sola llamada.
   *
   * Es util para pruebas aisladas. Un motor cargado debe validar una vez y usar
   * evaluateValidated() durante cada scan para no repetir todo el validador.
   */
  static bool evaluate(const LogicV2Program &program,
                       const bool *digitalInputs,
                       uint8_t digitalInputCount,
                       bool *blockValues,
                       size_t blockValueCapacity,
                       LogicV2PrototypeError &error);

  /**
   * @brief Evalua un programa que ya fue validado durante la carga.
   */
  static bool evaluateValidated(const LogicV2Program &program,
                                const bool *digitalInputs,
                                uint8_t digitalInputCount,
                                bool *blockValues,
                                size_t blockValueCapacity,
                                LogicV2PrototypeError &error);

  static constexpr size_t requiredImageBytes(uint16_t blockCount,
                                              uint16_t linkCount,
                                              uint16_t headerBytes = 64)
  {
    return static_cast<size_t>(headerBytes) +
           static_cast<size_t>(blockCount) * sizeof(LogicV2BlockRecord) +
           static_cast<size_t>(linkCount) * sizeof(LogicV2InputLink);
  }

  static const char *errorName(LogicV2PrototypeError error);

private:
  static bool isVariableGate(LogicV2BlockType type);
  static bool isKnownType(LogicV2BlockType type);
  static bool neutralValue(LogicV2BlockType type);
  static bool resolveInput(const LogicV2InputLink &link,
                           LogicV2BlockType consumerType,
                           const bool *blockValues,
                           bool &value);
};

#endif