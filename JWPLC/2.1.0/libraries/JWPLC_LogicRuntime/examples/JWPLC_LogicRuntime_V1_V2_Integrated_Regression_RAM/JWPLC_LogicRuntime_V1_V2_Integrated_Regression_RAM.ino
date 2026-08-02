#include <JWPLC_GlobalPeripherals.h>
#include <JWPLC_LogicRuntime.h>
#include <LogicV1ToV2Adapter.h>

#include <cstring>

static constexpr uint16_t BLOCK_COUNT = 14;
static constexpr uint16_t LINK_COUNT = 16;
static constexpr uint16_t SET_RESET_INDEX = 6;
static constexpr uint16_t TON_INDEX = 7;
static constexpr uint32_t TON_DELAY_MS = 1000;

static LogicV2EnginePrototype engine;
static uint16_t passedTests = 0;
static uint16_t failedTests = 0;

struct ReferenceState
{
  bool values[BLOCK_COUNT];
  bool tonTiming;
  uint32_t tonStartedAtMs;
};

static ReferenceState referenceState = {};

static const LogicBlockDefinition V1_BLOCKS[] = {
    // B00..B02: entradas simuladas I0.0, I0.1 e I0.2.
    {LogicBlockType::DigitalInput,
     JWPLC_LOGIC_NO_SOURCE,
     JWPLC_LOGIC_NO_SOURCE,
     0,
     0},
    {LogicBlockType::DigitalInput,
     JWPLC_LOGIC_NO_SOURCE,
     JWPLC_LOGIC_NO_SOURCE,
     1,
     0},
    {LogicBlockType::DigitalInput,
     JWPLC_LOGIC_NO_SOURCE,
     JWPLC_LOGIC_NO_SOURCE,
     2,
     0},

    // B03..B09: NOT, AND, OR, SET/RESET, TON, AND y OR.
    {LogicBlockType::Not, 2},
    {LogicBlockType::And, 0, 3},
    {LogicBlockType::Or, 4, 1},
    {LogicBlockType::SetReset, 4, 1},
    {LogicBlockType::Ton,
     6,
     JWPLC_LOGIC_NO_SOURCE,
     0,
     TON_DELAY_MS},
    {LogicBlockType::And, 7, 2},
    {LogicBlockType::Or, 8, 3},

    // B10..B13: cuatro salidas lógicas independientes.
    {LogicBlockType::DigitalOutput,
     6,
     JWPLC_LOGIC_NO_SOURCE,
     0,
     0},
    {LogicBlockType::DigitalOutput,
     7,
     JWPLC_LOGIC_NO_SOURCE,
     1,
     0},
    {LogicBlockType::DigitalOutput,
     9,
     JWPLC_LOGIC_NO_SOURCE,
     2,
     0},
    {LogicBlockType::DigitalOutput,
     5,
     JWPLC_LOGIC_NO_SOURCE,
     3,
     0}};

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

static void resetReference()
{
  memset(&referenceState, 0, sizeof(referenceState));
}

static bool sourceValue(uint16_t source)
{
  return source < BLOCK_COUNT ? referenceState.values[source] : false;
}

