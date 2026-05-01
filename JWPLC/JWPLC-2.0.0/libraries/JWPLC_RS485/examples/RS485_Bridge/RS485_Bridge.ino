/*
  JWPLC Basic v1.4.1 - USB <-> RS485 Bridge

  Convierte el puerto USB Serial del ESP32 en un puente hacia RS-485 usando Serial2.

  Hardware:
  - RS485 RX2 = GPIO16
  - RS485 TX2 = GPIO17
  - Transceptor con auto-direccionamiento, por ejemplo MAX13487.
  - No usa DE/RE.

  Uso:
  - Abre Monitor Serie a 115200 baud.
  - Lo que escribas por USB se enviará por RS-485.
  - Lo que llegue por RS-485 se imprimirá en el Monitor Serie.

  Recomendado para prueba:
  - USB Serial: 115200
  - RS485: 115200, SERIAL_8N1
*/

#include <Arduino.h>

static const int RS485_RX_PIN = 16;
static const int RS485_TX_PIN = 17;

static const uint32_t USB_BAUD = 115200;
static const uint32_t RS485_BAUD = 115200;
static const uint32_t RS485_CONFIG = SERIAL_8N1;

// Cambia a true si quieres ver mensajes de arranque.
// Para puente totalmente transparente, déjalo en false.
static const bool PRINT_STARTUP_INFO = true;

void setup()
{
  Serial.begin(USB_BAUD);
  delay(1200);

  Serial2.begin(RS485_BAUD, RS485_CONFIG, RS485_RX_PIN, RS485_TX_PIN);

  if (PRINT_STARTUP_INFO)
  {
    Serial.println();
    Serial.println("JWPLC Basic v1.4.1 USB <-> RS485 Bridge");
    Serial.println("----------------------------------------");
    Serial.print("USB Serial baud: ");
    Serial.println(USB_BAUD);
    Serial.print("RS485 baud: ");
    Serial.println(RS485_BAUD);
    Serial.println("RS485 config: SERIAL_8N1");
    Serial.print("RS485 RX pin: GPIO");
    Serial.println(RS485_RX_PIN);
    Serial.print("RS485 TX pin: GPIO");
    Serial.println(RS485_TX_PIN);
    Serial.println("----------------------------------------");
    Serial.println("Bridge ready.");
    Serial.println("USB -> RS485 and RS485 -> USB");
    Serial.println();
  }
}

void loop()
{
  // USB Serial -> RS485
  while (Serial.available() > 0)
  {
    int value = Serial.read();

    if (value >= 0)
    {
      Serial2.write((uint8_t)value);
    }
  }

  // Asegura que lo enviado por USB salga físicamente por RS485.
  // En transceptores auto-dirección ayuda a cerrar correctamente la transmisión.
  Serial2.flush();

  // RS485 -> USB Serial
  while (Serial2.available() > 0)
  {
    int value = Serial2.read();

    if (value >= 0)
    {
      Serial.write((uint8_t)value);
    }
  }
}