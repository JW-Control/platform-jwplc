#include <JW_FRAM.h>

JW_FRAM fram(5);  // Pin CS

struct PlcConfig {
  uint8_t mode;
  bool enabled;
  float kp;
  float ki;
  float kd;
  char machineName[24];
};

void setup() {
  Serial.begin(115200);
  while (!Serial) {}

  fram.enableDebug(Serial);

  if (!fram.begin(8 * 1024)) {
    Serial.println("Fallo al inicializar la FRAM");
    while (1) {}
  }

  PlcConfig cfg = {
      2,
      true,
      1.50f,
      0.30f,
      0.05f,
      "JWPLC_Basic_V2"
  };

  fram.put(0, cfg);

  PlcConfig cargado{};
  if (fram.get(0, cargado)) {
    Serial.println("Struct recuperado:");
    Serial.print("Modo: ");
    Serial.println(cargado.mode);
    Serial.print("Enabled: ");
    Serial.println(cargado.enabled ? "true" : "false");
    Serial.print("Kp: ");
    Serial.println(cargado.kp, 3);
    Serial.print("Machine: ");
    Serial.println(cargado.machineName);
  }
}

void loop() {}
