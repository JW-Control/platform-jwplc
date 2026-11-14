// JWPLC_mBlock_ModbusTCP.h
//
// Servidor Modbus TCP (Ethernet, puerto 502, Unit ID 1) para los bloques de
// mBlock, sobre JWPLC_ModbusTCP del package. Publica los 16 registros de
// JWPLC_Registers (40001..40016). El core atiende el servidor solo (hook
// cooperativo); no hace falta nada en loop().

#ifndef JWPLC_MBLOCK_MODBUS_TCP_H
#define JWPLC_MBLOCK_MODBUS_TCP_H

#include "JWPLC_mBlock.h"
#include <JWPLC_ModbusTCP.h>

class JWPLC_ModbusTcpEasyHelper
{
public:
    bool beginServer()
    {
        JWPLC_ModbusTCP.setHoldingRegisters(JWPLC_Registers.data(), JWPLC_RegistersHelper::COUNT);
        return JWPLC_ModbusTCP.beginServer(1, 502);
    }

    bool ready() const { return JWPLC_ModbusTCP.serverReady(); }
};

inline JWPLC_ModbusTcpEasyHelper JWPLC_ModbusTcpEasy;

#endif // JWPLC_MBLOCK_MODBUS_TCP_H
