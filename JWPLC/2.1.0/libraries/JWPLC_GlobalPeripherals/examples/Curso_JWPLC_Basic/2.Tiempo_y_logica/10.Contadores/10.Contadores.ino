/*
  Curso JWPLC Basic · Lección 10 · Contadores

  Qué vas a aprender
  - A contar eventos: cada pieza que pasa por un sensor suma 1.
  - A comparar la cuenta con una meta y actuar al llegar.
  - A juntar lo aprendido: flancos (lección 07), struct con su función
    (lección 09) y una regla de marcha (lección 08).

  El caso: llenar cajas en una cinta
  - La cinta avanza y cada pieza que cae en la caja pasa frente a un sensor.
  - Al llegar a 10 piezas la cinta se detiene y se enciende la lámpara
    "caja llena". El operador cambia la caja y aprieta RESET.

  Qué conectar
  - Borne 1M a 0 V.
  - I0.0: sensor PNP de piezas (o un pulsador para simularlo).
  - I0.1: pulsador RESET.
  - Q0.0: contactor del motor de la cinta.
  - Q0.1: lámpara "caja llena".

  Cómo probarlo
  1. Sube el programa y abre el Monitor Serie a 115200 baudios.
  2. Q0.0 está cerrado: la cinta avanza.
  3. Activa I0.0 diez veces: verás la cuenta y, en la décima, la cinta se
     detiene y Q0.1 se enciende.
  4. Aprieta RESET: la cuenta vuelve a 0 y la cinta arranca otra vez.

  Para practicar
  - Cambia la meta a 24 piezas.
  - Cuenta también las cajas llenas y muéstralas en el Monitor Serie.
*/

const uint16_t PIEZAS_POR_CAJA = 10;
const uint32_t ANTIRREBOTE_MS = 20;

uint16_t piezas = 0;
uint32_t ultimaLectura = 0;

// Detector de flanco de subida (como en la lección 07), dentro de un struct:
// cada detector recuerda su lectura anterior. subida() devuelve true solo en
// la vuelta en que la señal pasa de apagada a activa.
struct Flanco
{
    bool antes = false;

    bool subida(bool ahora)
    {
        bool flanco = ahora && !antes;
        antes = ahora;
        return flanco;
    }
};

// Un detector para cada señal.
Flanco flancoSensor;
Flanco flancoReset;

void setup()
{
    Serial.begin(115200);
    Serial.println("Lección 10: piezas en I0.0, RESET en I0.1");
}

void loop()
{
    if (millis() - ultimaLectura < ANTIRREBOTE_MS)
    {
        return;
    }
    ultimaLectura = millis();

    bool cajaLlena = piezas >= PIEZAS_POR_CAJA;

    // Solo se cuentan piezas mientras la caja no esté llena.
    if (flancoSensor.subida(digitalRead(I0_0) == HIGH) && !cajaLlena)
    {
        piezas++;
        Serial.print("Piezas: ");
        Serial.println(piezas);

        if (piezas == PIEZAS_POR_CAJA)
        {
            Serial.println("Caja llena: cambia la caja y aprieta RESET");
        }
    }

    if (flancoReset.subida(digitalRead(I0_1) == HIGH))
    {
        piezas = 0;
        Serial.println("RESET: caja nueva");
    }

    // Se vuelve a calcular porque la cuenta pudo cambiar en esta vuelta.
    cajaLlena = piezas >= PIEZAS_POR_CAJA;
    digitalWrite(Q0_0, !cajaLlena);
    digitalWrite(Q0_1, cajaLlena);
}
