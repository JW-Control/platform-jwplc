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
static constexpr size_t COPY_BYTES =
    RETAIN_BYTES / LogicRetentiveStore::COPY_COUNT;
static constexpr size_t HEADER_CRC_OFFSET = 28;
static constexpr size_t VERIFY_CHUNK_BYTES = 64;
static constexpr size_t GUARD_BYTES = 16;

static constexpr char NVS_NAMESPACE[] = "jwlr_ret_phys";
static constexpr char NVS_KEY_STAGE[] = "stage";
static constexpr char NVS_KEY_BACKUP[] = "backup";
static constexpr char NVS_KEY_CRC[] = "crc";

static constexpr uint8_t STAGE_IDLE = 0;
static constexpr uint8_t STAGE_RESTORE_PENDING = 1;

static constexpr uint32_t PROGRAM_ID = 0xA001;
static constexpr uint32_t PROGRAM_GENERATION = 7;
static constexpr uint16_t PROGRAM_BLOCKS = 5;
static constexpr uint8_t SNAPSHOT_A = 0x08;
static constexpr uint8_t SNAPSHOT_B = 0x00;

static uint8_t backupBytes[RETAIN_BYTES];
static uint8_t verifyChunk[VERIFY_CHUNK_BYTES];
static uint8_t guardBefore[GUARD_BYTES];
static uint8_t guardAfter[GUARD_BYTES];
static uint8_t loadedBitmap[1];

static Preferences preferences;
static LogicFRAMStorage rawStorage(JWPLC_FRAM, 0, FRAM_BYTES);
static LogicRetentiveStore store;

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
  if (!rawStorage.read(RETAIN_ADDRESS,
                       backupBytes,
                       sizeof(backupBytes)))
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

static bool verifyRegionMatchesBackup()
{
  size_t offset = 0;

  while (offset < sizeof(backupBytes))
  {
    const size_t remaining = sizeof(backupBytes) - offset;
    const size_t chunkLength =
        remaining < sizeof(verifyChunk) ? remaining : sizeof(verifyChunk);

    if (!rawStorage.read(RETAIN_ADDRESS + offset,
                         verifyChunk,
                         chunkLength) ||
        memcmp(backupBytes + offset, verifyChunk, chunkLength) != 0)
    {
      return false;
    }

    offset += chunkLength;
  }

  return true;
}

