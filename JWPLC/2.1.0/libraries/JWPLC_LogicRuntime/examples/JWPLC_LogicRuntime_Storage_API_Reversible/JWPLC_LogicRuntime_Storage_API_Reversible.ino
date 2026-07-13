#include <JWPLC_GlobalPeripherals.h>
#include <JWPLC_LogicRuntime.h>
#include <Preferences.h>

#include <cstring>

static constexpr size_t PROGRAM_STORE_BYTES =
    JWPLCLogicStorageLayouts::FRAM_8K.programStoreBytes();
static constexpr size_t VERIFY_CHUNK_BYTES = 64;

static constexpr char NVS_NAMESPACE[] = "jwlr_api_rw";
static constexpr char NVS_KEY_STAGE[] = "stage";
static constexpr char NVS_KEY_BACKUP[] = "backup";
static constexpr char NVS_KEY_CRC[] = "crc";

static constexpr uint8_t STAGE_IDLE = 0;
static constexpr uint8_t STAGE_RESTORE_PENDING = 1;

static constexpr uint32_t TEST_PROGRAM_ID = 0xA001;

static uint8_t backupBytes[PROGRAM_STORE_BYTES];
static uint8_t verifyChunk[VERIFY_CHUNK_BYTES];

static Preferences preferences;
static JWPLC_LogicRuntime runtime;
static LogicFRAMStorage rawStorage(JWPLC_FRAM, 0, PROGRAM_STORE_BYTES);

static uint16_t passedTests = 0;
static uint16_t failedTests = 0;

static const LogicBlockDefinition TEST_BLOCKS[] = {
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

static const LogicProgram TEST_PROGRAM = {
    "API Reversible",
    TEST_BLOCKS,
    static_cast<uint16_t>(sizeof(TEST_BLOCKS) / sizeof(TEST_BLOCKS[0]))};

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

static bool sameLoadedProgram()
{
  const LogicProgram loaded = runtime.storage().activeProgram();

  if (loaded.name == nullptr || loaded.blocks == nullptr ||
      loaded.blockCount != TEST_PROGRAM.blockCount ||
      strcmp(loaded.name, TEST_PROGRAM.name) != 0)
  {
    return false;
  }

  return loaded.blocks[4].type == LogicBlockType::Ton &&
         loaded.blocks[4].sourceA == 3 &&
         loaded.blocks[4].parameter == 2000 &&
         loaded.blocks[5].type == LogicBlockType::DigitalOutput &&
         loaded.blocks[5].sourceA == 4 &&
         loaded.blocks[5].resource == 0;
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
                     ? "API PERSISTENTE REVERSIBLE: PASS"
                     : "API PERSISTENTE REVERSIBLE: FAIL");
}

void setup()
{
  Serial.begin(115200);
  Serial.setTimeout(250);
  delay(300);

  Serial.println();
  Serial.println("JWPLC Logic Runtime - API persistente reversible");
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
  Serial.println("El contenido original se respaldara en NVS y se restaurara.");
  Serial.println("Si ocurre un reinicio, la recuperacion sera automatica.");
  Serial.println("Escribe FORMAT y pulsa Enviar para continuar.");

  if (!waitForCommand("FORMAT"))
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

  const bool saveOk = formatOk &&
                      runtime.storage().save(TEST_PROGRAM, TEST_PROGRAM_ID);
  expect("storage().save() guarda programa valido", saveOk);

  const LogicProgramStoreStatus &savedStatus = runtime.storage().status();
  expect("primer guardado activa Slot A",
         saveOk && savedStatus.activeSlot == 0);
  expect("Program ID conservado",
         saveOk && savedStatus.programId == TEST_PROGRAM_ID);
  expect("primera generacion automatica es 1",
         saveOk && savedStatus.generation == 1);

  const bool loadOk = saveOk && runtime.storage().loadActive();
  expect("storage().loadActive() reconstruye programa", loadOk);
  expect("fachada reporta programa cargado",
         loadOk && runtime.storage().hasLoadedProgram());
  expect("nombre y cantidad de bloques conservados",
         loadOk && sameLoadedProgram());
  expect("TON y salida conservados",
         loadOk && sameLoadedProgram());

  const bool reopenOk = runtime.storage().begin(JWPLC_FRAM);
  expect("storage().begin() reabre el mapa persistido", reopenOk);
  expect("formato detectado despues de reinicializar",
         reopenOk && runtime.storage().isFormatted());

  const LogicProgramStoreStatus &reopenedStatus = runtime.storage().status();
  expect("estado A/B persiste tras reinicializar",
         reopenOk && reopenedStatus.activeSlot == 0 &&
             reopenedStatus.programId == TEST_PROGRAM_ID &&
             reopenedStatus.generation == 1);

  const bool reloadOk = reopenOk && runtime.storage().loadActive();
  expect("programa vuelve a cargar tras reinicializar", reloadOk);
  expect("programa reabierto conserva su contenido",
         reloadOk && sameLoadedProgram());

  (void)restoreOriginalStore(true);
  printSummary();
}

void loop()
{
}
