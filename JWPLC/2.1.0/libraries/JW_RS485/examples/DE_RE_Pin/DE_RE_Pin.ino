/*
  DE_RE_Pin

  Transceptor clasico (MAX485 o similar) con DE y /RE unidos a un GPIO.
  La libreria pone el pin en alto durante cada transmision y lo devuelve
  a bajo cuando el ultimo byte salio de la UART.
*/

#include <JW_RS485.h>

static constexpr int8_t RS485_RX = JWPLC_RS485_RX_PIN;
static constexpr int8_t RS485_TX = JWPLC_RS485_TX_PIN;
static constexpr int8_t RS485_DE = 4;

JW_RS485 rs485(Serial2, RS485_RX, RS485_TX);

void setup()
{
    Serial.begin(115200);

    rs485.setDirectionPin(RS485_DE, true);

    if (!rs485.begin(9600, SERIAL_8N1))
    {
        Serial.println(rs485.lastErrorString());
        return;
    }

    rs485.printStatus(Serial);
}

void loop()
{
    static uint32_t lastSendMs = 0;

    if (millis() - lastSendMs >= 500)
    {
        lastSendMs = millis();
        const uint8_t frame[] = {0x01, 0x03, 0x00, 0x00, 0x00, 0x01, 0x84, 0x0A};
        rs485.write(frame, sizeof(frame));
    }
}
