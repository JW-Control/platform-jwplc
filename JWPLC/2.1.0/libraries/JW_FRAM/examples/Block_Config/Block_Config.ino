/*
  Block_Config

  Guarda una configuración (struct) como bloque validado: writeBlock() le
  añade versión y checksum, y readBlock() solo la acepta si coinciden. Así
  una FRAM nueva o un struct cambiado no cargan basura.

  updateBlock() guarda solo si el bloque cambió: sirve para llamarlo en
  loop() sin gastar tiempo de bus cuando nada cambió.

  En el JWPLC Basic usa el objeto global JWPLC_FRAM (mismos métodos).

  Qué mirar: el Monitor serie (115200) imprime «Bloque validado leido
  correctamente», el nombre de la máquina y los dos mensajes de updateBlock().
*/

#include <JW_FRAM.h>

JW_FRAM fram(JWPLC_FRAM_CS);  // CS de la FRAM del JWPLC Basic (GPIO13)

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

  // updateBlock(): guarda solo si cambió (no gasta escrituras ni tiempo de bus
  // si el bloque ya es igual). Útil para guardar en loop() «cuando cambie».
  if (fram.updateBlock(128, cfg, 1)) {
    Serial.println("updateBlock(): sin cambios, no se reescribe");
  }
  cfg.totalStarts++;
  if (fram.updateBlock(128, cfg, 1)) {
    Serial.println("updateBlock(): cambio guardado");
  }
}

void loop() {}
