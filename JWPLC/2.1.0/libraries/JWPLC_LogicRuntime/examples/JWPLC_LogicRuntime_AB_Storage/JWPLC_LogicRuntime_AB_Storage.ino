#include <JWPLC_LogicRuntime.h>

#include <cstring>

static constexpr size_t STORAGE_BYTES = 8UL * 1024UL;
static constexpr size_t SCRATCH_BYTES = 2560;
static constexpr size_t SUPERBLOCK_AREA_BYTES = 64;
static constexpr size_t SLOT_DESCRIPTOR_BYTES = 32;

static uint8_t storageBytes[STORAGE_BYTES];
static uint8_t baselineBytes[STORAGE_BYTES];
static uint8_t workBytes[STORAGE_BYTES];
static uint8_t scratch[SCRATCH_BYTES];
static LogicProgramBuffer loadedProgram;

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

static const LogicBlockDefinition PROGRAM_B_BLOCKS[] = {
    {LogicBlockType::DigitalInput,
     JWPLC_LOGIC_NO_SOURCE,
     JWPLC_LOGIC_NO_SOURCE,
     1,
     0},
    {LogicBlockType::Ton,
     0,
     JWPLC_LOGIC_NO_SOURCE,
     0,
     1500},
    {LogicBlockType::DigitalOutput,
     1,
     JWPLC_LOGIC_NO_SOURCE,
     1,
     0}};

static const LogicProgram PROGRAM_A = {
    "Programa A",
    PROGRAM_A_BLOCKS,
    static_cast<uint16_t>(sizeof(PROGRAM_A_BLOCKS) /
                          sizeof(PROGRAM_A_BLOCKS[0]))};

static const LogicProgram PROGRAM_B = {
    "Programa B",
    PROGRAM_B_BLOCKS,
    static_cast<uint16_t>(sizeof(PROGRAM_B_BLOCKS) /
                          sizeof(PROGRAM_B_BLOCKS[0]))};

static bool loadedMatches(uint32_t programId,
                          uint32_t generation,
                          const char *name)
{
  return loadedProgram.metadata.programId == programId &&
         loadedProgram.metadata.generation == generation &&
         std::strcmp(loadedProgram.name, name) == 0;
}

