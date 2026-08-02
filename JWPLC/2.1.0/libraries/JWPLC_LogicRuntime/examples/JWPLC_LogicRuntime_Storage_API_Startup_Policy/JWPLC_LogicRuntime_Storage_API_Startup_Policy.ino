#include <JWPLC_GlobalPeripherals.h>
#include <JWPLC_LogicRuntime.h>
#include <Preferences.h>

#include <cstring>

static constexpr size_t PROGRAM_STORE_BYTES =
    JWPLCLogicStorageLayouts::FRAM_8K.programStoreBytes();
static constexpr size_t VERIFY_CHUNK_BYTES = 64;

static constexpr char NVS_NAMESPACE[] = "jwlr_boot_pol";
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
    {LogicBlockType::DigitalOutput,
     0,
     JWPLC_LOGIC_NO_SOURCE,
     0,
     0}};

static const LogicProgram PROGRAM_A = {
    "Startup A",
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
     0},
    {LogicBlockType::DigitalOutput,
     1,
     JWPLC_LOGIC_NO_SOURCE,
     1,
     0}};

static const LogicProgram PROGRAM_B = {
    "Startup B",
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

static bool corruptFirstImageByte(const LogicStorageRegion &slot)
{
  const size_t address =
      slot.address + LogicStorageLayout::SLOT_DESCRIPTOR_BYTES;
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
                     ? "POLITICA DE ARRANQUE: PASS"
                     : "POLITICA DE ARRANQUE: FAIL");
}

void setup()
{
  Serial.begin(115200);
  Serial.setTimeout(250);
  delay(300);

  Serial.println();
  Serial.println("JWPLC Logic Runtime - politica de arranque persistente");
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

  const JWPLCLogicStorageBootState initialState =
      runtime.storage().prepareBoot();
  expect("politica detecta FRAM sin formato",
         initialState == JWPLCLogicStorageBootState::Unformatted);
  expect("bootState informa UNFORMATTED",
         runtime.storage().bootState() ==
             JWPLCLogicStorageBootState::Unformatted);
  expect("FRAM sin formato no ofrece programa arrancable",
         !runtime.storage().hasBootableProgram());

  if (initialState != JWPLCLogicStorageBootState::Unformatted)
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
  Serial.println("Probara UNFORMATTED, EMPTY, ACTIVE, FALLBACK y NO_VALID.");
  Serial.println("El contenido original se respaldara y restaurara.");
  Serial.println("Escribe BOOT y pulsa Enviar para continuar.");

  if (!waitForCommand("BOOT"))
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

  const JWPLCLogicStorageBootState emptyState =
      runtime.storage().prepareBoot();
  expect("politica detecta almacenamiento vacio",
         emptyState == JWPLCLogicStorageBootState::Empty);
  expect("bootState informa EMPTY",
         runtime.storage().bootState() == JWPLCLogicStorageBootState::Empty);
  expect("almacenamiento vacio no ofrece programa arrancable",
         !runtime.storage().hasBootableProgram());

  const bool saveAOk =
      formatOk && runtime.storage().save(PROGRAM_A, PROGRAM_A_ID);
  expect("Programa A guardado", saveAOk);

  const JWPLCLogicStorageBootState activeAState =
      runtime.storage().prepareBoot();
  expect("politica carga Programa A activo",
         activeAState ==
             JWPLCLogicStorageBootState::ActiveProgramLoaded);
  expect("bootState informa programa activo",
         runtime.storage().bootState() ==
             JWPLCLogicStorageBootState::ActiveProgramLoaded);
  expect("Slot A activo y cargado",
         runtime.storage().status().activeSlot == 0 &&
             runtime.storage().status().lastLoadedSlot == 0);
  expect("Programa A conserva contenido", loadedMatches(PROGRAM_A));

  const bool saveBOk =
      saveAOk && runtime.storage().save(PROGRAM_B, PROGRAM_B_ID);
  expect("Programa B guardado", saveBOk);

  const JWPLCLogicStorageBootState activeBState =
      runtime.storage().prepareBoot();
  expect("politica carga Programa B activo",
         activeBState ==
             JWPLCLogicStorageBootState::ActiveProgramLoaded);
  expect("bootState conserva estado activo para B",
         runtime.storage().bootState() ==
             JWPLCLogicStorageBootState::ActiveProgramLoaded);
  expect("Slot B activo y cargado",
         runtime.storage().status().activeSlot == 1 &&
             runtime.storage().status().lastLoadedSlot == 1);
  expect("Programa B conserva contenido", loadedMatches(PROGRAM_B));

  uint8_t probe = 0;
  const size_t slotBImageAddress =
      runtime.storage().layout().slotB.address +
      LogicStorageLayout::SLOT_DESCRIPTOR_BYTES;
  const bool readBProbeOk = rawStorage.read(slotBImageAddress, &probe, 1);
  expect("byte de imagen B accesible para inyeccion", readBProbeOk);
  const bool corruptBOk =
      readBProbeOk && corruptFirstImageByte(runtime.storage().layout().slotB);
  expect("imagen activa B corrompida de forma controlada", corruptBOk);

  const bool reopenAfterBOk = runtime.storage().begin(JWPLC_FRAM);
  expect("fachada reabre despues de corromper B", reopenAfterBOk);

  const JWPLCLogicStorageBootState fallbackState =
      runtime.storage().prepareBoot();
  expect("politica carga fallback cuando B falla",
         fallbackState ==
             JWPLCLogicStorageBootState::FallbackProgramLoaded);
  expect("bootState informa FALLBACK_PROGRAM_LOADED",
         runtime.storage().bootState() ==
             JWPLCLogicStorageBootState::FallbackProgramLoaded);
  expect("superblock conserva B como activo durante fallback",
         runtime.storage().status().activeSlot == 1);
  expect("slot cargado durante fallback es A",
         runtime.storage().status().lastLoadedSlot == 0);
  expect("fallback reconstruye Programa A", loadedMatches(PROGRAM_A));
  expect("fallback se considera programa arrancable",
         runtime.storage().hasBootableProgram());

  const size_t slotAImageAddress =
      runtime.storage().layout().slotA.address +
      LogicStorageLayout::SLOT_DESCRIPTOR_BYTES;
  const bool readAProbeOk = rawStorage.read(slotAImageAddress, &probe, 1);
  expect("byte de imagen A accesible para inyeccion", readAProbeOk);
  const bool corruptAOk =
      readAProbeOk && corruptFirstImageByte(runtime.storage().layout().slotA);
  expect("imagen alterna A corrompida de forma controlada", corruptAOk);

  const bool reopenAfterBothOk = runtime.storage().begin(JWPLC_FRAM);
  expect("fachada reabre con ambas imagenes corruptas", reopenAfterBothOk);

  const JWPLCLogicStorageBootState invalidState =
      runtime.storage().prepareBoot();
  expect("politica detecta ausencia de programa valido",
         invalidState == JWPLCLogicStorageBootState::NoValidProgram);
  expect("bootState informa NO_VALID_PROGRAM",
         runtime.storage().bootState() ==
             JWPLCLogicStorageBootState::NoValidProgram);
  expect("ningun programa queda cargado con ambas imagenes corruptas",
         !runtime.storage().hasLoadedProgram());
  expect("ambas imagenes corruptas no ofrecen programa arrancable",
         !runtime.storage().hasBootableProgram());

  (void)restoreOriginalStore(true);
  printSummary();
}

void loop()
{
}
