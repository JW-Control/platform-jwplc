/*
Este ejemplo configura un pin nativo para salida PWM mediante la función analogConfig,
y luego usa analogWrite para variar el duty cycle.
  Salidas digitales directas (MO) del ESP32 compatibles con PWM:
    Q0_8 = 14   // TRANSISTOR Q0.8
    Q0_9 = 12   // TRANSISTOR Q0.9
*/

void setup() {
  Serial.begin(115200);
  // Configura el pin 32 para PWM a 1000 Hz con resolución de 8 bits.
  analogConfig(Q0_9, 1000, 8);
  Serial.println("PWM configurado en pin 32 (1000 Hz, 8 bits).");
  // Establece un duty inicial (50% para 8 bits: 128 de 0-255).
  analogWrite(Q0_9, 128);
}

void loop() {
  // Aumenta y disminuye gradualmente el duty cycle.
  for (int duty = 0; duty <= 255; duty += 5) {
    analogWrite(32, duty);
    Serial.print("Duty: ");
    Serial.println(duty);
    delay(50);
  }
  for (int duty = 255; duty >= 0; duty -= 5) {
    analogWrite(32, duty);
    Serial.print("Duty: ");
    Serial.println(duty);
    delay(50);
  }
}