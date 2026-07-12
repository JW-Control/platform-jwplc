#include "LogicProgramStore.h"

#include <cstring>

LogicProgramStore::LogicProgramStore()
    : _storage(nullptr),
      _profile(nullptr),
      _status{},
      _lastError(LogicProgramStoreError::None)
{
  resetStatus();
}

void LogicProgramStore::resetStatus()
{
  _status.formatted = false;
  _status.activeSlot = JWPLC_LOGIC_INVALID_SLOT;
  _status.superblockCopy = JWPLC_LOGIC_INVALID_SLOT;
  _status.lastLoadedSlot = JWPLC_LOGIC_INVALID_SLOT;
  _status.sequence = 0;
  _status.programId = 0;
  _status.generation = 0;
}

size_t LogicProgramStore::requiredCapacity(
    const LogicStorageProfile &profile)
{
  return SUPERBLOCK_AREA_SIZE +
         (static_cast<size_t>(profile.programSlotBytes) * 2U);
}

size_t LogicProgramStore::requiredCapacity() const
{
  return _profile == nullptr ? 0 : requiredCapacity(*_profile);
}

size_t LogicProgramStore::slotPayloadCapacity() const
{
  if (_profile == nullptr ||
      _profile->programSlotBytes <= SLOT_DESCRIPTOR_SIZE)
  {
    return 0;
  }

  return static_cast<size_t>(_profile->programSlotBytes) -
         SLOT_DESCRIPTOR_SIZE;
}

bool LogicProgramStore::begin(LogicByteStorage &storage,
                              const LogicStorageProfile &profile)
{
  _storage = &storage;
  _profile = &profile;
  resetStatus();

  if (profile.programSlotBytes <= SLOT_DESCRIPTOR_SIZE)
  {
    _lastError = LogicProgramStoreError::InvalidProfile;
    return false;
  }

  if (storage.capacity() < requiredCapacity(profile))
  {
    _lastError = LogicProgramStoreError::StorageTooSmall;
    return false;
  }

  if (!loadLatestSuperblock())
  {
    _lastError = LogicProgramStoreError::Unformatted;
    return true;
  }

  _lastError = LogicProgramStoreError::None;
  return true;
}

bool LogicProgramStore::format()
{
  if (_storage == nullptr || _profile == nullptr)
  {
    _lastError = LogicProgramStoreError::NullArgument;
    return false;
  }

  if (!_storage->fill(0, 0xFF, requiredCapacity()))
  {
    _lastError = LogicProgramStoreError::FormatFailed;
    return false;
  }

  const SuperblockData primary = {
      1,
      JWPLC_LOGIC_INVALID_SLOT,
      0,
      0};
  const SuperblockData secondary = {
      0,
      JWPLC_LOGIC_INVALID_SLOT,
      0,
      0};

  if (!writeSuperblock(0, primary) ||
      !writeSuperblock(1, secondary))
  {
    _lastError = LogicProgramStoreError::FormatFailed;
    return false;
  }

  _status.formatted = true;
  _status.activeSlot = JWPLC_LOGIC_INVALID_SLOT;
  _status.superblockCopy = 0;
  _status.lastLoadedSlot = JWPLC_LOGIC_INVALID_SLOT;
  _status.sequence = primary.sequence;
  _status.programId = 0;
  _status.generation = 0;
  _lastError = LogicProgramStoreError::None;
  return true;
}

