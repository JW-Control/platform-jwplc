#include "LogicMemoryStorage.h"

#include <cstring>

LogicMemoryStorage::LogicMemoryStorage(uint8_t *buffer, size_t bufferCapacity)
    : _buffer(buffer),
      _capacity(bufferCapacity),
      _writeBudget(UNLIMITED_WRITE_BUDGET),
      _bytesWritten(0)
{
}

size_t LogicMemoryStorage::capacity() const
{
  return _capacity;
}

bool LogicMemoryStorage::isRangeValid(size_t address, size_t length) const
{
  if (_buffer == nullptr)
  {
    return false;
  }

  if (address > _capacity)
  {
    return false;
  }

  return length <= (_capacity - address);
}

bool LogicMemoryStorage::read(size_t address,
                              uint8_t *destination,
                              size_t length)
{
  if (destination == nullptr || !isRangeValid(address, length))
  {
    return false;
  }

  if (length > 0)
  {
    std::memcpy(destination, _buffer + address, length);
  }

  return true;
}

size_t LogicMemoryStorage::allowedWriteLength(size_t requested) const
{
  if (_writeBudget == UNLIMITED_WRITE_BUDGET)
  {
    return requested;
  }

  return requested < _writeBudget ? requested : _writeBudget;
}

void LogicMemoryStorage::consumeWriteBudget(size_t written)
{
  _bytesWritten += written;

  if (_writeBudget != UNLIMITED_WRITE_BUDGET)
  {
    _writeBudget -= written;
  }
}

bool LogicMemoryStorage::write(size_t address,
                               const uint8_t *source,
                               size_t length)
{
  if (source == nullptr || !isRangeValid(address, length))
  {
    return false;
  }

  const size_t writable = allowedWriteLength(length);

  if (writable > 0)
  {
    std::memcpy(_buffer + address, source, writable);
  }

  consumeWriteBudget(writable);
  return writable == length;
}

bool LogicMemoryStorage::fill(size_t address, uint8_t value, size_t length)
{
  if (!isRangeValid(address, length))
  {
    return false;
  }

  const size_t writable = allowedWriteLength(length);

  if (writable > 0)
  {
    std::memset(_buffer + address, value, writable);
  }

  consumeWriteBudget(writable);
  return writable == length;
}

void LogicMemoryStorage::setWriteBudget(size_t bytes)
{
  _writeBudget = bytes;
  _bytesWritten = 0;
}

void LogicMemoryStorage::clearWriteBudget()
{
  _writeBudget = UNLIMITED_WRITE_BUDGET;
  _bytesWritten = 0;
}

size_t LogicMemoryStorage::bytesWritten() const
{
  return _bytesWritten;
}
