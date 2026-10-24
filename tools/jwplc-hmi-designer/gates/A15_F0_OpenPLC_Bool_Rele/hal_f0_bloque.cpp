// ===== JWPLC HMI · F0 (Alpha15 -> Alpha14) ==================================
// Bloque para pegar en hal/jwplcbasic.cpp del VPP, justo después de
// #include <JWPLC_ModbusRTU.h>. Ver README.md de esta carpeta.
//
// 1) Aquí va el contenido de JWPLC_HMI_Generated.h (sin "#pragma once").
//    El HAL se copia solo (como arduino.cpp) al sketch de OpenPLC, así que en
//    F0 el header se pega; en F3 lo escribirá el editor junto al HAL.
//
// @@JWPLC_HMI_GENERATED@@
//
// 2) Enlace PLC -> HMI. Relé Q0_0 = %QX0.0 = bool_output[0][0].

static void jwplcHMIF0Begin()
{
    // Campos, página y entrada/salida de la IDLE: lo que se configuró en el
    // Designer (Proyecto · Pantalla y arranque). En este gate: arranca en la
    // IDLE, cualquier tecla entra a la página y ESC vuelve.
    jwplcHMISetup();
}

static void jwplcHMIF0Update()
{
    // bool_output[0][0] es NULL si el programa no declara una variable en %QX0.0.
    if (bool_output[0][0] != NULL)
    {
        rele_q0_0 = (*bool_output[0][0] != 0);
    }

    // Sólo setters: el display redibuja el campo únicamente si cambió.
    jwplcUIUpdate();
}
// ===== fin JWPLC HMI · F0 ===================================================
