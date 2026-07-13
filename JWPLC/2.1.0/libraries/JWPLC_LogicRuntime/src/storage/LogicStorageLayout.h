#ifndef JWPLC_LOGIC_STORAGE_LAYOUT_H
#define JWPLC_LOGIC_STORAGE_LAYOUT_H

#include <Arduino.h>

#include "LogicStorageProfile.h"

/**
 * @brief Región continua dentro de la memoria persistente del runtime.
 */
struct LogicStorageRegion
{
  uint32_t address;
  uint32_t size;

  constexpr LogicStorageRegion(uint32_t addressValue = 0,
                               uint32_t sizeValue = 0)
      : address(addressValue),
        size(sizeValue)
  {
  }

  constexpr uint32_t endExclusive() const
  {
    return address + size;
  }

  constexpr uint32_t lastAddress() const
  {
    return size == 0 ? address : endExclusive() - 1;
  }
};

/**
 * @brief Mapa completo de la FRAM asignada al JWPLC Logic Runtime.
 *
 * El mapa separa el almacenamiento transaccional A/B, la futura zona de
 * retentivos y una reserva explícita. El runtime solo debe apropiarse de estas
 * regiones cuando el usuario habilita de forma expresa el modo persistente.
 */
struct LogicStorageLayout
{
  static constexpr uint32_t SUPERBLOCK_AREA_BYTES = 64;

  uint32_t totalBytes;
  LogicStorageRegion superblocks;
  LogicStorageRegion slotA;
  LogicStorageRegion slotB;
  LogicStorageRegion retain;
  LogicStorageRegion reserved;
  uint16_t maxBlocks;

  constexpr LogicStorageLayout(const LogicStorageProfile &profile)
      : totalBytes(profile.framBytes),
        superblocks(0, profile.dualProgramSlot ? SUPERBLOCK_AREA_BYTES : 0),
        slotA(superblocks.endExclusive(), profile.programSlotBytes),
        slotB(slotA.endExclusive(),
              profile.dualProgramSlot ? profile.programSlotBytes : 0),
        retain(slotB.endExclusive(), profile.retainBytes),
        reserved(retain.endExclusive(),
                 profile.framBytes >= retain.endExclusive()
                     ? profile.framBytes - retain.endExclusive()
                     : 0),
        maxBlocks(profile.maxBlocks)
  {
  }

  constexpr uint32_t programStoreBytes() const
  {
    return slotB.endExclusive();
  }

  constexpr bool isValid() const
  {
    return totalBytes > 0 &&
           superblocks.address == 0 &&
           slotA.address == superblocks.endExclusive() &&
           slotB.address == slotA.endExclusive() &&
           retain.address == slotB.endExclusive() &&
           reserved.address == retain.endExclusive() &&
           reserved.endExclusive() == totalBytes &&
           slotA.size > 0 &&
           slotB.size == slotA.size &&
           maxBlocks > 0;
  }
};

namespace JWPLCLogicStorageLayouts
{
constexpr LogicStorageLayout FRAM_8K(JWPLCLogicStorageProfiles::FRAM_8K);
constexpr LogicStorageLayout FRAM_32K(JWPLCLogicStorageProfiles::FRAM_32K);

static_assert(FRAM_8K.isValid(), "Mapa FRAM 8 KiB invalido");
static_assert(FRAM_8K.superblocks.address == 0x0000,
              "Inicio de superblocks 8 KiB inesperado");
static_assert(FRAM_8K.slotA.address == 0x0040,
              "Inicio de Slot A 8 KiB inesperado");
static_assert(FRAM_8K.slotB.address == 0x0A40,
              "Inicio de Slot B 8 KiB inesperado");
static_assert(FRAM_8K.retain.address == 0x1440,
              "Inicio de retentivos 8 KiB inesperado");
static_assert(FRAM_8K.reserved.address == 0x1A40,
              "Inicio de reserva 8 KiB inesperado");
static_assert(FRAM_8K.reserved.endExclusive() == 0x2000,
              "Fin del mapa 8 KiB inesperado");

static_assert(FRAM_32K.isValid(), "Mapa FRAM 32 KiB invalido");
static_assert(FRAM_32K.reserved.endExclusive() == 0x8000,
              "Fin del mapa 32 KiB inesperado");

inline const LogicStorageLayout &forCapacity(uint32_t framBytes)
{
  if (framBytes >= JWPLCLogicStorageProfiles::FRAM_32K.framBytes)
  {
    return FRAM_32K;
  }

  return FRAM_8K;
}
} // namespace JWPLCLogicStorageLayouts

#endif
