#include <JWPLC_LogicRuntime.h>
#include <JWPLC_LogicRuntime_UI.h>

static LogicV2EnginePrototype engine;
static RuntimeUIV2EditSession editSession;
static uint16_t checksPassed = 0;
static uint16_t checksFailed = 0;

static const LogicV2InputLink BASE_LINKS[] = {
    // B02 AND2: B00, B01. Queda sin consumidores para probar eliminación.
    LogicV2InputLink::block(0),
    LogicV2InputLink::block(1),

    // B03 NOT: B00.
    LogicV2InputLink::block(0),

    // B04 Q0.0 lógica <- B03.
    LogicV2InputLink::block(3)};

static const LogicV2BlockRecord BASE_BLOCKS[] = {
    {LogicV2BlockType::DigitalInput, 0, 0, 0},
    {LogicV2BlockType::DigitalInput, 0, 0, 1},
    {LogicV2BlockType::And, 0, 2},
    {LogicV2BlockType::Not, 2, 1},
    {LogicV2BlockType::DigitalOutput, 3, 1, 0}};

static const LogicV2Program BASE_PROGRAM = {
    BASE_BLOCKS,
    static_cast<uint16_t>(sizeof(BASE_BLOCKS) / sizeof(BASE_BLOCKS[0])),
    BASE_LINKS,
    static_cast<uint16_t>(sizeof(BASE_LINKS) / sizeof(BASE_LINKS[0]))};

static void check(bool condition, const char *label)
{
  Serial.print(condition ? "[OK]   " : "[FAIL] ");
  Serial.println(label);
  if (condition)
  {
    ++checksPassed;
  }
  else
  {
    ++checksFailed;
  }
}

static bool sourceIs(uint16_t blockIndex,
                     uint8_t inputIndex,
                     uint16_t expectedSource)
{
  const LogicV2InputLink *link = editSession.inputLink(blockIndex, inputIndex);
  return link != nullptr && link->source() == expectedSource;
}

