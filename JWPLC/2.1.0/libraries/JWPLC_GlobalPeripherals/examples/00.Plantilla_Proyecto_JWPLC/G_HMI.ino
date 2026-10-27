/*
  G_HMI · Pantalla y botonera

  QUÉ VA AQUÍ:    lo que se muestra (indicadores RUN/ERR de la pantalla IDLE o las variables
                  de las pantallas del HMI Designer) y lo que hace cada tecla de la botonera.
  QUÉ NO VA AQUÍ: lógica de proceso (D_Logica). Las teclas sólo cambian variables de
                  B_Tags.h; la lógica decide qué hacer con ellas.

  Dos modos, se eligen solos:
  - Sin HMI Designer: pantalla IDLE del package. RUN = máquina en marcha, ERR = código
    de la primera alarma pendiente. ESC reconoce alarmas.
  - Con HMI Designer: al usar «Escribir en sketch» sobre esta carpeta aparece
    JWPLC_HMI_Generated.h y se activa solo. Copia aquí, en actualizarHMI(), las
    asignaciones del «Sketch base» de JWPLC_HMI_LEEME.md.

  Reglas del package: no dibujar en cada ciclo lo que no cambió, no usar TFT_eSPI
  directamente y no llamar JWPLC_Buttons.update() (el package escanea la botonera).
*/

void hmiInicio()
{
#if PROYECTO_CON_HMI_DESIGNER
    jwplcHMISetup(); // páginas, campos, MENU e IDLE del Designer
#else
    JWPLC_Display.setIdleWakeMode(IDLE_WAKE_DISABLED); // las teclas son de la aplicación
    JWPLC_Display.setBusLedAuto(true);
    JWPLC_Display.setEthLedAuto(true);
    JWPLC_Display.setRunLed(false);
    JWPLC_Display.setErrCode("");
#endif
    JWPLC_Buttons.clearPendingInput(); // descarta teclas pulsadas durante el arranque
}

void actualizarHMI()
{
#if PROYECTO_CON_HMI_DESIGNER
    // Asigna aquí las variables de tus pantallas, p. ej.:
    //   marchaMotor = (estado == ESTADO_MARCHA);
    // y atiende JWPLC_Display.menuChanged(...) si el diseño tiene MENU.
#else
    // Indicadores de la pantalla IDLE: sólo se envían cuando cambian.
    static bool runAnterior = false;
    const bool run = (estado == ESTADO_MARCHA);
    if (run != runAnterior)
    {
        runAnterior = run;
        JWPLC_Display.setRunLed(run);
    }

    static const char *errAnterior = "";
    const char *err = textoAlarmaPrincipal();
    if (strcmp(err, errAnterior) != 0)
    {
        errAnterior = err;
        JWPLC_Display.setErrCode(err);
        if (err[0] != '\0')
            JWPLC_Buzzer.beep(); // aviso corto al aparecer una alarma
    }

    if (JWPLC_Buttons.pressed(BTN_ESC))
        reconocerAlarmas = true;
#endif
}
