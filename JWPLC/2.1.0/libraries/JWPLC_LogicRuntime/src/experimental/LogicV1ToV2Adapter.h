#ifndef JWPLC_LOGIC_V1_TO_V2_ADAPTER_H
#define JWPLC_LOGIC_V1_TO_V2_ADAPTER_H

#include <Arduino.h>

#include "../runtime/LogicProgram.h"
#include "LogicVariableInputPrototype.h"

/**
 * @brief Error de conversión del formato RAM v1 al modelo RAM v2.
 *
 * Esta fase cubre DigitalInput, DigitalOutput, NOT, AND y OR. SetReset y Ton
 * se incorporarán después de añadir sus estados equivalentes al motor v2.
 */
enum class LogicV1ToV2AdapterError : uint8_t
{
  None = 0,
  NullArgument,
  EmptyProgram,
  TooManyBlocks,
  BlockBufferTooSmall,
  LinkBufferTooSmall,
  UnsupportedBlockType,
  UnsupportedFlags,
  MissingSource,
  SourceNotPrevious
};

class LogicV1ToV2Adapter
{
public:
  /**
   * @brief Convierte un programa v1 soportado a bloques y enlaces v2.
   *
   * El destino utiliza buffers proporcionados por la aplicación. No reserva
   * memoria dinámica y no modifica el programa de origen.
   */
  static bool convert(const LogicProgram &source,
                      LogicV2BlockRecord *destinationBlocks,
                      size_t destinationBlockCapacity,
                      LogicV2InputLink *destinationLinks,
                      size_t destinationLinkCapacity,
                      LogicV2Program &destination,
                      LogicV1ToV2AdapterError &error);

  /** @brief Cantidad exacta de enlaces requeridos por el subconjunto soportado. */
  static bool requiredLinkCount(const LogicProgram &source,
                                size_t &requiredLinks,
                                LogicV1ToV2AdapterError &error);

  static const char *errorName(LogicV1ToV2AdapterError error);

private:
  static bool sourceIsValid(uint16_t source, uint16_t currentBlockIndex);
  static uint8_t inputCountForType(LogicBlockType type);
  static bool mapType(LogicBlockType sourceType,
                      LogicV2BlockType &destinationType);
};

#endif