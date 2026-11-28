/*
  03.Red_Servidor

  Qué hace:
  - Atiende una página web en el puerto 80 con JWPLC_RedServer.
  - La página dice por qué red llegó el pedido (Ethernet o WiFi), la IP del PLC
    y cuántas visitas lleva.
  - Sin #include <JWPLC_WiFi.h> escucha solo por cable y corre en JWPLC Sim.
    Para atender también por WiFi: incluir JWPLC_WiFi, conectar la red (o abrir
    el punto de acceso) y llamar JWPLC_Red.useAuto(): escucha en las dos a la vez.

  Qué conectar:
  - Cable Ethernet a un router (DHCP).

  Qué mirar:
  - Abrir http://<IP del PLC>/ en el navegador (la IP sale en el monitor serie).
  - En JWPLC Sim: http://127.0.0.1:1080/ (el puerto 80 del PLC se publica en el 1080 de la PC).
*/

#include <JWPLC_Red.h>

JWPLC_RedServer servidor(80);
JWPLC_RedClient visita;

uint32_t visitas = 0;
uint32_t inicioVisita = 0;
uint32_t ultimoReporte = 0;
uint8_t finDeLineas = 0; // cuenta \r\n\r\n para saber que terminó el pedido
bool atendiendo = false;

void setup()
{
    Serial.begin(115200);
    JWPLC_Red.useEthernet(); // modo por defecto; con WiFi, useAuto()
    servidor.begin();
}

void responder()
{
    visitas = visitas + 1;
    String cuerpo = "<!doctype html><meta charset=utf-8><title>JWPLC</title>"
                    "<h1>JWPLC Basic</h1><p>Llegaste por <b>";
    cuerpo += visita.network() == RED_WIFI ? "WiFi" : "Ethernet";
    cuerpo += "</b>.</p><p>IP del PLC: ";
    cuerpo += JWPLC_Red.localIP().toString();
    cuerpo += "</p><p>Visitas: ";
    cuerpo += String(visitas);
    cuerpo += "</p>";

    visita.print("HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=utf-8\r\nConnection: close\r\n");
    visita.print("Content-Length: ");
    visita.print(cuerpo.length());
    visita.print("\r\n\r\n");
    visita.print(cuerpo);
    visita.stop();
    atendiendo = false;
    Serial.print("Visita ");
    Serial.print(visitas);
    Serial.print(" por ");
    Serial.println(visita.network() == RED_WIFI ? "WiFi" : "Ethernet");
}

void loop()
{
    if (millis() - ultimoReporte >= 10000)
    {
        ultimoReporte = millis();
        JWPLC_Red.printStatus(Serial);
    }

    if (!atendiendo)
    {
        visita = servidor.available(); // nunca espera
        if (visita)
        {
            atendiendo = true;
            inicioVisita = millis();
            finDeLineas = 0;
        }
        return;
    }

    // Lee el pedido sin detener el loop; basta con llegar al final de los encabezados.
    while (visita.available() > 0)
    {
        const int c = visita.read();
        if (c == '\r' || c == '\n')
            finDeLineas = finDeLineas + 1;
        else
            finDeLineas = 0;
        if (finDeLineas >= 4)
        {
            responder();
            return;
        }
    }
    if (!visita.connected() || millis() - inicioVisita >= 3000)
    {
        visita.stop();
        atendiendo = false;
    }
}
