#include "JWPLC_W5500_Socket.h"

JWPLC_W5500_Socket::JWPLC_W5500_Socket(uint8_t sn) : _sn(sn) {
}

void JWPLC_W5500_Socket::sendCommand(uint8_t cmd) {
    JWPLC_W5500.write8(W5500_Sn_CR, W5500_Sn_REG(_sn), cmd);
    // Esperar a que el comando se procese
    uint32_t start = millis();
    while (JWPLC_W5500.read8(W5500_Sn_CR, W5500_Sn_REG(_sn))) {
        if (millis() - start > 500) break; // Timeout
    }
}

bool JWPLC_W5500_Socket::begin(uint8_t protocol, uint16_t port) {
    close();
    JWPLC_W5500.write8(W5500_Sn_MR, W5500_Sn_REG(_sn), protocol);
    setSnPORT(port);
    sendCommand(W5500_CR_OPEN);
    
    uint8_t status = getStatus();
    if (status == W5500_SOCK_INIT || status == W5500_SOCK_UDP || status == W5500_SOCK_MACRAW) {
        return true;
    }
    return false;
}

void JWPLC_W5500_Socket::close() {
    sendCommand(W5500_CR_CLOSE);
    JWPLC_W5500.write8(W5500_Sn_IR, W5500_Sn_REG(_sn), 0xFF);
}

void JWPLC_W5500_Socket::disconnect() {
    sendCommand(W5500_CR_DISCON);
}

bool JWPLC_W5500_Socket::connect(const uint8_t* ip, uint16_t port) {
    setSnDIPR(ip);
    setSnDPORT(port);
    sendCommand(W5500_CR_CONNECT);
    return true;
}

uint8_t JWPLC_W5500_Socket::getStatus() {
    return JWPLC_W5500.read8(W5500_Sn_SR, W5500_Sn_REG(_sn));
}

size_t JWPLC_W5500_Socket::getTXFreeSize() {
    uint16_t val = 0, val1 = 0;
    do {
        val1 = JWPLC_W5500.read16(W5500_Sn_TX_FSR, W5500_Sn_REG(_sn));
        if (val1 != 0) {
            val = JWPLC_W5500.read16(W5500_Sn_TX_FSR, W5500_Sn_REG(_sn));
        }
    } while (val != val1);
    return val;
}

size_t JWPLC_W5500_Socket::getRXReceivedSize() {
    uint16_t val = 0, val1 = 0;
    do {
        val1 = JWPLC_W5500.read16(W5500_Sn_RX_RSR, W5500_Sn_REG(_sn));
        if (val1 != 0) {
            val = JWPLC_W5500.read16(W5500_Sn_RX_RSR, W5500_Sn_REG(_sn));
        }
    } while (val != val1);
    return val;
}

size_t JWPLC_W5500_Socket::write(const uint8_t* buffer, size_t size) {
    size_t freeSize = getTXFreeSize();
    if (size > freeSize) size = freeSize;
    if (size == 0) return 0;
    
    uint16_t ptr = JWPLC_W5500.read16(W5500_Sn_TX_WR, W5500_Sn_REG(_sn));
    JWPLC_W5500.writeBlock(ptr, W5500_Sn_TX(_sn), buffer, size);
    ptr += size;
    JWPLC_W5500.write16(W5500_Sn_TX_WR, W5500_Sn_REG(_sn), ptr);
    sendCommand(W5500_CR_SEND);
    
    return size;
}

size_t JWPLC_W5500_Socket::read(uint8_t* buffer, size_t size) {
    size_t rxSize = getRXReceivedSize();
    if (size > rxSize) size = rxSize;
    if (size == 0) return 0;
    
    uint16_t ptr = JWPLC_W5500.read16(W5500_Sn_RX_RD, W5500_Sn_REG(_sn));
    JWPLC_W5500.readBlock(ptr, W5500_Sn_RX(_sn), buffer, size);
    ptr += size;
    JWPLC_W5500.write16(W5500_Sn_RX_RD, W5500_Sn_REG(_sn), ptr);
    sendCommand(W5500_CR_RECV);
    
    return size;
}

void JWPLC_W5500_Socket::setSnPORT(uint16_t port) {
    JWPLC_W5500.write16(W5500_Sn_PORT, W5500_Sn_REG(_sn), port);
}

void JWPLC_W5500_Socket::setSnDIPR(const uint8_t* ip) {
    for (int i = 0; i < 4; i++) {
        JWPLC_W5500.write8(W5500_Sn_DIPR + i, W5500_Sn_REG(_sn), ip[i]);
    }
}

void JWPLC_W5500_Socket::setSnDPORT(uint16_t port) {
    JWPLC_W5500.write16(W5500_Sn_DPORT, W5500_Sn_REG(_sn), port);
}