static bool regionContains(uint8_t value)
{
  size_t offset = 0;

  while (offset < RETAIN_BYTES)
  {
    const size_t remaining = RETAIN_BYTES - offset;
    const size_t chunkLength =
        remaining < sizeof(verifyChunk) ? remaining : sizeof(verifyChunk);

    if (!rawStorage.read(RETAIN_ADDRESS + offset,
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

static bool captureGuards()
{
  return rawStorage.read(RETAIN_ADDRESS - GUARD_BYTES,
                         guardBefore,
                         sizeof(guardBefore)) &&
         rawStorage.read(RETAIN_ADDRESS + RETAIN_BYTES,
                         guardAfter,
                         sizeof(guardAfter));
}

static bool guardBeforeUnchanged()
{
  uint8_t current[GUARD_BYTES] = {};
  return rawStorage.read(RETAIN_ADDRESS - GUARD_BYTES,
                         current,
                         sizeof(current)) &&
         memcmp(current, guardBefore, sizeof(current)) == 0;
}

static bool guardAfterUnchanged()
{
  uint8_t current[GUARD_BYTES] = {};
  return rawStorage.read(RETAIN_ADDRESS + RETAIN_BYTES,
                         current,
                         sizeof(current)) &&
         memcmp(current, guardAfter, sizeof(current)) == 0;
}

static bool corruptNewestHeaderCrc()
{
  const size_t address =
      RETAIN_ADDRESS + COPY_BYTES + HEADER_CRC_OFFSET;
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
      rawStorage.write(RETAIN_ADDRESS,
                       backupBytes,
                       sizeof(backupBytes));
  if (printResult)
  {
    expect("restauracion de la region retentiva escrita", writeOk);
  }

  if (!writeOk)
  {
    return false;
  }

  const bool verifyOk = verifyRegionMatchesBackup();
  if (printResult)
  {
    expect("contenido retentivo original restaurado exactamente", verifyOk);
  }

  if (!verifyOk)
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
                     ? "STORE RETENTIVO EN FRAM: PASS"
                     : "STORE RETENTIVO EN FRAM: FAIL");
}

void setup()
{
  Serial.begin(115200);
  Serial.setTimeout(250);
  delay(300);

  Serial.println();
  Serial.println("JWPLC Logic Runtime - store retentivo en FRAM fisica");
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
    Serial.println("Se recuperara la region retentiva original.");

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
  expect("region retentiva inicia en 0x1440", RETAIN_ADDRESS == 0x1440);
  expect("region retentiva ocupa 1536 bytes", RETAIN_BYTES == 1536);

  const bool guardsOk = captureGuards();
  expect("guardas adyacentes capturadas", guardsOk);

  const bool originalStoreOk =
      store.begin(rawStorage, JWPLCLogicStorageLayouts::FRAM_8K.retain);
  expect("store puede inspeccionar la region original", originalStoreOk);

  if (!guardsOk || !originalStoreOk)
  {
    Serial.println();
    Serial.println("PRUEBA ABORTADA: no se modifico la FRAM.");
    printSummary();
    return;
  }

  Serial.println();
  Serial.println("ADVERTENCIA: esta prueba modificara temporalmente:");
  Serial.println("0x1440..0x1A3F (1536 bytes de retentivos).");
  Serial.println("No tocara superblocks, Slots A/B ni la reserva.");
  Serial.println("El contenido original se respaldara y restaurara.");
  Serial.println("Escribe RETENTIVE y pulsa Enviar para continuar.");

  if (!waitForCommand("RETENTIVE"))
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

  const bool clearRegionOk =
      rawStorage.fill(RETAIN_ADDRESS, 0xFF, RETAIN_BYTES);
  expect("region retentiva limpiada de forma controlada", clearRegionOk);
  expect("limpieza de region verificada",
         clearRegionOk && regionContains(0xFF));

  LogicRetentiveStore cleanStore;
  const bool cleanBeginOk =
      clearRegionOk &&
      cleanStore.begin(rawStorage, JWPLCLogicStorageLayouts::FRAM_8K.retain);
  expect("store reabre la region limpia", cleanBeginOk);
  expect("region limpia inicia sin snapshot",
         cleanBeginOk && !cleanStore.hasSnapshot());
  expect("cada copia fisica ocupa 768 bytes",
         cleanBeginOk && cleanStore.copyBytes() == 768);
  expect("cada copia fisica ofrece 704 bytes de payload",
         cleanBeginOk && cleanStore.payloadCapacity() == 704);

  const uint8_t snapshotA[1] = {SNAPSHOT_A};
  const uint8_t snapshotB[1] = {SNAPSHOT_B};

  const bool saveAOk =
      cleanBeginOk && cleanStore.save(PROGRAM_ID,
                                      PROGRAM_GENERATION,
                                      PROGRAM_BLOCKS,
                                      snapshotA,
                                      sizeof(snapshotA));
  expect("snapshot A guardado en copia 0", saveAOk &&
                                                    cleanStore.status().activeCopy == 0);
  expect("snapshot A usa secuencia 1",
         saveAOk && cleanStore.status().sequence == 1);
  expect("snapshot A conserva identidad completa",
         saveAOk &&
             cleanStore.status().programId == PROGRAM_ID &&
             cleanStore.status().generation == PROGRAM_GENERATION &&
             cleanStore.status().blockCount == PROGRAM_BLOCKS);

  LogicRetentiveStore reopenedA;
  const bool reopenAOk =
      saveAOk && reopenedA.begin(rawStorage,
                                 JWPLCLogicStorageLayouts::FRAM_8K.retain);
  expect("reinicio fisico detecta snapshot A", reopenAOk &&
                                                    reopenedA.status().activeCopy == 0);

  size_t loadedBytes = 0;
  memset(loadedBitmap, 0, sizeof(loadedBitmap));
  const bool loadAOk =
      reopenAOk && reopenedA.load(PROGRAM_ID,
                                  PROGRAM_GENERATION,
                                  PROGRAM_BLOCKS,
                                  loadedBitmap,
                                  sizeof(loadedBitmap),
                                  loadedBytes);
  expect("snapshot A se recarga por identidad", loadAOk);
  expect("snapshot A conserva bitmap",
         loadAOk && loadedBytes == 1 && loadedBitmap[0] == SNAPSHOT_A);
  expect("carga de A selecciona copia 0",
         loadAOk && reopenedA.status().lastLoadedCopy == 0);

  const bool saveBOk =
      loadAOk && reopenedA.save(PROGRAM_ID,
                                PROGRAM_GENERATION,
                                PROGRAM_BLOCKS,
                                snapshotB,
                                sizeof(snapshotB));
  expect("snapshot B guardado en copia 1", saveBOk &&
                                                    reopenedA.status().activeCopy == 1);
  expect("snapshot B usa secuencia 2",
         saveBOk && reopenedA.status().sequence == 2);
  expect("snapshot B conserva identidad completa",
         saveBOk &&
             reopenedA.status().programId == PROGRAM_ID &&
             reopenedA.status().generation == PROGRAM_GENERATION &&
             reopenedA.status().blockCount == PROGRAM_BLOCKS);

  LogicRetentiveStore reopenedB;
  const bool reopenBOk =
      saveBOk && reopenedB.begin(rawStorage,
                                 JWPLCLogicStorageLayouts::FRAM_8K.retain);
  expect("reinicio fisico detecta snapshot B", reopenBOk &&
                                                    reopenedB.status().activeCopy == 1);

  loadedBytes = 0;
  loadedBitmap[0] = 0xFF;
  const bool loadBOk =
      reopenBOk && reopenedB.load(PROGRAM_ID,
                                  PROGRAM_GENERATION,
                                  PROGRAM_BLOCKS,
                                  loadedBitmap,
                                  sizeof(loadedBitmap),
                                  loadedBytes);
  expect("snapshot B se recarga por identidad", loadBOk);
  expect("snapshot B conserva bitmap",
         loadBOk && loadedBytes == 1 && loadedBitmap[0] == SNAPSHOT_B);
  expect("carga de B selecciona copia 1",
         loadBOk && reopenedB.status().lastLoadedCopy == 1);

  loadedBytes = 0;
  expect("carga rechaza Program ID diferente",
         !reopenedB.load(PROGRAM_ID + 1U,
                         PROGRAM_GENERATION,
                         PROGRAM_BLOCKS,
                         loadedBitmap,
                         sizeof(loadedBitmap),
                         loadedBytes));
  expect("error NO_MATCHING_SNAPSHOT queda informado",
         reopenedB.lastError() ==
             LogicRetentiveStoreError::NoMatchingSnapshot);

  loadedBytes = 0;
  expect("carga rechaza generacion diferente",
         !reopenedB.load(PROGRAM_ID,
                         PROGRAM_GENERATION + 1U,
                         PROGRAM_BLOCKS,
                         loadedBitmap,
                         sizeof(loadedBitmap),
                         loadedBytes));

  loadedBytes = 0;
  expect("carga rechaza cantidad de bloques diferente",
         !reopenedB.load(PROGRAM_ID,
                         PROGRAM_GENERATION,
                         PROGRAM_BLOCKS + 1U,
                         loadedBitmap,
                         sizeof(loadedBitmap),
                         loadedBytes));

  uint8_t crcProbe = 0;
  const size_t newestHeaderCrcAddress =
      RETAIN_ADDRESS + COPY_BYTES + HEADER_CRC_OFFSET;
  const bool crcAccessible =
      rawStorage.read(newestHeaderCrcAddress, &crcProbe, 1);
  expect("CRC de la copia nueva es accesible", crcAccessible);

  const bool corruptOk = crcAccessible && corruptNewestHeaderCrc();
  expect("CRC de la copia nueva se corrompe de forma controlada",
         corruptOk);

  LogicRetentiveStore recoveredStore;
  const bool recoveredBeginOk =
      corruptOk && recoveredStore.begin(
                       rawStorage,
                       JWPLCLogicStorageLayouts::FRAM_8K.retain);
  expect("reinicio descarta copia B corrupta", recoveredBeginOk);
  expect("copia A vuelve a ser la mas reciente valida",
         recoveredBeginOk &&
             recoveredStore.status().activeCopy == 0 &&
             recoveredStore.status().sequence == 1);

  loadedBytes = 0;
  loadedBitmap[0] = 0;
  const bool recoveredLoadOk =
      recoveredBeginOk && recoveredStore.load(PROGRAM_ID,
                                               PROGRAM_GENERATION,
                                               PROGRAM_BLOCKS,
                                               loadedBitmap,
                                               sizeof(loadedBitmap),
                                               loadedBytes);
  expect("snapshot A carga despues de corrupcion", recoveredLoadOk);
  expect("bitmap A sobrevive corrupcion de B",
         recoveredLoadOk &&
             loadedBytes == 1 &&
             loadedBitmap[0] == SNAPSHOT_A);
  expect("fallback retentivo selecciona copia 0",
         recoveredLoadOk &&
             recoveredStore.status().lastLoadedCopy == 0);

  expect("guarda anterior al limite permanece intacta",
         guardBeforeUnchanged());
  expect("guarda posterior al limite permanece intacta",
         guardAfterUnchanged());

  (void)restoreOriginalRegion(true);
  printSummary();
}

void loop()
{
}
