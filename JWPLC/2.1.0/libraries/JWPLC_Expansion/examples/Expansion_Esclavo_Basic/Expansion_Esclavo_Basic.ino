// JWPLC Basic como módulo de expansión (DI8/DO8) de otro JWPLC con Runtime.
//
// De fábrica responde en el ID 247 a 115200 8N1. Desde el maestro:
// Configuración > Expansión > Agregar módulo (conectar solo este equipo).
// El maestro le asigna ID, estado seguro de cada salida y vigilancia; el
// esclavo lo guarda en su FRAM.
//
// Pantalla RUN: el título muestra "Esclavo ID n" o "Esclavo fabrica", el LED
// RUN indica que el maestro está presente y ERR "FS" que se aplicó el estado
// seguro porque el maestro dejó de escribir las salidas.
//
// Volver a fábrica: mantener ESC + OK durante 3 s al encender.
//
// Configuración fija desde el sketch (el maestro la lee y no la cambia):
//   JWPLC_ExpansionSlave.setSafeState(1, SAFE_ON);   // Q1 encendida sin maestro
//   JWPLC_ExpansionSlave.setWatchdog(2000);           // 2 s sin maestro
//   JWPLC_ExpansionSlave.begin(3, 115200, SERIAL_8N1);
#include <JWPLC_Expansion.h>

void setup()
{
    Serial.begin(115200);
    Serial.println("JWPLC Basic - Expansion_Esclavo_Basic");
    JWPLC_ExpansionSlave.begin();
}

void loop()
{
}
