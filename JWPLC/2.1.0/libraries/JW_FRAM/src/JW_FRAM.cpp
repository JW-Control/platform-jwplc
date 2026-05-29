#include "JW_FRAM.h"

/// Dispositivos FRAM soportados
const struct {
  uint8_t manufID;
  uint16_t prodID;
  uint32_t size;
} _jw_fram_supported_devices[] = {
    {0x04, 0x0101, 2 * 1024UL},    // MB85RS16*
    {0x04, 0x0302, 8 * 1024UL},    // MB85RS64V*
    {0x04, 0x2303, 8 * 1024UL},    // MB85RS64T*
    {0x04, 0x2503, 32 * 1024UL},   // MB85RS256TY*
    {0x04, 0x0509, 32 * 1024UL},   // MB85RS256B
    {0x04, 0x2603, 64 * 1024UL},   // MB85RS512T
    {0x04, 0x2703, 128 * 1024UL},  // MB85RS1MT*
    {0x04, 0x4803, 256 * 1024UL},  // MB85RS2MTA*
    {0x04, 0x4903, 512 * 1024UL},  // MB85RS4MT*
    {0x7F, 0x7F7F, 32 * 1024UL},   // FM25V02
    {0xAE, 0x8305, 8 * 1024UL}     // MR45V064B
};

static uint32_t jw_fram_check_supported_device(uint8_t manufID, uint16_t prodID) {
  for (uint8_t i = 0;
       i < sizeof(_jw_fram_supported_devices) / sizeof(_jw_fram_supported_devices[0]);
       i++) {
    if (manufID == _jw_fram_supported_devices[i].manufID &&
        prodID == _jw_fram_supported_devices[i].prodID) {
      return _jw_fram_supported_devices[i].size;
    }
  }
  return 0;
}

JW_FRAM::JW_FRAM(int8_t cs, SPIClass *theSPI, uint32_t freq) {
  if (_spi_dev) {
    delete _spi_dev;
  }

  _spi_dev = new Adafruit_SPIDevice(cs, freq, SPI_BITORDER_MSBFIRST, SPI_MODE0, theSPI);
}

JW_FRAM::JW_FRAM(int8_t clk, int8_t miso, int8_t mosi, int8_t cs) {
  if (_spi_dev) {
    delete _spi_dev;
  }

  _spi_dev = new Adafruit_SPIDevice(cs, clk, miso, mosi, 1000000,
                                    SPI_BITORDER_MSBFIRST, SPI_MODE0);
}

bool JW_FRAM::begin(uint32_t forcedSizeBytes) {
  if (_spi_dev == nullptr) {
    debugPrint(F("JW_FRAM::begin() SPI device nulo"));
    return false;
  }

  if (!_spi_dev->begin()) {
    debugPrint(F("JW_FRAM::begin() fallo en SPI begin"));
    return false;
  }

  if (forcedSizeBytes > 0) {
    _framSizeBytes = forcedSizeBytes;
    setAddressSize(forcedSizeBytes > (64UL * 1024UL) ? 3 : 2);
    debugPrint(F("JW_FRAM::begin() usando tamano de FRAM forzado"));
    debugPrintValue(F("Size bytes: "), _framSizeBytes);
    return true;
  }

  uint8_t manufID = 0;
  uint16_t prodID = 0;
  if (!getDeviceID(&manufID, &prodID)) {
    debugPrint(F("JW_FRAM::begin() fallo al leer device ID"));
    return false;
  }

  _framSizeBytes = jw_fram_check_supported_device(manufID, prodID);
  if (_framSizeBytes == 0) {
    debugPrint(F("JW_FRAM::begin() dispositivo no soportado, usa forcedSizeBytes"));
    debugPrintValue(F("Manufacturer ID: "), manufID, HEX);
    debugPrintValue(F("Product ID: "), prodID, HEX);
    return false;
  }

  setAddressSize(_framSizeBytes > (64UL * 1024UL) ? 3 : 2);

  debugPrint(F("JW_FRAM::begin() dispositivo detectado"));
  debugPrintValue(F("Size bytes: "), _framSizeBytes);
  return true;
}

void JW_FRAM::setBusLockCallbacks(JW_FRAM_BusLockCallback lockCallback,
                                  JW_FRAM_BusUnlockCallback unlockCallback,
                                  void *userData,
                                  uint32_t timeoutMs) {
  _busLockCallback = lockCallback;
  _busUnlockCallback = unlockCallback;
  _busLockUserData = userData;
  _busLockTimeoutMs = timeoutMs;
}

