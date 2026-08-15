#ifndef JWPLC_W5500_SOCKET_H
#define JWPLC_W5500_SOCKET_H

#include <Arduino.h>
#include "JWPLC_W5500.h"

class JWPLC_W5500_Socket {
public:
    JWPLC_W5500_Socket(uint8_t sn);
    
    bool begin(uint8_t protocol, uint16_t port);
    void close();
    void disconnect();
    
    bool connect(const uint8_t* ip, uint16_t port);
    
    // Status
    uint8_t getStatus();
    
    // TX/RX
    size_t getTXFreeSize();
    size_t getRXReceivedSize();
    
    // Read/Write
    size_t write(const uint8_t* buffer, size_t size);
    size_t read(uint8_t* buffer, size_t size);
    
    // Funciones comando directo
    void sendCommand(uint8_t cmd);

private:
    uint8_t _sn; // Número de socket (0-7)
    
    void setSnPORT(uint16_t port);
    void setSnDIPR(const uint8_t* ip);
    void setSnDPORT(uint16_t port);
};

#endif // JWPLC_W5500_SOCKET_H
