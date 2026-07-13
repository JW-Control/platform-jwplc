#include "LogicFRAMStorage.h"

#include <cstring>

LogicFRAMStorage::LogicFRAMStorage()
    : _fram(nullptr),
      _baseAddress(0),
      _requestedCapacity(0),
      _capacity(0),
      _ready(false)
{
}

LogicFRAMStorage::LogicFRAMStorage(JW_FRAM &fram,
                                   size_t baseAddress,
                                   size_t windowCapacity)
    : _fram(&fram),
      _baseAddress(baseAddress),
      _requestedCapacity(windowCapacity),
      _capacity(0),
      _ready(false)
{
}

bool LogicFRAMStorage::begin(JW_FRAM &fram,
                             size_t baseAddress,
                             size_t windowCapacity)
{
  _fram = &fram;
  _baseAddress = baseAddress;
  _requestedCapacity = windowCapacity;
  return begin();
}

bool LogicFRAMStorage::begin()
{
  _ready = false;
  _capacity = 0;

  if (_fram == nullptr)
  {
    return false;
  }

  const size_t physicalCapacity = static_cast<size_t>(_fram->size());
  if (physicalCapacity == 0 || _baseAddress >= physicalCapacity)
  {
    return false;
  }

  const size_t available = physicalCapacity - _baseAddress;
  _capacity = _requestedCapacity == 0 ? available : _requestedCapacity;

  if (_capacity == 0 || _capacity > available)
  {
    _capacity = 0;
    return false;
  }

  _ready = true;
  return true;
}

bool LogicFRAMStorage::isReady() const
{
  return _ready;
}

size_t LogicFRAMStorage::baseAddress() const
{
  return _baseAddress;
}

size_t LogicFRAMStorage::capacity() const
{
  return _ready ? _capacity : 0;
}

bool LogicFRAMStorage::isRangeValid(size_t address, size_t length) const
{
  if (!_ready)
  {
    return false;
  }

  if (length == 0)
  {
    return address <= _capacity;
  }

  if (address >= _capacity)
  {
    return false;
  }

  return length <= (_capacity - address);
}

bool LogicFRAMStorage::read(size_t address,
                            uint8_t *destination,
                            size_t length)
{
  if (length == 0)
  {
    return isRangeValid(address, length);
  }

  if (destination == nullptr || !isRangeValid(address, length))
  {
    return false;
  }

  return _fram->read(static_cast<uint32_t>(_baseAddress + address),
                     destination,
                     length);
}

bool LogicFRAMStorage::write(size_t address,
                             const uint8_t *source,
                             size_t length)
{
  if (length == 0)
  {
    return isRangeValid(address, length);
  }

  if (source == nullptr || !isRangeValid(address, length))
  {
    return false;
  }

  if (!_fram->writeEnable(true))
  {
    return false;
  }

  const bool writeOk = _fram->write(
      static_cast<uint32_t>(_baseAddress + address),
      source,
      length);

  const bool disableOk = _fram->writeEnable(false);
  return writeOk && disableOk;
}

bool LogicFRAMStorage::fill(size_t address, uint8_t value, size_t length)
{
  if (length == 0)
  {
    return isRangeValid(address, length);
  }

  if (!isRangeValid(address, length))
  {
    return false;
  }

  uint8_t chunk[FILL_CHUNK_BYTES];
  memset(chunk, value, sizeof(chunk));

  size_t offset = 0;
  while (offset < length)
  {
    const size_t remaining = length - offset;
    const size_t chunkLength =
        remaining < sizeof(chunk) ? remaining : sizeof(chunk);

    if (!write(address + offset, chunk, chunkLength))
    {
      return false;
    }

    offset += chunkLength;
  }

  return true;
}