void setup()
{
  Serial.begin(115200);
  delay(300);

  Serial.println();
  Serial.println("JWPLC Logic Runtime UI - EditSession estructural RAM v0.5.5");
  Serial.println("Prueba append/remove/compactacion/aplicacion sin TFT ni FRAM.");
  Serial.println();

  check(LogicVariableInputPrototype::validate(BASE_PROGRAM, 100, 512, 8, 8) ==
            LogicV2PrototypeError::None,
        "Programa base valida");
  check(engine.loadProgram(BASE_PROGRAM, 8, 8),
        "Motor carga programa base");
  check(engine.start(),
        "Motor inicia RUNNING");

  editSession.attach(engine);
  check(editSession.begin(),
        "Sesion copia programa activo");
  check(editSession.blockCount() == 5 && editSession.linkCount() == 4,
        "Conteos iniciales 5 bloques / 4 enlaces");

  check(editSession.consumerCount(2) == 0,
        "B02 AND no tiene consumidores");
  check(editSession.consumerCount(3) == 1,
        "B03 NOT tiene un consumidor");
  check(!editSession.removeBlock(3),
        "No elimina bloque con consumidores");

  check(editSession.removeBlock(2),
        "Elimina B02 sin consumidores");
  check(editSession.blockCount() == 4 && editSession.linkCount() == 2,
        "Compacta a 4 bloques / 2 enlaces");
  check(editSession.block(2) != nullptr &&
            editSession.block(2)->type == LogicV2BlockType::Not &&
            editSession.block(2)->firstInput == 0,
        "B03 NOT pasa a B02 y ajusta firstInput");
  check(sourceIs(2, 0, 0),
        "Entrada de NOT conserva B00");
  check(editSession.block(3) != nullptr &&
            editSession.block(3)->type == LogicV2BlockType::DigitalOutput &&
            editSession.block(3)->firstInput == 1,
        "Q0.0 pasa a B03 y compacta firstInput");
  check(sourceIs(3, 0, 2),
        "Q0.0 actualiza referencia B03->B02");
  check(editSession.validate() == LogicV2PrototypeError::None,
        "Borrador valido tras compactacion");

  const LogicV2InputLink tonInputs[] = {
      LogicV2InputLink::block(2)};
  uint16_t tonIndex = 0xFFFFU;
  check(editSession.appendBlock(LogicV2BlockType::Ton,
                                tonInputs,
                                1,
                                0,
                                2500,
                                &tonIndex),
        "Agrega TON al final");
  check(tonIndex == 4 &&
            editSession.blockCount() == 5 &&
            editSession.linkCount() == 3,
        "TON queda como B04 con un enlace nuevo");

  const LogicV2InputLink notInputs[] = {
      LogicV2InputLink::block(tonIndex)};
  uint16_t notIndex = 0xFFFFU;
  check(editSession.appendBlock(LogicV2BlockType::Not,
                                notInputs,
                                1,
                                0,
                                0,
                                &notIndex),
        "Agrega NOT consumidor del TON");
  check(notIndex == 5 && editSession.consumerCount(tonIndex) == 1,
        "TON registra un consumidor");
  check(!editSession.removeBlock(tonIndex),
        "Bloquea eliminacion de TON utilizado");
  check(editSession.removeBlock(notIndex),
        "Elimina consumidor NOT");
  check(editSession.removeBlock(tonIndex),
        "Elimina TON ya sin consumidores");
  check(editSession.blockCount() == 4 && editSession.linkCount() == 2,
        "Retorna a 4 bloques / 2 enlaces");

  const uint16_t blocksBeforeInvalid = editSession.blockCount();
  const uint16_t linksBeforeInvalid = editSession.linkCount();
  const LogicV2InputLink duplicateOutputInput[] = {
      LogicV2InputLink::block(2)};
  check(!editSession.appendBlock(LogicV2BlockType::DigitalOutput,
                                 duplicateOutputInput,
                                 1,
                                 0),
        "Rechaza Q0.0 duplicada");
  check(editSession.blockCount() == blocksBeforeInvalid &&
            editSession.linkCount() == linksBeforeInvalid,
        "Append invalido no altera borrador");

  const LogicV2InputLink invalidNotInput[] = {
      LogicV2InputLink::block(99)};
  check(!editSession.appendBlock(LogicV2BlockType::Not,
                                 invalidNotInput,
                                 1),
        "Rechaza fuente futura/inexistente");
  check(editSession.blockCount() == blocksBeforeInvalid &&
            editSession.linkCount() == linksBeforeInvalid,
        "Segundo append invalido tambien es atomico");

  const LogicV2InputLink orInputs[] = {
      LogicV2InputLink::block(0),
      LogicV2InputLink::block(1)};
  uint16_t orIndex = 0xFFFFU;
  check(editSession.appendBlock(LogicV2BlockType::Or,
                                orInputs,
                                2,
                                0,
                                0,
                                &orIndex),
        "Agrega OR2 valido");
  check(orIndex == 4 && editSession.validate() == LogicV2PrototypeError::None,
        "OR2 queda como B04 y programa valida");

  check(editSession.apply(),
        "Aplica borrador y reinicia motor");
  check(engine.blockCount() == 5 && engine.linkCount() == 4,
        "Motor activo recibe 5 bloques / 4 enlaces");

  const bool inputs[8] = {true, false, false, false,
                          false, false, false, false};
  check(engine.scan(inputs, 8, millis()),
        "Scan posterior a aplicacion");
  check(engine.blockValue(4),
        "OR2 agregado evalua TRUE con I0.0=1 e I0.1=0");
  check(!engine.digitalOutputValue(0),
        "Q0.0 conserva NOT(I0.0) y evalua FALSE");

  Serial.println();
  Serial.print("RESULTADO: ");
  Serial.print(checksPassed);
  Serial.print(" OK, ");
  Serial.print(checksFailed);
  Serial.println(" FAIL");
  Serial.println(checksFailed == 0 ? "PRUEBA APROBADA" : "PRUEBA FALLIDA");
}

void loop()
{
  delay(1000);
}
