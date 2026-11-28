/*
  JWPLC Basic - 01.Panel_Basico

  Qué hace: publica dos variables y una alarma en el panel web del equipo.
  El panel se arma solo: no hay que escribir HTML.

  Qué conectar: el JWPLC Basic a la red (router o cable directo a la laptop).

  Qué mirar:
  1. En el Monitor Serie (115200) sale la dirección del panel y, la primera
     vez, el código de 6 dígitos para crear el usuario administrador.
  2. Abrir esa dirección en el navegador del celular o de la PC, escribir el
     código y elegir la contraseña (mínimo 10 caracteres).
  3. En Inicio se ve el contador en vivo y se puede cambiar el «Límite».
     Cuando el contador pasa el límite se enciende la alarma.
*/

#include <JWPLC_Web.h>

long contador = 0;
long limite = 50;

void setup()
{
    Serial.begin(115200);
    Serial.println("JWPLC Basic - 01.Panel_Basico");

    JWPLC_Web.begin("Panel basico");
    JWPLC_Web.publish("contador", contador).label("Contador").unit("u");
    JWPLC_Web.publish("limite", limite).label("Límite").range(1, 1000).writable(WEB_OPERADOR);
    JWPLC_Web.addAlarm("limite", "Contador sobre el límite", WEB_ALARMA_MEDIA);
}

void loop()
{
    static uint32_t ultimo = 0;
    if (millis() - ultimo >= 1000)
    {
        ultimo = millis();
        contador = (contador + 1) % 100;
    }
    JWPLC_Web.setAlarm("limite", contador > limite);

    // Avisa una vez cuando la red tiene IP.
    static bool avisado = false;
    if (!avisado && JWPLC_Web.url().length() > 0)
    {
        avisado = true;
        Serial.print("Panel web: ");
        Serial.println(JWPLC_Web.url());
        if (!JWPLC_Web.hasUsers())
        {
            Serial.print("Codigo de primer ingreso: ");
            Serial.println(JWPLC_Web.pairingCode());
        }
    }
}
