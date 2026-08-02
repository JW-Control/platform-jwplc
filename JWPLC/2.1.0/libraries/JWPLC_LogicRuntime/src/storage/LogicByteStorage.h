#ifndef JWPLC_LOGIC_BYTE_STORAGE_H
#define JWPLC_LOGIC_BYTE_STORAGE_H

#include <Arduino.h>

/**
 * @brief Interfaz mínima para un almacenamiento direccionable por bytes.
 *
 * El gestor A/B no conoce si el backend es RAM, FRAM u otro medio. Las
 * implementaciones deben devolver false ante una operación incompleta.
 */
class LogicByteStorage
{
public:
  virtual ~LogicByteStorage() = default;

  virtual size_t capacity() const = 0;
  virtual bool read(size_t address, uint8_t *destination, size_t length) = 0;
  virtual bool write(size_t address, const uint8_t *source, size_t length) = 0;
  virtual bool fill(size_t address, uint8_t value, size_t length) = 0;
};

#endif
