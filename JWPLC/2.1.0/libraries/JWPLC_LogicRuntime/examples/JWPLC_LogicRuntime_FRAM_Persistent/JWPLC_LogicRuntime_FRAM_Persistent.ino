#include <JWPLC_GlobalPeripherals.h>
#include <JWPLC_LogicRuntime.h>
#include <Preferences.h>

#include <cstring>

static constexpr size_t PHYSICAL_FRAM_BYTES = 8UL * 1024UL;
static constexpr size_t TEST_SLOT_BYTES = 512;
static constexpr size_t TEST_WINDOW_BYTES = 64 + (2 * TEST_SLOT_BYTES);
static constexpr size_t TEST_BASE_ADDRESS =
    PHYSICAL_FRAM_BYTES - TEST_WINDOW_BYTES;
static constexpr size_t SCRATCH_BYTES = TEST_SLOT_BYTES;

static constexpr char NVS_NAMESPACE[] = "jwlrpoc6";
static constexpr char NVS_KEY_STAGE[] = "stage";
static constexpr char NVS_KEY_BACKUP[] = "backup";
static constexpr char NVS_KEY_CRC[] = "backup_crc";

static constexpr uint8_t STAGE_IDLE = 0;
static constexpr uint8_t STAGE_BACKUP_READY = 1;
static constexpr uint8_t STAGE_PROGRAM_A_READY = 2;
static constexpr uint8_t STAGE_PROGRAM_B_READY = 3;
static constexpr uint8_t STAGE_RESTORE_PENDING = 4;

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

static LogicFRAMStorage storage(
    JWPLC_FRAM,
    TEST_BASE_ADDRESS,
    TEST_WINDOW_BYTES);

static Preferences preferences;

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
    "Persist Program A",
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
    "Persist Program B",
    PROGRAM_B_BLOCKS,
    static_cast<uint16_t>(sizeof(PROGRAM_B_BLOCKS) /
                          sizeof(PROGRAM_B_BLOCKS[0]))};

static bool waitForStartConfirmation()
{
  Serial.println();
  Serial.println("ADVERTENCIA: esta prueba usara temporalmente los ultimos");
  Serial.print(TEST_WINDOW_BYTES);
  Serial.println(" bytes de la FRAM fisica durante varios reinicios.");
  Serial.println("El contenido original se respaldara en NVS y se restaurara");
  Serial.println("automaticamente al completar la tercera etapa.");
  Serial.println("Escribe START y pulsa Enviar para comenzar.");

  for (;;)
  {
    if (Serial.available() > 0)
    {
      String command = Serial.readStringUntil('\n');
      command.trim();

      if (command == "START")
      {
        return true;
      }

      if (command.length() > 0)
      {
        Serial.println("Comando no reconocido. Escribe exactamente START.");
      }
    }

    delay(10);
  }
}

static bool loadBackupFromNVS()
{
  if (preferences.getBytesLength(NVS_KEY_BACKUP) != sizeof(backupBytes))
  {
    return false;
  }

  const size_t readBytes = preferences.getBytes(
      NVS_KEY_BACKUP,
      backupBytes,
      sizeof(backupBytes));

  if (readBytes != sizeof(backupBytes))
  {
    return false;
  }

  const uint32_t expectedCrc = preferences.getUInt(NVS_KEY_CRC, 0);
  return LogicProgramCodec::crc32(backupBytes, sizeof(backupBytes)) ==
         expectedCrc;
}

static bool createPersistentBackup()
{
  if (!storage.read(0, backupBytes, sizeof(backupBytes)))
  {
    return false;
  }

  const uint32_t backupCrc =
      LogicProgramCodec::crc32(backupBytes, sizeof(backupBytes));

  if (preferences.putBytes(NVS_KEY_BACKUP,
                           backupBytes,
                           sizeof(backupBytes)) != sizeof(backupBytes))
  {
    return false;
  }

  if (preferences.putUInt(NVS_KEY_CRC, backupCrc) != sizeof(uint32_t))
  {
    return false;
  }

  return preferences.putUChar(NVS_KEY_STAGE, STAGE_BACKUP_READY) ==
         sizeof(uint8_t);
}

