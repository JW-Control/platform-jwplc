#include <JWPLC_GlobalPeripherals.h>
#include <JWPLC_LogicRuntime.h>
#include <JWPLC_LogicRuntime_UI.h>

static JWPLC_LogicRuntime runtime;

static const LogicBlockDefinition DEMO_BLOCKS[] = {
    // B00: Entrada digital I0.0
    {LogicBlockType::DigitalInput,
     JWPLC_LOGIC_NO_SOURCE,
     JWPLC_LOGIC_NO_SOURCE,
     0,
     0},

    // B01: Entrada digital I0.1
    {LogicBlockType::DigitalInput,
     JWPLC_LOGIC_NO_SOURCE,
     JWPLC_LOGIC_NO_SOURCE,
     1,
     0},

    // B02: NOT de B00
    {LogicBlockType::Not,
     0,
     JWPLC_LOGIC_NO_SOURCE,
     0,
     0},

    // B03: B00 AND B01
    {LogicBlockType::And,
     0,
     1,
     0,
     0},

    // B04: B02 OR B03
    {LogicBlockType::Or,
     2,
     3,
     0,
     0},

    // B05: SET desde B03, RESET desde B01, retentivo en RAM.
    {LogicBlockType::SetReset,
     3,
     1,
     0,
     0,
     JWPLC_LOGIC_BLOCK_FLAG_RETENTIVE},

    // B06: TON de 2000 ms alimentado por B04.
    {LogicBlockType::Ton,
     4,
     JWPLC_LOGIC_NO_SOURCE,
     0,
     2000}};

static const LogicProgram DEMO_PROGRAM = {
    "DEMO UI BLOQUES",
    DEMO_BLOCKS,
    static_cast<uint16_t>(sizeof(DEMO_BLOCKS) / sizeof(DEMO_BLOCKS[0]))};

void setup()
{
  Serial.begin(115200);
  delay(300);

  Serial.println();
  Serial.println("JWPLC Logic Runtime UI - diagrama grafico de bloques");
  Serial.println("Carga un programa RAM de 7 bloques sin salidas digitales.");
  Serial.println("No formatea ni escribe la FRAM.");
  Serial.println();

  const bool storageOk = runtime.storage().begin(JWPLC_FRAM);
  const bool runtimeOk =
      runtime.begin(JWPLCLogicStorageProfiles::FRAM_8K.framBytes);
  const bool programOk = runtimeOk && runtime.loadProgram(DEMO_PROGRAM);
  const bool startOk = programOk && runtime.start();
  const bool uiOk = JWPLC_LogicRuntime_UI.begin(runtime);

  Serial.print("Storage: ");
  Serial.println(storageOk ? "OK" : "FAIL");
  Serial.print("Runtime: ");
  Serial.println(runtimeOk ? "OK" : "FAIL");
  Serial.print("Programa RAM: ");
  Serial.println(programOk ? "OK" : "FAIL");
  Serial.print("RUN: ");
  Serial.println(startOk ? "OK" : "FAIL");
  Serial.print("Runtime UI: ");
  Serial.println(uiOk ? "OK" : "FAIL");
  Serial.println();
  Serial.println("USER -> BLOQUES abre DIAGRAMA como vista principal.");
  Serial.println("UP/DOWN cambian el bloque central.");
  Serial.println("LEFT/RIGHT eligen DETALLE, LISTA o VOLVER.");
  Serial.println("OK ejecuta la accion; ESC vuelve directamente a IDLE.");
}

void loop()
{
  JWPLC_LogicRuntime_UI.update();

  if (runtime.state() == JWPLCLogicRuntimeState::Running)
  {
    runtime.tick();
  }

  delay(1);
}