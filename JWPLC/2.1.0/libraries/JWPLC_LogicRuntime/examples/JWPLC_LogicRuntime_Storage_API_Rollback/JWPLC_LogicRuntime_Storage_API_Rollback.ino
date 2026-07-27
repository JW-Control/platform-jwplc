#include <JWPLC_GlobalPeripherals.h>
#include <JWPLC_LogicRuntime.h>
#include <Preferences.h>

#include <cstring>

static constexpr size_t PROGRAM_STORE_BYTES =
    JWPLCLogicStorageLayouts::FRAM_8K.programStoreBytes();
static constexpr size_t VERIFY_CHUNK_BYTES = 64;

static constexpr char NVS_NAMESPACE[] = "jwlr_api_rb";
static constexpr char NVS_KEY_STAGE[] = "stage";
static constexpr char NVS_KEY_BACKUP[] = "backup";
static constexpr char NVS_KEY_CRC[] = "crc";

static constexpr uint8_t STAGE_IDLE = 0;
static constexpr uint8_t STAGE_RESTORE_PENDING = 1;

static constexpr uint32_t PROGRAM_A_ID = 0xA001;
static constexpr uint32_t PROGRAM_B_ID = 0xB002;

static uint8_t backupBytes[PROGRAM_STORE_BYTES];
static uint8_t verifyChunk[VERIFY_CHUNK_BYTES];

static Preferences preferences;
static JWPLC_LogicRuntime runtime;
static LogicFRAMStorage rawStorage(JWPLC_FRAM, 0, PROGRAM_STORE_BYTES);

static uint16_t passedTests = 0;
static uint16_t failedTests = 0;