bool LogicProgramStore::saveProgram(const LogicProgram &program,
                                    uint32_t programId,
                                    uint32_t generation,
                                    uint32_t flags,
                                    uint8_t *scratch,
                                    size_t scratchCapacity)
{
  if (_storage == nullptr || _profile == nullptr || scratch == nullptr)
  {
    _lastError = LogicProgramStoreError::NullArgument;
    return false;
  }

  if (!_status.formatted)
  {
    _lastError = LogicProgramStoreError::Unformatted;
    return false;
  }

  const size_t imageSize = LogicProgramCodec::requiredSize(program.blockCount);
  if (imageSize > slotPayloadCapacity() || imageSize > scratchCapacity)
  {
    _lastError = LogicProgramStoreError::ProgramTooLarge;
    return false;
  }

  size_t writtenBytes = 0;
  const LogicProgramCodecError codecError =
      LogicProgramCodec::serialize(program,
                                   programId,
                                   generation,
                                   flags,
                                   scratch,
                                   scratchCapacity,
                                   writtenBytes);

  if (codecError != LogicProgramCodecError::None)
  {
    _lastError = LogicProgramStoreError::CodecFailed;
    return false;
  }

  const uint8_t targetSlot =
      _status.activeSlot == 0 ? 1 : 0;
  const uint32_t imageCrc32 =
      LogicProgramCodec::crc32(scratch, writtenBytes);

  SlotDescriptorData descriptor = {
      LogicProgramSlotState::Writing,
      targetSlot,
      generation,
      static_cast<uint32_t>(writtenBytes),
      imageCrc32,
      programId};

  if (!writeSlotDescriptor(targetSlot, descriptor))
  {
    _lastError = LogicProgramStoreError::WriteFailed;
    return false;
  }

  if (!_storage->write(slotImageAddress(targetSlot),
                       scratch,
                       writtenBytes))
  {
    _lastError = LogicProgramStoreError::WriteFailed;
    return false;
  }

  if (!_storage->read(slotImageAddress(targetSlot),
                      scratch,
                      writtenBytes))
  {
    _lastError = LogicProgramStoreError::ReadFailed;
    return false;
  }

  if (LogicProgramCodec::crc32(scratch, writtenBytes) != imageCrc32)
  {
    _lastError = LogicProgramStoreError::ImageCrcMismatch;
    return false;
  }

  descriptor.state = LogicProgramSlotState::Verified;
  if (!writeSlotDescriptor(targetSlot, descriptor))
  {
    _lastError = LogicProgramStoreError::WriteFailed;
    return false;
  }

  const uint8_t nextSuperblockCopy =
      _status.superblockCopy == 0 ? 1 : 0;
  const SuperblockData nextSuperblock = {
      _status.sequence + 1U,
      targetSlot,
      programId,
      generation};

  if (!writeSuperblock(nextSuperblockCopy, nextSuperblock))
  {
    _lastError = LogicProgramStoreError::WriteFailed;
    return false;
  }

  _status.formatted = true;
  _status.activeSlot = targetSlot;
  _status.superblockCopy = nextSuperblockCopy;
  _status.lastLoadedSlot = JWPLC_LOGIC_INVALID_SLOT;
  _status.sequence = nextSuperblock.sequence;
  _status.programId = programId;
  _status.generation = generation;
  _lastError = LogicProgramStoreError::None;
  return true;
}

bool LogicProgramStore::loadActive(LogicProgramBuffer &destination,
                                   uint8_t *scratch,
                                   size_t scratchCapacity)
{
  if (_storage == nullptr || _profile == nullptr || scratch == nullptr)
  {
    _lastError = LogicProgramStoreError::NullArgument;
    return false;
  }

  if (!_status.formatted)
  {
    _lastError = LogicProgramStoreError::Unformatted;
    return false;
  }

  if (_status.activeSlot <= 1)
  {
    if (loadSlot(_status.activeSlot,
                 destination,
                 scratch,
                 scratchCapacity))
    {
      _status.lastLoadedSlot = _status.activeSlot;
      _lastError = LogicProgramStoreError::None;
      return true;
    }

    const uint8_t fallbackSlot = _status.activeSlot == 0 ? 1 : 0;
    if (loadSlot(fallbackSlot,
                 destination,
                 scratch,
                 scratchCapacity))
    {
      _status.lastLoadedSlot = fallbackSlot;
      _lastError = LogicProgramStoreError::None;
      return true;
    }
  }

  _status.lastLoadedSlot = JWPLC_LOGIC_INVALID_SLOT;
  _lastError = LogicProgramStoreError::NoValidProgram;
  return false;
}

bool LogicProgramStore::loadSlot(uint8_t slotIndex,
                                 LogicProgramBuffer &destination,
                                 uint8_t *scratch,
                                 size_t scratchCapacity)
{
  SlotDescriptorData descriptor = {};
  if (!readSlotDescriptor(slotIndex, descriptor))
  {
    return false;
  }

  if (descriptor.state != LogicProgramSlotState::Verified ||
      descriptor.slotIndex != slotIndex ||
      descriptor.imageLength == 0 ||
      descriptor.imageLength > slotPayloadCapacity() ||
      descriptor.imageLength > scratchCapacity)
  {
    return false;
  }

  if (!_storage->read(slotImageAddress(slotIndex),
                      scratch,
                      descriptor.imageLength))
  {
    return false;
  }

  if (LogicProgramCodec::crc32(scratch, descriptor.imageLength) !=
      descriptor.imageCrc32)
  {
    return false;
  }

  if (LogicProgramCodec::deserialize(scratch,
                                     descriptor.imageLength,
                                     destination) !=
      LogicProgramCodecError::None)
  {
    return false;
  }

  if (destination.metadata.programId != descriptor.programId ||
      destination.metadata.generation != descriptor.generation ||
      destination.metadata.payloadLength +
              JWPLC_LOGIC_IMAGE_HEADER_SIZE !=
          descriptor.imageLength)
  {
    return false;
  }

  return true;
}

