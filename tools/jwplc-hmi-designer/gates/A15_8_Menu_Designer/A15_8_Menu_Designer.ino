// Gate A15-8 (Designer): el mismo MENU que gates/A15_8_Menu, pero declarado
// por el HMI Designer (JWPLC_HMI_Generated.h generado por tests/menu-smoke.cjs).
//
// En la placa: con una sola página el MENU recibe las teclas directamente.
// UP/DOWN mueve, OK edita/confirma, ESC cancela o sube; Serial (115200)
// imprime cada valor confirmado.
#include "JWPLC_HMI_Generated.h"

void setup()
{
    Serial.begin(115200);
    jwplcHMISetup();
}

void loop()
{
    static uint32_t last = 0;
    if (millis() - last >= 500)
    {
        last = millis();
        temperatura += bomba ? -0.3f : 0.2f; // VIEW: el menú repinta solo
        ciclos++;
    }

    uint8_t nodo;
    if (JWPLC_Display.menuChanged(MENU_AJUSTES, &nodo))
    {
        switch (nodo)
        {
        case MENU_AJUSTES_CONSIGNA:
            Serial.printf("consigna=%.1f\n", consigna);
            break;
        case MENU_AJUSTES_BOMBA:
            Serial.printf("bomba=%d\n", bomba);
            break;
        case MENU_AJUSTES_RETARDO:
            Serial.printf("retardo=%ld\n", (long)retardo);
            break;
        case MENU_AJUSTES_ALTA:
            Serial.printf("alarmaAlta=%.0f\n", alarmaAlta);
            break;
        case MENU_AJUSTES_HABILITADA:
            Serial.printf("alarmaHabilitada=%d\n", alarmaHabilitada);
            break;
        default:
            break;
        }
    }
}
