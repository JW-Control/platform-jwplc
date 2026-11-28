/*
  01.Cloud_Variables_Alarmas

  Qué hace:
  - Manda a JW Cloud, cada 10 s, el estado de un motor: marcha (I0.0),
    salida Q0.0, horas de trabajo y arranques. La parada de emergencia (I0.1
    abierta) es una alarma: llega como aviso al celular del cliente.
  - El loop nunca espera a la red: JWPLC_Cloud envía desde una tarea aparte.
    Sin red guarda las muestras y las manda al volver.

  Antes de subir:
  - JW Control da de alta el equipo en JW Cloud y entrega su token: pegarlo
    en TOKEN.

  Qué conectar:
  - Cable Ethernet a un router con Internet (DHCP).
  - I0.0 pulsador o selector de marcha; I0.1 parada de emergencia (NC).

  Qué mirar:
  - Monitor serie a 115200: "JW Cloud: EN LINEA, 12 envíos".
  - En la app de JW Cloud: Equipos JWPLC.
  - En JWPLC Sim, para probar con un JW Cloud en la PC (node
    scripts/cloud-dev.mjs 8788 --plc --push): SERVIDOR "192.168.250.1",
    PUERTO 8788, TLS false y el token que muestra cloud-dev.

  Para WiFi: agregar #include <JWPLC_WiFi.h>, conectar la red y llamar
  JWPLC_Red.useAuto(): el resto del sketch no cambia.
*/

#include <JWPLC_Cloud.h>

const char TOKEN[] = "pega-aqui-el-token-del-equipo";
const char SERVIDOR[] = "ota.jw-control.com";
const uint16_t PUERTO = 443;
const bool TLS = true;

uint32_t segundosMarcha = 0;
uint32_t arranques = 0;
bool marchaAntes = false;
uint32_t ultimoSegundo = 0;
uint32_t ultimoAviso = 0;

void setup()
{
    Serial.begin(115200);
    JWPLC_Red.useEthernet();

    JWPLC_Cloud.setServer(SERVIDOR, PUERTO, TLS);
    JWPLC_Cloud.setProgram("Motor v1.0");
    JWPLC_Cloud.addBool("marcha", "Marcha");
    JWPLC_Cloud.addBool("q0", "Contactor");
    JWPLC_Cloud.addNumber("horas", "Horas de trabajo", "h", 2);
    JWPLC_Cloud.addNumber("arranques", "Arranques", "", 0);
    JWPLC_Cloud.addAlarm("PE", "Parada de emergencia");
    JWPLC_Cloud.begin(TOKEN);
}

void loop()
{
    const bool emergencia = !digitalRead(I0_1);
    const bool marcha = digitalRead(I0_0) && !emergencia;
    digitalWrite(Q0_0, marcha);

    if (marcha && !marchaAntes)
        arranques++;
    marchaAntes = marcha;
    if (millis() - ultimoSegundo >= 1000)
    {
        ultimoSegundo += 1000;
        if (marcha)
            segundosMarcha++;
    }

    // Solo guardan el valor: no esperan a la red.
    JWPLC_Cloud.set("marcha", marcha);
    JWPLC_Cloud.set("q0", digitalRead(Q0_0));
    JWPLC_Cloud.set("horas", segundosMarcha / 3600.0);
    JWPLC_Cloud.set("arranques", arranques);
    JWPLC_Cloud.alarm("PE", emergencia);

    if (millis() - ultimoAviso >= 10000)
    {
        ultimoAviso = millis();
        Serial.print("JW Cloud: ");
        Serial.print(JWPLC_Cloud.statusString());
        Serial.print(", ");
        Serial.print(JWPLC_Cloud.sentCount());
        Serial.print(" envíos");
        if (JWPLC_Cloud.pending() > 0)
        {
            Serial.print(", ");
            Serial.print(JWPLC_Cloud.pending());
            Serial.print(" muestras esperando red");
        }
        Serial.println();
    }
}
