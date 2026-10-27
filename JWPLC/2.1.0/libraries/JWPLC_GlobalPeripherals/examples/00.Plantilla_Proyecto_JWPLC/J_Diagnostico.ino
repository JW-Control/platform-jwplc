/*
  J_Diagnostico · Monitor serie y estado del ciclo

  QUÉ VA AQUÍ:    mensajes de arranque, errores y un resumen periódico para la puesta en
                  marcha (monitor serie a SERIAL_BAUDIOS). Se apaga con USAR_SERIAL_DIAGNOSTICO.
  QUÉ NO VA AQUÍ: nada de lo que dependa la máquina: el equipo debe funcionar igual con
                  o sin cable USB conectado.
*/

static uint32_t ciclosPorPeriodo = 0;
static uint32_t cicloMaximoUs = 0;

void diagnosticoInicio()
{
#if USAR_SERIAL_DIAGNOSTICO
    Serial.begin(SERIAL_BAUDIOS);
    Serial.println();
    Serial.print(PROYECTO_NOMBRE);
    Serial.print(" v");
    Serial.print(PROYECTO_VERSION);
    Serial.print(" | cliente: ");
    Serial.println(PROYECTO_CLIENTE);
#endif
}

void diagnosticoMensaje(const char *origen, const char *detalle)
{
#if USAR_SERIAL_DIAGNOSTICO
    Serial.print("[");
    Serial.print(origen);
    Serial.print("] ");
    Serial.println(detalle);
#else
    (void)origen;
    (void)detalle;
#endif
}

void diagnosticoCiclo()
{
    // Tiempo de ciclo: útil para ver si alguna pestaña está bloqueando.
    static uint32_t inicioCicloUs = micros();
    const uint32_t ahoraUs = micros();
    const uint32_t cicloUs = ahoraUs - inicioCicloUs;
    inicioCicloUs = ahoraUs;
    if (cicloUs > cicloMaximoUs)
        cicloMaximoUs = cicloUs;
    ciclosPorPeriodo++;

#if USAR_SERIAL_DIAGNOSTICO
    static uint32_t ultimoMs = 0;
    if ((uint32_t)(millis() - ultimoMs) < T_DIAGNOSTICO_MS)
        return;
    ultimoMs = millis();

    Serial.printf("estado=%u IN=0x%02X OUT=0x%02X alm=0x%04X/0x%04X ciclos=%lu max=%luus horas=%lu\n",
                  (unsigned)estado, imagenEntradas, imagenSalidas,
                  alarmasActivas, alarmasEnclavadas,
                  (unsigned long)ciclosPorPeriodo, (unsigned long)cicloMaximoUs,
                  (unsigned long)persistentes.horasMarcha);
    ciclosPorPeriodo = 0;
    cicloMaximoUs = 0;
#endif
}
