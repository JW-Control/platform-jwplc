#ifndef JWPLC_LOGIC_STORAGE_PROFILE_H
#define JWPLC_LOGIC_STORAGE_PROFILE_H

#include <Arduino.h>

#include "../runtime/LogicBlock.h"

/**
 * @brief Límites de almacenamiento disponibles para un programa lógico.
 *
 * El motor usa el mismo formato y API para todas las capacidades. Solo cambian
 * los límites del perfil seleccionado. maxBlocks siempre es el menor valor
 * entre la capacidad física del perfil y la capacidad reservada por el build.
 */
struct LogicStorageProfile
{
  uint32_t framBytes;
  uint32_t programSlotBytes;
  uint32_t retainBytes;
  uint16_t maxBlocks;
  bool dualProgramSlot;

  constexpr LogicStorageProfile(uint32_t framBytesValue = 0,
                                uint32_t programSlotBytesValue = 0,
                                uint32_t retainBytesValue = 0,
                                uint16_t maxBlocksValue = 0,
                                bool dualProgramSlotValue = false)
      : framBytes(framBytesValue),
        programSlotBytes(programSlotBytesValue),
        retainBytes(retainBytesValue),
        maxBlocks(maxBlocksValue),
        dualProgramSlot(dualProgramSlotValue)
  {
  }
};

namespace JWPLCLogicStorageProfiles
{
constexpr uint16_t FRAM_8K_PHYSICAL_MAX_BLOCKS = 100;
constexpr uint16_t FRAM_32K_PHYSICAL_MAX_BLOCKS = 400;

constexpr uint16_t compiledBlockLimit(uint16_t physicalLimit)
{
  return JWPLC_LOGIC_COMPILED_MAX_BLOCKS < physicalLimit
             ? static_cast<uint16_t>(JWPLC_LOGIC_COMPILED_MAX_BLOCKS)
             : physicalLimit;
}

constexpr LogicStorageProfile NONE(0, 0, 0, 0, false);
constexpr LogicStorageProfile FRAM_8K(
    8UL * 1024UL,
    2560UL,
    1536UL,
    compiledBlockLimit(FRAM_8K_PHYSICAL_MAX_BLOCKS),
    true);
constexpr LogicStorageProfile FRAM_32K(
    32UL * 1024UL,
    12UL * 1024UL,
    4UL * 1024UL,
    compiledBlockLimit(FRAM_32K_PHYSICAL_MAX_BLOCKS),
    true);

static_assert(FRAM_8K.maxBlocks <= FRAM_8K_PHYSICAL_MAX_BLOCKS,
              "El perfil 8 KiB excede su capacidad física");
static_assert(FRAM_32K.maxBlocks <= FRAM_32K_PHYSICAL_MAX_BLOCKS,
              "El perfil 32 KiB excede su capacidad física");

inline const LogicStorageProfile &forCapacity(uint32_t framBytes)
{
  if (framBytes >= FRAM_32K.framBytes)
  {
    return FRAM_32K;
  }

  if (framBytes >= FRAM_8K.framBytes)
  {
    return FRAM_8K;
  }

  return NONE;
}
} // namespace JWPLCLogicStorageProfiles

#endif
