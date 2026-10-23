// Lado original: JWPLC_ModbusRTU.cpp del package sobre un JWPLC_RS485 minimo.
#include "JWPLC_ModbusRTU.h"
HardwareSerial g_uart;
JWPLC_RS485Class JWPLC_RS485;
#define MB JWPLC_ModbusRTU
#define MB_CLASS JWPLC_ModbusRTUClass
#define MB_ASYNC ASYNC
#define MB_SYNC SYNC
#include "scenarios.h"
int main()
{
    runScenarios();
    return 0;
}
