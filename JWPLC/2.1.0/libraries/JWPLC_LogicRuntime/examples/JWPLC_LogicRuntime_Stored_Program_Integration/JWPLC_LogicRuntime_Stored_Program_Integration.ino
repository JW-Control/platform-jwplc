#include <JWPLC_GlobalPeripherals.h>
#include <JWPLC_LogicRuntime.h>
#include <Preferences.h>

#include <cstring>

static constexpr size_t PROGRAM_STORE_BYTES =
    JWPLCLogicStorageLayouts::FRAM_8K.programStoreBytes();
static constexpr size_t VERIFY_CHUNK_BYTES = 64;

static constexpr char NVS_NAMESPACE[] = "jwlr_rt_store";
static constexpr char NVS_KEY_STAGE[] = "stage";
static constexpr char NVS_KEY_BACKUP[] = "backup";
static constexpr char NVS_KEY_CRC[] = "crc";

static constexpr uint8_t STAGE_IDLE = 0;
static constexpr uint8_t STAGE_RESTORE_PENDING = 1;

static constexpr uint32_t PROGRAM_A_ID = 0xA101;
static constexpr uint32_t PROGRAM_B_ID = 0xB202;

static uint8_t backupBytes[PROGRAM_STORE_BYTES];
static uint8_t verifyChunk[VERIFY_CHUNK_BYTES];

static Preferences preferences;
static JWPLC_LogicRuntime runtime;
static LogicFRAMStorage rawStorage(JWPLC_FRAM, 0, PROGRAM_STORE_BYTES);

static uint16_t passedTests = 0;
static uint16_t failedTests = 0;

// Los programas no contienen bloques de salida. El ejemplo inicializa E/S,
// ejecuta scans y mantiene Q0 completamente apagado.
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
    "Runtime A",
    PROGRAM_A_BLOCKS,
    static_cast<uint16_t>(sizeof(PROGRAM_A_BLOCKS) /
                          sizeof(PROGRAM_A_BLOCKS[0]))};

static const LogicBlockDefinition PROGRAM_B_BLOCKS[] = {
    {LogicBlockType::DigitalInput,
     JWPLC_LOGIC_NO_SOURCE,
     JWPLC_LOGIC_NO_SOURCE,
     1,
     0},
    {LogicBlockType::DigitalInput,
     JWPLC_LOGIC_NO_SOURCE,
     JWPLC_LOGIC_NO_SOURCE,
     2,
     0},
    {LogicBlockType::Or,
     0,
     1,
     0,
     0}};