void JW_FRAM::clearBusLockCallbacks() {
  _busLockCallback = nullptr;
  _busUnlockCallback = nullptr;
  _busLockUserData = nullptr;
  _busLockTimeoutMs = 50;
}

bool JW_FRAM::lockBus() {
  if (_busLockCallback == nullptr) {
    return true;
  }

  return _busLockCallback(_busLockTimeoutMs, _busLockUserData);
}

void JW_FRAM::unlockBus() {
  if (_busUnlockCallback != nullptr) {
    _busUnlockCallback(_busLockUserData);
  }
}

bool JW_FRAM::spiWrite(const uint8_t *buffer, size_t len) {
  if (_spi_dev == nullptr) {
    return false;
  }

  if (!lockBus()) {
    debugPrint(F("JW_FRAM::spiWrite() no se pudo bloquear el bus SPI"));
    return false;
  }

  const bool ok = _spi_dev->write(buffer, len);
  unlockBus();

  return ok;
}

bool JW_FRAM::spiWrite(const uint8_t *buffer, size_t len,
                       const uint8_t *prefixBuffer, size_t prefixLen) {
  if (_spi_dev == nullptr) {
    return false;
  }

  if (!lockBus()) {
    debugPrint(F("JW_FRAM::spiWrite() no se pudo bloquear el bus SPI"));
    return false;
  }

  const bool ok = _spi_dev->write(buffer, len, prefixBuffer, prefixLen);
  unlockBus();

  return ok;
}

bool JW_FRAM::spiWriteThenRead(const uint8_t *writeBuffer, size_t writeLen,
                               uint8_t *readBuffer, size_t readLen) {
  if (_spi_dev == nullptr) {
    return false;
  }

  if (!lockBus()) {
    debugPrint(F("JW_FRAM::spiWriteThenRead() no se pudo bloquear el bus SPI"));
    return false;
  }

  const bool ok = _spi_dev->write_then_read(writeBuffer, writeLen, readBuffer, readLen);
  unlockBus();

  return ok;
}

uint32_t JW_FRAM::size() const { return _framSizeBytes; }

uint8_t JW_FRAM::addressSize() const { return _addressSizeBytes; }

bool JW_FRAM::isAddressValid(uint32_t addr, size_t len) const {
  if (_framSizeBytes == 0) {
    return false;
  }

  if (len == 0) {
    return addr < _framSizeBytes;
  }

  if (addr >= _framSizeBytes) {
    return false;
  }

  const uint64_t end = static_cast<uint64_t>(addr) + static_cast<uint64_t>(len);
  return end <= static_cast<uint64_t>(_framSizeBytes);
}

bool JW_FRAM::writeEnable(bool enable) {
  uint8_t cmd = enable ? JW_FRAM_OPCODE_WREN : JW_FRAM_OPCODE_WRDI;
  return spiWrite(&cmd, 1);
}

bool JW_FRAM::write8(uint32_t addr, uint8_t value) {
  if (!isAddressValid(addr, 1)) {
    debugPrint(F("JW_FRAM::write8() direccion fuera de rango"));
    return false;
  }

  uint8_t buffer[5];
  uint8_t i = 0;

  buffer[i++] = JW_FRAM_OPCODE_WRITE;
  if (_addressSizeBytes > 2) {
    buffer[i++] = static_cast<uint8_t>(addr >> 16);
  }
  buffer[i++] = static_cast<uint8_t>(addr >> 8);
  buffer[i++] = static_cast<uint8_t>(addr & 0xFF);
  buffer[i++] = value;

  return spiWrite(buffer, i);
}

uint8_t JW_FRAM::read8(uint32_t addr) {
  if (!isAddressValid(addr, 1)) {
    debugPrint(F("JW_FRAM::read8() direccion fuera de rango"));
    return 0;
  }

  uint8_t buffer[4];
  uint8_t value = 0;
  uint8_t i = 0;

  buffer[i++] = JW_FRAM_OPCODE_READ;
  if (_addressSizeBytes > 2) {
    buffer[i++] = static_cast<uint8_t>(addr >> 16);
  }
  buffer[i++] = static_cast<uint8_t>(addr >> 8);
  buffer[i++] = static_cast<uint8_t>(addr & 0xFF);

  if (!spiWriteThenRead(buffer, i, &value, 1)) {
    debugPrint(F("JW_FRAM::read8() fallo de lectura SPI"));
    return 0;
  }

  return value;
}

