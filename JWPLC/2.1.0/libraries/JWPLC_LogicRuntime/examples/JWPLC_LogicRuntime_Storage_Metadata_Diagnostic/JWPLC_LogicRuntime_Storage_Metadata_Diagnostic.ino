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
    "Metadata A",
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
    "Metadata B",
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

  const LogicSuperblockInspection originalInspection =
      LogicSuperblockInspector::inspect(rawStorage);
  if (printResult)
  {
    expect("diagnostico recupera estado original sin formato",
           originalInspection.health ==
               LogicSuperblockHealth::Unformatted);
  }

  if (originalInspection.health != LogicSuperblockHealth::Unformatted)
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

static void printInspection(const char *label,
                            const LogicSuperblockInspection &inspection)
{
  Serial.print(label);
  Serial.print(": ");
  Serial.print(LogicSuperblockInspector::healthName(inspection.health));
  Serial.print(" / copia seleccionada: ");

  if (inspection.selectedCopy == LogicSuperblockInspector::INVALID_COPY)
  {
    Serial.println("NINGUNA");
  }
  else
  {
    Serial.println(inspection.selectedCopy);
  }
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
                     ? "DIAGNOSTICO DE METADATA: PASS"
                     : "DIAGNOSTICO DE METADATA: FAIL");
}

void setup()
{
  Serial.begin(115200);
  Serial.setTimeout(250);
  delay(300);

  Serial.println();
  Serial.println("JWPLC Logic Runtime - diagnostico de metadata persistente");
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

  const LogicSuperblockInspection initialInspection =
      LogicSuperblockInspector::inspect(rawStorage);
  printInspection("Estado inicial", initialInspection);
  expect("FRAM original se clasifica UNFORMATTED",
         initialInspection.health == LogicSuperblockHealth::Unformatted);

  if (!beginOk ||
      initialInspection.health != LogicSuperblockHealth::Unformatted)
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
  Serial.println("Diferenciara UNFORMATTED de CORRUPT_METADATA.");
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

  const bool saveAOk =
      formatOk && runtime.storage().save(PROGRAM_A, PROGRAM_A_ID);
  expect("Programa A guardado", saveAOk);

  const bool saveBOk =
      saveAOk && runtime.storage().save(PROGRAM_B, PROGRAM_B_ID);
  expect("Programa B guardado", saveBOk);

  const bool snapshotOk = captureSuperblocks();
  expect("superblocks validos respaldados en RAM", snapshotOk);

  const LogicSuperblockInspection validInspection =
      LogicSuperblockInspector::inspect(rawStorage);
  printInspection("Metadata valida", validInspection);
  expect("metadata integra se clasifica VALID",
         validInspection.health == LogicSuperblockHealth::Valid);
  expect("metadata valida selecciona copia 0 mas reciente",
         validInspection.selectedCopy == 0);
  expect("ambas copias validas antes de inyecciones",
         validInspection.copies[0].valid &&
             validInspection.copies[1].valid);

  const bool corruptNewestOk =
      snapshotOk && corruptSuperblockCrc(0);
  expect("CRC de copia 0 corrompido de forma controlada",
         corruptNewestOk);

  const LogicSuperblockInspection oneValidInspection =
      LogicSuperblockInspector::inspect(rawStorage);
  printInspection("Una copia valida", oneValidInspection);
  expect("una copia integra mantiene estado VALID",
         oneValidInspection.health == LogicSuperblockHealth::Valid);
  expect("diagnostico selecciona copia 1 redundante",
         oneValidInspection.selectedCopy == 1);
  expect("copia 0 conserva evidencia JWPLC aunque falle CRC",
         oneValidInspection.copies[0].runtimeEvidence &&
             !oneValidInspection.copies[0].valid);

  const bool restoreMetadataOk = writeSuperblockSnapshot();
  expect("superblocks restaurados antes de prueba doble",
         restoreMetadataOk);
  expect("restauracion intermedia verificada",
         restoreMetadataOk && verifySuperblockSnapshot());

  const bool corruptBoth0Ok =
      restoreMetadataOk && corruptSuperblockCrc(0);
  expect("copia 0 corrompida para prueba doble", corruptBoth0Ok);

  const bool corruptBoth1Ok =
      corruptBoth0Ok && corruptSuperblockCrc(1);
  expect("copia 1 corrompida para prueba doble", corruptBoth1Ok);

  const LogicSuperblockInspection corruptInspection =
      LogicSuperblockInspector::inspect(rawStorage);
  printInspection("Ambas copias corruptas", corruptInspection);
  expect("ambas copias se clasifican CORRUPT_METADATA",
         corruptInspection.health ==
             LogicSuperblockHealth::CorruptMetadata);
  expect("copia 0 mantiene evidencia reconocible",
         corruptInspection.copies[0].runtimeEvidence);
  expect("copia 1 mantiene evidencia reconocible",
         corruptInspection.copies[1].runtimeEvidence);
  expect("ninguna copia corrupta queda validada",
         !corruptInspection.copies[0].valid &&
             !corruptInspection.copies[1].valid);

  const bool reopenCorruptOk = runtime.storage().begin(JWPLC_FRAM);
  expect("fachada permanece lista con metadata corrupta",
         reopenCorruptOk && runtime.storage().isReady());
  expect("metadata corrupta no se declara formateada",
         reopenCorruptOk && !runtime.storage().isFormatted());

  const JWPLCLogicStorageBootState transitionalBootState =
      runtime.storage().prepareBoot();
  expect("politica publica actual permanece segura UNFORMATTED",
         transitionalBootState ==
             JWPLCLogicStorageBootState::Unformatted);
  expect("metadata corrupta no ofrece programa arrancable",
         !runtime.storage().hasBootableProgram());

  const bool restoreSuperblocksOk = writeSuperblockSnapshot();
  expect("superblocks restaurados despues del diagnostico",
         restoreSuperblocksOk);
  expect("restauracion final de metadata verificada",
         restoreSuperblocksOk && verifySuperblockSnapshot());

  (void)restoreOriginalStore(true);
  printSummary();
}

void loop()
{
}
