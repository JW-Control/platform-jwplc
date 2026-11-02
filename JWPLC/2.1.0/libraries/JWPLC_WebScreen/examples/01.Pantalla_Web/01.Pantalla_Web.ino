/*
  JWPLC Basic - 01.Pantalla_Web

  Muestra la pantalla del equipo en vivo en el navegador de la laptop, con
  la botonera virtual. Sirve para demostraciones y soporte remoto.

  1. Conectar el JWPLC Basic a la red (o directo a la laptop con un cable).
  2. Ver la IP en la pantalla de reposo (o en el Monitor Serie).
  3. Abrir http://<IP>/ en el navegador.

  La página repite la TFT tal cual y sus teclas (o las flechas, Enter y Esc
  del teclado) funcionan como la botonera física.
*/

#include <JWPLC_WebScreen.h>

void setup()
{
    Serial.begin(115200);
    Serial.println("JWPLC Basic - 01.Pantalla_Web");
    JWPLC_WebScreen.begin();
}

void loop()
{
    // Avisa una vez cuando la red tiene IP.
    static bool avisado = false;
    if (!avisado && JWPLC_WebScreen.url().length() > 0)
    {
        avisado = true;
        Serial.print("Pantalla en vivo: ");
        Serial.println(JWPLC_WebScreen.url());
    }
}
