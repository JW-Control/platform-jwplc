#include <JWPLC_GlobalPeripherals.h>
#include <JWPLC_LogicRuntime.h>
#include <LogicV1ToV2Adapter.h>

#include <cstring>

static constexpr uint16_t BLOCK_COUNT = 5;
static constexpr uint16_t LINK_COUNT = 4;

static LogicV2EnginePrototype engine;
static uint16_t passedTests = 0;
static uint16_t failedTests = 0;
static bool referenceValues[BLOCK_COUNT] = {};

static const LogicBlockDefinition V1_BLOCKS[] = {
    {LogicBlockType::DigitalInput, JWPLC_LOGIC_NO_SOURCE, JWPLC_LOGIC_NO_SOURCE, 0, 0},
    {LogicBlockType::DigitalInput, JWPLC_LOGIC_NO_SOURCE, JWPLC_LOGIC_NO_SOURCE, 1, 0},
    {LogicBlockType::SetReset, 0, 1},
    {LogicBlockType::Not, 2},
    {LogicBlockType::DigitalOutput, 2, JWPLC_LOGIC_NO_SOURCE, 0, 0}};

static const LogicProgram V1_PROGRAM = {
    "SETRESET V1 V2",
    V1_BLOCKS,
    BLOCK_COUNT};

static void expect(const char *name, bool condition)
{
  Serial.print(condition ? "[PASS] " : "[FAIL] ");
  Serial.println(name);
  condition ? ++passedTests : ++failedTests;
}

static bool evaluateV1Reference(const bool inputs[8])
{
  for (uint16_t index = 0; index < BLOCK_COUNT; ++index)
  {
    const LogicBlockDefinition &block = V1_BLOCKS[index];
    switch (block.type)
    {
    case LogicBlockType::DigitalInput:
      referenceValues[index] = inputs[block.resource];
      break;
    case LogicBlockType::SetReset:
    {
      const bool setValue = referenceValues[block.sourceA];
      const bool resetValue = referenceValues[block.sourceB];
      if (resetValue)
      {
        referenceValues[index] = false;
      }
      else if (setValue)
      {
        referenceValues[index] = true;
      }
      break;
    }
    case LogicBlockType::Not:
      referenceValues[index] = !referenceValues[block.sourceA];
      break;
    case LogicBlockType::DigitalOutput:
      referenceValues[index] = referenceValues[block.sourceA];
      break;
    default:
      return false;
    }
  }
  return true;
}

static bool allValuesMatch()
{
  for (uint16_t index = 0; index < BLOCK_COUNT; ++index)
  {
    if (referenceValues[index] != engine.blockValue(index))
    {
      return false;
    }
  }
  return true;
}

static void testStep(const char *label,
                     const bool inputs[8],
                     bool expectedSetReset)
{
  char message[88];

  snprintf(message, sizeof(message), "%s: referencia v1 evalua", label);
  expect(message, evaluateV1Reference(inputs));

  snprintf(message, sizeof(message), "%s: motor v2 ejecuta scan", label);
  expect(message, engine.scan(inputs, 8));

  snprintf(message, sizeof(message), "%s: todos los bloques coinciden", label);
  expect(message, allValuesMatch());

  snprintf(message, sizeof(message), "%s: SET/RESET coincide", label);
  expect(message, engine.blockValue(2) == expectedSetReset);

  snprintf(message, sizeof(message), "%s: Q0 logica coincide", label);
  expect(message, engine.digitalOutputValue(0) == expectedSetReset);
}

