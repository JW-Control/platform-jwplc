/*
  Curso JWPLC Basic · Lección 09 · Temporizadores

  Qué vas a aprender
  - Los dos temporizadores más usados en un PLC:
      Retardo a la conexión (TON): la salida se activa cuando la entrada
        lleva cierto tiempo activa. Si la entrada se suelta antes, no pasa nada.
      Retardo a la desconexión (TOF): la salida se activa con la entrada y
        se queda activa cierto tiempo después de que la entrada se apaga.
  - A agrupar datos y funciones con struct: cada temporizador guarda su
    estado y sabe "ejecutarse" con temporizador.ejecutar(...).
  - Son los mismos TON y TOF de la Plantilla de proyecto (K_Utilidades.h).

  Qué conectar
  - Borne 1M a 0 V.
  - I0.0: pulsador. Hay que mantenerlo apretado 3 segundos para arrancar
    (evita arranques por un toque accidental).
  - I0.1: sensor de presencia de un pasillo.

  Qué hace
  - Q0.0 se cierra cuando I0.0 lleva 3 segundos apretado (TON).
  - Q0.1, la luz del pasillo, se enciende con la presencia y se apaga
    10 segundos después de la última detección (TOF).

  Cómo probarlo
  1. Toca I0.0 un segundo: no pasa nada. Mantenlo 3 segundos: cierra Q0.0.
  2. Activa I0.1 y suéltalo: la luz sigue 10 segundos y se apaga. Si lo
     activas otra vez antes, los 10 segundos empiezan de nuevo.

  Para practicar
  - Cambia los tiempos.
  - Agrega un tercer temporizador: una bocina en Q0.2 que suene si la
    presencia de I0.1 dura más de 30 segundos.
*/

// struct agrupa datos y funciones bajo un mismo nombre. Cada temporizador
// guarda el momento en que empezó a contar y si su salida está activa, y su
// función ejecutar() lo actualiza en cada vuelta de loop().

// Retardo a la conexión (TON).
struct TON
{
    uint32_t inicioMs = 0;
    bool salida = false;

    bool ejecutar(bool entrada, uint32_t tiempoMs)
    {
        if (!entrada)
        {
            // Entrada apagada: la cuenta vuelve a empezar desde ahora.
            inicioMs = millis();
            salida = false;
        }
        else if (millis() - inicioMs >= tiempoMs)
        {
            salida = true;
        }
        return salida;
    }
};

// Retardo a la desconexión (TOF).
struct TOF
{
    uint32_t inicioMs = 0;
    bool salida = false;

    bool ejecutar(bool entrada, uint32_t tiempoMs)
    {
        if (entrada)
        {
            // Mientras la entrada esté activa, la salida también, y la cuenta
            // se reinicia en cada vuelta.
            inicioMs = millis();
            salida = true;
        }
        else if (millis() - inicioMs >= tiempoMs)
        {
            salida = false;
        }
        return salida;
    }
};

// Un temporizador para cada uso: cada uno recuerda su propio estado.
TON arranque;
TOF luzPasillo;

void setup()
{
}

void loop()
{
    bool pulsador = digitalRead(I0_0) == HIGH;
    bool presencia = digitalRead(I0_1) == HIGH;

    // Se llama con nombre.ejecutar(entrada, tiempo). Devuelve la salida del
    // temporizador, que se puede escribir directo en la salida del PLC.
    digitalWrite(Q0_0, arranque.ejecutar(pulsador, 3000));
    digitalWrite(Q0_1, luzPasillo.ejecutar(presencia, 10000));
}
