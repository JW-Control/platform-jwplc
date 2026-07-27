#include <JWPLC_GlobalPeripherals.h>
#include <JWPLC_LogicRuntime.h>
#include <Preferences.h>

#include <cstring>

static constexpr size_t PROGRAM_STORE_BYTES =
    JWPLCLogicStorageLayouts::FRAM_8K.programStoreBytes();
static constexpr size_t SUPERBLOCK_AREA_BYTES =
    LogicStorageLayout::SUPERBLOCK_AREA_BYTES;
static constexpr size_t SUPERBLOCK_COPY_BYTES = 32;
static constexpr size_t SUPERBLOCK_CRC_OFFSET = 28;
static constexpr size_t VERIFY_CHUNK_BYTES = 64;

static constexpr char NVS_NAMESPACE[] = "jwlr_meta_diag";
static constexpr char NVS_KEY_STAGE[] = "stage";
static constexpr char NVS_KEY_BACKUP[] = "backup";
static constexpr char NVS_KEY_CRC[] = "crc";

static constexpr uint8_t STAGE_IDLE = 0;
static constexpr uint8_t STAGE_RESTORE_PENDING = 1;

static uint8_t backupBytes[PROGRAM_STORE_BYTES];
static uint8_t verifyChunk[VERIFY_CHUNK_BYTES];
static uint8_t corruptedSuperblocks[SUPERBLOCK_AREA_BYTES];
static uint8_t superblockVerify[SUPERBLOCK_AREA_BYTES];

static Preferences preferences;
static JWPLC_LogicRuntime runtime;
static LogicFRAMStorage rawStorage(JWPLC_FRAM, 0, PROGRAM_STORE_BYTES);

static uint16_t passedTests = 0;
static uint16_t failedTests = 0;

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

static bool corruptSuperblockCrc(uint8_t copyIndex)
{
  if (copyIndex > 1)
  {
    return false;
  }

  const size_t address =
      static_cast<size_t>(copyIndex) * SUPERBLOCK_COPY_BYTES +
      SUPERBLOCK_CRC_OFFSET;
  uint8_t value = 0;

  if (!rawStorage.read(address, &value, 1))
  {
    return false;
  }

  value ^= 0x01U;
  return rawStorage.write(address, &value, 1);
}

static bool captureCorruptedSuperblocks()
{
  return rawStorage.read(0,
                         corruptedSuperblocks,
                         sizeof(corruptedSuperblocks));
}

static bool superblocksRemainUnchanged()
{
  if (!rawStorage.read(0,
                       superblockVerify,
                       sizeof(superblockVerify)))
  {
    return false;
  }

  return memcmp(corruptedSuperblocks,
                superblockVerify,
                sizeof(corruptedSuperblocks)) == 0;
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
  }

  if (!beginOk)
  {
    return false;
  }

  const bool originalUnformatted =
      runtime.storage().metadataHealth() ==
      LogicSuperblockHealth::Unformatted;
  if (printResult)
  {
    expect("diagnostico recupera estado original sin formato",
           originalUnformatted);
  }

  if (!originalUnformatted)
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

static void printSummary()
{
  Serial.println();
  Serial.print("Resultado: ");
  Serial.print(passedTests);
  Serial.print(" PASS, ");
  Serial.print(failedTests);
  Serial.println(" FAIL");

  Serial.println(failedTests == 0
                     ? "DIAGNOSTICO PUBLICO DE METADATA: PASS"
                     : "DIAGNOSTICO PUBLICO DE METADATA: FAIL");
}

