#include <JWPLC_GlobalPeripherals.h>
#include <JWPLC_LogicRuntime.h>
#include <LogicV1ToV2Adapter.h>

#include <cstring>

static constexpr uint16_t BLOCK_COUNT = 14;
static constexpr uint16_t EXPECTED_LINK_COUNT = 10;

static LogicV2EnginePrototype engine;
static uint16_t passedTests = 0;
static uint16_t failedTests = 0;

static const LogicBlockDefinition V1_BLOCKS[] = {
    {LogicBlockType::DigitalInput, JWPLC_LOGIC_NO_SOURCE, JWPLC_LOGIC_NO_SOURCE, 0, 0},
    {LogicBlockType::DigitalInput, JWPLC_LOGIC_NO_SOURCE, JWPLC_LOGIC_NO_SOURCE, 1, 0},
    {LogicBlockType::DigitalInput, JWPLC_LOGIC_NO_SOURCE, JWPLC_LOGIC_NO_SOURCE, 2, 0},
    {LogicBlockType::DigitalInput, JWPLC_LOGIC_NO_SOURCE, JWPLC_LOGIC_NO_SOURCE, 3, 0},
    {LogicBlockType::DigitalInput, JWPLC_LOGIC_NO_SOURCE, JWPLC_LOGIC_NO_SOURCE, 4, 0},
    {LogicBlockType::DigitalInput, JWPLC_LOGIC_NO_SOURCE, JWPLC_LOGIC_NO_SOURCE, 5, 0},
    {LogicBlockType::DigitalInput, JWPLC_LOGIC_NO_SOURCE, JWPLC_LOGIC_NO_SOURCE, 6, 0},
    {LogicBlockType::DigitalInput, JWPLC_LOGIC_NO_SOURCE, JWPLC_LOGIC_NO_SOURCE, 7, 0},
    {LogicBlockType::Not, 0},
    {LogicBlockType::And, 0, 1},
    {LogicBlockType::Or, 8, 9},
    {LogicBlockType::And, 2, 3},
    {LogicBlockType::Or, 10, 11},
    {LogicBlockType::Not, 12}};

static const LogicProgram V1_PROGRAM = {
    "REGRESION V1 V2",
    V1_BLOCKS,
    BLOCK_COUNT};

static void expect(const char *name, bool condition)
{
  Serial.print(condition ? "[PASS] " : "[FAIL] ");
  Serial.println(name);
  condition ? ++passedTests : ++failedTests;
}

static bool evaluateV1Reference(const bool *inputs,
                                bool *values)
{
  memset(values, 0, BLOCK_COUNT * sizeof(bool));
  for (uint16_t index = 0; index < BLOCK_COUNT; ++index)
  {
    const LogicBlockDefinition &block = V1_BLOCKS[index];
    switch (block.type)
    {
    case LogicBlockType::DigitalInput:
      values[index] = inputs[block.resource];
      break;
    case LogicBlockType::Not:
      values[index] = !values[block.sourceA];
      break;
    case LogicBlockType::And:
      values[index] = values[block.sourceA] && values[block.sourceB];
      break;
    case LogicBlockType::Or:
      values[index] = values[block.sourceA] || values[block.sourceB];
      break;
    default:
      return false;
    }
  }
  return true;
}

static bool allValuesMatch(const bool *reference)
{
  for (uint16_t index = 0; index < BLOCK_COUNT; ++index)
  {
    if (reference[index] != engine.blockValue(index))
    {
      return false;
    }
  }
  return true;
}

static void testPattern(const char *label,
                        const bool *inputs,
                        bool expectedFinal)
{
  bool reference[BLOCK_COUNT] = {};
  char message[80];

  snprintf(message, sizeof(message), "%s: referencia v1 evalua", label);
  expect(message, evaluateV1Reference(inputs, reference));
  snprintf(message, sizeof(message), "%s: motor v2 ejecuta scan", label);
  expect(message, engine.scan(inputs, 8));
  snprintf(message, sizeof(message), "%s: todos los bloques coinciden", label);
  expect(message, allValuesMatch(reference));
  snprintf(message, sizeof(message), "%s: salida final coincide", label);
  expect(message, engine.blockValue(13) == expectedFinal);
}

