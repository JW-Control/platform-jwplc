#include <JWPLC_LogicRuntime.h>

static JWPLC_LogicRuntime runtime;

static uint16_t passedTests = 0;
static uint16_t failedTests = 0;

static void expect(const char *name, bool condition)
{
  Serial.print(condition ? "[PASS] " : "[FAIL] ");
  Serial.println(name);

  if (condition)
  {
    ++passedTests;
  }
  else
  {
    ++failedTests;
  }
}

void setup()
{
  Serial.begin(115200);
  delay(300);

  Serial.println();
  Serial.println("JWPLC Logic Runtime - perfil compilado de memoria");
  Serial.println("No inicializa E/S ni lee o escribe la FRAM.");
  Serial.println();

  Serial.print("Bloques compilados: ");
  Serial.println(JWPLC_LOGIC_COMPILED_MAX_BLOCKS);
  Serial.print("Limite efectivo FRAM 8 KiB: ");
  Serial.println(JWPLCLogicStorageProfiles::FRAM_8K.maxBlocks);
  Serial.print("Limite efectivo FRAM 32 KiB: ");
  Serial.println(JWPLCLogicStorageProfiles::FRAM_32K.maxBlocks);
  Serial.print("Capacidad fisica FRAM 8 KiB: ");
  Serial.println(JWPLCLogicStorageProfiles::FRAM_8K_PHYSICAL_MAX_BLOCKS);
  Serial.print("Capacidad fisica FRAM 32 KiB: ");
  Serial.println(JWPLCLogicStorageProfiles::FRAM_32K_PHYSICAL_MAX_BLOCKS);
  Serial.println();

  Serial.print("sizeof(LogicBlockDefinition): ");
  Serial.println(sizeof(LogicBlockDefinition));
  Serial.print("sizeof(LogicBlockState): ");
  Serial.println(sizeof(LogicBlockState));
  Serial.print("sizeof(LogicProgramBuffer): ");
  Serial.println(sizeof(LogicProgramBuffer));
  Serial.print("sizeof(LogicEngine): ");
  Serial.println(sizeof(LogicEngine));
  Serial.print("sizeof(JWPLCLogicStorage): ");
  Serial.println(sizeof(JWPLCLogicStorage));
  Serial.print("sizeof(JWPLC_LogicRuntime): ");
  Serial.println(sizeof(JWPLC_LogicRuntime));
  Serial.println();

  const size_t bufferBlockCapacity =
      sizeof(((LogicProgramBuffer *)nullptr)->blocks) /
      sizeof(LogicBlockDefinition);

  expect("build predeterminado reserva 100 bloques",
         JWPLC_LOGIC_COMPILED_MAX_BLOCKS == 100);
  expect("perfil 8 KiB tiene capacidad fisica de 100 bloques",
         JWPLCLogicStorageProfiles::FRAM_8K_PHYSICAL_MAX_BLOCKS == 100);
  expect("perfil 32 KiB conserva capacidad fisica de 400 bloques",
         JWPLCLogicStorageProfiles::FRAM_32K_PHYSICAL_MAX_BLOCKS == 400);
  expect("limite efectivo de 8 KiB es 100 bloques",
         JWPLCLogicStorageProfiles::FRAM_8K.maxBlocks == 100);
  expect("build actual limita tambien 32 KiB a 100 bloques",
         JWPLCLogicStorageProfiles::FRAM_32K.maxBlocks == 100);
  expect("LogicProgramBuffer reserva exactamente el limite compilado",
         bufferBlockCapacity == JWPLC_LOGIC_COMPILED_MAX_BLOCKS);
  expect("imagen de 100 bloques cabe en el slot de 8 KiB",
         LogicProgramCodec::requiredSize(100) <=
             JWPLCLogicStorageLayouts::FRAM_8K.slotPayloadBytes());
  expect("imagen de 400 bloques cabe fisicamente en el slot de 32 KiB",
         LogicProgramCodec::requiredSize(400) <=
             JWPLCLogicStorageLayouts::FRAM_32K.slotPayloadBytes());
  expect("perfil seleccionado para 8192 bytes usa 100 bloques",
         JWPLCLogicStorageProfiles::forCapacity(8192).maxBlocks == 100);
  expect("capacidad menor a 8 KiB sigue sin soporte",
         JWPLCLogicStorageProfiles::forCapacity(4096).maxBlocks == 0);

  Serial.println();
  Serial.print("Resultado: ");
  Serial.print(passedTests);
  Serial.print(" PASS, ");
  Serial.print(failedTests);
  Serial.println(" FAIL");

  Serial.println(failedTests == 0
                     ? "PERFIL COMPILADO 100 BLOQUES: PASS"
                     : "PERFIL COMPILADO 100 BLOQUES: FAIL");
}

void loop()
{
}
