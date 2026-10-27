/*
  Curso JWPLC Basic · Lección 03 · Leer una entrada

  Qué vas a aprender
  - A leer una entrada con digitalRead().
  - Que una entrada solo tiene dos estados: HIGH (activa) o LOW (inactiva).
  - A usar lo que se lee para mandar una salida.

  Cómo es una entrada del JWPLC Basic
  - Las entradas I0.0 a I0.7 trabajan con 24 V DC y son sinking, para
    sensores PNP: la entrada se activa cuando le llegan +24 V.
  - El borne 1M es el común de las entradas y se une a 0 V de la fuente.
  - En el programa la entrada I0.0 se escribe I0_0.

  Qué conectar
  - Borne 1M a 0 V de la fuente de 24 V.
  - Un pulsador entre +24 V y la entrada I0.0. Sirve también un sensor PNP:
    marrón a +24 V, azul a 0 V y negro (la señal) a I0.0.

  Cómo probarlo
  1. Sube el programa y abre el Monitor Serie a 115200 baudios.
  2. Mientras mantengas el pulsador apretado, el relé Q0.0 estará cerrado y
     el Monitor Serie dirá "I0.0 activa".

  Para practicar
  - Haz lo contrario: que Q0.0 esté cerrado mientras el pulsador NO esté
    apretado (pista: el operador ! invierte un valor, lo verás en la lección 04).
  - Conecta un segundo pulsador a I0.1 y haz que mande el relé Q0.1.
*/

void setup()
{
    Serial.begin(115200);
    Serial.println("Lección 03: aprieta el pulsador de I0.0");
}

void loop()
{
    // digitalRead() devuelve HIGH si la entrada recibe 24 V, o LOW si no.
    // El resultado se guarda en una variable para usarlo dos veces.
    int estado = digitalRead(I0_0);

    // La salida copia a la entrada: entrada activa -> relé cerrado.
    digitalWrite(Q0_0, estado);

    // == compara dos valores. Ojo: = guarda un valor, == pregunta si son iguales.
    if (estado == HIGH)
    {
        Serial.println("I0.0 activa");
    }
    else
    {
        Serial.println("I0.0 inactiva");
    }

    // Una pausa corta para que el Monitor Serie no se llene de mensajes.
    // En la lección 07 verás cómo avisar solo cuando la entrada cambia.
    delay(200);
}
