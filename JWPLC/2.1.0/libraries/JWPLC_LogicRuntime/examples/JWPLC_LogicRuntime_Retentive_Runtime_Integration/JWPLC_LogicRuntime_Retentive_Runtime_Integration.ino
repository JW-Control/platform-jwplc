#include <JWPLC_GlobalPeripherals.h>
#include <JWPLC_LogicRuntime.h>
#include <Preferences.h>

#include <cstring>

static constexpr size_t FRAM_BYTES =
    JWPLCLogicStorageLayouts::FRAM_8K.totalBytes;
static constexpr size_t RETAIN_ADDRESS =
    JWPLCLogicStorageLayouts::FRAM_8K.retain.address;
static constexpr size_t RETAIN_BYTES =
    JWPLCLogicStorageLayouts::FRAM_8K.retain.size;
static constexpr size_t OWNED_BYTES =
    JWPLCLogicStorageLayouts::FRAM_8K.retain.endExclusive();
static constexpr size_t RETENTIVE_COPY_BYTES =
    RETAIN_BYTES / LogicRetentiveStore::COPY_COUNT;
static constexpr size_t HEADER_CRC_OFFSET = 28;
static constexpr size_t VERIFY_CHUNK_BYTES = 64;
static constexpr size_t GUARD_BYTES = 16;
static constexpr uint8_t RETENTIVE_BLOCK_INDEX = 2;

static constexpr char NVS_NAMESPACE[] = "jwlr_ret_flow";
static constexpr char NVS_KEY_STAGE[] = "stage";
static constexpr char NVS_KEY_BACKUP[] = "backup";
static constexpr char NVS_KEY_CRC[] = "crc";

static constexpr uint8_t STAGE_IDLE = 0;
static constexpr uint8_t STAGE_RESTORE_PENDING = 1;

static constexpr uint32_t PROGRAM_A_ID = 0xA001;
static constexpr uint32_t PROGRAM_B_ID = 0xB002;

static uint8_t backupBytes[OWNED_BYTES];
static uint8_t verifyChunk[VERIFY_CHUNK_BYTES];
static uint8_t guardAfter[GUARD_BYTES];

static Preferences preferences;
static LogicFRAMStorage rawStorage(JWPLC_FRAM, 0, FRAM_BYTES);
static JWPLC_LogicRuntime runtime;

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
    {LogicBlockType::SetReset,
     0,
     1,
     0,
     0,
     JWPLC_LOGIC_BLOCK_FLAG_RETENTIVE},
    {LogicBlockType::SetReset,
     0,
     1,
     0,
     0},
    {LogicBlockType::Ton,
     0,
     JWPLC_LOGIC_NO_SOURCE,
     0,
     2000}};

static const LogicProgram PROGRAM_A = {
    "Retentivo A",
    PROGRAM_A_BLOCKS,
    static_cast<uint16_t>(sizeof(PROGRAM_A_BLOCKS) /
                          sizeof(PROGRAM_A_BLOCKS[0]))};

static const LogicBlockDefinition PROGRAM_B_BLOCKS[] = {
    {LogicBlockType::DigitalInput,
     JWPLC_LOGIC_NO_SOURCE,
     JWPLC_LOGIC_NO_SOURCE,
     2,
     0},
    {LogicBlockType::DigitalInput,
     JWPLC_LOGIC_NO_SOURCE,
     JWPLC_LOGIC_NO_SOURCE,
     3,
     0},
    {LogicBlockType::SetReset,
     0,
     1,
     0,
     0,
     JWPLC_LOGIC_BLOCK_FLAG_RETENTIVE},
    {LogicBlockType::SetReset,
     0,
     1,
     0,
     0},
    {LogicBlockType::Ton,
     0,
     JWPLC_LOGIC_NO_SOURCE,
     0,
     3000}};

static const LogicProgram PROGRAM_B = {
    "Retentivo B",
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

  return preferences.putUInt(NVS_KEY_CRC, backupCrc) == sizeof(uint32_t);
}

