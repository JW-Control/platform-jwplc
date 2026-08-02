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

static constexpr char NVS_NAMESPACE[] = "jwlr_sb_red";
static constexpr char NVS_KEY_STAGE[] = "stage";
static constexpr char NVS_KEY_BACKUP[] = "backup";
static constexpr char NVS_KEY_CRC[] = "crc";

static constexpr uint8_t STAGE_IDLE = 0;
static constexpr uint8_t STAGE_RESTORE_PENDING = 1;

static constexpr uint32_t PROGRAM_A_ID = 0xA001;
static constexpr uint32_t PROGRAM_B_ID = 0xB002;

static uint8_t backupBytes[PROGRAM_STORE_BYTES];
static uint8_t verifyChunk[VERIFY_CHUNK_BYTES];
static uint8_t superblockSnapshot[SUPERBLOCK_AREA_BYTES];
static uint8_t superblockVerify[SUPERBLOCK_AREA_BYTES];

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
    {LogicBlockType::Not,
     0,
     JWPLC_LOGIC_NO_SOURCE,
     0,
     0}};

static const LogicProgram PROGRAM_A = {
    "Superblock A",
    PROGRAM_A_BLOCKS,
    static_cast<uint16_t>(sizeof(PROGRAM_A_BLOCKS) /
                          sizeof(PROGRAM_A_BLOCKS[0]))};

static const LogicBlockDefinition PROGRAM_B_BLOCKS[] = {
    {LogicBlockType::DigitalInput,
     JWPLC_LOGIC_NO_SOURCE,
     JWPLC_LOGIC_NO_SOURCE,
     1,
     0},
    {LogicBlockType::Not,
     0,
     JWPLC_LOGIC_NO_SOURCE,
     0,
     0}};

static const LogicProgram PROGRAM_B = {
    "Superblock B",
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

static bool loadedMatches(const LogicProgram &expected)
{
  const LogicProgram loaded = runtime.storage().activeProgram();

  if (loaded.name == nullptr || loaded.blocks == nullptr ||
      loaded.blockCount != expected.blockCount ||
      strcmp(loaded.name, expected.name) != 0)
  {
    return false;
  }

  for (uint16_t index = 0; index < loaded.blockCount; ++index)
  {
    const LogicBlockDefinition &left = loaded.blocks[index];
    const LogicBlockDefinition &right = expected.blocks[index];

    if (left.type != right.type ||
        left.sourceA != right.sourceA ||
        left.sourceB != right.sourceB ||
        left.resource != right.resource ||
        left.parameter != right.parameter)
    {
      return false;
    }
  }

  return true;
}

static bool captureSuperblocks()
{
  return rawStorage.read(0,
                         superblockSnapshot,
                         sizeof(superblockSnapshot));
}

static bool writeSuperblockSnapshot()
{
  return rawStorage.write(0,
                          superblockSnapshot,
                          sizeof(superblockSnapshot));
}

static bool verifySuperblockSnapshot()
{
  if (!rawStorage.read(0,
                       superblockVerify,
                       sizeof(superblockVerify)))
  {
    return false;
  }

  return memcmp(superblockSnapshot,
                superblockVerify,
                sizeof(superblockSnapshot)) == 0;
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

  const JWPLCLogicStorageBootState restoredState =
      runtime.storage().prepareBoot();
  if (printResult)
  {
    expect("politica recupera estado original sin formato",
           restoredState == JWPLCLogicStorageBootState::Unformatted);
  }

  if (restoredState != JWPLCLogicStorageBootState::Unformatted)
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
                     ? "REDUNDANCIA DE SUPERBLOCKS: PASS"
                     : "REDUNDANCIA DE SUPERBLOCKS: FAIL");
}

