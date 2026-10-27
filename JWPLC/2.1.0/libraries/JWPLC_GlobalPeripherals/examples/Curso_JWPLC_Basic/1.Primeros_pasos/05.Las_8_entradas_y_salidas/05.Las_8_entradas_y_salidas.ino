/*
  Curso JWPLC Basic · Lección 05 · Las 8 entradas y las 8 salidas

  Qué vas a aprender
  - Qué es un arreglo: una lista de datos con un solo nombre.
  - A recorrer una lista con el bucle for, en lugar de escribir 8 líneas iguales.
  - A mostrar el estado de todas las entradas en una sola línea.

  Las listas que ya trae el JWPLC
  - I0_X[0] es I0_0, I0_X[1] es I0_1 ... I0_X[7] es I0_7.
  - Q0_X[0] es Q0_0 ... Q0_X[7] es Q0_7.
  - I0_COUNT y Q0_COUNT valen 8: cuántas entradas y salidas hay.
  - Las posiciones de una lista se cuentan desde 0, no desde 1.

  Qué conectar
  - Borne 1M a 0 V y pulsadores o sensores en las entradas que quieras usar.

  Cómo probarlo
  1. Sube el programa y abre el Monitor Serie a 115200 baudios.
  2. Cada entrada activa cierra el relé de su mismo número.
  3. Cada medio segundo verás una línea como "Entradas: 1 0 0 1 0 0 0 0".

  Para practicar
  - Cruza las salidas: I0.0 manda Q0.7, I0.1 manda Q0.6 ... (pista: 7 - i).
  - Cuenta cuántas entradas están activas y muestra el total.
*/

void setup()
{
    Serial.begin(115200);
    Serial.println("Lección 05: entradas y salidas en una lista");
}

void loop()
{
    // El bucle for repite el bloque con i = 0, 1, 2 ... 7.
    //   uint8_t i = 0   -> empieza en 0 (uint8_t es un número de 0 a 255)
    //   i < I0_COUNT    -> sigue mientras i sea menor que 8
    //   i++             -> al terminar cada vuelta, suma 1
    for (uint8_t i = 0; i < I0_COUNT; i++)
    {
        int estado = digitalRead(I0_X[i]);
        digitalWrite(Q0_X[i], estado);
    }

    // El mismo bucle para escribir el estado de cada entrada en el Monitor.
    Serial.print("Entradas: ");
    for (uint8_t i = 0; i < I0_COUNT; i++)
    {
        Serial.print(digitalRead(I0_X[i]));
        Serial.print(' ');
    }
    Serial.println();

    // Fíjate en algo: con esta pausa, un relé tarda hasta medio segundo en
    // seguir a su entrada, porque mientras delay() espera el programa no
    // lee nada. La lección 06 muestra cómo esperar sin detener el programa.
    delay(500);
}
