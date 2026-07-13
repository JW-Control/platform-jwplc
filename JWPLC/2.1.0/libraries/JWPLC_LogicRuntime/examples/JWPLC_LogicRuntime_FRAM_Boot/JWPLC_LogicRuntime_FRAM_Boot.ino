#include <JWPLC_GlobalPeripherals.h>
#include <JWPLC_LogicRuntime.h>
#include <Preferences.h>

#include <cstring>

static constexpr size_t PHYSICAL_FRAM_BYTES = 8UL * 1024UL;
static constexpr size_t TEST_SLOT_BYTES = 512;
static constexpr size_t TEST_WINDOW_BYTES = 64 + (2 * TEST_SLOT_BYTES);
static constexpr size_t TEST_BASE_ADDRESS =
    PHYSICAL_FRAM_BYTES - TEST_WINDOW_BYTES;
static constexpr size_t SCRATCH_BYTES = TEST_SLOT_BYTES;

static constexpr char NVS_NAMESPACE[] = "jwlrpoc7";
static constexpr char NVS_KEY_STAGE[] = "stage";
static constexpr char NVS_KEY_BACKUP[] = "backup";
static constexpr char NVS_KEY_CRC[] = "backup_crc";

static constexpr uint8_t STAGE_IDLE = 0;
static constexpr uint8_t STAGE_PROGRAM_READY = 1;
static constexpr uint8_t STAGE_RESTORE_PENDING = 2;

static constexpr LogicStorageProfile TEST_PROFILE(
    PHYSICAL_FRAM_BYTES,
    TEST_SLOT_BYTES,
    0,
    32,
    true);

static uint8_t backupBytes[TEST_WINDOW_BYTES];
static uint8_t verifyBytes[TEST_WINDOW_BYTES];
static uint8_t scratch[SCRATCH_BYTES];
static LogicProgramBuffer loadedProgram;

static LogicFRAMStorage storage(
    JWPLC_FRAM,
    TEST_BASE_ADDRESS,
    TEST_WINDOW_BYTES);

static Preferences preferences;
static JWPLC_LogicRuntime runtime;

static bool runtimeActive = false;
static uint32_t lastReportMs = 0;

static const LogicBlockDefinition BOOT_PROGRAM_BLOCKS[] = {
    // 0: I0_0
    {LogicBlockType::DigitalInput,
     JWPLC_LOGIC_NO_SOURCE,
     JWPLC_LOGIC_NO_SOURCE,
     0,
     0},

    // 1: I0_1
    {LogicBlockType::DigitalInput,
     JWPLC_LOGIC_NO_SOURCE,
     JWPLC_LOGIC_NO_SOURCE,
     1,
     0},

    // 2: NOT I0_1
    {LogicBlockType::Not,
     1,
     JWPLC_LOGIC_NO_SOURCE,
     0,
     0},

    // 3: I0_0 AND NOT I0_1
    {LogicBlockType::And,
     0,
     2,
     0,
     0},

    // 4: TON 2 segundos
    {LogicBlockType::Ton,
     3,
     JWPLC_LOGIC_NO_SOURCE,
     0,
     2000},

    // 5: Q0_0
    {LogicBlockType::DigitalOutput,
     4,
     JWPLC_LOGIC_NO_SOURCE,
     0,
     0}};

static const LogicProgram BOOT_PROGRAM = {
    "FRAM Boot Runtime",
    BOOT_PROGRAM_BLOCKS,
    static_cast<uint16_t>(sizeof(BOOT_PROGRAM_BLOCKS) /
                          sizeof(BOOT_PROGRAM_BLOCKS[0]))};

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
  if (!storage.read(0, backupBytes, sizeof(backupBytes)))
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

