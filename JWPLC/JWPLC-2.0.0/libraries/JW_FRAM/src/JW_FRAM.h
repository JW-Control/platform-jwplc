#ifndef _JW_FRAM_H_
#define _JW_FRAM_H_

#include <Adafruit_SPIDevice.h>
#include <Arduino.h>
#include <SPI.h>
#include <string.h>
#include <type_traits>

/** Códigos de operación SPI FRAM */
typedef enum jw_fram_opcodes_e
{
  JW_FRAM_OPCODE_WREN = 0b0110,    /* Write Enable Latch */
  JW_FRAM_OPCODE_WRDI = 0b0100,    /* Reset Write Enable Latch */
  JW_FRAM_OPCODE_RDSR = 0b0101,    /* Read Status Register */
  JW_FRAM_OPCODE_WRSR = 0b0001,    /* Write Status Register */
  JW_FRAM_OPCODE_READ = 0b0011,    /* Read Memory */
  JW_FRAM_OPCODE_WRITE = 0b0010,   /* Write Memory */
  JW_FRAM_OPCODE_RDID = 0b10011111 /* Read Device ID */
} JW_FRAM_Opcode;

typedef bool (*JW_FRAM_BusLockCallback)(uint32_t timeoutMs, void *userData);
typedef void (*JW_FRAM_BusUnlockCallback)(void *userData);

/**
 * @brief Librería SPI FRAM con API de bajo nivel y API de alto nivel estilo EEPROM.
 */
class JW_FRAM
{
public:
  static constexpr uint8_t MAX_TEXT_LENGTH = 127;
  static constexpr uint16_t BLOCK_MAGIC = 0x4A57; // "JW"

  struct BlockHeader
  {
    uint16_t magic;
    uint8_t version;
    uint8_t reserved;
    uint16_t length;
    uint16_t checksum;
  } __attribute__((packed));

  JW_FRAM(int8_t cs, SPIClass *theSPI = &SPI, uint32_t freq = 1000000);
  JW_FRAM(int8_t clk, int8_t miso, int8_t mosi, int8_t cs);

  bool begin(uint32_t forcedSizeBytes = 0);

  uint32_t size() const;
  uint8_t addressSize() const;
  bool isAddressValid(uint32_t addr, size_t len = 1) const;

  // API de bajo nivel (write enable manual)
  bool writeEnable(bool enable);
  bool write8(uint32_t addr, uint8_t value);
  uint8_t read8(uint32_t addr);
  bool write(uint32_t addr, const uint8_t *values, size_t count);
  bool read(uint32_t addr, uint8_t *values, size_t count);

  bool getDeviceID(uint8_t *manufacturerID, uint16_t *productID);
  uint8_t getStatusRegister();
  bool setStatusRegister(uint8_t value);
  void setAddressSize(uint8_t nAddressSize);

  // API de alto nivel (write enable automático)
  template <typename T>
  bool get(uint32_t addr, T &value);

  template <typename T>
  bool put(uint32_t addr, const T &value);

  template <typename T>
  bool update(uint32_t addr, const T &value);

  bool writeString(uint32_t addr, const String &value);
  bool readString(uint32_t addr, String &value, uint8_t maxLen = MAX_TEXT_LENGTH);

  bool writeCString(uint32_t addr, const char *str, uint8_t maxLen = MAX_TEXT_LENGTH);
  bool readCString(uint32_t addr, char *buffer, size_t bufferSize,
                   uint8_t maxLen = MAX_TEXT_LENGTH);

  template <typename T>
  bool writeBlock(uint32_t addr, const T &value, uint8_t version = 1);

  template <typename T>
  bool readBlock(uint32_t addr, T &value, uint8_t expectedVersion = 1);

  void setBusLockCallbacks(JW_FRAM_BusLockCallback lockCallback,
                           JW_FRAM_BusUnlockCallback unlockCallback,
                           void *userData = nullptr,
                           uint32_t timeoutMs = 50);

  void clearBusLockCallbacks();

  void enableDebug(Stream &port);
  void disableDebug();
  bool debugEnabled() const;

private:
  Adafruit_SPIDevice *_spi_dev = nullptr;
  uint8_t _addressSizeBytes = 2;
  uint32_t _framSizeBytes = 0;
  Stream *_debugPort = nullptr;
  bool _debugEnabled = false;

  JW_FRAM_BusLockCallback _busLockCallback = nullptr;
  JW_FRAM_BusUnlockCallback _busUnlockCallback = nullptr;
  void *_busLockUserData = nullptr;
  uint32_t _busLockTimeoutMs = 50;

  bool lockBus();
  void unlockBus();

  bool spiWrite(const uint8_t *buffer, size_t len);
  bool spiWrite(const uint8_t *buffer, size_t len,
                const uint8_t *prefixBuffer, size_t prefixLen);
  bool spiWriteThenRead(const uint8_t *writeBuffer, size_t writeLen,
                        uint8_t *readBuffer, size_t readLen);

  void debugPrint(const __FlashStringHelper *msg);
  void debugPrint(const String &msg);
  void debugPrint(const char *msg);
  void debugPrintValue(const __FlashStringHelper *prefix, uint32_t value, int base = DEC);
  uint16_t computeChecksum(const uint8_t *data, size_t len) const;
};