bool JW_FRAM::write(uint32_t addr, const uint8_t *values, size_t count) {
  if (count == 0) {
    return true;
  }

  if (!isAddressValid(addr, count)) {
    debugPrint(F("JW_FRAM::write() direccion fuera de rango"));
    return false;
  }

  uint8_t prebuf[4];
  uint8_t i = 0;

  prebuf[i++] = JW_FRAM_OPCODE_WRITE;
  if (_addressSizeBytes > 2) {
    prebuf[i++] = static_cast<uint8_t>(addr >> 16);
  }
  prebuf[i++] = static_cast<uint8_t>(addr >> 8);
  prebuf[i++] = static_cast<uint8_t>(addr & 0xFF);

  return spiWrite(values, count, prebuf, i);
}

bool JW_FRAM::read(uint32_t addr, uint8_t *values, size_t count) {
  if (count == 0) {
    return true;
  }

  if (!isAddressValid(addr, count)) {
    debugPrint(F("JW_FRAM::read() direccion fuera de rango"));
    return false;
  }

  uint8_t buffer[4];
  uint8_t i = 0;

  buffer[i++] = JW_FRAM_OPCODE_READ;
  if (_addressSizeBytes > 2) {
    buffer[i++] = static_cast<uint8_t>(addr >> 16);
  }
  buffer[i++] = static_cast<uint8_t>(addr >> 8);
  buffer[i++] = static_cast<uint8_t>(addr & 0xFF);

  return spiWriteThenRead(buffer, i, values, count);
}

bool JW_FRAM::getDeviceID(uint8_t *manufacturerID, uint16_t *productID) {
  if (manufacturerID == nullptr || productID == nullptr) {
    debugPrint(F("JW_FRAM::getDeviceID() puntero nulo"));
    return false;
  }

  uint8_t cmd = JW_FRAM_OPCODE_RDID;
  uint8_t rx[4] = {0, 0, 0, 0};

  if (!spiWriteThenRead(&cmd, 1, rx, 4)) {
    return false;
  }

  if (rx[1] == 0x7F) {
    *manufacturerID = rx[0];
    *productID = static_cast<uint16_t>((rx[2] << 8) | rx[3]);
  } else {
    *manufacturerID = rx[0];
    *productID = static_cast<uint16_t>((rx[1] << 8) | rx[2]);
  }

  return true;
}

uint8_t JW_FRAM::getStatusRegister() {
  uint8_t cmd = JW_FRAM_OPCODE_RDSR;
  uint8_t val = 0;

  if (!spiWriteThenRead(&cmd, 1, &val, 1)) {
    debugPrint(F("JW_FRAM::getStatusRegister() fallo de lectura SPI"));
    return 0;
  }

  return val;
}

bool JW_FRAM::setStatusRegister(uint8_t value) {
  uint8_t cmd[2];
  cmd[0] = JW_FRAM_OPCODE_WRSR;
  cmd[1] = value;
  return spiWrite(cmd, 2);
}

void JW_FRAM::setAddressSize(uint8_t nAddressSize) {
  if (nAddressSize < 2) {
    _addressSizeBytes = 2;
  } else if (nAddressSize > 3) {
    _addressSizeBytes = 3;
  } else {
    _addressSizeBytes = nAddressSize;
  }
}

bool JW_FRAM::writeString(uint32_t addr, const String &value) {
  const size_t len = value.length();

  if (len > MAX_TEXT_LENGTH) {
    debugPrint(F("JW_FRAM::writeString() texto demasiado largo"));
    return false;
  }

  if (!isAddressValid(addr, 1 + len)) {
    debugPrint(F("JW_FRAM::writeString() direccion fuera de rango"));
    return false;
  }

  uint8_t buffer[MAX_TEXT_LENGTH + 1];
  buffer[0] = static_cast<uint8_t>(len);
  for (size_t i = 0; i < len; i++) {
    buffer[i + 1] = static_cast<uint8_t>(value[i]);
  }

  if (!writeEnable(true)) {
    debugPrint(F("JW_FRAM::writeString() fallo al habilitar escritura"));
    return false;
  }

  const bool ok = write(addr, buffer, len + 1);

  if (!writeEnable(false)) {
    debugPrint(F("JW_FRAM::writeString() fallo al deshabilitar escritura"));
  }

  return ok;
}

