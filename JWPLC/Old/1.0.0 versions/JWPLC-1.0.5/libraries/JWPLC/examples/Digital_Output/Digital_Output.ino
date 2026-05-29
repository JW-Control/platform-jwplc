/*En este ejemplo se configura un pin como salida digital. 
  Salidas digitales (DO) (a través del expansor TCA6424A):
    Q0_0 = 0x2208  // RELE Q0.0
    Q0_1 = 0x2209  // RELE Q0.1
    Q0_2 = 0x220A  // RELE Q0.2
    Q0_3 = 0x220B  // RELE Q0.3
    Q0_4 = 0x220C  // RELE Q0.4
    Q0_5 = 0x220D  // RELE Q0.5
    Q0_6 = 0x220E  // RELE Q0.6
    Q0_7 = 0x220F  // RELE Q0.7

  Salidas digitales directas (MO) del ESP32:
    Q0_8 = 14   // TRANSISTOR Q0.8
    Q0_9 = 12   // TRANSISTOR Q0.9

*/

void setup() {
  Serial.begin(115200);
  // Configura Q0_0 como salida digital.
  pinMode(Q0_0, OUTPUT);
  Serial.println("Digital Output configurado en Q0_0.");
}

void loop() {
  digitalWrite(Q0_0, HIGH);
  Serial.println("Q0_0 en HIGH");
  delay(500);
  digitalWrite(Q0_0, LOW);
  Serial.println("Q0_0 en LOW");
  delay(500);
}