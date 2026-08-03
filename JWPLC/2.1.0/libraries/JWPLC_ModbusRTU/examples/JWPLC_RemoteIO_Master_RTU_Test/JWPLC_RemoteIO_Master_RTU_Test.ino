/*
  JWPLC_RemoteIO_Master_RTU_Test

  PoC 1 - Master de prueba para JWPLC Basic Remote I/O por Modbus RTU / RS-485

  Objetivo:
  - Usar un JWPLC Basic como Master de pruebas.
  - Consultar un JWPLC Basic Remote I/O Slave con ID fijo.
  - Validar FC2, FC1, FC5 y FC15 sin PC/QModMaster como intermediario.

  No incluye:
  - OpenPLC integrado.
  - Commissioning.
  - FRAM.
  - Modbus TCP.
  - API final de Remote I/O dentro de JWPLC_ModbusRTU.

  Configuración inicial:
  - Target Slave ID: 2
  - Baudrate: 115200
  - Formato: 8N1
  - RS-485: Serial2 RX2=IO16 TX2=IO17

  Cableado:
  - Master RS485 A -> Slave RS485 A
  - Master RS485 B -> Slave RS485 B
  - GND común recomendado en banco de pruebas
*/

#include <Arduino.h>
#include <JWPLC_RS485.h>
#include <JWPLC_ModbusRTU.h>

// -----------------------------------------------------------------------------
// Configuración PoC 1
// -----------------------------------------------------------------------------

static const uint8_t JWPLC_REMOTE_IO_SLAVE_ID = 2;
static const uint32_t JWPLC_REMOTE_IO_BAUDRATE = 115200;
static const uint32_t MODBUS_RESPONSE_TIMEOUT_MS = 1000;
static const uint32_t MODBUS_FRAME_GAP_MS = 4;
static const uint16_t MODBUS_MAX_FRAME = 256;

// -----------------------------------------------------------------------------
// Utilidades de impresión
// -----------------------------------------------------------------------------

static void printHexByte(uint8_t value) {
  if (value < 0x10) {
    Serial.print('0');
  }

  Serial.print(value, HEX);
}

static void printFrame(const __FlashStringHelper *label, const uint8_t *frame, uint16_t length) {
  Serial.print(label);
  Serial.print(F(" ["));
  Serial.print(length);
  Serial.print(F("]: "));

  for (uint16_t i = 0; i < length; i++) {
    printHexByte(frame[i]);

    if (i + 1 < length) {
      Serial.print(' ');
    }
  }

  Serial.println();
}

static const __FlashStringHelper *exceptionName(uint8_t code) {
  switch (code) {
    case 0x01:
      return F("Illegal function");
    case 0x02:
      return F("Illegal data address");
    case 0x03:
      return F("Illegal data value");
    case 0x04:
      return F("Slave device failure");
    default:
      return F("Unknown exception");
  }
}

static void printBitMap(uint8_t value, uint8_t quantity) {
  Serial.print(F("Bits: "));

  for (uint8_t i = 0; i < quantity; i++) {
    Serial.print((value & (1 << i)) ? '1' : '0');

    if (i + 1 < quantity) {
      Serial.print(' ');
    }
  }

  Serial.print(F("  Bitmap: 0x"));
  printHexByte(value);
  Serial.println();
}

// -----------------------------------------------------------------------------
// Transporte RTU usando JWPLC_RS485 + helpers CRC de JWPLC_ModbusRTU
// -----------------------------------------------------------------------------

static void drainRs485() {
  while (JWPLC_RS485.available() > 0) {
    JWPLC_RS485.read();
    delay(1);
  }
}

static bool readResponse(uint8_t *response, uint16_t &length, uint32_t timeoutMs) {
  length = 0;

  const uint32_t startMs = millis();
  uint32_t lastByteMs = 0;
  bool receivedAnyByte = false;

  while ((millis() - startMs) < timeoutMs) {
    while (JWPLC_RS485.available() > 0) {
      const int value = JWPLC_RS485.read();

      if (value < 0) {
        break;
      }

      if (length < MODBUS_MAX_FRAME) {
        response[length++] = (uint8_t)value;
        lastByteMs = millis();
        receivedAnyByte = true;
      } else {
        Serial.println(F("[RTU] ERROR: overflow de buffer RX"));
        length = 0;
        return false;
      }
    }

    if (receivedAnyByte && (millis() - lastByteMs) >= MODBUS_FRAME_GAP_MS) {
      return true;
    }

    delay(1);
  }

  return false;
}