bool LogicProgramStore::loadLatestSuperblock()
{
  SuperblockData copy0 = {};
  SuperblockData copy1 = {};
  const bool valid0 = readSuperblock(0, copy0);
  const bool valid1 = readSuperblock(1, copy1);

  if (!valid0 && !valid1)
  {
    resetStatus();
    return false;
  }

  const bool useCopy1 =
      valid1 && (!valid0 || isSequenceNewer(copy1.sequence, copy0.sequence));
  const SuperblockData &selected = useCopy1 ? copy1 : copy0;

  _status.formatted = true;
  _status.activeSlot = selected.activeSlot;
  _status.superblockCopy = useCopy1 ? 1 : 0;
  _status.lastLoadedSlot = JWPLC_LOGIC_INVALID_SLOT;
  _status.sequence = selected.sequence;
  _status.programId = selected.programId;
  _status.generation = selected.generation;
  return true;
}

bool LogicProgramStore::readSuperblock(uint8_t copyIndex,
                                       SuperblockData &destination)
{
  if (_storage == nullptr || copyIndex >= SUPERBLOCK_COPIES)
  {
    return false;
  }

  uint8_t bytes[SUPERBLOCK_SIZE] = {};
  const size_t address = static_cast<size_t>(copyIndex) * SUPERBLOCK_SIZE;

  if (!_storage->read(address, bytes, sizeof(bytes)))
  {
    return false;
  }

  if (readU32LE(bytes + 0) != SUPERBLOCK_MAGIC ||
      readU16LE(bytes + 4) != STORE_FORMAT_VERSION ||
      readU16LE(bytes + 6) != SUPERBLOCK_SIZE)
  {
    return false;
  }

  const uint8_t activeSlot = bytes[12];
  if (activeSlot != JWPLC_LOGIC_INVALID_SLOT && activeSlot > 1)
  {
    return false;
  }

  const uint32_t expectedCrc = readU32LE(bytes + 28);
  if (LogicProgramCodec::crc32(bytes, 28) != expectedCrc)
  {
    return false;
  }

  destination.sequence = readU32LE(bytes + 8);
  destination.activeSlot = activeSlot;
  destination.programId = readU32LE(bytes + 16);
  destination.generation = readU32LE(bytes + 20);
  return true;
}

bool LogicProgramStore::writeSuperblock(uint8_t copyIndex,
                                        const SuperblockData &source)
{
  if (_storage == nullptr || copyIndex >= SUPERBLOCK_COPIES)
  {
    return false;
  }

  uint8_t bytes[SUPERBLOCK_SIZE] = {};
  writeU32LE(bytes + 0, SUPERBLOCK_MAGIC);
  writeU16LE(bytes + 4, STORE_FORMAT_VERSION);
  writeU16LE(bytes + 6, SUPERBLOCK_SIZE);
  writeU32LE(bytes + 8, source.sequence);
  bytes[12] = source.activeSlot;
  writeU32LE(bytes + 16, source.programId);
  writeU32LE(bytes + 20, source.generation);
  writeU32LE(bytes + 28, LogicProgramCodec::crc32(bytes, 28));

  const size_t address = static_cast<size_t>(copyIndex) * SUPERBLOCK_SIZE;
  return _storage->write(address, bytes, sizeof(bytes));
}

bool LogicProgramStore::readSlotDescriptor(
    uint8_t slotIndex,
    SlotDescriptorData &destination)
{
  if (_storage == nullptr || slotIndex > 1)
  {
    return false;
  }

  uint8_t bytes[SLOT_DESCRIPTOR_SIZE] = {};
  if (!_storage->read(slotAddress(slotIndex), bytes, sizeof(bytes)))
  {
    return false;
  }

  if (readU32LE(bytes + 0) != SLOT_MAGIC ||
      readU16LE(bytes + 4) != STORE_FORMAT_VERSION ||
      readU16LE(bytes + 6) != SLOT_DESCRIPTOR_SIZE ||
      bytes[9] != slotIndex)
  {
    return false;
  }

  const LogicProgramSlotState state =
      static_cast<LogicProgramSlotState>(bytes[8]);
  if (state != LogicProgramSlotState::Writing &&
      state != LogicProgramSlotState::Verified)
  {
    return false;
  }

  const uint32_t expectedCrc = readU32LE(bytes + 28);
  if (LogicProgramCodec::crc32(bytes, 28) != expectedCrc)
  {
    return false;
  }

  destination.state = state;
  destination.slotIndex = bytes[9];
  destination.generation = readU32LE(bytes + 12);
  destination.imageLength = readU32LE(bytes + 16);
  destination.imageCrc32 = readU32LE(bytes + 20);
  destination.programId = readU32LE(bytes + 24);
  return true;
}

