/*
  JWPLC Basic - 02.Runtime_En_Vivo

  El Runtime de panel (programación estilo LOGO! 8 en el equipo) con la
  pantalla en vivo en el navegador: lo que se programa en el panel se ve en
  grande en la laptop o en el proyector, y también se puede manejar desde ahí.

  Cable directo laptop <-> JWPLC (sin router): el equipo usa la IP fija
  192.168.10.10. En la laptop, una sola vez: Configuración > Red > Ethernet >
  Asignación de IP > Manual, IPv4 192.168.10.1, máscara 255.255.255.0.
  Luego abrir http://192.168.10.10/ en el navegador.

  Con un router o switch de por medio, cambiar la IP de abajo por una libre de
  esa red. MENU > Red muestra la IP que tiene el equipo.

  Con loadDemoIfEmpty(true), si la FRAM no tiene programa se carga el de
  ejemplo (ver Runtime_Panel en JWPLC_Runtime).
*/

#include <JWPLC_Runtime.h>
#include <JWPLC_WebScreen.h>

static const IPAddress IP_EQUIPO(192, 168, 10, 10);
static const IPAddress IP_LAPTOP(192, 168, 10, 1); // puerta de enlace y DNS
static const IPAddress MASCARA(255, 255, 255, 0);

void setup()
{
    Serial.begin(115200);
    Serial.println("JWPLC Basic - 02.Runtime_En_Vivo");

    // Antes de que el sistema arranque Ethernet (en setup()).
    JWPLC_Ethernet.setStaticIP(IP_EQUIPO, IP_LAPTOP, IP_LAPTOP, MASCARA);
    JWPLC_WebScreen.begin();
    JWPLC_Runtime.loadDemoIfEmpty(true);
    JWPLC_Runtime.begin();
}

void loop()
{
    static bool avisado = false;
    if (!avisado && JWPLC_WebScreen.url().length() > 0)
    {
        avisado = true;
        Serial.print("Pantalla en vivo: ");
        Serial.println(JWPLC_WebScreen.url());
    }
}
