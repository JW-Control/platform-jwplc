#include <JWPLC_GlobalPeripherals.h>
#include <JWPLC_LogicRuntime.h>

#include <cstring>

static constexpr size_t PHYSICAL_FRAM_BYTES = 8UL * 1024UL;
static constexpr size_t TEST_SLOT_BYTES = 512;
static constexpr size_t TEST_WINDOW_BYTES = 64 + (2 * TEST_SLOT_BYTES);
static constexpr size_t TEST_BASE_ADDRESS =
    PHYSICAL_FRAM_BYTES - TEST_WINDOW_BYTES;
static constexpr size_t SCRATCH_BYTES = TEST_SLOT_BYTES;

static constexpr LogicStorageProfile TEST_PROFILE(
    PHYSICAL_FRAM_BYTES,
    TEST_SLOT_BYTES,
    0,
    32,
    true);

static uint8_t backupBytes[TEST_WINDOW_BYTES];
static uint8_t verifyBytes[TEST_WINDOW_BYTES];
static uint8_t scratch[SCRATCH_BYTES];
static LogicProgramBuffer loadedProgram;

static uint16_t passedTests = 0;
static uint16_t failedTests = 0;

static const LogicBlockDefinition PROGRAM_A_BLOCKS[] = {
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

static const LogicProgram PROGRAM_A = {
    "FRAM Program A",
    PROGRAM_A_BLOCKS,
    static_cast<uint16_t>(sizeof(PROGRAM_A_BLOCKS) /
                          sizeof(PROGRAM_A_BLOCKS[0]))};

static const LogicBlockDefinition PROGRAM_B_BLOCKS[] = {
    {LogicBlockType::DigitalInput,
     JWPLC_LOGIC_NO_SOURCE,
     JWPLC_LOGIC_NO_SOURCE,
     1,
     0},
    {LogicBlockType::DigitalOutput,
     0,
     JWPLC_LOGIC_NO_SOURCE,
     1,
     0}};

static const LogicProgram PROGRAM_B = {
    "FRAM Program B",
    PROGRAM_B_BLOCKS,
    static_cast<uint16_t>(sizeof(PROGRAM_B_BLOCKS) /
                          sizeof(PROGRAM_B_BLOCKS[0]))};

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

static bool waitForConfirmation()
{
  Serial.println();
  Serial.println("ADVERTENCIA: esta prueba escribira temporalmente los ultimos");
  Serial.print(TEST_WINDOW_BYTES);
  Serial.println(" bytes de la FRAM fisica.");
  Serial.println("La region sera respaldada en RAM y restaurada al finalizar.");
  Serial.println("No reinicies ni cortes energia durante la prueba.");
  Serial.println("Escribe ERASE y pulsa Enviar para continuar.");

  for (;;)
  {
    if (Serial.available() > 0)
    {
      String command = Serial.readStringUntil('\n');
      command.trim();

      if (command == "ERASE")
      {
        return true;
      }

      if (command.length() > 0)
      {
        Serial.println("Comando no reconocido. Escribe exactamente ERASE.");
      }
    }

    delay(10);
  }
}

void setup()
{
  Serial.begin(115200);
  Serial.setTimeout(250);
  delay(300);

  Serial.println();
  Serial.println("JWPLC Logic Runtime - PoC 5 backend FRAM fisica");
  Serial.println("No inicializa E/S ni conmuta salidas.");
  Serial.println("Usa una ventana reducida al final de la FRAM.");
  Serial.println();

  Serial.print("FRAM detectada: ");
  Serial.print(JWPLC_FRAM.size());
  Serial.println(" bytes");
  Serial.print("Ventana de prueba: 0x");
  Serial.print(TEST_BASE_ADDRESS, HEX);
  Serial.print("..0x");
  Serial.println(TEST_BASE_ADDRESS + TEST_WINDOW_BYTES - 1, HEX);

  LogicFRAMStorage storage(
      JWPLC_FRAM,
      TEST_BASE_ADDRESS,
      TEST_WINDOW_BYTES);

  const bool backendReady = storage.begin();
  expect("backend FRAM inicializado", backendReady);
  expect("FRAM fisica reporta 8 KiB",
         JWPLC_FRAM.size() == PHYSICAL_FRAM_BYTES);
  expect("ventana FRAM tiene capacidad esperada",
         storage.capacity() == TEST_WINDOW_BYTES);
  expect("layout A/B reducido cabe en la ventana",
         LogicProgramStore::requiredCapacity(TEST_PROFILE) <=
             storage.capacity());

  if (!backendReady ||
      LogicProgramStore::requiredCapacity(TEST_PROFILE) > storage.capacity())
  {
    Serial.println("PRUEBA ABORTADA: backend o layout invalido.");
    return;
  }

  if (!waitForConfirmation())
  {
    return;
  }

  const bool backupOk =
      storage.read(0, backupBytes, sizeof(backupBytes));
  expect("respaldo temporal de la ventana", backupOk);

  if (!backupOk)
  {
    Serial.println("PRUEBA ABORTADA: no se pudo respaldar la FRAM.");
    return;
  }

  const uint32_t backupCrc =
      LogicProgramCodec::crc32(backupBytes, sizeof(backupBytes));

  LogicProgramStore store;
  const bool beginOk = store.begin(storage, TEST_PROFILE);
  expect("gestor A/B acepta backend FRAM", beginOk);

  const bool formatOk = beginOk && store.format();
  expect("formateo temporal de la ventana", formatOk);

  const bool saveAOk =
      formatOk && store.saveProgram(PROGRAM_A,
                                    0xA001,
                                    1,
                                    0,
                                    scratch,
                                    sizeof(scratch));
  expect("programa A guardado en FRAM", saveAOk);
  expect("Slot A activo tras primer guardado",
         saveAOk && store.status().activeSlot == 0);

  LogicProgramStore rebootA;
  const bool rebootABeginOk = rebootA.begin(storage, TEST_PROFILE);
  expect("reinicio logico detecta superblock A", rebootABeginOk);

  const bool loadAOk =
      rebootABeginOk && rebootA.loadActive(loadedProgram,
                                           scratch,
                                           sizeof(scratch));
  expect("programa A cargado desde FRAM", loadAOk);
  expect("programa A conserva metadatos",
         loadAOk &&
             loadedProgram.metadata.programId == 0xA001 &&
             loadedProgram.metadata.generation == 1 &&
             strcmp(loadedProgram.name, PROGRAM_A.name) == 0);
  expect("programa A reconstruido supera validador",
         loadAOk &&
             LogicValidator::validate(loadedProgram.asProgram(),
                                      TEST_PROFILE.maxBlocks,
                                      8,
                                      8) == LogicValidationError::None);

  const bool saveBOk =
      loadAOk && rebootA.saveProgram(PROGRAM_B,
                                     0xB002,
                                     2,
                                     0,
                                     scratch,
                                     sizeof(scratch));
  expect("programa B guardado en FRAM", saveBOk);
  expect("Slot B activo tras segundo guardado",
         saveBOk && rebootA.status().activeSlot == 1);

  LogicProgramStore rebootB;
  const bool rebootBBeginOk = rebootB.begin(storage, TEST_PROFILE);
  expect("reinicio logico detecta superblock B", rebootBBeginOk);

  const bool loadBOk =
      rebootBBeginOk && rebootB.loadActive(loadedProgram,
                                           scratch,
                                           sizeof(scratch));
  expect("programa B cargado desde FRAM", loadBOk);
  expect("programa B conserva metadatos",
         loadBOk &&
             loadedProgram.metadata.programId == 0xB002 &&
             loadedProgram.metadata.generation == 2 &&
             strcmp(loadedProgram.name, PROGRAM_B.name) == 0);
  expect("programa B reconstruido supera validador",
         loadBOk &&
             LogicValidator::validate(loadedProgram.asProgram(),
                                      TEST_PROFILE.maxBlocks,
                                      8,
                                      8) == LogicValidationError::None);

  const bool restoreWriteOk =
      storage.write(0, backupBytes, sizeof(backupBytes));
  expect("restauracion de la ventana escrita", restoreWriteOk);

  const bool restoreReadOk =
      restoreWriteOk && storage.read(0, verifyBytes, sizeof(verifyBytes));
  expect("restauracion de la ventana releida", restoreReadOk);

  const bool restoreMatches =
      restoreReadOk &&
      memcmp(backupBytes, verifyBytes, sizeof(backupBytes)) == 0 &&
      LogicProgramCodec::crc32(verifyBytes, sizeof(verifyBytes)) == backupCrc;
  expect("contenido original restaurado exactamente", restoreMatches);

  Serial.println();
  Serial.print("Resultado: ");
  Serial.print(passedTests);
  Serial.print(" PASS, ");
  Serial.print(failedTests);
  Serial.println(" FAIL");

  if (failedTests == 0 && restoreMatches)
  {
    Serial.println("BACKEND FRAM FISICA: PASS");
    Serial.println("La ventana de prueba fue restaurada.");
  }
  else
  {
    Serial.println("BACKEND FRAM FISICA: FAIL");
    Serial.println("No uses esta FRAM para datos importantes hasta revisar el log.");
  }
}

void loop()
{
}
