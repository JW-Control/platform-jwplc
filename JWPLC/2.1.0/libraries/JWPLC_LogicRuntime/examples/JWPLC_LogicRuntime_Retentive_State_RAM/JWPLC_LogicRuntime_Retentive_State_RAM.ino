#include <JWPLC_GlobalPeripherals.h>
#include <JWPLC_LogicRuntime.h>

#include <cstring>

static constexpr uint8_t RETENTIVE_BLOCK_INDEX = 3;
static constexpr size_t IMAGE_BYTES =
    JWPLC_LOGIC_IMAGE_HEADER_SIZE +
    (5U * JWPLC_LOGIC_IMAGE_BLOCK_SIZE);

static JWPLC_LogicRuntime runtime;
static uint16_t passedTests = 0;
static uint16_t failedTests = 0;

static const LogicBlockDefinition PROGRAM_BLOCKS[] = {
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
    {LogicBlockType::SetReset,
     0,
     1,
     0,
     0},
    {LogicBlockType::SetReset,
     0,
     1,
     0,
     0,
     JWPLC_LOGIC_BLOCK_FLAG_RETENTIVE},
    {LogicBlockType::Ton,
     0,
     JWPLC_LOGIC_NO_SOURCE,
     0,
     2000}};

static const LogicProgram PROGRAM = {
    "Retentivos RAM",
    PROGRAM_BLOCKS,
    static_cast<uint16_t>(sizeof(PROGRAM_BLOCKS) /
                          sizeof(PROGRAM_BLOCKS[0]))};

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
  Serial.print("Resultado: ");
  Serial.print(passedTests);
  Serial.print(" PASS, ");
  Serial.print(failedTests);
  Serial.println(" FAIL");

  Serial.println(failedTests == 0
                     ? "RETENTIVOS EN RAM: PASS"
                     : "RETENTIVOS EN RAM: FAIL");
}

