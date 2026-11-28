/*
  01.TLS_HTTPS_Cable

  Qué hace:
  - Cada 30 s pide https://ota.jw-control.com/health (JW Cloud) por HTTPS con
    JWPLC_TLSClient e imprime la primera línea de la respuesta.
  - Verifica el certificado del servidor con las raíces de Let's Encrypt que
    trae la librería: si alguien se hace pasar por el servidor, no conecta.
  - Si falla, dice por qué: SIN CONEXION, CERTIFICADO NO VALIDO, etc.

  Qué conectar:
  - Cable Ethernet a un router con Internet (DHCP).

  Qué mirar:
  - Monitor serie a 115200: "Respuesta por Ethernet: HTTP/1.1 200 OK".
  - En JWPLC Sim sale a Internet por la PC (el cifrado lo hace la PC).

  Para WiFi: agregar #include <JWPLC_WiFi.h>, conectar la red y llamar
  JWPLC_Red.useAuto(): el resto del sketch no cambia.

  Nota: connect() hace el saludo TLS y puede tardar 1-2 s en la placa. En un
  programa con control en loop(), conectar desde una tarea aparte (como hará
  JWPLC_Cloud) para que el loop nunca espere.
*/

#include <JWPLC_TLS.h>

const char SERVIDOR[] = "ota.jw-control.com";
const char RUTA[] = "/health";

JWPLC_TLSClient cliente;
uint32_t ultimaPeticion = 0;
uint32_t inicioEspera = 0;
bool esperando = false;
bool primera = true;
String linea = "";

void setup()
{
    Serial.begin(115200);
    JWPLC_Red.useEthernet();
}

void pedirPagina()
{
    JWPLC_Red.printStatus(Serial);
    if (!JWPLC_Red.isReady())
        return;
    if (!cliente.connect(SERVIDOR, 443))
    {
        Serial.print("No se pudo conectar por HTTPS: ");
        Serial.println(cliente.errorString());
        return;
    }
    cliente.print("GET ");
    cliente.print(RUTA);
    cliente.print(" HTTP/1.1\r\nHost: ");
    cliente.print(SERVIDOR);
    cliente.print("\r\nConnection: close\r\n\r\n");
    linea = "";
    inicioEspera = millis();
    esperando = true;
}

void terminar()
{
    cliente.stop();
    esperando = false;
}

void leerRespuesta()
{
    // Lee lo que haya llegado sin detener el loop; como máximo 5 s.
    while (cliente.available() > 0)
    {
        const int c = cliente.read();
        if (c == '\n')
        {
            linea.trim();
            Serial.print("Respuesta por ");
            Serial.print(cliente.network() == RED_WIFI ? "WiFi" : "Ethernet");
            Serial.print(": ");
            Serial.println(linea);
            terminar();
            return;
        }
        if (c >= 0 && linea.length() < 80)
            linea += (char)c;
    }
    if (millis() - inicioEspera >= 5000 || !cliente.connected())
    {
        Serial.println("Sin respuesta del servidor");
        terminar();
    }
}

void loop()
{
    if (esperando)
    {
        leerRespuesta();
        return;
    }
    if (primera ? millis() >= 10000 : millis() - ultimaPeticion >= 30000)
    {
        primera = false;
        ultimaPeticion = millis();
        pedirPagina();
    }
}
