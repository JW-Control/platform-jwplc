#include <Arduino.h>
#include <JWPLC_RS485.h>
#include <JWPLC_ModbusRTU.h>

// -----------------------------------------------------------------------------
// JWPLC Remote I/O Slave RTU - PoC 1 ampliada
// -----------------------------------------------------------------------------
//
// Soporta:
//
// FC1  Read Coils              -> feedback Q0_0..Q0_7
// FC2  Read Discrete Inputs    -> I0_0..I0_7
// FC3  Read Holding Registers  -> holdingRegisters[0..63]
// FC4  Read Input Registers    -> inputRegisters[0..63]
// FC5  Write Single Coil       -> Q0_x
// FC6  Write Single Register   -> holdingRegisters[x]
// FC15 Write Multiple Coils    -> Q0_0..Q0_7
// FC16 Write Multiple Registers-> holdingRegisters[x..y]
//
// Transporte oficial:
// - JWPLC_RS485.begin()
// - JWPLC_RS485.read/write/flush()
//
// Helpers Modbus:
// - JWPLC_ModbusRTU.checkCRC()
// - JWPLC_ModbusRTU.appendCRC()
//
// -----------------------------------------------------------------------------

static const uint8_t JWPLC_REMOTE_IO_SLAVE_ID = 2;
static const uint32_t JWPLC_REMOTE_IO_BAUDRATE = 115200;

static const uint16_t MODBUS_MAX_FRAME = 256;
static const uint32_t MODBUS_FRAME_GAP_MS = 4;

static const uint8_t BIT_IO_COUNT = 8;
static const uint16_t REGISTER_COUNT = 64;

// -----------------------------------------------------------------------------
// Mapa físico JWPLC Basic
// -----------------------------------------------------------------------------

static const int INPUT_PINS[BIT_IO_COUNT] = {
  I0_0, I0_1, I0_2, I0_3,
  I0_4, I0_5, I0_6, I0_7
};

static const int OUTPUT_PINS[BIT_IO_COUNT] = {
  Q0_0, Q0_1, Q0_2, Q0_3,
  Q0_4, Q0_5, Q0_6, Q0_7
};

static uint8_t outputState = 0x00;

// -----------------------------------------------------------------------------
// Registros demo para JW Modbus Tool
// -----------------------------------------------------------------------------

static uint16_t holdingRegisters[REGISTER_COUNT];
static uint16_t inputRegisters[REGISTER_COUNT];

// -----------------------------------------------------------------------------
// Buffer RTU
// -----------------------------------------------------------------------------

static uint8_t rxBuffer[MODBUS_MAX_FRAME];
static uint16_t rxLength = 0;
static uint32_t lastRxMs = 0;

// -----------------------------------------------------------------------------
// Utilidades
// -----------------------------------------------------------------------------

static bool validateCrc(const uint8_t *frame, uint16_t length) {
  return JWPLC_ModbusRTU.checkCRC(frame, length);
}

static void appendCrcAndSend(uint8_t *frame, uint16_t lengthWithoutCrc) {
  JWPLC_ModbusRTU.appendCRC(frame, lengthWithoutCrc);

  JWPLC_RS485.write(frame, lengthWithoutCrc + 2);
  JWPLC_RS485.flush();
}

static uint16_t readU16BE(const uint8_t *buffer, uint16_t index) {
  return ((uint16_t)buffer[index] << 8) | buffer[index + 1];
}

static void writeU16BE(uint8_t *buffer, uint16_t index, uint16_t value) {
  buffer[index] = highByte(value);
  buffer[index + 1] = lowByte(value);
}

// -----------------------------------------------------------------------------
// I/O helpers
// -----------------------------------------------------------------------------

static void applyOutputs(uint8_t state) {
  outputState = state;

  for (uint8_t i = 0; i < BIT_IO_COUNT; i++) {
    const bool enabled = (outputState & (1 << i)) != 0;
    digitalWrite(OUTPUT_PINS[i], enabled ? HIGH : LOW);
  }
}

static bool readInputBit(uint8_t index) {
  if (index >= BIT_IO_COUNT) {
    return false;
  }

  return digitalRead(INPUT_PINS[index]) == HIGH;
}

static bool readOutputFeedbackBit(uint8_t index) {
  if (index >= BIT_IO_COUNT) {
    return false;
  }

  return (outputState & (1 << index)) != 0;
}

static uint8_t buildInputBitmap(uint16_t start, uint16_t quantity) {
  uint8_t value = 0x00;

  for (uint16_t i = 0; i < quantity; i++) {
    const uint8_t physicalIndex = start + i;

    if (physicalIndex < BIT_IO_COUNT && readInputBit(physicalIndex)) {
      value |= (1 << i);
    }
  }

  return value;
}

static uint8_t buildOutputBitmap(uint16_t start, uint16_t quantity) {
  uint8_t value = 0x00;

  for (uint16_t i = 0; i < quantity; i++) {
    const uint8_t physicalIndex = start + i;

    if (physicalIndex < BIT_IO_COUNT && readOutputFeedbackBit(physicalIndex)) {
      value |= (1 << i);
    }
  }

  return value;
}

