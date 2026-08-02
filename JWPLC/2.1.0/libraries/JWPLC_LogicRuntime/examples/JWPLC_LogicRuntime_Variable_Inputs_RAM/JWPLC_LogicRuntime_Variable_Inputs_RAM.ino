#include <JWPLC_GlobalPeripherals.h>
#include <JWPLC_LogicRuntime.h>

#include <cstring>

static constexpr uint16_t BLOCK_COUNT = 16;
static constexpr uint16_t LINK_COUNT = 33;

static uint16_t passedTests = 0;
static uint16_t failedTests = 0;

static const LogicV2InputLink PROGRAM_LINKS[] = {
    // B08 AND de cuatro entradas: B00, B01, B02, B03.
    LogicV2InputLink::block(0),
    LogicV2InputLink::block(1),
    LogicV2InputLink::block(2),
    LogicV2InputLink::block(3),

    // B09 AND: B00, !B01, X y HI.
    LogicV2InputLink::block(0),
    LogicV2InputLink::block(1, true),
    LogicV2InputLink::open(),
    LogicV2InputLink::constantTrue(),

    // B10 OR: B00, B01, X y LO.
    LogicV2InputLink::block(0),
    LogicV2InputLink::block(1),
    LogicV2InputLink::open(),
    LogicV2InputLink::constantFalse(),

    // B11 NAND de cuatro entradas.
    LogicV2InputLink::block(0),
    LogicV2InputLink::block(1),
    LogicV2InputLink::block(2),
    LogicV2InputLink::block(3),

    // B12 NOR de cuatro entradas.
    LogicV2InputLink::block(0),
    LogicV2InputLink::block(1),
    LogicV2InputLink::block(2),
    LogicV2InputLink::block(3),

    // B13 XOR de cuatro entradas.
    LogicV2InputLink::block(0),
    LogicV2InputLink::block(1),
    LogicV2InputLink::block(2),
    LogicV2InputLink::block(3),

    // B14 NOT B00.
    LogicV2InputLink::block(0),

    // B15 AND de ocho entradas: B00..B07.
    LogicV2InputLink::block(0),
    LogicV2InputLink::block(1),
    LogicV2InputLink::block(2),
    LogicV2InputLink::block(3),
    LogicV2InputLink::block(4),
    LogicV2InputLink::block(5),
    LogicV2InputLink::block(6),
    LogicV2InputLink::block(7)};

static const LogicV2BlockRecord PROGRAM_BLOCKS[] = {
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

static const LogicV2Program PROGRAM = {
    PROGRAM_BLOCKS,
    BLOCK_COUNT,
    PROGRAM_LINKS,
    LINK_COUNT};

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

static bool evaluate(const bool inputs[8],
                     bool values[BLOCK_COUNT],
                     LogicV2PrototypeError &error)
{
  memset(values, 0, sizeof(bool) * BLOCK_COUNT);
  return LogicVariableInputPrototype::evaluate(PROGRAM,
                                                inputs,
                                                8,
                                                values,
                                                BLOCK_COUNT,
                                                error);
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
                     ? "ENTRADAS VARIABLES V2 EN RAM: PASS"
                     : "ENTRADAS VARIABLES V2 EN RAM: FAIL");
}

