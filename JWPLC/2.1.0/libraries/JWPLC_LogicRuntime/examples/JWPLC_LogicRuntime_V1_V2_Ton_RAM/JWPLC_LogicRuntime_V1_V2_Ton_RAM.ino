#include <JWPLC_GlobalPeripherals.h>
#include <JWPLC_LogicRuntime.h>
#include <LogicV1ToV2Adapter.h>

#include <cstring>

static constexpr uint16_t BLOCK_COUNT = 4;
static constexpr uint16_t LINK_COUNT = 3;
static constexpr uint16_t TON_INDEX = 1;
static constexpr uint32_t TON_DELAY_MS = 1000;
static constexpr size_t MIN_ENGINE_STORAGE_BYTES =
    (JWPLC_LOGIC_V2_COMPILED_MAX_BLOCKS * sizeof(LogicV2BlockRecord)) +
    (JWPLC_LOGIC_V2_COMPILED_MAX_LINKS * sizeof(LogicV2InputLink)) +
    (JWPLC_LOGIC_V2_COMPILED_MAX_BLOCKS * sizeof(bool)) +
    (JWPLC_LOGIC_V2_COMPILED_MAX_BLOCKS * sizeof(bool)) +
    (JWPLC_LOGIC_V2_COMPILED_MAX_BLOCKS * sizeof(uint32_t));

static LogicV2EnginePrototype engine;
static uint16_t passedTests = 0;
static uint16_t failedTests = 0;

struct ReferenceTonState
{
  bool values[BLOCK_COUNT];
  bool timing;
  uint32_t startedAtMs;
};

static ReferenceTonState referenceState = {};

static const LogicBlockDefinition V1_BLOCKS[] = {
    {LogicBlockType::DigitalInput,
     JWPLC_LOGIC_NO_SOURCE,
     JWPLC_LOGIC_NO_SOURCE,
     0,
     0},
    {LogicBlockType::Ton,
     0,
     JWPLC_LOGIC_NO_SOURCE,
     0,
     TON_DELAY_MS},
    {LogicBlockType::Not, 1},
    {LogicBlockType::DigitalOutput,
     1,
     JWPLC_LOGIC_NO_SOURCE,
     0,
     0}};

static const LogicProgram V1_PROGRAM = {
    "TON V1 V2",
    V1_BLOCKS,
    BLOCK_COUNT};

static void expect(const char *name, bool condition)
{
  Serial.print(condition ? "[PASS] " : "[FAIL] ");
  Serial.println(name);
  condition ? ++passedTests : ++failedTests;
}

static void resetReference()
{
  memset(&referenceState, 0, sizeof(referenceState));
}

