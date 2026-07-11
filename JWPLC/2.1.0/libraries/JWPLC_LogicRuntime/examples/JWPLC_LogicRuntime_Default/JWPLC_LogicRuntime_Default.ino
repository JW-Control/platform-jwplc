#include <JWPLC_LogicRuntime.h>

JWPLC_LogicRuntime runtime;

uint32_t lastReportMs = 0;

void setup()
{
  Serial.begin(115200);
  delay(200);

  Serial.println();
  Serial.println("JWPLC Logic Runtime - PoC 0");

  if (!runtime.begin())
  {
    Serial.print("Error de inicializacion: ");
    Serial.println(JWPLC_LogicRuntime::errorName(runtime.lastError()));
    return;
  }

  const LogicStorageProfile &profile = runtime.storageProfile();

  Serial.print("FRAM configurada: ");
  Serial.print(profile.framBytes);
  Serial.println(" bytes");

  Serial.print("Limite inicial: ");
  Serial.print(profile.maxBlocks);
  Serial.println(" bloques");

  Serial.print("Slot de programa: ");
  Serial.print(profile.programSlotBytes);
  Serial.println(" bytes");

  if (!runtime.start())
  {
    Serial.print("No se pudo iniciar: ");
    Serial.println(JWPLC_LogicRuntime::errorName(runtime.lastError()));
    return;
  }

  Serial.println("Runtime iniciado.");
}

void loop()
{
  if (!runtime.tick())
  {
    delay(100);
    return;
  }

  const uint32_t now = millis();
  if (now - lastReportMs >= 1000)
  {
    lastReportMs = now;

    Serial.print("Estado: ");
    Serial.print(JWPLC_LogicRuntime::stateName(runtime.state()));
    Serial.print(" | scans: ");
    Serial.print(runtime.scanCount());
    Serial.print(" | ultimo scan: ");
    Serial.print(runtime.lastScanMicros());
    Serial.println(" us");
  }

  delay(1);
}
