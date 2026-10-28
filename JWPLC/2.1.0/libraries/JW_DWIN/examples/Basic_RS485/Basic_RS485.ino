/*
  Basic_RS485

  DWIN con firmware DGUS Modbus RTU (esclavo 1, 115200 8N1) en Serial2.

  Mapa de ejemplo (ajustar al proyecto DGUS):
  - 0x1000: valor numérico (contador)
  - 0x10A0: texto de 12 caracteres
  - 0x1031: tecla (ID 4, código 5)
*/

#include <JW_DWIN.h>
#include <JW_DWIN_ModbusRTU.h>

JW_RS485 rs485(Serial2, 16, 17);
JW_ModbusRTU modbus(rs485);
JW_DWIN_ModbusRTU transport(modbus, 1, 115200, SERIAL_8N1);
JW_DWIN dwin(transport);

DWIN_Value counterValue = {0x10, 0x00, 0, 0, 0};
DWIN_String message = {0x10, 0xA0, "", 12};
DWIN_Key button = {0x10, 0x31, 4, 5};

uint16_t counter = 0;

void setup()
{
    Serial.begin(115200);

    rs485.setAutoDirection();
    dwin.setDebug(Serial, DWIN_DBG_ERR);

    if (!dwin.begin())
    {
        Serial.println(dwin.lastErrorString());
    }

    dwin.setPage(0x10);
    dwin.sendText(message, "JW_DWIN v0.1");
    dwin.enableTouch(button, 1);
}

void loop()
{
    // Tras recuperar la pantalla, volver a la página actual.
    if (dwin.reconnected())
    {
        dwin.setPage(dwin.currentPage());
    }

    dwin.checkKey(button);
    if (button.Enable)
    {
        button.Enable = 0;
        counter++;
    }

    dwin.sendValue(counterValue, counter);
    dwin.sendText(message, dwin.isOnline() ? "EN LINEA" : "SIN ENLACE");
}
