#include <JWPLC_LogicRuntime.h>

#include <cstring>

static constexpr size_t RETAIN_BYTES =
    JWPLCLogicStorageLayouts::FRAM_8K.retain.size;
static constexpr LogicStorageRegion TEST_REGION(0, RETAIN_BYTES);
static constexpr uint16_t BLOCK_COUNT = 5;
static constexpr uint32_t PROGRAM_A_ID = 0xA001;
static constexpr uint32_t PROGRAM_B_ID = 0xB002;
static constexpr uint32_t PROGRAM_A_GENERATION = 1;
static constexpr uint32_t PROGRAM_B_GENERATION = 7;
static constexpr size_t HEADER_CRC_OFFSET = 28;

static uint8_t storageBytes[RETAIN_BYTES];
static uint8_t baselineBytes[RETAIN_BYTES];
static uint8_t workBytes[RETAIN_BYTES];

static const uint8_t SNAPSHOT_A[] = {
    static_cast<uint8_t>(1U << 3U)};
static const uint8_t SNAPSHOT_B[] = {
    static_cast<uint8_t>(1U << 4U)};

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

static bool bitmapEquals(const uint8_t *left,
                         const uint8_t *right,
                         size_t length)
{
  return memcmp(left, right, length) == 0;
}

static void printSummary(size_t completeSaveBytes)
{
  Serial.println();
  Serial.print("Bytes escritos por snapshot completo: ");
  Serial.println(completeSaveBytes);
  Serial.print("Puntos de corte probados: ");
  Serial.println(completeSaveBytes);
  Serial.print("Resultado: ");
  Serial.print(passedTests);
  Serial.print(" PASS, ");
  Serial.print(failedTests);
  Serial.println(" FAIL");

  Serial.println(failedTests == 0
                     ? "STORE RETENTIVO A/B SIMULADO: PASS"
                     : "STORE RETENTIVO A/B SIMULADO: FAIL");
}

