/*
  JWPLC Basic v1.4.1 - Modbus RTU Slave externo

  Este sketch convierte el JWPLC Basic v1.4.1 en un slave Modbus RTU
  para probar el JWPLC Basic v2.0.0 corriendo como master con:

    - ModbusRTU_Master_ReadHoldingRegisters
    - ModbusRTU_Master_WriteSingleRegister

  Hardware:
  - RS485 RX2 = GPIO16
  - RS485 TX2 = GPIO17
  - Transceptor con auto-dirección, por ejemplo MAX13487.
  - No usa DE/RE.

  Modbus:
  - Slave ID: 1
  - Baud: 19200
  - Config: SERIAL_8E1

  Funciones soportadas:
  - 0x03 Read Holding Registers
  - 0x06 Write Single Register
  - 0x10 Write Multiple Registers
*/

#include <Arduino.h>

static const int RS485_RX_PIN = 16;
static const int RS485_TX_PIN = 17;

static const uint32_t USB_BAUD = 115200;
static const uint32_t MODBUS_BAUD = 19200;
static const uint32_t MODBUS_CONFIG = SERIAL_8E1;

static const uint8_t SLAVE_ID = 1;
static const uint32_t FRAME_GAP_MS = 5;

static const uint16_t HOLDING_COUNT = 16;
uint16_t holdingRegs[HOLDING_COUNT];

uint8_t rxBuffer[256];
uint16_t rxLength = 0;
uint32_t lastByteMs = 0;

uint32_t rxFrames = 0;
uint32_t txFrames = 0;
uint32_t crcErrors = 0;
uint32_t exceptionsSent = 0;

uint16_t modbusCRC16(const uint8_t *data, size_t length)
{
  uint16_t crc = 0xFFFF;

  for (size_t i = 0; i < length; i++)
  {
    crc ^= data[i];

    for (uint8_t bit = 0; bit < 8; bit++)
    {
      if (crc & 0x0001)
      {
        crc = (crc >> 1) ^ 0xA001;
      }
      else
      {
        crc >>= 1;
      }
    }
  }

  return crc;
}

void appendCRC(uint8_t *frame, size_t payloadLength)
{
  uint16_t crc = modbusCRC16(frame, payloadLength);
  frame[payloadLength] = lowByte(crc);
  frame[payloadLength + 1] = highByte(crc);
}

bool checkCRC(const uint8_t *frame, size_t length)
{
  if (frame == nullptr || length < 4)
  {
    return false;
  }

  uint16_t received = (uint16_t)frame[length - 2] | ((uint16_t)frame[length - 1] << 8);
  uint16_t calculated = modbusCRC16(frame, length - 2);

  return received == calculated;
}

void sendFrame(uint8_t *frame, uint16_t payloadLength)
{
  appendCRC(frame, payloadLength);
  Serial2.write(frame, payloadLength + 2);
  Serial2.flush();
  txFrames++;
}

void sendException(uint8_t functionCode, uint8_t exceptionCode)
{
  uint8_t response[5];

  response[0] = SLAVE_ID;
  response[1] = functionCode | 0x80;
  response[2] = exceptionCode;

  sendFrame(response, 3);
  exceptionsSent++;
}

void handleReadHoldingRegisters(const uint8_t *frame, uint16_t length, bool broadcast)
{
  if (broadcast)
  {
    return;
  }

  if (length != 8)
  {
    sendException(0x03, 0x03); // Illegal Data Value
    return;
  }

  uint16_t start = ((uint16_t)frame[2] << 8) | frame[3];
  uint16_t quantity = ((uint16_t)frame[4] << 8) | frame[5];

  if (quantity == 0 || quantity > 125)
  {
    sendException(0x03, 0x03); // Illegal Data Value
    return;
  }

  if (start >= HOLDING_COUNT || (uint32_t)start + quantity > HOLDING_COUNT)
  {
    sendException(0x03, 0x02); // Illegal Data Address
    return;
  }

  uint8_t response[256];

  response[0] = SLAVE_ID;
  response[1] = 0x03;
  response[2] = quantity * 2;

  for (uint16_t i = 0; i < quantity; i++)
  {
    uint16_t value = holdingRegs[start + i];
    response[3 + i * 2] = highByte(value);
    response[4 + i * 2] = lowByte(value);
  }

  sendFrame(response, 3 + quantity * 2);
}

void handleWriteSingleRegister(const uint8_t *frame, uint16_t length, bool broadcast)
{
  if (length != 8)
  {
    if (!broadcast)
    {
      sendException(0x06, 0x03); // Illegal Data Value
    }
    return;
  }

  uint16_t address = ((uint16_t)frame[2] << 8) | frame[3];
  uint16_t value = ((uint16_t)frame[4] << 8) | frame[5];

  if (address >= HOLDING_COUNT)
  {
    if (!broadcast)
    {
      sendException(0x06, 0x02); // Illegal Data Address
    }
    return;
  }

  holdingRegs[address] = value;

  if (!broadcast)
  {
    uint8_t response[8];

    for (uint8_t i = 0; i < 6; i++)
    {
      response[i] = frame[i];
    }

    sendFrame(response, 6);
  }
}