static const LogicBlockDefinition PROGRAM_A_BLOCKS[] = {
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

static const LogicProgram PROGRAM_A = {
    "Rollback A",
    PROGRAM_A_BLOCKS,
    static_cast<uint16_t>(sizeof(PROGRAM_A_BLOCKS) /
                          sizeof(PROGRAM_A_BLOCKS[0]))};

static const LogicBlockDefinition PROGRAM_B_BLOCKS[] = {
    {LogicBlockType::DigitalInput,
     JWPLC_LOGIC_NO_SOURCE,
     JWPLC_LOGIC_NO_SOURCE,
     2,
     0},
    {LogicBlockType::Not,
     0,
     JWPLC_LOGIC_NO_SOURCE,
     0,
     0},
    {LogicBlockType::DigitalOutput,
     1,
     JWPLC_LOGIC_NO_SOURCE,
     1,
     0}};

static const LogicProgram PROGRAM_B = {
    "Rollback B",
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

static bool waitForCommand(const char *expected)
{
  for (;;)
  {
    if (Serial.available() > 0)
    {
      String command = Serial.readStringUntil('\n');
      command.trim();

      if (command == expected)
      {
        return true;
      }

      if (command.length() > 0)
      {
        Serial.print("Comando no reconocido. Escribe exactamente ");
        Serial.print(expected);
        Serial.println('.');
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

  if (preferences.getBytes(NVS_KEY_BACKUP,
                           backupBytes,
                           sizeof(backupBytes)) != sizeof(backupBytes))
  {
    return false;
  }

  const uint32_t expectedCrc = preferences.getUInt(NVS_KEY_CRC, 0);
  return LogicProgramCodec::crc32(backupBytes, sizeof(backupBytes)) ==
         expectedCrc;
}

static bool createPersistentBackup()
{
  if (!rawStorage.read(0, backupBytes, sizeof(backupBytes)))
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

  return true;
}

static bool verifyRestoredBytes()
{
  size_t offset = 0;

  while (offset < sizeof(backupBytes))
  {
    const size_t remaining = sizeof(backupBytes) - offset;
    const size_t chunkLength =
        remaining < sizeof(verifyChunk) ? remaining : sizeof(verifyChunk);

    if (!rawStorage.read(offset, verifyChunk, chunkLength) ||
        memcmp(backupBytes + offset, verifyChunk, chunkLength) != 0)
    {
      return false;
    }

    offset += chunkLength;
  }

  return true;
}

static bool restoreOriginalStore(bool printResult)
{
  if (!loadBackupFromNVS())
  {
    Serial.println("FAIL: respaldo NVS ausente o corrupto.");
    return false;
  }

  const bool writeOk =
      rawStorage.write(0, backupBytes, sizeof(backupBytes));
  if (printResult)
  {
    expect("restauracion del mapa A/B escrita", writeOk);
  }

  if (!writeOk)
  {
    return false;
  }

  const bool verifyOk = verifyRestoredBytes();
  if (printResult)
  {
    expect("contenido original restaurado exactamente", verifyOk);
  }

  if (!verifyOk)
  {
    return false;
  }

  const bool beginOk = runtime.storage().begin(JWPLC_FRAM);
  if (printResult)
  {
    expect("fachada reinicializada despues de restaurar", beginOk);
    expect("estado original sin formato recuperado",
           beginOk && !runtime.storage().isFormatted());
  }

  if (!beginOk || runtime.storage().isFormatted())
  {
    return false;
  }

  const bool clearOk = preferences.clear();
  if (printResult)
  {
    expect("respaldo temporal NVS eliminado", clearOk);
  }

  return clearOk;
}

static bool loadedMatchesA()
{
  const LogicProgram loaded = runtime.storage().activeProgram();

  return loaded.name != nullptr &&
         loaded.blocks != nullptr &&
         strcmp(loaded.name, PROGRAM_A.name) == 0 &&
         loaded.blockCount == PROGRAM_A.blockCount &&
         loaded.blocks[4].type == LogicBlockType::Ton &&
         loaded.blocks[4].parameter == 2000 &&
         loaded.blocks[5].type == LogicBlockType::DigitalOutput &&
         loaded.blocks[5].resource == 0;
}

static bool loadedMatchesB()
{
  const LogicProgram loaded = runtime.storage().activeProgram();

  return loaded.name != nullptr &&
         loaded.blocks != nullptr &&
         strcmp(loaded.name, PROGRAM_B.name) == 0 &&
         loaded.blockCount == PROGRAM_B.blockCount &&
         loaded.blocks[0].type == LogicBlockType::DigitalInput &&
         loaded.blocks[0].resource == 2 &&
         loaded.blocks[2].type == LogicBlockType::DigitalOutput &&
         loaded.blocks[2].resource == 1;
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
                     ? "ROLLBACK PERSISTENTE: PASS"
                     : "ROLLBACK PERSISTENTE: FAIL");
}

void setup()
{
  Serial.begin(115200);
  Serial.setTimeout(250);
  delay(300);

  Serial.println();
  Serial.println("JWPLC Logic Runtime - rollback persistente reversible");
  Serial.println("No inicializa E/S ni conmuta salidas.");
  Serial.println();

  if (!preferences.begin(NVS_NAMESPACE, false))
  {
    Serial.println("PRUEBA ABORTADA: no se pudo abrir NVS.");
    return;
  }

  if (!rawStorage.begin())
  {
    Serial.println("PRUEBA ABORTADA: backend FRAM no disponible.");
    return;
  }

  const uint8_t stage = preferences.getUChar(NVS_KEY_STAGE, STAGE_IDLE);
  if (stage == STAGE_RESTORE_PENDING)
  {
    Serial.println("Restauracion pendiente detectada tras reinicio.");
    Serial.println("Se recuperara el contenido original antes de continuar.");

    if (restoreOriginalStore(false))
    {
      Serial.println("[PASS] Recuperacion automatica completada.");
    }
    else
    {
      Serial.println("[FAIL] No se pudo completar la recuperacion automatica.");
    }
    return;
  }

  expect("backend de respaldo inicializado", rawStorage.isReady());

  const bool beginOk = runtime.storage().begin(JWPLC_FRAM);
  expect("storage().begin(JWPLC_FRAM)", beginOk);
  expect("fachada queda lista", beginOk && runtime.storage().isReady());

  if (!beginOk)
  {
    printSummary();
    return;
  }

  const bool initiallyUnformatted = !runtime.storage().isFormatted();
  expect("mapa completo aun no esta formateado", initiallyUnformatted);

  if (!initiallyUnformatted)
  {
    Serial.println();
    Serial.println("PRUEBA ABORTADA: ya existe un formato runtime valido.");
    Serial.println("No se modifico la FRAM.");
    printSummary();
    return;
  }

  Serial.println();
  Serial.println("ADVERTENCIA: esta prueba modificara temporalmente:");
  Serial.println("0x0000..0x143F (5184 bytes: superblocks + Slots A/B).");
  Serial.println("Guardara A, luego B, y reactivara A mediante rollback.");
  Serial.println("El contenido original se respaldara y restaurara.");
  Serial.println("Escribe ROLLBACK y pulsa Enviar para continuar.");

  if (!waitForCommand("ROLLBACK"))
  {
    return;
  }

  const bool backupOk = createPersistentBackup();
  expect("respaldo persistente guardado en NVS", backupOk);
  expect("respaldo NVS verificado por CRC",
         backupOk && loadBackupFromNVS());

  if (!backupOk)
  {
    printSummary();
    return;
  }

  const bool stageOk =
      preferences.putUChar(NVS_KEY_STAGE, STAGE_RESTORE_PENDING) ==
      sizeof(uint8_t);
  expect("restauracion pendiente registrada", stageOk);

  if (!stageOk)
  {
    printSummary();
    return;
  }

  const bool formatOk = runtime.storage().format();
  expect("storage().format() explicito", formatOk);
  expect("fachada reporta formato valido",
         formatOk && runtime.storage().isFormatted());

  const bool saveAOk =
      formatOk && runtime.storage().save(PROGRAM_A, PROGRAM_A_ID);
  expect("Programa A guardado", saveAOk);

  const LogicProgramStoreStatus statusA = runtime.storage().status();
  expect("Programa A activa Slot A",
         saveAOk && statusA.activeSlot == 0);
  expect("Programa A conserva ID",
         saveAOk && statusA.programId == PROGRAM_A_ID);
  expect("Programa A usa generacion 1",
         saveAOk && statusA.generation == 1);

  const bool saveBOk =
      saveAOk && runtime.storage().save(PROGRAM_B, PROGRAM_B_ID);
  expect("Programa B guardado", saveBOk);

  const LogicProgramStoreStatus statusB = runtime.storage().status();
  expect("Programa B activa Slot B",
         saveBOk && statusB.activeSlot == 1);
  expect("Programa B conserva ID",
         saveBOk && statusB.programId == PROGRAM_B_ID);
  expect("Programa B usa generacion 2",
         saveBOk && statusB.generation == 2);

  const bool loadBOk = saveBOk && runtime.storage().loadActive();
  expect("Programa B carga como activo", loadBOk);
  expect("Programa B conserva su contenido",
         loadBOk && loadedMatchesB());

  const bool rollbackOk = loadBOk && runtime.storage().rollback();
  expect("storage().rollback() reactiva candidato verificado", rollbackOk);

  const LogicProgramStoreStatus rollbackStatus = runtime.storage().status();
  expect("rollback reactiva Slot A",
         rollbackOk && rollbackStatus.activeSlot == 0);
  expect("rollback recupera ID y generacion de A",
         rollbackOk && rollbackStatus.programId == PROGRAM_A_ID &&
             rollbackStatus.generation == 1);
  expect("rollback deja Programa A cargado en RAM",
         rollbackOk && runtime.storage().hasLoadedProgram() &&
             loadedMatchesA());
  expect("rollback avanza la secuencia del superblock",
         rollbackOk && rollbackStatus.sequence == 4);

  const bool reopenOk = runtime.storage().begin(JWPLC_FRAM);
  expect("storage().begin() reabre el mapa tras rollback", reopenOk);
  expect("formato sigue valido tras reabrir",
         reopenOk && runtime.storage().isFormatted());

  const LogicProgramStoreStatus reopenedStatus = runtime.storage().status();
  expect("Slot A permanece activo tras reinicializar",
         reopenOk && reopenedStatus.activeSlot == 0 &&
             reopenedStatus.programId == PROGRAM_A_ID &&
             reopenedStatus.generation == 1 &&
             reopenedStatus.sequence == 4);

  const bool reloadAOk = reopenOk && runtime.storage().loadActive();
  expect("Programa A vuelve a cargar tras reinicializar", reloadAOk);
  expect("Programa A reabierto conserva su contenido",
         reloadAOk && loadedMatchesA());

  (void)restoreOriginalStore(true);
  printSummary();
}

void loop()
{
}
