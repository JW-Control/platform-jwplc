#include <JWPLC_LogicRuntime.h>

static constexpr uint16_t TEST_MAX_BLOCKS = 100;
static constexpr uint8_t TEST_INPUT_COUNT = 8;
static constexpr uint8_t TEST_OUTPUT_COUNT = 8;

static uint8_t passedTests = 0;
static uint8_t failedTests = 0;

static bool runValidationTest(const char *name,
                              const LogicProgram &program,
                              LogicValidationError expected)
{
  const LogicValidationError actual = LogicValidator::validate(program,
                                                                TEST_MAX_BLOCKS,
                                                                TEST_INPUT_COUNT,
                                                                TEST_OUTPUT_COUNT);
  const bool passed = actual == expected;

  Serial.print(passed ? "[PASS] " : "[FAIL] ");
  Serial.print(name);
  Serial.print(" | esperado=");
  Serial.print(LogicValidator::errorName(expected));
  Serial.print(" | obtenido=");
  Serial.println(LogicValidator::errorName(actual));

  if (passed)
  {
    ++passedTests;
  }
  else
  {
    ++failedTests;
  }

  return passed;
}

static const LogicBlockDefinition VALID_BLOCKS[] = {
    {LogicBlockType::DigitalInput,
     JWPLC_LOGIC_NO_SOURCE,
     JWPLC_LOGIC_NO_SOURCE,
     0,
     0},
    {LogicBlockType::Not,
     0,
     JWPLC_LOGIC_NO_SOURCE,
     0,
     0},
    {LogicBlockType::DigitalOutput,
     1,
     JWPLC_LOGIC_NO_SOURCE,
     0,
     0}};

static const LogicBlockDefinition EMPTY_PLACEHOLDER[] = {
    {LogicBlockType::DigitalInput,
     JWPLC_LOGIC_NO_SOURCE,
     JWPLC_LOGIC_NO_SOURCE,
     0,
     0}};

static const LogicBlockDefinition INVALID_TYPE_BLOCKS[] = {
    {static_cast<LogicBlockType>(0xFF),
     JWPLC_LOGIC_NO_SOURCE,
     JWPLC_LOGIC_NO_SOURCE,
     0,
     0}};

static const LogicBlockDefinition MISSING_SOURCE_BLOCKS[] = {
    {LogicBlockType::DigitalOutput,
     JWPLC_LOGIC_NO_SOURCE,
     JWPLC_LOGIC_NO_SOURCE,
     0,
     0}};

static const LogicBlockDefinition SOURCE_OUT_OF_RANGE_BLOCKS[] = {
    {LogicBlockType::DigitalInput,
     JWPLC_LOGIC_NO_SOURCE,
     JWPLC_LOGIC_NO_SOURCE,
     0,
     0},
    {LogicBlockType::Not,
     2,
     JWPLC_LOGIC_NO_SOURCE,
     0,
     0}};

static const LogicBlockDefinition SOURCE_NOT_PREVIOUS_BLOCKS[] = {
    {LogicBlockType::DigitalInput,
     JWPLC_LOGIC_NO_SOURCE,
     JWPLC_LOGIC_NO_SOURCE,
     0,
     0},
    {LogicBlockType::Not,
     1,
     JWPLC_LOGIC_NO_SOURCE,
     0,
     0}};

static const LogicBlockDefinition RESOURCE_OUT_OF_RANGE_BLOCKS[] = {
    {LogicBlockType::DigitalInput,
     JWPLC_LOGIC_NO_SOURCE,
     JWPLC_LOGIC_NO_SOURCE,
     TEST_INPUT_COUNT,
     0}};

static const LogicBlockDefinition DUPLICATE_OUTPUT_BLOCKS[] = {
    {LogicBlockType::DigitalInput,
     JWPLC_LOGIC_NO_SOURCE,
     JWPLC_LOGIC_NO_SOURCE,
     0,
     0},
    {LogicBlockType::DigitalOutput,
     0,
     JWPLC_LOGIC_NO_SOURCE,
     0,
     0},
    {LogicBlockType::DigitalOutput,
     0,
     JWPLC_LOGIC_NO_SOURCE,
     0,
     0}};

void setup()
{
  Serial.begin(115200);
  delay(200);

  Serial.println();
  Serial.println("JWPLC Logic Runtime - pruebas del validador");
  Serial.println("Este ejemplo no inicializa E/S ni conmuta salidas.");
  Serial.println();

  const LogicProgram validProgram = {
      "Valid",
      VALID_BLOCKS,
      static_cast<uint16_t>(sizeof(VALID_BLOCKS) / sizeof(VALID_BLOCKS[0]))};

  const LogicProgram nullProgram = {
      "Null",
      nullptr,
      1};

  const LogicProgram emptyProgram = {
      "Empty",
      EMPTY_PLACEHOLDER,
      0};

  const LogicProgram tooManyBlocksProgram = {
      "TooMany",
      EMPTY_PLACEHOLDER,
      TEST_MAX_BLOCKS + 1};

  const LogicProgram invalidTypeProgram = {
      "InvalidType",
      INVALID_TYPE_BLOCKS,
      1};

  const LogicProgram missingSourceProgram = {
      "MissingSource",
      MISSING_SOURCE_BLOCKS,
      1};

  const LogicProgram sourceOutOfRangeProgram = {
      "SourceOutOfRange",
      SOURCE_OUT_OF_RANGE_BLOCKS,
      2};

  const LogicProgram sourceNotPreviousProgram = {
      "SourceNotPrevious",
      SOURCE_NOT_PREVIOUS_BLOCKS,
      2};

  const LogicProgram resourceOutOfRangeProgram = {
      "ResourceOutOfRange",
      RESOURCE_OUT_OF_RANGE_BLOCKS,
      1};

  const LogicProgram duplicateOutputProgram = {
      "DuplicateOutput",
      DUPLICATE_OUTPUT_BLOCKS,
      3};

  runValidationTest("programa valido",
                    validProgram,
                    LogicValidationError::None);
  runValidationTest("puntero de bloques nulo",
                    nullProgram,
                    LogicValidationError::NullProgram);
  runValidationTest("programa vacio",
                    emptyProgram,
                    LogicValidationError::EmptyProgram);
  runValidationTest("limite de bloques excedido",
                    tooManyBlocksProgram,
                    LogicValidationError::TooManyBlocks);
  runValidationTest("tipo de bloque invalido",
                    invalidTypeProgram,
                    LogicValidationError::InvalidBlockType);
  runValidationTest("fuente ausente",
                    missingSourceProgram,
                    LogicValidationError::MissingSource);
  runValidationTest("fuente fuera de rango",
                    sourceOutOfRangeProgram,
                    LogicValidationError::SourceOutOfRange);
  runValidationTest("fuente no anterior",
                    sourceNotPreviousProgram,
                    LogicValidationError::SourceNotPrevious);
  runValidationTest("recurso fuera de rango",
                    resourceOutOfRangeProgram,
                    LogicValidationError::ResourceOutOfRange);
  runValidationTest("salida digital duplicada",
                    duplicateOutputProgram,
                    LogicValidationError::DuplicateDigitalOutput);

  Serial.println();
  Serial.print("Resultado: ");
  Serial.print(passedTests);
  Serial.print(" PASS, ");
  Serial.print(failedTests);
  Serial.println(" FAIL");

  if (failedTests == 0)
  {
    Serial.println("VALIDACION COMPLETA: PASS");
  }
  else
  {
    Serial.println("VALIDACION COMPLETA: FAIL");
  }
}

void loop()
{
}