static const LogicProgram PROGRAM_B = {
    "Runtime B",
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
      runtime.prepareStoredProgram();
  if (printResult)
  {
    expect("runtime recupera estado original sin formato",
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
                     ? "INTEGRACION RUNTIME PERSISTENTE: PASS"
                     : "INTEGRACION RUNTIME PERSISTENTE: FAIL");
}

void setup()
{
  Serial.begin(115200);
  Serial.setTimeout(250);
  delay(300);

  Serial.println();
  Serial.println("JWPLC Logic Runtime - integracion persistente de alto nivel");
  Serial.println("Inicializa E/S, mantiene Q0 apagadas y no contiene bloques de salida.");
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

  const bool storageBeginOk = runtime.storage().begin(JWPLC_FRAM);
  expect("storage().begin(JWPLC_FRAM)", storageBeginOk);

  const bool runtimeBeginOk =
      storageBeginOk && runtime.begin(static_cast<uint32_t>(JWPLC_FRAM.size()));
  expect("runtime.begin() inicializa E/S", runtimeBeginOk);
  expect("runtime queda READY",
         runtimeBeginOk &&
             runtime.state() == JWPLCLogicRuntimeState::Ready);

  if (!runtimeBeginOk)
  {
    printSummary();
    return;
  }

  const JWPLCLogicStorageBootState initialState =
      runtime.prepareStoredProgram();
  expect("runtime detecta almacenamiento sin formato",
         initialState == JWPLCLogicStorageBootState::Unformatted);
  expect("runtime no conserva programa anterior",
         !runtime.hasProgram());

  const bool startWithoutProgram = runtime.start();
  expect("start() rechaza ausencia de programa", !startWithoutProgram);
  expect("error PROGRAM_NOT_LOADED informado",
         runtime.lastError() == JWPLCLogicRuntimeError::ProgramNotLoaded);

  Serial.println();
  Serial.println("ADVERTENCIA: esta prueba modificara temporalmente:");
  Serial.println("0x0000..0x143F (5184 bytes: superblocks + Slots A/B).");
  Serial.println("Validara prepareStoredProgram(), copia profunda y fallback.");
  Serial.println("El contenido original se respaldara y restaurara.");
  Serial.println("Escribe RUNTIME y pulsa Enviar para continuar.");

  if (!waitForCommand("RUNTIME"))
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
      runtime.prepareStoredProgram();
  expect("runtime clasifica almacenamiento vacio",
         emptyState == JWPLCLogicStorageBootState::Empty);
  expect("almacenamiento vacio descarga el motor",
         !runtime.hasProgram());

  const bool saveAOk =
      formatOk && runtime.storage().save(PROGRAM_A, PROGRAM_A_ID);
  expect("Programa A guardado", saveAOk);

  const JWPLCLogicStorageBootState activeAState =
      runtime.prepareStoredProgram();
  expect("Programa A preparado por API de alto nivel",
         activeAState ==
             JWPLCLogicStorageBootState::ActiveProgramLoaded);
  expect("motor conserva Programa A", runtime.hasProgram());
  expect("runtime permanece READY antes de start",
         runtime.state() == JWPLCLogicRuntimeState::Ready);

  const bool startAOk = runtime.start();
  expect("Programa A puede iniciar", startAOk);
  const bool tickAOk = startAOk && runtime.tick();
  expect("Programa A ejecuta un scan", tickAOk);
  runtime.stop();
  expect("Programa A se detiene con salidas apagadas",
         runtime.state() == JWPLCLogicRuntimeState::Stopped);

  // save() reutiliza buffers internos de storage(). El motor debe conservar A
  // porque LogicEngine ya posee una copia profunda independiente.
  const bool saveBOk = runtime.storage().save(PROGRAM_B, PROGRAM_B_ID);
  expect("Programa B guardado mientras A sigue cargado", saveBOk);
  expect("motor conserva copia independiente de A", runtime.hasProgram());

  const bool restartAOk = runtime.start();
  expect("copia de A puede reiniciar despues de save(B)", restartAOk);
  const bool retickAOk = restartAOk && runtime.tick();
  expect("copia de A ejecuta scan despues de reutilizar buffer", retickAOk);
  runtime.stop();

  const JWPLCLogicStorageBootState activeBState =
      runtime.prepareStoredProgram();
  expect("Programa B preparado como activo",
         activeBState ==
             JWPLCLogicStorageBootState::ActiveProgramLoaded);
  expect("motor reemplaza su copia por Programa B", runtime.hasProgram());
  expect("storage informa Slot B cargado",
         runtime.storage().status().activeSlot == 1 &&
             runtime.storage().status().lastLoadedSlot == 1);

  const bool startBOk = runtime.start();
  expect("Programa B puede iniciar", startBOk);
  const bool tickBOk = startBOk && runtime.tick();
  expect("Programa B ejecuta un scan", tickBOk);
  runtime.stop();

  const bool corruptBOk =
      corruptFirstImageByte(runtime.storage().layout().slotB);
  expect("imagen activa B corrompida de forma controlada", corruptBOk);

  const bool reopenBOk = runtime.storage().begin(JWPLC_FRAM);
  expect("fachada reabre despues de corromper B", reopenBOk);

  const JWPLCLogicStorageBootState fallbackState =
      runtime.prepareStoredProgram();
  expect("runtime prepara fallback A",
         fallbackState ==
             JWPLCLogicStorageBootState::FallbackProgramLoaded);
  expect("fallback queda cargado en el motor", runtime.hasProgram());
  expect("storage informa que se cargo Slot A",
         runtime.storage().status().activeSlot == 1 &&
             runtime.storage().status().lastLoadedSlot == 0);

  const bool startFallbackOk = runtime.start();
  expect("fallback puede iniciar solo tras start explicito", startFallbackOk);
  const bool tickFallbackOk = startFallbackOk && runtime.tick();
  expect("fallback ejecuta un scan", tickFallbackOk);
  runtime.stop();

  const bool corruptAOk =
      corruptFirstImageByte(runtime.storage().layout().slotA);
  expect("imagen alterna A corrompida de forma controlada", corruptAOk);

  const bool reopenBothOk = runtime.storage().begin(JWPLC_FRAM);
  expect("fachada reabre con ambas imagenes corruptas", reopenBothOk);

  const JWPLCLogicStorageBootState invalidState =
      runtime.prepareStoredProgram();
  expect("runtime detecta ausencia total de programa valido",
         invalidState == JWPLCLogicStorageBootState::NoValidProgram);
  expect("motor descarga el programa ante fallo total",
         !runtime.hasProgram());

  const bool startInvalidOk = runtime.start();
  expect("start() rechaza el fallo persistente", !startInvalidOk);
  expect("fallo total termina como PROGRAM_NOT_LOADED al iniciar",
         runtime.lastError() == JWPLCLogicRuntimeError::ProgramNotLoaded);

  (void)restoreOriginalStore(true);
  printSummary();
}

void loop()
{
}
