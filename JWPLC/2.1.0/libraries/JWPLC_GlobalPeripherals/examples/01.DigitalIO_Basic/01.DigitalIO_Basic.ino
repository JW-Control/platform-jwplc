/*
  01.DigitalIO_Basic

  Las 8 entradas se copian a las 8 salidas: I0_0 -> Q0_0, ... I0_7 -> Q0_7.

  Usa las mismas funciones que un Arduino:
  - digitalRead(I0_0)                 lee una entrada (HIGH o LOW).
  - digitalWrite(Q0_0, HIGH)          escribe una salida.
  - digitalReadBlock(I0_X)            las 8 entradas en un byte (bit 0 = I0_0).
  - digitalWriteBlock(Q0_X, bits)     escribe las 8 salidas de una vez (bit 0 = Q0_0).
  - digitalReadBlock(Q0_X)            estado de las 8 salidas.
  I0_X y Q0_X son las listas de las 8 entradas y las 8 salidas que trae el
  package. Leer no hace accesos extra al bus: se usa lo que el package ya escaneó.

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
    digitalWriteBlock(Q0_X, 0x00);

    Serial.println("JWPLC Basic - Digital I/O");
}

void loop()
{
    // Espejo: cada entrada en su salida.
    const uint8_t entradas = digitalReadBlock(I0_X);
    digitalWriteBlock(Q0_X, entradas);

    if (millis() - ultimoMonitorMs >= PERIODO_MONITOR_MS)
    {
        ultimoMonitorMs = millis();

        Serial.print("IN=");
        imprimirHex(entradas);
        Serial.print(" OUT=");
        imprimirHex(digitalReadBlock(Q0_X));
        Serial.print(" I0_0=");
        Serial.print(digitalRead(I0_0));
        Serial.print(" Q0_0=");
        Serial.println(digitalRead(Q0_0));
    }
}