bool JW_FRAM::readString(uint32_t addr, String &value, uint8_t maxLen) {
  value = "";

  if (maxLen > MAX_TEXT_LENGTH) {
    maxLen = MAX_TEXT_LENGTH;
  }

  if (!isAddressValid(addr, 1)) {
    debugPrint(F("JW_FRAM::readString() direccion fuera de rango"));
    return false;
  }

  const uint8_t len = read8(addr);
  if (len > maxLen || len > MAX_TEXT_LENGTH) {
    debugPrint(F("JW_FRAM::readString() longitud almacenada invalida"));
    return false;
  }

  if (!isAddressValid(addr + 1, len)) {
    debugPrint(F("JW_FRAM::readString() payload fuera de rango"));
    return false;
  }

  uint8_t buffer[MAX_TEXT_LENGTH];
  if (len > 0 && !read(addr + 1, buffer, len)) {
    debugPrint(F("JW_FRAM::readString() fallo al leer payload"));
    return false;
  }

  value.reserve(len);
  for (uint8_t i = 0; i < len; i++) {
    value += static_cast<char>(buffer[i]);
  }

  return true;
}

bool JW_FRAM::writeCString(uint32_t addr, const char *str, uint8_t maxLen) {
  if (str == nullptr) {
    debugPrint(F("JW_FRAM::writeCString() puntero nulo"));
    return false;
  }

  if (maxLen > MAX_TEXT_LENGTH) {
    maxLen = MAX_TEXT_LENGTH;
  }

  const size_t len = strnlen(str, static_cast<size_t>(maxLen) + 1);
  if (len > maxLen) {
    debugPrint(F("JW_FRAM::writeCString() texto demasiado largo"));
    return false;
  }

  if (!isAddressValid(addr, len + 1)) {
    debugPrint(F("JW_FRAM::writeCString() direccion fuera de rango"));
    return false;
  }

  if (!writeEnable(true)) {
    debugPrint(F("JW_FRAM::writeCString() fallo al habilitar escritura"));
    return false;
  }

  bool ok = write(addr, reinterpret_cast<const uint8_t *>(str), len + 1);

  if (!writeEnable(false)) {
    debugPrint(F("JW_FRAM::writeCString() fallo al deshabilitar escritura"));
  }

  return ok;
}

bool JW_FRAM::readCString(uint32_t addr, char *buffer, size_t bufferSize, uint8_t maxLen) {
  if (buffer == nullptr || bufferSize == 0) {
    debugPrint(F("JW_FRAM::readCString() buffer invalido"));
    return false;
  }

  if (maxLen > MAX_TEXT_LENGTH) {
    maxLen = MAX_TEXT_LENGTH;
  }

  const size_t bytesToRead = static_cast<size_t>(maxLen) + 1;
  if (!isAddressValid(addr, bytesToRead)) {
    debugPrint(F("JW_FRAM::readCString() direccion fuera de rango"));
    return false;
  }

  uint8_t temp[MAX_TEXT_LENGTH + 1];
  if (!read(addr, temp, bytesToRead)) {
    debugPrint(F("JW_FRAM::readCString() fallo de lectura"));
    return false;
  }

  size_t len = 0;
  bool terminatorFound = false;
  for (; len < bytesToRead; len++) {
    if (temp[len] == '\0') {
      terminatorFound = true;
      break;
    }
  }

  if (!terminatorFound) {
    debugPrint(F("JW_FRAM::readCString() no se encontro terminador"));
    return false;
  }

  if (bufferSize < (len + 1)) {
    debugPrint(F("JW_FRAM::readCString() buffer destino demasiado pequeno"));
    return false;
  }

  memcpy(buffer, temp, len + 1);
  return true;
}

void JW_FRAM::enableDebug(Stream &port) {
  _debugPort = &port;
  _debugEnabled = true;
}

void JW_FRAM::disableDebug() { _debugEnabled = false; }

bool JW_FRAM::debugEnabled() const { return _debugEnabled; }

void JW_FRAM::debugPrint(const __FlashStringHelper *msg) {
  if (_debugEnabled && _debugPort != nullptr) {
    _debugPort->println(msg);
  }
}

void JW_FRAM::debugPrint(const String &msg) {
  if (_debugEnabled && _debugPort != nullptr) {
    _debugPort->println(msg);
  }
}

void JW_FRAM::debugPrint(const char *msg) {
  if (_debugEnabled && _debugPort != nullptr) {
    _debugPort->println(msg);
  }
}

void JW_FRAM::debugPrintValue(const __FlashStringHelper *prefix, uint32_t value, int base) {
  if (_debugEnabled && _debugPort != nullptr) {
    _debugPort->print(prefix);
    _debugPort->println(value, base);
  }
}

uint16_t JW_FRAM::computeChecksum(const uint8_t *data, size_t len) const {
  uint32_t sum = 0;
  for (size_t i = 0; i < len; i++) {
    sum += data[i];
  }
  return static_cast<uint16_t>(sum & 0xFFFF);
}
