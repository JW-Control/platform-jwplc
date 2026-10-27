/*
  Simular_USB

  La pantalla la emula JW DWIN en la PC: Simular -> enlace USB, el puerto COM
  de la tarjeta, 115200 baudios, esclavo 1. El Modbus va en sobres por el
  mismo Serial que el texto; el texto aparece en la consola del simulador.

  El texto que se escribe en esa consola llega por transport.text(). Este
  ejemplo atiende dos comandos por linea:
  - "ping"      -> responde "pong"
  - "cnt <n>"   -> pone el contador en n

  Mapa de ejemplo (el mismo de Basic_RS485; ajustar al proyecto DGUS):
  - 0x1000: valor numérico (contador)
  - 0x10A0: texto de 12 caracteres
  - 0x1031: tecla (ID 4, código 5)
*/

#include <JW_DWIN.h>
#include <JW_DWIN_ModbusUSB.h>

JW_DWIN_ModbusUSB transport(Serial, 1);
JW_DWIN dwin(transport);

DWIN_Value counterValue = {0x10, 0x00, 0, 0, 0};
DWIN_String message = {0x10, 0xA0, "", 12};
DWIN_Key button = {0x10, 0x31, 4, 5};

uint16_t counter = 0;
String line;

void handleCommand(const String &cmd)
{
    if (cmd == "ping")
    {
        Serial.println("pong");
    }
    else if (cmd.startsWith("cnt "))
    {
        counter = cmd.substring(4).toInt();
        Serial.printf("contador = %u\n", counter);
    }
    else if (cmd.length())
    {
        Serial.printf("comando desconocido: %s\n", cmd.c_str());
    }
}

void readConsole()
{
    Stream &console = transport.text();
    while (console.available())
    {
        char c = console.read();
        if (c == '\n')
        {
            line.trim();
            handleCommand(line);
            line = "";
        }
        else if (line.length() < 64)
        {
            line += c;
        }
    }
}

void setup()
{
    Serial.begin(115200);
    Serial.println("Simular_USB: JW_DWIN por USB");

    dwin.setDebug(Serial, DWIN_DBG_ERR);
    dwin.begin();

    dwin.setPage(0x10);
    dwin.sendText(message, "JW_DWIN USB");
    dwin.enableTouch(button, 1);
}

void loop()
{
    readConsole();

    // Tras recuperar la pantalla, volver a la página actual.
    if (dwin.reconnected())
    {
        Serial.println("pantalla en linea");
        dwin.setPage(dwin.currentPage());
    }

    dwin.checkKey(button);
    if (button.Enable)
    {
        button.Enable = 0;
        counter++;
        Serial.printf("tecla: contador = %u\n", counter);
    }

    dwin.sendValue(counterValue, counter);
    dwin.sendText(message, dwin.isOnline() ? "EN LINEA" : "SIN ENLACE");
}