static bool validateLoadedProgram(uint32_t expectedProgramId,
                                  uint32_t expectedGeneration,
                                  const char *expectedName)
{
  return loadedProgram.metadata.programId == expectedProgramId &&
         loadedProgram.metadata.generation == expectedGeneration &&
         strcmp(loadedProgram.name, expectedName) == 0 &&
         LogicValidator::validate(loadedProgram.asProgram(),
                                  TEST_PROFILE.maxBlocks,
                                  8,
                                  8) == LogicValidationError::None;
}

static bool restoreOriginalWindow()
{
  if (!loadBackupFromNVS())
  {
    Serial.println("FAIL: respaldo NVS ausente o corrupto.");
    return false;
  }

  if (!storage.write(0, backupBytes, sizeof(backupBytes)) ||
      !storage.read(0, verifyBytes, sizeof(verifyBytes)) ||
      memcmp(backupBytes, verifyBytes, sizeof(backupBytes)) != 0)
  {
    Serial.println("FAIL: no se pudo restaurar exactamente la ventana FRAM.");
    return false;
  }

  const uint32_t expectedCrc = preferences.getUInt(NVS_KEY_CRC, 0);
  if (LogicProgramCodec::crc32(verifyBytes, sizeof(verifyBytes)) != expectedCrc)
  {
    Serial.println("FAIL: CRC de la ventana restaurada no coincide.");
    return false;
  }

  const bool clearOk = preferences.clear();

  Serial.println("[PASS] Contenido original de FRAM restaurado exactamente.");
  Serial.println(clearOk ? "[PASS] Estado temporal NVS eliminado."
                         : "[WARN] No se pudo limpiar NVS; la FRAM si fue restaurada.");
  Serial.println();
  Serial.println("PERSISTENCIA ENTRE REINICIOS: PASS");
  Serial.println("PoC 6 completada.");
  return true;
}

static bool runStageBackupReady()
{
  Serial.println("Etapa 1/3: formatear ventana y guardar Programa A.");

  if (!loadBackupFromNVS())
  {
    Serial.println("FAIL: respaldo NVS ausente o corrupto.");
    return false;
  }

  LogicProgramStore store;
  if (!store.begin(storage, TEST_PROFILE) ||
      !store.format() ||
      !store.saveProgram(PROGRAM_A,
                         0xA001,
                         1,
                         0,
                         scratch,
                         sizeof(scratch)))
  {
    Serial.print("FAIL: no se pudo guardar Programa A. Error: ");
    Serial.println(LogicProgramStore::errorName(store.lastError()));
    return false;
  }

  LogicProgramStore reboot;
  if (!reboot.begin(storage, TEST_PROFILE) ||
      !reboot.loadActive(loadedProgram, scratch, sizeof(scratch)) ||
      !validateLoadedProgram(0xA001, 1, PROGRAM_A.name))
  {
    Serial.println("FAIL: Programa A no se reconstruyo correctamente.");
    return false;
  }

  if (preferences.putUChar(NVS_KEY_STAGE, STAGE_PROGRAM_A_READY) !=
      sizeof(uint8_t))
  {
    Serial.println("FAIL: no se pudo registrar la etapa A en NVS.");
    return false;
  }

  Serial.println("[PASS] Programa A persistido y validado.");
  Serial.println("ETAPA 1 COMPLETA.");
  Serial.println("Ahora pulsa RESET o corta y restablece la alimentacion.");
  return true;
}