static bool evaluateV1Reference(const bool *inputs, uint32_t nowMs)
{
  if (inputs == nullptr)
  {
    return false;
  }

  for (uint16_t index = 0; index < BLOCK_COUNT; ++index)
  {
    const LogicBlockDefinition &block = V1_BLOCKS[index];

    switch (block.type)
    {
    case LogicBlockType::DigitalInput:
      referenceState.values[index] = inputs[block.resource];
      break;

    case LogicBlockType::DigitalOutput:
      referenceState.values[index] = sourceValue(block.sourceA);
      break;

    case LogicBlockType::Not:
      referenceState.values[index] = !sourceValue(block.sourceA);
      break;

    case LogicBlockType::And:
      referenceState.values[index] =
          sourceValue(block.sourceA) && sourceValue(block.sourceB);
      break;

    case LogicBlockType::Or:
      referenceState.values[index] =
          sourceValue(block.sourceA) || sourceValue(block.sourceB);
      break;

    case LogicBlockType::SetReset:
      if (sourceValue(block.sourceB))
      {
        referenceState.values[index] = false;
      }
      else if (sourceValue(block.sourceA))
      {
        referenceState.values[index] = true;
      }
      break;

    case LogicBlockType::Ton:
    {
      const bool input = sourceValue(block.sourceA);
      if (!input)
      {
        referenceState.values[index] = false;
        referenceState.tonTiming = false;
        referenceState.tonStartedAtMs = 0;
      }
      else if (!referenceState.values[index])
      {
        if (!referenceState.tonTiming)
        {
          referenceState.tonTiming = true;
          referenceState.tonStartedAtMs = nowMs;
        }

        const uint32_t elapsedMs = static_cast<uint32_t>(
            nowMs - referenceState.tonStartedAtMs);
        if (elapsedMs >= block.parameter)
        {
          referenceState.values[index] = true;
          referenceState.tonTiming = false;
        }
      }
      break;
    }

    default:
      return false;
    }
  }

  return true;
}

static bool allBlockValuesMatch()
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

static uint8_t referenceOutputMask()
{
  uint8_t mask = 0;
  for (uint8_t output = 0; output < 4; ++output)
  {
    if (referenceState.values[10U + output])
    {
      mask |= static_cast<uint8_t>(1U << output);
    }
  }
  return mask;
}

static uint8_t engineOutputMask()
{
  uint8_t mask = 0;
  for (uint8_t output = 0; output < 4; ++output)
  {
    if (engine.digitalOutputValue(output))
    {
      mask |= static_cast<uint8_t>(1U << output);
    }
  }
  return mask;
}

static void testStep(const char *label,
                     bool input0,
                     bool input1,
                     bool input2,
                     uint32_t nowMs,
                     bool expectedSetReset,
                     bool expectedTon,
                     bool expectedTiming,
                     uint32_t expectedElapsedMs,
                     uint32_t expectedRemainingMs,
                     uint8_t expectedOutputMask)
{
  bool inputs[8] = {};
  inputs[0] = input0;
  inputs[1] = input1;
  inputs[2] = input2;
  char message[112];

  snprintf(message, sizeof(message), "%s: referencia v1 evalua", label);
  expect(message, evaluateV1Reference(inputs, nowMs));

  snprintf(message, sizeof(message), "%s: motor v2 ejecuta scan", label);
  expect(message, engine.scan(inputs, 8, nowMs));

  snprintf(message, sizeof(message), "%s: los 14 bloques coinciden", label);
  expect(message, allBlockValuesMatch());

  snprintf(message, sizeof(message), "%s: las cuatro Q logicas coinciden", label);
  expect(message, referenceOutputMask() == engineOutputMask());

  snprintf(message, sizeof(message), "%s: mascara de salidas esperada", label);
  expect(message, engineOutputMask() == expectedOutputMask);

  snprintf(message, sizeof(message), "%s: estados SR TON y timing", label);
  expect(message,
         engine.blockValue(SET_RESET_INDEX) == expectedSetReset &&
             engine.blockValue(TON_INDEX) == expectedTon &&
             engine.tonTiming(TON_INDEX) == expectedTiming);

  snprintf(message, sizeof(message), "%s: elapsed y remaining", label);
  expect(message,
         engine.tonElapsedMs(TON_INDEX, nowMs) == expectedElapsedMs &&
             engine.tonRemainingMs(TON_INDEX, nowMs) ==
                 expectedRemainingMs);
}

