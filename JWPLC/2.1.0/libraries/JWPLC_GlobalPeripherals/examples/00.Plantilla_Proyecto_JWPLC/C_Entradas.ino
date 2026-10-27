/*
  C_Entradas · Leer entradas

  QUÉ VA AQUÍ:    la lectura física de I0_0..I0_7 (una sola vez por ciclo), el filtro
                  antirrebote, la inversión de contactos NC y el escalado de señales.
  QUÉ NO VA AQUÍ: decisiones de la máquina (D_Logica) ni escrituras de salidas.

  Todo el resto del programa usa las variables de B_Tags.h, no las entradas físicas.
*/

static bool bitEntrada(uint8_t bit)
{
    return filtroEntrada[bit].ejecutar(bitRead(imagenEntradas, bit), T_ANTIRREBOTE_MS);
}

void leerEntradas()
{
    // Un solo acceso al expansor por ciclo: bit 0 = I0_0 ... bit 7 = I0_7.
    imagenEntradas = JWPLC_readInputs();

    pbMarcha = bitEntrada(BIT_PB_MARCHA);
    pbParo = !bitEntrada(BIT_PB_PARO);          // NC: abierto = pulsado
    emergencia = !bitEntrada(BIT_EMERGENCIA);   // NC: abierto = emergencia (y si se corta el cable)
    fallaTermico = !bitEntrada(BIT_TERMICO);    // NC: abierto = térmico disparado
}
