// Lado nuevo: JW_RS485 + JW_ModbusRTU.
#include "JW_ModbusRTU.h"
HardwareSerial g_uart;
JW_RS485 g_port(g_uart, 16, 17);
JW_ModbusRTU g_mb(g_port);
#define MB g_mb
#define MB_CLASS JW_ModbusRTU
#define MB_ASYNC JW_MODBUS_ASYNC
#define MB_SYNC JW_MODBUS_SYNC
#include "scenarios.h"
int main()
{
    g_port.setAutoDirection(512);
    runScenarios();
    return 0;
}
