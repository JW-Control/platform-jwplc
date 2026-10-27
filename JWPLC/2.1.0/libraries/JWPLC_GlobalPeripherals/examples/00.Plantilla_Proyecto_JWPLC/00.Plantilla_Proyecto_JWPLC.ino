/*
  00.Plantilla_Proyecto_JWPLC

  Plantilla estándar de proyecto para el JWPLC Basic.
  Para empezar un proyecto nuevo: Archivo -> Guardar como... con el nombre del proyecto.

  Cada pestaña tiene una sola responsabilidad y siempre está en el mismo lugar:

    A_Config.h          datos del proyecto y parámetros fijos
    B_Tags.h            nombres de E/S y variables globales
    C_Entradas          leer entradas
    D_Logica            lógica de control
    E_Alarmas           alarmas
    F_Salidas           escribir salidas
    G_HMI               pantalla y botonera
    H_Comunicaciones    Modbus RTU / Modbus TCP
    I_Persistencia      FRAM (consignas y contadores que sobreviven a un corte)
    J_Diagnostico       Serial y estado del ciclo
    K_Utilidades.h      temporizadores y flancos (no se edita)

  La ayuda sin internet está en LEEME.md y en la carpeta docs/ de este proyecto.

  QUÉ VA AQUÍ:    sólo el orden del ciclo. Normalmente no se toca.
  QUÉ NO VA AQUÍ: lógica, lecturas o escrituras de E/S (van en su pestaña).
*/

#include "A_Config.h"
#include "K_Utilidades.h"
#include "B_Tags.h"

// Si el JWPLC HMI Designer escribió sus pantallas en esta carpeta, se usan solas.
#if __has_include("JWPLC_HMI_Generated.h")
#include "JWPLC_HMI_Generated.h"
#define PROYECTO_CON_HMI_DESIGNER 1
#else
#define PROYECTO_CON_HMI_DESIGNER 0
#endif

#if USAR_MODBUS_TCP
#include <JWPLC_ModbusTCP.h>
#endif

void setup()
{
    diagnosticoInicio();     // J
    salidasApagarTodo();     // F: arrancar siempre con las salidas apagadas
    persistenciaCargar();    // I
    hmiInicio();             // G
    comunicacionesInicio();  // H
}

// Un ciclo de scan, como en un PLC. Nada de delay(): cada sección vuelve enseguida.
// Las alarmas se evalúan antes que la lógica para que reaccione en el mismo ciclo.
void loop()
{
    leerEntradas();          // C
    evaluarAlarmas();        // E
    ejecutarLogica();        // D
    escribirSalidas();       // F
    actualizarHMI();         // G
    atenderComunicaciones(); // H
    guardarPersistencia();   // I
    diagnosticoCiclo();      // J
}
