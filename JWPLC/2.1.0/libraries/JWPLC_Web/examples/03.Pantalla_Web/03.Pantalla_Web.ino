/*
  JWPLC Basic - 03.Pantalla_Web

  Qué hace: muestra la pantalla del equipo en vivo en el navegador, con la
  botonera virtual (pestaña «Pantalla» del panel). Sirve para demostraciones
  y soporte remoto. Antes era la librería JWPLC_WebScreen.

  Qué conectar: el JWPLC Basic a la red.

  Qué mirar: abrir la dirección que sale en el Monitor Serie, entrar con el
  usuario y elegir la pestaña Pantalla. Las teclas de la página (o las
  flechas, Enter y Esc del teclado) funcionan como la botonera física, que
  sigue funcionando. El rol Visor solo mira; desde Operador se usan teclas.
*/

#include <JWPLC_Web.h>

void setup()
{
    Serial.begin(115200);
    Serial.println("JWPLC Basic - 03.Pantalla_Web");
    JWPLC_Web.begin("Pantalla en vivo");
}

void loop()
{
    static bool avisado = false;
    if (!avisado && JWPLC_Web.url().length() > 0)
    {
        avisado = true;
        Serial.print("Pantalla en vivo: ");
        Serial.println(JWPLC_Web.url());
        if (!JWPLC_Web.hasUsers())
        {
            Serial.print("Codigo de primer ingreso: ");
            Serial.println(JWPLC_Web.pairingCode());
        }
    }
}