void setup()
{
  Serial.begin(115200);
  delay(200);

  Serial.println();
  Serial.println("JWPLC Logic Runtime - PoC 4 almacenamiento A/B en RAM");
  Serial.println("No inicializa E/S ni escribe la FRAM fisica.");
  Serial.println();

  std::memset(storageBytes, 0xFF, sizeof(storageBytes));

  LogicMemoryStorage storage(storageBytes, sizeof(storageBytes));
  LogicProgramStore store;

  expect("backend RAM acepta perfil de 8 KiB",
         store.begin(storage, JWPLCLogicStorageProfiles::FRAM_8K));
  expect("almacenamiento nuevo se detecta sin formato",
         !store.isFormatted() &&
             store.lastError() == LogicProgramStoreError::Unformatted);
  expect("layout A/B cabe en 8 KiB",
         store.requiredCapacity() <= sizeof(storageBytes));
  expect("capacidad util por slot es coherente",
         store.slotPayloadCapacity() ==
             JWPLCLogicStorageProfiles::FRAM_8K.programSlotBytes -
                 SLOT_DESCRIPTOR_BYTES);

  expect("formateo inicial correcto", store.format());
  expect("sin programa activo tras formateo",
         store.status().activeSlot == JWPLC_LOGIC_INVALID_SLOT);

  expect("guardado inicial en Slot A",
         store.saveProgram(PROGRAM_A,
                           0xA001,
                           1,
                           0,
                           scratch,
                           sizeof(scratch)));
  expect("Slot A queda activo",
         store.status().activeSlot == 0 &&
             store.status().generation == 1);
  expect("carga del programa A",
         store.loadActive(loadedProgram, scratch, sizeof(scratch)) &&
             loadedMatches(0xA001, 1, "Programa A"));
  expect("programa A reconstruido supera validador",
         LogicValidator::validate(loadedProgram.asProgram(),
                                  100,
                                  8,
                                  8) == LogicValidationError::None);

  std::memcpy(baselineBytes, storageBytes, sizeof(storageBytes));

  storage.setWriteBudget(10000);
  expect("guardado candidato en Slot B",
         store.saveProgram(PROGRAM_B,
                           0xB002,
                           2,
                           0,
                           scratch,
                           sizeof(scratch)));
  const size_t completeSaveBytes = storage.bytesWritten();
  storage.clearWriteBudget();

  expect("Slot B queda activo",
         store.status().activeSlot == 1 &&
             store.status().generation == 2);
  expect("carga del programa B",
         store.loadActive(loadedProgram, scratch, sizeof(scratch)) &&
             loadedMatches(0xB002, 2, "Programa B"));

  // Corromper la imagen activa B. El store debe recuperar A.
  const size_t slotBImageAddress =
      SUPERBLOCK_AREA_BYTES +
      JWPLCLogicStorageProfiles::FRAM_8K.programSlotBytes +
      SLOT_DESCRIPTOR_BYTES;
  storageBytes[slotBImageAddress + 70] ^= 0x5A;

  LogicProgramStore corruptedImageStore;
  expect("reinicio tras corrupcion de imagen",
         corruptedImageStore.begin(storage,
                                   JWPLCLogicStorageProfiles::FRAM_8K));
  expect("fallback al programa A si B esta corrupto",
         corruptedImageStore.loadActive(loadedProgram,
                                        scratch,
                                        sizeof(scratch)) &&
             loadedMatches(0xA001, 1, "Programa A") &&
             corruptedImageStore.status().lastLoadedSlot == 0);

  // Restaurar A+B y corromper el superblock mas nuevo (copia 0).
  std::memcpy(storageBytes, baselineBytes, sizeof(storageBytes));
  LogicProgramStore rebuildStore;
  rebuildStore.begin(storage, JWPLCLogicStorageProfiles::FRAM_8K);
  rebuildStore.saveProgram(PROGRAM_B,
                           0xB002,
                           2,
                           0,
                           scratch,
                           sizeof(scratch));
  storageBytes[28] ^= 0xA5;

  LogicProgramStore corruptedSuperblockStore;
  expect("copia anterior de superblock sigue valida",
         corruptedSuperblockStore.begin(
             storage,
             JWPLCLogicStorageProfiles::FRAM_8K) &&
             corruptedSuperblockStore.status().activeSlot == 0);
  expect("superblock redundante conserva programa A",
         corruptedSuperblockStore.loadActive(loadedProgram,
                                             scratch,
                                             sizeof(scratch)) &&
             loadedMatches(0xA001, 1, "Programa A"));

  // Probar cada posible corte antes de completar una actualización A -> B.
  bool allInterruptedUpdatesRecovered = true;
  for (size_t cut = 0; cut < completeSaveBytes; ++cut)
  {
    std::memcpy(workBytes, baselineBytes, sizeof(workBytes));

    LogicMemoryStorage workStorage(workBytes, sizeof(workBytes));
    LogicProgramStore interruptedStore;
    if (!interruptedStore.begin(workStorage,
                                JWPLCLogicStorageProfiles::FRAM_8K))
    {
      allInterruptedUpdatesRecovered = false;
      break;
    }

    workStorage.setWriteBudget(cut);
    const bool unexpectedlyCompleted =
        interruptedStore.saveProgram(PROGRAM_B,
                                     0xB002,
                                     2,
                                     0,
                                     scratch,
                                     sizeof(scratch));
    workStorage.clearWriteBudget();

    if (unexpectedlyCompleted)
    {
      allInterruptedUpdatesRecovered = false;
      break;
    }

    LogicProgramStore rebootedStore;
    if (!rebootedStore.begin(workStorage,
                             JWPLCLogicStorageProfiles::FRAM_8K) ||
        !rebootedStore.loadActive(loadedProgram,
                                  scratch,
                                  sizeof(scratch)) ||
        !loadedMatches(0xA001, 1, "Programa A"))
    {
      allInterruptedUpdatesRecovered = false;
      break;
    }
  }

  expect("todos los cortes parciales conservan programa A",
         allInterruptedUpdatesRecovered);

  std::memcpy(workBytes, baselineBytes, sizeof(workBytes));
  LogicMemoryStorage finalStorage(workBytes, sizeof(workBytes));
  LogicProgramStore finalStore;
  finalStore.begin(finalStorage, JWPLCLogicStorageProfiles::FRAM_8K);
  finalStorage.setWriteBudget(completeSaveBytes);
  const bool exactBudgetSaved =
      finalStore.saveProgram(PROGRAM_B,
                             0xB002,
                             2,
                             0,
                             scratch,
                             sizeof(scratch));
  finalStorage.clearWriteBudget();

  LogicProgramStore finalReboot;
  expect("presupuesto completo confirma programa B",
         exactBudgetSaved &&
             finalReboot.begin(finalStorage,
                               JWPLCLogicStorageProfiles::FRAM_8K) &&
             finalReboot.loadActive(loadedProgram,
                                    scratch,
                                    sizeof(scratch)) &&
             loadedMatches(0xB002, 2, "Programa B"));

  Serial.println();
  Serial.print("Bytes escritos por actualizacion completa: ");
  Serial.println(completeSaveBytes);
  Serial.print("Puntos de corte probados: ");
  Serial.println(completeSaveBytes);
  Serial.print("Resultado: ");
  Serial.print(passedTests);
  Serial.print(" PASS, ");
  Serial.print(failedTests);
  Serial.println(" FAIL");

  if (failedTests == 0)
  {
    Serial.println("ALMACENAMIENTO A/B SIMULADO: PASS");
  }
  else
  {
    Serial.println("ALMACENAMIENTO A/B SIMULADO: FAIL");
  }
}

void loop()
{
}