static bool evaluateV1Reference(bool inputValue, uint32_t nowMs)
{
  for (uint16_t index = 0; index < BLOCK_COUNT; ++index)
  {
    const LogicBlockDefinition &block = V1_BLOCKS[index];

    switch (block.type)
    {
    case LogicBlockType::DigitalInput:
      referenceState.values[index] = inputValue;
      break;

    case LogicBlockType::Ton:
    {
      const bool input = referenceState.values[block.sourceA];
      if (!input)
      {
        referenceState.values[index] = false;
        referenceState.timing = false;
        referenceState.startedAtMs = 0;
      }
      else if (!referenceState.values[index])
      {
        if (!referenceState.timing)
        {
          referenceState.timing = true;
          referenceState.startedAtMs = nowMs;
        }

        if (static_cast<uint32_t>(
                nowMs - referenceState.startedAtMs) >= block.parameter)
        {
          referenceState.values[index] = true;
          referenceState.timing = false;
        }
      }
      break;
    }

    case LogicBlockType::Not:
      referenceState.values[index] =
          !referenceState.values[block.sourceA];
      break;

    case LogicBlockType::DigitalOutput:
      referenceState.values[index] =
          referenceState.values[block.sourceA];
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
    if (referenceState.values[index] != engine.blockValue(index))
    {
      return false;
    }
  }
  return true;
}

static void testStep(const char *label,
                     bool inputValue,
                     uint32_t nowMs,
                     bool expectedTon,
                     bool expectedTiming,
                     uint32_t expectedElapsedMs,
                     uint32_t expectedRemainingMs)
{
  bool inputs[8] = {};
  inputs[0] = inputValue;
  char message[96];

  snprintf(message, sizeof(message), "%s: referencia v1 evalua", label);
  expect(message, evaluateV1Reference(inputValue, nowMs));

  snprintf(message, sizeof(message), "%s: motor v2 ejecuta scan", label);
  expect(message, engine.scan(inputs, 8, nowMs));

  snprintf(message, sizeof(message), "%s: bloques coinciden", label);
  expect(message, allValuesMatch());

  snprintf(message, sizeof(message), "%s: salida TON coincide", label);
  expect(message, engine.blockValue(TON_INDEX) == expectedTon);

  snprintf(message, sizeof(message), "%s: Q0 logica coincide", label);
  expect(message, engine.digitalOutputValue(0) == expectedTon);

  snprintf(message, sizeof(message), "%s: estado timing coincide", label);
  expect(message, engine.tonTiming(TON_INDEX) == expectedTiming);

  snprintf(message, sizeof(message), "%s: tiempo transcurrido coincide", label);
  expect(message,
         engine.tonElapsedMs(TON_INDEX, nowMs) == expectedElapsedMs);

  snprintf(message, sizeof(message), "%s: tiempo restante coincide", label);
  expect(message,
         engine.tonRemainingMs(TON_INDEX, nowMs) == expectedRemainingMs);
}

void setup()
{
  Serial.begin(115200);
  delay(300);

  Serial.println();
  Serial.println("JWPLC Logic Runtime - TON v1 a v2 en RAM");
  Serial.println("Tiempo explicito por scan; no conmuta Q0 ni usa FRAM.");
  Serial.println();

  expect("TON v2 conserva descriptor de 12 bytes",
         sizeof(LogicV2BlockRecord) == 12);
  expect("TON usa enlaces de 2 bytes",
         sizeof(LogicV2InputLink) == 2);
  expect("motor incluye estados temporales requeridos",
         sizeof(LogicV2EnginePrototype) >= MIN_ENGINE_STORAGE_BYTES);
  expect("overhead del motor permanece menor o igual a 64 bytes",
         sizeof(LogicV2EnginePrototype) <= MIN_ENGINE_STORAGE_BYTES + 64U);

  size_t requiredLinks = 0;
  LogicV1ToV2AdapterError adapterError =
      LogicV1ToV2AdapterError::None;

  expect("adaptador calcula enlaces con TON",
         LogicV1ToV2Adapter::requiredLinkCount(
             V1_PROGRAM,
             requiredLinks,
             adapterError));
  expect("programa requiere tres enlaces",
         requiredLinks == LINK_COUNT);
  expect("calculo de enlaces no deja error",
         adapterError == LogicV1ToV2AdapterError::None);

  LogicV2BlockRecord convertedBlocks[BLOCK_COUNT];
  LogicV2InputLink convertedLinks[LINK_COUNT];
  LogicV2Program convertedProgram = {nullptr, 0, nullptr, 0};

  expect("adaptador convierte TON",
         LogicV1ToV2Adapter::convert(
             V1_PROGRAM,
             convertedBlocks,
             BLOCK_COUNT,
             convertedLinks,
             LINK_COUNT,
             convertedProgram,
             adapterError));
  expect("conversion conserva cuatro bloques",
         convertedProgram.blockCount == BLOCK_COUNT);
  expect("conversion genera tres enlaces",
         convertedProgram.linkCount == LINK_COUNT);
  expect("B01 se convierte como TON",
         convertedBlocks[TON_INDEX].type == LogicV2BlockType::Ton);
  expect("TON conserva parametro de 1000 ms",
         convertedBlocks[TON_INDEX].parameter == TON_DELAY_MS);
  expect("TON conserva B00 como disparo",
         convertedLinks[0].source() == 0 &&
             !convertedLinks[0].inverted());
  expect("DigitalOutput conserva la salida del TON",
         convertedLinks[2].source() == TON_INDEX);

  expect("validador v2 acepta TON",
         LogicVariableInputPrototype::validate(
             convertedProgram,
             100,
             512,
             8,
             8) == LogicV2PrototypeError::None);

  bool statelessInputs[8] = {};
  bool statelessValues[BLOCK_COUNT] = {};
  LogicV2PrototypeError prototypeError =
      LogicV2PrototypeError::None;
  expect("evaluador sin estado temporal rechaza TON",
         !LogicVariableInputPrototype::evaluateValidated(
             convertedProgram,
             statelessInputs,
             8,
             statelessValues,
             BLOCK_COUNT,
             prototypeError));
  expect("TON sin buffers informa TIMER_STATE_REQUIRED",
         prototypeError == LogicV2PrototypeError::TimerStateRequired);

  expect("motor carga programa con TON",
         engine.loadProgram(convertedProgram, 8, 8));
  expect("motor inicia programa con TON",
         engine.start());

  const bool allFalse[8] = {};
  expect("scan sin nowMs se rechaza cuando existe TON",
         !engine.scan(allFalse, 8));
  expect("scan sin tiempo informa TIME_REQUIRED",
         engine.lastError() == LogicV2EngineError::TimeRequired);
  expect("consulta timing de bloque no TON devuelve FALSE",
         !engine.tonTiming(0));
  expect("consulta elapsed de bloque no TON devuelve cero",
         engine.tonElapsedMs(0, 0) == 0);
  expect("consulta TON fuera de rango devuelve cero",
         engine.tonRemainingMs(BLOCK_COUNT, 0) == 0);

  testStep("entrada baja inicial",
           false, 100, false, false, 0, 1000);
  testStep("flanco de subida",
           true, 1000, false, true, 0, 1000);
  testStep("cuenta 499 ms",
           true, 1499, false, true, 499, 501);
  testStep("cuenta 999 ms",
           true, 1999, false, true, 999, 1);
  testStep("alcanza 1000 ms",
           true, 2000, true, false, 1000, 0);
  testStep("mantiene salida alta",
           true, 2500, true, false, 1000, 0);
  testStep("caida cancela y limpia",
           false, 2600, false, false, 0, 1000);
  testStep("segundo inicio",
           true, 3000, false, true, 0, 1000);
  testStep("cancelacion antes del tiempo",
           false, 3400, false, false, 0, 1000);

  engine.stop();
  expect("stop deja motor detenido",
         engine.state() == LogicV2EngineState::Stopped);
  expect("stop limpia salida TON",
         !engine.blockValue(TON_INDEX));
  expect("stop limpia estado timing",
         !engine.tonTiming(TON_INDEX));
  expect("stop limpia tiempo transcurrido",
         engine.tonElapsedMs(TON_INDEX, 3400) == 0);
  expect("motor reinicia despues de stop",
         engine.start());

  resetReference();
  testStep("rollover inicia cerca de UINT32_MAX",
           true, 0xFFFFFF00UL, false, true, 0, 1000);
  testStep("rollover conserva 999 ms",
           true, 0x000002E7UL, false, true, 999, 1);
  testStep("rollover alcanza 1000 ms",
           true, 0x000002E8UL, true, false, 1000, 0);

  LogicV2BlockRecord invalidBlocks[BLOCK_COUNT];
  LogicV2InputLink invalidLinks[LINK_COUNT];
  memcpy(invalidBlocks, convertedBlocks, sizeof(invalidBlocks));
  memcpy(invalidLinks, convertedLinks, sizeof(invalidLinks));
  LogicV2Program invalidProgram = {
      invalidBlocks,
      BLOCK_COUNT,
      invalidLinks,
      LINK_COUNT};

  invalidBlocks[TON_INDEX].inputCount = 0;
  expect("TON rechaza cero entradas",
         LogicVariableInputPrototype::validate(
             invalidProgram, 100, 512, 8, 8) ==
             LogicV2PrototypeError::InvalidInputCount);

  memcpy(invalidBlocks, convertedBlocks, sizeof(invalidBlocks));
  invalidBlocks[TON_INDEX].inputCount = 2;
  expect("TON rechaza dos entradas",
         LogicVariableInputPrototype::validate(
             invalidProgram, 100, 512, 8, 8) ==
             LogicV2PrototypeError::InvalidInputCount);

  memcpy(invalidBlocks, convertedBlocks, sizeof(invalidBlocks));
  memcpy(invalidLinks, convertedLinks, sizeof(invalidLinks));
  invalidLinks[0] = LogicV2InputLink::open();
  expect("TON rechaza entrada abierta",
         LogicVariableInputPrototype::validate(
             invalidProgram, 100, 512, 8, 8) ==
             LogicV2PrototypeError::OpenInputNotAllowed);

  memcpy(invalidBlocks, convertedBlocks, sizeof(invalidBlocks));
  memcpy(invalidLinks, convertedLinks, sizeof(invalidLinks));
  invalidBlocks[TON_INDEX].flags = JWPLC_LOGIC_BLOCK_FLAG_RETENTIVE;
  expect("TON retentivo sigue rechazado",
         LogicVariableInputPrototype::validate(
             invalidProgram, 100, 512, 8, 8) ==
             LogicV2PrototypeError::InvalidBlockFlags);

  LogicBlockDefinition invalidV1Blocks[BLOCK_COUNT];
  memcpy(invalidV1Blocks, V1_BLOCKS, sizeof(invalidV1Blocks));
  LogicProgram invalidV1Program = {
      "TON INVALIDO",
      invalidV1Blocks,
      BLOCK_COUNT};

  invalidV1Blocks[TON_INDEX].sourceA = JWPLC_LOGIC_NO_SOURCE;
  expect("adaptador rechaza TON sin disparo",
         !LogicV1ToV2Adapter::requiredLinkCount(
             invalidV1Program,
             requiredLinks,
             adapterError));
  expect("TON sin disparo informa MISSING_SOURCE",
         adapterError == LogicV1ToV2AdapterError::MissingSource);

  invalidV1Blocks[TON_INDEX].sourceA = TON_INDEX;
  expect("adaptador rechaza TON con fuente propia",
         !LogicV1ToV2Adapter::requiredLinkCount(
             invalidV1Program,
             requiredLinks,
             adapterError));
  expect("fuente propia informa SOURCE_NOT_PREVIOUS",
         adapterError == LogicV1ToV2AdapterError::SourceNotPrevious);

  const LogicV2BlockRecord zeroBlocks[] = {
      {LogicV2BlockType::DigitalInput, 0, 0, 0},
      {LogicV2BlockType::Ton, 0, 1, 0, 0},
      {LogicV2BlockType::DigitalOutput, 1, 1, 0}};
  const LogicV2InputLink zeroLinks[] = {
      LogicV2InputLink::block(0),
      LogicV2InputLink::block(1)};
  const LogicV2Program zeroProgram = {
      zeroBlocks, 3, zeroLinks, 2};

  LogicV2EnginePrototype zeroEngine;
  expect("TON de cero ms supera validador",
         LogicVariableInputPrototype::validate(
             zeroProgram, 100, 512, 8, 8) ==
             LogicV2PrototypeError::None);
  expect("motor carga TON de cero ms",
         zeroEngine.loadProgram(zeroProgram, 8, 8));
  expect("motor inicia TON de cero ms",
         zeroEngine.start());

  bool highInputs[8] = {};
  highInputs[0] = true;
  expect("TON de cero ms ejecuta scan",
         zeroEngine.scan(highInputs, 8, 777));
  expect("TON de cero ms activa en el mismo scan",
         zeroEngine.blockValue(1));
  expect("TON de cero ms no queda temporizando",
         !zeroEngine.tonTiming(1));

  const LogicV2BlockRecord invertedBlocks[] = {
      {LogicV2BlockType::DigitalInput, 0, 0, 0},
      {LogicV2BlockType::Ton, 0, 1, 0, 100}};
  const LogicV2InputLink invertedLinks[] = {
      LogicV2InputLink::block(0, true)};
  const LogicV2Program invertedProgram = {
      invertedBlocks, 2, invertedLinks, 1};

  LogicV2EnginePrototype invertedEngine;
  expect("TON permite negacion individual del disparo",
         LogicVariableInputPrototype::validate(
             invertedProgram, 100, 512, 8, 0) ==
             LogicV2PrototypeError::None);
  expect("motor carga TON con disparo negado",
         invertedEngine.loadProgram(invertedProgram, 8));
  expect("motor inicia TON con disparo negado",
         invertedEngine.start());
  expect("entrada FALSE inicia TON negado",
         invertedEngine.scan(allFalse, 8, 10) &&
             invertedEngine.tonTiming(1));
  expect("TON negado alcanza su tiempo",
         invertedEngine.scan(allFalse, 8, 110) &&
             invertedEngine.blockValue(1));

  Serial.println();
  Serial.print("sizeof motor v2 con TON: ");
  Serial.print(sizeof(LogicV2EnginePrototype));
  Serial.println(" bytes");
  Serial.print("almacen minimo con estados TON: ");
  Serial.print(MIN_ENGINE_STORAGE_BYTES);
  Serial.println(" bytes");
  Serial.println();

  Serial.print("Resultado: ");
  Serial.print(passedTests);
  Serial.print(" PASS, ");
  Serial.print(failedTests);
  Serial.println(" FAIL");
  Serial.println(failedTests == 0
                     ? "TON V1 A V2 EN RAM: PASS"
                     : "TON V1 A V2 EN RAM: FAIL");
}

void loop()
{
}