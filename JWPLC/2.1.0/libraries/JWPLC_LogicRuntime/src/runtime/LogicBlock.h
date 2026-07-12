#ifndef JWPLC_LOGIC_BLOCK_H
#define JWPLC_LOGIC_BLOCK_H

#include <Arduino.h>

static constexpr uint16_t JWPLC_LOGIC_NO_SOURCE = 0xFFFF;
static constexpr uint16_t JWPLC_LOGIC_COMPILED_MAX_BLOCKS = 400;

enum class LogicBlockType : uint8_t
{
  DigitalInput = 0,
  DigitalOutput,
  Not,
  And,
  Or,
  SetReset,
  Ton
};

/**
 * @brief Definición persistible de un bloque lógico.
 *
 * Cada bloque ocupa una posición en el programa. Las fuentes apuntan a la
 * salida de bloques anteriores para mantener un orden de ejecución acíclico y
 * determinista.
 */
struct LogicBlockDefinition
{
  LogicBlockType type;
  uint16_t sourceA;
  uint16_t sourceB;
  uint16_t resource;
  uint32_t parameter;
};

/**
 * @brief Estado temporal de ejecución de un bloque.
 *
 * Este estado vive en RAM. Más adelante solo los campos declarados como
 * retentivos se persistirán en FRAM.
 */
struct LogicBlockState
{
  bool value;
  bool timing;
  uint32_t startedAtMs;
};

#endif
