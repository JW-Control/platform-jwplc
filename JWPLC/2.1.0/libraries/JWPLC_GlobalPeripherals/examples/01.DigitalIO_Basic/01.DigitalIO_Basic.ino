/*
  01.DigitalIO_Basic

  Las 8 entradas se copian a las 8 salidas: I0_0 -> Q0_0, ... I0_7 -> Q0_7.

  Muestra las E/S «en bloque» con el objeto JWPLC_IO:
  - JWPLC_IO.inputs()         las 8 entradas en un byte (bit 0 = I0_0).
  - JWPLC_IO.setOutputs(bits) escribe las 8 salidas de una vez (bit 0 = Q0_0).
  - JWPLC_IO.outputs()        estado de las 8 salidas.
  - JWPLC_IO.input(n) / JWPLC_IO.output(n): un solo punto.
  JWPLC_IO lee lo que el package ya escaneó: no hace lecturas extra del bus.
  Para un solo punto también sirven digitalRead(I0_0) y digitalWrite(Q0_0, HIGH).

  Qué conectar: pulsadores o interruptores en I0_0..I0_7 (o «Simular
  entradas I» en JWPLC Sim).
  Qué mirar: los LED de Q0_0..Q0_7 siguen a las entradas; el Monitor serie
  (115200) imprime cada 500 ms «IN=0xA5 OUT=0xA5 I0_0=1 Q0_0=1».
*/

const uint32_t PERIODO_MONITOR_MS = 500;
uint32_t ultimoMonitorMs = 0;

// Imprime un byte como 0x05, 0xA5...
void imprimirHex(uint8_t valor)
{
    Serial.print("0x");
    if (valor < 0x10)
        Serial.print('0');
    Serial.print(valor, HEX);
}

void setup()
{
    Serial.begin(115200);

    // Arrancar con todas las salidas apagadas.
    JWPLC_IO.setOutputs(0x00);

    Serial.println("JWPLC Basic - Digital I/O");
}

void loop()
{
    // Espejo: cada entrada en su salida.
    const uint8_t entradas = JWPLC_IO.inputs();
    JWPLC_IO.setOutputs(entradas);

    if (millis() - ultimoMonitorMs >= PERIODO_MONITOR_MS)
    {
        ultimoMonitorMs = millis();

        Serial.print("IN=");
        imprimirHex(entradas);
        Serial.print(" OUT=");
        imprimirHex(JWPLC_IO.outputs());
        Serial.print(" I0_0=");
        Serial.print(JWPLC_IO.input(0));
        Serial.print(" Q0_0=");
        Serial.println(JWPLC_IO.output(0));
    }
}
