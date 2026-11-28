/*
  JWPLC Basic - 04.Runtime_En_Vivo

  Qué hace: el Runtime de panel (programación estilo LOGO! 8 en el equipo)
  con el panel web: la pantalla en vivo se ve en grande en la laptop o en el
  proyector y también se puede manejar desde ahí.

  Qué conectar: cable directo laptop <-> JWPLC (sin router). El equipo usa la
  IP fija 192.168.10.10. En la laptop, una sola vez: Configuración > Red >
  Ethernet > Asignación de IP > Manual, IPv4 192.168.10.1, máscara
  255.255.255.0. Con un router o switch de por medio, cambiar la IP de abajo
  por una libre de esa red.

  Qué mirar: en el equipo, MENU > Red > Conectar celular muestra el QR y el
  código de primer ingreso. Abrir http://192.168.10.10/ (o escanear el QR),
  escribir el código y crear el administrador. Luego, pestaña Pantalla.

  Con loadDemoIfEmpty(true), si la FRAM no tiene programa se carga el de
  ejemplo (ver Runtime_Panel en JWPLC_Runtime).
*/

#include <JWPLC_Runtime.h>
#include <JWPLC_Web.h>

static const IPAddress IP_EQUIPO(192, 168, 10, 10);
static const IPAddress IP_LAPTOP(192, 168, 10, 1); // puerta de enlace y DNS
static const IPAddress MASCARA(255, 255, 255, 0);

void setup()
{
    Serial.begin(115200);
    Serial.println("JWPLC Basic - 04.Runtime_En_Vivo");

    // Antes de que el sistema arranque Ethernet (en setup()).
    JWPLC_Ethernet.setStaticIP(IP_EQUIPO, IP_LAPTOP, IP_LAPTOP, MASCARA);
    JWPLC_Web.begin("Runtime en vivo");
    JWPLC_Runtime.loadDemoIfEmpty(true);
    JWPLC_Runtime.begin();
}

void loop()
{
    static bool avisado = false;
    if (!avisado && JWPLC_Web.url().length() > 0)
    {
        avisado = true;
        Serial.print("Panel web: ");
        Serial.println(JWPLC_Web.url());
    }
}
