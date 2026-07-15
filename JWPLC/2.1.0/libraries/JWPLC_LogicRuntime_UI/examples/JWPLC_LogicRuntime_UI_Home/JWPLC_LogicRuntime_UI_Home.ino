#include <JWPLC_GlobalPeripherals.h>
#include <JWPLC_LogicRuntime.h>
#include <JWPLC_LogicRuntime_UI.h>

static JWPLC_LogicRuntime runtime;

void setup()
{
  Serial.begin(115200);
  delay(300);

  Serial.println();
  Serial.println("JWPLC Logic Runtime UI - pantalla principal USER");
  Serial.println("IDLE conserva monitor I/O y LEDs del package.");
  Serial.println("Pulsa cualquier boton para entrar a USER.");
  Serial.println();

  const bool storageOk = runtime.storage().begin(JWPLC_FRAM);
  const bool runtimeOk =
      runtime.begin(JWPLCLogicStorageProfiles::FRAM_8K.framBytes);
  const bool uiOk = JWPLC_LogicRuntime_UI.begin(runtime);

  Serial.print("Storage: ");
  Serial.println(storageOk ? "OK" : "FAIL");
  Serial.print("Runtime: ");
  Serial.println(runtimeOk ? "OK" : "FAIL");
  Serial.print("Runtime UI: ");
  Serial.println(uiOk ? "OK" : "FAIL");
  Serial.println();
  Serial.println("En USER: flechas mueven el selector, OK confirma y ESC vuelve a IDLE.");
}

void loop()
{
  // Mantiene RUN/ERR de la pantalla IDLE sincronizados con el runtime.
  JWPLC_LogicRuntime_UI.update();
  delay(1);
}
