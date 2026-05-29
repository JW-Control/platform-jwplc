#include <JW_FRAM.h>

JW_FRAM fram(5);  // Pin CS

void setup() {
  Serial.begin(115200);
  while (!Serial) {}

  fram.enableDebug(Serial);

  if (!fram.begin(8 * 1024)) {
    Serial.println("Fallo al inicializar la FRAM");
    while (1) {}
  }

  String nombreEquipo = "JWPLC Basic";
  if (fram.writeString(0, nombreEquipo)) {
    Serial.println("String almacenado correctamente");
  }

  String nombreLeido;
  if (fram.readString(0, nombreLeido)) {
    Serial.print("readString(): ");
    Serial.println(nombreLeido);
  }

  const char *nombreProyecto = "JW Control";
  if (fram.writeCString(64, nombreProyecto)) {
    Serial.println("CString almacenado correctamente");
  }

  char buffer[32];
  if (fram.readCString(64, buffer, sizeof(buffer))) {
    Serial.print("readCString(): ");
    Serial.println(buffer);
  }
}

void loop() {}
