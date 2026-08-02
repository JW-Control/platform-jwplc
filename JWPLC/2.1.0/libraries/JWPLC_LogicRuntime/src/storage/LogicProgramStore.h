#ifndef JWPLC_LOGIC_PROGRAM_STORE_H
#define JWPLC_LOGIC_PROGRAM_STORE_H

#include <Arduino.h>

#include "LogicByteStorage.h"
#include "LogicProgramCodec.h"
#include "LogicStorageLayout.h"
#include "LogicStorageProfile.h"

static constexpr uint8_t JWPLC_LOGIC_INVALID_SLOT = 0xFF;

enum class LogicProgramSlotState : uint8_t
{
  Empty = 0,
  Writing = 1,
  Verified = 2
};

enum class LogicProgramStoreError : uint8_t
{
  None = 0,
  NullArgument,
  StorageTooSmall,
  InvalidProfile,
  Unformatted,
  FormatFailed,
  ProgramTooLarge,
  CodecFailed,
  ReadFailed,
  WriteFailed,
  InvalidSuperblock,
  InvalidSlotDescriptor,
  ImageCrcMismatch,
  NoValidProgram,
  NoAlternateProgram
};

struct LogicProgramStoreStatus
{
  bool formatted;
  uint8_t activeSlot;
  uint8_t superblockCopy;
  uint8_t lastLoadedSlot;
  uint32_t sequence;
  uint32_t programId;
  uint32_t generation;
};

/**
 * @brief Gestor transaccional de dos slots para imágenes de programas lógicos.
 *
 * El backend puede ser RAM o FRAM. El flujo de guardado nunca modifica el slot
 * activo: escribe y verifica el slot inactivo antes de cambiar el superblock.
 */
class LogicProgramStore
{
public:
  LogicProgramStore();

  bool begin(LogicByteStorage &storage,
             const LogicStorageProfile &profile);
  bool format();

  bool saveProgram(const LogicProgram &program,
                   uint32_t programId,
                   uint32_t generation,
                   uint32_t flags,
                   uint8_t *scratch,
                   size_t scratchCapacity);

  bool loadActive(LogicProgramBuffer &destination,
                  uint8_t *scratch,
                  size_t scratchCapacity);

  /**
   * @brief Carga y verifica un slot concreto sin activarlo.
   *
   * Esta operación permite validar lógicamente el candidato antes de cambiar
   * el superblock activo. Si tiene éxito, lastLoadedSlot queda actualizado.
   */
  bool loadVerifiedSlot(uint8_t slotIndex,
                        LogicProgramBuffer &destination,
                        uint8_t *scratch,
                        size_t scratchCapacity);

  /**
   * @brief Activa el último slot verificado y cargado.
   *
   * Solo escribe la copia alterna del superblock. No modifica la imagen ni su
   * descriptor, por lo que la activación es atómica respecto al programa.
   */
  bool activateVerifiedSlot(uint8_t slotIndex);

  bool isFormatted() const;
  size_t requiredCapacity() const;
  size_t slotPayloadCapacity() const;
  const LogicProgramStoreStatus &status() const;
  LogicProgramStoreError lastError() const;

  static size_t requiredCapacity(const LogicStorageProfile &profile);
  static const char *errorName(LogicProgramStoreError error);

private:
  static constexpr uint32_t SUPERBLOCK_MAGIC = 0x4253574AUL; // "JWSB"
  static constexpr uint32_t SLOT_MAGIC = 0x4C53574AUL;       // "JWSL"
  static constexpr uint16_t STORE_FORMAT_VERSION = 1;
  static constexpr size_t SUPERBLOCK_SIZE = 32;
  static constexpr size_t SUPERBLOCK_COPIES = 2;
  static constexpr size_t SUPERBLOCK_AREA_SIZE =
      LogicStorageLayout::SUPERBLOCK_AREA_BYTES;
  static constexpr size_t SLOT_DESCRIPTOR_SIZE =
      LogicStorageLayout::SLOT_DESCRIPTOR_BYTES;

  static_assert(SUPERBLOCK_SIZE * SUPERBLOCK_COPIES == SUPERBLOCK_AREA_SIZE,
                "El layout y el gestor discrepan en superblocks");

  struct SuperblockData
  {
    uint32_t sequence;
    uint8_t activeSlot;
    uint32_t programId;
    uint32_t generation;
  };

  struct SlotDescriptorData
  {
    LogicProgramSlotState state;
    uint8_t slotIndex;
    uint32_t generation;
    uint32_t imageLength;
    uint32_t imageCrc32;
    uint32_t programId;
  };

  void resetStatus();
  bool loadLatestSuperblock();
  bool readSuperblock(uint8_t copyIndex, SuperblockData &destination);
  bool writeSuperblock(uint8_t copyIndex, const SuperblockData &source);
  bool readSlotDescriptor(uint8_t slotIndex,
                          SlotDescriptorData &destination);
  bool writeSlotDescriptor(uint8_t slotIndex,
                           const SlotDescriptorData &source);
  bool loadSlotInternal(uint8_t slotIndex,
                        LogicProgramBuffer &destination,
                        uint8_t *scratch,
                        size_t scratchCapacity);

  size_t slotAddress(uint8_t slotIndex) const;
  size_t slotImageAddress(uint8_t slotIndex) const;
  static bool isSequenceNewer(uint32_t candidate, uint32_t reference);
  static void writeU16LE(uint8_t *destination, uint16_t value);
  static void writeU32LE(uint8_t *destination, uint32_t value);
  static uint16_t readU16LE(const uint8_t *source);
  static uint32_t readU32LE(const uint8_t *source);

  LogicByteStorage *_storage;
  const LogicStorageProfile *_profile;
  LogicProgramStoreStatus _status;
  LogicProgramStoreError _lastError;
};

#endif
