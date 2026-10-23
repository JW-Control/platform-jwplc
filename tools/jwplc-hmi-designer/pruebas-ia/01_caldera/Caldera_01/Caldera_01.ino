#include "JWPLC_HMI_Generated.h"

void setup()
{
    jwplcHMISetup(); // páginas, campos, MENU e IDLE del Designer
}

void loop()
{
    // Escribe las variables: la pantalla redibuja sólo lo que cambió.
    // Página 01 · Resumen
    temperatura = 0.0f; // p. ej. la lectura de tu sensor
    presion = 0.0f; // p. ej. la lectura de tu sensor
    nivelAgua = 0.0f; // p. ej. la lectura de tu sensor
    quemador = false; // p. ej. digitalRead(I0_0) o el estado de tu lógica
    bomba = false; // p. ej. digitalRead(I0_0) o el estado de tu lógica
    strncpy(modo, "Listo", sizeof(modo) - 1); // TEXT: 6 caracteres (UTF-8: cada tilde ocupa 2 bytes)
    // Página 02 · Proceso
    tempHogar = 0.0f; // p. ej. la lectura de tu sensor
    caudalVapor = 0.0f; // p. ej. la lectura de tu sensor
    caudalAgua = 0.0f; // p. ej. la lectura de tu sensor
    valvulaVapor = false; // p. ej. digitalRead(I0_0) o el estado de tu lógica
    // Página 03 · Alarmas
    alNivelBajo = false; // p. ej. digitalRead(I0_0) o el estado de tu lógica
    alSobrepresion = false; // p. ej. digitalRead(I0_0) o el estado de tu lógica
    alFallaLlama = false; // p. ej. digitalRead(I0_0) o el estado de tu lógica
    strncpy(ultimaAlarma, "Listo", sizeof(ultimaAlarma) - 1); // TEXT: 24 caracteres (UTF-8: cada tilde ocupa 2 bytes)
    // Página 04 · Producción
    vaporTotalKg = 0; // p. ej. un contador de tu lógica
    horometroSeg = 0; // p. ej. un contador de tu lógica
    eficiencia = 0.0f; // p. ej. la lectura de tu sensor
    combustible = 0.0f; // p. ej. la lectura de tu sensor

    // MENU_AJUSTES: el operador cambió un valor (OK en la botonera).
    uint8_t nodo;
    if (JWPLC_Display.menuChanged(MENU_AJUSTES, &nodo))
    {
        switch (nodo)
        {
        case MENU_AJUSTES_CONSIGNA_PRESION:
            // «Consigna presi\242n» -> consignaPresion ya tiene el valor nuevo
            break;
        case MENU_AJUSTES_HISTERESIS:
            // «Hist\202resis» -> histeresis ya tiene el valor nuevo
            break;
        case MENU_AJUSTES_MODO_AUTOMATICO:
            // «Modo autom\240tico» -> modoAuto ya tiene el valor nuevo
            break;
        case MENU_AJUSTES_TEMP__MAXIMA:
            // «Temp. m\240xima» -> tempMax ya tiene el valor nuevo
            break;
        case MENU_AJUSTES_PRESION_MAXIMA:
            // «Presi\242n m\240xima» -> presionMax ya tiene el valor nuevo
            break;
        case MENU_AJUSTES_RETARDO_LLAMA:
            // «Retardo llama» -> retardoLlama ya tiene el valor nuevo
            break;
        case MENU_AJUSTES_PURGA_MANUAL:
            // «Purga manual» -> purga ya tiene el valor nuevo
            break;
        default:
            break;
        }
    }
}
