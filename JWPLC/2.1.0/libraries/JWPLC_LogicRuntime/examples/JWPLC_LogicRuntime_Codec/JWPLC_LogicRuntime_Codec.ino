#include <JWPLC_LogicRuntime.h>

#include <string.h>

static constexpr size_t TEST_IMAGE_CAPACITY = 2560;

static uint8_t imageBuffer[TEST_IMAGE_CAPACITY];
static LogicProgramBuffer decodedBuffer;
static uint8_t passedTests = 0;
static uint8_t failedTests = 0;

static const LogicBlockDefinition SOURCE_BLOCKS[] = {
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
    {LogicBlockType::Not,
     1,
     JWPLC_LOGIC_NO_SOURCE,
     0,
     0},
    {LogicBlockType::And,
     0,
     2,
     0,
     0},
    {LogicBlockType::Ton,
     3,
     JWPLC_LOGIC_NO_SOURCE,
     0,
     2000},
    {LogicBlockType::DigitalOutput,
     4,
     JWPLC_LOGIC_NO_SOURCE,
     0,
     0}};

static const LogicProgram SOURCE_PROGRAM = {
    "JWPLC Default",
    SOURCE_BLOCKS,
    static_cast<uint16_t>(sizeof(SOURCE_BLOCKS) / sizeof(SOURCE_BLOCKS[0]))};

static void check(const char *name, bool condition)
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

static bool blocksMatch(const LogicProgram &left, const LogicProgram &right)
{
  if (left.blockCount != right.blockCount)
  {
    return false;
  }

  for (uint16_t index = 0; index < left.blockCount; ++index)
  {
    const LogicBlockDefinition &a = left.blocks[index];
    const LogicBlockDefinition &b = right.blocks[index];

    if (a.type != b.type ||
        a.sourceA != b.sourceA ||
        a.sourceB != b.sourceB ||
        a.resource != b.resource ||
        a.parameter != b.parameter)
    {
      return false;
    }
  }

  return true;
}

void setup()
{
  Serial.begin(115200);
  delay(200);

  Serial.println();
  Serial.println("JWPLC Logic Runtime - PoC 3 codec binario");
  Serial.println("Este ejemplo no inicializa E/S ni conmuta salidas.");
  Serial.println();

  const size_t expectedSize =
      JWPLC_LOGIC_IMAGE_HEADER_SIZE +
      SOURCE_PROGRAM.blockCount * JWPLC_LOGIC_IMAGE_BLOCK_SIZE;

  check("tamano calculado del programa",
        LogicProgramCodec::requiredSize(SOURCE_PROGRAM.blockCount) == expectedSize);

  const size_t max100Size = LogicProgramCodec::requiredSize(100);
  check("100 bloques caben en slot FRAM 8 KiB",
        max100Size <= JWPLCLogicStorageProfiles::FRAM_8K.programSlotBytes);

  size_t writtenBytes = 0;
  LogicProgramCodecError codecError = LogicProgramCodec::serialize(
      SOURCE_PROGRAM,
      0x12345678UL,
      7,
      0,
      imageBuffer,
      sizeof(imageBuffer),
      writtenBytes);

  check("serializacion correcta", codecError == LogicProgramCodecError::None);
  check("bytes escritos correctos", writtenBytes == expectedSize);

  codecError = LogicProgramCodec::deserialize(imageBuffer,
                                               writtenBytes,
                                               decodedBuffer);
  check("deserializacion correcta", codecError == LogicProgramCodecError::None);

  const LogicProgram decodedProgram = decodedBuffer.asProgram();
  check("nombre conservado",
        strcmp(decodedProgram.name, SOURCE_PROGRAM.name) == 0);
  check("metadatos conservados",
        decodedBuffer.metadata.programId == 0x12345678UL &&
            decodedBuffer.metadata.generation == 7 &&
            decodedBuffer.metadata.blockCount == SOURCE_PROGRAM.blockCount);
  check("bloques conservados", blocksMatch(SOURCE_PROGRAM, decodedProgram));

  const LogicValidationError validationError = LogicValidator::validate(
      decodedProgram,
      JWPLCLogicStorageProfiles::FRAM_8K.maxBlocks,
      8,
      8);
  check("programa reconstruido supera validador",
        validationError == LogicValidationError::None);

  const uint8_t payloadBackup = imageBuffer[JWPLC_LOGIC_IMAGE_HEADER_SIZE];
  imageBuffer[JWPLC_LOGIC_IMAGE_HEADER_SIZE] ^= 0x01;
  codecError = LogicProgramCodec::deserialize(imageBuffer,
                                               writtenBytes,
                                               decodedBuffer);
  check("corrupcion de payload detectada",
        codecError == LogicProgramCodecError::PayloadCrcMismatch);
  imageBuffer[JWPLC_LOGIC_IMAGE_HEADER_SIZE] = payloadBackup;

  const uint8_t headerBackup = imageBuffer[12];
  imageBuffer[12] ^= 0x01;
  codecError = LogicProgramCodec::deserialize(imageBuffer,
                                               writtenBytes,
                                               decodedBuffer);
  check("corrupcion de cabecera detectada",
        codecError == LogicProgramCodecError::HeaderCrcMismatch);
  imageBuffer[12] = headerBackup;

  codecError = LogicProgramCodec::deserialize(imageBuffer,
                                               writtenBytes - 1,
                                               decodedBuffer);
  check("imagen truncada detectada",
        codecError == LogicProgramCodecError::InvalidLength);

  size_t ignoredBytes = 0;
  codecError = LogicProgramCodec::serialize(SOURCE_PROGRAM,
                                            1,
                                            1,
                                            0,
                                            imageBuffer,
                                            expectedSize - 1,
                                            ignoredBytes);
  check("buffer pequeno rechazado",
        codecError == LogicProgramCodecError::BufferTooSmall);

  static const LogicProgram LONG_NAME_PROGRAM = {
      "1234567890123456789012345",
      SOURCE_BLOCKS,
      SOURCE_PROGRAM.blockCount};

  codecError = LogicProgramCodec::serialize(LONG_NAME_PROGRAM,
                                            1,
                                            1,
                                            0,
                                            imageBuffer,
                                            sizeof(imageBuffer),
                                            ignoredBytes);
  check("nombre largo rechazado",
        codecError == LogicProgramCodecError::NameTooLong);

  Serial.println();
  Serial.print("Imagen de ");
  Serial.print(SOURCE_PROGRAM.blockCount);
  Serial.print(" bloques: ");
  Serial.print(expectedSize);
  Serial.println(" bytes");

  Serial.print("Imagen maxima inicial de 100 bloques: ");
  Serial.print(max100Size);
  Serial.print(" / ");
  Serial.print(JWPLCLogicStorageProfiles::FRAM_8K.programSlotBytes);
  Serial.println(" bytes por slot");

  Serial.print("Resultado: ");
  Serial.print(passedTests);
  Serial.print(" PASS, ");
  Serial.print(failedTests);
  Serial.println(" FAIL");

  Serial.println(failedTests == 0
                     ? "CODEC BINARIO: PASS"
                     : "CODEC BINARIO: FAIL");
}

void loop()
{
}
