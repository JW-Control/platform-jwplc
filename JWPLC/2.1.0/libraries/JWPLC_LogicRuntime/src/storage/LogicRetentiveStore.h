#ifndef JWPLC_LOGIC_RETENTIVE_STORE_H
#define JWPLC_LOGIC_RETENTIVE_STORE_H

#include <Arduino.h>

#include "LogicByteStorage.h"
#include "LogicProgramCodec.h"
#include "LogicStorageLayout.h"

static constexpr uint8_t JWPLC_LOGIC_INVALID_RETENTIVE_COPY = 0xFF;

enum class LogicRetentiveStoreError : uint8_t
{
  None = 0,
  NullArgument,
  InvalidRegion,
  StorageTooSmall,
  InvalidBitmap,
  ReadFailed,
  WriteFailed,
  VerificationFailed,
  NoMatchingSnapshot
};

struct LogicRetentiveStoreStatus
{
  bool hasSnapshot;
  uint8_t activeCopy;
  uint8_t lastLoadedCopy;
  uint32_t sequence;
  uint32_t programId;
  uint32_t generation;
  uint16_t blockCount;
  uint16_t bitmapBytes;
};

/**
 * @brief Gestor A/B para el bitmap de estados retentivos.
 *
 * Cada copia es autocontenida. La firma JWRT se escribe al final como commit,
 * de modo que un corte parcial no invalida la copia activa anterior.
 */
class LogicRetentiveStore
{
public:
  static constexpr uint16_t FORMAT_MAX_BLOCKS = 400;
  static constexpr size_t MAX_BITMAP_BYTES =
      (FORMAT_MAX_BLOCKS + 7U) / 8U;
  static constexpr size_t HEADER_BYTES = 64;
  static constexpr uint8_t COPY_COUNT = 2;

  LogicRetentiveStore();

  bool begin(LogicByteStorage &storage,
             const LogicStorageRegion &region);

  bool save(uint32_t programId,
            uint32_t generation,
            uint16_t blockCount,
            const uint8_t *bitmap,
            size_t bitmapBytes);

  bool load(uint32_t programId,
            uint32_t generation,
            uint16_t blockCount,
            uint8_t *destination,
            size_t destinationCapacity,
            size_t &loadedBytes);

  bool hasSnapshot() const;
  size_t copyBytes() const;
  size_t payloadCapacity() const;
  const LogicRetentiveStoreStatus &status() const;
  LogicRetentiveStoreError lastError() const;

  static size_t requiredBitmapBytes(uint16_t blockCount);
  static const char *errorName(LogicRetentiveStoreError error);

private:
  static constexpr uint32_t RECORD_MAGIC = 0x5452574AUL; // "JWRT"
  static constexpr uint16_t FORMAT_VERSION = 1;
  static constexpr size_t OFFSET_MAGIC = 0;
  static constexpr size_t OFFSET_VERSION = 4;
  static constexpr size_t OFFSET_HEADER_SIZE = 6;
  static constexpr size_t OFFSET_SEQUENCE = 8;
  static constexpr size_t OFFSET_PROGRAM_ID = 12;
  static constexpr size_t OFFSET_GENERATION = 16;
  static constexpr size_t OFFSET_BLOCK_COUNT = 20;
  static constexpr size_t OFFSET_BITMAP_BYTES = 22;
  static constexpr size_t OFFSET_BITMAP_CRC = 24;
  static constexpr size_t OFFSET_HEADER_CRC = 28;
  static constexpr size_t OFFSET_FLAGS = 32;

  struct RecordData
  {
    bool valid;
    uint32_t sequence;
    uint32_t programId;
    uint32_t generation;
    uint16_t blockCount;
    uint16_t bitmapBytes;
    uint8_t bitmap[MAX_BITMAP_BYTES];
  };

  void resetStatus();
  bool refreshStatus();
  bool readRecord(uint8_t copyIndex,
                  RecordData &destination,
                  bool &readFailed) const;
  bool writeRecord(uint8_t copyIndex,
                   const RecordData &record);
  size_t copyAddress(uint8_t copyIndex) const;

  static bool identityMatches(const RecordData &record,
                              uint32_t programId,
                              uint32_t generation,
                              uint16_t blockCount,
                              size_t bitmapBytes);
  static bool isSequenceNewer(uint32_t candidate,
                              uint32_t reference);
  static void writeU16LE(uint8_t *destination, uint16_t value);
  static void writeU32LE(uint8_t *destination, uint32_t value);
  static uint16_t readU16LE(const uint8_t *source);
  static uint32_t readU32LE(const uint8_t *source);

  LogicByteStorage *_storage;
  LogicStorageRegion _region;
  size_t _copyBytes;
  LogicRetentiveStoreStatus _status;
  LogicRetentiveStoreError _lastError;
};

#endif