/*
  JWPLC Basic v1.4.1 - Modbus RTU Master Test

  Este sketch convierte el JWPLC Basic v1.4.1 en un master Modbus RTU
  básico para probar el JWPLC Basic v2.0.0 corriendo:

      ModbusRTU_Slave_HoldingRegisters

  Hardware:
  - RS485 RX2 = GPIO16
  - RS485 TX2 = GPIO17
  - Transceptor con auto-dirección, por ejemplo MAX13487.
  - No usa DE/RE.

  Modbus:
  - Target slave ID: 1
  - Baud: 19200
  - Config: SERIAL_8E1
  - Function: 0x03 Read Holding Registers
  - Lee HR0..HR3
*/

#include <Arduino.h>

static const int RS485_RX_PIN = 16;
static const int RS485_TX_PIN = 17;

static const uint32_t USB_BAUD = 115200;
static const uint32_t MODBUS_BAUD = 19200;
static const uint32_t MODBUS_CONFIG = SERIAL_8E1;

static const uint8_t TARGET_SLAVE_ID = 1;
static const uint16_t START_ADDRESS = 0;
static const uint16_t QUANTITY = 4;

static const uint32_t REQUEST_PERIOD_MS = 1000;
static const uint32_t RESPONSE_TIMEOUT_MS = 1000;
static const uint32_t FRAME_GAP_MS = 5;

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
  if (length < 4)
  {
    return false;
  }

  uint16_t received = (uint16_t)frame[length - 2] | ((uint16_t)frame[length - 1] << 8);
  uint16_t calculated = modbusCRC16(frame, length - 2);

  return received == calculated;
}

void clearRS485Input()
{
  while (Serial2.available() > 0)
  {
    Serial2.read();
  }
}

bool readFrame(uint8_t *buffer, size_t maxLength, size_t &length, uint32_t timeoutMs)
{
  length = 0;

  uint32_t startMs = millis();
  uint32_t lastByteMs = 0;
  bool receivedAny = false;

  while ((uint32_t)(millis() - startMs) < timeoutMs)
  {
    while (Serial2.available() > 0)
    {
      int value = Serial2.read();

      if (value < 0)
      {
        break;
      }

      if (length >= maxLength)
      {
        return false;
      }

      buffer[length++] = (uint8_t)value;
      lastByteMs = millis();
      receivedAny = true;
    }

    if (receivedAny && (uint32_t)(millis() - lastByteMs) >= FRAME_GAP_MS)
    {
      return true;
    }

    yield();
  }

  return false;
}

bool readHoldingRegisters(uint8_t slaveId, uint16_t startAddress, uint16_t quantity)
{
  if (quantity == 0 || quantity > 125)
  {
    Serial.println("Invalid quantity");
    return false;
  }

  clearRS485Input();

  uint8_t request[8];
  request[0] = slaveId;
  request[1] = 0x03;
  request[2] = highByte(startAddress);
  request[3] = lowByte(startAddress);
  request[4] = highByte(quantity);
  request[5] = lowByte(quantity);
  appendCRC(request, 6);

  Serial2.write(request, sizeof(request));
  Serial2.flush();

  uint8_t response[256];
  size_t responseLength = 0;

  if (!readFrame(response, sizeof(response), responseLength, RESPONSE_TIMEOUT_MS))
  {
    Serial.println("Read failed: timeout");
    return false;
  }

  if (!checkCRC(response, responseLength))
  {
    Serial.print("Read failed: CRC error | Length: ");
    Serial.println(responseLength);
    return false;
  }

  if (response[0] != slaveId)
  {
    Serial.println("Read failed: wrong slave ID");
    return false;
  }

  if (response[1] & 0x80)
  {
    Serial.print("Modbus exception: 0x");
    Serial.println(response[2], HEX);
    return false;
  }

  if (response[1] != 0x03)
  {
    Serial.println("Read failed: wrong function");
    return false;
  }

  uint8_t byteCount = response[2];

  if (byteCount != quantity * 2)
  {
    Serial.println("Read failed: wrong byte count");
    return false;
  }

  Serial.print("Read OK | ");

  for (uint16_t i = 0; i < quantity; i++)
  {
    uint16_t value = ((uint16_t)response[3 + i * 2] << 8) | response[4 + i * 2];

    Serial.print("HR");
    Serial.print(startAddress + i);
    Serial.print(": ");
    Serial.print(value);

    if (i + 1 < quantity)
    {
      Serial.print(" | ");
    }
  }

  Serial.println();
  return true;
}

void setup()
{
  Serial.begin(USB_BAUD);
  delay(1200);

  Serial2.begin(MODBUS_BAUD, MODBUS_CONFIG, RS485_RX_PIN, RS485_TX_PIN);

  Serial.println();
  Serial.println("JWPLC Basic v1.4.1 Modbus RTU Master Test");
  Serial.println("------------------------------------------");
  Serial.print("Target slave ID: ");
  Serial.println(TARGET_SLAVE_ID);
  Serial.print("RS485 RX pin: GPIO");
  Serial.println(RS485_RX_PIN);
  Serial.print("RS485 TX pin: GPIO");
  Serial.println(RS485_TX_PIN);
  Serial.print("Modbus baud: ");
  Serial.println(MODBUS_BAUD);
  Serial.println("Modbus config: SERIAL_8E1");
  Serial.println("Function: 0x03 Read Holding Registers");
  Serial.println("------------------------------------------");
}

void loop()
{
  static unsigned long lastRequestMs = 0;
  unsigned long now = millis();

  if (now - lastRequestMs >= REQUEST_PERIOD_MS)
  {
    lastRequestMs = now;
    readHoldingRegisters(TARGET_SLAVE_ID, START_ADDRESS, QUANTITY);
  }
}