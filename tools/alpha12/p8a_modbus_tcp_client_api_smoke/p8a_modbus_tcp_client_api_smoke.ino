/*
  P8A Modbus TCP Client API link smoke

  Objetivo:
  - obligar al compilador/linker a resolver las ocho operaciones públicas
    declaradas por JWPLC_ModbusTCPClient;
  - no realizar upload ni tráfico de red;
  - mantener el gate aislado de los ejemplos de usuario.

  Nota:
  - no se llama begin(); por tanto, si este sketch se cargara accidentalmente,
    las requests terminan localmente como NOT_CONFIGURED y no abren sockets.
*/

#include <JWPLC_ModbusTCP.h>

static uint8_t bitBuffer[2] = {0};
static uint8_t coilWriteBuffer[2] = {0x55, 0x01};
static uint16_t registerBuffer[2] = {0};
static uint16_t registerWriteBuffer[2] = {0x1234, 0x5678};

static volatile uint8_t linkSmokeSink = 0;

static void exerciseClientApi()
{
    linkSmokeSink ^= (uint8_t)JWPLC_ModbusTCPClient.requestReadCoils(
        0, 8, bitBuffer, 1000);

    linkSmokeSink ^= (uint8_t)JWPLC_ModbusTCPClient.requestReadDiscreteInputs(
        0, 8, bitBuffer, 1000);

    linkSmokeSink ^=
        (uint8_t)JWPLC_ModbusTCPClient.requestReadHoldingRegisters(
            0, 2, registerBuffer, 1000);

    linkSmokeSink ^=
        (uint8_t)JWPLC_ModbusTCPClient.requestReadInputRegisters(
            0, 2, registerBuffer, 1000);

    linkSmokeSink ^= (uint8_t)JWPLC_ModbusTCPClient.requestWriteSingleCoil(
        0, true, 1000);

    linkSmokeSink ^=
        (uint8_t)JWPLC_ModbusTCPClient.requestWriteSingleRegister(
            0, 0x1234, 1000);

    linkSmokeSink ^=
        (uint8_t)JWPLC_ModbusTCPClient.requestWriteMultipleCoils(
            0, 9, coilWriteBuffer, 1000);

    linkSmokeSink ^=
        (uint8_t)JWPLC_ModbusTCPClient.requestWriteMultipleRegisters(
            0, 2, registerWriteBuffer, 1000);

    linkSmokeSink ^= (uint8_t)JWPLC_ModbusTCPClient.result();
}

void setup()
{
    exerciseClientApi();
}

void loop()
{
}
