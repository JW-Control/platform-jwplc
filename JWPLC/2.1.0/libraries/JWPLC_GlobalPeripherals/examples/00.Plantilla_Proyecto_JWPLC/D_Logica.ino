/*
  D_Logica · Lógica de control

  QUÉ VA AQUÍ:    la máquina de estados, secuencias, temporizadores y enclavamientos
                  de proceso. Lee variables de B_Tags.h y decide qué salidas pedir.
  QUÉ NO VA AQUÍ: leer bornes (C_Entradas), escribir bornes (F_Salidas), dibujar en la
                  pantalla (G_HMI) ni delay().

  El ejemplo es un arranque directo de motor con marcha/paro. Reemplázalo por el
  proceso real manteniendo la forma: un switch por estado y transiciones claras.
*/

static constexpr uint32_t T_ARRANQUE_MS = 2000;

void ejecutarLogica()
{
    const bool hayFalla = alarmasActivas != 0;
    const bool ordenMarcha = flancoMarcha.subida(pbMarcha);

    switch (estado)
    {
    case ESTADO_PARADO:
        if (ordenMarcha && !hayFalla && alarmasEnclavadas == 0)
            estado = ESTADO_ARRANCANDO;
        break;

    case ESTADO_ARRANCANDO:
        if (hayFalla)
            estado = ESTADO_FALLA;
        else if (pbParo)
            estado = ESTADO_PARADO;
        else if (tArranque.ejecutar(true, T_ARRANQUE_MS))
            estado = ESTADO_MARCHA;
        break;

    case ESTADO_MARCHA:
        if (hayFalla)
            estado = ESTADO_FALLA;
        else if (pbParo)
            estado = ESTADO_PARADO;
        break;

    case ESTADO_FALLA:
        // Sale de falla sólo cuando la causa desapareció y se reconoció la alarma.
        if (!hayFalla && alarmasEnclavadas == 0)
            estado = ESTADO_PARADO;
        break;
    }

    if (estado != ESTADO_ARRANCANDO)
        tArranque.ejecutar(false, T_ARRANQUE_MS);

    // Salidas pedidas por la lógica (F_Salidas las escribe y aplica seguridades).
    motor = (estado == ESTADO_ARRANCANDO || estado == ESTADO_MARCHA);
    lamparaFalla = (estado == ESTADO_FALLA) || alarmasEnclavadas != 0;
}
