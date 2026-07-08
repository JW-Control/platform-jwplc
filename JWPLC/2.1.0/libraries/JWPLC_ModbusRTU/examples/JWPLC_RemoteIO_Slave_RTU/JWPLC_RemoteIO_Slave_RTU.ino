/*
  JWPLC_RemoteIO_Slave_RTU

  PoC 1 - JWPLC Basic Remote I/O por Modbus RTU / RS-485

  Objetivo:
  - Slave ID fijo.
  - Serial2 / RS-485 físico.
  - FC2  lee I0_0..I0_7 como Discrete Inputs.
  - FC1  lee feedback Q0_0..Q0_7 como Coils.
  - FC5  escribe una salida individual Q0_x.
  - FC15 escribe salidas Q0_0..Q0_7 en bloque.

  No incluye:
  - FRAM.
  - Commissioning.
  - UID/MAC.
  - Modbus TCP.
  - OpenPLC integrado al runtime normal.

  Configuración inicial:
  - Slave ID: 2
  - Baudrate: 115200
  - Formato: 8N1
  - RS-485: Serial2 RX2=IO16 TX2=IO17
*/

#include <Arduino.h>

// -----------------------------------------------------------------------------
// Configuración PoC 1
// -----------------------------------------------------------------------------

static const uint8_t JWPLC_REMOTE_IO_SLAVE_ID = 2;
static const uint32_t JWPLC_REMOTE_IO_BAUDRATE = 115200;

// RS-485 físico del JWPLC Basic
static const int JWPLC_RS485_RX_PIN = 16;
static const int JWPLC_RS485_TX_PIN = 17;

// Modbus RTU
static const uint16_t MODBUS_MAX_FRAME = 256;
static const uint32_t MODBUS_FRAME_GAP_MS = 4;

// -----------------------------------------------------------------------------
// Mapa físico JWPLC Basic
// -----------------------------------------------------------------------------

static const int INPUT_PINS[8] = {
  I0_0, I0_1, I0_2, I0_3,
  I0_4, I0_5, I0_6, I0_7
};

static const int OUTPUT_PINS[8] = {
  Q0_0, Q0_1, Q0_2, Q0_3,
  Q0_4, Q0_5, Q0_6, Q0_7
};

static uint8_t outputState = 0x00;

// -----------------------------------------------------------------------------
// Buffer RTU
// -----------------------------------------------------------------------------

static uint8_t rxBuffer[MODBUS_MAX_FRAME];
static uint16_t rxLength = 0;
static uint32_t lastRxMs = 0;

// -----------------------------------------------------------------------------
// CRC16 Modbus
// -----------------------------------------------------------------------------

static uint16_t modbusCrc16(const uint8_t *data, uint16_t length) {
  uint16_t crc = 0xFFFF;

  for (uint16_t i = 0; i < length; i++) {
    crc ^= data[i];

    for (uint8_t bit = 0; bit < 8; bit++) {
      if (crc & 0x0001) {
        crc >>= 1;
        crc ^= 0xA001;
      } else {
        crc >>= 1;
      }
    }
  }

  return crc;
}

static bool validateCrc(const uint8_t *frame, uint16_t length) {
  if (length < 4) {
    return false;
  }

  const uint16_t receivedCrc =
    (uint16_t)frame[length - 2] |
    ((uint16_t)frame[length - 1] << 8);

  const uint16_t calculatedCrc = modbusCrc16(frame, length - 2);

  return receivedCrc == calculatedCrc;
}

static void appendCrcAndSend(uint8_t *frame, uint16_t lengthWithoutCrc) {
  const uint16_t crc = modbusCrc16(frame, lengthWithoutCrc);

  frame[lengthWithoutCrc] = crc & 0xFF;
  frame[lengthWithoutCrc + 1] = (crc >> 8) & 0xFF;

  Serial2.write(frame, lengthWithoutCrc + 2);
  Serial2.flush();
}

// -----------------------------------------------------------------------------
// I/O helpers
// -----------------------------------------------------------------------------

static void applyOutputs(uint8_t state) {
  outputState = state;

  for (uint8_t i = 0; i < 8; i++) {
    const bool enabled = (outputState & (1 << i)) != 0;
    digitalWrite(OUTPUT_PINS[i], enabled ? HIGH : LOW);
  }
}

static bool readInputBit(uint8_t index) {
  if (index >= 8) {
    return false;
  }

  return digitalRead(INPUT_PINS[index]) == HIGH;
}

