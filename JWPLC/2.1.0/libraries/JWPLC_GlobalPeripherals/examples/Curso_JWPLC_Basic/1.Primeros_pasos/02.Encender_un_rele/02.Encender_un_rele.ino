/*
  Curso JWPLC Basic · Lección 02 · Encender un relé

  Qué vas a aprender
  - A encender y apagar una salida con digitalWrite().
  - Qué significan HIGH (encendido) y LOW (apagado).
  - A hacer una intermitencia: encender, esperar, apagar, esperar.

  Cómo es una salida del JWPLC Basic
  - Cada salida Q0.0 a Q0.7 es un relé: un interruptor que el programa abre y
    cierra. Tiene dos bornes, COM y NO (normalmente abierto).
  - Con la salida en HIGH el relé cierra y une COM con NO; con LOW los separa.
  - Es un contacto seco: el relé no entrega voltaje, solo deja pasar el de tu
    circuito. Admite hasta 5 A con 250 V AC o 30 V DC.
  - En el programa la salida Q0.0 se escribe Q0_0 (con guion bajo).

  Qué necesitas
  - Nada más que el equipo: el indicador de Q0.0 en el frente y el "clic" del
    relé muestran que funciona.
  - Opcional: una lámpara de 220 V AC con la fase pasando por COM y NO de Q0.0.
    Hazlo con el tablero desenergizado.

  Cómo probarlo
  1. Sube el programa.
  2. El relé Q0.0 se enciende 1 segundo y se apaga 1 segundo, sin parar.

  Para practicar
  - Haz que esté encendido 2 segundos y apagado medio segundo.
  - Agrega Q0_1 para que se encienda justo cuando Q0_0 se apaga.
*/

void setup()
{
    // En un Arduino común habría que llamar a pinMode() para decir que el pin
    // es una salida. En el JWPLC no hace falta: las entradas y los relés ya
    // quedan listos al arrancar.
}

void loop()
{
    digitalWrite(Q0_0, HIGH); // Cierra el relé Q0.0: la lámpara enciende.
    delay(1000);              // Espera 1 segundo.

    digitalWrite(Q0_0, LOW);  // Abre el relé Q0.0: la lámpara se apaga.
    delay(1000);              // Espera 1 segundo y vuelve a empezar.
}