template <typename T>
bool JW_FRAM::get(uint32_t addr, T &value)
{
  static_assert(std::is_trivially_copyable<T>::value,
                "JW_FRAM::get<T>() requiere un tipo trivially copyable");

  if (!isAddressValid(addr, sizeof(T)))
  {
    debugPrint(F("JW_FRAM::get() direccion fuera de rango"));
    return false;
  }

  return read(addr, reinterpret_cast<uint8_t *>(&value), sizeof(T));
}

template <typename T>
bool JW_FRAM::put(uint32_t addr, const T &value)
{
  static_assert(std::is_trivially_copyable<T>::value,
                "JW_FRAM::put<T>() requiere un tipo trivially copyable");

  if (!isAddressValid(addr, sizeof(T)))
  {
    debugPrint(F("JW_FRAM::put() direccion fuera de rango"));
    return false;
  }

  if (!writeEnable(true))
  {
    debugPrint(F("JW_FRAM::put() fallo al habilitar escritura"));
    return false;
  }

  const bool ok = write(addr, reinterpret_cast<const uint8_t *>(&value), sizeof(T));

  if (!writeEnable(false))
  {
    debugPrint(F("JW_FRAM::put() fallo al deshabilitar escritura"));
  }

  if (!ok)
  {
    debugPrint(F("JW_FRAM::put() fallo de escritura"));
  }

  return ok;
}

template <typename T>
bool JW_FRAM::update(uint32_t addr, const T &value)
{
  static_assert(std::is_trivially_copyable<T>::value,
                "JW_FRAM::update<T>() requiere un tipo trivially copyable");

  T current{};
  if (!get(addr, current))
  {
    return false;
  }

  if (memcmp(&current, &value, sizeof(T)) == 0)
  {
    return true;
  }

  return put(addr, value);
}

template <typename T>
bool JW_FRAM::writeBlock(uint32_t addr, const T &value, uint8_t version)
{
  static_assert(std::is_trivially_copyable<T>::value,
                "JW_FRAM::writeBlock<T>() requiere un tipo trivially copyable");
  static_assert(sizeof(T) <= 0xFFFF,
                "JW_FRAM::writeBlock<T>() excede el tamano permitido por uint16_t");

  const size_t totalSize = sizeof(BlockHeader) + sizeof(T);
  if (!isAddressValid(addr, totalSize))
  {
    debugPrint(F("JW_FRAM::writeBlock() direccion fuera de rango"));
    return false;
  }

  BlockHeader header{};
  header.magic = BLOCK_MAGIC;
  header.version = version;
  header.reserved = 0;
  header.length = static_cast<uint16_t>(sizeof(T));
  header.checksum = computeChecksum(reinterpret_cast<const uint8_t *>(&value), sizeof(T));

  if (!writeEnable(true))
  {
    debugPrint(F("JW_FRAM::writeBlock() fallo al habilitar escritura"));
    return false;
  }

  bool ok = write(addr, reinterpret_cast<const uint8_t *>(&header), sizeof(header));
  if (ok)
  {
    ok = write(addr + sizeof(header), reinterpret_cast<const uint8_t *>(&value), sizeof(T));
  }

  if (!writeEnable(false))
  {
    debugPrint(F("JW_FRAM::writeBlock() fallo al deshabilitar escritura"));
  }

  if (!ok)
  {
    debugPrint(F("JW_FRAM::writeBlock() fallo de escritura"));
  }

  return ok;
}

template <typename T>
bool JW_FRAM::readBlock(uint32_t addr, T &value, uint8_t expectedVersion)
{
  static_assert(std::is_trivially_copyable<T>::value,
                "JW_FRAM::readBlock<T>() requiere un tipo trivially copyable");
  static_assert(sizeof(T) <= 0xFFFF,
                "JW_FRAM::readBlock<T>() excede el tamano permitido por uint16_t");

  const size_t totalSize = sizeof(BlockHeader) + sizeof(T);
  if (!isAddressValid(addr, totalSize))
  {
    debugPrint(F("JW_FRAM::readBlock() direccion fuera de rango"));
    return false;
  }

  BlockHeader header{};
  if (!read(addr, reinterpret_cast<uint8_t *>(&header), sizeof(header)))
  {
    debugPrint(F("JW_FRAM::readBlock() fallo al leer header"));
    return false;
  }

  if (header.magic != BLOCK_MAGIC)
  {
    debugPrint(F("JW_FRAM::readBlock() magic invalido"));
    return false;
  }

  if (header.version != expectedVersion)
  {
    debugPrint(F("JW_FRAM::readBlock() version incompatible"));
    return false;
  }

  if (header.length != sizeof(T))
  {
    debugPrint(F("JW_FRAM::readBlock() tamano de payload incompatible"));
    return false;
  }

  if (!read(addr + sizeof(header), reinterpret_cast<uint8_t *>(&value), sizeof(T)))
  {
    debugPrint(F("JW_FRAM::readBlock() fallo al leer payload"));
    return false;
  }

  const uint16_t checksum =
      computeChecksum(reinterpret_cast<const uint8_t *>(&value), sizeof(T));

  if (checksum != header.checksum)
  {
    debugPrint(F("JW_FRAM::readBlock() checksum invalido"));
    return false;
  }

  return true;
}

#endif
