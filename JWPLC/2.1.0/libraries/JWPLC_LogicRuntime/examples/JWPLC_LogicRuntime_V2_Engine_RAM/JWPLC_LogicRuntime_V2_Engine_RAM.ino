#include <JWPLC_GlobalPeripherals.h>
#include <JWPLC_LogicRuntime.h>

#include <cstring>

static constexpr uint16_t DEMO_BLOCK_COUNT = 16;
static constexpr uint16_t DEMO_LINK_COUNT = 33;
static constexpr size_t MIN_ENGINE_STORAGE_BYTES =
    (JWPLC_LOGIC_V2_COMPILED_MAX_BLOCKS * sizeof(LogicV2BlockRecord)) +
    (JWPLC_LOGIC_V2_COMPILED_MAX_LINKS * sizeof(LogicV2InputLink)) +
    (JWPLC_LOGIC_V2_COMPILED_MAX_BLOCKS * sizeof(bool));

static LogicV2EnginePrototype engine;
static uint16_t passedTests = 0;
static uint16_t failedTests = 0;

static const LogicV2InputLink DEMO_LINKS[] = {
    // B08 AND4: B00, B01, B02, B03.
    LogicV2InputLink::block(0),
    LogicV2InputLink::block(1),
    LogicV2InputLink::block(2),
    LogicV2InputLink::block(3),

    // B09 AND4: B00, !B01, X, HI.
    LogicV2InputLink::block(0),
    LogicV2InputLink::block(1, true),
    LogicV2InputLink::open(),
    LogicV2InputLink::constantTrue(),

    // B10 OR4: B00, B01, X, LO.
    LogicV2InputLink::block(0),
    LogicV2InputLink::block(1),
    LogicV2InputLink::open(),
    LogicV2InputLink::constantFalse(),

    // B11 NAND4.
    LogicV2InputLink::block(0),
    LogicV2InputLink::block(1),
    LogicV2InputLink::block(2),
    LogicV2InputLink::block(3),

    // B12 NOR4.
    LogicV2InputLink::block(0),
    LogicV2InputLink::block(1),
    LogicV2InputLink::block(2),
    LogicV2InputLink::block(3),

    // B13 XOR4.
    LogicV2InputLink::block(0),
    LogicV2InputLink::block(1),
    LogicV2InputLink::block(2),
    LogicV2InputLink::block(3),

    // B14 NOT B00.
    LogicV2InputLink::block(0),

    // B15 AND8: B00..B07.
    LogicV2InputLink::block(0),
    LogicV2InputLink::block(1),
    LogicV2InputLink::block(2),
    LogicV2InputLink::block(3),
    LogicV2InputLink::block(4),
    LogicV2InputLink::block(5),
    LogicV2InputLink::block(6),
    LogicV2InputLink::block(7)};

static const LogicV2BlockRecord DEMO_BLOCKS[] = {
    {LogicV2BlockType::DigitalInput, 0, 0, 0},
    {LogicV2BlockType::DigitalInput, 0, 0, 1},
    {LogicV2BlockType::DigitalInput, 0, 0, 2},
    {LogicV2BlockType::DigitalInput, 0, 0, 3},
    {LogicV2BlockType::DigitalInput, 0, 0, 4},
    {LogicV2BlockType::DigitalInput, 0, 0, 5},
    {LogicV2BlockType::DigitalInput, 0, 0, 6},
    {LogicV2BlockType::DigitalInput, 0, 0, 7},
    {LogicV2BlockType::And, 0, 4},
    {LogicV2BlockType::And, 4, 4},
    {LogicV2BlockType::Or, 8, 4},
    {LogicV2BlockType::Nand, 12, 4},
    {LogicV2BlockType::Nor, 16, 4},
    {LogicV2BlockType::Xor, 20, 4},
    {LogicV2BlockType::Not, 24, 1},
    {LogicV2BlockType::And, 25, 8}};

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

static void printSummary()
{
  Serial.println();
  Serial.print("sizeof motor v2: ");
  Serial.print(sizeof(LogicV2EnginePrototype));
  Serial.println(" bytes");
  Serial.print("almacen minimo de arreglos: ");
  Serial.print(MIN_ENGINE_STORAGE_BYTES);
  Serial.println(" bytes");
  Serial.println();

  Serial.print("Resultado: ");
  Serial.print(passedTests);
  Serial.print(" PASS, ");
  Serial.print(failedTests);
  Serial.println(" FAIL");
  Serial.println(failedTests == 0
                     ? "MOTOR V2 EN RAM: PASS"
                     : "MOTOR V2 EN RAM: FAIL");
}