static bool restoreOriginalWindow()
{
  if (!loadBackupFromNVS())
  {
    Serial.println("FAIL: respaldo NVS ausente o corrupto.");
    return false;
  }

  if (!storage.write(0, backupBytes, sizeof(backupBytes)) ||
      !storage.read(0, verifyBytes, sizeof(verifyBytes)) ||
      memcmp(backupBytes, verifyBytes, sizeof(backupBytes)) != 0)
  {
    Serial.println("FAIL: no se pudo restaurar exactamente la ventana FRAM.");
    return false;
  }

  const uint32_t expectedCrc = preferences.getUInt(NVS_KEY_CRC, 0);
  if (LogicProgramCodec::crc32(verifyBytes, sizeof(verifyBytes)) != expectedCrc)
  {
    Serial.println("FAIL: CRC de la ventana restaurada no coincide.");
    return false;
  }

  const bool clearOk = preferences.clear();

  Serial.println("[PASS] Contenido original de FRAM restaurado exactamente.");
  Serial.println(clearOk ? "[PASS] Estado temporal NVS eliminado."
                         : "[WARN] No se pudo limpiar NVS; la FRAM si fue restaurada.");
  Serial.println();
  Serial.println("ARRANQUE Y EJECUCION DESDE FRAM: PASS");
  Serial.println("PoC 7 completada.");
  return true;
}

static bool installProgram()
{
  Serial.println();
  Serial.println("ADVERTENCIA: esta prueba usara temporalmente los ultimos");
  Serial.print(TEST_WINDOW_BYTES);
  Serial.println(" bytes de la FRAM fisica.");
  Serial.println("El contenido original se respaldara en NVS.");
  Serial.println("Escribe START y pulsa Enviar para instalar el programa.");

  if (!waitForCommand("START"))
  {
    return false;
  }

  if (!createPersistentBackup())
  {
    Serial.println("FAIL: no se pudo crear el respaldo persistente.");
    return false;
  }

  LogicProgramStore store;
  if (!store.begin(storage, TEST_PROFILE) ||
      !store.format() ||
      !store.saveProgram(BOOT_PROGRAM,
                         0x7001,
                         1,
                         0,
                         scratch,
                         sizeof(scratch)))
  {
    Serial.print("FAIL: no se pudo instalar el programa. Error: ");
    Serial.println(LogicProgramStore::errorName(store.lastError()));
    return false;
  }

  LogicProgramStore verifyStore;
  if (!verifyStore.begin(storage, TEST_PROFILE) ||
      !verifyStore.loadActive(loadedProgram, scratch, sizeof(scratch)) ||
      loadedProgram.metadata.programId != 0x7001 ||
      loadedProgram.metadata.generation != 1 ||
      strcmp(loadedProgram.name, BOOT_PROGRAM.name) != 0 ||
      LogicValidator::validate(loadedProgram.asProgram(),
                               TEST_PROFILE.maxBlocks,
                               8,
                               8) != LogicValidationError::None)
  {
    Serial.println("FAIL: el programa instalado no se pudo reconstruir.");
    return false;
  }

  if (preferences.putUChar(NVS_KEY_STAGE, STAGE_PROGRAM_READY) !=
      sizeof(uint8_t))
  {
    Serial.println("FAIL: no se pudo registrar la etapa de arranque.");
    return false;
  }

  Serial.println("[PASS] Programa persistido y verificado en FRAM.");
  Serial.println("INSTALACION COMPLETA.");
  Serial.println("Pulsa RESET o corta y restablece la alimentacion.");
  return true;
}

static bool startRuntimeFromFRAM()
{
  if (!loadBackupFromNVS())
  {
    Serial.println("FAIL: respaldo NVS ausente o corrupto.");
    return false;
  }

  LogicProgramStore store;
  if (!store.begin(storage, TEST_PROFILE) ||
      !store.loadActive(loadedProgram, scratch, sizeof(scratch)))
  {
    Serial.print("FAIL: no se pudo cargar el programa activo. Error: ");
    Serial.println(LogicProgramStore::errorName(store.lastError()));
    return false;
  }

  if (loadedProgram.metadata.programId != 0x7001 ||
      loadedProgram.metadata.generation != 1 ||
      strcmp(loadedProgram.name, BOOT_PROGRAM.name) != 0)
  {
    Serial.println("FAIL: metadatos inesperados en el programa activo.");
    return false;
  }

  if (!runtime.begin())
  {
    Serial.print("FAIL: runtime.begin(): ");
    Serial.println(JWPLC_LogicRuntime::errorName(runtime.lastError()));
    return false;
  }

  if (!runtime.loadProgram(loadedProgram.asProgram()))
  {
    Serial.print("FAIL: runtime.loadProgram(): ");
    Serial.println(LogicValidator::errorName(runtime.validationError()));
    return false;
  }

  if (!runtime.start())
  {
    Serial.print("FAIL: runtime.start(): ");
    Serial.println(JWPLC_LogicRuntime::errorName(runtime.lastError()));
    return false;
  }

  runtimeActive = true;
  lastReportMs = millis();

  Serial.println("[PASS] Programa cargado desde FRAM hacia RAM.");
  Serial.println("[PASS] Runtime iniciado con el programa persistente.");
  Serial.println();
  Serial.println("Logica activa: I0_0 AND NOT I0_1 -> TON 2 s -> Q0_0");
  Serial.println("Prueba las entradas y confirma el comportamiento de Q0_0.");
  Serial.println("Escribe RESTORE para detener, apagar salidas y restaurar FRAM.");
  return true;
}