static bool sendRequest(uint8_t *request,
                        uint16_t payloadLength,
                        uint8_t expectedFunction,
                        uint8_t *response,
                        uint16_t &responseLength) {
  drainRs485();

  JWPLC_ModbusRTU.appendCRC(request, payloadLength);
  const uint16_t requestLength = payloadLength + 2;

  printFrame(F("TX"), request, requestLength);

  JWPLC_RS485.write(request, requestLength);
  JWPLC_RS485.flush();

  if (!readResponse(response, responseLength, MODBUS_RESPONSE_TIMEOUT_MS)) {
    Serial.println(F("[RTU] ERROR: timeout esperando respuesta"));
    return false;
  }

  printFrame(F("RX"), response, responseLength);

  if (responseLength < 5) {
    Serial.println(F("[RTU] ERROR: respuesta demasiado corta"));
    return false;
  }

  if (!JWPLC_ModbusRTU.checkCRC(response, responseLength)) {
    Serial.println(F("[RTU] ERROR: CRC inválido en respuesta"));
    return false;
  }

  if (response[0] != JWPLC_REMOTE_IO_SLAVE_ID) {
    Serial.println(F("[RTU] ERROR: Slave ID inesperado"));
    return false;
  }

  if (response[1] == (expectedFunction | 0x80)) {
    Serial.print(F("[RTU] EXCEPTION: "));
    Serial.print(response[2]);
    Serial.print(F(" - "));
    Serial.println(exceptionName(response[2]));
    return false;
  }

  if (response[1] != expectedFunction) {
    Serial.println(F("[RTU] ERROR: Function Code inesperado"));
    return false;
  }

  Serial.println(F("[RTU] CRC OK"));
  return true;
}

// -----------------------------------------------------------------------------
// Requests Remote I/O
// -----------------------------------------------------------------------------

static bool readBits(uint8_t functionCode,
                     const __FlashStringHelper *label,
                     uint16_t start,
                     uint16_t quantity,
                     uint8_t *bitmapOut = nullptr) {
  uint8_t request[8];
  uint8_t response[MODBUS_MAX_FRAME];
  uint16_t responseLength = 0;

  request[0] = JWPLC_REMOTE_IO_SLAVE_ID;
  request[1] = functionCode;
  request[2] = highByte(start);
  request[3] = lowByte(start);
  request[4] = highByte(quantity);
  request[5] = lowByte(quantity);

  Serial.println();
  Serial.println(label);

  if (!sendRequest(request, 6, functionCode, response, responseLength)) {
    return false;
  }

  if (responseLength < 6) {
    Serial.println(F("[RTU] ERROR: respuesta de bits incompleta"));
    return false;
  }

  const uint8_t byteCount = response[2];

  if (byteCount < 1) {
    Serial.println(F("[RTU] ERROR: byte count inválido"));
    return false;
  }

  const uint8_t bitmap = response[3];

  printBitMap(bitmap, (uint8_t)quantity);

  if (bitmapOut != nullptr) {
    *bitmapOut = bitmap;
  }

  return true;
}

static bool readDiscreteInputs(uint8_t *bitmapOut = nullptr) {
  return readBits(0x02, F("FC2 - Read Discrete Inputs I0_0..I0_7"), 0, 8, bitmapOut);
}

static bool readCoils(uint8_t *bitmapOut = nullptr) {
  return readBits(0x01, F("FC1 - Read Coils feedback Q0_0..Q0_7"), 0, 8, bitmapOut);
}