void setup()
{
  Serial.begin(115200);
  delay(300);

  Serial.println();
  Serial.println("JWPLC Logic Runtime - retentivos v1 en RAM");
  Serial.println("Inicializa E/S, mantiene Q0 apagadas y no usa la FRAM.");
  Serial.println();

  expect("LogicBlockDefinition conserva 12 bytes",
         sizeof(LogicBlockDefinition) == 12);
  expect("inicializador historico deja flags en cero",
         PROGRAM_BLOCKS[2].flags == JWPLC_LOGIC_BLOCK_FLAG_NONE);
  expect("SET/RESET marcado reconoce flag retentivo",
         PROGRAM_BLOCKS[RETENTIVE_BLOCK_INDEX].isRetentive());
  expect("TON permanece no retentivo",
         !PROGRAM_BLOCKS[4].isRetentive());

  expect("validador acepta programa retentivo",
         LogicValidator::validate(PROGRAM, 100, 8, 8) ==
             LogicValidationError::None);

  const bool beginOk = runtime.begin(
      JWPLCLogicStorageProfiles::FRAM_8K.framBytes);
  expect("runtime.begin() inicializa E/S", beginOk);

  const bool loadOk = beginOk && runtime.loadProgram(PROGRAM);
  expect("runtime carga programa con flags", loadOk);
  expect("runtime identifica un bloque retentivo",
         loadOk && runtime.retentiveBlockCount() == 1);
  expect("bitmap del programa ocupa un byte",
         loadOk && runtime.retentiveStateBytes() == 1);
  expect("estado retentivo inicia en falso",
         loadOk && !runtime.blockValue(RETENTIVE_BLOCK_INDEX));

  uint8_t importedBitmap[1] = {
      static_cast<uint8_t>((1U << 2U) |
                           (1U << RETENTIVE_BLOCK_INDEX) |
                           (1U << 4U))};

  const bool importOk =
      loadOk && runtime.importRetentiveState(importedBitmap,
                                             sizeof(importedBitmap));
  expect("bitmap retentivo se importa", importOk);
  expect("SET/RESET retentivo recupera valor verdadero",
         importOk && runtime.blockValue(RETENTIVE_BLOCK_INDEX));
  expect("SET/RESET no retentivo ignora su bit",
         importOk && !runtime.blockValue(2));
  expect("TON ignora cualquier bit del snapshot",
         importOk && !runtime.blockValue(4));

  uint8_t exportedBitmap[1] = {0xFF};
  const bool exportOk =
      importOk && runtime.exportRetentiveState(exportedBitmap,
                                               sizeof(exportedBitmap));
  expect("estado retentivo se exporta", exportOk);
  expect("exportacion contiene solo SET/RESET retentivo activo",
         exportOk &&
             exportedBitmap[0] ==
                 static_cast<uint8_t>(1U << RETENTIVE_BLOCK_INDEX));

  expect("exportacion rechaza buffer insuficiente",
         !runtime.exportRetentiveState(exportedBitmap, 0));
  expect("importacion rechaza buffer insuficiente",
         !runtime.importRetentiveState(importedBitmap, 0));

  runtime.clearRetentiveStates();
  expect("clearRetentiveStates limpia el valor",
         !runtime.blockValue(RETENTIVE_BLOCK_INDEX));

  expect("snapshot exportado puede restaurarse",
         runtime.importRetentiveState(exportedBitmap,
                                      sizeof(exportedBitmap)));
  expect("restauracion recupera nuevamente el valor",
         runtime.blockValue(RETENTIVE_BLOCK_INDEX));

  uint8_t image[IMAGE_BYTES] = {};
  size_t writtenBytes = 0;
  const LogicProgramCodecError serializeError =
      LogicProgramCodec::serialize(PROGRAM,
                                   0xA001,
                                   1,
                                   0,
                                   image,
                                   sizeof(image),
                                   writtenBytes);
  expect("codec serializa programa con retentivo",
         serializeError == LogicProgramCodecError::None &&
             writtenBytes == sizeof(image));

  const size_t legacyFlagsOffset =
      JWPLC_LOGIC_IMAGE_HEADER_SIZE +
      (2U * JWPLC_LOGIC_IMAGE_BLOCK_SIZE) + 1U;
  const size_t retentiveFlagsOffset =
      JWPLC_LOGIC_IMAGE_HEADER_SIZE +
      (RETENTIVE_BLOCK_INDEX * JWPLC_LOGIC_IMAGE_BLOCK_SIZE) + 1U;

  expect("registro historico conserva byte de flags cero",
         image[legacyFlagsOffset] == JWPLC_LOGIC_BLOCK_FLAG_NONE);
  expect("registro retentivo conserva flag en byte reservado",
         image[retentiveFlagsOffset] ==
             JWPLC_LOGIC_BLOCK_FLAG_RETENTIVE);

  LogicProgramBuffer decoded = {};
  const LogicProgramCodecError deserializeError =
      LogicProgramCodec::deserialize(image, writtenBytes, decoded);
  expect("codec reconstruye imagen con flags",
         deserializeError == LogicProgramCodecError::None);
  expect("imagen reconstruida conserva SET/RESET retentivo",
         deserializeError == LogicProgramCodecError::None &&
             decoded.blocks[RETENTIVE_BLOCK_INDEX].isRetentive());
  expect("programa reconstruido supera validador",
         deserializeError == LogicProgramCodecError::None &&
             LogicValidator::validate(decoded.asProgram(), 100, 8, 8) ==
                 LogicValidationError::None);

  LogicBlockDefinition unknownFlagBlocks[5];
  memcpy(unknownFlagBlocks, PROGRAM_BLOCKS, sizeof(PROGRAM_BLOCKS));
  unknownFlagBlocks[RETENTIVE_BLOCK_INDEX].flags = 0x80;
  const LogicProgram unknownFlagProgram = {
      "Flag desconocido",
      unknownFlagBlocks,
      5};

  expect("validador rechaza flag desconocido",
         LogicValidator::validate(unknownFlagProgram, 100, 8, 8) ==
             LogicValidationError::InvalidBlockFlags);

  writtenBytes = 0;
  expect("codec rechaza flag desconocido",
         LogicProgramCodec::serialize(unknownFlagProgram,
                                      0xA002,
                                      1,
                                      0,
                                      image,
                                      sizeof(image),
                                      writtenBytes) ==
             LogicProgramCodecError::InvalidBlockFlags);

  LogicBlockDefinition invalidTypeBlocks[5];
  memcpy(invalidTypeBlocks, PROGRAM_BLOCKS, sizeof(PROGRAM_BLOCKS));
  invalidTypeBlocks[4].flags = JWPLC_LOGIC_BLOCK_FLAG_RETENTIVE;
  const LogicProgram invalidTypeProgram = {
      "TON retentivo invalido",
      invalidTypeBlocks,
      5};

  expect("validador rechaza retencion aplicada a TON",
         LogicValidator::validate(invalidTypeProgram, 100, 8, 8) ==
             LogicValidationError::InvalidBlockFlags);

  writtenBytes = 0;
  expect("codec rechaza retencion aplicada a TON",
         LogicProgramCodec::serialize(invalidTypeProgram,
                                      0xA003,
                                      1,
                                      0,
                                      image,
                                      sizeof(image),
                                      writtenBytes) ==
             LogicProgramCodecError::InvalidBlockFlags);

  runtime.stop();
  expect("stop limpia el estado temporal en RAM",
         !runtime.blockValue(RETENTIVE_BLOCK_INDEX));
  expect("snapshot puede reimportarse despues de stop",
         runtime.importRetentiveState(exportedBitmap,
                                      sizeof(exportedBitmap)));
  expect("valor retentivo vuelve a quedar disponible",
         runtime.blockValue(RETENTIVE_BLOCK_INDEX));

  printSummary();
}

void loop()
{
}
