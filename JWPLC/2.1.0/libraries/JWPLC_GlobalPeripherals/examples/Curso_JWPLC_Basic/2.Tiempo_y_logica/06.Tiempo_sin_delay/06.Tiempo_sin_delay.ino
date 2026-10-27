/*
  Curso JWPLC Basic · Lección 06 · Esperar sin detener el programa

  Qué vas a aprender
  - Por qué delay() es un problema en una máquina: mientras espera, el
    programa no lee entradas ni atiende nada más.
  - A usar millis(), el reloj interno que cuenta los milisegundos desde que
    el equipo arrancó, para hacer varias cosas "a la vez".

  La idea
  - En lugar de "esperar 500 ms", el programa pregunta en cada vuelta:
    "¿ya pasaron 500 ms desde la última vez que cambié la luz?"
  - Si pasaron, actúa y anota la hora. Si no, sigue con lo demás.
  - loop() da miles de vueltas por segundo, así que nada queda esperando.

  Qué conectar
  - Borne 1M a 0 V y un pulsador entre +24 V e I0.0.

  Qué hace
  - Q0.0 parpadea cada medio segundo, sin parar.
  - Q0.1 sigue al pulsador de I0.0 al instante, aunque Q0.0 esté parpadeando.

  Cómo probarlo
  1. Sube el programa y mira el parpadeo de Q0.0.
  2. Aprieta y suelta el pulsador: Q0.1 responde en el momento.
  3. Compáralo con la lección 05, donde la salida tardaba en responder.

  Para practicar
  - Haz que Q0.2 parpadee a otro ritmo (cada 2 segundos) con su propia
    variable de tiempo: verás dos intermitencias independientes.
*/

// Cada cuánto cambia la intermitencia, en milisegundos. "const" indica que
// es un valor fijo: el programa no lo puede modificar por error.
const unsigned long INTERVALO_MS = 500;

// Momento (en milisegundos) del último cambio de la intermitencia.
unsigned long ultimoCambio = 0;

// Estado actual de la luz intermitente.
bool luzEncendida = false;

void setup()
{
}

void loop()
{
    // millis() devuelve los milisegundos que pasaron desde el arranque.
    unsigned long ahora = millis();

    // Tarea 1: la intermitencia. "ahora - ultimoCambio" es el tiempo que pasó
    // desde el último cambio. Esta resta sigue funcionando bien incluso
    // cuando millis() vuelve a empezar desde 0, después de unos 49 días.
    if (ahora - ultimoCambio >= INTERVALO_MS)
    {
        ultimoCambio = ahora;
        luzEncendida = !luzEncendida; // Si estaba encendida se apaga, y al revés.
        digitalWrite(Q0_0, luzEncendida);
    }

    // Tarea 2: el pulsador. Como nadie se queda esperando, esta línea se
    // ejecuta miles de veces por segundo y responde al instante.
    digitalWrite(Q0_1, digitalRead(I0_0));
}