static bool writeSingleCoil(uint16_t address, bool enabled) {
  uint8_t request[8];
  uint8_t response[MODBUS_MAX_FRAME];
  uint16_t responseLength = 0;

  request[0] = JWPLC_REMOTE_IO_SLAVE_ID;
  request[1] = 0x05;
  request[2] = highByte(address);
  request[3] = lowByte(address);
  request[4] = enabled ? 0xFF : 0x00;
  request[5] = 0x00;

  Serial.println();
  Serial.print(F("FC5 - Write Single Coil Q0_"));
  Serial.print(address);
  Serial.print(F(" = "));
  Serial.println(enabled ? F("ON") : F("OFF"));

  if (!sendRequest(request, 6, 0x05, response, responseLength)) {
    return false;
  }

  if (responseLength != 8) {
    Serial.println(F("[RTU] ERROR: respuesta FC5 no tiene longitud esperada"));
    return false;
  }

  for (uint8_t i = 0; i < 6; i++) {
    if (response[i] != request[i]) {
      Serial.println(F("[RTU] ERROR: eco FC5 no coincide con solicitud"));
      return false;
    }
  }

  Serial.println(F("[RTU] FC5 OK"));
  return true;
}

static bool writeMultipleCoils(uint8_t pattern) {
  uint8_t request[10];
  uint8_t response[MODBUS_MAX_FRAME];
  uint16_t responseLength = 0;

  request[0] = JWPLC_REMOTE_IO_SLAVE_ID;
  request[1] = 0x0F;
  request[2] = 0x00;
  request[3] = 0x00;
  request[4] = 0x00;
  request[5] = 0x08;
  request[6] = 0x01;
  request[7] = pattern;

  Serial.println();
  Serial.print(F("FC15 - Write Multiple Coils Q0_0..Q0_7 = 0x"));
  printHexByte(pattern);
  Serial.println();

  if (!sendRequest(request, 8, 0x0F, response, responseLength)) {
    return false;
  }

  if (responseLength != 8) {
    Serial.println(F("[RTU] ERROR: respuesta FC15 no tiene longitud esperada"));
    return false;
  }

  if (response[2] != 0x00 || response[3] != 0x00 ||
      response[4] != 0x00 || response[5] != 0x08) {
    Serial.println(F("[RTU] ERROR: eco FC15 no coincide con start/quantity"));
    return false;
  }

  Serial.println(F("[RTU] FC15 OK"));
  return true;
}

// -----------------------------------------------------------------------------
// Secuencia de validación
// -----------------------------------------------------------------------------

static void printStepResult(const __FlashStringHelper *name, bool ok) {
  Serial.print(ok ? F("[PASS] ") : F("[FAIL] "));
  Serial.println(name);
}

static bool expectBitmap(const __FlashStringHelper *name, uint8_t actual, uint8_t expected) {
  const bool ok = actual == expected;

  Serial.print(ok ? F("[PASS] ") : F("[FAIL] "));
  Serial.print(name);
  Serial.print(F(" esperado=0x"));
  printHexByte(expected);
  Serial.print(F(" actual=0x"));
  printHexByte(actual);
  Serial.println();

  return ok;
}

static void runValidationSequence() {
  Serial.println();
  Serial.println(F("=============================================="));
  Serial.println(F(" JWPLC Remote I/O Master RTU - Validation"));
  Serial.println(F(" ATENCION: esta prueba conmuta salidas Q0_0..Q0_7"));
  Serial.println(F("=============================================="));

  uint8_t passed = 0;
  uint8_t total = 0;
  uint8_t bitmap = 0x00;

  total++;
  bool ok = writeMultipleCoils(0x00);
  printStepResult(F("FC15 apagar todas las salidas"), ok);
  if (ok) passed++;
  delay(200);

  total++;
  ok = readDiscreteInputs(&bitmap);
  printStepResult(F("FC2 leer entradas I0_0..I0_7"), ok);
  if (ok) passed++;
  delay(200);

  total++;
  ok = writeSingleCoil(0, true);
  printStepResult(F("FC5 encender Q0_0"), ok);
  if (ok) passed++;
  delay(300);

  total++;
  ok = readCoils(&bitmap) && expectBitmap(F("Feedback tras Q0_0 ON"), bitmap, 0x01);
  if (ok) passed++;
  delay(200);

  total++;
  ok = writeSingleCoil(0, false);
  printStepResult(F("FC5 apagar Q0_0"), ok);
  if (ok) passed++;
  delay(300);

  total++;
  ok = readCoils(&bitmap) && expectBitmap(F("Feedback tras Q0_0 OFF"), bitmap, 0x00);
  if (ok) passed++;
  delay(200);

  total++;
  ok = writeMultipleCoils(0x55);
  printStepResult(F("FC15 escribir patron 0x55"), ok);
  if (ok) passed++;
  delay(300);

  total++;
  ok = readCoils(&bitmap) && expectBitmap(F("Feedback patron 0x55"), bitmap, 0x55);
  if (ok) passed++;
  delay(200);

  total++;
  ok = writeMultipleCoils(0x00);
  printStepResult(F("FC15 apagar todo al cierre"), ok);
  if (ok) passed++;
  delay(300);

  total++;
  ok = readCoils(&bitmap) && expectBitmap(F("Feedback final 0x00"), bitmap, 0x00);
  if (ok) passed++;

  Serial.println();
  Serial.println(F("=============================================="));
  Serial.print(F("Resultado: "));
  Serial.print(passed);
  Serial.print('/');
  Serial.println(total);
  Serial.println(passed == total ? F("VALIDACION OK") : F("VALIDACION CON FALLAS"));
  Serial.println(F("=============================================="));
}