void setup()
{
  Serial.begin(115200);
  delay(300);

  Serial.println();
  Serial.println("JWPLC Logic Runtime - motor v2 en RAM");
  Serial.println("No inicializa E/S, no conmuta Q0 y no usa la FRAM.");
  Serial.println();

  expect("perfil compilado conserva 100 bloques",
         JWPLC_LOGIC_V2_COMPILED_MAX_BLOCKS == 100);
  expect("perfil compilado reserva 512 enlaces",
         JWPLC_LOGIC_V2_COMPILED_MAX_LINKS == 512);
  expect("motor contiene todos los arreglos requeridos",
         sizeof(LogicV2EnginePrototype) >= MIN_ENGINE_STORAGE_BYTES);
  expect("overhead del motor permanece menor o igual a 64 bytes",
         sizeof(LogicV2EnginePrototype) <= MIN_ENGINE_STORAGE_BYTES + 64U);
  expect("motor inicia sin programa", !engine.hasProgram());
  expect("estado inicial es EMPTY",
         engine.state() == LogicV2EngineState::Empty);

  expect("start rechaza motor sin programa", !engine.start());
  expect("start sin programa informa NOT_READY",
         engine.lastError() == LogicV2EngineError::NotReady);
  engine.unloadProgram();
  expect("unload recupera estado EMPTY",
         engine.state() == LogicV2EngineState::Empty);

  LogicV2BlockRecord sourceBlocks[DEMO_BLOCK_COUNT];
  LogicV2InputLink sourceLinks[DEMO_LINK_COUNT];
  memcpy(sourceBlocks, DEMO_BLOCKS, sizeof(sourceBlocks));
  memcpy(sourceLinks, DEMO_LINKS, sizeof(sourceLinks));
  const LogicV2Program sourceProgram = {
      sourceBlocks,
      DEMO_BLOCK_COUNT,
      sourceLinks,
      DEMO_LINK_COUNT};

  expect("motor carga programa demo", engine.loadProgram(sourceProgram, 8));
  expect("carga deja motor READY",
         engine.state() == LogicV2EngineState::Ready);
  expect("motor conserva 16 bloques", engine.blockCount() == 16);
  expect("motor conserva 33 enlaces", engine.linkCount() == 33);
  expect("motor conserva perfil de 8 entradas",
         engine.digitalInputCount() == 8);
  expect("carga valida no deja error de validacion",
         engine.validationError() == LogicV2PrototypeError::None);

  const LogicV2Program *loadedProgram = engine.program();
  expect("bloques usan copia profunda",
         loadedProgram != nullptr && loadedProgram->blocks != sourceBlocks);
  expect("enlaces usan copia profunda",
         loadedProgram != nullptr && loadedProgram->links != sourceLinks);

  sourceBlocks[0].resource = 7;
  sourceLinks[0] = LogicV2InputLink::constantFalse();
  expect("modificar origen no altera bloque cargado",
         engine.blockDefinition(0) != nullptr &&
             engine.blockDefinition(0)->resource == 0);
  expect("modificar origen no altera enlace cargado",
         engine.inputLink(0) != nullptr &&
             engine.inputLink(0)->source() == 0);

  bool inputs[8] = {};
  expect("scan antes de start se rechaza", !engine.scan(inputs, 8));
  expect("scan detenido informa NOT_RUNNING",
         engine.lastError() == LogicV2EngineError::NotRunning);
  expect("start inicia programa cargado", engine.start());
  expect("estado cambia a RUNNING",
         engine.state() == LogicV2EngineState::Running);

  expect("scan rechaza arreglo nulo", !engine.scan(nullptr, 8));
  expect("arreglo nulo informa INPUT_PROFILE_MISMATCH",
         engine.lastError() == LogicV2EngineError::InputProfileMismatch);
  expect("scan rechaza perfil de entradas corto", !engine.scan(inputs, 7));

  expect("scan con todas las entradas en cero", engine.scan(inputs, 8));
  expect("primer scan incrementa contador", engine.scanCount() == 1);
  expect("resultados en cero coinciden",
         !engine.blockValue(8) &&
             !engine.blockValue(9) &&
             !engine.blockValue(10) &&
             engine.blockValue(11) &&
             engine.blockValue(12) &&
             !engine.blockValue(13) &&
             engine.blockValue(14) &&
             !engine.blockValue(15));

  const bool mixedInputs[8] = {true, false, true, true,
                               true, true, true, true};
  memcpy(inputs, mixedInputs, sizeof(inputs));
  expect("scan con patron mixto", engine.scan(inputs, 8));
  expect("segundo scan incrementa contador", engine.scanCount() == 2);
  expect("negacion por pin permanece operativa", engine.blockValue(9));
  expect("XOR conserva paridad impar", engine.blockValue(13));

  for (uint8_t input = 0; input < 8; ++input)
  {
    inputs[input] = true;
  }
  expect("scan con ocho entradas verdaderas", engine.scan(inputs, 8));
  expect("tercer scan incrementa contador", engine.scanCount() == 3);
  expect("AND4 queda verdadero", engine.blockValue(8));
  expect("AND8 queda verdadero", engine.blockValue(15));
  expect("consulta de bloque fuera de rango devuelve nullptr",
         engine.blockDefinition(DEMO_BLOCK_COUNT) == nullptr);
  expect("consulta de enlace fuera de rango devuelve nullptr",
         engine.inputLink(DEMO_LINK_COUNT) == nullptr);

  engine.stop();
  expect("stop deja estado STOPPED",
         engine.state() == LogicV2EngineState::Stopped);
  expect("stop limpia valores y contador",
         !engine.blockValue(8) &&
             !engine.blockValue(15) &&
             engine.scanCount() == 0);
  expect("scan despues de stop se rechaza", !engine.scan(inputs, 8));
  expect("motor puede reiniciarse despues de stop", engine.start());
  expect("scan funciona despues de reiniciar", engine.scan(inputs, 8));

  // Programa máximo válido para el perfil Basic: 100 bloques y 512 enlaces.
  LogicV2BlockRecord maxBlocks[JWPLC_LOGIC_V2_COMPILED_MAX_BLOCKS];
  LogicV2InputLink maxLinks[JWPLC_LOGIC_V2_COMPILED_MAX_LINKS];

  for (uint16_t block = 0; block < 8; ++block)
  {
    maxBlocks[block] = LogicV2BlockRecord(
        LogicV2BlockType::DigitalInput,
        0,
        0,
        block);
  }

  for (uint16_t block = 8; block < 72; ++block)
  {
    const uint16_t firstInput = static_cast<uint16_t>((block - 8U) * 8U);
    maxBlocks[block] = LogicV2BlockRecord(
        LogicV2BlockType::And,
        firstInput,
        8);

    for (uint8_t input = 0; input < 8; ++input)
    {
      maxLinks[firstInput + input] = LogicV2InputLink::block(input);
    }
  }

  for (uint16_t block = 72;
       block < JWPLC_LOGIC_V2_COMPILED_MAX_BLOCKS;
       ++block)
  {
    maxBlocks[block] = LogicV2BlockRecord(LogicV2BlockType::ConstantTrue);
  }

  const LogicV2Program maxProgram = {
      maxBlocks,
      JWPLC_LOGIC_V2_COMPILED_MAX_BLOCKS,
      maxLinks,
      JWPLC_LOGIC_V2_COMPILED_MAX_LINKS};

  expect("motor carga capacidad maxima Basic",
         engine.loadProgram(maxProgram, 8));
  expect("programa maximo conserva 100 bloques",
         engine.blockCount() == 100);
  expect("programa maximo conserva 512 enlaces",
         engine.linkCount() == 512);
  expect("programa maximo puede iniciar", engine.start());
  expect("programa maximo ejecuta un scan", engine.scan(inputs, 8));
  expect("ultima compuerta AND8 queda verdadera",
         engine.blockValue(71));
  expect("ultimo bloque constante queda verdadero",
         engine.blockValue(99));

  const LogicV2Program tooManyBlocks = {
      maxBlocks,
      static_cast<uint16_t>(JWPLC_LOGIC_V2_COMPILED_MAX_BLOCKS + 1U),
      maxLinks,
      JWPLC_LOGIC_V2_COMPILED_MAX_LINKS};
  expect("motor rechaza mas bloques que la capacidad compilada",
         !engine.loadProgram(tooManyBlocks, 8));
  expect("exceso de bloques conserva error detallado",
         engine.validationError() == LogicV2PrototypeError::TooManyBlocks);
  expect("carga invalida descarga el programa anterior",
         !engine.hasProgram() &&
             engine.state() == LogicV2EngineState::Fault);

  const LogicV2Program tooManyLinks = {
      maxBlocks,
      JWPLC_LOGIC_V2_COMPILED_MAX_BLOCKS,
      maxLinks,
      static_cast<uint16_t>(JWPLC_LOGIC_V2_COMPILED_MAX_LINKS + 1U)};
  expect("motor rechaza mas enlaces que la capacidad compilada",
         !engine.loadProgram(tooManyLinks, 8));
  expect("exceso de enlaces conserva error detallado",
         engine.validationError() == LogicV2PrototypeError::TooManyLinks);

  engine.unloadProgram();
  expect("unload final limpia motor",
         !engine.hasProgram() &&
             engine.state() == LogicV2EngineState::Empty &&
             engine.lastError() == LogicV2EngineError::None);

  printSummary();
}

void loop()
{
}