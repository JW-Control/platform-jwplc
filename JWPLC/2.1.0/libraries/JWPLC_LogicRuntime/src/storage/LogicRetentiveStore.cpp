#include "LogicRetentiveStore.h"

#include <cstring>

LogicRetentiveStore::LogicRetentiveStore()
    : _storage(nullptr),
      _region(),
      _copyBytes(0),
      _status{},
      _lastError(LogicRetentiveStoreError::None)
{
  resetStatus();
}

void LogicRetentiveStore::resetStatus()
{
  _status.hasSnapshot = false;
  _status.activeCopy = JWPLC_LOGIC_INVALID_RETENTIVE_COPY;
  _status.lastLoadedCopy = JWPLC_LOGIC_INVALID_RETENTIVE_COPY;
  _status.sequence = 0;
  _status.programId = 0;
  _status.generation = 0;
  _status.blockCount = 0;
  _status.bitmapBytes = 0;
}

size_t LogicRetentiveStore::requiredBitmapBytes(uint16_t blockCount)
{
  if (blockCount == 0 || blockCount > FORMAT_MAX_BLOCKS)
  {
    return 0;
  }

  return (static_cast<size_t>(blockCount) + 7U) / 8U;
}

bool LogicRetentiveStore::begin(LogicByteStorage &storage,
                                const LogicStorageRegion &region)
{
  _storage = &storage;
  _region = region;
  _copyBytes = 0;
  resetStatus();

  if (region.size == 0 ||
      (region.size % COPY_COUNT) != 0 ||
      region.size < COPY_COUNT * (HEADER_BYTES + 1U))
  {
    _lastError = LogicRetentiveStoreError::InvalidRegion;
    return false;
  }

  if (region.address > storage.capacity() ||
      region.size > storage.capacity() - region.address)
  {
    _lastError = LogicRetentiveStoreError::StorageTooSmall;
    return false;
  }

  _copyBytes = region.size / COPY_COUNT;
  if (_copyBytes <= HEADER_BYTES)
  {
    _lastError = LogicRetentiveStoreError::InvalidRegion;
    return false;
  }

  return refreshStatus();
}

bool LogicRetentiveStore::refreshStatus()
{
  if (_storage == nullptr || _copyBytes == 0)
  {
    _lastError = LogicRetentiveStoreError::NullArgument;
    return false;
  }

  RecordData copy0 = {};
  RecordData copy1 = {};
  bool readFailed0 = false;
  bool readFailed1 = false;

  const bool valid0 = readRecord(0, copy0, readFailed0);
  const bool valid1 = readRecord(1, copy1, readFailed1);

  if (readFailed0 || readFailed1)
  {
    resetStatus();
    _lastError = LogicRetentiveStoreError::ReadFailed;
    return false;
  }

  const uint8_t previousLoadedCopy = _status.lastLoadedCopy;
  resetStatus();
  _status.lastLoadedCopy = previousLoadedCopy;

  if (!valid0 && !valid1)
  {
    _lastError = LogicRetentiveStoreError::None;
    return true;
  }

  const bool useCopy1 =
      valid1 && (!valid0 || isSequenceNewer(copy1.sequence, copy0.sequence));
  const RecordData &selected = useCopy1 ? copy1 : copy0;

  _status.hasSnapshot = true;
  _status.activeCopy = useCopy1 ? 1 : 0;
  _status.sequence = selected.sequence;
  _status.programId = selected.programId;
  _status.generation = selected.generation;
  _status.blockCount = selected.blockCount;
  _status.bitmapBytes = selected.bitmapBytes;
  _lastError = LogicRetentiveStoreError::None;
  return true;
}

