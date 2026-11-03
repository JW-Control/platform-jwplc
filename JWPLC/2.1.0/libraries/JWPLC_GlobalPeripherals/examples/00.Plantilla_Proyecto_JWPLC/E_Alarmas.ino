/*
  E_Alarmas · Alarmas

  QUÉ VA AQUÍ:    qué condición dispara cada alarma, su enclavamiento y el reconocimiento.
                  Una alarma = un bit de CodigoAlarma (B_Tags.h) y un texto corto.
  QUÉ NO VA AQUÍ: la reacción de la máquina (D_Logica) ni cómo se muestra (G_HMI).

  alarmasActivas    -> la causa está presente ahora.
  alarmasEnclavadas -> ocurrió y sigue pendiente de reconocer aunque la causa se fue.
*/

// Texto corto de cada alarma (1 a 4 caracteres A-Z/0-9: se muestra como código ERR en la pantalla IDLE).
const char TEXTO_ALARMA[ALM_CANTIDAD][5] = {
    "EMG", // ALM_EMERGENCIA
    "TER", // ALM_TERMICO
};

static void alarma(CodigoAlarma codigo, bool condicion)
{
    if (condicion)
    {
        bitSet(alarmasActivas, codigo);
        bitSet(alarmasEnclavadas, codigo);
    }
    else
    {
        bitClear(alarmasActivas, codigo);
    }
}

void evaluarAlarmas()
{
    alarma(ALM_EMERGENCIA, emergencia);
    alarma(ALM_TERMICO, fallaTermico);

    // Reconocer sólo borra las que ya no están presentes.
    if (reconocerAlarmas)
    {
        alarmasEnclavadas &= alarmasActivas;
        reconocerAlarmas = false;
    }
}

// Número de la primera alarma pendiente, o ALM_CANTIDAD si no hay ninguna
// (lo usa G_HMI; su texto es TEXTO_ALARMA[numero]).
uint8_t alarmaPrincipal()
{
    for (uint8_t i = 0; i < ALM_CANTIDAD; ++i)
    {
        if (bitRead(alarmasEnclavadas, i))
            return i;
    }
    return ALM_CANTIDAD;
}
