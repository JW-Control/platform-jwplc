#include <JW_FRAM.h>

JW_FRAM fram(13);  // Pin CS

void setup() {
  Serial.begin(115200);
  while (!Serial) {}

  fram.enableDebug(Serial);

  if (!fram.begin(8 * 1024)) {
    Serial.println("Fallo al inicializar la FRAM");
    while (1) {}
  }

  int32_t contador = 100;
  float ganancia = 1.2345f;
  bool habilitado = true;

  fram.put(0, contador);
  fram.put(8, ganancia);
  fram.put(16, habilitado);

  contador = 101;
  fram.update(0, contador);

  int32_t contadorLeido = 0;
  float gananciaLeida = 0.0f;
  bool habilitadoLeido = false;

  fram.get(0, contadorLeido);
  fram.get(8, gananciaLeida);
  fram.get(16, habilitadoLeido);

  Serial.print("Contador: ");
  Serial.println(contadorLeido);

  Serial.print("Ganancia: ");
  Serial.println(gananciaLeida, 4);

  Serial.print("Habilitado: ");
  Serial.println(habilitadoLeido ? "true" : "false");
}

void loop() {}
