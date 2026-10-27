/*
  Curso JWPLC Basic · Lección 09 · Temporizadores

  Qué vas a aprender
  - Los dos temporizadores más usados en un PLC:
      Retardo a la conexión (TON): la salida se activa cuando la entrada
        lleva cierto tiempo activa. Si la entrada se suelta antes, no pasa nada.
      Retardo a la desconexión (TOF): la salida se activa con la entrada y
        se queda activa cierto tiempo después de que la entrada se apaga.
  - A escribir tus propias funciones: un bloque con nombre que se usa las
    veces que haga falta.
  - A agrupar datos con struct.

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

// struct agrupa varios datos bajo un mismo nombre. Cada temporizador guarda
// el momento en que empezó a contar y si su salida está activa.
struct Temporizador
{
    uint32_t inicio;
    bool salida;
};

// Un temporizador para cada uso. {0, false}: empieza en 0 y con la salida apagada.
Temporizador arranque = {0, false};
Temporizador luzPasillo = {0, false};

// Retardo a la conexión (TON).
// El & delante de "t" hace que la función trabaje sobre el temporizador
// original y no sobre una copia: así recuerda su estado entre vueltas.
bool retardoConexion(Temporizador &t, bool entrada, uint32_t tiempoMs)
{
    if (!entrada)
    {
        // Entrada apagada: la cuenta vuelve a empezar desde ahora.
        t.inicio = millis();
        t.salida = false;
    }
    else if (millis() - t.inicio >= tiempoMs)
    {
        t.salida = true;
    }
    return t.salida;
}

// Retardo a la desconexión (TOF).
bool retardoDesconexion(Temporizador &t, bool entrada, uint32_t tiempoMs)
{
    if (entrada)
    {
        // Mientras la entrada esté activa, la salida también, y la cuenta
        // se reinicia en cada vuelta.
        t.inicio = millis();
        t.salida = true;
    }
    else if (millis() - t.inicio >= tiempoMs)
    {
        t.salida = false;
    }
    return t.salida;
}

void setup()
{
}

void loop()
{
    bool pulsador = digitalRead(I0_0) == HIGH;
    bool presencia = digitalRead(I0_1) == HIGH;

    // Una función se "llama" escribiendo su nombre con los datos que necesita.
    // Devuelve un resultado que se puede escribir directo en la salida.
    digitalWrite(Q0_0, retardoConexion(arranque, pulsador, 3000));
    digitalWrite(Q0_1, retardoDesconexion(luzPasillo, presencia, 10000));
}