void setup()
{
  Serial.begin(115200);
  delay(300);

  Serial.println();
  Serial.println("JWPLC Logic Runtime - adaptador combinacional v1 a v2");
  Serial.println("No inicializa E/S, no conmuta Q0 y no usa la FRAM.");
  Serial.println();

  expect("descriptor v1 conserva 12 bytes", sizeof(LogicBlockDefinition) == 12);
  expect("descriptor v2 conserva 12 bytes", sizeof(LogicV2BlockRecord) == 12);
  expect("enlace v2 conserva 2 bytes", sizeof(LogicV2InputLink) == 2);

  size_t requiredLinks = 0;
  LogicV1ToV2AdapterError adapterError = LogicV1ToV2AdapterError::None;
  expect("adaptador calcula enlaces del programa",
         LogicV1ToV2Adapter::requiredLinkCount(V1_PROGRAM, requiredLinks, adapterError));
  expect("programa requiere exactamente 10 enlaces",
         requiredLinks == EXPECTED_LINK_COUNT);
  expect("calculo no deja error",
         adapterError == LogicV1ToV2AdapterError::None);

  LogicV2BlockRecord convertedBlocks[BLOCK_COUNT];
  LogicV2InputLink convertedLinks[EXPECTED_LINK_COUNT];
  LogicV2Program convertedProgram = {nullptr, 0, nullptr, 0};

  expect("programa v1 se convierte a v2",
         LogicV1ToV2Adapter::convert(
             V1_PROGRAM,
             convertedBlocks,
             BLOCK_COUNT,
             convertedLinks,
             EXPECTED_LINK_COUNT,
             convertedProgram,
             adapterError));
  expect("conversion conserva 14 bloques", convertedProgram.blockCount == BLOCK_COUNT);
  expect("conversion genera 10 enlaces", convertedProgram.linkCount == EXPECTED_LINK_COUNT);
  expect("entradas conservan recursos",
         convertedBlocks[0].resource == 0 && convertedBlocks[7].resource == 7);
  expect("NOT usa un enlace",
         convertedBlocks[8].type == LogicV2BlockType::Not &&
             convertedBlocks[8].firstInput == 0 &&
             convertedBlocks[8].inputCount == 1);
  expect("AND usa dos enlaces",
         convertedBlocks[9].type == LogicV2BlockType::And &&
             convertedBlocks[9].firstInput == 1 &&
             convertedBlocks[9].inputCount == 2);
  expect("OR conserva sus dos fuentes",
         convertedLinks[3].source() == 8 &&
             convertedLinks[4].source() == 9);
  expect("adaptador no introduce negaciones",
         !convertedLinks[0].inverted() &&
             !convertedLinks[9].inverted());
  expect("programa convertido supera validador v2",
         LogicVariableInputPrototype::validate(
             convertedProgram,
             100,
             512,
             8) == LogicV2PrototypeError::None);
  expect("motor v2 carga programa adaptado",
         engine.loadProgram(convertedProgram, 8));
  expect("motor v2 inicia programa adaptado", engine.start());

  const bool allFalse[8] = {};
  const bool branchPattern[8] = {
      true, false, false, true,
      false, false, false, false};
  const bool allTrue[8] = {
      true, true, true, true,
      true, true, true, true};

  testPattern("todo FALSE", allFalse, false);
  testPattern("rama final negada", branchPattern, true);
  testPattern("todo TRUE", allTrue, false);

  LogicV2Program failedDestination = convertedProgram;
  const LogicProgram nullProgram = {"NULL", nullptr, 1};
  expect("adaptador rechaza bloques nulos",
         !LogicV1ToV2Adapter::convert(
             nullProgram,
             convertedBlocks,
             BLOCK_COUNT,
             convertedLinks,
             EXPECTED_LINK_COUNT,
             failedDestination,
             adapterError));
  expect("bloques nulos informan NULL_ARGUMENT",
         adapterError == LogicV1ToV2AdapterError::NullArgument);
  expect("fallo limpia descriptor destino",
         failedDestination.blocks == nullptr &&
             failedDestination.blockCount == 0);

  const LogicProgram emptyProgram = {"EMPTY", V1_BLOCKS, 0};
  expect("adaptador rechaza programa vacio",
         !LogicV1ToV2Adapter::requiredLinkCount(
             emptyProgram,
             requiredLinks,
             adapterError));
  expect("programa vacio informa EMPTY_PROGRAM",
         adapterError == LogicV1ToV2AdapterError::EmptyProgram);

  expect("adaptador rechaza buffer de bloques corto",
         !LogicV1ToV2Adapter::convert(
             V1_PROGRAM,
             convertedBlocks,
             BLOCK_COUNT - 1,
             convertedLinks,
             EXPECTED_LINK_COUNT,
             failedDestination,
             adapterError));
  expect("buffer corto informa BLOCK_BUFFER_TOO_SMALL",
         adapterError == LogicV1ToV2AdapterError::BlockBufferTooSmall);

  expect("adaptador rechaza buffer de enlaces corto",
         !LogicV1ToV2Adapter::convert(
             V1_PROGRAM,
             convertedBlocks,
             BLOCK_COUNT,
             convertedLinks,
             EXPECTED_LINK_COUNT - 1,
             failedDestination,
             adapterError));
  expect("enlaces cortos informan LINK_BUFFER_TOO_SMALL",
         adapterError == LogicV1ToV2AdapterError::LinkBufferTooSmall);

  LogicBlockDefinition outputBlocks[2] = {
      {LogicBlockType::DigitalInput,
       JWPLC_LOGIC_NO_SOURCE,
       JWPLC_LOGIC_NO_SOURCE,
       0,
       0},
      {LogicBlockType::DigitalOutput,
       0,
       JWPLC_LOGIC_NO_SOURCE,
       0,
       0}};
  const LogicProgram outputProgram = {"OUTPUT", outputBlocks, 2};
  expect("DigitalOutput ya es aceptado por el adaptador",
         LogicV1ToV2Adapter::requiredLinkCount(
             outputProgram,
             requiredLinks,
             adapterError));
  expect("DigitalOutput requiere un enlace",
         requiredLinks == 1 &&
             adapterError == LogicV1ToV2AdapterError::None);

  outputBlocks[1] = LogicBlockDefinition(LogicBlockType::SetReset, 0, 0);
  expect("SET/RESET queda pendiente de fase posterior",
         !LogicV1ToV2Adapter::requiredLinkCount(
             outputProgram,
             requiredLinks,
             adapterError));

  outputBlocks[1] =
      LogicBlockDefinition(LogicBlockType::Ton,
                           0,
                           JWPLC_LOGIC_NO_SOURCE,
                           0,
                           1000);
  expect("TON queda pendiente de fase posterior",
         !LogicV1ToV2Adapter::requiredLinkCount(
             outputProgram,
             requiredLinks,
             adapterError));

  outputBlocks[0] =
      LogicBlockDefinition(LogicBlockType::DigitalInput,
                           JWPLC_LOGIC_NO_SOURCE,
                           JWPLC_LOGIC_NO_SOURCE,
                           0,
                           0,
                           JWPLC_LOGIC_BLOCK_FLAG_RETENTIVE);
  const LogicProgram flagProgram = {"FLAGS", outputBlocks, 1};
  expect("adaptador rechaza flags no soportados",
         !LogicV1ToV2Adapter::requiredLinkCount(
             flagProgram,
             requiredLinks,
             adapterError));
  expect("flags informan UNSUPPORTED_FLAGS",
         adapterError == LogicV1ToV2AdapterError::UnsupportedFlags);

  LogicBlockDefinition invalidSourceBlocks[2] = {
      {LogicBlockType::DigitalInput,
       JWPLC_LOGIC_NO_SOURCE,
       JWPLC_LOGIC_NO_SOURCE,
       0,
       0},
      {LogicBlockType::Not, JWPLC_LOGIC_NO_SOURCE}};
  const LogicProgram invalidSourceProgram = {
      "INVALID SOURCE",
      invalidSourceBlocks,
      2};

  expect("adaptador rechaza fuente ausente",
         !LogicV1ToV2Adapter::requiredLinkCount(
             invalidSourceProgram,
             requiredLinks,
             adapterError));
  expect("fuente ausente informa MISSING_SOURCE",
         adapterError == LogicV1ToV2AdapterError::MissingSource);

  invalidSourceBlocks[1].sourceA = 1;
  expect("adaptador rechaza fuente propia",
         !LogicV1ToV2Adapter::requiredLinkCount(
             invalidSourceProgram,
             requiredLinks,
             adapterError));
  expect("fuente propia informa SOURCE_NOT_PREVIOUS",
         adapterError == LogicV1ToV2AdapterError::SourceNotPrevious);

  engine.stop();
  expect("motor puede detener programa adaptado",
         engine.state() == LogicV2EngineState::Stopped);

  Serial.println();
  Serial.print("Resultado: ");
  Serial.print(passedTests);
  Serial.print(" PASS, ");
  Serial.print(failedTests);
  Serial.println(" FAIL");
  Serial.println(failedTests == 0
                     ? "ADAPTADOR COMBINACIONAL V1 A V2: PASS"
                     : "ADAPTADOR COMBINACIONAL V1 A V2: FAIL");
}

void loop()
{
}
