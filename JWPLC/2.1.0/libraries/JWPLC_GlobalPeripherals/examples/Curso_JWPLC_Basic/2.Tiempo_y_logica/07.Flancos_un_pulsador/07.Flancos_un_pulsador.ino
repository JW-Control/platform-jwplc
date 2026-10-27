/*
  Curso JWPLC Basic · Lección 07 · Flancos: encender y apagar con un pulsador

  Qué vas a aprender
  - La diferencia entre "el pulsador ESTÁ apretado" (un estado, dura) y
    "el pulsador SE APRETÓ" (un flanco, ocurre una sola vez).
  - A detectar flancos comparando la lectura de ahora con la de la vuelta
    anterior de loop().
  - A hacer un telerruptor: una pulsación enciende, la siguiente apaga.

  Los dos flancos
  - Flanco de subida: antes estaba inactivo y ahora está activo (se apretó).
  - Flanco de bajada: antes estaba activo y ahora está inactivo (se soltó).

  Qué conectar
  - Borne 1M a 0 V y un pulsador entre +24 V e I0.0.

  Cómo probarlo
  1. Sube el programa y abre el Monitor Serie a 115200 baudios.
  2. Cada vez que aprietes el pulsador, Q0.0 cambia: se enciende o se apaga.
  3. El Monitor Serie avisa solo cuando algo cambia, sin repetir mensajes.

  Para practicar
  - Muestra cuántas veces se apretó el pulsador desde el arranque.
  - Haz que Q0.1 cambie al SOLTAR el pulsador, en lugar de al apretarlo.
*/

// Un contacto mecánico "rebota": al cerrarse se abre y se cierra varias veces
// en pocos milisegundos. Si el programa lo leyera en cada vuelta, contaría una
// pulsación como varias. Por eso se lee solo cada 20 ms: el rebote ya pasó.
const unsigned long ANTIRREBOTE_MS = 20;
unsigned long ultimaLectura = 0;

// Lectura de la vuelta anterior. Empieza en false: pulsador suelto.
bool antes = false;

// Estado del telerruptor.
bool luz = false;

void setup()
{
    Serial.begin(115200);
    Serial.println("Lección 07: cada pulsación en I0.0 cambia Q0.0");
}

void loop()
{
    // Lo que aprendiste en la lección 06: si todavía no pasaron 20 ms, return
    // termina esta vuelta de loop() y el programa sigue con la siguiente.
    if (millis() - ultimaLectura < ANTIRREBOTE_MS)
    {
        return;
    }
    ultimaLectura = millis();

    bool ahora = digitalRead(I0_0) == HIGH;

    // Flanco de subida: ahora está apretado y en la vuelta anterior no.
    if (ahora && !antes)
    {
        luz = !luz;
        digitalWrite(Q0_0, luz);

        Serial.print("Pulsación: Q0.0 ");
        Serial.println(luz ? "encendida" : "apagada");
    }

    // Flanco de bajada: ahora está suelto y en la vuelta anterior no.
    if (!ahora && antes)
    {
        Serial.println("Pulsador suelto");
    }

    // Muy importante: guardar la lectura para compararla en la próxima vuelta.
    antes = ahora;
}
