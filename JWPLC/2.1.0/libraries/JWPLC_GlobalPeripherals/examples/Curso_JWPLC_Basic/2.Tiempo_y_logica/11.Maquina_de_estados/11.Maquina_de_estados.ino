/*
  Curso JWPLC Basic · Lección 11 · Máquina de estados: un semáforo

  Qué vas a aprender
  - A pensar una máquina como una serie de ESTADOS (pasos) y las
    condiciones que la hacen pasar de un estado al siguiente.
  - enum: una lista de nombres para los estados.
  - switch: hacer una cosa distinta según el estado actual.

  El caso: semáforo de un cruce peatonal
  - VERDE para los autos, como mínimo 10 segundos. Si un peatón aprieta el
    botón, al cumplirse esos 10 segundos pasa a AMARILLO. Sin peatones,
    sigue en verde hasta 30 segundos.
  - AMARILLO durante 3 segundos.
  - ROJO para los autos durante 8 segundos, con el verde peatonal encendido.
  - Y vuelve a VERDE.

  Qué conectar
  - Borne 1M a 0 V.
  - I0.0: botón del peatón.
  - Q0.0 rojo, Q0.1 amarillo, Q0.2 verde de los autos.
  - Q0.3 verde de los peatones.

  Cómo probarlo
  1. Sube el programa y abre el Monitor Serie a 115200 baudios.
  2. Sin tocar nada, el semáforo cambia cada 30 segundos.
  3. Aprieta el botón: después de los 10 segundos mínimos de verde, cambia.

  Para practicar
  - Agrega un estado "ROJO PARPADEANDO" para los peatones en los últimos
    3 segundos de su verde.
  - Que una entrada I0.1 ponga el semáforo en amarillo intermitente (modo
    noche) mientras esté activa.
*/

// Los estados del semáforo. Internamente cada nombre es un número, pero
// en el programa se lee mucho mejor VERDE que 0.
enum Estado
{
    VERDE,
    AMARILLO,
    ROJO
};

const uint32_t VERDE_MINIMO_MS = 10000;
const uint32_t VERDE_MAXIMO_MS = 30000;
const uint32_t AMARILLO_MS = 3000;
const uint32_t ROJO_MS = 8000;

Estado estado = VERDE;
uint32_t inicioEstado = 0;     // Momento en que se entró al estado actual.
bool peatonEsperando = false;  // Recuerda el pedido del botón.

// Cambia de estado y anota el momento, para medir cuánto lleva en él.
void cambiarA(Estado nuevo, const char *nombre)
{
    estado = nuevo;
    inicioEstado = millis();
    Serial.print("Estado: ");
    Serial.println(nombre);
}

void setup()
{
    Serial.begin(115200);
    cambiarA(VERDE, "VERDE");
}

void loop()
{
    uint32_t enEstado = millis() - inicioEstado;

    // El pedido del peatón se guarda aunque el botón se suelte enseguida.
    if (digitalRead(I0_0) == HIGH)
    {
        peatonEsperando = true;
    }

    // switch elige el bloque "case" del estado actual. Cada bloque termina
    // con break. Así solo se revisan las condiciones del paso en el que está.
    switch (estado)
    {
    case VERDE:
        if ((peatonEsperando && enEstado >= VERDE_MINIMO_MS) || enEstado >= VERDE_MAXIMO_MS)
        {
            cambiarA(AMARILLO, "AMARILLO");
        }
        break;

    case AMARILLO:
        if (enEstado >= AMARILLO_MS)
        {
            peatonEsperando = false; // El pedido ya se atiende.
            cambiarA(ROJO, "ROJO");
        }
        break;

    case ROJO:
        if (enEstado >= ROJO_MS)
        {
            cambiarA(VERDE, "VERDE");
        }
        break;
    }

    // Las salidas dependen solo del estado: cada luz tiene una sola regla.
    digitalWrite(Q0_0, estado == ROJO);
    digitalWrite(Q0_1, estado == AMARILLO);
    digitalWrite(Q0_2, estado == VERDE);
    digitalWrite(Q0_3, estado == ROJO);
}
