#ifndef JWPLC_LOGIC_PROGRAM_IMAGE_H
#define JWPLC_LOGIC_PROGRAM_IMAGE_H

#include <Arduino.h>

#include "../runtime/LogicProgram.h"

static constexpr uint32_t JWPLC_LOGIC_IMAGE_MAGIC = 0x524C574AUL; // "JWLR" en little-endian
static constexpr uint16_t JWPLC_LOGIC_IMAGE_FORMAT_VERSION = 1;
static constexpr uint16_t JWPLC_LOGIC_IMAGE_HEADER_SIZE = 64;
static constexpr uint16_t JWPLC_LOGIC_IMAGE_BLOCK_SIZE = 12;
static constexpr uint8_t JWPLC_LOGIC_IMAGE_NAME_BYTES =
    JWPLC_LOGIC_PROGRAM_NAME_BYTES;

struct LogicProgramImageMetadata
{
  uint32_t programId;
  uint32_t generation;
  uint32_t flags;
  uint32_t payloadLength;
  uint32_t payloadCrc32;
  uint16_t blockCount;
};

/**
 * @brief Contenedor propietario para reconstruir un programa desde bytes.
 *
 * No usa memoria dinámica. El LogicProgram devuelto por asProgram() referencia
 * los arreglos internos, por lo que este objeto debe vivir mientras el llamador
 * use directamente ese descriptor. LogicEngine realiza una copia profunda al
 * cargarlo y no conserva referencias hacia este buffer.
 */
struct LogicProgramBuffer
{
  char name[JWPLC_LOGIC_IMAGE_NAME_BYTES + 1];
  LogicBlockDefinition blocks[JWPLC_LOGIC_COMPILED_MAX_BLOCKS];
  uint16_t blockCount;
  LogicProgramImageMetadata metadata;

  LogicProgram asProgram() const
  {
    LogicProgram program = {name, blocks, blockCount};
    return program;
  }
};

#endif