void handleWriteMultipleRegisters(const uint8_t *frame, uint16_t length, bool broadcast)
{
  if (length < 9)
  {
    if (!broadcast)
    {
      sendException(0x10, 0x03); // Illegal Data Value
    }
    return;
  }

  uint16_t start = ((uint16_t)frame[2] << 8) | frame[3];
  uint16_t quantity = ((uint16_t)frame[4] << 8) | frame[5];
  uint8_t byteCount = frame[6];

  if (quantity == 0 || quantity > 123 || byteCount != quantity * 2 || length != (uint16_t)(9 + byteCount))
  {
    if (!broadcast)
    {
      sendException(0x10, 0x03); // Illegal Data Value
    }
    return;
  }

  if (start >= HOLDING_COUNT || (uint32_t)start + quantity > HOLDING_COUNT)
  {
    if (!broadcast)
    {
      sendException(0x10, 0x02); // Illegal Data Address
    }
    return;
  }

  for (uint16_t i = 0; i < quantity; i++)
  {
    holdingRegs[start + i] = ((uint16_t)frame[7 + i * 2] << 8) | frame[8 + i * 2];
  }

  if (!broadcast)
  {
    uint8_t response[8];

    response[0] = SLAVE_ID;
    response[1] = 0x10;
    response[2] = frame[2];
    response[3] = frame[3];
    response[4] = frame[4];
    response[5] = frame[5];

    sendFrame(response, 6);
  }
}

void processFrame(const uint8_t *frame, uint16_t length)
{
  if (frame == nullptr || length < 4)
  {
    return;
  }

  rxFrames++;

  if (!checkCRC(frame, length))
  {
    crcErrors++;
    Serial.println("CRC error");
    return;
  }

  uint8_t address = frame[0];
  uint8_t functionCode = frame[1];
  bool broadcast = (address == 0);

  if (address != SLAVE_ID && !broadcast)
  {
    return;
  }

  switch (functionCode)
  {
    case 0x03:
      handleReadHoldingRegisters(frame, length, broadcast);
      break;

    case 0x06:
      handleWriteSingleRegister(frame, length, broadcast);
      break;

    case 0x10:
      handleWriteMultipleRegisters(frame, length, broadcast);
      break;

    default:
      if (!broadcast)
      {
        sendException(functionCode, 0x01); // Illegal Function
      }
      break;
  }
}

void clearRxBuffer()
{
  rxLength = 0;
}

void pollModbus()
{
  while (Serial2.available() > 0)
  {
    int value = Serial2.read();

    if (value < 0)
    {
      break;
    }

    if (rxLength >= sizeof(rxBuffer))
    {
      clearRxBuffer();
      Serial.println("RX buffer overflow");
      return;
    }

    rxBuffer[rxLength++] = (uint8_t)value;
    lastByteMs = millis();
  }

  if (rxLength > 0 && (uint32_t)(millis() - lastByteMs) >= FRAME_GAP_MS)
  {
    processFrame(rxBuffer, rxLength);
    clearRxBuffer();
  }
}

void setup()
{
  Serial.begin(USB_BAUD);
  delay(1200);

  Serial2.begin(MODBUS_BAUD, MODBUS_CONFIG, RS485_RX_PIN, RS485_TX_PIN);

  for (uint16_t i = 0; i < HOLDING_COUNT; i++)
  {
    holdingRegs[i] = 2000 + i;
  }

  Serial.println();
  Serial.println("JWPLC Basic v1.4.1 Modbus RTU Slave externo");
  Serial.println("---------------------------------------------");
  Serial.print("Slave ID: ");
  Serial.println(SLAVE_ID);
  Serial.print("RS485 RX pin: GPIO");
  Serial.println(RS485_RX_PIN);
  Serial.print("RS485 TX pin: GPIO");
  Serial.println(RS485_TX_PIN);
  Serial.print("Modbus baud: ");
  Serial.println(MODBUS_BAUD);
  Serial.println("Modbus config: SERIAL_8E1");
  Serial.println("Functions: 0x03, 0x06, 0x10");
  Serial.println("---------------------------------------------");
}

void loop()
{
  pollModbus();

  // HR0 funciona como heartbeat en segundos.
  holdingRegs[0] = (uint16_t)(millis() / 1000);

  static unsigned long lastPrintMs = 0;
  unsigned long now = millis();

  if (now - lastPrintMs >= 1000)
  {
    lastPrintMs = now;

    Serial.print("HR0: ");
    Serial.print(holdingRegs[0]);

    Serial.print(" | HR1: ");
    Serial.print(holdingRegs[1]);

    Serial.print(" | HR2: ");
    Serial.print(holdingRegs[2]);

    Serial.print(" | HR3: ");
    Serial.print(holdingRegs[3]);

    Serial.print(" | RX: ");
    Serial.print(rxFrames);

    Serial.print(" | TX: ");
    Serial.print(txFrames);

    Serial.print(" | CRC errors: ");
    Serial.print(crcErrors);

    Serial.print(" | Exceptions: ");
    Serial.println(exceptionsSent);
  }
}