void setup()
{
  Serial.begin(115200);
  delay(300);

  Serial.println();
  Serial.println("JWPLC Logic Runtime - store retentivo A/B simulado");
  Serial.println("No inicializa E/S ni accede a la FRAM fisica.");
  Serial.println();

  expect("region retentiva 8 KiB inicia en 0x1440",
         JWPLCLogicStorageLayouts::FRAM_8K.retain.address == 0x1440);
  expect("region retentiva 8 KiB ocupa 1536 bytes",
         RETAIN_BYTES == 1536);

  memset(storageBytes, 0xFF, sizeof(storageBytes));
  LogicMemoryStorage storage(storageBytes, sizeof(storageBytes));
  LogicRetentiveStore store;

  const bool beginOk = store.begin(storage, TEST_REGION);
  expect("store acepta region retentiva simulada", beginOk);
  expect("cada copia ocupa 768 bytes",
         beginOk && store.copyBytes() == 768);
  expect("cada copia ofrece 704 bytes de payload",
         beginOk && store.payloadCapacity() == 704);
  expect("bitmap maximo de 400 bloques cabe en una copia",
         beginOk &&
             LogicRetentiveStore::MAX_BITMAP_BYTES <=
                 store.payloadCapacity());
  expect("region nueva inicia sin snapshot",
         beginOk && !store.hasSnapshot());

  expect("5 bloques requieren un byte",
         LogicRetentiveStore::requiredBitmapBytes(5) == 1);
  expect("100 bloques requieren 13 bytes",
         LogicRetentiveStore::requiredBitmapBytes(100) == 13);
  expect("400 bloques requieren 50 bytes",
         LogicRetentiveStore::requiredBitmapBytes(400) == 50);
  expect("cero bloques no define bitmap",
         LogicRetentiveStore::requiredBitmapBytes(0) == 0);

  const bool saveAOk =
      beginOk && store.save(PROGRAM_A_ID,
                            PROGRAM_A_GENERATION,
                            BLOCK_COUNT,
                            SNAPSHOT_A,
                            sizeof(SNAPSHOT_A));
  expect("snapshot A se guarda en copia 0", saveAOk);

  const LogicRetentiveStoreStatus statusA = store.status();
  expect("snapshot A usa secuencia 1",
         saveAOk && statusA.activeCopy == 0 &&
             statusA.sequence == 1);
  expect("snapshot A conserva identidad completa",
         saveAOk && statusA.programId == PROGRAM_A_ID &&
             statusA.generation == PROGRAM_A_GENERATION &&
             statusA.blockCount == BLOCK_COUNT &&
             statusA.bitmapBytes == sizeof(SNAPSHOT_A));

  uint8_t loaded[LogicRetentiveStore::MAX_BITMAP_BYTES] = {};
  size_t loadedBytes = 0;
  const bool loadAOk =
      store.load(PROGRAM_A_ID,
                 PROGRAM_A_GENERATION,
                 BLOCK_COUNT,
                 loaded,
                 sizeof(loaded),
                 loadedBytes);
  expect("snapshot A se recarga por identidad", loadAOk);
  expect("snapshot A conserva bitmap",
         loadAOk && loadedBytes == sizeof(SNAPSHOT_A) &&
             bitmapEquals(loaded, SNAPSHOT_A, loadedBytes));

  loadedBytes = 0;
  expect("carga rechaza buffer insuficiente",
         !store.load(PROGRAM_A_ID,
                     PROGRAM_A_GENERATION,
                     BLOCK_COUNT,
                     loaded,
                     0,
                     loadedBytes));
  expect("error BUFFER_TOO_SMALL queda informado",
         store.lastError() == LogicRetentiveStoreError::BufferTooSmall);

  loadedBytes = 0;
  expect("carga rechaza Program ID diferente",
         !store.load(0xFFFF,
                     PROGRAM_A_GENERATION,
                     BLOCK_COUNT,
                     loaded,
                     sizeof(loaded),
                     loadedBytes) &&
             store.lastError() ==
                 LogicRetentiveStoreError::NoMatchingSnapshot);

  loadedBytes = 0;
  expect("carga rechaza generacion diferente",
         !store.load(PROGRAM_A_ID,
                     PROGRAM_A_GENERATION + 1U,
                     BLOCK_COUNT,
                     loaded,
                     sizeof(loaded),
                     loadedBytes) &&
             store.lastError() ==
                 LogicRetentiveStoreError::NoMatchingSnapshot);

  loadedBytes = 0;
  expect("carga rechaza cantidad de bloques diferente",
         !store.load(PROGRAM_A_ID,
                     PROGRAM_A_GENERATION,
                     BLOCK_COUNT + 1U,
                     loaded,
                     sizeof(loaded),
                     loadedBytes) &&
             store.lastError() ==
                 LogicRetentiveStoreError::NoMatchingSnapshot);

  memcpy(baselineBytes, storageBytes, sizeof(baselineBytes));

  storage.setWriteBudget(10000);
  const bool saveBOk =
      store.save(PROGRAM_B_ID,
                 PROGRAM_B_GENERATION,
                 BLOCK_COUNT,
                 SNAPSHOT_B,
                 sizeof(SNAPSHOT_B));
  const size_t completeSaveBytes = storage.bytesWritten();
  storage.clearWriteBudget();

  expect("snapshot B se guarda en copia 1", saveBOk);
  expect("snapshot completo escribe 66 bytes",
         saveBOk && completeSaveBytes == 66);

  const LogicRetentiveStoreStatus statusB = store.status();
  expect("snapshot B usa secuencia 2",
         saveBOk && statusB.activeCopy == 1 &&
             statusB.sequence == 2);
  expect("snapshot B conserva identidad completa",
         saveBOk && statusB.programId == PROGRAM_B_ID &&
             statusB.generation == PROGRAM_B_GENERATION &&
             statusB.blockCount == BLOCK_COUNT);

  memset(loaded, 0, sizeof(loaded));
  loadedBytes = 0;
  const bool loadBOk =
      store.load(PROGRAM_B_ID,
                 PROGRAM_B_GENERATION,
                 BLOCK_COUNT,
                 loaded,
                 sizeof(loaded),
                 loadedBytes);
  expect("snapshot B se carga como identidad nueva", loadBOk);
  expect("snapshot B conserva bitmap",
         loadBOk && loadedBytes == sizeof(SNAPSHOT_B) &&
             bitmapEquals(loaded, SNAPSHOT_B, loadedBytes));

  memset(loaded, 0, sizeof(loaded));
  loadedBytes = 0;
  const bool olderAOk =
      store.load(PROGRAM_A_ID,
                 PROGRAM_A_GENERATION,
                 BLOCK_COUNT,
                 loaded,
                 sizeof(loaded),
                 loadedBytes);
  expect("snapshot A anterior sigue disponible por identidad", olderAOk);
  expect("carga de A selecciona copia 0 y conserva bitmap",
         olderAOk && store.status().lastLoadedCopy == 0 &&
             bitmapEquals(loaded, SNAPSHOT_A, loadedBytes));

  const size_t copy1HeaderCrc =
      store.copyBytes() + HEADER_CRC_OFFSET;
  storageBytes[copy1HeaderCrc] ^= 0x5A;
  expect("CRC de la copia nueva se corrompe de forma controlada", true);

  LogicRetentiveStore corruptedStore;
  const bool reopenCorruptOk =
      corruptedStore.begin(storage, TEST_REGION);
  expect("reinicio descarta copia B corrupta", reopenCorruptOk);
  expect("copia A vuelve a ser la mas reciente valida",
         reopenCorruptOk && corruptedStore.status().activeCopy == 0 &&
             corruptedStore.status().sequence == 1 &&
             corruptedStore.status().programId == PROGRAM_A_ID);

  memset(loaded, 0, sizeof(loaded));
  loadedBytes = 0;
  const bool loadAfterCorruptOk =
      corruptedStore.load(PROGRAM_A_ID,
                          PROGRAM_A_GENERATION,
                          BLOCK_COUNT,
                          loaded,
                          sizeof(loaded),
                          loadedBytes);
  expect("snapshot A carga despues de corrupcion", loadAfterCorruptOk);
  expect("bitmap A sobrevive corrupcion de B",
         loadAfterCorruptOk &&
             bitmapEquals(loaded, SNAPSHOT_A, loadedBytes));

  bool allInterruptedUpdatesRecovered = completeSaveBytes > 0;
  for (size_t cut = 0;
       cut < completeSaveBytes && allInterruptedUpdatesRecovered;
       ++cut)
  {
    memcpy(workBytes, baselineBytes, sizeof(workBytes));

    LogicMemoryStorage workStorage(workBytes, sizeof(workBytes));
    LogicRetentiveStore interruptedStore;
    if (!interruptedStore.begin(workStorage, TEST_REGION))
    {
      allInterruptedUpdatesRecovered = false;
      break;
    }

    workStorage.setWriteBudget(cut);
    const bool unexpectedlyCompleted =
        interruptedStore.save(PROGRAM_B_ID,
                              PROGRAM_B_GENERATION,
                              BLOCK_COUNT,
                              SNAPSHOT_B,
                              sizeof(SNAPSHOT_B));
    workStorage.clearWriteBudget();

    if (unexpectedlyCompleted)
    {
      allInterruptedUpdatesRecovered = false;
      break;
    }

    LogicRetentiveStore rebootedStore;
    uint8_t recovered[LogicRetentiveStore::MAX_BITMAP_BYTES] = {};
    size_t recoveredBytes = 0;

    if (!rebootedStore.begin(workStorage, TEST_REGION) ||
        !rebootedStore.load(PROGRAM_A_ID,
                            PROGRAM_A_GENERATION,
                            BLOCK_COUNT,
                            recovered,
                            sizeof(recovered),
                            recoveredBytes) ||
        recoveredBytes != sizeof(SNAPSHOT_A) ||
        !bitmapEquals(recovered, SNAPSHOT_A, recoveredBytes))
    {
      allInterruptedUpdatesRecovered = false;
    }
  }

  expect("todos los cortes parciales conservan snapshot A",
         allInterruptedUpdatesRecovered);

  memcpy(workBytes, baselineBytes, sizeof(workBytes));
  LogicMemoryStorage finalStorage(workBytes, sizeof(workBytes));
  LogicRetentiveStore finalStore;
  const bool finalBeginOk = finalStore.begin(finalStorage, TEST_REGION);

  finalStorage.setWriteBudget(completeSaveBytes);
  const bool exactBudgetSaved =
      finalBeginOk && finalStore.save(PROGRAM_B_ID,
                                     PROGRAM_B_GENERATION,
                                     BLOCK_COUNT,
                                     SNAPSHOT_B,
                                     sizeof(SNAPSHOT_B));
  finalStorage.clearWriteBudget();
  expect("presupuesto exacto confirma snapshot B", exactBudgetSaved);

  LogicRetentiveStore finalReboot;
  memset(loaded, 0, sizeof(loaded));
  loadedBytes = 0;
  const bool finalLoadOk =
      exactBudgetSaved &&
      finalReboot.begin(finalStorage, TEST_REGION) &&
      finalReboot.load(PROGRAM_B_ID,
                       PROGRAM_B_GENERATION,
                       BLOCK_COUNT,
                       loaded,
                       sizeof(loaded),
                       loadedBytes);
  expect("reinicio con commit completo carga snapshot B", finalLoadOk);
  expect("snapshot B final conserva bitmap",
         finalLoadOk && loadedBytes == sizeof(SNAPSHOT_B) &&
             bitmapEquals(loaded, SNAPSHOT_B, loadedBytes));

  printSummary(completeSaveBytes);
}

void loop()
{
}