void setup()
{
  Serial.begin(115200);
  delay(300);

  Serial.println();
  Serial.println("JWPLC Logic Runtime - SET/RESET v1 a v2 en RAM");
  Serial.println("No inicializa E/S, no conmuta Q0 y no usa la FRAM.");
  Serial.println();

  expect("SET/RESET v2 conserva descriptor de 12 bytes",
         sizeof(LogicV2BlockRecord) == 12);
  expect("SET/RESET usa enlaces de 2 bytes",
         sizeof(LogicV2InputLink) == 2);

  size_t requiredLinks = 0;
  LogicV1ToV2AdapterError adapterError =
      LogicV1ToV2AdapterError::None;

  expect("adaptador calcula enlaces con SET/RESET",
         LogicV1ToV2Adapter::requiredLinkCount(
             V1_PROGRAM,
             requiredLinks,
             adapterError));
  expect("programa requiere cuatro enlaces",
         requiredLinks == LINK_COUNT);
  expect("calculo de enlaces no deja error",
         adapterError == LogicV1ToV2AdapterError::None);

  LogicV2BlockRecord convertedBlocks[BLOCK_COUNT];
  LogicV2InputLink convertedLinks[LINK_COUNT];
  LogicV2Program convertedProgram = {nullptr, 0, nullptr, 0};

  expect("adaptador convierte SET/RESET",
         LogicV1ToV2Adapter::convert(
             V1_PROGRAM,
             convertedBlocks,
             BLOCK_COUNT,
             convertedLinks,
             LINK_COUNT,
             convertedProgram,
             adapterError));
  expect("conversion conserva cinco bloques",
         convertedProgram.blockCount == BLOCK_COUNT);
  expect("conversion genera cuatro enlaces",
         convertedProgram.linkCount == LINK_COUNT);
  expect("B02 se convierte como SET/RESET",
         convertedBlocks[2].type == LogicV2BlockType::SetReset);
  expect("SET/RESET usa dos enlaces desde el inicio",
         convertedBlocks[2].firstInput == 0 &&
             convertedBlocks[2].inputCount == 2);
  expect("enlace S conserva B00 y enlace R conserva B01",
         convertedLinks[0].source() == 0 &&
             convertedLinks[1].source() == 1);
  expect("DigitalOutput conserva la salida del SET/RESET",
         convertedLinks[3].source() == 2);

  expect("validador v2 acepta SET/RESET no retentivo",
         LogicVariableInputPrototype::validate(
             convertedProgram,
             100,
             512,
             8,
             8) == LogicV2PrototypeError::None);
  expect("motor carga programa con SET/RESET",
         engine.loadProgram(convertedProgram, 8, 8));
  expect("motor conserva perfil de ocho salidas",
         engine.digitalOutputCount() == 8);
  expect("motor inicia programa con SET/RESET",
         engine.start());

  const bool allFalse[8] = {};
  const bool setOnly[8] = {
      true, false, false, false,
      false, false, false, false};
  const bool setAndReset[8] = {
      true, true, false, false,
      false, false, false, false};

  testStep("estado inicial", allFalse, false);
  testStep("SET activo", setOnly, true);
  testStep("memoria sin entradas", allFalse, true);
  testStep("SET y RESET simultaneos", setAndReset, false);
  testStep("mantiene reset", allFalse, false);
  testStep("SET vuelve a activar", setOnly, true);

  engine.stop();
  expect("stop deja motor detenido",
         engine.state() == LogicV2EngineState::Stopped);
  expect("stop limpia SET/RESET",
         !engine.blockValue(2));
  expect("stop limpia Q0 logica",
         !engine.digitalOutputValue(0));
  expect("motor reinicia despues de stop",
         engine.start());
  memset(referenceValues, 0, sizeof(referenceValues));
  expect("scan inicial funciona despues de reiniciar",
         engine.scan(allFalse, 8));
  expect("SET/RESET reinicia en FALSE",
         !engine.blockValue(2));

  LogicV2BlockRecord invalidBlocks[BLOCK_COUNT];
  LogicV2InputLink invalidLinks[LINK_COUNT];
  memcpy(invalidBlocks, convertedBlocks, sizeof(invalidBlocks));
  memcpy(invalidLinks, convertedLinks, sizeof(invalidLinks));
  LogicV2Program invalidProgram = {
      invalidBlocks,
      BLOCK_COUNT,
      invalidLinks,
      LINK_COUNT};

  invalidBlocks[2].inputCount = 1;
  expect("SET/RESET rechaza una sola entrada",
         LogicVariableInputPrototype::validate(
             invalidProgram,
             100,
             512,
             8,
             8) == LogicV2PrototypeError::InvalidInputCount);

  memcpy(invalidBlocks, convertedBlocks, sizeof(invalidBlocks));
  memcpy(invalidLinks, convertedLinks, sizeof(invalidLinks));
  invalidLinks[0] = LogicV2InputLink::open();
  expect("SET/RESET rechaza entrada S abierta",
         LogicVariableInputPrototype::validate(
             invalidProgram,
             100,
             512,
             8,
             8) == LogicV2PrototypeError::OpenInputNotAllowed);

  memcpy(invalidLinks, convertedLinks, sizeof(invalidLinks));
  invalidLinks[1] = LogicV2InputLink::open();
  expect("SET/RESET rechaza entrada R abierta",
         LogicVariableInputPrototype::validate(
             invalidProgram,
             100,
             512,
             8,
             8) == LogicV2PrototypeError::OpenInputNotAllowed);

  memcpy(invalidBlocks, convertedBlocks, sizeof(invalidBlocks));
  memcpy(invalidLinks, convertedLinks, sizeof(invalidLinks));
  invalidBlocks[2].flags = JWPLC_LOGIC_BLOCK_FLAG_RETENTIVE;
  expect("SET/RESET retentivo v2 sigue pendiente",
         LogicVariableInputPrototype::validate(
             invalidProgram,
             100,
             512,
             8,
             8) == LogicV2PrototypeError::InvalidBlockFlags);

  LogicBlockDefinition invalidV1Blocks[BLOCK_COUNT];
  memcpy(invalidV1Blocks, V1_BLOCKS, sizeof(invalidV1Blocks));
  LogicProgram invalidV1Program = {
      "SETRESET INVALIDO",
      invalidV1Blocks,
      BLOCK_COUNT};

  invalidV1Blocks[2].flags = JWPLC_LOGIC_BLOCK_FLAG_RETENTIVE;
  expect("adaptador rechaza SET/RESET retentivo por ahora",
         !LogicV1ToV2Adapter::requiredLinkCount(
             invalidV1Program,
             requiredLinks,
             adapterError));
  expect("retentivo pendiente informa UNSUPPORTED_FLAGS",
         adapterError == LogicV1ToV2AdapterError::UnsupportedFlags);

  memcpy(invalidV1Blocks, V1_BLOCKS, sizeof(invalidV1Blocks));
  invalidV1Blocks[2].sourceB = JWPLC_LOGIC_NO_SOURCE;
  expect("adaptador rechaza RESET sin fuente",
         !LogicV1ToV2Adapter::requiredLinkCount(
             invalidV1Program,
             requiredLinks,
             adapterError));
  expect("RESET sin fuente informa MISSING_SOURCE",
         adapterError == LogicV1ToV2AdapterError::MissingSource);

  invalidV1Blocks[2].sourceB = 2;
  expect("adaptador rechaza RESET con fuente propia",
         !LogicV1ToV2Adapter::requiredLinkCount(
             invalidV1Program,
             requiredLinks,
             adapterError));
  expect("fuente propia informa SOURCE_NOT_PREVIOUS",
         adapterError == LogicV1ToV2AdapterError::SourceNotPrevious);

  Serial.println();
  Serial.print("Resultado: ");
  Serial.print(passedTests);
  Serial.print(" PASS, ");
  Serial.print(failedTests);
  Serial.println(" FAIL");
  Serial.println(failedTests == 0
                     ? "SETRESET V1 A V2 EN RAM: PASS"
                     : "SETRESET V1 A V2 EN RAM: FAIL");
}

void loop()
{
}