static bool verifyOwnedRegionMatchesBackup()
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

static bool regionContains(size_t address,
                           size_t length,
                           uint8_t value)
{
  size_t offset = 0;

  while (offset < length)
  {
    const size_t remaining = length - offset;
    const size_t chunkLength =
        remaining < sizeof(verifyChunk) ? remaining : sizeof(verifyChunk);

    if (!rawStorage.read(address + offset,
                         verifyChunk,
                         chunkLength))
    {
      return false;
    }

    for (size_t index = 0; index < chunkLength; ++index)
    {
      if (verifyChunk[index] != value)
      {
        return false;
      }
    }

    offset += chunkLength;
  }

  return true;
}

static bool captureGuardAfter()
{
  return rawStorage.read(OWNED_BYTES,
                         guardAfter,
                         sizeof(guardAfter));
}

static bool guardAfterUnchanged()
{
  uint8_t current[GUARD_BYTES] = {};
  return rawStorage.read(OWNED_BYTES,
                         current,
                         sizeof(current)) &&
         memcmp(current, guardAfter, sizeof(current)) == 0;
}

static bool corruptRetentiveCopyBHeaderCrc()
{
  const size_t address =
      RETAIN_ADDRESS + RETENTIVE_COPY_BYTES + HEADER_CRC_OFFSET;
  uint8_t value = 0;

  if (!rawStorage.read(address, &value, 1))
  {
    return false;
  }

  value ^= 0x01U;
  return rawStorage.write(address, &value, 1);
}

static bool restoreOriginalRegion(bool printResult)
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
    expect("restauracion del mapa persistente escrita", writeOk);
  }

  if (!writeOk)
  {
    return false;
  }

  const bool verifyOk = verifyOwnedRegionMatchesBackup();
  if (printResult)
  {
    expect("contenido persistente original restaurado exactamente", verifyOk);
  }

  if (!verifyOk)
  {
    return false;
  }

  const bool storageBeginOk = runtime.storage().begin(JWPLC_FRAM);
  if (printResult)
  {
    expect("fachada reinicializada despues de restaurar", storageBeginOk);
    expect("politica recupera estado original sin formato",
           storageBeginOk &&
               runtime.storage().prepareBoot() ==
                   JWPLCLogicStorageBootState::Unformatted);
  }

  if (!storageBeginOk)
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

static bool loadedIdentity(uint32_t expectedProgramId,
                           uint32_t &generation)
{
  uint32_t programId = 0;
  uint16_t blockCount = 0;
  generation = 0;

  return runtime.storage().loadedProgramIdentity(programId,
                                                 generation,
                                                 blockCount) &&
         programId == expectedProgramId &&
         blockCount == 5;
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
                     ? "INTEGRACION RETENTIVA DEL RUNTIME: PASS"
                     : "INTEGRACION RETENTIVA DEL RUNTIME: FAIL");
}

