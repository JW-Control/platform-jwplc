/*
  Curso JWPLC Basic · Lección 08 · Marcha y parada de un motor

  Qué vas a aprender
  - El circuito más usado en la industria: arrancar un motor con un pulsador
    y que siga en marcha al soltarlo (enclavamiento o "sello").
  - Por qué la parada y el relé térmico se cablean normalmente cerrados (NC).

  Qué conectar
  - Borne 1M a 0 V.
  - I0.0: pulsador de MARCHA, normalmente abierto (NO), desde +24 V.
  - I0.1: pulsador de PARADA, normalmente cerrado (NC), desde +24 V.
  - I0.2: contacto auxiliar NC del relé térmico (bornes 95-96), desde +24 V.
  - Q0.0: bobina del contactor del motor. El relé del JWPLC maneja la
    bobina, nunca el motor directo: la corriente del motor pasa por el
    contactor.
  - Q0.1: lámpara de falla.

  Por qué normalmente cerrado
  - Con la parada NC la entrada I0.1 recibe 24 V mientras NADIE la aprieta.
  - Si alguien aprieta la parada, o si un cable se corta, la entrada se
    apaga y el motor se detiene. Una falla del cableado deja la máquina
    detenida, nunca en marcha. Por eso el programa pide "parada activa"
    para dejar funcionar el motor.

  La regla, como en un diagrama de escalera
      motor = (marcha O motor) Y parada Y térmico
  "O motor" es el sello: si el motor ya está en marcha, se mantiene aunque
  se suelte el pulsador de marcha.

  Cómo probarlo
  1. Sin parada ni térmico conectados, el motor no arranca: es lo correcto.
  2. Con todo conectado, un toque a MARCHA cierra Q0.0 y se queda cerrado.
  3. Un toque a PARADA lo abre. Si el térmico se dispara, se abre y Q0.1
     enciende la lámpara de falla.

  Para practicar
  - Agrega una lámpara de marcha en Q0.2.
  - Haz que con el térmico disparado no se pueda arrancar hasta que se
    apriete PARADA (un "reconocimiento" de la falla).
*/

bool motor = false;

void setup()
{
    Serial.begin(115200);
    Serial.println("Lección 08: marcha I0.0, parada NC I0.1, térmico NC I0.2");
}

void loop()
{
    bool marcha = digitalRead(I0_0) == HIGH;
    bool paradaSinApretar = digitalRead(I0_1) == HIGH;  // NC: activa en reposo.
    bool termicoNormal = digitalRead(I0_2) == HIGH;     // NC: activo si no hay falla.

    bool motorAntes = motor;

    // La regla de la escalera, en una línea.
    motor = (marcha || motor) && paradaSinApretar && termicoNormal;

    digitalWrite(Q0_0, motor);
    digitalWrite(Q0_1, !termicoNormal);

    // Avisa solo cuando el motor cambia (flanco, como en la lección 07).
    if (motor != motorAntes)
    {
        Serial.println(motor ? "Motor en marcha" : "Motor detenido");
    }
}
