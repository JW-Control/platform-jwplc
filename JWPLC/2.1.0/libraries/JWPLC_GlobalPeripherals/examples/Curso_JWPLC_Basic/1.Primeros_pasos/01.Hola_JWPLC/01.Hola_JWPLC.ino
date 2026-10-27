/*
  Curso JWPLC Basic · Lección 01 · Hola, JWPLC

  Qué vas a aprender
  - Las dos partes de todo programa de Arduino: setup() y loop().
  - A enviar mensajes a la PC por el cable USB-C con el Monitor Serie.
  - Qué es una variable: una caja con nombre donde el programa guarda un dato.

  Qué necesitas
  - El JWPLC Basic alimentado con 24 V DC.
  - Un cable USB-C con datos entre el JWPLC y la PC.

  Cómo probarlo
  1. Elige la placa JWPLC Basic y su puerto en el menú Herramientas.
  2. Pulsa Subir (el botón con la flecha).
  3. Abre Herramientas → Monitor Serie y elige 115200 baudios.
  4. Verás el saludo una sola vez y, debajo, un número que sube cada segundo.

  Para practicar
  - Cambia el texto del saludo y vuelve a subir el programa.
  - Haz que el número suba cada medio segundo (500 milisegundos).
*/

// Una variable guarda un dato que puede cambiar. "unsigned long" es un número
// entero grande y sin signo (nunca negativo): sirve para contar mucho tiempo.
unsigned long segundos = 0;

// setup() se ejecuta UNA sola vez, justo después de encender o reiniciar el
// equipo. Aquí se prepara todo lo que el programa necesita.
void setup()
{
    // Abre la comunicación con la PC a 115200 baudios (bits por segundo).
    // El Monitor Serie debe estar a la misma velocidad para leer bien.
    Serial.begin(115200);

    // println() escribe el texto y salta a la línea siguiente.
    Serial.println("Hola, JWPLC");
    Serial.println("Este mensaje sale una sola vez, desde setup().");
}

// loop() se repite sin parar mientras el equipo esté encendido: cuando llega
// a la última línea, vuelve a empezar desde la primera.
void loop()
{
    // print() escribe sin saltar de línea; así el número queda al lado.
    Serial.print("Segundos encendido: ");
    Serial.println(segundos);

    // delay() detiene el programa la cantidad de milisegundos indicada.
    // 1000 milisegundos = 1 segundo.
    delay(1000);

    // Suma 1 a la variable. Es lo mismo que escribir: segundos = segundos + 1;
    segundos++;
}
