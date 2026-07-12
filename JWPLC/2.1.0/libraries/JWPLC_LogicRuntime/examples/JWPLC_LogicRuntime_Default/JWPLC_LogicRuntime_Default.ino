#include <JWPLC_LogicRuntime.h>

JWPLC_LogicRuntime runtime;

static const LogicBlockDefinition DEFAULT_BLOCKS[] = {
    // 0: I0_0
    {LogicBlockType::DigitalInput,
     JWPLC_LOGIC_NO_SOURCE,
     JWPLC_LOGIC_NO_SOURCE,
     0,
     0},

    // 1: I0_1
    {LogicBlockType::DigitalInput,
     JWPLC_LOGIC_NO_SOURCE,
     JWPLC_LOGIC_NO_SOURCE,
     1,
     0},

    // 2: NOT I0_1
    {LogicBlockType::Not,
     1,
     JWPLC_LOGIC_NO_SOURCE,
     0,
     0},

    // 3: I0_0 AND NOT I0_1
    {LogicBlockType::And,
     0,
     2,
     0,
     0},

    // 4: TON de 2 segundos
    {LogicBlockType::Ton,
     3,
     JWPLC_LOGIC_NO_SOURCE,
     0,
     2000},

    // 5: resultado hacia Q0_0
    {LogicBlockType::DigitalOutput,
     4,
     JWPLC_LOGIC_NO_SOURCE,
     0,
     0}};

static const LogicProgram DEFAULT_PROGRAM = {
    "JWPLC Default",
    DEFAULT_BLOCKS,
    static_cast<uint16_t>(sizeof(DEFAULT_BLOCKS) / sizeof(DEFAULT_BLOCKS[0]))};

uint32_t lastReportMs = 0;

void setup()
{
  Serial.begin(115200);
  delay(200);

  Serial.println();
  Serial.println("JWPLC Logic Runtime - PoC 1");
  Serial.println("Logica: I0_0 AND NOT I0_1 -> TON 2 s -> Q0_0");

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

  Serial.print("Bloques del programa: ");
  Serial.println(DEFAULT_PROGRAM.blockCount);

  if (!runtime.loadProgram(DEFAULT_PROGRAM))
  {
    Serial.print("Programa invalido: ");
    Serial.println(LogicValidator::errorName(runtime.validationError()));
    return;
  }

  if (!runtime.start())
  {
    Serial.print("No se pudo iniciar: ");
    Serial.println(JWPLC_LogicRuntime::errorName(runtime.lastError()));
    return;
  }

  Serial.println("Runtime iniciado. Las salidas no usadas permanecen apagadas.");
}

void loop()
{
  if (!runtime.tick())
  {
    Serial.print("Fallo de runtime: ");
    Serial.println(JWPLC_LogicRuntime::errorName(runtime.lastError()));
    delay(1000);
    return;
  }

  const uint32_t now = millis();
  if (now - lastReportMs >= 500)
  {
    lastReportMs = now;

    Serial.print("I0_0=");
    Serial.print(runtime.blockValue(0));
    Serial.print(" I0_1=");
    Serial.print(runtime.blockValue(1));
    Serial.print(" AND=");
    Serial.print(runtime.blockValue(3));
    Serial.print(" TON=");
    Serial.print(runtime.blockValue(4));
    Serial.print(" Q0_0=");
    Serial.print(runtime.blockValue(5));
    Serial.print(" | scan=");
    Serial.print(runtime.lastScanMicros());
    Serial.println(" us");
  }

  delay(1);
}
