#include "JWPLC_W5500.h"
#include "jwplc_spi_bus.h"

JWPLC_W5500_Class JWPLC_W5500;

JWPLC_W5500_Class::JWPLC_W5500_Class() : _csPin(5), _spi(NULL), _busAcquired(false) {}

void JWPLC_W5500_Class::acquireBus() {
    if (_busAcquired) return;
    while (!jwplcSPI_acquire(100)) {
        delay(1);
    }
    _spi->beginTransaction(_spiSettings);
    _busAcquired = true;
}

void JWPLC_W5500_Class::releaseBus() {
    if (!_busAcquired) return;
    _spi->endTransaction();
    jwplcSPI_release();
    _busAcquired = false;
}

bool JWPLC_W5500_Class::begin(uint8_t csPin, SPIClass* spi) {
    _csPin = csPin;
    _spi = spi;
    _spiSettings = SPISettings(JWPLC_W5500_SPI_HZ, MSBFIRST, SPI_MODE0);
    
    pinMode(_csPin, OUTPUT);
    digitalWrite(_csPin, HIGH);
    
    // Iniciar bus SPI de JWPLC
    jwplcSPI_begin();
    
    // Configuración base sin esperas excesivas
    softReset();
    
    return hardwarePresent();
}

void JWPLC_W5500_Class::startTransaction() {
    if (!_busAcquired) {
        while (!jwplcSPI_acquire(1000)) {
            Serial.println("Warning: Mutex SPI ocupado, esperando...");
            delay(10); // Evitar spam muy rápido
        }
        jwplcSPI_deselectAll();
        _spi->beginTransaction(_spiSettings);
    }
    digitalWrite(_csPin, LOW);
}

void JWPLC_W5500_Class::endTransaction() {
    digitalWrite(_csPin, HIGH);
    if (!_busAcquired) {
        _spi->endTransaction();
        jwplcSPI_release();
    }
}

void JWPLC_W5500_Class::sendHeader(uint16_t addr, uint8_t block, uint8_t control) {
    _spi->transfer((addr >> 8) & 0xFF);
    _spi->transfer(addr & 0xFF);
    _spi->transfer((block << 3) | control);
}

bool JWPLC_W5500_Class::hardwarePresent() {
    uint8_t version = read8(W5500_VERSIONR, W5500_COMMON_REG);
    return (version == 0x04);
}

bool JWPLC_W5500_Class::linkUp() {
    uint8_t phyStatus = read8(W5500_PHYCFGR, W5500_COMMON_REG);
    return (phyStatus & 0x01); // bit 0 indica Link
}

void JWPLC_W5500_Class::softReset() {
    write8(W5500_MR, W5500_COMMON_REG, W5500_MR_RST);
    
    // Esperar a que el bit de reset se limpie solo (típicamente muy rápido)
    // Se elimina el delay fijo de 560ms que tenía la librería W5x00 genérica
    uint32_t start = millis();
    while (read8(W5500_MR, W5500_COMMON_REG) & W5500_MR_RST) {
        if (millis() - start > 100) break; // Timeout de seguridad 100ms
    }
}

uint8_t JWPLC_W5500_Class::read8(uint16_t addr, uint8_t block) {
    startTransaction();
    sendHeader(addr, block, W5500_RWB_READ | W5500_FDM1);
    uint8_t val = _spi->transfer(0x00);
    endTransaction();
    return val;
}

void JWPLC_W5500_Class::write8(uint16_t addr, uint8_t block, uint8_t data) {
    startTransaction();
    sendHeader(addr, block, W5500_RWB_WRITE | W5500_FDM1);
    _spi->transfer(data);
    endTransaction();
}

uint16_t JWPLC_W5500_Class::read16(uint16_t addr, uint8_t block) {
    startTransaction();
    sendHeader(addr, block, W5500_RWB_READ | W5500_FDM2);
    uint16_t val = _spi->transfer(0x00) << 8;
    val |= _spi->transfer(0x00);
    endTransaction();
    return val;
}

void JWPLC_W5500_Class::write16(uint16_t addr, uint8_t block, uint16_t data) {
    startTransaction();
    sendHeader(addr, block, W5500_RWB_WRITE | W5500_FDM2);
    _spi->transfer((data >> 8) & 0xFF);
    _spi->transfer(data & 0xFF);
    endTransaction();
}

void JWPLC_W5500_Class::readBlock(uint16_t addr, uint8_t block, uint8_t* buffer, size_t len) {
    startTransaction();
    sendHeader(addr, block, W5500_RWB_READ | W5500_VDM);
    // Para optimizar a frecuencias altas usaremos transferBytes en vez de transfer() byte a byte
    _spi->transferBytes(NULL, buffer, len);
    endTransaction();
}

void JWPLC_W5500_Class::writeBlock(uint16_t addr, uint8_t block, const uint8_t* buffer, size_t len) {
    startTransaction();
    sendHeader(addr, block, W5500_RWB_WRITE | W5500_VDM);
    _spi->transferBytes(buffer, NULL, len);
    endTransaction();
}

void JWPLC_W5500_Class::setBuffers(uint8_t txSize, uint8_t rxSize) {
    // Configura el tamaño de los buffers para los sockets.
    for (int i = 0; i < JWPLC_W5500_MAX_SOCKETS; i++) {
        write8(W5500_Sn_TXBUF_SIZE, W5500_Sn_REG(i), txSize);
        write8(W5500_Sn_RXBUF_SIZE, W5500_Sn_REG(i), rxSize);
    }
}

void JWPLC_W5500_Class::setSocketBuffer(uint8_t socket, uint8_t txSize, uint8_t rxSize) {
    if (socket < JWPLC_W5500_MAX_SOCKETS) {
        write8(W5500_Sn_TXBUF_SIZE, W5500_Sn_REG(socket), txSize);
        write8(W5500_Sn_RXBUF_SIZE, W5500_Sn_REG(socket), rxSize);
    }
}
