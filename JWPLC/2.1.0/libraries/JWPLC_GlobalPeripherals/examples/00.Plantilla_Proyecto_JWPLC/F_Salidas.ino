/*
  F_Salidas · Escribir salidas

  QUÉ VA AQUÍ:    la escritura física de Q0_0..Q0_7 (una sola vez por ciclo, al final)
                  y las seguridades de última palabra (p. ej. emergencia apaga todo).
  QUÉ NO VA AQUÍ: decisiones de proceso (D_Logica).
*/

void salidasApagarTodo()
{
    imagenSalidas = 0x00;
    JWPLC_writeOutputs(imagenSalidas);
}

void escribirSalidas()
{
    uint8_t nueva = 0x00;
    bitWrite(nueva, BIT_MOTOR, motor);
    bitWrite(nueva, BIT_LAMPARA_FALLA, lamparaFalla);

    // Seguridad de última palabra: con emergencia no sale ninguna orden de marcha.
    if (emergencia)
        bitClear(nueva, BIT_MOTOR);

    // Sólo se escribe al expansor si algo cambió.
    if (nueva != imagenSalidas)
    {
        imagenSalidas = nueva;
        JWPLC_writeOutputs(imagenSalidas);
    }
}
