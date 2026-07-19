#ifndef JWPLC_LOGIC_RUNTIME_V2_H
#define JWPLC_LOGIC_RUNTIME_V2_H

#include <Arduino.h>

#include "experimental/LogicVariableInputPrototype.h"
#include "experimental/LogicV2EnginePrototype.h"

/**
 * @brief Contrato público explícito del motor lógico RAM v2.
 *
 * Este encabezado evita que sketches y la UI v2 dependan accidentalmente del
 * runtime v1 con FRAM/codec. Los nombres Prototype se conservan por
 * compatibilidad mientras se estabiliza la API; los aliases siguientes son los
 * nombres recomendados para código nuevo.
 */
namespace JWPLCLogicV2
{
  static constexpr uint16_t CONTRACT_MAJOR = 1;
  static constexpr uint16_t CONTRACT_MINOR = 0;
  static constexpr uint32_t CONTRACT_VERSION =
      (static_cast<uint32_t>(CONTRACT_MAJOR) << 16U) |
      static_cast<uint32_t>(CONTRACT_MINOR);

  static constexpr uint16_t RECORD_SCHEMA_VERSION = 1;
  static constexpr size_t BLOCK_RECORD_BYTES = sizeof(LogicV2BlockRecord);
  static constexpr size_t INPUT_LINK_BYTES = sizeof(LogicV2InputLink);

  using Engine = LogicV2EnginePrototype;
  using Program = LogicV2Program;
  using BlockRecord = LogicV2BlockRecord;
  using InputLink = LogicV2InputLink;
  using BlockType = LogicV2BlockType;
  using EngineState = LogicV2EngineState;
  using EngineError = LogicV2EngineError;
  using ValidationError = LogicV2PrototypeError;

  static_assert(BLOCK_RECORD_BYTES == 12,
                "El contrato v2 requiere bloques de 12 bytes");
  static_assert(INPUT_LINK_BYTES == 2,
                "El contrato v2 requiere enlaces de 2 bytes");
}

#endif