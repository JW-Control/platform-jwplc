#include <JWPLC_GlobalPeripherals.h>
#include <JWPLC_LogicRuntime.h>
#include <LogicV1ToV2Adapter.h>

#include <cstring>

static constexpr uint16_t BLOCK_COUNT = 8;
static constexpr uint16_t LINK_COUNT = 8;

static LogicV2EnginePrototype engine;
static uint16_t passedTests = 0;
static uint16_t failedTests = 0;

static const LogicBlockDefinition V1_BLOCKS[] = {
    {LogicBlockType::DigitalInput, JWPLC_LOGIC_NO_SOURCE, JWPLC_LOGIC_NO_SOURCE, 0, 0},
    {LogicBlockType::DigitalInput, JWPLC_LOGIC_NO_SOURCE, JWPLC_LOGIC_NO_SOURCE, 1, 0},
    {LogicBlockType::And, 0, 1},
    {LogicBlockType::Not, 0},
    {LogicBlockType::Or, 2, 3},
    {LogicBlockType::DigitalOutput, 2, JWPLC_LOGIC_NO_SOURCE, 0, 0},
    {LogicBlockType::DigitalOutput, 3, JWPLC_LOGIC_NO_SOURCE, 1, 0},
    {LogicBlockType::DigitalOutput, 4, JWPLC_LOGIC_NO_SOURCE, 2, 0}};

static const LogicProgram V1_PROGRAM = {
    "REGRESION SALIDAS V1 V2",
    V1_BLOCKS,
    BLOCK_COUNT};

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

static bool evaluateV1Reference(const bool inputs[8],
                                bool values[BLOCK_COUNT])
{
  memset(values, 0, sizeof(bool) * BLOCK_COUNT);

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
    case LogicBlockType::DigitalOutput:
      values[index] = values[block.sourceA];
      break;
    default:
      return false;
    }
  }

  return true;
}

static bool allValuesMatch(const bool reference[BLOCK_COUNT])
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
                        const bool inputs[8],
                        bool q0,
                        bool q1,
                        bool q2)
{
  bool reference[BLOCK_COUNT] = {};
  char message[88];

  snprintf(message, sizeof(message), "%s: referencia v1 evalua", label);
  expect(message, evaluateV1Reference(inputs, reference));

  snprintf(message, sizeof(message), "%s: motor v2 ejecuta scan", label);
  expect(message, engine.scan(inputs, 8));

  snprintf(message, sizeof(message), "%s: bloques coinciden", label);
  expect(message, allValuesMatch(reference));

  snprintf(message, sizeof(message), "%s: Q0 logica coincide", label);
  expect(message, engine.digitalOutputValue(0) == q0);

  snprintf(message, sizeof(message), "%s: Q1 logica coincide", label);
  expect(message, engine.digitalOutputValue(1) == q1);

  snprintf(message, sizeof(message), "%s: Q2 logica coincide", label);
  expect(message, engine.digitalOutputValue(2) == q2);
}

