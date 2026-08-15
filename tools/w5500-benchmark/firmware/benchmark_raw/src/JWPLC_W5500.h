#ifndef JWPLC_W5500_H
#define JWPLC_W5500_H

#include <Arduino.h>
#include <SPI.h>
#include "JWPLC_W5500_Registers.h"

// Frecuencia inicial para el benchmark
#ifndef JWPLC_W5500_SPI_HZ
#define JWPLC_W5500_SPI_HZ 20000000UL // Probando 20 MHz
#endif

#define JWPLC_W5500_MAX_SOCKETS 8

class JWPLC_W5500_Class {
public:
    JWPLC_W5500_Class();
    
    // Configura el pin CS y la configuración SPI
    bool begin(uint8_t csPin, SPIClass* spi = &SPI);
    
    // Verifica si el hardware W5500 está presente y operativo
    bool hardwarePresent();
    
    // Verifica si hay conexión física Ethernet
    bool linkUp();
    
    // Reinicia el W5500 suavemente (por registro MR)
    void softReset();
    
    // Funciones básicas de lectura/escritura de 1 byte
    uint8_t read8(uint16_t addr, uint8_t block);
    void write8(uint16_t addr, uint8_t block, uint8_t data);
    
    // Funciones básicas de lectura/escritura de 2 bytes
    uint16_t read16(uint16_t addr, uint8_t block);
    void write16(uint16_t addr, uint8_t block, uint16_t data);
    
    // Funciones de bloque (burst)
    void readBlock(uint16_t addr, uint8_t block, uint8_t* buffer, size_t len);
    void writeBlock(uint16_t addr, uint8_t block, const uint8_t* buffer, size_t len);

    // Ajuste de Buffers (4 = 4KB por socket, 2 = 2KB, etc.)
    void setBuffers(uint8_t txSize, uint8_t rxSize);
    void setSocketBuffer(uint8_t socket, uint8_t txSize, uint8_t rxSize);
    
    // Gestión explícita de Mutex para operaciones agrupadas de alto rendimiento
    void acquireBus();
    void releaseBus();

private:
    uint8_t _csPin;
    SPIClass* _spi;
    SPISettings _spiSettings;
    bool _busAcquired;

    // Métodos internos para transacciones SPI
    void startTransaction();
    void endTransaction();
    
    // Envía la cabecera (Dirección + Bloque/Control)
    void sendHeader(uint16_t addr, uint8_t block, uint8_t control);
};

extern JWPLC_W5500_Class JWPLC_W5500;

#endif // JWPLC_W5500_H
