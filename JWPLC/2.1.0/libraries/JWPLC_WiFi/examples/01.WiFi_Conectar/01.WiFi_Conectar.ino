/*
  01.WiFi_Conectar

  Qué hace:
  - Conecta el JWPLC Basic a una red WiFi por DHCP y vuelve a conectar solo
    si la red se cae o el router se reinicia.
  - Cada 5 s imprime el estado: IP, señal y cuántas veces reconectó.
  - Si no conecta, dice por qué: CLAVE INCORRECTA o RED NO ENCONTRADA.

  Qué conectar:
  - Nada. Solo escribir el nombre y la clave de tu red en RED y CLAVE.

  Qué mirar:
  - Monitor serie a 115200.
  - Apagar y prender el router: el estado pasa a RECONECTANDO y luego a LISTO
    con una reconexión más, sin reiniciar el PLC.

  Nota: JWPLC Sim todavía no simula la radio WiFi; este ejemplo es para la placa.
*/

#include <JWPLC_WiFi.h>

const char RED[] = "MiRed";
const char CLAVE[] = "MiClave";

uint32_t ultimoReporte = 0;

void setup()
{
    Serial.begin(115200);

    // Para IP fija, antes de connect():
    // JWPLC_WiFi.setStaticIP(IPAddress(192, 168, 1, 60), IPAddress(192, 168, 1, 1),
    //                        IPAddress(255, 255, 255, 0), IPAddress(192, 168, 1, 1));

    if (!JWPLC_WiFi.connect(RED, CLAVE))
        Serial.println("Nombre o clave de red inválidos");
}

void loop()
{
    // connect() no bloquea: el loop sigue corriendo mientras se conecta.
    if (millis() - ultimoReporte >= 5000)
    {
        ultimoReporte = millis();
        JWPLC_WiFi.printStatus(Serial);
    }
}