void setup()
{
  Serial.begin(115200);
  delay(300);

  Serial.println();
  Serial.println("JWPLC Logic Runtime - DigitalOutput v1 a v2 en RAM");
  Serial.println("No inicializa E/S, no conmuta Q0 y no usa la FRAM.");
  Serial.println();

  expect("DigitalOutput v2 conserva descriptor de 12 bytes",
         sizeof(LogicV2BlockRecord) == 12);
  expect("DigitalOutput se agrega sin cambiar enlace de 2 bytes",
         sizeof(LogicV2InputLink) == 2);

  size_t requiredLinks = 0;
  LogicV1ToV2AdapterError adapterError =
      LogicV1ToV2AdapterError::None;

  expect("adaptador acepta programa con DigitalOutput",
         LogicV1ToV2Adapter::requiredLinkCount(
             V1_PROGRAM,
             requiredLinks,
             adapterError));
  expect("programa requiere ocho enlaces", requiredLinks == LINK_COUNT);
  expect("calculo de enlaces no deja error",
         adapterError == LogicV1ToV2AdapterError::None);

  LogicV2BlockRecord convertedBlocks[BLOCK_COUNT];
  LogicV2InputLink convertedLinks[LINK_COUNT];
  LogicV2Program convertedProgram = {nullptr, 0, nullptr, 0};

  expect("adaptador convierte DigitalOutput",
         LogicV1ToV2Adapter::convert(
             V1_PROGRAM,
             convertedBlocks,
             BLOCK_COUNT,
             convertedLinks,
             LINK_COUNT,
             convertedProgram,
             adapterError));
  expect("conversion conserva ocho bloques",
         convertedProgram.blockCount == BLOCK_COUNT);
  expect("conversion conserva ocho enlaces",
         convertedProgram.linkCount == LINK_COUNT);
  expect("Q0 se convierte como DigitalOutput",
         convertedBlocks[5].type == LogicV2BlockType::DigitalOutput);
  expect("Q0 conserva recurso cero",
         convertedBlocks[5].resource == 0);
  expect("Q1 conserva recurso uno",
         convertedBlocks[6].resource == 1);
  expect("Q2 conserva recurso dos",
         convertedBlocks[7].resource == 2);
  expect("cada salida usa exactamente una entrada",
         convertedBlocks[5].inputCount == 1 &&
             convertedBlocks[6].inputCount == 1 &&
             convertedBlocks[7].inputCount == 1);
  expect("salidas conservan sus fuentes",
         convertedLinks[5].source() == 2 &&
             convertedLinks[6].source() == 3 &&
             convertedLinks[7].source() == 4);
  expect("adaptador no niega conexiones de salida",
         !convertedLinks[5].inverted() &&
             !convertedLinks[6].inverted() &&
             !convertedLinks[7].inverted());

  expect("validador v2 acepta recursos Q0.0 a Q0.2",
         LogicVariableInputPrototype::validate(
             convertedProgram,
             JWPLC_LOGIC_V2_COMPILED_MAX_BLOCKS,
             JWPLC_LOGIC_V2_COMPILED_MAX_LINKS,
             8,
             8) == LogicV2PrototypeError::None);

  expect("motor carga programa con perfil de ocho salidas",
         engine.loadProgram(convertedProgram, 8, 8));
  expect("motor conserva perfil de ocho salidas",
         engine.digitalOutputCount() == 8);
  expect("motor inicia programa con salidas logicas", engine.start());

  const bool allFalse[8] = {};
  const bool onlyI0[8] = {
      true, false, false, false,
      false, false, false, false};
  const bool i0i1[8] = {
      true, true, false, false,
      false, false, false, false};

  testPattern("todo FALSE", allFalse, false, true, true);
  testPattern("solo I0 TRUE", onlyI0, false, false, false);
  testPattern("I0 e I1 TRUE", i0i1, true, false, true);

  expect("salida sin bloque asociado consulta FALSE",
         !engine.digitalOutputValue(7));
  expect("salida fuera del perfil consulta FALSE",
         !engine.digitalOutputValue(8));

  LogicV2BlockRecord invalidBlocks[BLOCK_COUNT];
  LogicV2InputLink invalidLinks[LINK_COUNT];
  memcpy(invalidBlocks, convertedBlocks, sizeof(invalidBlocks));
  memcpy(invalidLinks, convertedLinks, sizeof(invalidLinks));
  LogicV2Program invalidProgram = {
      invalidBlocks,
      BLOCK_COUNT,
      invalidLinks,
      LINK_COUNT};

  invalidBlocks[5].resource = 8;
  expect("validador rechaza recurso de salida fuera del perfil",
         LogicVariableInputPrototype::validate(
             invalidProgram,
             100,
             512,
             8,
             8) == LogicV2PrototypeError::ResourceOutOfRange);

  memcpy(invalidBlocks, convertedBlocks, sizeof(invalidBlocks));
  invalidBlocks[6].resource = 0;
  expect("validador rechaza salida duplicada",
         LogicVariableInputPrototype::validate(
             invalidProgram,
             100,
             512,
             8,
             8) == LogicV2PrototypeError::DuplicateDigitalOutput);

  memcpy(invalidBlocks, convertedBlocks, sizeof(invalidBlocks));
  invalidLinks[5] = LogicV2InputLink::open();
  expect("DigitalOutput rechaza entrada abierta",
         LogicVariableInputPrototype::validate(
             invalidProgram,
             100,
             512,
             8,
             8) == LogicV2PrototypeError::OpenInputNotAllowed);

  memcpy(invalidLinks, convertedLinks, sizeof(invalidLinks));
  invalidLinks[5] = LogicV2InputLink::block(2, true);
  expect("validador permite negacion explicita en pin de salida",
         LogicVariableInputPrototype::validate(
             invalidProgram,
             100,
             512,
             8,
             8) == LogicV2PrototypeError::None);

  LogicV2EnginePrototype invertedEngine;
  expect("motor carga salida con pin negado",
         invertedEngine.loadProgram(invalidProgram, 8, 8));
  expect("motor inicia salida con pin negado",
         invertedEngine.start());
  expect("motor evalua salida negada",
         invertedEngine.scan(i0i1, 8));
  expect("Q0 negada invierte la fuente TRUE",
         !invertedEngine.digitalOutputValue(0));

  LogicBlockDefinition missingSourceBlocks[2] = {
      {LogicBlockType::DigitalInput,
       JWPLC_LOGIC_NO_SOURCE,
       JWPLC_LOGIC_NO_SOURCE,
       0,
       0},
      {LogicBlockType::DigitalOutput,
       JWPLC_LOGIC_NO_SOURCE,
       JWPLC_LOGIC_NO_SOURCE,
       0,
       0}};
  const LogicProgram missingSourceProgram = {
      "OUTPUT SIN FUENTE",
      missingSourceBlocks,
      2};

  expect("adaptador rechaza salida sin fuente",
         !LogicV1ToV2Adapter::requiredLinkCount(
             missingSourceProgram,
             requiredLinks,
             adapterError));
  expect("salida sin fuente informa MISSING_SOURCE",
         adapterError == LogicV1ToV2AdapterError::MissingSource);

  missingSourceBlocks[1].sourceA = 1;
  expect("adaptador rechaza salida con fuente propia",
         !LogicV1ToV2Adapter::requiredLinkCount(
             missingSourceProgram,
             requiredLinks,
             adapterError));
  expect("fuente propia informa SOURCE_NOT_PREVIOUS",
         adapterError == LogicV1ToV2AdapterError::SourceNotPrevious);

  engine.stop();
  expect("stop deja motor detenido",
         engine.state() == LogicV2EngineState::Stopped);
  expect("stop limpia valores de salida",
         !engine.digitalOutputValue(0) &&
             !engine.digitalOutputValue(1) &&
             !engine.digitalOutputValue(2));

  Serial.println();
  Serial.print("Resultado: ");
  Serial.print(passedTests);
  Serial.print(" PASS, ");
  Serial.print(failedTests);
  Serial.println(" FAIL");
  Serial.println(failedTests == 0
                     ? "DIGITALOUTPUT V1 A V2 EN RAM: PASS"
                     : "DIGITALOUTPUT V1 A V2 EN RAM: FAIL");
}

void loop()
{
}