// -----------------------------------------------------------------------------
// Consola Serial0
// -----------------------------------------------------------------------------

static void printHelp() {
  Serial.println();
  Serial.println(F("Comandos Serial Monitor:"));
  Serial.println(F("  h : mostrar ayuda"));
  Serial.println(F("  r : ejecutar secuencia completa de validacion"));
  Serial.println(F("  i : leer entradas FC2"));
  Serial.println(F("  f : leer feedback salidas FC1"));
  Serial.println(F("  1 : encender Q0_0 por FC5"));
  Serial.println(F("  2 : apagar Q0_0 por FC5"));
  Serial.println(F("  5 : escribir patron 0x55 por FC15"));
  Serial.println(F("  a : encender todas las salidas por FC15"));
  Serial.println(F("  0 : apagar todas las salidas por FC15"));
  Serial.println();
}

static void handleCommand(char command) {
  switch (command) {
    case 'h':
    case 'H':
      printHelp();
      break;

    case 'r':
    case 'R':
      runValidationSequence();
      break;

    case 'i':
    case 'I':
      readDiscreteInputs();
      break;

    case 'f':
    case 'F':
      readCoils();
      break;

    case '1':
      writeSingleCoil(0, true);
      break;

    case '2':
      writeSingleCoil(0, false);
      break;

    case '5':
      writeMultipleCoils(0x55);
      break;

    case 'a':
    case 'A':
      writeMultipleCoils(0xFF);
      break;

    case '0':
      writeMultipleCoils(0x00);
      break;

    case '\r':
    case '\n':
      break;

    default:
      Serial.print(F("Comando no reconocido: "));
      Serial.println(command);
      printHelp();
      break;
  }
}

// -----------------------------------------------------------------------------
// Setup / loop
// -----------------------------------------------------------------------------

void setup() {
  Serial.begin(115200);
  delay(500);

  Serial.println();
  Serial.println(F("=============================================="));
  Serial.println(F(" JWPLC Remote I/O Master RTU Test - PoC 1"));
  Serial.println(F("=============================================="));
  Serial.print(F("Target Slave ID: "));
  Serial.println(JWPLC_REMOTE_IO_SLAVE_ID);
  Serial.print(F("Baudrate: "));
  Serial.println(JWPLC_REMOTE_IO_BAUDRATE);
  Serial.println(F("Formato: 8N1"));
  Serial.println(F("RS-485: Serial2 RX=IO16 TX=IO17"));
  Serial.println(F("Transporte: JWPLC_RS485"));
  Serial.println(F("CRC: JWPLC_ModbusRTU helpers"));

  if (!JWPLC_RS485.begin(JWPLC_REMOTE_IO_BAUDRATE, SERIAL_8N1)) {
    Serial.println(F("[RTU] ERROR: JWPLC_RS485 no pudo iniciar"));
    JWPLC_RS485.printStatus(Serial);
    while (true) {
      delay(1000);
    }
  }

  JWPLC_RS485.printStatus(Serial);

  Serial.println(F("[RTU] Master listo"));
  printHelp();
}

void loop() {
  while (Serial.available() > 0) {
    const char command = (char)Serial.read();
    handleCommand(command);
  }
}
