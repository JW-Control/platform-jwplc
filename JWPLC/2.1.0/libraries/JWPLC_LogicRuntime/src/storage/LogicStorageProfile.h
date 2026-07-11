#ifndef JWPLC_LOGIC_STORAGE_PROFILE_H
#define JWPLC_LOGIC_STORAGE_PROFILE_H

#include <Arduino.h>

/**
 * @brief Límites de almacenamiento disponibles para un programa lógico.
 *
 * El motor usa el mismo formato y API para todas las capacidades. Solo cambian
 * los límites del perfil seleccionado.
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
constexpr LogicStorageProfile NONE(0, 0, 0, 0, false);
constexpr LogicStorageProfile FRAM_8K(8UL * 1024UL,
                                      2560UL,
                                      1536UL,
                                      100,
                                      true);
constexpr LogicStorageProfile FRAM_32K(32UL * 1024UL,
                                       12UL * 1024UL,
                                       4UL * 1024UL,
                                       400,
                                       true);

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