static bool runStageProgramAReady()
{
  Serial.println("Etapa 2/3: cargar Programa A tras reinicio y guardar B.");

  if (!loadBackupFromNVS())
  {
    Serial.println("FAIL: respaldo NVS ausente o corrupto.");
    return false;
  }

  LogicProgramStore store;
  if (!store.begin(storage, TEST_PROFILE) ||
      !store.loadActive(loadedProgram, scratch, sizeof(scratch)))
  {
    Serial.println("FAIL: no se pudo cargar un programa activo tras el reinicio.");
    return false;
  }

  if (validateLoadedProgram(0xB002, 2, PROGRAM_B.name))
  {
    Serial.println("[PASS] Programa B ya estaba confirmado tras una interrupcion previa.");
  }
  else
  {
    if (!validateLoadedProgram(0xA001, 1, PROGRAM_A.name))
    {
      Serial.println("FAIL: Programa A no sobrevivio al reinicio.");
      return false;
    }

    Serial.println("[PASS] Programa A cargado despues del reinicio.");

    if (!store.saveProgram(PROGRAM_B,
                           0xB002,
                           2,
                           0,
                           scratch,
                           sizeof(scratch)))
    {
      Serial.print("FAIL: no se pudo guardar Programa B. Error: ");
      Serial.println(LogicProgramStore::errorName(store.lastError()));
      return false;
    }
  }

  if (preferences.putUChar(NVS_KEY_STAGE, STAGE_PROGRAM_B_READY) !=
      sizeof(uint8_t))
  {
    Serial.println("FAIL: no se pudo registrar la etapa B en NVS.");
    return false;
  }

  Serial.println("[PASS] Programa B persistido en el slot inactivo.");
  Serial.println("ETAPA 2 COMPLETA.");
  Serial.println("Pulsa RESET o corta y restablece la alimentacion otra vez.");
  return true;
}

static bool runStageProgramBReady()
{
  Serial.println("Etapa 3/3: cargar Programa B y restaurar la ventana.");

  if (!loadBackupFromNVS())
  {
    Serial.println("FAIL: respaldo NVS ausente o corrupto.");
    return false;
  }

  LogicProgramStore store;
  if (!store.begin(storage, TEST_PROFILE) ||
      !store.loadActive(loadedProgram, scratch, sizeof(scratch)) ||
      !validateLoadedProgram(0xB002, 2, PROGRAM_B.name))
  {
    Serial.println("FAIL: Programa B no sobrevivio al reinicio.");
    return false;
  }

  Serial.println("[PASS] Programa B cargado despues del reinicio.");

  if (preferences.putUChar(NVS_KEY_STAGE, STAGE_RESTORE_PENDING) !=
      sizeof(uint8_t))
  {
    Serial.println("FAIL: no se pudo registrar la restauracion pendiente.");
    return false;
  }

  return restoreOriginalWindow();
}

void setup()
{
  Serial.begin(115200);
  Serial.setTimeout(250);
  delay(300);

  Serial.println();
  Serial.println("JWPLC Logic Runtime - PoC 6 persistencia entre reinicios");
  Serial.println("No inicializa E/S ni conmuta salidas.");
  Serial.println();

  Serial.print("FRAM detectada: ");
  Serial.print(JWPLC_FRAM.size());
  Serial.println(" bytes");
  Serial.print("Ventana persistente: 0x");
  Serial.print(TEST_BASE_ADDRESS, HEX);
  Serial.print("..0x");
  Serial.println(TEST_BASE_ADDRESS + TEST_WINDOW_BYTES - 1, HEX);

  if (JWPLC_FRAM.size() != PHYSICAL_FRAM_BYTES || !storage.begin())
  {
    Serial.println("PRUEBA ABORTADA: FRAM o ventana no disponibles.");
    return;
  }

  if (!preferences.begin(NVS_NAMESPACE, false))
  {
    Serial.println("PRUEBA ABORTADA: no se pudo abrir NVS.");
    return;
  }

  uint8_t stage = preferences.getUChar(NVS_KEY_STAGE, STAGE_IDLE);

  if (stage == STAGE_IDLE)
  {
    if (!waitForStartConfirmation())
    {
      return;
    }

    if (!createPersistentBackup())
    {
      Serial.println("PRUEBA ABORTADA: no se pudo crear el respaldo persistente.");
      return;
    }

    Serial.println("[PASS] Respaldo persistente guardado en NVS.");
    stage = STAGE_BACKUP_READY;
  }

  switch (stage)
  {
  case STAGE_BACKUP_READY:
    (void)runStageBackupReady();
    break;

  case STAGE_PROGRAM_A_READY:
    (void)runStageProgramAReady();
    break;

  case STAGE_PROGRAM_B_READY:
    (void)runStageProgramBReady();
    break;

  case STAGE_RESTORE_PENDING:
    Serial.println("Restauracion pendiente detectada; reintentando.");
    (void)restoreOriginalWindow();
    break;

  default:
    Serial.println("PRUEBA ABORTADA: etapa NVS desconocida.");
    Serial.println("No se modifico la FRAM en este arranque.");
    break;
  }
}

void loop()
{
}