bool LogicRetentiveStore::readRecord(uint8_t copyIndex,
                                     RecordData &destination,
                                     bool &readFailed) const
{
  destination = {};
  readFailed = false;

  if (_storage == nullptr || copyIndex >= COPY_COUNT)
  {
    return false;
  }

  uint8_t header[HEADER_BYTES] = {};
  if (!_storage->read(copyAddress(copyIndex), header, sizeof(header)))
  {
    readFailed = true;
    return false;
  }

  if (readU32LE(header + OFFSET_MAGIC) != RECORD_MAGIC ||
      readU16LE(header + OFFSET_VERSION) != FORMAT_VERSION ||
      readU16LE(header + OFFSET_HEADER_SIZE) != HEADER_BYTES ||
      readU32LE(header + OFFSET_FLAGS) != 0)
  {
    return false;
  }

  uint8_t headerCopy[HEADER_BYTES];
  memcpy(headerCopy, header, sizeof(headerCopy));
  const uint32_t storedHeaderCrc =
      readU32LE(headerCopy + OFFSET_HEADER_CRC);
  writeU32LE(headerCopy + OFFSET_HEADER_CRC, 0);

  if (LogicProgramCodec::crc32(headerCopy, sizeof(headerCopy)) !=
      storedHeaderCrc)
  {
    return false;
  }

  const uint16_t blockCount = readU16LE(header + OFFSET_BLOCK_COUNT);
  const uint16_t bitmapBytes = readU16LE(header + OFFSET_BITMAP_BYTES);
  const size_t expectedBitmapBytes = requiredBitmapBytes(blockCount);

  if (expectedBitmapBytes == 0 ||
      bitmapBytes != expectedBitmapBytes ||
      bitmapBytes > MAX_BITMAP_BYTES ||
      bitmapBytes > payloadCapacity())
  {
    return false;
  }

  if (!_storage->read(copyAddress(copyIndex) + HEADER_BYTES,
                      destination.bitmap,
                      bitmapBytes))
  {
    readFailed = true;
    return false;
  }

  if (LogicProgramCodec::crc32(destination.bitmap, bitmapBytes) !=
      readU32LE(header + OFFSET_BITMAP_CRC))
  {
    return false;
  }

  destination.valid = true;
  destination.sequence = readU32LE(header + OFFSET_SEQUENCE);
  destination.programId = readU32LE(header + OFFSET_PROGRAM_ID);
  destination.generation = readU32LE(header + OFFSET_GENERATION);
  destination.blockCount = blockCount;
  destination.bitmapBytes = bitmapBytes;
  return true;
}

bool LogicRetentiveStore::writeRecord(uint8_t copyIndex,
                                      const RecordData &record)
{
  if (_storage == nullptr || copyIndex >= COPY_COUNT)
  {
    _lastError = LogicRetentiveStoreError::NullArgument;
    return false;
  }

  uint8_t header[HEADER_BYTES] = {};
  writeU32LE(header + OFFSET_MAGIC, RECORD_MAGIC);
  writeU16LE(header + OFFSET_VERSION, FORMAT_VERSION);
  writeU16LE(header + OFFSET_HEADER_SIZE, HEADER_BYTES);
  writeU32LE(header + OFFSET_SEQUENCE, record.sequence);
  writeU32LE(header + OFFSET_PROGRAM_ID, record.programId);
  writeU32LE(header + OFFSET_GENERATION, record.generation);
  writeU16LE(header + OFFSET_BLOCK_COUNT, record.blockCount);
  writeU16LE(header + OFFSET_BITMAP_BYTES, record.bitmapBytes);
  writeU32LE(header + OFFSET_BITMAP_CRC,
             LogicProgramCodec::crc32(record.bitmap,
                                      record.bitmapBytes));
  writeU32LE(header + OFFSET_FLAGS, 0);
  writeU32LE(header + OFFSET_HEADER_CRC, 0);
  writeU32LE(header + OFFSET_HEADER_CRC,
             LogicProgramCodec::crc32(header, sizeof(header)));

  const size_t address = copyAddress(copyIndex);
  const uint8_t invalidMagicByte = 0;

  if (!_storage->write(address, &invalidMagicByte, 1) ||
      !_storage->write(address + HEADER_BYTES,
                       record.bitmap,
                       record.bitmapBytes) ||
      !_storage->write(address + 4,
                       header + 4,
                       HEADER_BYTES - 4) ||
      !_storage->write(address, header, 4))
  {
    _lastError = LogicRetentiveStoreError::WriteFailed;
    return false;
  }

  RecordData verified = {};
  bool readFailed = false;
  if (!readRecord(copyIndex, verified, readFailed) ||
      readFailed ||
      verified.sequence != record.sequence ||
      verified.programId != record.programId ||
      verified.generation != record.generation ||
      verified.blockCount != record.blockCount ||
      verified.bitmapBytes != record.bitmapBytes ||
      memcmp(verified.bitmap,
             record.bitmap,
             record.bitmapBytes) != 0)
  {
    _lastError = readFailed
                     ? LogicRetentiveStoreError::ReadFailed
                     : LogicRetentiveStoreError::VerificationFailed;
    return false;
  }

  return true;
}