static uint8_t currentInputBitmap() {
  return buildInputBitmap(0, BIT_IO_COUNT);
}

static void updateInputRegisters() {
  const uint32_t uptimeSec = millis() / 1000UL;

  inputRegisters[0] = currentInputBitmap();
  inputRegisters[1] = outputState;
  inputRegisters[2] = (uint16_t)(uptimeSec & 0xFFFF);
  inputRegisters[3] = (uint16_t)((uptimeSec >> 16) & 0xFFFF);

  for (uint16_t i = 4; i < REGISTER_COUNT; i++) {
    inputRegisters[i] = 0x2000 + i;
  }
}

static void initRegisters() {
  for (uint16_t i = 0; i < REGISTER_COUNT; i++) {
    holdingRegisters[i] = 0x1000 + i;
    inputRegisters[i] = 0x2000 + i;
  }

  updateInputRegisters();
}

// -----------------------------------------------------------------------------
// Validación de rangos
// -----------------------------------------------------------------------------

static bool isValidBitRange(uint16_t start, uint16_t quantity) {
  if (quantity == 0) {
    return false;
  }

  if (quantity > BIT_IO_COUNT) {
    return false;
  }

  if (start >= BIT_IO_COUNT) {
    return false;
  }

  if ((start + quantity) > BIT_IO_COUNT) {
    return false;
  }

  return true;
}