static void requestRestore()
{
  runtime.stop();
  runtimeActive = false;

  if (preferences.putUChar(NVS_KEY_STAGE, STAGE_RESTORE_PENDING) !=
      sizeof(uint8_t))
  {
    Serial.println("FAIL: no se pudo registrar la restauracion pendiente.");
    return;
  }

  Serial.println("Runtime detenido. Q0 apagadas.");
  (void)restoreOriginalWindow();
}

void setup()
{
  Serial.begin(115200);
  Serial.setTimeout(250);
  delay(300);

  Serial.println();
  Serial.println("JWPLC Logic Runtime - PoC 7 arranque desde FRAM");
  Serial.println();

  Serial.print("FRAM detectada: ");
  Serial.print(JWPLC_FRAM.size());
  Serial.println(" bytes");
  Serial.print("Ventana persistente: 0x");
  Serial.print(TEST_BASE_ADDRESS, HEX);
  Serial.print("..0x");
  Serial.println(TEST_BASE_ADDRESS + TEST_WINDOW_BYTES - 1, HEX);

  if (JWPLC_FRAM.size() != PHYSICAL_FRAM_BYTES || !storage.begin())
  {
    Serial.println("PRUEBA ABORTADA: FRAM o ventana no disponibles.");
    return;
  }

  if (!preferences.begin(NVS_NAMESPACE, false))
  {
    Serial.println("PRUEBA ABORTADA: no se pudo abrir NVS.");
    return;
  }

  const uint8_t stage = preferences.getUChar(NVS_KEY_STAGE, STAGE_IDLE);

  switch (stage)
  {
  case STAGE_IDLE:
    (void)installProgram();
    break;

  case STAGE_PROGRAM_READY:
    (void)startRuntimeFromFRAM();
    break;

  case STAGE_RESTORE_PENDING:
    Serial.println("Restauracion pendiente detectada tras reinicio.");
    (void)restoreOriginalWindow();
    break;

  default:
    Serial.println("PRUEBA ABORTADA: etapa NVS desconocida.");
    Serial.println("No se modifico la FRAM en este arranque.");
    break;
  }
}

void loop()
{
  if (!runtimeActive)
  {
    delay(10);
    return;
  }

  if (!runtime.tick())
  {
    Serial.print("FALLO DE RUNTIME: ");
    Serial.println(JWPLC_LogicRuntime::errorName(runtime.lastError()));
    runtime.stop();
    runtimeActive = false;
    return;
  }

  if (Serial.available() > 0)
  {
    String command = Serial.readStringUntil('\n');
    command.trim();

    if (command == "RESTORE")
    {
      requestRestore();
      return;
    }

    if (command.length() > 0)
    {
      Serial.println("Comando no reconocido. Escribe exactamente RESTORE.");
    }
  }

  const uint32_t now = millis();
  if (static_cast<uint32_t>(now - lastReportMs) >= 1000)
  {
    lastReportMs = now;

    Serial.print("I0_0=");
    Serial.print(runtime.blockValue(0));
    Serial.print(" I0_1=");
    Serial.print(runtime.blockValue(1));
    Serial.print(" AND=");
    Serial.print(runtime.blockValue(3));
    Serial.print(" TON=");
    Serial.print(runtime.blockValue(4));
    Serial.print(" Q0_0=");
    Serial.print(runtime.blockValue(5));
    Serial.print(" | scan us [last/min/avg/max]=");
    Serial.print(runtime.lastScanMicros());
    Serial.print('/');
    Serial.print(runtime.minScanMicros());
    Serial.print('/');
    Serial.print(runtime.averageScanMicros());
    Serial.print('/');
    Serial.print(runtime.maxScanMicros());
    Serial.print(" | escrituras Q0=");
    Serial.println(runtime.outputWriteCount());
  }

  delay(1);
}
