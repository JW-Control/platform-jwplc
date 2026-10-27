/*
  K_Utilidades.h · Temporizadores y flancos tipo PLC (no se edita)

  Herramientas sin delay() para usar en cualquier pestaña:

    TON  retardo a la conexión:     t.ejecutar(condicion, 3000) -> true tras 3 s de condicion
    TOF  retardo a la desconexión:  t.ejecutar(condicion, 3000) -> sigue true 3 s después
    Flanco subida / bajada:         f.subida(senal) -> true sólo en el ciclo en que senal pasa a 1
    Antirrebote:                    a.ejecutar(senal, 30) -> senal estable 30 ms

  Declara cada instancia una sola vez en B_Tags.h (una por uso, nunca compartida).
*/
#pragma once

struct TON
{
    uint32_t inicioMs = 0;
    bool contando = false;
    bool salida = false;

    bool ejecutar(bool entrada, uint32_t presetMs)
    {
        if (!entrada)
        {
            contando = false;
            salida = false;
            return false;
        }
        if (!contando)
        {
            contando = true;
            inicioMs = millis();
        }
        salida = (uint32_t)(millis() - inicioMs) >= presetMs;
        return salida;
    }

    uint32_t transcurridoMs() const
    {
        return contando ? (uint32_t)(millis() - inicioMs) : 0;
    }
};

struct TOF
{
    uint32_t caidaMs = 0;
    bool salida = false;

    bool ejecutar(bool entrada, uint32_t presetMs)
    {
        if (entrada)
        {
            salida = true;
            caidaMs = millis();
        }
        else if (salida && (uint32_t)(millis() - caidaMs) >= presetMs)
        {
            salida = false;
        }
        return salida;
    }
};

struct Flanco
{
    bool anterior = false;

    bool subida(bool senal)
    {
        const bool r = senal && !anterior;
        anterior = senal;
        return r;
    }

    bool bajada(bool senal)
    {
        const bool r = !senal && anterior;
        anterior = senal;
        return r;
    }
};

struct Antirrebote
{
    bool estable = false;
    bool candidato = false;
    bool iniciado = false;
    uint32_t cambioMs = 0;

    bool ejecutar(bool senal, uint32_t filtroMs)
    {
        // La primera lectura se toma tal cual: así un contacto NC cerrado no se ve
        // abierto durante el filtro al encender (y no dispara alarmas falsas).
        if (!iniciado)
        {
            iniciado = true;
            estable = candidato = senal;
        }
        else if (senal != candidato)
        {
            candidato = senal;
            cambioMs = millis();
        }
        else if (estable != candidato && (uint32_t)(millis() - cambioMs) >= filtroMs)
        {
            estable = candidato;
        }
        return estable;
    }
};