bool LogicProgramStore::writeSlotDescriptor(
    uint8_t slotIndex,
    const SlotDescriptorData &source)
{
  if (_storage == nullptr || slotIndex > 1)
  {
    return false;
  }

  uint8_t bytes[SLOT_DESCRIPTOR_SIZE] = {};
  writeU32LE(bytes + 0, SLOT_MAGIC);
  writeU16LE(bytes + 4, STORE_FORMAT_VERSION);
  writeU16LE(bytes + 6, SLOT_DESCRIPTOR_SIZE);
  bytes[8] = static_cast<uint8_t>(source.state);
  bytes[9] = source.slotIndex;
  writeU32LE(bytes + 12, source.generation);
  writeU32LE(bytes + 16, source.imageLength);
  writeU32LE(bytes + 20, source.imageCrc32);
  writeU32LE(bytes + 24, source.programId);
  writeU32LE(bytes + 28, LogicProgramCodec::crc32(bytes, 28));

  return _storage->write(slotAddress(slotIndex), bytes, sizeof(bytes));
}

size_t LogicProgramStore::slotAddress(uint8_t slotIndex) const
{
  return SUPERBLOCK_AREA_SIZE +
         (static_cast<size_t>(slotIndex) * _profile->programSlotBytes);
}

size_t LogicProgramStore::slotImageAddress(uint8_t slotIndex) const
{
  return slotAddress(slotIndex) + SLOT_DESCRIPTOR_SIZE;
}

bool LogicProgramStore::isFormatted() const
{
  return _status.formatted;
}

const LogicProgramStoreStatus &LogicProgramStore::status() const
{
  return _status;
}

LogicProgramStoreError LogicProgramStore::lastError() const
{
  return _lastError;
}

bool LogicProgramStore::isSequenceNewer(uint32_t candidate,
                                        uint32_t reference)
{
  return static_cast<int32_t>(candidate - reference) > 0;
}

void LogicProgramStore::writeU16LE(uint8_t *destination, uint16_t value)
{
  destination[0] = static_cast<uint8_t>(value & 0xFFU);
  destination[1] = static_cast<uint8_t>((value >> 8) & 0xFFU);
}

void LogicProgramStore::writeU32LE(uint8_t *destination, uint32_t value)
{
  destination[0] = static_cast<uint8_t>(value & 0xFFUL);
  destination[1] = static_cast<uint8_t>((value >> 8) & 0xFFUL);
  destination[2] = static_cast<uint8_t>((value >> 16) & 0xFFUL);
  destination[3] = static_cast<uint8_t>((value >> 24) & 0xFFUL);
}

uint16_t LogicProgramStore::readU16LE(const uint8_t *source)
{
  return static_cast<uint16_t>(source[0]) |
         (static_cast<uint16_t>(source[1]) << 8);
}

uint32_t LogicProgramStore::readU32LE(const uint8_t *source)
{
  return static_cast<uint32_t>(source[0]) |
         (static_cast<uint32_t>(source[1]) << 8) |
         (static_cast<uint32_t>(source[2]) << 16) |
         (static_cast<uint32_t>(source[3]) << 24);
}

const char *LogicProgramStore::errorName(LogicProgramStoreError error)
{
  switch (error)
  {
  case LogicProgramStoreError::None:
    return "NONE";
  case LogicProgramStoreError::NullArgument:
    return "NULL_ARGUMENT";
  case LogicProgramStoreError::StorageTooSmall:
    return "STORAGE_TOO_SMALL";
  case LogicProgramStoreError::InvalidProfile:
    return "INVALID_PROFILE";
  case LogicProgramStoreError::Unformatted:
    return "UNFORMATTED";
  case LogicProgramStoreError::FormatFailed:
    return "FORMAT_FAILED";
  case LogicProgramStoreError::ProgramTooLarge:
    return "PROGRAM_TOO_LARGE";
  case LogicProgramStoreError::CodecFailed:
    return "CODEC_FAILED";
  case LogicProgramStoreError::ReadFailed:
    return "READ_FAILED";
  case LogicProgramStoreError::WriteFailed:
    return "WRITE_FAILED";
  case LogicProgramStoreError::InvalidSuperblock:
    return "INVALID_SUPERBLOCK";
  case LogicProgramStoreError::InvalidSlotDescriptor:
    return "INVALID_SLOT_DESCRIPTOR";
  case LogicProgramStoreError::ImageCrcMismatch:
    return "IMAGE_CRC_MISMATCH";
  case LogicProgramStoreError::NoValidProgram:
    return "NO_VALID_PROGRAM";
  default:
    return "UNKNOWN";
  }
}
