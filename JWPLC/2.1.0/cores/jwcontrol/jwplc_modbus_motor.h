#ifndef JWPLC_MODBUS_MOTOR_H
#define JWPLC_MODBUS_MOTOR_H

#include <stdint.h>

// Selector comun de motor para las APIs Modbus del package.
//
// ASYNC es el motor recomendado y por defecto para runtime PLC.
// SYNC se conserva para compatibilidad, commissioning y sketches donde
// una llamada bloqueante sea aceptable.
enum JWPLCModbusMotor : uint8_t
{
    SYNC = 0,
    ASYNC = 1
};

#endif // JWPLC_MODBUS_MOTOR_H
