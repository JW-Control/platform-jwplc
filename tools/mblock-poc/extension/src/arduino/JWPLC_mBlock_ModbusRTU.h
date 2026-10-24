// JWPLC_mBlock_ModbusRTU.h
//
// Modbus RTU por RS485 para los bloques de mBlock, sobre JWPLC_ModbusRTU del
// package. Aparte de JWPLC_mBlock.h: sólo lo incluyen los bloques Modbus RTU.
//
//   JWPLC_ModbusEasy.beginSlave(2, 115200); // publica JWPLC_Registers (40001..40016)
//   JWPLC_ModbusEasy.beginMaster(115200);
//   JWPLC_ModbusEasy.readRegister(2, 1);    // registro 1 (40001) del esclavo 2, -1 si falla
//   JWPLC_ModbusEasy.writeRegister(2, 1, 5);
//
// El bloque que inicia Modbus añade JWPLC_ModbusRTU.task() en loop() para
// atender las tramas. No usar a la vez que el texto por RS485.

#ifndef JWPLC_MBLOCK_MODBUS_RTU_H
#define JWPLC_MBLOCK_MODBUS_RTU_H

#include "JWPLC_mBlock.h"
#include <JWPLC_ModbusRTU.h>

class JWPLC_ModbusEasyHelper
{
public:
    // Esclavo: los 16 registros de JWPLC_Registers como Holding Registers.
    bool beginSlave(int slaveId, uint32_t baud)
    {
        JWPLC_ModbusRTU.setHoldingRegisters(JWPLC_Registers.data(), JWPLC_RegistersHelper::COUNT);
        return JWPLC_ModbusRTU.begin((uint8_t)slaveId, baud, SERIAL_8N1);
    }

    // Maestro: 247 es el ID local interno (como en los ejemplos del package).
    bool beginMaster(uint32_t baud)
    {
        return JWPLC_ModbusRTU.begin(247, baud, SERIAL_8N1);
    }

    // Lectura FC03 de un registro (1 = 40001). -1 si no responde o hay error.
    double readRegister(int slaveId, int number)
    {
        uint16_t value = 0;
        if (number < 1 || !JWPLC_ModbusRTU.readHoldingRegistersSync((uint8_t)slaveId, (uint16_t)(number - 1), 1, &value, 1000))
            return -1;
        return value;
    }

    // Escritura FC06 de un registro (1 = 40001). true si el esclavo respondió.
    bool writeRegister(int slaveId, int number, double value)
    {
        if (number < 1)
            return false;
        const uint16_t v = (uint16_t)constrain(lround(value), 0L, 65535L);
        return JWPLC_ModbusRTU.writeSingleRegisterSync((uint8_t)slaveId, (uint16_t)(number - 1), v, 1000);
    }
};

inline JWPLC_ModbusEasyHelper JWPLC_ModbusEasy;

#endif // JWPLC_MBLOCK_MODBUS_RTU_H
