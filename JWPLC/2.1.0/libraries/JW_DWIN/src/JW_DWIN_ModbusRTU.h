#ifndef JW_DWIN_MODBUS_RTU_H
#define JW_DWIN_MODBUS_RTU_H

#include "JW_DWIN_Transport.h"
#include <JW_ModbusRTU.h>

// =====================================================
// JW_DWIN_ModbusRTU - transporte DWIN por Modbus RTU / RS-485
// =====================================================
//
// Para pantallas DWIN con firmware DGUS Modbus RTU (esclavo).
// Usa la API Sync de JW_ModbusRTU: cada operacion espera la respuesta de la
// pantalla o el timeout.

#ifndef JW_DWIN_MODBUS_DEFAULT_TIMEOUT_MS
#define JW_DWIN_MODBUS_DEFAULT_TIMEOUT_MS 150
#endif

// Id Modbus propio del ESP32. No se atiende como esclavo; solo debe ser
// distinto del id de la pantalla.
#ifndef JW_DWIN_MODBUS_LOCAL_ID
#define JW_DWIN_MODBUS_LOCAL_ID 247
#endif

class JW_DWIN_ModbusRTU : public JW_DWIN_Transport
{
public:
    JW_DWIN_ModbusRTU(JW_ModbusRTU &modbus,
                      uint8_t slaveId = 1,
                      uint32_t baud = 115200,
                      uint32_t config = SERIAL_8N1)
        : _modbus(modbus),
          _slaveId(slaveId),
          _baud(baud),
          _config(config),
          _timeoutMs(JW_DWIN_MODBUS_DEFAULT_TIMEOUT_MS)
    {
    }

    void setTimeout(uint32_t timeoutMs) { _timeoutMs = timeoutMs; }
    uint32_t timeout() const { return _timeoutMs; }
    uint8_t slaveId() const { return _slaveId; }

    bool begin() override
    {
        // TX bloqueante: igual que ModbusMaster en JW_DWIN_RS485 1.x.
        _modbus.setQueuedTxEnabled(false);
        return _modbus.begin(JW_DWIN_MODBUS_LOCAL_ID, _baud, _config);
    }

    bool readRegisters(uint16_t vp, uint16_t count, uint16_t *destination) override
    {
        return _modbus.readHoldingRegistersSync(_slaveId, vp, count, destination, _timeoutMs);
    }

    bool writeRegister(uint16_t vp, uint16_t value) override
    {
        return _modbus.writeSingleRegisterSync(_slaveId, vp, value, _timeoutMs);
    }

    bool writeRegisters(uint16_t vp, uint16_t count, const uint16_t *source) override
    {
        return _modbus.writeMultipleRegistersSync(_slaveId, vp, count, source, _timeoutMs);
    }

    const char *lastErrorString() const override
    {
        return _modbus.lastErrorString();
    }

private:
    JW_ModbusRTU &_modbus;
    uint8_t _slaveId;
    uint32_t _baud;
    uint32_t _config;
    uint32_t _timeoutMs;
};

#endif // JW_DWIN_MODBUS_RTU_H