void setup()
{
  Serial.begin(115200);
  Serial.setTimeout(250);
  delay(5000);

  Serial.println();
  Serial.println("JWPLC Logic Runtime - diagnostico publico de metadata");
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
  expect("FRAM original reporta metadata UNFORMATTED",
         runtime.storage().metadataHealth() ==
             LogicSuperblockHealth::Unformatted);
  expect("politica original informa UNFORMATTED",
         runtime.storage().prepareBoot() ==
             JWPLCLogicStorageBootState::Unformatted);

  if (!beginOk || runtime.storage().isFormatted())
  {
    Serial.println();
    Serial.println("PRUEBA ABORTADA: el estado inicial no es el esperado.");
    Serial.println("No se modifico la FRAM.");
    printSummary();
    return;
  }

  Serial.println();
  Serial.println("ADVERTENCIA: esta prueba modificara temporalmente:");
  Serial.println("0x0000..0x143F (5184 bytes: superblocks + Slots A/B).");
  Serial.println("Validara CORRUPT_METADATA en la API publica.");
  Serial.println("El contenido original se respaldara y restaurara.");
  Serial.println("Escribe METADATA y pulsa Enviar para continuar.");

  if (!waitForCommand("METADATA"))
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
  expect("formato valido actualiza metadataHealth a VALID",
         runtime.storage().metadataHealth() ==
             LogicSuperblockHealth::Valid);
  expect("almacenamiento recien formateado informa EMPTY",
         runtime.storage().prepareBoot() ==
             JWPLCLogicStorageBootState::Empty);

  const bool corrupt0Ok = formatOk && corruptSuperblockCrc(0);
  expect("copia 0 corrompida de forma controlada", corrupt0Ok);

  const bool corrupt1Ok = corrupt0Ok && corruptSuperblockCrc(1);
  expect("copia 1 corrompida de forma controlada", corrupt1Ok);

  const bool captureOk = corrupt1Ok && captureCorruptedSuperblocks();
  expect("metadata corrupta capturada para verificar solo lectura", captureOk);

  const bool reopenOk =
      captureOk && runtime.storage().begin(JWPLC_FRAM);
  expect("fachada reabre con metadata corrupta", reopenOk);
  expect("fachada permanece lista", reopenOk && runtime.storage().isReady());
  expect("metadataHealth informa CORRUPT_METADATA",
         reopenOk &&
             runtime.storage().metadataHealth() ==
                 LogicSuperblockHealth::CorruptMetadata);
  expect("metadata corrupta no se declara formateada",
         reopenOk && !runtime.storage().isFormatted());

  const JWPLCLogicStorageBootState bootState =
      runtime.storage().prepareBoot();
  expect("prepareBoot informa CORRUPT_METADATA",
         bootState == JWPLCLogicStorageBootState::CorruptMetadata);
  expect("bootState conserva CORRUPT_METADATA",
         runtime.storage().bootState() ==
             JWPLCLogicStorageBootState::CorruptMetadata);
  expect("bootStateName expone CORRUPT_METADATA",
         strcmp(JWPLCLogicStorage::bootStateName(bootState),
                "CORRUPT_METADATA") == 0);
  expect("lastError informa CORRUPT_METADATA",
         runtime.storage().lastError() ==
             JWPLCLogicStorageError::CorruptMetadata);
  expect("errorName expone CORRUPT_METADATA",
         strcmp(JWPLCLogicStorage::errorName(runtime.storage().lastError()),
                "CORRUPT_METADATA") == 0);
  expect("metadata corrupta descarga cualquier programa",
         !runtime.storage().hasLoadedProgram());
  expect("metadata corrupta no ofrece programa arrancable",
         !runtime.storage().hasBootableProgram());

  const JWPLCLogicStorageBootState runtimeBootState =
      runtime.prepareStoredProgram();
  expect("runtime propaga CORRUPT_METADATA",
         runtimeBootState ==
             JWPLCLogicStorageBootState::CorruptMetadata);
  expect("runtime no conserva programa ante metadata corrupta",
         !runtime.hasProgram());
  expect("runtime informa fallo de carga persistente",
         runtime.lastError() ==
             JWPLCLogicRuntimeError::StoredProgramLoadFailed);

  expect("evaluar metadata corrupta no escribe superblocks",
         captureOk && superblocksRemainUnchanged());

  (void)restoreOriginalStore(true);
  printSummary();
}

void loop()
{
}
