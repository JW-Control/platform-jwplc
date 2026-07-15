#include <JWPLC_GlobalPeripherals.h>
#include <JWPLC_LogicRuntime.h>
#include <JWPLC_LogicRuntime_UI.h>

static JWPLC_LogicRuntime runtime;

void setup()
{
  Serial.begin(115200);
  delay(300);

  Serial.println();
  Serial.println("JWPLC Logic Runtime UI - Home y Programa");
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
  Serial.println("Home: OK en PROGRAMA abre el control del runtime.");
  Serial.println("Programa: PREPARAR, RUN, STOP y VOLVER.");
  Serial.println("Las acciones se procesan fuera del callback grafico.");
}

void loop()
{
  // El scan lógico sigue siendo explícito y no pertenece a la capa gráfica.
  if (runtime.state() == JWPLCLogicRuntimeState::Running)
  {
    runtime.tick();
  }

  // Sincroniza IDLE y procesa acciones diferidas solicitadas desde USER.
  JWPLC_LogicRuntime_UI.update();
  delay(1);
}