void setup()
{
  Serial.begin(115200);
  delay(300);

  Serial.println();
  Serial.println("JWPLC Logic Runtime - entradas variables v2 en RAM");
  Serial.println("No inicializa E/S, no conmuta Q0 y no usa la FRAM.");
  Serial.println();

  expect("LogicBlockDefinition v1 conserva 12 bytes",
         sizeof(LogicBlockDefinition) == 12);
  expect("LogicV2BlockRecord conserva 12 bytes",
         sizeof(LogicV2BlockRecord) == 12);
  expect("cada enlace v2 ocupa 2 bytes",
         sizeof(LogicV2InputLink) == 2);
  expect("politica inicial limita a 8 entradas por bloque",
         JWPLC_LOGIC_V2_MAX_INPUTS_PER_BLOCK == 8);

  const LogicV2InputLink normal = LogicV2InputLink::block(23);
  const LogicV2InputLink inverted = LogicV2InputLink::block(23, true);
  expect("enlace normal conserva indice de bloque",
         normal.source() == 23);
  expect("enlace normal no queda negado", !normal.inverted());
  expect("enlace negado conserva el mismo bloque",
         inverted.source() == 23);
  expect("enlace negado conserva bit individual",
         inverted.inverted());
  expect("X usa codigo especial",
         LogicV2InputLink::open().source() ==
             JWPLC_LOGIC_V2_SOURCE_OPEN);
  expect("HI usa codigo especial",
         LogicV2InputLink::constantTrue().source() ==
             JWPLC_LOGIC_V2_SOURCE_CONST_TRUE);
  expect("LO usa codigo especial",
         LogicV2InputLink::constantFalse().source() ==
             JWPLC_LOGIC_V2_SOURCE_CONST_FALSE);

  const size_t basicImageBytes =
      LogicVariableInputPrototype::requiredImageBytes(100, 512);
  const size_t largeImageBytes =
      LogicVariableInputPrototype::requiredImageBytes(400, 2048);
  expect("imagen v2 Basic usa 2288 bytes",
         basicImageBytes == 2288);
  expect("imagen v2 Basic cabe en payload de 2528 bytes",
         basicImageBytes <= 2528);
  expect("imagen v2 de 400 bloques usa 8960 bytes",
         largeImageBytes == 8960);
  expect("imagen v2 de 400 bloques cabe en payload de 12256 bytes",
         largeImageBytes <= 12256);

  expect("validador acepta programa de 4 y 8 entradas",
         LogicVariableInputPrototype::validate(PROGRAM, 100, 512, 8) ==
             LogicV2PrototypeError::None);

  bool values[BLOCK_COUNT] = {};
  LogicV2PrototypeError error = LogicV2PrototypeError::None;

  bool inputs[8] = {false, false, false, false,
                    false, false, false, false};
  expect("evaluacion con todas las entradas en cero",
         evaluate(inputs, values, error) &&
             error == LogicV2PrototypeError::None);
  expect("AND4 queda falso con entradas en cero", !values[8]);
  expect("AND con pin negado queda falso porque B00 es cero", !values[9]);
  expect("OR con X y LO queda falso", !values[10]);
  expect("NAND4 queda verdadero", values[11]);
  expect("NOR4 queda verdadero", values[12]);
  expect("XOR4 queda falso con paridad cero", !values[13]);
  expect("NOT invierte B00 a verdadero", values[14]);
  expect("AND8 queda falso", !values[15]);

  const bool mixedInputs[8] = {true, false, true, true,
                               true, true, true, true};
  memcpy(inputs, mixedInputs, sizeof(inputs));
  expect("evaluacion con patron mixto",
         evaluate(inputs, values, error) &&
             error == LogicV2PrototypeError::None);
  expect("AND4 detecta B01 falso", !values[8]);
  expect("negacion individual convierte B01 en verdadero", values[9]);
  expect("X es neutro para AND y HI no altera el resultado", values[9]);
  expect("OR4 detecta B00 verdadero", values[10]);
  expect("NAND4 niega la conjuncion incompleta", values[11]);
  expect("NOR4 queda falso ante una entrada verdadera", !values[12]);
  expect("XOR4 entrega paridad impar", values[13]);
  expect("NOT B00 queda falso", !values[14]);
  expect("AND8 detecta una de ocho entradas falsa", !values[15]);

  for (uint8_t input = 0; input < 8; ++input)
  {
    inputs[input] = true;
  }
  expect("evaluacion con ocho entradas verdaderas",
         evaluate(inputs, values, error) &&
             error == LogicV2PrototypeError::None);
  expect("AND4 queda verdadero", values[8]);
  expect("AND con B01 negado queda falso", !values[9]);
  expect("OR4 queda verdadero", values[10]);
  expect("NAND4 queda falso", !values[11]);
  expect("NOR4 queda falso", !values[12]);
  expect("XOR4 queda falso con paridad par", !values[13]);
  expect("NOT B00 queda falso con B00 verdadero", !values[14]);
  expect("AND8 queda verdadero", values[15]);

  LogicV2BlockRecord invalidBlocks[BLOCK_COUNT];
  LogicV2InputLink invalidLinks[LINK_COUNT];
  memcpy(invalidBlocks, PROGRAM_BLOCKS, sizeof(PROGRAM_BLOCKS));
  memcpy(invalidLinks, PROGRAM_LINKS, sizeof(PROGRAM_LINKS));

  LogicV2Program invalidProgram = {
      invalidBlocks,
      BLOCK_COUNT,
      invalidLinks,
      LINK_COUNT};

  invalidBlocks[8].inputCount = 1;
  expect("AND rechaza una sola entrada",
         LogicVariableInputPrototype::validate(invalidProgram, 100, 512, 8) ==
             LogicV2PrototypeError::InvalidInputCount);

  memcpy(invalidBlocks, PROGRAM_BLOCKS, sizeof(PROGRAM_BLOCKS));
  invalidBlocks[8].inputCount = 9;
  expect("AND rechaza mas de ocho entradas en politica inicial",
         LogicVariableInputPrototype::validate(invalidProgram, 100, 512, 8) ==
             LogicV2PrototypeError::InvalidInputCount);

  memcpy(invalidBlocks, PROGRAM_BLOCKS, sizeof(PROGRAM_BLOCKS));
  memcpy(invalidLinks, PROGRAM_LINKS, sizeof(PROGRAM_LINKS));
  invalidLinks[0] = LogicV2InputLink::block(8);
  expect("fuente debe apuntar a un bloque anterior",
         LogicVariableInputPrototype::validate(invalidProgram, 100, 512, 8) ==
             LogicV2PrototypeError::SourceNotPrevious);

  memcpy(invalidBlocks, PROGRAM_BLOCKS, sizeof(PROGRAM_BLOCKS));
  memcpy(invalidLinks, PROGRAM_LINKS, sizeof(PROGRAM_LINKS));
  invalidBlocks[8].firstInput = 32;
  expect("rango de enlaces fuera del programa se rechaza",
         LogicVariableInputPrototype::validate(invalidProgram, 100, 512, 8) ==
             LogicV2PrototypeError::InputRangeOutOfBounds);

  memcpy(invalidBlocks, PROGRAM_BLOCKS, sizeof(PROGRAM_BLOCKS));
  invalidBlocks[0].resource = 8;
  expect("entrada digital fuera del perfil se rechaza",
         LogicVariableInputPrototype::validate(invalidProgram, 100, 512, 8) ==
             LogicV2PrototypeError::ResourceOutOfRange);

  memcpy(invalidBlocks, PROGRAM_BLOCKS, sizeof(PROGRAM_BLOCKS));
  memcpy(invalidLinks, PROGRAM_LINKS, sizeof(PROGRAM_LINKS));
  invalidLinks[24] = LogicV2InputLink::open();
  expect("NOT rechaza una entrada abierta",
         LogicVariableInputPrototype::validate(invalidProgram, 100, 512, 8) ==
             LogicV2PrototypeError::OpenInputNotAllowed);

  memcpy(invalidBlocks, PROGRAM_BLOCKS, sizeof(PROGRAM_BLOCKS));
  invalidBlocks[8].type = static_cast<LogicV2BlockType>(0xFF);
  expect("tipo desconocido se rechaza",
         LogicVariableInputPrototype::validate(invalidProgram, 100, 512, 8) ==
             LogicV2PrototypeError::InvalidBlockType);

  memcpy(invalidBlocks, PROGRAM_BLOCKS, sizeof(PROGRAM_BLOCKS));
  invalidBlocks[8].flags = 0x80;
  expect("flags v2 desconocidos se rechazan",
         LogicVariableInputPrototype::validate(invalidProgram, 100, 512, 8) ==
             LogicV2PrototypeError::InvalidBlockFlags);

  expect("presupuesto de bloques se aplica",
         LogicVariableInputPrototype::validate(PROGRAM, 15, 512, 8) ==
             LogicV2PrototypeError::TooManyBlocks);
  expect("presupuesto de enlaces se aplica",
         LogicVariableInputPrototype::validate(PROGRAM, 100, 32, 8) ==
             LogicV2PrototypeError::TooManyLinks);

  const LogicV2Program emptyProgram = {PROGRAM_BLOCKS, 0, nullptr, 0};
  expect("programa vacio se rechaza",
         LogicVariableInputPrototype::validate(emptyProgram, 100, 512, 8) ==
             LogicV2PrototypeError::EmptyProgram);

  const LogicV2Program nullLinksProgram = {
      PROGRAM_BLOCKS,
      BLOCK_COUNT,
      nullptr,
      LINK_COUNT};
  expect("tabla de enlaces nula se rechaza",
         LogicVariableInputPrototype::validate(nullLinksProgram,
                                               100,
                                               512,
                                               8) ==
             LogicV2PrototypeError::NullArgument);

  error = LogicV2PrototypeError::None;
  expect("evaluador rechaza arreglo de entradas nulo",
         !LogicVariableInputPrototype::evaluate(PROGRAM,
                                                nullptr,
                                                8,
                                                values,
                                                BLOCK_COUNT,
                                                error) &&
             error == LogicV2PrototypeError::NullDigitalInputs);

  error = LogicV2PrototypeError::None;
  expect("evaluador rechaza buffer de valores insuficiente",
         !LogicVariableInputPrototype::evaluate(PROGRAM,
                                                inputs,
                                                8,
                                                values,
                                                BLOCK_COUNT - 1,
                                                error) &&
             error == LogicV2PrototypeError::OutputBufferTooSmall);

  printSummary();
}

void loop()
{
}