void setup()
{
  Serial.begin(115200);
  delay(300);

  Serial.println();
  Serial.println("JWPLC Logic Runtime - regresion integrada v1 a v2");
  Serial.println("Entradas simuladas; no conmuta Q0 fisicas ni usa FRAM.");
  Serial.println();

  expect("descriptor v1 conserva 12 bytes",
         sizeof(LogicBlockDefinition) == 12);
  expect("descriptor v2 conserva 12 bytes",
         sizeof(LogicV2BlockRecord) == 12);
  expect("enlace v2 conserva 2 bytes",
         sizeof(LogicV2InputLink) == 2);

  size_t requiredLinks = 0;
  LogicV1ToV2AdapterError adapterError =
      LogicV1ToV2AdapterError::None;

  expect("adaptador calcula el programa integrado",
         LogicV1ToV2Adapter::requiredLinkCount(
             V1_PROGRAM,
             requiredLinks,
             adapterError));
  expect("programa integrado requiere 16 enlaces",
         requiredLinks == LINK_COUNT);
  expect("calculo integrado no deja error",
         adapterError == LogicV1ToV2AdapterError::None);

  LogicV2BlockRecord convertedBlocks[BLOCK_COUNT];
  LogicV2InputLink convertedLinks[LINK_COUNT];
  LogicV2Program convertedProgram = {nullptr, 0, nullptr, 0};

  expect("adaptador convierte el programa integrado",
         LogicV1ToV2Adapter::convert(
             V1_PROGRAM,
             convertedBlocks,
             BLOCK_COUNT,
             convertedLinks,
             LINK_COUNT,
             convertedProgram,
             adapterError));
  expect("conversion conserva 14 bloques",
         convertedProgram.blockCount == BLOCK_COUNT);
  expect("conversion conserva 16 enlaces",
         convertedProgram.linkCount == LINK_COUNT);
  expect("tipos historicos se convierten correctamente",
         convertedBlocks[3].type == LogicV2BlockType::Not &&
             convertedBlocks[4].type == LogicV2BlockType::And &&
             convertedBlocks[5].type == LogicV2BlockType::Or &&
             convertedBlocks[6].type == LogicV2BlockType::SetReset &&
             convertedBlocks[7].type == LogicV2BlockType::Ton &&
             convertedBlocks[10].type == LogicV2BlockType::DigitalOutput);
  expect("TON conserva fuente y parametro",
         convertedLinks[7].source() == 6 &&
             convertedBlocks[TON_INDEX].parameter == TON_DELAY_MS);
  expect("salidas conservan recursos y fuentes",
         convertedBlocks[10].resource == 0 &&
             convertedBlocks[11].resource == 1 &&
             convertedBlocks[12].resource == 2 &&
             convertedBlocks[13].resource == 3 &&
             convertedLinks[12].source() == 6 &&
             convertedLinks[13].source() == 7 &&
             convertedLinks[14].source() == 9 &&
             convertedLinks[15].source() == 5);

  expect("validador v2 acepta el programa integrado",
         LogicVariableInputPrototype::validate(
             convertedProgram,
             100,
             512,
             8,
             8) == LogicV2PrototypeError::None);
  expect("motor carga el programa integrado",
         engine.loadProgram(convertedProgram, 8, 8));
  expect("motor inicia el programa integrado",
         engine.start());

  const bool allFalse[8] = {};
  expect("scan sin tiempo se rechaza por presencia de TON",
         !engine.scan(allFalse, 8));
  expect("scan sin tiempo informa TIME_REQUIRED",
         engine.lastError() == LogicV2EngineError::TimeRequired);

  resetReference();

  testStep("estado inicial",
           false, false, false,
           100,
           false, false, false,
           0, 1000,
           0x04);

  testStep("SET inicia memoria y TON",
           true, false, false,
           1000,
           true, false, true,
           0, 1000,
           0x0D);

  testStep("SET liberado conserva memoria",
           false, false, false,
           1300,
           true, false, true,
           300, 700,
           0x05);

  testStep("TON llega a 999 ms",
           false, false, true,
           1999,
           true, false, true,
           999, 1,
           0x01);

  testStep("TON alcanza 1000 ms",
           false, false, true,
           2000,
           true, true, false,
           1000, 0,
           0x07);

  testStep("TON mantiene salida",
           false, false, true,
           2200,
           true, true, false,
           1000, 0,
           0x07);

  testStep("RESET apaga SR y TON",
           false, true, true,
           2300,
           false, false, false,
           0, 1000,
           0x08);

  testStep("RESET liberado mantiene apagado",
           false, false, true,
           2400,
           false, false, false,
           0, 1000,
           0x00);

  testStep("SET y RESET simultaneos",
           true, true, false,
           3000,
           false, false, false,
           0, 1000,
           0x0C);

  testStep("RESET liberado permite nuevo SET",
           true, false, false,
           3100,
           true, false, true,
           0, 1000,
           0x0D);

  testStep("segunda cuenta llega a 999 ms",
           false, false, false,
           4099,
           true, false, true,
           999, 1,
           0x05);

  testStep("segunda cuenta alcanza 1000 ms",
           false, false, false,
           4100,
           true, true, false,
           1000, 0,
           0x07);

  testStep("SR sostiene TON con permiso activo",
           false, false, true,
           4200,
           true, true, false,
           1000, 0,
           0x07);

  testStep("RESET final limpia cadena",
           false, true, true,
           4300,
           false, false, false,
           0, 1000,
           0x08);

  engine.stop();
  expect("stop deja el motor STOPPED",
         engine.state() == LogicV2EngineState::Stopped);
  expect("stop limpia SET/RESET y TON",
         !engine.blockValue(SET_RESET_INDEX) &&
             !engine.blockValue(TON_INDEX));
  expect("stop limpia las cuatro Q logicas",
         engineOutputMask() == 0);
  expect("stop limpia timing del TON",
         !engine.tonTiming(TON_INDEX));
  expect("motor reinicia para prueba de rollover",
         engine.start());

  resetReference();

  testStep("rollover inicia TON",
           true, false, false,
           0xFFFFFF00UL,
           true, false, true,
           0, 1000,
           0x0D);

  testStep("rollover conserva 999 ms",
           false, false, false,
           0x000002E7UL,
           true, false, true,
           999, 1,
           0x05);

  testStep("rollover alcanza 1000 ms",
           false, false, false,
           0x000002E8UL,
           true, true, false,
           1000, 0,
           0x07);

  expect("tres scans ejecutados tras reinicio",
         engine.scanCount() == 3);

  // Regresión del overload antiguo para programas sin temporizadores.
  const LogicV2BlockRecord combinationalBlocks[] = {
      {LogicV2BlockType::DigitalInput, 0, 0, 0},
      {LogicV2BlockType::DigitalInput, 0, 0, 1},
      {LogicV2BlockType::And, 0, 2}};
  const LogicV2InputLink combinationalLinks[] = {
      LogicV2InputLink::block(0),
      LogicV2InputLink::block(1)};
  const LogicV2Program combinationalProgram = {
      combinationalBlocks,
      3,
      combinationalLinks,
      2};

  expect("motor carga programa final sin TON",
         engine.loadProgram(combinationalProgram, 8));
  expect("motor inicia programa final sin TON",
         engine.start());

  bool highInputs[8] = {};
  highInputs[0] = true;
  highInputs[1] = true;
  expect("overload scan sin nowMs sigue operativo sin TON",
         engine.scan(highInputs, 8));
  expect("AND final entrega TRUE",
         engine.blockValue(2));
  expect("scan combinacional no deja TIME_REQUIRED",
         engine.lastError() == LogicV2EngineError::None);

  engine.unloadProgram();
  expect("unload final deja motor EMPTY",
         engine.state() == LogicV2EngineState::Empty &&
             !engine.hasProgram());

  Serial.println();
  Serial.print("Resultado: ");
  Serial.print(passedTests);
  Serial.print(" PASS, ");
  Serial.print(failedTests);
  Serial.println(" FAIL");
  Serial.println(failedTests == 0
                     ? "REGRESION INTEGRADA V1 A V2: PASS"
                     : "REGRESION INTEGRADA V1 A V2: FAIL");
}

void loop()
{
}