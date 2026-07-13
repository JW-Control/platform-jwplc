#ifndef JWPLC_LOGIC_FRAM_STORAGE_H
#define JWPLC_LOGIC_FRAM_STORAGE_H

#include <Arduino.h>
#include <JW_FRAM.h>

#include "LogicByteStorage.h"

/**
 * @brief Backend direccionable por bytes sobre una instancia JW_FRAM.
 *
 * Puede limitarse a una ventana de la FRAM física para aislar pruebas o separar
 * regiones de almacenamiento. Todas las direcciones recibidas por la interfaz
 * son relativas a esa ventana.
 */
class LogicFRAMStorage : public LogicByteStorage
{
public:
  LogicFRAMStorage();
  LogicFRAMStorage(JW_FRAM &fram,
                   size_t baseAddress = 0,
                   size_t windowCapacity = 0);

  bool begin();
  bool begin(JW_FRAM &fram,
             size_t baseAddress = 0,
             size_t windowCapacity = 0);
  bool isReady() const;
  size_t baseAddress() const;

  size_t capacity() const override;
  bool read(size_t address, uint8_t *destination, size_t length) override;
  bool write(size_t address, const uint8_t *source, size_t length) override;
  bool fill(size_t address, uint8_t value, size_t length) override;

private:
  bool isRangeValid(size_t address, size_t length) const;

  static constexpr size_t FILL_CHUNK_BYTES = 64;

  JW_FRAM *_fram;
  size_t _baseAddress;
  size_t _requestedCapacity;
  size_t _capacity;
  bool _ready;
};

#endif
