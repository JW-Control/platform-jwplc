#ifndef JWPLC_LOGIC_SUPERBLOCK_INSPECTOR_H
#define JWPLC_LOGIC_SUPERBLOCK_INSPECTOR_H

#include <Arduino.h>

#include "LogicByteStorage.h"
#include "LogicProgramCodec.h"

/**
 * @brief Estado diagnóstico de las dos copias de metadata del gestor A/B.
 *
 * Esta inspección es de solo lectura. No formatea, no repara y no activa slots.
 */
enum class LogicSuperblockHealth : uint8_t
{
  Unknown = 0,
  Unformatted,
  Valid,
  CorruptMetadata,
  ReadFailed
};

struct LogicSuperblockCopyInspection
{
  bool readable;
  bool runtimeEvidence;
  bool valid;
  uint32_t sequence;
  uint8_t activeSlot;
  uint32_t programId;
  uint32_t generation;
};

struct LogicSuperblockInspection
{
  LogicSuperblockHealth health;
  LogicSuperblockCopyInspection copies[2];
  uint8_t selectedCopy;
};

/**
 * @brief Inspector no destructivo de los dos superblocks JWPLC.
 *
 * CORRUPT_METADATA solo se informa cuando existe evidencia reconocible del
 * formato JWPLC en al menos una copia, pero ninguna copia supera la validación
 * completa. Datos ajenos al runtime permanecen clasificados como UNFORMATTED.
 */
class LogicSuperblockInspector
{
public:
  static constexpr uint8_t INVALID_COPY = 0xFF;

  static LogicSuperblockInspection inspect(LogicByteStorage &storage)
  {
    LogicSuperblockInspection result = {};
    result.health = LogicSuperblockHealth::Unknown;
    result.selectedCopy = INVALID_COPY;

    const bool readable0 = inspectCopy(storage, 0, result.copies[0]);
    const bool readable1 = inspectCopy(storage, 1, result.copies[1]);

    const bool valid0 = readable0 && result.copies[0].valid;
    const bool valid1 = readable1 && result.copies[1].valid;

    if (valid0 || valid1)
    {
      result.health = LogicSuperblockHealth::Valid;
      result.selectedCopy = valid1 &&
                                    (!valid0 ||
                                     isSequenceNewer(result.copies[1].sequence,
                                                     result.copies[0].sequence))
                                ? 1
                                : 0;
      return result;
    }

    if (!readable0 || !readable1)
    {
      result.health = LogicSuperblockHealth::ReadFailed;
      return result;
    }

    const bool runtimeEvidence =
        result.copies[0].runtimeEvidence ||
        result.copies[1].runtimeEvidence;

    result.health = runtimeEvidence
                        ? LogicSuperblockHealth::CorruptMetadata
                        : LogicSuperblockHealth::Unformatted;
    return result;
  }

  static const char *healthName(LogicSuperblockHealth health)
  {
    switch (health)
    {
    case LogicSuperblockHealth::Unknown:
      return "UNKNOWN";
    case LogicSuperblockHealth::Unformatted:
      return "UNFORMATTED";
    case LogicSuperblockHealth::Valid:
      return "VALID";
    case LogicSuperblockHealth::CorruptMetadata:
      return "CORRUPT_METADATA";
    case LogicSuperblockHealth::ReadFailed:
      return "READ_FAILED";
    default:
      return "UNKNOWN";
    }
  }

private:
  static constexpr uint32_t SUPERBLOCK_MAGIC = 0x4253574AUL; // "JWSB"
  static constexpr uint16_t STORE_FORMAT_VERSION = 1;
  static constexpr size_t SUPERBLOCK_SIZE = 32;
  static constexpr size_t SUPERBLOCK_COPIES = 2;
  static constexpr uint8_t INVALID_SLOT = 0xFF;

  static bool inspectCopy(LogicByteStorage &storage,
                          uint8_t copyIndex,
                          LogicSuperblockCopyInspection &destination)
  {
    destination = {};
    destination.activeSlot = INVALID_SLOT;

    if (copyIndex >= SUPERBLOCK_COPIES)
    {
      return false;
    }

    uint8_t bytes[SUPERBLOCK_SIZE] = {};
    const size_t address =
        static_cast<size_t>(copyIndex) * SUPERBLOCK_SIZE;

    if (!storage.read(address, bytes, sizeof(bytes)))
    {
      return false;
    }

    destination.readable = true;

    const uint32_t magic = readU32LE(bytes + 0);
    const uint16_t version = readU16LE(bytes + 4);
    const uint16_t headerSize = readU16LE(bytes + 6);

    destination.runtimeEvidence =
        magic == SUPERBLOCK_MAGIC ||
        (version == STORE_FORMAT_VERSION &&
         headerSize == SUPERBLOCK_SIZE);

    const uint8_t activeSlot = bytes[12];
    const bool slotValid =
        activeSlot == INVALID_SLOT || activeSlot <= 1;
    const uint32_t expectedCrc = readU32LE(bytes + 28);
    const bool crcValid =
        LogicProgramCodec::crc32(bytes, 28) == expectedCrc;

    destination.valid =
        magic == SUPERBLOCK_MAGIC &&
        version == STORE_FORMAT_VERSION &&
        headerSize == SUPERBLOCK_SIZE &&
        slotValid &&
        crcValid;

    if (destination.valid)
    {
      destination.sequence = readU32LE(bytes + 8);
      destination.activeSlot = activeSlot;
      destination.programId = readU32LE(bytes + 16);
      destination.generation = readU32LE(bytes + 20);
    }

    return true;
  }

  static bool isSequenceNewer(uint32_t candidate,
                              uint32_t reference)
  {
    return static_cast<int32_t>(candidate - reference) > 0;
  }

  static uint16_t readU16LE(const uint8_t *source)
  {
    return static_cast<uint16_t>(source[0]) |
           (static_cast<uint16_t>(source[1]) << 8);
  }

  static uint32_t readU32LE(const uint8_t *source)
  {
    return static_cast<uint32_t>(source[0]) |
           (static_cast<uint32_t>(source[1]) << 8) |
           (static_cast<uint32_t>(source[2]) << 16) |
           (static_cast<uint32_t>(source[3]) << 24);
  }
};

#endif