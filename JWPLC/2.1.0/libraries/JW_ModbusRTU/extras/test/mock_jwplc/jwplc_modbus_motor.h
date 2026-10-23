#ifndef JWPLC_MODBUS_MOTOR_H
#define JWPLC_MODBUS_MOTOR_H
#include <stdint.h>
enum JWPLCModbusMotor : uint8_t
{
    SYNC = 0,
    ASYNC = 1
};
#endif