bool LogicRetentiveStore::save(uint32_t programId,
                               uint32_t generation,
                               uint16_t blockCount,
                               const uint8_t *bitmap,
                               size_t bitmapBytes)
{
  const size_t expectedBitmapBytes = requiredBitmapBytes(blockCount);
  if (_storage == nullptr || bitmap == nullptr)
  {
    _lastError = LogicRetentiveStoreError::NullArgument;
    return false;
  }

  if (expectedBitmapBytes == 0 ||
      bitmapBytes != expectedBitmapBytes ||
      bitmapBytes > payloadCapacity() ||
      bitmapBytes > MAX_BITMAP_BYTES)
  {
    _lastError = LogicRetentiveStoreError::InvalidBitmap;
    return false;
  }

  if (!refreshStatus())
  {
    return false;
  }

  RecordData record = {};
  record.valid = true;
  record.sequence = _status.hasSnapshot ? _status.sequence + 1U : 1U;
  record.programId = programId;
  record.generation = generation;
  record.blockCount = blockCount;
  record.bitmapBytes = static_cast<uint16_t>(bitmapBytes);
  memcpy(record.bitmap, bitmap, bitmapBytes);

  const uint8_t targetCopy =
      _status.activeCopy == 0 ? 1 : 0;

  if (!writeRecord(targetCopy, record))
  {
    return false;
  }

  if (!refreshStatus() ||
      !_status.hasSnapshot ||
      _status.activeCopy != targetCopy ||
      _status.sequence != record.sequence)
  {
    _lastError = LogicRetentiveStoreError::VerificationFailed;
    return false;
  }

  _lastError = LogicRetentiveStoreError::None;
  return true;
}

bool LogicRetentiveStore::load(uint32_t programId,
                               uint32_t generation,
                               uint16_t blockCount,
                               uint8_t *destination,
                               size_t destinationCapacity,
                               size_t &loadedBytes)
{
  loadedBytes = 0;
  const size_t expectedBitmapBytes = requiredBitmapBytes(blockCount);

  if (_storage == nullptr || destination == nullptr)
  {
    _lastError = LogicRetentiveStoreError::NullArgument;
    return false;
  }

  if (expectedBitmapBytes == 0)
  {
    _lastError = LogicRetentiveStoreError::InvalidBitmap;
    return false;
  }

  if (destinationCapacity < expectedBitmapBytes)
  {
    _lastError = LogicRetentiveStoreError::BufferTooSmall;
    return false;
  }

  if (!refreshStatus())
  {
    return false;
  }

  RecordData copy0 = {};
  RecordData copy1 = {};
  bool readFailed0 = false;
  bool readFailed1 = false;
  const bool valid0 = readRecord(0, copy0, readFailed0);
  const bool valid1 = readRecord(1, copy1, readFailed1);

  if (readFailed0 || readFailed1)
  {
    _status.lastLoadedCopy = JWPLC_LOGIC_INVALID_RETENTIVE_COPY;
    _lastError = LogicRetentiveStoreError::ReadFailed;
    return false;
  }

  const bool match0 =
      valid0 && identityMatches(copy0,
                                programId,
                                generation,
                                blockCount,
                                expectedBitmapBytes);
  const bool match1 =
      valid1 && identityMatches(copy1,
                                programId,
                                generation,
                                blockCount,
                                expectedBitmapBytes);

  if (!match0 && !match1)
  {
    _status.lastLoadedCopy = JWPLC_LOGIC_INVALID_RETENTIVE_COPY;
    _lastError = LogicRetentiveStoreError::NoMatchingSnapshot;
    return false;
  }

  const bool useCopy1 =
      match1 && (!match0 || isSequenceNewer(copy1.sequence, copy0.sequence));
  const RecordData &selected = useCopy1 ? copy1 : copy0;

  memcpy(destination, selected.bitmap, selected.bitmapBytes);
  loadedBytes = selected.bitmapBytes;
  _status.lastLoadedCopy = useCopy1 ? 1 : 0;
  _lastError = LogicRetentiveStoreError::None;
  return true;
}