static bool isValidRegisterRange(uint16_t start, uint16_t quantity) {
  if (quantity == 0) {
    return false;
  }

  if (start >= REGISTER_COUNT) {
    return false;
  }

  if ((start + quantity) > REGISTER_COUNT) {
    return false;
  }

  return true;
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

// -----------------------------------------------------------------------------
// FC1 - Read Coils / output feedback
// -----------------------------------------------------------------------------

static void handleReadCoils(const uint8_t *request, uint16_t length) {
  if (length != 8) {
    sendException(request[0], request[1], 0x03);
    return;
  }

  const uint16_t start = readU16BE(request, 2);
  const uint16_t quantity = readU16BE(request, 4);

  if (!isValidBitRange(start, quantity)) {
    sendException(request[0], request[1], 0x02);
    return;
  }

  uint8_t response[6];

  response[0] = request[0];
  response[1] = request[1];
  response[2] = 1;
  response[3] = buildOutputBitmap(start, quantity);

  appendCrcAndSend(response, 4);
}

// -----------------------------------------------------------------------------
// FC2 - Read Discrete Inputs
// -----------------------------------------------------------------------------

static void handleReadDiscreteInputs(const uint8_t *request, uint16_t length) {
  if (length != 8) {
    sendException(request[0], request[1], 0x03);
    return;
  }

  const uint16_t start = readU16BE(request, 2);
  const uint16_t quantity = readU16BE(request, 4);

  if (!isValidBitRange(start, quantity)) {
    sendException(request[0], request[1], 0x02);
    return;
  }

  uint8_t response[6];

  response[0] = request[0];
  response[1] = request[1];
  response[2] = 1;
  response[3] = buildInputBitmap(start, quantity);

  appendCrcAndSend(response, 4);
}

// -----------------------------------------------------------------------------
// FC3 / FC4 - Read Registers
// -----------------------------------------------------------------------------

static void handleReadRegisters(const uint8_t *request,
                                uint16_t length,
                                const uint16_t *registerMap) {
  if (length != 8) {
    sendException(request[0], request[1], 0x03);
    return;
  }

  const uint16_t start = readU16BE(request, 2);
  const uint16_t quantity = readU16BE(request, 4);

  if (!isValidRegisterRange(start, quantity)) {
    sendException(request[0], request[1], 0x02);
    return;
  }

  if (quantity > 60) {
    sendException(request[0], request[1], 0x03);
    return;
  }

  const uint8_t byteCount = quantity * 2;

  uint8_t response[5 + (60 * 2)];

  response[0] = request[0];
  response[1] = request[1];
  response[2] = byteCount;

  for (uint16_t i = 0; i < quantity; i++) {
    writeU16BE(response, 3 + (i * 2), registerMap[start + i]);
  }

  appendCrcAndSend(response, 3 + byteCount);
}

// -----------------------------------------------------------------------------
// FC5 - Write Single Coil
// -----------------------------------------------------------------------------

static void handleWriteSingleCoil(const uint8_t *request, uint16_t length) {
  if (length != 8) {
    sendException(request[0], request[1], 0x03);
    return;
  }

  const uint16_t address = readU16BE(request, 2);
  const uint16_t value = readU16BE(request, 4);

  if (address >= BIT_IO_COUNT) {
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

  uint8_t response[8];

  for (uint8_t i = 0; i < 6; i++) {
    response[i] = request[i];
  }

  appendCrcAndSend(response, 6);
}

// -----------------------------------------------------------------------------
// FC6 - Write Single Register
// -----------------------------------------------------------------------------

static void handleWriteSingleRegister(const uint8_t *request, uint16_t length) {
  if (length != 8) {
    sendException(request[0], request[1], 0x03);
    return;
  }

  const uint16_t address = readU16BE(request, 2);
  const uint16_t value = readU16BE(request, 4);

  if (address >= REGISTER_COUNT) {
    sendException(request[0], request[1], 0x02);
    return;
  }

  holdingRegisters[address] = value;

  uint8_t response[8];

  for (uint8_t i = 0; i < 6; i++) {
    response[i] = request[i];
  }

  appendCrcAndSend(response, 6);
}

// -----------------------------------------------------------------------------
// FC15 - Write Multiple Coils
// -----------------------------------------------------------------------------

static void handleWriteMultipleCoils(const uint8_t *request, uint16_t length) {
  if (length < 10) {
    sendException(request[0], request[1], 0x03);
    return;
  }

  const uint16_t start = readU16BE(request, 2);
  const uint16_t quantity = readU16BE(request, 4);
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
// FC16 - Write Multiple Registers
// -----------------------------------------------------------------------------

static void handleWriteMultipleRegisters(const uint8_t *request, uint16_t length) {
  if (length < 11) {
    sendException(request[0], request[1], 0x03);
    return;
  }

  const uint16_t start = readU16BE(request, 2);
  const uint16_t quantity = readU16BE(request, 4);
  const uint8_t byteCount = request[6];

  if (!isValidRegisterRange(start, quantity)) {
    sendException(request[0], request[1], 0x02);
    return;
  }

  if (quantity > 60) {
    sendException(request[0], request[1], 0x03);
    return;
  }

  if (byteCount != (quantity * 2)) {
    sendException(request[0], request[1], 0x03);
    return;
  }

  if (length != (uint16_t)(9 + byteCount)) {
    sendException(request[0], request[1], 0x03);
    return;
  }

  for (uint16_t i = 0; i < quantity; i++) {
    holdingRegisters[start + i] = readU16BE(request, 7 + (i * 2));
  }

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
// Dispatcher Modbus
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
    Serial.println(F("[RTU] CRC invalido"));
    return;
  }

  updateInputRegisters();

  switch (functionCode) {
    case 0x01:
      handleReadCoils(frame, length);
      break;

    case 0x02:
      handleReadDiscreteInputs(frame, length);
      break;

    case 0x03:
      handleReadRegisters(frame, length, holdingRegisters);
      break;

    case 0x04:
      handleReadRegisters(frame, length, inputRegisters);
      break;

    case 0x05:
      handleWriteSingleCoil(frame, length);
      break;

    case 0x06:
      handleWriteSingleRegister(frame, length);
      break;

    case 0x0F:
      handleWriteMultipleCoils(frame, length);
      break;

    case 0x10:
      handleWriteMultipleRegisters(frame, length);
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
  for (uint8_t i = 0; i < BIT_IO_COUNT; i++) {
    pinMode(INPUT_PINS[i], INPUT);
  }

  for (uint8_t i = 0; i < BIT_IO_COUNT; i++) {
    pinMode(OUTPUT_PINS[i], OUTPUT);
  }

  applyOutputs(0x00);
}

void setup() {
  Serial.begin(115200);
  delay(500);

  Serial.println();
  Serial.println(F("================================================"));
  Serial.println(F(" JWPLC Remote I/O Slave RTU - PoC 1 Extended"));
  Serial.println(F("================================================"));
  Serial.print(F("Slave ID: "));
  Serial.println(JWPLC_REMOTE_IO_SLAVE_ID);
  Serial.print(F("Baudrate: "));
  Serial.println(JWPLC_REMOTE_IO_BAUDRATE);
  Serial.println(F("Formato: 8N1"));
  Serial.println(F("RS-485: Serial2 RX=IO16 TX=IO17"));
  Serial.println(F("Transport: JWPLC_RS485"));
  Serial.println(F("CRC: JWPLC_ModbusRTU helpers"));
  Serial.println(F("FC1  -> feedback Q0_0..Q0_7"));
  Serial.println(F("FC2  -> I0_0..I0_7"));
  Serial.println(F("FC3  -> Holding Registers 0..63"));
  Serial.println(F("FC4  -> Input Registers 0..63"));
  Serial.println(F("FC5  -> Write single Q0_x"));
  Serial.println(F("FC6  -> Write single Holding Register"));
  Serial.println(F("FC15 -> Write block Q0_0..Q0_7"));
  Serial.println(F("FC16 -> Write block Holding Registers"));

  setupIo();
  initRegisters();

  if (!JWPLC_RS485.begin(JWPLC_REMOTE_IO_BAUDRATE, SERIAL_8N1)) {
    Serial.println(F("[RTU] ERROR: JWPLC_RS485 no pudo iniciar"));
    JWPLC_RS485.printStatus(Serial);

    while (true) {
      delay(1000);
    }
  }

  JWPLC_RS485.printStatus(Serial);

  Serial.println(F("[RTU] Slave listo"));
}

void loop() {
  while (JWPLC_RS485.available() > 0) {
    const int value = JWPLC_RS485.read();

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