static bool readOutputFeedbackBit(uint8_t index) {
  if (index >= 8) {
    return false;
  }

  return (outputState & (1 << index)) != 0;
}

static uint8_t buildInputBitmap(uint16_t start, uint16_t quantity) {
  uint8_t value = 0x00;

  for (uint16_t i = 0; i < quantity; i++) {
    const uint8_t physicalIndex = start + i;

    if (physicalIndex < 8 && readInputBit(physicalIndex)) {
      value |= (1 << i);
    }
  }

  return value;
}

static uint8_t buildOutputBitmap(uint16_t start, uint16_t quantity) {
  uint8_t value = 0x00;

  for (uint16_t i = 0; i < quantity; i++) {
    const uint8_t physicalIndex = start + i;

    if (physicalIndex < 8 && readOutputFeedbackBit(physicalIndex)) {
      value |= (1 << i);
    }
  }

  return value;
}

// -----------------------------------------------------------------------------
// Modbus responses
// -----------------------------------------------------------------------------

static void sendException(uint8_t slaveId, uint8_t functionCode, uint8_t exceptionCode) {
  uint8_t response[5];

  response[0] = slaveId;
  response[1] = functionCode | 0x80;
  response[2] = exceptionCode;

  appendCrcAndSend(response, 3);
}

static bool isValidBitRange(uint16_t start, uint16_t quantity) {
  if (quantity == 0) {
    return false;
  }

  if (quantity > 8) {
    return false;
  }

  if (start >= 8) {
    return false;
  }

  if ((start + quantity) > 8) {
    return false;
  }

  return true;
}

// FC2 - Read Discrete Inputs
static void handleReadDiscreteInputs(const uint8_t *request, uint16_t length) {
  if (length != 8) {
    sendException(request[0], request[1], 0x03);
    return;
  }

  const uint16_t start = ((uint16_t)request[2] << 8) | request[3];
  const uint16_t quantity = ((uint16_t)request[4] << 8) | request[5];

  if (!isValidBitRange(start, quantity)) {
    sendException(request[0], request[1], 0x02);
    return;
  }

  uint8_t response[6];

  response[0] = request[0];
  response[1] = request[1];
  response[2] = 1;  // byte count, PoC 1 solo usa hasta 8 bits
  response[3] = buildInputBitmap(start, quantity);

  appendCrcAndSend(response, 4);
}

// FC1 - Read Coils / output feedback
static void handleReadCoils(const uint8_t *request, uint16_t length) {
  if (length != 8) {
    sendException(request[0], request[1], 0x03);
    return;
  }

  const uint16_t start = ((uint16_t)request[2] << 8) | request[3];
  const uint16_t quantity = ((uint16_t)request[4] << 8) | request[5];

  if (!isValidBitRange(start, quantity)) {
    sendException(request[0], request[1], 0x02);
    return;
  }

  uint8_t response[6];

  response[0] = request[0];
  response[1] = request[1];
  response[2] = 1;  // byte count, PoC 1 solo usa hasta 8 bits
  response[3] = buildOutputBitmap(start, quantity);

  appendCrcAndSend(response, 4);
}

// FC5 - Write Single Coil
static void handleWriteSingleCoil(const uint8_t *request, uint16_t length) {
  if (length != 8) {
    sendException(request[0], request[1], 0x03);
    return;
  }

  const uint16_t address = ((uint16_t)request[2] << 8) | request[3];
  const uint16_t value = ((uint16_t)request[4] << 8) | request[5];

  if (address >= 8) {
    sendException(request[0], request[1], 0x02);
    return;
  }

  if (value != 0xFF00 && value != 0x0000) {
    sendException(request[0], request[1], 0x03);
    return;
  }

  if (value == 0xFF00) {
    outputState |= (1 << address);
  } else {
    outputState &= ~(1 << address);
  }

  applyOutputs(outputState);

  // FC5 responde eco exacto de la solicitud
  uint8_t response[8];
  for (uint8_t i = 0; i < 6; i++) {
    response[i] = request[i];
  }

  appendCrcAndSend(response, 6);
}