void setup()
{
  Serial.begin(115200);
  Serial.setTimeout(250);
  delay(300);

  Serial.println();
  Serial.println("JWPLC Logic Runtime - integracion retentiva de alto nivel");
  Serial.println("Inicializa E/S, mantiene Q0 apagadas y restaura todo al final.");
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
    Serial.println("Se recuperara el mapa persistente original.");

    if (restoreOriginalRegion(false))
    {
      Serial.println("[PASS] Recuperacion automatica completada.");
    }
    else
    {
      Serial.println("[FAIL] No se pudo completar la recuperacion automatica.");
    }
    return;
  }

  expect("backend FRAM inicializado", rawStorage.isReady());
  expect("region administrada termina en 0x1A40", OWNED_BYTES == 0x1A40);

  const bool guardOk = captureGuardAfter();
  expect("guarda posterior capturada", guardOk);

  const bool storageBeginOk = runtime.storage().begin(JWPLC_FRAM);
  expect("storage().begin(JWPLC_FRAM)", storageBeginOk);

  const bool runtimeBeginOk =
      runtime.begin(JWPLCLogicStorageProfiles::FRAM_8K.framBytes);
  expect("runtime.begin() inicializa E/S", runtimeBeginOk);
  expect("estado original permanece UNFORMATTED",
         storageBeginOk &&
             runtime.storage().prepareBoot() ==
                 JWPLCLogicStorageBootState::Unformatted);

  if (!guardOk || !storageBeginOk || !runtimeBeginOk)
  {
    Serial.println();
    Serial.println("PRUEBA ABORTADA: no se modifico la FRAM.");
    printSummary();
    return;
  }

  Serial.println();
  Serial.println("ADVERTENCIA: esta prueba modificara temporalmente:");
  Serial.println("0x0000..0x1A3F (programas A/B + retentivos).");
  Serial.println("No tocara la reserva 0x1A40..0x1FFF.");
  Serial.println("El contenido original se respaldara y restaurara.");
  Serial.println("Escribe RETRUNTIME y pulsa Enviar para continuar.");

  if (!waitForCommand("RETRUNTIME"))
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
  expect("program store formateado explicitamente", formatOk);

  const bool clearRetainOk =
      rawStorage.fill(RETAIN_ADDRESS, 0xFF, RETAIN_BYTES);
  expect("region retentiva limpiada temporalmente", clearRetainOk);
  expect("limpieza retentiva verificada",
         clearRetainOk &&
             regionContains(RETAIN_ADDRESS, RETAIN_BYTES, 0xFF));

  const bool saveAOk =
      formatOk && clearRetainOk &&
      runtime.storage().save(PROGRAM_A, PROGRAM_A_ID);
  expect("Programa A guardado", saveAOk);

  const JWPLCLogicStorageBootState bootA =
      runtime.prepareStoredProgram();
  expect("Programa A preparado como activo",
         bootA == JWPLCLogicStorageBootState::ActiveProgramLoaded);

  uint32_t generationA = 0;
  const bool identityAOk = loadedIdentity(PROGRAM_A_ID, generationA);
  expect("identidad cargada de A disponible", identityAOk);
  expect("Programa A usa generacion valida", identityAOk && generationA > 0);
  expect("Programa A contiene un bloque retentivo",
         runtime.retentiveBlockCount() == 1);

  expect("A inicia sin snapshot compatible",
         runtime.restoreStoredRetentiveState() ==
             JWPLCLogicRetentiveState::NoSnapshot);
  expect("A permanece falso sin snapshot",
         !runtime.blockValue(RETENTIVE_BLOCK_INDEX));

  const uint8_t activeBitmap[1] = {
      static_cast<uint8_t>(1U << RETENTIVE_BLOCK_INDEX)};
  expect("estado A se activa en RAM",
         runtime.importRetentiveState(activeBitmap,
                                      sizeof(activeBitmap)));
  expect("SET/RESET retentivo A queda verdadero",
         runtime.blockValue(RETENTIVE_BLOCK_INDEX));
  expect("snapshot A se guarda mediante API de alto nivel",
         runtime.saveStoredRetentiveState());
  expect("estado retentivo informa SAVED",
         runtime.retentiveState() ==
             JWPLCLogicRetentiveState::Saved);
  expect("snapshot A conserva identidad",
         runtime.retentiveStoreStatus().programId == PROGRAM_A_ID &&
             runtime.retentiveStoreStatus().generation == generationA);

  runtime.stop();
  expect("stop limpia el valor A en RAM",
         !runtime.blockValue(RETENTIVE_BLOCK_INDEX));

  expect("Programa A puede prepararse nuevamente",
         runtime.prepareStoredProgram() ==
             JWPLCLogicStorageBootState::ActiveProgramLoaded);
  expect("snapshot A se restaura mediante API de alto nivel",
         runtime.restoreStoredRetentiveState() ==
             JWPLCLogicRetentiveState::Restored);
  expect("valor A reaparece antes de start",
         runtime.blockValue(RETENTIVE_BLOCK_INDEX));

  const bool saveBOk = runtime.storage().save(PROGRAM_B, PROGRAM_B_ID);
  expect("Programa B guardado", saveBOk);
  expect("Programa B preparado como activo",
         runtime.prepareStoredProgram() ==
             JWPLCLogicStorageBootState::ActiveProgramLoaded);

  uint32_t generationB = 0;
  const bool identityBOk = loadedIdentity(PROGRAM_B_ID, generationB);
  expect("identidad cargada de B disponible", identityBOk);
  expect("Programa B usa una generacion nueva",
         identityBOk && generationB != generationA);

  expect("snapshot A no se aplica a Programa B",
         runtime.restoreStoredRetentiveState() ==
             JWPLCLogicRetentiveState::NoMatchingSnapshot);
  expect("B permanece falso ante identidad distinta",
         !runtime.blockValue(RETENTIVE_BLOCK_INDEX));

  expect("estado B se activa en RAM",
         runtime.importRetentiveState(activeBitmap,
                                      sizeof(activeBitmap)));
  expect("snapshot B se guarda mediante API de alto nivel",
         runtime.saveStoredRetentiveState());
  expect("snapshot B queda en copia 1",
         runtime.retentiveStoreStatus().activeCopy == 1);

  runtime.stop();
  expect("stop limpia el valor B en RAM",
         !runtime.blockValue(RETENTIVE_BLOCK_INDEX));
  expect("Programa B puede prepararse nuevamente",
         runtime.prepareStoredProgram() ==
             JWPLCLogicStorageBootState::ActiveProgramLoaded);
  expect("snapshot B se restaura",
         runtime.restoreStoredRetentiveState() ==
             JWPLCLogicRetentiveState::Restored);
  expect("valor B reaparece antes de start",
         runtime.blockValue(RETENTIVE_BLOCK_INDEX));

  const bool rollbackAOk = runtime.storage().rollback();
  expect("rollback activa nuevamente Programa A", rollbackAOk);
  expect("Programa A se prepara despues de rollback",
         runtime.prepareStoredProgram() ==
             JWPLCLogicStorageBootState::ActiveProgramLoaded);

  uint32_t rollbackGenerationA = 0;
  expect("rollback recupera identidad A",
         loadedIdentity(PROGRAM_A_ID, rollbackGenerationA) &&
             rollbackGenerationA == generationA);
  expect("snapshot A vuelve a restaurarse",
         runtime.restoreStoredRetentiveState() ==
             JWPLCLogicRetentiveState::Restored);
  expect("valor A restaurado queda verdadero",
         runtime.blockValue(RETENTIVE_BLOCK_INDEX));

  const bool rollbackBOk = runtime.storage().rollback();
  expect("segundo rollback reactiva Programa B", rollbackBOk);
  expect("Programa B se prepara despues del segundo rollback",
         runtime.prepareStoredProgram() ==
             JWPLCLogicStorageBootState::ActiveProgramLoaded);

  uint32_t rollbackGenerationB = 0;
  expect("segundo rollback recupera identidad B",
         loadedIdentity(PROGRAM_B_ID, rollbackGenerationB) &&
             rollbackGenerationB == generationB);

  const bool corruptBOk = corruptRetentiveCopyBHeaderCrc();
  expect("CRC del snapshot B se corrompe de forma controlada",
         corruptBOk);
  expect("B corrupto no encuentra snapshot compatible",
         corruptBOk &&
             runtime.restoreStoredRetentiveState() ==
                 JWPLCLogicRetentiveState::NoMatchingSnapshot);
  expect("estado B se limpia ante snapshot corrupto",
         !runtime.blockValue(RETENTIVE_BLOCK_INDEX));
  expect("store retentivo conserva copia A valida",
         runtime.retentiveStoreStatus().activeCopy == 0);

  expect("guarda posterior permanece intacta",
         guardAfterUnchanged());

  (void)restoreOriginalRegion(true);
  printSummary();
}

void loop()
{
}
