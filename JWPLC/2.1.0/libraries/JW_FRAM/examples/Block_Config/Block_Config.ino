#include <JW_FRAM.h>

JW_FRAM fram(13);  // Pin CS de la FRAM en el JWPLC Basic (GPIO5 es el CS del W5500)

struct SystemConfig {
  uint8_t configVersion;
  bool firstBootDone;
  float kp;
  float ki;
  float kd;
  uint32_t totalStarts;
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

  SystemConfig cfg = {
      1,
      true,
      2.0f,
      0.5f,
      0.1f,
      42,
      "JWPLC Coffee & Milk"
  };

  if (fram.writeBlock(128, cfg, 1)) {
    Serial.println("Bloque validado escrito correctamente");
  }

  SystemConfig restaurado{};
  if (fram.readBlock(128, restaurado, 1)) {
    Serial.println("Bloque validado leido correctamente");
    Serial.print("Machine: ");
    Serial.println(restaurado.machineName);
    Serial.print("Starts: ");
    Serial.println(restaurado.totalStarts);
  } else {
    Serial.println("Bloque invalido o incompatible");
  }
}

void loop() {}