// FC15 - Write Multiple Coils
static void handleWriteMultipleCoils(const uint8_t *request, uint16_t length) {
  if (length < 10) {
    sendException(request[0], request[1], 0x03);
    return;
  }

  const uint16_t start = ((uint16_t)request[2] << 8) | request[3];
  const uint16_t quantity = ((uint16_t)request[4] << 8) | request[5];
  const uint8_t byteCount = request[6];

  if (!isValidBitRange(start, quantity)) {
    sendException(request[0], request[1], 0x02);
    return;
  }

  const uint8_t expectedByteCount = (quantity + 7) / 8;

  if (byteCount != expectedByteCount) {
    sendException(request[0], request[1], 0x03);
    return;
  }

  if (length != (uint16_t)(9 + byteCount)) {
    sendException(request[0], request[1], 0x03);
    return;
  }

  const uint8_t coilData = request[7];

  for (uint16_t i = 0; i < quantity; i++) {
    const uint8_t physicalIndex = start + i;
    const bool enabled = (coilData & (1 << i)) != 0;

    if (enabled) {
      outputState |= (1 << physicalIndex);
    } else {
      outputState &= ~(1 << physicalIndex);
    }
  }

  applyOutputs(outputState);

  uint8_t response[8];

  response[0] = request[0];
  response[1] = request[1];
  response[2] = request[2];
  response[3] = request[3];
  response[4] = request[4];
  response[5] = request[5];

  appendCrcAndSend(response, 6);
}

// -----------------------------------------------------------------------------
// Modbus dispatcher
// -----------------------------------------------------------------------------

static void processModbusFrame(const uint8_t *frame, uint16_t length) {
  if (length < 4) {
    return;
  }

  const uint8_t slaveId = frame[0];
  const uint8_t functionCode = frame[1];

  if (slaveId != JWPLC_REMOTE_IO_SLAVE_ID) {
    return;
  }

  if (!validateCrc(frame, length)) {
    Serial.println(F("[RTU] CRC inválido"));
    return;
  }

  switch (functionCode) {
    case 0x01:
      handleReadCoils(frame, length);
      break;

    case 0x02:
      handleReadDiscreteInputs(frame, length);
      break;

    case 0x05:
      handleWriteSingleCoil(frame, length);
      break;

    case 0x0F:
      handleWriteMultipleCoils(frame, length);
      break;

    default:
      sendException(slaveId, functionCode, 0x01);
      break;
  }
}

// -----------------------------------------------------------------------------
// Setup / loop
// -----------------------------------------------------------------------------

static void setupIo() {
  for (uint8_t i = 0; i < 8; i++) {
    pinMode(INPUT_PINS[i], INPUT);
  }

  for (uint8_t i = 0; i < 8; i++) {
    pinMode(OUTPUT_PINS[i], OUTPUT);
  }

  applyOutputs(0x00);
}

void setup() {
  Serial.begin(115200);
  delay(500);

  Serial.println();
  Serial.println(F("=============================================="));
  Serial.println(F(" JWPLC Remote I/O Slave RTU - PoC 1"));
  Serial.println(F("=============================================="));
  Serial.print(F("Slave ID: "));
  Serial.println(JWPLC_REMOTE_IO_SLAVE_ID);
  Serial.print(F("Baudrate: "));
  Serial.println(JWPLC_REMOTE_IO_BAUDRATE);
  Serial.println(F("Formato: 8N1"));
  Serial.println(F("RS-485: Serial2 RX=IO16 TX=IO17"));
  Serial.println(F("FC2  -> I0_0..I0_7"));
  Serial.println(F("FC1  -> feedback Q0_0..Q0_7"));
  Serial.println(F("FC5  -> write single Q0_x"));
  Serial.println(F("FC15 -> write block Q0_0..Q0_7"));

  setupIo();

  Serial2.begin(
    JWPLC_REMOTE_IO_BAUDRATE,
    SERIAL_8N1,
    JWPLC_RS485_RX_PIN,
    JWPLC_RS485_TX_PIN
  );

  Serial.println(F("[RTU] Slave listo"));
}

void loop() {
  while (Serial2.available() > 0) {
    const int value = Serial2.read();

    if (value < 0) {
      break;
    }

    if (rxLength < MODBUS_MAX_FRAME) {
      rxBuffer[rxLength++] = (uint8_t)value;
      lastRxMs = millis();
    } else {
      rxLength = 0;
      Serial.println(F("[RTU] Buffer overflow"));
    }
  }

  if (rxLength > 0 && (millis() - lastRxMs) >= MODBUS_FRAME_GAP_MS) {
    processModbusFrame(rxBuffer, rxLength);
    rxLength = 0;
  }
}