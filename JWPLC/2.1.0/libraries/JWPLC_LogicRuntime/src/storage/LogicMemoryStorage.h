#ifndef JWPLC_LOGIC_MEMORY_STORAGE_H
#define JWPLC_LOGIC_MEMORY_STORAGE_H

#include <Arduino.h>

#include "LogicByteStorage.h"

/**
 * @brief Backend de almacenamiento sobre un buffer RAM externo.
 *
 * Incluye inyección de fallos por presupuesto de escritura para simular cortes
 * de alimentación durante operaciones transaccionales.
 */
class LogicMemoryStorage : public LogicByteStorage
{
public:
  LogicMemoryStorage(uint8_t *buffer, size_t bufferCapacity);

  size_t capacity() const override;
  bool read(size_t address, uint8_t *destination, size_t length) override;
  bool write(size_t address, const uint8_t *source, size_t length) override;
  bool fill(size_t address, uint8_t value, size_t length) override;

  void setWriteBudget(size_t bytes);
  void clearWriteBudget();
  size_t bytesWritten() const;

private:
  bool isRangeValid(size_t address, size_t length) const;
  size_t allowedWriteLength(size_t requested) const;
  void consumeWriteBudget(size_t written);

  static constexpr size_t UNLIMITED_WRITE_BUDGET = static_cast<size_t>(-1);

  uint8_t *_buffer;
  size_t _capacity;
  size_t _writeBudget;
  size_t _bytesWritten;
};

#endif
