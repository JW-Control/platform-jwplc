/*
  Este sketch configura un pin de entrada digital. 
  Entradas digitales (DI) (a través del expansor TCA6424A):
    I0_0 = 0x2207
    I0_1 = 0x2206
    I0_2 = 0x2205
    I0_3 = 0x2204
    I0_4 = 0x2203
    I0_5 = 0x2202
*/

void setup() {
  Serial.begin(115200);
  // Configura I0_0 como entrada digital.
  pinMode(I0_0, INPUT);
  Serial.println("Digital Input configurado en I0_0.");
}

void loop() {
  int estado = digitalRead(I0_0);
  Serial.print("Estado digital de I0_0: ");
  Serial.println(estado);
  delay(500);
}