void setup()
{
  Serial.begin(115200);
  Serial.setTimeout(250);
  delay(300);

  Serial.println();
  Serial.println("JWPLC Logic Runtime - redundancia de superblocks");
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
  Serial.println("Corrompera de forma controlada una y ambas copias de metadata.");
  Serial.println("El contenido original se respaldara y restaurara.");
  Serial.println("Escribe SUPERBLOCK y pulsa Enviar para continuar.");

  if (!waitForCommand("SUPERBLOCK"))
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

  const bool saveAOk =
      formatOk && runtime.storage().save(PROGRAM_A, PROGRAM_A_ID);
  expect("Programa A guardado", saveAOk);

  const LogicProgramStoreStatus statusA = runtime.storage().status();
  expect("Programa A activa Slot A",
         saveAOk && statusA.activeSlot == 0 &&
             statusA.superblockCopy == 1 && statusA.sequence == 2);

  const bool saveBOk =
      saveAOk && runtime.storage().save(PROGRAM_B, PROGRAM_B_ID);
  expect("Programa B guardado", saveBOk);

  const LogicProgramStoreStatus statusB = runtime.storage().status();
  expect("Programa B activa Slot B mediante superblock 0",
         saveBOk && statusB.activeSlot == 1 &&
             statusB.superblockCopy == 0 && statusB.sequence == 3 &&
             statusB.programId == PROGRAM_B_ID && statusB.generation == 2);

  const JWPLCLogicStorageBootState initialBState =
      runtime.storage().prepareBoot();
  expect("Programa B carga como activo antes de inyecciones",
         initialBState ==
             JWPLCLogicStorageBootState::ActiveProgramLoaded);
  expect("Programa B conserva contenido", loadedMatches(PROGRAM_B));

  const bool snapshotOk = captureSuperblocks();
  expect("copias validas de superblock respaldadas en RAM", snapshotOk);

  const bool corruptNewestOk = snapshotOk && corruptSuperblockCrc(0);
  expect("superblock mas nuevo corrompido de forma controlada",
         corruptNewestOk);

  const bool reopenFromCopy1Ok =
      corruptNewestOk && runtime.storage().begin(JWPLC_FRAM);
  expect("fachada reabre usando la copia redundante 1", reopenFromCopy1Ok);
  expect("almacenamiento sigue formateado con una copia valida",
         reopenFromCopy1Ok && runtime.storage().isFormatted());

  const LogicProgramStoreStatus copy1Status = runtime.storage().status();
  expect("copia 1 recupera metadata anterior",
         reopenFromCopy1Ok && copy1Status.superblockCopy == 1 &&
             copy1Status.sequence == 2 && copy1Status.activeSlot == 0 &&
             copy1Status.programId == PROGRAM_A_ID &&
             copy1Status.generation == 1);

  const JWPLCLogicStorageBootState copy1BootState =
      runtime.storage().prepareBoot();
  expect("programa de la copia 1 queda arrancable",
         copy1BootState ==
             JWPLCLogicStorageBootState::ActiveProgramLoaded);
  expect("copia 1 reconstruye Programa A", loadedMatches(PROGRAM_A));

  const bool restoreAfterNewestOk = writeSuperblockSnapshot();
  expect("superblocks restaurados despues de corromper copia nueva",
         restoreAfterNewestOk);
  expect("restauracion de superblocks verificada",
         restoreAfterNewestOk && verifySuperblockSnapshot());

  const bool reopenRestoredOk =
      restoreAfterNewestOk && runtime.storage().begin(JWPLC_FRAM);
  expect("fachada reabre con ambas copias restauradas", reopenRestoredOk);

  const LogicProgramStoreStatus restoredBStatus = runtime.storage().status();
  expect("metadata mas nueva B vuelve a quedar seleccionada",
         reopenRestoredOk && restoredBStatus.superblockCopy == 0 &&
             restoredBStatus.sequence == 3 && restoredBStatus.activeSlot == 1);

  const bool corruptOlderOk =
      reopenRestoredOk && corruptSuperblockCrc(1);
  expect("superblock anterior corrompido de forma controlada",
         corruptOlderOk);

  const bool reopenFromCopy0Ok =
      corruptOlderOk && runtime.storage().begin(JWPLC_FRAM);
  expect("fachada reabre usando la copia mas nueva 0", reopenFromCopy0Ok);
  expect("almacenamiento sigue formateado con copia 0 valida",
         reopenFromCopy0Ok && runtime.storage().isFormatted());

  const LogicProgramStoreStatus copy0Status = runtime.storage().status();
  expect("copia 0 conserva metadata B mas nueva",
         reopenFromCopy0Ok && copy0Status.superblockCopy == 0 &&
             copy0Status.sequence == 3 && copy0Status.activeSlot == 1 &&
             copy0Status.programId == PROGRAM_B_ID &&
             copy0Status.generation == 2);

  const JWPLCLogicStorageBootState copy0BootState =
      runtime.storage().prepareBoot();
  expect("programa de la copia 0 queda arrancable",
         copy0BootState ==
             JWPLCLogicStorageBootState::ActiveProgramLoaded);
  expect("copia 0 reconstruye Programa B", loadedMatches(PROGRAM_B));

  const bool restoreAfterOlderOk = writeSuperblockSnapshot();
  expect("superblocks restaurados despues de corromper copia anterior",
         restoreAfterOlderOk);
  expect("segunda restauracion de superblocks verificada",
         restoreAfterOlderOk && verifySuperblockSnapshot());

  const bool corruptBoth0Ok =
      restoreAfterOlderOk && corruptSuperblockCrc(0);
  expect("copia 0 corrompida para prueba doble", corruptBoth0Ok);
  const bool corruptBoth1Ok =
      corruptBoth0Ok && corruptSuperblockCrc(1);
  expect("copia 1 corrompida para prueba doble", corruptBoth1Ok);

  const bool reopenBothInvalidOk =
      corruptBoth1Ok && runtime.storage().begin(JWPLC_FRAM);
  expect("fachada permanece operativa con ambas copias invalidas",
         reopenBothInvalidOk && runtime.storage().isReady());
  expect("sin superblock valido no se declara formato activo",
         reopenBothInvalidOk && !runtime.storage().isFormatted());
  expect("gestor informa UNFORMATTED con ambas copias invalidas",
         reopenBothInvalidOk &&
             runtime.storage().storeError() ==
                 LogicProgramStoreError::Unformatted);

  const JWPLCLogicStorageBootState bothInvalidState =
      runtime.storage().prepareBoot();
  expect("politica mantiene estado seguro UNFORMATTED",
         bothInvalidState == JWPLCLogicStorageBootState::Unformatted);
  expect("ambas copias invalidas descargan el programa",
         !runtime.storage().hasLoadedProgram());
  expect("ambas copias invalidas no ofrecen programa arrancable",
         !runtime.storage().hasBootableProgram());

  const bool restoreAfterBothOk = writeSuperblockSnapshot();
  expect("superblocks restaurados despues de prueba doble",
         restoreAfterBothOk);
  expect("restauracion posterior a prueba doble verificada",
         restoreAfterBothOk && verifySuperblockSnapshot());

  const bool reopenFinalBok =
      restoreAfterBothOk && runtime.storage().begin(JWPLC_FRAM);
  expect("fachada reabre despues de restaurar ambas copias",
         reopenFinalBok);

  const JWPLCLogicStorageBootState finalBState =
      runtime.storage().prepareBoot();
  expect("Programa B vuelve a quedar activo tras restaurar metadata",
         finalBState ==
             JWPLCLogicStorageBootState::ActiveProgramLoaded);
  expect("Programa B final conserva contenido", loadedMatches(PROGRAM_B));

  (void)restoreOriginalStore(true);
  printSummary();
}

void loop()
{
}
