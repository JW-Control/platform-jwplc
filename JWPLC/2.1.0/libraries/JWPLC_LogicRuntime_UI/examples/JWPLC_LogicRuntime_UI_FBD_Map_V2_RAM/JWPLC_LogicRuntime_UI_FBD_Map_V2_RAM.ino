#include <JWPLC_GlobalPeripherals.h>
#include <JWPLC_LogicRuntime.h>
#include <JWPLC_LogicRuntime_UI.h>

static constexpr uint16_t BLOCK_COUNT = 11;
static constexpr uint16_t LINK_COUNT = 12;
static constexpr uint32_t TON_DELAY_MS = 2000;
static constexpr uint32_t CYCLE_MS = 9000;

static LogicV2EnginePrototype engine;
static uint8_t lastPhase = 0xFF;

static const LogicV2InputLink PROGRAM_LINKS[] = {
    // B04 AND4: B00, B01, !B02, B03.
    LogicV2InputLink::block(0),
    LogicV2InputLink::block(1),
    LogicV2InputLink::block(2, true),
    LogicV2InputLink::block(3),

    // B05 OR2: B02, LO.
    LogicV2InputLink::block(2),
    LogicV2InputLink::constantFalse(),

    // B06 SET/RESET: S=B04, R=B05.
    LogicV2InputLink::block(4),
    LogicV2InputLink::block(5),

    // B07 TON: Trg=B06.
    LogicV2InputLink::block(6),

    // B08 NOT B07.
    LogicV2InputLink::block(7),

    // B09 Q0.0 lógica <- B07.
    LogicV2InputLink::block(7),

    // B10 Q0.1 lógica <- B08.
    LogicV2InputLink::block(8)};

static const LogicV2BlockRecord PROGRAM_BLOCKS[] = {
    {LogicV2BlockType::DigitalInput, 0, 0, 0},
    {LogicV2BlockType::DigitalInput, 0, 0, 1},
    {LogicV2BlockType::DigitalInput, 0, 0, 2},
    {LogicV2BlockType::DigitalInput, 0, 0, 3},
    {LogicV2BlockType::And, 0, 4},
    {LogicV2BlockType::Or, 4, 2},
    {LogicV2BlockType::SetReset, 6, 2},
    {LogicV2BlockType::Ton, 8, 1, 0, TON_DELAY_MS},
    {LogicV2BlockType::Not, 9, 1},
    {LogicV2BlockType::DigitalOutput, 10, 1, 0},
    {LogicV2BlockType::DigitalOutput, 11, 1, 1}};

static const LogicV2Program PROGRAM = {
    PROGRAM_BLOCKS,
    BLOCK_COUNT,
    PROGRAM_LINKS,
    LINK_COUNT};

static uint8_t fillInputs(uint32_t nowMs, bool inputs[8])
{
  for (uint8_t index = 0; index < 8; ++index)
  {
    inputs[index] = false;
  }

  const uint32_t phaseMs = nowMs % CYCLE_MS;

  if (phaseMs >= 2000 && phaseMs < 3000)
  {
    inputs[0] = true;
    inputs[1] = true;
    inputs[2] = false;
    inputs[3] = true;
    return 1;
  }

  if (phaseMs >= 3000 && phaseMs < 6000)
  {
    return 2;
  }

  if (phaseMs >= 6000 && phaseMs < 7000)
  {
    inputs[2] = true;
    return 3;
  }

  if (phaseMs >= 7000)
  {
    return 4;
  }

  return 0;
}

static const char *phaseName(uint8_t phase)
{
  switch (phase)
  {
  case 0:
    return "INICIAL";
  case 1:
    return "SET";
  case 2:
    return "MEMORIA_Y_TON";
  case 3:
    return "RESET";
  case 4:
    return "ESPERA";
  default:
    return "?";
  }
}

void setup()
{
  Serial.begin(115200);
  delay(300);

  Serial.println();
  Serial.println("JWPLC Logic Runtime UI - mapa FBD v2 en RAM");
  Serial.println("11 bloques, 12 enlaces, AND4 con pin negado y TON 2 s.");
  Serial.println("Editor v0.5.0: RIGHT en DETALLE edita fuente y negacion.");
  Serial.println("No escribe FRAM ni conmuta salidas Q0 fisicas.");
  Serial.println();

  const LogicV2PrototypeError validation =
      LogicVariableInputPrototype::validate(PROGRAM, 100, 512, 8, 8);
  if (validation != LogicV2PrototypeError::None)
  {
    Serial.print("VALIDACION ERROR: ");
    Serial.println(LogicVariableInputPrototype::errorName(validation));
    return;
  }

  if (!engine.loadProgram(PROGRAM, 8, 8))
  {
    Serial.print("CARGA ERROR: ");
    Serial.println(LogicV2EnginePrototype::errorName(engine.lastError()));
    return;
  }

  if (!engine.start())
  {
    Serial.print("START ERROR: ");
    Serial.println(LogicV2EnginePrototype::errorName(engine.lastError()));
    return;
  }

  JWPLC_Display.setIdleWakeMode(IDLE_WAKE_ANY_BUTTON);
  JWPLC_LogicRuntime_UI.begin(engine);

  Serial.println("Motor v2: RUNNING");
  Serial.println("UI FBD v0.5.0: LISTA");
  Serial.println("Pulse cualquier boton para entrar a USER.");
}

void loop()
{
  const uint32_t nowMs = millis();
  bool inputs[8] = {};
  const uint8_t phase = fillInputs(nowMs, inputs);

  if (!engine.scan(inputs, 8, nowMs))
  {
    Serial.print("SCAN ERROR: ");
    Serial.println(LogicV2EnginePrototype::errorName(engine.lastError()));
    delay(1000);
    return;
  }

  JWPLC_LogicRuntime_UI.update();
  JWPLC_LogicRuntime_UI.processV2EditorPending();

  if (phase != lastPhase)
  {
    lastPhase = phase;
    Serial.print("Fase: ");
    Serial.print(phaseName(phase));
    Serial.print(" | SR=");
    Serial.print(engine.blockValue(6) ? 1 : 0);
    Serial.print(" TON=");
    Serial.print(engine.blockValue(7) ? 1 : 0);
    Serial.print(" timing=");
    Serial.print(engine.tonTiming(7) ? 1 : 0);
    Serial.print(" Q0.0(logica)=");
    Serial.print(engine.digitalOutputValue(0) ? 1 : 0);
    Serial.print(" Q0.1(logica)=");
    Serial.println(engine.digitalOutputValue(1) ? 1 : 0);
  }

  delay(5);
}
