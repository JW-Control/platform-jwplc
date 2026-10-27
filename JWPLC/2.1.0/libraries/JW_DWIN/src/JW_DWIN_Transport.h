#ifndef JW_DWIN_TRANSPORT_H
#define JW_DWIN_TRANSPORT_H

#include <Arduino.h>

// =====================================================
// JW_DWIN_Transport - acceso a la memoria VP de la DWIN
// =====================================================
//
// JW_DWIN no conoce el medio fisico. Un transporte lee y escribe palabras
// de 16 bits en la memoria de variables (VP) de la pantalla.
//
// v0.1: JW_DWIN_ModbusRTU (DGUS con firmware Modbus RTU por RS-485).
// Previsto: transporte UART con el protocolo nativo DGUS (5A A5).
//
// Todas las operaciones son bloqueantes con timeout y devuelven true solo
// si la pantalla confirmo la operacion.

class JW_DWIN_Transport
{
public:
    virtual ~JW_DWIN_Transport() {}

    virtual bool begin() = 0;

    virtual bool readRegisters(uint16_t vp, uint16_t count, uint16_t *destination) = 0;
    virtual bool writeRegister(uint16_t vp, uint16_t value) = 0;
    virtual bool writeRegisters(uint16_t vp, uint16_t count, const uint16_t *source) = 0;

    virtual const char *lastErrorString() const = 0;
};

#endif // JW_DWIN_TRANSPORT_H
