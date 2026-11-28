/*
  01.Red_Cable_o_WiFi

  Qué hace:
  - Usa la red en modo automático: si el cable Ethernet está conectado sale
    por cable; si no, por WiFi.
  - Cada 10 s pide una página por HTTP con JWPLC_RedClient e imprime por
    qué red salió y la primera línea de la respuesta. El loop nunca se
    detiene esperando a la red.

  Qué conectar:
  - Cable Ethernet a un router con Internet, o la red WiFi de RED y CLAVE,
    o los dos. Desenchufar el cable con el ejemplo corriendo: la próxima
    petición sale por WiFi sola.

  Qué mirar:
  - Monitor serie a 115200: "sale por: Ethernet" o "sale por: WiFi" y
    "HTTP/1.1 200 OK".

  Nota: JWPLC Sim todavía no simula la radio WiFi; para probar en el Sim usar
  02.Red_Solo_Cable.
*/

#include <JWPLC_Red.h>
#include <JWPLC_WiFi.h>

const char RED[] = "MiRed";
const char CLAVE[] = "MiClave";
const char SERVIDOR[] = "example.com";
const uint16_t PUERTO = 80;

void terminar();

void setup()
{
    Serial.begin(115200);
    JWPLC_WiFi.connect(RED, CLAVE);
    JWPLC_Red.useAuto();
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
