/*
  02.Red_Solo_Cable

  Qué hace:
  - Lo mismo que 01.Red_Cable_o_WiFi pero solo por cable: sin incluir
    JWPLC_WiFi el programa no lleva la pila WiFi y corre en JWPLC Sim.
  - Cada 10 s pide una página por HTTP con JWPLC_RedClient e imprime la
    primera línea de la respuesta.

  Qué conectar:
  - Cable Ethernet a un router con Internet (DHCP).

  Qué mirar:
  - Monitor serie a 115200: "Respuesta por Ethernet: HTTP/1.1 200 OK".
  - En JWPLC Sim la placa sale a Internet por la red de la PC.

  Para pasar a WiFi basta agregar #include <JWPLC_WiFi.h>, conectar la red
  y llamar JWPLC_Red.useWiFi() o JWPLC_Red.useAuto(): el resto no cambia.
*/

#include <JWPLC_Red.h>

const char SERVIDOR[] = "example.com";
const uint16_t PUERTO = 80;

void terminar();

void setup()
{
    Serial.begin(115200);
    JWPLC_Red.useEthernet(); // es el modo por defecto; se deja explícito
}

JWPLC_RedClient cliente;
uint32_t ultimaPeticion = 0;
uint32_t inicioEspera = 0;
bool esperando = false;
String linea = "";

void pedirPagina()
{
    JWPLC_Red.printStatus(Serial);
    if (!JWPLC_Red.isReady())
        return;
    if (!cliente.connect(SERVIDOR, PUERTO))
    {
        Serial.println("No se pudo conectar al servidor");
        return;
    }
    cliente.print("GET / HTTP/1.1\r\nHost: ");
    cliente.print(SERVIDOR);
    cliente.print("\r\nConnection: close\r\n\r\n");
    linea = "";
    inicioEspera = millis();
    esperando = true;
}

void leerRespuesta()
{
    // Lee lo que haya llegado sin detener el loop; como máximo 3 s.
    while (cliente.available())
    {
        char c = cliente.read();
        if (c == '\n')
        {
            terminar();
            return;
        }
        if (c != '\r')
            linea += c;
    }
    if (millis() - inicioEspera >= 3000 || !cliente.connected())
        terminar();
}

void terminar()
{
    cliente.stop();
    esperando = false;
    Serial.print("Respuesta por ");
    Serial.print(JWPLC_Red.activeName());
    Serial.print(": ");
    Serial.println(linea.length() ? linea : String("(sin respuesta)"));
}

void loop()
{
    if (esperando)
    {
        leerRespuesta();
        return;
    }
    if (millis() - ultimaPeticion >= 10000)
    {
        ultimaPeticion = millis();
        pedirPagina();
    }
}