bool LogicRetentiveStore::identityMatches(const RecordData &record,
                                          uint32_t programId,
                                          uint32_t generation,
                                          uint16_t blockCount,
                                          size_t bitmapBytes)
{
  return record.valid &&
         record.programId == programId &&
         record.generation == generation &&
         record.blockCount == blockCount &&
         record.bitmapBytes == bitmapBytes;
}

size_t LogicRetentiveStore::copyAddress(uint8_t copyIndex) const
{
  return static_cast<size_t>(_region.address) +
         static_cast<size_t>(copyIndex) * _copyBytes;
}

bool LogicRetentiveStore::hasSnapshot() const
{
  return _status.hasSnapshot;
}

size_t LogicRetentiveStore::copyBytes() const
{
  return _copyBytes;
}

size_t LogicRetentiveStore::payloadCapacity() const
{
  return _copyBytes > HEADER_BYTES ? _copyBytes - HEADER_BYTES : 0;
}

const LogicRetentiveStoreStatus &LogicRetentiveStore::status() const
{
  return _status;
}

LogicRetentiveStoreError LogicRetentiveStore::lastError() const
{
  return _lastError;
}

bool LogicRetentiveStore::isSequenceNewer(uint32_t candidate,
                                          uint32_t reference)
{
  return static_cast<int32_t>(candidate - reference) > 0;
}

void LogicRetentiveStore::writeU16LE(uint8_t *destination,
                                     uint16_t value)
{
  destination[0] = static_cast<uint8_t>(value & 0xFFU);
  destination[1] = static_cast<uint8_t>((value >> 8) & 0xFFU);
}

void LogicRetentiveStore::writeU32LE(uint8_t *destination,
                                     uint32_t value)
{
  destination[0] = static_cast<uint8_t>(value & 0xFFUL);
  destination[1] = static_cast<uint8_t>((value >> 8) & 0xFFUL);
  destination[2] = static_cast<uint8_t>((value >> 16) & 0xFFUL);
  destination[3] = static_cast<uint8_t>((value >> 24) & 0xFFUL);
}

uint16_t LogicRetentiveStore::readU16LE(const uint8_t *source)
{
  return static_cast<uint16_t>(source[0]) |
         (static_cast<uint16_t>(source[1]) << 8);
}

uint32_t LogicRetentiveStore::readU32LE(const uint8_t *source)
{
  return static_cast<uint32_t>(source[0]) |
         (static_cast<uint32_t>(source[1]) << 8) |
         (static_cast<uint32_t>(source[2]) << 16) |
         (static_cast<uint32_t>(source[3]) << 24);
}

const char *LogicRetentiveStore::errorName(
    LogicRetentiveStoreError error)
{
  switch (error)
  {
  case LogicRetentiveStoreError::None:
    return "NONE";
  case LogicRetentiveStoreError::NullArgument:
    return "NULL_ARGUMENT";
  case LogicRetentiveStoreError::InvalidRegion:
    return "INVALID_REGION";
  case LogicRetentiveStoreError::StorageTooSmall:
    return "STORAGE_TOO_SMALL";
  case LogicRetentiveStoreError::InvalidBitmap:
    return "INVALID_BITMAP";
  case LogicRetentiveStoreError::BufferTooSmall:
    return "BUFFER_TOO_SMALL";
  case LogicRetentiveStoreError::ReadFailed:
    return "READ_FAILED";
  case LogicRetentiveStoreError::WriteFailed:
    return "WRITE_FAILED";
  case LogicRetentiveStoreError::VerificationFailed:
    return "VERIFICATION_FAILED";
  case LogicRetentiveStoreError::NoMatchingSnapshot:
    return "NO_MATCHING_SNAPSHOT";
  default:
    return "UNKNOWN";
  }
}