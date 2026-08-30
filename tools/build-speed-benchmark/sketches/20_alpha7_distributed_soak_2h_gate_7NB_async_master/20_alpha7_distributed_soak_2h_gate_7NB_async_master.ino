/*
  ============================================================================
  JWPLC BASIC - DISTRIBUTED SOAK / BURN-IN TEST 2H
  Sketch 20 - alpha.7 integration stress

  MISMO .ino PARA TODOS LOS JWPLC.

  Asignacion inicial por USB / Serial:
      M2      -> Master esperando 2 Slaves
      M4      -> Master esperando 4 Slaves
      S1      -> Slave Modbus ID 1
      S2      -> Slave Modbus ID 2
      ...
      S8      -> Slave Modbus ID 8

  Otros comandos por Serial:
      STATUS  -> resumen local
      DIAG    -> diagnostico runtime/core/Modbus del gate Alpha7
      START   -> solo Master: inicia soak cuando commissioning + RTC estan OK
      STOP    -> Master: detiene el soak
      WIFI    -> reabre AP de provisioning
      RESYNC  -> Master: fuerza nueva sincronizacion RTC inicial
      ETHNEXT -> Master: termina ventana Ethernet actual y pide mover cable
      CLEAR   -> borra identidad/configuracion del soak y reinicia

  Nombre BLE:
      JWPLC_M2
      JWPLC_S1
      JWPLC_S2
      ...

  AP de provisioning:
      JWPLC_M2_SETUP
      JWPLC_S1_SETUP
      ...

  Secuencia general:
    1. Asignar rol por USB.
    2. Conectar por BLE al nombre JWPLC_XX -> BLE PASS.
    3. Provisionar WiFi desde AP local -> STA + HTTP a PC -> WIFI PASS.
    4. Formar red Modbus RTU.
    5. Master espera todos los Slaves y sus prechecks.
    6. Cable Ethernet inicialmente en Master.
    7. Master obtiene hora NTP por W5500 y sincroniza RTC de toda la red.
    8. Enviar START al Master.
    9. Durante el soak:
         - TCA + Q0.x -> I0.x loopback fisico
         - conteo de pulsos ordenados/detectados
         - Modbus RTU FC06 + FC03
         - FRAM writeBlock/readBlock/compare
         - RTC read / avance / temperatura
         - BLE qualification durante commissioning (fase separada)
         - WiFi HTTP continuo hacia PC durante commissioning/soak
         - BLE y WiFi NO se mantienen simultaneos en este acceptance
         - TFT IDLE + USER
         - Ethernet HTTP + NTP durante la ventana del nodo
         - microSD solamente en Master
         - uptime / boot counter / heap / max loop
   10. Rotar un solo cable Ethernet entre nodos.
   11. Al completar todas las ventanas, detener Q y emitir resumen.

  Cableado de loopback por cada JWPLC:
      COM de reles -> +24 VDC
      NO Q0.0 -> I0.0
      NO Q0.1 -> I0.1
      ...
      NO Q0.7 -> I0.7
      GND de entradas -> 0 V

  RS-485:
      P/A en daisy chain
      N/B en daisy chain
      Terminacion 120 ohm SOLO en los dos extremos fisicos.
      Bias SOLO en un punto de la red (normalmente el extremo Master).

  IMPORTANTE:
    - Este sketch activa fisicamente los reles.
    - Ejecutar sin cargas/actuadores reales conectados.
    - Cadencia por canal: ~1.5 s ON + ~1.5 s OFF.
    - Cada rele realiza aproximadamente 300 activaciones en 2 horas.
    - ERR se reserva para fallas de la aplicacion/soak.
    - BUS y ETH permanecen con diagnostico automatico del package.

  Target:
      JWPLC Basic
      platform-jwplc 2.1.0-alpha.7
  ============================================================================
*/

#include <Arduino.h>

// Necesario aqui porque USER dibuja directamente sobre Adafruit_ST7789.
// JWPLC_ModbusRTU y los perifericos globales siguen llegando por el package.
#include <JWPLC_Display.h>

#include <WiFi.h>
#include <WebServer.h>
#include <WiFiUdp.h>
#include <Preferences.h>

#include <BLEDevice.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>

#include <ctype.h>
#include <string.h>

// ============================================================================
// Configuracion general
// ============================================================================

static constexpr uint32_t SERIAL_BAUD = 115200UL;

static constexpr uint8_t MAX_SLAVES = 8;
static constexpr uint8_t MASTER_MODBUS_LOCAL_ID = 247;

static constexpr uint32_t MODBUS_BAUD = 115200UL;
static constexpr uint32_t MODBUS_CONFIG = SERIAL_8N1;
static constexpr uint32_t MODBUS_TIMEOUT_MS = 250UL;
static constexpr uint32_t MASTER_POLL_GAP_MS = 80UL;

// Alpha7 Gate 7NB:
// La API Master de runtime usa request...() + task() de forma cooperativa.
// Las operaciones ...Sync() se conservan solamente para commissioning,
// comandos poco frecuentes y compatibilidad. El poll periodico del soak NO
// puede bloquear el loop critico del PLC.

static constexpr uint8_t BUZZER_PIN = 26;

// Dos horas de ventanas Ethernet efectivas.
// Los tiempos de cambio de cable se agregan al tiempo real de banco.
static constexpr uint32_t SOAK_TARGET_MS = 2UL * 60UL * 60UL * 1000UL;
static constexpr uint32_t ETH_MIN_WINDOW_MS = 15UL * 60UL * 1000UL;
static constexpr uint32_t ETH_MAX_WINDOW_MS = 30UL * 60UL * 1000UL;

// I/O:
// 1.5 s ON + 1.5 s OFF por canal.
static constexpr uint32_t IO_PHASE_MS = 1500UL;
static constexpr uint32_t IO_SETTLE_MS = 180UL;
static constexpr uint32_t IO_VERIFY_DEADLINE_MS = 500UL;
static constexpr uint32_t IO_SCAN_MS = 20UL;
// Alpha7 Gate RT:
    // Sin stagger intencional: queda solo la diferencia propia del despacho Modbus.
    static constexpr uint32_t IO_NODE_STAGGER_MS = 0UL;

// Stress perifericos.
static constexpr uint32_t FRAM_PERIOD_MS = 10000UL;
static constexpr uint32_t RTC_PERIOD_MS = 1000UL;
static constexpr uint32_t RTC_TEMP_PERIOD_MS = 30000UL;
static constexpr uint32_t WIFI_HTTP_PERIOD_MS = 2000UL;
static constexpr uint32_t ETH_HTTP_PERIOD_MS = 1000UL;
static constexpr uint32_t SD_LOG_PERIOD_MS = 5000UL;
static constexpr uint32_t SD_RW_PERIOD_MS = 30000UL;
static constexpr uint32_t MASTER_COMMISSION_PRINT_MS = 5000UL;

// Gate Alpha7 - runtime / multicore diagnostics.
static constexpr uint32_t RUNTIME_DIAG_PRINT_MS = 10000UL;
static constexpr uint32_t LONG_LOOP_WARN_US = 50000UL;
static constexpr uint32_t LONG_LOOP_CRIT_US = 250000UL;

// HTTP WiFi de aplicacion fuera del loop Arduino.
// El worker evita que WiFiClient.connect()/ACK bloquee Modbus en Core 1.
static constexpr uint32_t WIFI_WORKER_STACK_BYTES = 6144UL;
static constexpr UBaseType_t WIFI_WORKER_PRIORITY = 1;
static constexpr BaseType_t WIFI_WORKER_CORE = 0;
static constexpr size_t WIFI_TELEMETRY_BODY_MAX = 512;


// Red.
static constexpr uint16_t DEFAULT_PC_PORT = 8080;
static constexpr uint16_t NTP_PORT = 123;
static constexpr uint16_t NTP_LOCAL_PORT_BASE = 2390;
static constexpr uint32_t NTP_TIMEOUT_MS = 1800UL;
static constexpr uint32_t NTP_UNIX_DELTA = 2208988800UL;
static constexpr int32_t LIMA_UTC_OFFSET_SECONDS = -5L * 3600L;

static const char NTP_HOST[] = "pool.ntp.org";

// Telemetria.
static constexpr uint32_t WIFI_CONNECT_RETRY_MS = 10000UL;
static constexpr uint32_t WIFI_PROVISION_REOPEN_MS = 30000UL;
static constexpr uint8_t WIFI_FAIL_STREAK_TO_LATCH = 5;

// UI.
static constexpr uint32_t USER_PAGE_MS = 5000UL;
static constexpr uint32_t USER_REFRESH_MS = 500UL;

// FRAM scratch al final de la memoria.
// Se respalda en RAM al iniciar y se restaura al finalizar normalmente.
static constexpr uint16_t FRAM_SCRATCH_BYTES = 128;
static constexpr uint8_t FRAM_RECORD_VERSION = 1;

// Paths microSD Master.
static const char SD_SNAPSHOT_PATH[] = "/SOAK_NODES.CSV";
static const char SD_EVENTS_PATH[] = "/SOAK_EVENTS.CSV";
static const char SD_RW_PATH[] = "/SOAK_RW.TMP";

// ============================================================================
// BLE UUIDs del acceptance test
// ============================================================================

static const char BLE_SERVICE_UUID[] =
    "8b7b0001-5d7e-4b72-a6c0-4a574a57504c";
static const char BLE_STATUS_UUID[] =
    "8b7b0002-5d7e-4b72-a6c0-4a574a57504c";

// ============================================================================
// Tipos y estados
// ============================================================================

enum NodeRole : uint8_t
{
    ROLE_NONE = 0,
    ROLE_MASTER = 1,
    ROLE_SLAVE = 2
};

enum SoakState : uint16_t
{
    SOAK_NEED_ROLE = 0,
    SOAK_COMMISSIONING = 1,
    SOAK_WAIT_RTC_SYNC = 2,
    SOAK_READY_TO_START = 3,
    SOAK_RUNNING = 4,
    SOAK_COMPLETE = 5,
    SOAK_STOPPED = 6
};

enum ErrorCode : uint16_t
{
    ERR_NONE = 0,
    ERR_IO = 1,
    ERR_FRAM = 2,
    ERR_RTC = 3,
    ERR_WIFI = 4,
    ERR_ETH = 5,
    ERR_MODBUS = 6,
    ERR_SD = 7,
    ERR_RESET = 8,
    ERR_NTP = 9
};

enum CommandCode : uint16_t
{
    CMD_NONE = 0,
    CMD_START_SOAK = 1,
    CMD_STOP_SOAK = 2,
    CMD_SYNC_RTC = 3,
    CMD_BEEP = 4
};

enum StatusFlag : uint16_t
{
    ST_ROLE = 1u << 0,
    ST_BLE_PASS = 1u << 1,
    ST_WIFI_PASS = 1u << 2,
    ST_MODBUS_READY = 1u << 3,
    ST_RTC_SYNCED = 1u << 4,
    ST_FRAM_READY = 1u << 5,
    ST_SOAK_RUNNING = 1u << 6,
    ST_IO_OK = 1u << 7,
    ST_ETH_READY = 1u << 8,
    ST_ETH_WINDOW_ACTIVITY = 1u << 9,
    ST_SD_READY = 1u << 10,
    ST_ERROR_LATCH = 1u << 11,
    ST_NTP_VALID = 1u << 12,
    ST_WIFI_HTTP_ACTIVE = 1u << 13,
    ST_ETH_HTTP_ACTIVE = 1u << 14,
    ST_ALL_COMMISSIONED = 1u << 15
};

// ============================================================================
// Mapa Modbus Holding Registers v1
//
// Todos los nodos Slave exponen exactamente el mismo mapa.
// El Master lee el mapa completo por FC03 y usa FC06 para challenge/comandos.
//
// 32-bit = dos registros, HIGH primero.
// ============================================================================

enum HoldingRegister : uint16_t
{
    HR_SIGNATURE = 0,         // 0x4A57
    HR_PROTOCOL_VERSION = 1,  // 1
    HR_NODE_ID = 2,           // 1..N
    HR_STATUS_FLAGS = 3,
    HR_SOAK_STATE = 4,

    HR_UPTIME_HI = 5,
    HR_UPTIME_LO = 6,
    HR_BOOT_COUNT_HI = 7,
    HR_BOOT_COUNT_LO = 8,

    HR_Q_BITMAP = 9,
    HR_I_BITMAP = 10,
    HR_IO_CHANNEL = 11,

    HR_IO_MISMATCH_HI = 12,
    HR_IO_MISMATCH_LO = 13,

    // Q pulse counters: 8 x uint32_t = 16 registers [14..29]
    HR_Q_PULSE_BASE = 14,

    // I pulse counters: 8 x uint32_t = 16 registers [30..45]
    HR_I_PULSE_BASE = 30,

    HR_FRAM_OK_HI = 46,
    HR_FRAM_OK_LO = 47,
    HR_FRAM_FAIL_HI = 48,
    HR_FRAM_FAIL_LO = 49,

    HR_RTC_EPOCH_HI = 50,
    HR_RTC_EPOCH_LO = 51,
    HR_RTC_FAIL_HI = 52,
    HR_RTC_FAIL_LO = 53,

    HR_WIFI_HTTP_OK_HI = 54,
    HR_WIFI_HTTP_OK_LO = 55,
    HR_WIFI_HTTP_FAIL_HI = 56,
    HR_WIFI_HTTP_FAIL_LO = 57,

    HR_ETH_HTTP_OK_HI = 58,
    HR_ETH_HTTP_OK_LO = 59,
    HR_ETH_HTTP_FAIL_HI = 60,
    HR_ETH_HTTP_FAIL_LO = 61,

    HR_BLE_CONNECTIONS = 62,
    HR_WIFI_RSSI = 63,        // int16_t reinterpretado
    HR_NTP_DRIFT_SECONDS = 64,// int16_t reinterpretado

    HR_FIRST_ERROR = 65,
    HR_ERROR_COUNT = 66,

    HR_MIN_FREE_HEAP_HI = 67,
    HR_MIN_FREE_HEAP_LO = 68,
    HR_MAX_LOOP_US_HI = 69,
    HR_MAX_LOOP_US_LO = 70,

    // FC06 continuo Master -> Slave para probar escritura/readback.
    HR_CHALLENGE = 71,

    // Comando atomico: primero args/code, CMD_SEQ se escribe al final.
    HR_CMD_CODE = 72,
    HR_CMD_ARG0 = 73,
    HR_CMD_ARG1 = 74,
    HR_TIME_HI = 75,
    HR_TIME_LO = 76,
    HR_ETH_OWNER = 77,
    HR_CMD_SEQ = 78,

    HR_SD_STATUS = 79,        // Master local; en Slave queda 0
    HR_ETH_WINDOWS_OK = 80,
    HR_ETH_WINDOWS_FAIL = 81,

    // Alpha7 Gate RT diagnostics.
    HR_MODBUS_CRC_HI = 82,
    HR_MODBUS_CRC_LO = 83,
    HR_MODBUS_CRC_DELTA_HI = 84,
    HR_MODBUS_CRC_DELTA_LO = 85,
    HR_LAST_CRC_AGE_MS_HI = 86,
    HR_LAST_CRC_AGE_MS_LO = 87,
    HR_LONG_LOOP_50MS_HI = 88,
    HR_LONG_LOOP_50MS_LO = 89,
    HR_LONG_LOOP_250MS_HI = 90,
    HR_LONG_LOOP_250MS_LO = 91,
    HR_WIFI_MAX_LATENCY_MS_HI = 92,
    HR_WIFI_MAX_LATENCY_MS_LO = 93,

    HR_COUNT = 94
};

static constexpr uint16_t MODBUS_SIGNATURE = 0x4A57;
static constexpr uint16_t MODBUS_PROTOCOL_VERSION = 2;
static constexpr uint16_t ETH_OWNER_NONE = 0xFFFF;

// ============================================================================
// Estructuras
// ============================================================================

struct PersistentConfig
{
    bool roleValid = false;
    NodeRole role = ROLE_NONE;
    uint8_t nodeId = 0;
    uint8_t expectedSlaves = 0;

    bool wifiValid = false;
    String wifiSsid;
    String wifiPass;
    String pcIp;
    uint16_t pcPort = DEFAULT_PC_PORT;

    bool bleQualified = false;
    uint32_t bootCount = 0;
};

struct IOStressState
{
    bool running = false;
    bool waitingInitialStagger = false;
    bool onPhase = false;
    bool phaseFailureLatched = false;
    bool pulseDetectedThisOn = false;

    uint8_t channel = 0;
    uint8_t outputBitmap = 0;
    uint8_t inputBitmap = 0;

    uint32_t phaseStartMs = 0;
    uint32_t startAfterMs = 0;
    uint32_t lastScanMs = 0;

    uint32_t qPulses[8] = {};
    uint32_t iPulses[8] = {};
    uint32_t mismatches = 0;
};

struct FramProbeRecord
{
    uint32_t magic;
    uint32_t sequence;
    uint32_t rtcEpoch;
    uint32_t pulseTotal;
    uint8_t nodeId;
    uint8_t qBitmap;
    uint8_t iBitmap;
    uint8_t reserved;
};

struct FramBackup
{
    uint8_t bytes[FRAM_SCRATCH_BYTES];
};

struct NodeSnapshot
{
    bool seen = false;
    uint32_t lastSeenMs = 0;
    uint32_t consecutivePollFails = 0;

    uint16_t regs[HR_COUNT] = {};

    bool bootKnown = false;
    uint32_t lastBootCount = 0;

    uint16_t commandSequence = 0;
    uint16_t lastChallengeSent = 0;
};

struct HttpStats
{
    uint32_t ok = 0;
    uint32_t fail = 0;
    uint32_t failStreak = 0;
    uint32_t maxFailStreak = 0;
    uint32_t lastLatencyMs = 0;
    uint32_t maxLatencyMs = 0;
    uint32_t sequence = 0;
    uint32_t lastSuccessMs = 0;
};


struct WifiTelemetryJob
{
    uint32_t sequence = 0;
    char body[WIFI_TELEMETRY_BODY_MAX] = {};
};

struct WifiTelemetryResult
{
    uint32_t sequence = 0;
    uint32_t latencyMs = 0;
    bool ok = false;
};

struct ModbusTimingStats
{
    uint32_t pollOk = 0;
    uint32_t pollFail = 0;
    uint32_t writeFail = 0;
    uint32_t readFail = 0;
    uint32_t validateFail = 0;
    uint32_t lastWriteUs = 0;
    uint32_t maxWriteUs = 0;
    uint32_t lastReadUs = 0;
    uint32_t maxReadUs = 0;
    uint32_t lastPollUs = 0;
    uint32_t maxPollUs = 0;
};

enum MasterRuntimePollPhase : uint8_t
{
    MASTER_RT_POLL_IDLE = 0,
    MASTER_RT_POLL_WAIT_WRITE,
    MASTER_RT_POLL_WAIT_READ
};

struct EthernetWindowState
{
    uint16_t owner = ETH_OWNER_NONE;

    bool localWasOwner = false;
    bool ntpDone = false;
    bool activity = false;

    uint32_t startOk = 0;
    uint32_t startFail = 0;

    uint16_t windowsOk = 0;
    uint16_t windowsFail = 0;
};

// ============================================================================
// Globals
// ============================================================================

PersistentConfig cfg;

Preferences preferences;
WebServer provisionServer(80);

BLEServer *bleServer = nullptr;
BLECharacteristic *bleStatusCharacteristic = nullptr;

bool bleInitialized = false;
bool bleConnected = false;
uint16_t bleConnections = 0;

// Alpha7 radio commissioning:
// Bluedroid BLE y WiFi se validan en fases separadas para no forzar ambos
// stacks simultaneamente en el ESP32-WROOM. Tras BLE PASS se reinicia de
// forma controlada; el siguiente boot salta BLE y abre WiFi provisioning.
bool blePassRestartPending = false;
uint32_t blePassRestartAtMs = 0;
static constexpr uint32_t BLE_PASS_RESTART_DELAY_MS = 1200UL;

bool provisioningApActive = false;
bool provisioningServerStarted = false;

// Alpha7 WiFi commissioning:
// Las credenciales se guardan desde SoftAP y se aplican tras un reboot
// controlado. Esto evita lanzar WiFi.begin() mientras AP+STA o el driver
// interno todavia estan procesando una conexion previa.
bool pendingWifiReboot = false;
uint32_t pendingWifiRebootAtMs = 0;
static constexpr uint32_t WIFI_SAVE_REBOOT_DELAY_MS = 1400UL;

uint32_t wifiStaStartMs = 0;
bool wifiStaAttemptIssued = false;

bool wifiPass = false;
bool wifiHttpActive = false;
IPAddress pcIp;
HttpStats wifiHttp;
HttpStats ethHttp;

bool modbusReady = false;
uint16_t holdingRegisters[HR_COUNT] = {};
uint16_t lastProcessedCommandSeq = 0;

NodeSnapshot nodes[MAX_SLAVES + 1];
uint8_t masterPollNextSlave = 1;
uint32_t lastMasterPollMs = 0;
uint32_t masterChallengeCounter = 0;

// Gate 7NB: contexto persistente del poll Master cooperativo.
MasterRuntimePollPhase masterRuntimePollPhase = MASTER_RT_POLL_IDLE;
uint8_t masterRuntimePollSlave = 0;
uint16_t masterRuntimePollChallenge = 0;
uint16_t masterRuntimePollRegs[HR_COUNT] = {};
uint32_t masterRuntimePollStartedUs = 0;
uint32_t masterRuntimeOpStartedUs = 0;
JWPLCModbusRTUError masterRuntimeLastResult = JWPLC_MODBUS_OK;

SoakState soakState = SOAK_NEED_ROLE;
IOStressState ioStress;

bool framReady = false;
bool framBackupValid = false;
uint32_t framScratchAddr = 0;
FramBackup framBackup;
uint32_t framSequence = 0;
uint32_t framOk = 0;
uint32_t framFail = 0;
uint32_t lastFramMs = 0;

bool rtcPresent = false;
bool rtcSynced = false;
bool ntpValid = false;
uint32_t rtcEpoch = 0;
uint32_t rtcFail = 0;
uint32_t lastRtcMs = 0;
uint32_t lastRtcTempMs = 0;
uint32_t lastRtcEpochObserved = 0;
uint8_t rtcSameCount = 0;
int16_t rtcTemperatureCenti = 0;
int16_t ntpDriftSeconds = 0;

bool sdReady = false;
uint32_t sdOk = 0;
uint32_t sdFail = 0;
uint32_t sdRwSequence = 0;
uint32_t lastSdLogMs = 0;
uint32_t lastSdRwMs = 0;

EthernetWindowState ethWindow;

uint16_t firstError = ERR_NONE;
uint32_t errorCount = 0;

uint32_t minFreeHeap = 0xFFFFFFFFUL;
uint32_t maxLoopUs = 0;
uint32_t loopStartUs = 0;
uint32_t lastLoopUs = 0;
uint32_t longLoop50msCount = 0;
uint32_t longLoop250msCount = 0;
uint32_t lastRuntimeDiagMs = 0;

uint32_t modbusCrcBaseline = 0;
uint32_t modbusLastCrc = 0;
uint32_t modbusLastCrcMs = 0;

ModbusTimingStats modbusTiming;

QueueHandle_t wifiTelemetryJobQueue = nullptr;
QueueHandle_t wifiTelemetryResultQueue = nullptr;
TaskHandle_t wifiTelemetryTaskHandle = nullptr;
bool wifiTelemetryOutstanding = false;
volatile int8_t wifiWorkerObservedCore = -1;

uint32_t framServiceMaxUs = 0;
uint32_t ethernetServiceMaxUs = 0;
uint32_t sdServiceMaxUs = 0;

uint32_t lastWifiHttpMs = 0;
uint32_t lastEthHttpMs = 0;
uint32_t lastCommissionPrintMs = 0;

// Forward declaration: startIoStress() aparece antes que la implementacion
// completa de los helpers de diagnostico.
static void resetRuntimeDiagnosticsForRun();

bool masterInitialRtcSyncRequested = false;
bool masterInitialRtcSyncSent = false;
uint32_t lastInitialNtpAttemptMs = 0;
uint8_t initialNtpFailCount = 0;

bool masterSoakRunning = false;
uint32_t masterSoakStartMs = 0;

// Rotacion Ethernet Master.
uint8_t rotationRounds = 1;
uint16_t rotationTotalSlots = 0;
uint16_t rotationSlot = 0;
uint32_t ethWindowDurationMs = 0;
uint32_t ethWindowStartMs = 0;
bool rotationWaitingForReady = false;

// USER UI.
uint8_t userPage = 0;
uint32_t lastUserPageChangeMs = 0;

// Serial line parser.
String serialLine;

// ============================================================================
// Helpers basicos
// ============================================================================

static uint32_t get32(const uint16_t *regs, uint16_t hiIndex)
{
    return ((uint32_t)regs[hiIndex] << 16) |
           (uint32_t)regs[hiIndex + 1];
}

static void set32(uint16_t *regs, uint16_t hiIndex, uint32_t value)
{
    regs[hiIndex] = (uint16_t)(value >> 16);
    regs[hiIndex + 1] = (uint16_t)(value & 0xFFFFU);
}

static uint16_t qPulseReg(uint8_t channel)
{
    return (uint16_t)(HR_Q_PULSE_BASE + channel * 2U);
}

static uint16_t iPulseReg(uint8_t channel)
{
    return (uint16_t)(HR_I_PULSE_BASE + channel * 2U);
}

static uint16_t localNodeOrdinal()
{
    if (cfg.role == ROLE_MASTER)
        return 0;
    return cfg.nodeId;
}

static bool isMaster()
{
    return cfg.roleValid && cfg.role == ROLE_MASTER;
}

static bool isSlave()
{
    return cfg.roleValid && cfg.role == ROLE_SLAVE;
}

static String nodeName()
{
    if (!cfg.roleValid)
        return "JWPLC_UNASSIGNED";

    if (cfg.role == ROLE_MASTER)
        return String("JWPLC_M") + String(cfg.expectedSlaves);

    return String("JWPLC_S") + String(cfg.nodeId);
}

static String shortRoleName()
{
    if (!cfg.roleValid)
        return "NONE";

    if (cfg.role == ROLE_MASTER)
        return String("M") + String(cfg.expectedSlaves);

    return String("S") + String(cfg.nodeId);
}

static const char *soakStateName(SoakState state)
{
    switch (state)
    {
    case SOAK_NEED_ROLE: return "NEED_ROLE";
    case SOAK_COMMISSIONING: return "COMMISSION";
    case SOAK_WAIT_RTC_SYNC: return "RTC_SYNC";
    case SOAK_READY_TO_START: return "READY";
    case SOAK_RUNNING: return "RUNNING";
    case SOAK_COMPLETE: return "COMPLETE";
    case SOAK_STOPPED: return "STOPPED";
    default: return "UNKNOWN";
    }
}

static const char *errorName(uint16_t code)
{
    switch (code)
    {
    case ERR_NONE: return "NONE";
    case ERR_IO: return "IO";
    case ERR_FRAM: return "FRAM";
    case ERR_RTC: return "RTC";
    case ERR_WIFI: return "WIFI";
    case ERR_ETH: return "ETH";
    case ERR_MODBUS: return "MODBUS";
    case ERR_SD: return "SD";
    case ERR_RESET: return "RESET";
    case ERR_NTP: return "NTP";
    default: return "UNKNOWN";
    }
}

static const char *errorDisplayCode(uint16_t code)
{
    switch (code)
    {
    case ERR_NONE: return "";
    case ERR_IO: return "IO";
    case ERR_FRAM: return "FRAM";
    case ERR_RTC: return "RTC";
    case ERR_WIFI: return "WIFI";
    case ERR_ETH: return "ETH";
    case ERR_MODBUS: return "MBUS";
    case ERR_SD: return "SD";
    case ERR_RESET: return "RST";
    case ERR_NTP: return "NTP";
    default: return "ERR";
    }
}

static void updateErrDisplay()
{
    JWPLC_Display.setErrCode(errorDisplayCode(firstError));
}

static void beep(uint16_t frequency = 2200, uint16_t durationMs = 140)
{
    tone(BUZZER_PIN, frequency, durationMs);
}

static void tripleBeep()
{
    tone(BUZZER_PIN, 1800, 120);
    delay(170);
    tone(BUZZER_PIN, 2300, 120);
    delay(170);
    tone(BUZZER_PIN, 2800, 180);
}

static void printRule()
{
    Serial.println(F("============================================================"));
}

// ============================================================================
// Logging de eventos / errores
// ============================================================================

static void sdLogEvent(const char *eventName, const char *detail);

static void latchError(ErrorCode code, const char *detail = nullptr)
{
    if (code == ERR_NONE)
        return;

    if (firstError == ERR_NONE)
    {
        firstError = code;
        updateErrDisplay();
    }

    errorCount++;

    Serial.print(F("[ERROR] node="));
    Serial.print(shortRoleName());
    Serial.print(F(" type="));
    Serial.print(errorName(code));

    if (detail && detail[0])
    {
        Serial.print(F(" detail="));
        Serial.print(detail);
    }

    Serial.println();

    if (isMaster() && sdReady)
    {
        sdLogEvent(errorName(code), detail ? detail : "");
    }
}

// ============================================================================
// Preferences / identidad
// ============================================================================

static void loadPersistentConfig()
{
    preferences.begin("jwsoak", false);

    String role = preferences.getString("role", "");

    cfg.roleValid = false;
    cfg.role = ROLE_NONE;
    cfg.nodeId = 0;
    cfg.expectedSlaves = 0;

    if (role.startsWith("M"))
    {
        int n = role.substring(1).toInt();
        if (n >= 1 && n <= MAX_SLAVES)
        {
            cfg.roleValid = true;
            cfg.role = ROLE_MASTER;
            cfg.expectedSlaves = (uint8_t)n;
        }
    }
    else if (role.startsWith("S"))
    {
        int id = role.substring(1).toInt();
        if (id >= 1 && id <= MAX_SLAVES)
        {
            cfg.roleValid = true;
            cfg.role = ROLE_SLAVE;
            cfg.nodeId = (uint8_t)id;
        }
    }

    cfg.wifiSsid = preferences.getString("ssid", "");
    cfg.wifiPass = preferences.getString("pass", "");
    cfg.pcIp = preferences.getString("pcip", "");
    cfg.pcPort = preferences.getUShort("pcport", DEFAULT_PC_PORT);

    cfg.wifiValid =
        cfg.wifiSsid.length() > 0 &&
        cfg.pcIp.length() > 0 &&
        cfg.pcPort > 0 &&
        pcIp.fromString(cfg.pcIp);

    cfg.bleQualified = preferences.getBool("bleok", false);

    cfg.bootCount = preferences.getULong("boots", 0) + 1UL;
    preferences.putULong("boots", cfg.bootCount);
}

static void saveRole(const String &role)
{
    preferences.putString("role", role);
}

static void clearPersistentConfig()
{
    preferences.clear();

    Serial.println();
    Serial.println(F("CONFIG=CLEARED"));
    Serial.println(F("Reiniciando..."));
    delay(500);
    ESP.restart();
}

static bool parseAndSaveRole(String value)
{
    value.trim();
    value.toUpperCase();

    if (value.length() < 2)
        return false;

    if (value.charAt(0) == 'M')
    {
        int count = value.substring(1).toInt();

        if (count < 1 || count > MAX_SLAVES)
            return false;

        saveRole(String("M") + String(count));

        // Cada nueva asignacion de rol inicia un acceptance limpio de
        // BLE + SoftAP provisioning + WiFi STA + HTTP.
        preferences.putBool("bleok", false);
        preferences.remove("ssid");
        preferences.remove("pass");
        preferences.remove("pcip");
        preferences.remove("pcport");

        Serial.print(F("ROLE_SAVED=M"));
        Serial.println(count);
        Serial.println(F("Reiniciando para aplicar nombre BLE/WiFi y Modbus..."));
        delay(700);
        ESP.restart();
        return true;
    }

    if (value.charAt(0) == 'S')
    {
        int id = value.substring(1).toInt();

        if (id < 1 || id > MAX_SLAVES)
            return false;

        saveRole(String("S") + String(id));

        // Cada nueva asignacion de rol inicia un acceptance limpio de
        // BLE + SoftAP provisioning + WiFi STA + HTTP.
        preferences.putBool("bleok", false);
        preferences.remove("ssid");
        preferences.remove("pass");
        preferences.remove("pcip");
        preferences.remove("pcport");

        Serial.print(F("ROLE_SAVED=S"));
        Serial.println(id);
        Serial.println(F("Reiniciando para aplicar nombre BLE/WiFi y Modbus..."));
        delay(700);
        ESP.restart();
        return true;
    }

    return false;
}

// ============================================================================
// BLE
// ============================================================================

class SoakBleServerCallbacks : public BLEServerCallbacks
{
    void onConnect(BLEServer *server) override
    {
        (void)server;

        bleConnected = true;
        bleConnections++;

        if (!cfg.bleQualified)
        {
            cfg.bleQualified = true;
            preferences.putBool("bleok", true);
        }

        Serial.print(F("[BLE] CONNECTED -> PASS | "));
        Serial.println(nodeName());
        Serial.println(F("[BLE] PASS persisted. Rebooting to enter WiFi phase..."));

        beep(2600, 100);

        // No reiniciar desde el callback BLE. Dejamos que loop() ejecute
        // el reboot fuera del contexto interno del stack Bluetooth.
        blePassRestartPending = true;
        blePassRestartAtMs =
            millis() + BLE_PASS_RESTART_DELAY_MS;
    }

    void onDisconnect(BLEServer *server) override
    {
        (void)server;

        bleConnected = false;

        if (cfg.bleQualified || blePassRestartPending)
        {
            Serial.println(F("[BLE] disconnected after PASS; waiting controlled reboot"));
            return;
        }

        Serial.println(F("[BLE] disconnected before PASS; advertising restart"));

        BLEAdvertising *advertising =
            BLEDevice::getAdvertising();

        if (advertising != nullptr)
            advertising->start();
    }
};

static SoakBleServerCallbacks bleServerCallbacks;

static void startBLE()
{
    if (!cfg.roleValid ||
        cfg.bleQualified ||
        bleInitialized)
    {
        return;
    }

    const String name = nodeName();

    Serial.print(F("[BLE] heap before init="));
    Serial.println(ESP.getFreeHeap());

    BLEDevice::init(name.c_str());

    bleServer = BLEDevice::createServer();

    if (bleServer == nullptr)
    {
        Serial.println(F("[BLE] createServer=FAIL"));
        return;
    }

    bleServer->setCallbacks(&bleServerCallbacks);

    BLEService *service =
        bleServer->createService(BLE_SERVICE_UUID);

    if (service == nullptr)
    {
        Serial.println(F("[BLE] createService=FAIL"));
        return;
    }

    bleStatusCharacteristic =
        service->createCharacteristic(
            BLE_STATUS_UUID,
            BLECharacteristic::PROPERTY_READ |
            BLECharacteristic::PROPERTY_WRITE);

    if (bleStatusCharacteristic == nullptr)
    {
        Serial.println(F("[BLE] createCharacteristic=FAIL"));
        return;
    }

    String value = String("READY:") + name;
    bleStatusCharacteristic->setValue(value.c_str());

    service->start();

    BLEAdvertising *advertising =
        BLEDevice::getAdvertising();

    if (advertising == nullptr)
    {
        Serial.println(F("[BLE] getAdvertising=FAIL"));
        return;
    }

    advertising->addServiceUUID(BLE_SERVICE_UUID);
    advertising->setScanResponse(true);
    advertising->start();

    bleInitialized = true;

    Serial.print(F("[BLE] advertising as "));
    Serial.println(name);

    Serial.print(F("[BLE] heap after init="));
    Serial.println(ESP.getFreeHeap());

    Serial.println(F("[BLE] Connect once to qualify. WiFi is intentionally deferred."));
}

static void serviceBleQualificationRestart()
{
    if (!blePassRestartPending)
        return;

    if ((int32_t)(millis() - blePassRestartAtMs) < 0)
        return;

    blePassRestartPending = false;

    Serial.println();
    Serial.println(F("[BLE] qualification complete -> controlled reboot"));
    Serial.flush();

    delay(50);
    ESP.restart();
}

// ============================================================================
// WiFi provisioning + STA
// ============================================================================

static String htmlEscape(const String &s)
{
    String out;
    out.reserve(s.length() + 8);

    for (size_t i = 0; i < s.length(); ++i)
    {
        const char c = s.charAt(i);

        if (c == '&') out += F("&amp;");
        else if (c == '<') out += F("&lt;");
        else if (c == '>') out += F("&gt;");
        else if (c == '"') out += F("&quot;");
        else out += c;
    }

    return out;
}

static void handleProvisionRoot()
{
    String page;
    page.reserve(1800);

    page += F("<!doctype html><html><head><meta name='viewport' content='width=device-width,initial-scale=1'>");
    page += F("<title>JWPLC Soak Setup</title></head><body>");
    page += F("<h2>");
    page += nodeName();
    page += F(" - WiFi Setup</h2>");
    page += F("<form method='POST' action='/save'>");

    page += F("<label>SSID</label><br><input name='ssid' value='");
    page += htmlEscape(cfg.wifiSsid);
    page += F("' required><br><br>");

    page += F("<label>Password</label><br><input name='pass' type='password' value='");
    page += htmlEscape(cfg.wifiPass);
    page += F("'><br><br>");

    page += F("<label>PC IPv4</label><br><input name='pcip' placeholder='192.168.1.50' value='");
    page += htmlEscape(cfg.pcIp);
    page += F("' required><br><br>");

    page += F("<label>PC HTTP port</label><br><input name='port' type='number' value='");
    page += String(cfg.pcPort);
    page += F("' required><br><br>");

    page += F("<button type='submit'>Guardar y probar</button></form>");
    page += F("<p>El JWPLC mantendra el AP hasta conseguir WiFi STA + HTTP PASS.</p>");
    page += F("</body></html>");

    provisionServer.send(200, "text/html", page);
}

static void handleProvisionSave()
{
    const String ssid = provisionServer.arg("ssid");
    const String pass = provisionServer.arg("pass");
    const String pc = provisionServer.arg("pcip");
    const uint16_t port = (uint16_t)provisionServer.arg("port").toInt();

    IPAddress parsed;

    if (ssid.length() == 0 || !parsed.fromString(pc) || port == 0)
    {
        provisionServer.send(
            400,
            "text/plain",
            "Datos invalidos. Revisa SSID, IPv4 y puerto.");
        return;
    }

    cfg.wifiSsid = ssid;
    cfg.wifiPass = pass;
    cfg.pcIp = pc;
    cfg.pcPort = port;
    cfg.wifiValid = true;
    pcIp = parsed;

    preferences.putString("ssid", cfg.wifiSsid);
    preferences.putString("pass", cfg.wifiPass);
    preferences.putString("pcip", cfg.pcIp);
    preferences.putUShort("pcport", cfg.pcPort);

    provisionServer.send(
        200,
        "text/html",
        "<html><body><h2>Guardado</h2>"
        "<p>Credenciales almacenadas.</p>"
        "<p>El JWPLC reiniciara y probara WiFi STA + HTTP en modo limpio.</p>"
        "<p>Puedes cerrar esta pagina.</p></body></html>");

    Serial.println(F("[WIFI] credentials saved"));
    Serial.println(F("[WIFI] controlled reboot scheduled for clean STA start"));

    pendingWifiReboot = true;
    pendingWifiRebootAtMs =
        millis() + WIFI_SAVE_REBOOT_DELAY_MS;
}

static void ensureProvisionServerRoutes()
{
    if (provisioningServerStarted)
        return;

    provisionServer.on("/", HTTP_GET, handleProvisionRoot);
    provisionServer.on("/save", HTTP_POST, handleProvisionSave);

    provisionServer.onNotFound([]()
    {
        provisionServer.sendHeader("Location", "/", true);
        provisionServer.send(302, "text/plain", "");
    });

    provisioningServerStarted = true;
}

static void startProvisioningAP()
{
    if (!cfg.roleValid)
        return;

    // Alpha7: BLE y WiFi se califican secuencialmente.
    // Nunca inicializar WiFi mientras la fase BLE de este boot esta activa.
    if (bleInitialized && !cfg.bleQualified)
    {
        Serial.println(F("[WIFI] deferred until BLE PASS + controlled reboot"));
        return;
    }

    ensureProvisionServerRoutes();

    WiFi.mode(WIFI_AP_STA);

    String apName = nodeName() + "_SETUP";

    if (!provisioningApActive)
    {
        Serial.print(F("[WIFI] heap before softAP="));
        Serial.println(ESP.getFreeHeap());

        const bool apOk = WiFi.softAP(apName.c_str());

        if (!apOk)
        {
            provisioningApActive = false;

            Serial.println(F("[WIFI] softAP=FAIL"));
            Serial.print(F("[WIFI] heap after failure="));
            Serial.println(ESP.getFreeHeap());

            return;
        }

        provisionServer.begin();
        provisioningApActive = true;

        Serial.println();
        Serial.print(F("[WIFI] AP provisioning: "));
        Serial.println(apName);
        Serial.print(F("[WIFI] Open: http://"));
        Serial.println(WiFi.softAPIP());

        Serial.print(F("[WIFI] heap after softAP="));
        Serial.println(ESP.getFreeHeap());
    }
}

static void startWifiSTA()
{
    // Alpha7: evita coexistencia Bluedroid + WiFi durante commissioning.
    if (bleInitialized && !cfg.bleQualified)
    {
        Serial.println(F("[WIFI] STA deferred until BLE PASS + controlled reboot"));
        return;
    }

    if (!cfg.wifiValid)
    {
        startProvisioningAP();
        return;
    }

    // Una sola orden de conexion por intento.
    if (wifiStaAttemptIssued)
        return;

    pcIp.fromString(cfg.pcIp);

    // El camino normal tras provisioning llega aqui despues de reboot,
    // por lo que NO debe existir SoftAP activo.
    if (provisioningApActive)
    {
        Serial.println(F("[WIFI] STA start refused while provisioning AP is active"));
        return;
    }

    Serial.print(F("[WIFI] heap before clean STA="));
    Serial.println(ESP.getFreeHeap());

    // Levantar exclusivamente STA.
    WiFi.mode(WIFI_STA);

    // Evitamos que el autoreconnect interno compita con nuestra maquina
    // de estados. El sketch decide cuando existe un nuevo intento.
    WiFi.setAutoReconnect(false);

    // El driver WiFi conserva credenciales propias en NVS aparte de
    // Preferences. Nosotros somos la fuente de verdad, asi que eliminamos
    // cualquier perfil anterior y detenemos una conexion residual antes
    // de aplicar las credenciales del soak.
    WiFi.disconnect(false, true);
    delay(120);

    String hostname = nodeName();
    WiFi.setHostname(hostname.c_str());

    wifiStaAttemptIssued = true;
    wifiStaStartMs = millis();

    Serial.print(F("[WIFI] connecting SSID="));
    Serial.println(cfg.wifiSsid);
    Serial.print(F("[WIFI] PC endpoint="));
    Serial.print(cfg.pcIp);
    Serial.print(':');
    Serial.println(cfg.pcPort);

    const wl_status_t beginStatus =
        WiFi.begin(
            cfg.wifiSsid.c_str(),
            cfg.wifiPass.c_str());

    Serial.print(F("[WIFI] begin status="));
    Serial.println((int)beginStatus);
}

static void serviceWifiSaveReboot()
{
    if (!pendingWifiReboot)
        return;

    if ((int32_t)(millis() - pendingWifiRebootAtMs) < 0)
        return;

    pendingWifiReboot = false;

    Serial.println();
    Serial.println(F("[WIFI] credentials persisted -> controlled reboot"));
    Serial.flush();

    // Detener el servidor/AP de forma ordenada antes del reset.
    if (provisioningApActive)
    {
        provisionServer.stop();
        WiFi.softAPdisconnect(true);
        provisioningApActive = false;
    }

    delay(80);
    ESP.restart();
}

// ============================================================================
// HTTP telemetry
// ============================================================================

static String telemetryBody(const char *transport, uint32_t forcedSequence = 0)
{
    String body;
    body.reserve(420);

    body += F("{\"node\":\"");
    body += shortRoleName();
    body += F("\",\"seq\":");

    const uint32_t seq =
        forcedSequence != 0
            ? forcedSequence
            : ((strcmp(transport, "wifi") == 0)
                   ? wifiHttp.sequence
                   : ethHttp.sequence);

    body += String(seq);

    body += F(",\"transport\":\"");
    body += transport;
    body += F("\",\"uptime\":");
    body += String(millis() / 1000UL);

    body += F(",\"boot\":");
    body += String(cfg.bootCount);

    body += F(",\"state\":\"");
    body += soakStateName(soakState);
    body += F("\",\"q\":");
    body += String(ioStress.outputBitmap);

    body += F(",\"i\":");
    body += String(ioStress.inputBitmap);

    body += F(",\"io_err\":");
    body += String(ioStress.mismatches);

    body += F(",\"fram_ok\":");
    body += String(framOk);

    body += F(",\"fram_fail\":");
    body += String(framFail);

    body += F(",\"rtc\":");
    body += String(rtcEpoch);

    body += F(",\"err\":");
    body += String(firstError);

    body += F(",\"heap\":");
    body += String(ESP.getFreeHeap());

    body += F(",\"max_loop_us\":");
    body += String(maxLoopUs);

    const JWPLCModbusRTUStats &mbStats =
        JWPLC_ModbusRTU.stats();

    body += F(",\"crc\":");
    body += String(mbStats.crcErrors);

    body += F(",\"crc_delta\":");
    body += String(
        mbStats.crcErrors >= modbusCrcBaseline
            ? mbStats.crcErrors - modbusCrcBaseline
            : mbStats.crcErrors);

    body += F("}");

    return body;
}

static bool validateHttpAck(Stream &client, uint32_t expectedSeq, bool serviceModbusWhileWaiting = true)
{
    const uint32_t start = millis();
    String response;
    response.reserve(220);

    while ((uint32_t)(millis() - start) < 450UL)
    {
        while (client.available())
        {
            char c = (char)client.read();

            if (response.length() < 220)
                response += c;
        }

        if (response.indexOf("ACK:") >= 0)
            break;

        // Solo el loop critico (Core 1) atiende Modbus.
        // El worker WiFi de Core 0 nunca entra a JWPLC_ModbusRTU.
        if (serviceModbusWhileWaiting && modbusReady)
            JWPLC_ModbusRTU.task();

        delay(1);
    }

    const String token = String("ACK:") + String(expectedSeq);

    return response.indexOf("200") >= 0 &&
           response.indexOf(token) >= 0;
}

static void recordWifiHttpResult(bool ok);

static bool performWifiTelemetryJob(
    const WifiTelemetryJob &job,
    uint32_t &elapsedMs)
{
    elapsedMs = 0;

    if (WiFi.status() != WL_CONNECTED ||
        !cfg.wifiValid)
    {
        return false;
    }

    WiFiClient client;
    client.setTimeout(250);

    const uint32_t started = millis();

    if (!client.connect(pcIp, cfg.pcPort, 250))
    {
        elapsedMs = millis() - started;
        client.stop();
        return false;
    }

    client.print(F("POST /jwplc HTTP/1.1\r\nHost: "));
    client.print(cfg.pcIp);
    client.print(F("\r\nContent-Type: application/json\r\nContent-Length: "));
    client.print(strlen(job.body));
    client.print(F("\r\nConnection: close\r\n\r\n"));
    client.print(job.body);

    const bool ok =
        validateHttpAck(
            client,
            job.sequence,
            false);

    elapsedMs = millis() - started;

    client.stop();
    return ok;
}

static void wifiTelemetryTask(void *parameter)
{
    (void)parameter;

    // Guardar el core observado, pero no imprimir desde este task.
    // Serial es compartido con loop/setup y las impresiones simultaneas
    // de Core 0/Core 1 pueden intercalarse visualmente.
    wifiWorkerObservedCore =
        (int8_t)xPortGetCoreID();

    WifiTelemetryJob job;

    for (;;)
    {
        if (xQueueReceive(
                wifiTelemetryJobQueue,
                &job,
                portMAX_DELAY) != pdTRUE)
        {
            continue;
        }

        WifiTelemetryResult result;
        result.sequence = job.sequence;
        result.ok =
            performWifiTelemetryJob(
                job,
                result.latencyMs);

        xQueueOverwrite(
            wifiTelemetryResultQueue,
            &result);
    }
}

static bool startWifiTelemetryWorker()
{
    if (wifiTelemetryTaskHandle != nullptr)
        return true;

    wifiTelemetryJobQueue =
        xQueueCreate(
            1,
            sizeof(WifiTelemetryJob));

    wifiTelemetryResultQueue =
        xQueueCreate(
            1,
            sizeof(WifiTelemetryResult));

    if (wifiTelemetryJobQueue == nullptr ||
        wifiTelemetryResultQueue == nullptr)
    {
        Serial.println(F("[WIFI WORKER] queue create FAIL"));
        return false;
    }

    const BaseType_t created =
        xTaskCreatePinnedToCore(
            wifiTelemetryTask,
            "jw_wifi_http",
            WIFI_WORKER_STACK_BYTES,
            nullptr,
            WIFI_WORKER_PRIORITY,
            &wifiTelemetryTaskHandle,
            WIFI_WORKER_CORE);

    if (created != pdPASS)
    {
        wifiTelemetryTaskHandle = nullptr;
        Serial.println(F("[WIFI WORKER] task create FAIL"));
        return false;
    }

    Serial.print(F("[WIFI WORKER] created targetCore="));
    Serial.println((int)WIFI_WORKER_CORE);

    return true;
}

static bool queueWifiTelemetry()
{
    if (wifiTelemetryJobQueue == nullptr ||
        wifiTelemetryOutstanding ||
        WiFi.status() != WL_CONNECTED ||
        !cfg.wifiValid)
    {
        return false;
    }

    WifiTelemetryJob job;
    job.sequence = ++wifiHttp.sequence;

    const String body =
        telemetryBody(
            "wifi",
            job.sequence);

    if (body.length() >= sizeof(job.body))
    {
        Serial.println(F("[WIFI WORKER] telemetry body overflow"));
        return false;
    }

    body.toCharArray(
        job.body,
        sizeof(job.body));

    if (xQueueSend(
            wifiTelemetryJobQueue,
            &job,
            0) != pdTRUE)
    {
        return false;
    }

    wifiTelemetryOutstanding = true;
    return true;
}

static void serviceWifiTelemetryResult()
{
    if (wifiTelemetryResultQueue == nullptr)
        return;

    WifiTelemetryResult result;

    if (xQueueReceive(
            wifiTelemetryResultQueue,
            &result,
            0) != pdTRUE)
    {
        return;
    }

    wifiTelemetryOutstanding = false;

    wifiHttp.lastLatencyMs =
        result.latencyMs;

    if (result.latencyMs >
        wifiHttp.maxLatencyMs)
    {
        wifiHttp.maxLatencyMs =
            result.latencyMs;
    }

    recordWifiHttpResult(result.ok);
}

static bool sendEthernetTelemetry()
{
    if (!cfg.wifiValid || !JWPLC_Ethernet.isReady())
        return false;

    ethHttp.sequence++;

    const uint32_t seq = ethHttp.sequence;
    const String body = telemetryBody("eth");

    EthernetClient client;
    client.setTimeout(450);

    const uint32_t started = millis();

    if (!client.connect(pcIp, cfg.pcPort))
    {
        client.stop();
        return false;
    }

    client.print(F("POST /jwplc HTTP/1.1\r\nHost: "));
    client.print(cfg.pcIp);
    client.print(F("\r\nContent-Type: application/json\r\nContent-Length: "));
    client.print(body.length());
    client.print(F("\r\nConnection: close\r\n\r\n"));
    client.print(body);

    const bool ok = validateHttpAck(client, seq);

    const uint32_t elapsed = millis() - started;
    ethHttp.lastLatencyMs = elapsed;
    if (elapsed > ethHttp.maxLatencyMs)
        ethHttp.maxLatencyMs = elapsed;

    client.stop();

    return ok;
}

static void recordWifiHttpResult(bool ok)
{
    if (ok)
    {
        wifiHttp.ok++;
        wifiHttp.failStreak = 0;
        wifiHttp.lastSuccessMs = millis();

        if (!wifiPass)
        {
            wifiPass = true;
            Serial.println(F("[WIFI] STA + HTTP = PASS"));
            beep(2500, 100);
        }
    }
    else
    {
        wifiHttp.fail++;
        wifiHttp.failStreak++;

        if (wifiHttp.failStreak > wifiHttp.maxFailStreak)
            wifiHttp.maxFailStreak = wifiHttp.failStreak;

        if (soakState == SOAK_RUNNING &&
            wifiHttp.failStreak == WIFI_FAIL_STREAK_TO_LATCH)
        {
            latchError(ERR_WIFI, "HTTP WiFi fail streak");
        }
    }
}

static void recordEthHttpResult(bool ok)
{
    if (ok)
    {
        ethHttp.ok++;
        ethHttp.failStreak = 0;
        ethHttp.lastSuccessMs = millis();
        ethWindow.activity = true;
    }
    else
    {
        ethHttp.fail++;
        ethHttp.failStreak++;

        if (ethHttp.failStreak > ethHttp.maxFailStreak)
            ethHttp.maxFailStreak = ethHttp.failStreak;
    }
}

// ============================================================================
// WiFi runtime
// ============================================================================

static void serviceWiFi()
{
    const uint32_t now = millis();

    // El resultado de la tarea de Core 0 se aplica aqui, en Core 1.
    serviceWifiTelemetryResult();

    if (!cfg.roleValid)
        return;

    if (provisioningApActive)
        provisionServer.handleClient();

    serviceWifiSaveReboot();

    // Si el reboot ya quedo programado, no iniciar ninguna otra transicion.
    if (pendingWifiReboot)
        return;

    if (!cfg.wifiValid)
        return;

    if (WiFi.status() == WL_CONNECTED)
    {
        wifiHttpActive = true;

        if ((uint32_t)(now - lastWifiHttpMs) >= WIFI_HTTP_PERIOD_MS &&
            !wifiTelemetryOutstanding)
        {
            lastWifiHttpMs = now;

            if (!queueWifiTelemetry() &&
                wifiTelemetryTaskHandle != nullptr)
            {
                recordWifiHttpResult(false);
            }
        }

        return;
    }

    wifiHttpActive = false;

    // En el boot de prueba STA se emite exactamente un WiFi.begin().
    if (!wifiStaAttemptIssued && !provisioningApActive)
    {
        startWifiSTA();
        return;
    }

    // No llamar WiFi.begin()/reconnect() repetidamente mientras el driver
    // esta intentando asociarse. Si el intento no prospera, cancelamos STA
    // antes de volver a abrir provisioning.
    if (wifiStaAttemptIssued &&
        !provisioningApActive &&
        !wifiPass &&
        (uint32_t)(now - wifiStaStartMs) >= WIFI_PROVISION_REOPEN_MS)
    {
        Serial.println(F("[WIFI] STA timeout -> stop attempt and reopen provisioning AP"));

        WiFi.disconnect(false, false);
        delay(100);

        wifiStaAttemptIssued = false;
        wifiStaStartMs = 0;

        startProvisioningAP();
    }
}

// ============================================================================
// NTP
// ============================================================================

static void buildNtpPacket(uint8_t *packet)
{
    memset(packet, 0, 48);
    packet[0] = 0x1B; // LI=0, VN=3, Mode=3 client
}

static bool decodeNtpPacket(const uint8_t *packet, uint32_t &utcUnix)
{
    const uint32_t seconds1900 =
        ((uint32_t)packet[40] << 24) |
        ((uint32_t)packet[41] << 16) |
        ((uint32_t)packet[42] << 8) |
        ((uint32_t)packet[43]);

    if (seconds1900 < NTP_UNIX_DELTA)
        return false;

    utcUnix = seconds1900 - NTP_UNIX_DELTA;
    return true;
}

static bool queryNtpEthernet(uint32_t &localUnix)
{
    if (!JWPLC_Ethernet.isReady())
        return false;

    EthernetUDP udp;

    const uint16_t localPort =
        (uint16_t)(NTP_LOCAL_PORT_BASE + localNodeOrdinal());

    if (!udp.begin(localPort))
        return false;

    uint8_t packet[48];
    buildNtpPacket(packet);

    if (!udp.beginPacket(NTP_HOST, NTP_PORT))
    {
        udp.stop();
        return false;
    }

    udp.write(packet, sizeof(packet));

    if (!udp.endPacket())
    {
        udp.stop();
        return false;
    }

    const uint32_t started = millis();

    while ((uint32_t)(millis() - started) < NTP_TIMEOUT_MS)
    {
        const int packetSize = udp.parsePacket();

        if (packetSize >= 48)
        {
            udp.read(packet, sizeof(packet));

            uint32_t utc = 0;

            if (decodeNtpPacket(packet, utc))
            {
                localUnix =
                    (uint32_t)((int64_t)utc + LIMA_UTC_OFFSET_SECONDS);

                udp.stop();
                return true;
            }
        }

        if (modbusReady)
            JWPLC_ModbusRTU.task();

        delay(2);
    }

    udp.stop();
    return false;
}

static bool queryNtpWiFi(uint32_t &localUnix)
{
    if (WiFi.status() != WL_CONNECTED)
        return false;

    WiFiUDP udp;

    const uint16_t localPort =
        (uint16_t)(NTP_LOCAL_PORT_BASE + 20 + localNodeOrdinal());

    if (!udp.begin(localPort))
        return false;

    uint8_t packet[48];
    buildNtpPacket(packet);

    if (!udp.beginPacket(NTP_HOST, NTP_PORT))
    {
        udp.stop();
        return false;
    }

    udp.write(packet, sizeof(packet));

    if (!udp.endPacket())
    {
        udp.stop();
        return false;
    }

    const uint32_t started = millis();

    while ((uint32_t)(millis() - started) < NTP_TIMEOUT_MS)
    {
        const int packetSize = udp.parsePacket();

        if (packetSize >= 48)
        {
            udp.read(packet, sizeof(packet));

            uint32_t utc = 0;

            if (decodeNtpPacket(packet, utc))
            {
                localUnix =
                    (uint32_t)((int64_t)utc + LIMA_UTC_OFFSET_SECONDS);

                udp.stop();
                return true;
            }
        }

        if (modbusReady)
            JWPLC_ModbusRTU.task();

        delay(2);
    }

    udp.stop();
    return false;
}

static bool obtainNtpLocalTime(uint32_t &localUnix, bool ethernetPreferred)
{
    if (ethernetPreferred && queryNtpEthernet(localUnix))
        return true;

    if (queryNtpWiFi(localUnix))
        return true;

    if (!ethernetPreferred && queryNtpEthernet(localUnix))
        return true;

    return false;
}

// ============================================================================
// RTC
// ============================================================================

static void syncLocalRtc(uint32_t localUnix)
{
    if (JWPLC_RTC.writeUnix(localUnix))
    {
        (void)JWPLC_RTC.clearOscillatorStopFlag();

        rtcSynced = true;
        ntpValid = true;
        rtcEpoch = localUnix;
        lastRtcEpochObserved = localUnix;
        rtcSameCount = 0;

        Serial.print(F("[RTC] synced local epoch="));
        Serial.println(localUnix);
    }
    else
    {
        latchError(ERR_RTC, "writeUnix failed");
    }
}

static void serviceRTC()
{
    const uint32_t now = millis();

    if ((uint32_t)(now - lastRtcMs) >= RTC_PERIOD_MS)
    {
        lastRtcMs = now;

        uint32_t value = 0;

        if (JWPLC_RTC.readUnix(value))
        {
            rtcPresent = true;
            rtcEpoch = value;

            if (lastRtcEpochObserved != 0)
            {
                if (value < lastRtcEpochObserved)
                {
                    rtcFail++;
                    latchError(ERR_RTC, "RTC moved backwards");
                }
                else if (value == lastRtcEpochObserved)
                {
                    rtcSameCount++;

                    if (rtcSameCount >= 4)
                    {
                        rtcFail++;
                        rtcSameCount = 0;
                        latchError(ERR_RTC, "RTC freeze");
                    }
                }
                else
                {
                    rtcSameCount = 0;
                }
            }

            lastRtcEpochObserved = value;
        }
        else
        {
            // Antes de la sincronizacion inicial una hora invalida/no inicializada
            // es una condicion de commissioning, no una falla del soak.
            rtcPresent = JWPLC_RTC.isPresent();

            if (rtcSynced || soakState == SOAK_RUNNING)
            {
                rtcFail++;
                latchError(ERR_RTC, "readUnix failed");
            }
        }
    }

    if ((uint32_t)(now - lastRtcTempMs) >= RTC_TEMP_PERIOD_MS)
    {
        lastRtcTempMs = now;

        int16_t centi = 0;

        if (JWPLC_RTC.readTemperatureCentiC(centi))
            rtcTemperatureCenti = centi;
        else
            rtcFail++;
    }
}

// ============================================================================
// FRAM
// ============================================================================

static bool prepareFRAM()
{
    const uint32_t size = JWPLC_FRAM.size();

    if (size < FRAM_SCRATCH_BYTES + 64U)
    {
        Serial.println(F("[FRAM] invalid/too small"));
        framReady = false;
        return false;
    }

    framScratchAddr = size - FRAM_SCRATCH_BYTES;

    if (!JWPLC_FRAM.get(framScratchAddr, framBackup))
    {
        Serial.println(F("[FRAM] backup read FAIL"));
        framReady = false;
        return false;
    }

    framBackupValid = true;
    framReady = true;

    Serial.print(F("[FRAM] size="));
    Serial.print(size);
    Serial.print(F(" scratch=0x"));
    Serial.println(framScratchAddr, HEX);

    return true;
}

static void restoreFRAM()
{
    if (!framReady || !framBackupValid)
        return;

    if (JWPLC_FRAM.put(framScratchAddr, framBackup))
    {
        Serial.println(F("[FRAM] scratch restored"));
    }
    else
    {
        framFail++;
        latchError(ERR_FRAM, "scratch restore failed");
    }
}

static void runFRAMProbe()
{
    if (!framReady)
        return;

    FramProbeRecord written = {};
    written.magic = 0x534F414BUL; // "SOAK"
    written.sequence = ++framSequence;
    written.rtcEpoch = rtcEpoch;
    written.nodeId = (uint8_t)localNodeOrdinal();
    written.qBitmap = ioStress.outputBitmap;
    written.iBitmap = ioStress.inputBitmap;

    uint32_t pulseTotal = 0;

    for (uint8_t i = 0; i < 8; ++i)
        pulseTotal += ioStress.qPulses[i];

    written.pulseTotal = pulseTotal;

    FramProbeRecord readBack = {};

    const bool writeOk =
        JWPLC_FRAM.writeBlock(
            framScratchAddr,
            written,
            FRAM_RECORD_VERSION);

    const bool readOk =
        writeOk &&
        JWPLC_FRAM.readBlock(
            framScratchAddr,
            readBack,
            FRAM_RECORD_VERSION);

    const bool compareOk =
        readOk &&
        memcmp(&written, &readBack, sizeof(written)) == 0;

    if (compareOk)
    {
        framOk++;
    }
    else
    {
        framFail++;
        latchError(ERR_FRAM, "write/read/compare failed");
    }
}

static void serviceFRAM()
{
    if (soakState != SOAK_RUNNING || !framReady)
        return;

    const uint32_t now = millis();

    if ((uint32_t)(now - lastFramMs) >= FRAM_PERIOD_MS)
    {
        lastFramMs = now;
        runFRAMProbe();
    }
}

// ============================================================================
// I/O loopback stress
// ============================================================================

static void setOutputs(uint8_t bitmap)
{
    digitalWriteBlock(Q0_X, bitmap);
    ioStress.outputBitmap = bitmap;
}

static void recordIoMismatch(const char *reason)
{
    if (ioStress.phaseFailureLatched)
        return;

    ioStress.phaseFailureLatched = true;
    ioStress.mismatches++;

    Serial.print(F("[IO FAIL] node="));
    Serial.print(shortRoleName());
    Serial.print(F(" ch="));
    Serial.print(ioStress.channel);
    Serial.print(F(" Q=0x"));
    Serial.print(ioStress.outputBitmap, HEX);
    Serial.print(F(" I=0x"));
    Serial.print(ioStress.inputBitmap, HEX);
    Serial.print(F(" reason="));
    Serial.println(reason);

    latchError(ERR_IO, reason);
}

static void startOnPhase(uint32_t now)
{
    ioStress.onPhase = true;
    ioStress.phaseStartMs = now;
    ioStress.phaseFailureLatched = false;
    ioStress.pulseDetectedThisOn = false;

    const uint8_t expected =
        (uint8_t)(1U << ioStress.channel);

    setOutputs(expected);
    ioStress.qPulses[ioStress.channel]++;
}

static void startOffPhase(uint32_t now)
{
    ioStress.onPhase = false;
    ioStress.phaseStartMs = now;
    ioStress.phaseFailureLatched = false;
    ioStress.pulseDetectedThisOn = false;

    setOutputs(0x00);
}

static void startIoStress()
{
    setOutputs(0x00);

    resetRuntimeDiagnosticsForRun();

    ioStress.running = true;
    ioStress.waitingInitialStagger = true;
    ioStress.onPhase = false;
    ioStress.channel = 0;
    ioStress.phaseFailureLatched = false;
    ioStress.pulseDetectedThisOn = false;

    ioStress.startAfterMs =
        millis() +
        (uint32_t)localNodeOrdinal() * IO_NODE_STAGGER_MS;

    ioStress.phaseStartMs = millis();
    ioStress.lastScanMs = 0;

    Serial.print(F("[IO] stress start; stagger="));
    Serial.print((uint32_t)localNodeOrdinal() * IO_NODE_STAGGER_MS);
    Serial.println(F("ms"));
}

static void stopIoStress()
{
    ioStress.running = false;
    ioStress.waitingInitialStagger = false;
    setOutputs(0x00);
}

static void serviceIO()
{
    const uint32_t now = millis();

    if ((uint32_t)(now - ioStress.lastScanMs) >= IO_SCAN_MS)
    {
        ioStress.lastScanMs = now;
        ioStress.inputBitmap = digitalReadBlock(I0_X);
    }

    if (!ioStress.running)
        return;

    if (ioStress.waitingInitialStagger)
    {
        if ((int32_t)(now - ioStress.startAfterMs) >= 0)
        {
            ioStress.waitingInitialStagger = false;
            startOnPhase(now);
        }

        return;
    }

    const uint32_t age =
        (uint32_t)(now - ioStress.phaseStartMs);

    if (ioStress.onPhase)
    {
        const uint8_t expected =
            (uint8_t)(1U << ioStress.channel);

        if (age >= IO_SETTLE_MS &&
            ioStress.inputBitmap == expected &&
            !ioStress.pulseDetectedThisOn)
        {
            ioStress.pulseDetectedThisOn = true;
            ioStress.iPulses[ioStress.channel]++;
        }

        if (age >= IO_VERIFY_DEADLINE_MS &&
            !ioStress.pulseDetectedThisOn)
        {
            recordIoMismatch("expected input pulse missing");
        }

        if (age >= IO_VERIFY_DEADLINE_MS &&
            ioStress.inputBitmap != expected)
        {
            recordIoMismatch("input bitmap != output bitmap");
        }

        if (age >= IO_PHASE_MS)
        {
            startOffPhase(now);
        }
    }
    else
    {
        if (age >= IO_VERIFY_DEADLINE_MS &&
            ioStress.inputBitmap != 0)
        {
            recordIoMismatch("input remained ON after output OFF");
        }

        if (age >= IO_PHASE_MS)
        {
            ioStress.channel++;

            if (ioStress.channel >= 8)
                ioStress.channel = 0;

            startOnPhase(now);
        }
    }
}

// ============================================================================
// Holding register refresh
// ============================================================================

static uint16_t buildStatusFlags()
{
    uint16_t flags = 0;

    if (cfg.roleValid) flags |= ST_ROLE;
    if (cfg.bleQualified) flags |= ST_BLE_PASS;
    if (wifiPass) flags |= ST_WIFI_PASS;
    if (modbusReady) flags |= ST_MODBUS_READY;
    if (rtcSynced) flags |= ST_RTC_SYNCED;
    if (framReady) flags |= ST_FRAM_READY;
    if (soakState == SOAK_RUNNING) flags |= ST_SOAK_RUNNING;
    if (ioStress.mismatches == 0) flags |= ST_IO_OK;
    if (JWPLC_Ethernet.isReady()) flags |= ST_ETH_READY;
    if (ethWindow.activity) flags |= ST_ETH_WINDOW_ACTIVITY;
    if (sdReady) flags |= ST_SD_READY;
    if (firstError != ERR_NONE) flags |= ST_ERROR_LATCH;
    if (ntpValid) flags |= ST_NTP_VALID;
    if (wifiHttpActive) flags |= ST_WIFI_HTTP_ACTIVE;

    if (ethWindow.owner == localNodeOrdinal() &&
        JWPLC_Ethernet.isReady())
    {
        flags |= ST_ETH_HTTP_ACTIVE;
    }

    if (isMaster() && soakState >= SOAK_READY_TO_START)
        flags |= ST_ALL_COMMISSIONED;

    return flags;
}

static void refreshHoldingRegisters()
{
    holdingRegisters[HR_SIGNATURE] = MODBUS_SIGNATURE;
    holdingRegisters[HR_PROTOCOL_VERSION] = MODBUS_PROTOCOL_VERSION;
    holdingRegisters[HR_NODE_ID] = localNodeOrdinal();
    holdingRegisters[HR_STATUS_FLAGS] = buildStatusFlags();
    holdingRegisters[HR_SOAK_STATE] = soakState;

    set32(
        holdingRegisters,
        HR_UPTIME_HI,
        millis() / 1000UL);

    set32(
        holdingRegisters,
        HR_BOOT_COUNT_HI,
        cfg.bootCount);

    holdingRegisters[HR_Q_BITMAP] = ioStress.outputBitmap;
    holdingRegisters[HR_I_BITMAP] = ioStress.inputBitmap;
    holdingRegisters[HR_IO_CHANNEL] = ioStress.channel;

    set32(
        holdingRegisters,
        HR_IO_MISMATCH_HI,
        ioStress.mismatches);

    for (uint8_t i = 0; i < 8; ++i)
    {
        set32(
            holdingRegisters,
            qPulseReg(i),
            ioStress.qPulses[i]);

        set32(
            holdingRegisters,
            iPulseReg(i),
            ioStress.iPulses[i]);
    }

    set32(holdingRegisters, HR_FRAM_OK_HI, framOk);
    set32(holdingRegisters, HR_FRAM_FAIL_HI, framFail);

    set32(holdingRegisters, HR_RTC_EPOCH_HI, rtcEpoch);
    set32(holdingRegisters, HR_RTC_FAIL_HI, rtcFail);

    set32(holdingRegisters, HR_WIFI_HTTP_OK_HI, wifiHttp.ok);
    set32(holdingRegisters, HR_WIFI_HTTP_FAIL_HI, wifiHttp.fail);

    set32(holdingRegisters, HR_ETH_HTTP_OK_HI, ethHttp.ok);
    set32(holdingRegisters, HR_ETH_HTTP_FAIL_HI, ethHttp.fail);

    holdingRegisters[HR_BLE_CONNECTIONS] = bleConnections;
    holdingRegisters[HR_WIFI_RSSI] =
        (uint16_t)(int16_t)(
            WiFi.status() == WL_CONNECTED
                ? WiFi.RSSI()
                : -127);

    holdingRegisters[HR_NTP_DRIFT_SECONDS] =
        (uint16_t)ntpDriftSeconds;

    holdingRegisters[HR_FIRST_ERROR] = firstError;
    holdingRegisters[HR_ERROR_COUNT] =
        (uint16_t)min(errorCount, 0xFFFFUL);

    set32(
        holdingRegisters,
        HR_MIN_FREE_HEAP_HI,
        minFreeHeap == 0xFFFFFFFFUL
            ? ESP.getFreeHeap()
            : minFreeHeap);

    set32(
        holdingRegisters,
        HR_MAX_LOOP_US_HI,
        maxLoopUs);

    holdingRegisters[HR_SD_STATUS] =
        sdReady ? 1 : 0;

    holdingRegisters[HR_ETH_WINDOWS_OK] =
        ethWindow.windowsOk;

    holdingRegisters[HR_ETH_WINDOWS_FAIL] =
        ethWindow.windowsFail;

    const JWPLCModbusRTUStats &diagStats =
        JWPLC_ModbusRTU.stats();

    set32(
        holdingRegisters,
        HR_MODBUS_CRC_HI,
        diagStats.crcErrors);

    set32(
        holdingRegisters,
        HR_MODBUS_CRC_DELTA_HI,
        diagStats.crcErrors >= modbusCrcBaseline
            ? diagStats.crcErrors - modbusCrcBaseline
            : diagStats.crcErrors);

    set32(
        holdingRegisters,
        HR_LAST_CRC_AGE_MS_HI,
        modbusLastCrcMs == 0
            ? 0
            : millis() - modbusLastCrcMs);

    set32(
        holdingRegisters,
        HR_LONG_LOOP_50MS_HI,
        longLoop50msCount);

    set32(
        holdingRegisters,
        HR_LONG_LOOP_250MS_HI,
        longLoop250msCount);

    set32(
        holdingRegisters,
        HR_WIFI_MAX_LATENCY_MS_HI,
        wifiHttp.maxLatencyMs);
}

// ============================================================================
// Modbus
// ============================================================================

static void beginModbus()
{
    if (!cfg.roleValid)
        return;

    if (isSlave())
    {
        memset(holdingRegisters, 0, sizeof(holdingRegisters));
        holdingRegisters[HR_ETH_OWNER] = ETH_OWNER_NONE;

        refreshHoldingRegisters();

        JWPLC_ModbusRTU.setHoldingRegisters(
            holdingRegisters,
            HR_COUNT);

        modbusReady = JWPLC_ModbusRTU.begin(
            cfg.nodeId,
            MODBUS_BAUD,
            MODBUS_CONFIG);

        Serial.print(F("[MODBUS] SLAVE ID="));
        Serial.print(cfg.nodeId);
        Serial.print(F(" begin="));
        Serial.println(modbusReady ? F("PASS") : F("FAIL"));
    }
    else
    {
        modbusReady = JWPLC_ModbusRTU.begin(
            MASTER_MODBUS_LOCAL_ID,
            MODBUS_BAUD,
            MODBUS_CONFIG);

        Serial.print(F("[MODBUS] MASTER expectedSlaves="));
        Serial.print(cfg.expectedSlaves);
        Serial.print(F(" begin="));
        Serial.println(modbusReady ? F("PASS") : F("FAIL"));
    }

    if (modbusReady)
    {
        JWPLC_ModbusRTU.setFrameGapMs(2);
    }

    JWPLC_ModbusRTU.resetStats();

    modbusCrcBaseline = 0;
    modbusLastCrc = 0;
    modbusLastCrcMs = 0;

    if (!modbusReady)
        latchError(ERR_MODBUS, "begin failed");
}

static void resetRuntimeDiagnosticsForRun()
{
    maxLoopUs = 0;
    lastLoopUs = 0;
    longLoop50msCount = 0;
    longLoop250msCount = 0;
    lastRuntimeDiagMs = 0;

    framServiceMaxUs = 0;
    ethernetServiceMaxUs = 0;
    sdServiceMaxUs = 0;

    modbusTiming = ModbusTimingStats{};

    masterRuntimePollPhase = MASTER_RT_POLL_IDLE;
    masterRuntimePollSlave = 0;
    masterRuntimePollChallenge = 0;
    masterRuntimePollStartedUs = 0;
    masterRuntimeOpStartedUs = 0;
    masterRuntimeLastResult = JWPLC_MODBUS_OK;
    memset(masterRuntimePollRegs, 0, sizeof(masterRuntimePollRegs));

    if (JWPLC_ModbusRTU.masterDone())
        JWPLC_ModbusRTU.clearMasterResult();

    const JWPLCModbusRTUStats &stats =
        JWPLC_ModbusRTU.stats();

    modbusCrcBaseline = stats.crcErrors;
    modbusLastCrc = stats.crcErrors;
    modbusLastCrcMs = 0;

    wifiHttp.maxLatencyMs = 0;
    ethHttp.maxLatencyMs = 0;

    Serial.print(F("[DIAG RESET] node="));
    Serial.print(shortRoleName());
    Serial.print(F(" crcBaseline="));
    Serial.println(modbusCrcBaseline);
}

static void serviceModbusDiagnostics()
{
    if (!modbusReady)
        return;

    const JWPLCModbusRTUStats &stats =
        JWPLC_ModbusRTU.stats();

    if (stats.crcErrors == modbusLastCrc)
        return;

    const uint32_t previous = modbusLastCrc;

    modbusLastCrc = stats.crcErrors;
    modbusLastCrcMs = millis();

    const uint32_t delta =
        stats.crcErrors >= previous
            ? stats.crcErrors - previous
            : stats.crcErrors;

    const uint32_t runCrc =
        stats.crcErrors >= modbusCrcBaseline
            ? stats.crcErrors - modbusCrcBaseline
            : stats.crcErrors;

    // No inundar Serial: imprimir los primeros eventos y luego checkpoints.
    // Los contadores siguen siendo exactos en RAM/TFT/telemetria.
    const bool shouldPrint =
        runCrc <= 10 ||
        delta > 1 ||
        (runCrc % 100UL) == 0;

    if (!shouldPrint)
        return;

    Serial.print(F("[MODBUS CRC] node="));
    Serial.print(shortRoleName());
    Serial.print(F(" +"));
    Serial.print(delta);
    Serial.print(F(" total="));
    Serial.print(stats.crcErrors);
    Serial.print(F(" run="));
    Serial.print(runCrc);
    Serial.print(F(" prevLoopUs="));
    Serial.print(lastLoopUs);
    Serial.print(F(" maxLoopUs="));
    Serial.print(maxLoopUs);
    Serial.print(F(" wifiMaxMs="));
    Serial.print(wifiHttp.maxLatencyMs);
    Serial.print(F(" ethMaxMs="));
    Serial.println(ethHttp.maxLatencyMs);
}

static void printRuntimeDiagnostics()
{
    const JWPLCModbusRTUStats &stats =
        JWPLC_ModbusRTU.stats();

    Serial.println();
    Serial.println(F("---- RUNTIME DIAG ----"));

    Serial.print(F("NODE="));
    Serial.print(shortRoleName());
    Serial.print(F(" LOOP_CORE="));
    Serial.print((int)xPortGetCoreID());
    Serial.print(F(" WIFI_WORKER_CORE="));
    Serial.println((int)wifiWorkerObservedCore);

    Serial.print(F("LOOP_US last/max="));
    Serial.print(lastLoopUs);
    Serial.print('/');
    Serial.println(maxLoopUs);

    Serial.print(F("LONG_LOOPS >50ms/>250ms="));
    Serial.print(longLoop50msCount);
    Serial.print('/');
    Serial.println(longLoop250msCount);

    Serial.print(F("MODBUS_CRC total/run="));
    Serial.print(stats.crcErrors);
    Serial.print('/');
    Serial.println(
        stats.crcErrors >= modbusCrcBaseline
            ? stats.crcErrors - modbusCrcBaseline
            : stats.crcErrors);

    Serial.print(F("WIFI_LAT ms last/max="));
    Serial.print(wifiHttp.lastLatencyMs);
    Serial.print('/');
    Serial.println(wifiHttp.maxLatencyMs);

    Serial.print(F("ETH_LAT ms last/max="));
    Serial.print(ethHttp.lastLatencyMs);
    Serial.print('/');
    Serial.println(ethHttp.maxLatencyMs);

    Serial.print(F("SERVICE_MAX_US FRAM/ETH/SD="));
    Serial.print(framServiceMaxUs);
    Serial.print('/');
    Serial.print(ethernetServiceMaxUs);
    Serial.print('/');
    Serial.println(sdServiceMaxUs);

    if (isMaster())
    {
        Serial.print(F("MB_POLL ok/fail="));
        Serial.print(modbusTiming.pollOk);
        Serial.print('/');
        Serial.println(modbusTiming.pollFail);

        Serial.print(F("MB_FAIL W/R/V="));
        Serial.print(modbusTiming.writeFail);
        Serial.print('/');
        Serial.print(modbusTiming.readFail);
        Serial.print('/');
        Serial.println(modbusTiming.validateFail);

        Serial.print(F("MB_PHASE="));
        Serial.print((int)masterRuntimePollPhase);
        Serial.print(F(" lastResult="));
        Serial.println((int)masterRuntimeLastResult);

        Serial.print(F("MB_WRITE_US last/max="));
        Serial.print(modbusTiming.lastWriteUs);
        Serial.print('/');
        Serial.println(modbusTiming.maxWriteUs);

        Serial.print(F("MB_READ_US last/max="));
        Serial.print(modbusTiming.lastReadUs);
        Serial.print('/');
        Serial.println(modbusTiming.maxReadUs);

        Serial.print(F("MB_POLL_US last/max="));
        Serial.print(modbusTiming.lastPollUs);
        Serial.print('/');
        Serial.println(modbusTiming.maxPollUs);

        for (uint8_t id = 1; id <= cfg.expectedSlaves; ++id)
        {
            if (!nodes[id].seen)
                continue;

            const uint32_t slaveCrc =
                get32(nodes[id].regs, HR_MODBUS_CRC_HI);
            const uint32_t slaveCrcRun =
                get32(nodes[id].regs, HR_MODBUS_CRC_DELTA_HI);
            const uint32_t slaveMaxLoopUs =
                get32(nodes[id].regs, HR_MAX_LOOP_US_HI);
            const uint32_t slaveLong50 =
                get32(nodes[id].regs, HR_LONG_LOOP_50MS_HI);
            const uint32_t slaveLong250 =
                get32(nodes[id].regs, HR_LONG_LOOP_250MS_HI);

            Serial.print(F("S"));
            Serial.print(id);
            Serial.print(F(" CRC total/run="));
            Serial.print(slaveCrc);
            Serial.print('/');
            Serial.print(slaveCrcRun);
            Serial.print(F(" maxLoopUs="));
            Serial.print(slaveMaxLoopUs);
            Serial.print(F(" long50/250="));
            Serial.print(slaveLong50);
            Serial.print('/');
            Serial.println(slaveLong250);
        }
    }

    Serial.println(F("----------------------"));
}

static void servicePeriodicRuntimeDiagnostics()
{
    if (soakState != SOAK_RUNNING)
        return;

    const uint32_t now = millis();

    if ((uint32_t)(now - lastRuntimeDiagMs) <
        RUNTIME_DIAG_PRINT_MS)
    {
        return;
    }

    lastRuntimeDiagMs = now;

    if (isMaster())
        printRuntimeDiagnostics();
}

static void processSlaveCommand()
{
    if (!isSlave() || !modbusReady)
        return;

    const uint16_t seq = holdingRegisters[HR_CMD_SEQ];

    if (seq == lastProcessedCommandSeq)
        return;

    lastProcessedCommandSeq = seq;

    const uint16_t command =
        holdingRegisters[HR_CMD_CODE];

    switch (command)
    {
    case CMD_START_SOAK:
        if (soakState != SOAK_RUNNING)
        {
            soakState = SOAK_RUNNING;
            startIoStress();
            Serial.println(F("[CMD] START_SOAK"));
        }
        break;

    case CMD_STOP_SOAK:
        stopIoStress();
        soakState = SOAK_STOPPED;
        Serial.println(F("[CMD] STOP_SOAK"));
        break;

    case CMD_SYNC_RTC:
    {
        const uint32_t epoch =
            ((uint32_t)holdingRegisters[HR_TIME_HI] << 16) |
            holdingRegisters[HR_TIME_LO];

        syncLocalRtc(epoch);

        if (rtcSynced && soakState < SOAK_RUNNING)
            soakState = SOAK_READY_TO_START;

        break;
    }

    case CMD_BEEP:
        tripleBeep();
        break;

    default:
        break;
    }
}

static bool prepareMasterForSyncCommand()
{
    if (!isMaster() || !modbusReady)
        return false;

    // Los comandos Sync son poco frecuentes. Si coinciden con un poll
    // cooperativo, dejamos terminar la transaccion pendiente y descartamos
    // ese snapshot antes de entrar a la API Sync.
    const uint32_t startedMs = millis();

    while (JWPLC_ModbusRTU.masterBusy())
    {
        JWPLC_ModbusRTU.task();

        if ((uint32_t)(millis() - startedMs) >=
            MODBUS_TIMEOUT_MS + 20UL)
        {
            return false;
        }

        delay(1);
    }

    if (JWPLC_ModbusRTU.masterDone())
        JWPLC_ModbusRTU.clearMasterResult();

    if (masterRuntimePollPhase != MASTER_RT_POLL_IDLE)
    {
        masterRuntimePollPhase = MASTER_RT_POLL_IDLE;
        masterRuntimePollSlave = 0;
        lastMasterPollMs = millis();
    }

    return true;
}

static bool masterWriteRegister(
    uint8_t slaveId,
    uint16_t reg,
    uint16_t value)
{
    if (!prepareMasterForSyncCommand())
    {
        Serial.println(F("[MODBUS FAIL] sync command could not acquire Master"));
        return false;
    }

    const bool ok =
        JWPLC_ModbusRTU.writeSingleRegisterSync(
            slaveId,
            reg,
            value,
            MODBUS_TIMEOUT_MS);

    if (!ok)
    {
        Serial.print(F("[MODBUS FAIL] write S"));
        Serial.print(slaveId);
        Serial.print(F(" reg="));
        Serial.print(reg);
        Serial.print(F(" err="));
        Serial.println(JWPLC_ModbusRTU.lastErrorString());
    }

    return ok;
}

static bool sendSlaveCommand(
    uint8_t slaveId,
    CommandCode command,
    uint16_t arg0 = 0,
    uint16_t arg1 = 0,
    uint32_t epoch = 0)
{
    if (!isMaster() ||
        slaveId == 0 ||
        slaveId > cfg.expectedSlaves)
    {
        return false;
    }

    NodeSnapshot &node = nodes[slaveId];

    bool ok = true;

    ok &= masterWriteRegister(slaveId, HR_CMD_ARG0, arg0);
    ok &= masterWriteRegister(slaveId, HR_CMD_ARG1, arg1);

    if (command == CMD_SYNC_RTC)
    {
        ok &= masterWriteRegister(
            slaveId,
            HR_TIME_HI,
            (uint16_t)(epoch >> 16));

        ok &= masterWriteRegister(
            slaveId,
            HR_TIME_LO,
            (uint16_t)(epoch & 0xFFFFU));
    }

    ok &= masterWriteRegister(
        slaveId,
        HR_CMD_CODE,
        (uint16_t)command);

    node.commandSequence++;

    ok &= masterWriteRegister(
        slaveId,
        HR_CMD_SEQ,
        node.commandSequence);

    if (!ok)
        latchError(ERR_MODBUS, "slave command write failed");

    return ok;
}

static void setEthernetOwner(uint16_t owner)
{
    ethWindow.owner = owner;
    holdingRegisters[HR_ETH_OWNER] = owner;

    if (isMaster())
    {
        for (uint8_t id = 1; id <= cfg.expectedSlaves; ++id)
        {
            (void)masterWriteRegister(
                id,
                HR_ETH_OWNER,
                owner);
        }
    }

    Serial.print(F("[ETH] owner="));

    if (owner == ETH_OWNER_NONE)
        Serial.println(F("NONE"));
    else if (owner == 0)
        Serial.println(F("MASTER"));
    else
    {
        Serial.print('S');
        Serial.println(owner);
    }
}

static void masterPollOneSlaveSyncCommissioning()
{
    if (!isMaster() ||
        !modbusReady ||
        cfg.expectedSlaves == 0)
    {
        return;
    }

    const uint32_t now = millis();

    if ((uint32_t)(now - lastMasterPollMs) <
        MASTER_POLL_GAP_MS)
    {
        return;
    }

    lastMasterPollMs = now;

    const uint8_t id = masterPollNextSlave;

    masterPollNextSlave++;

    if (masterPollNextSlave > cfg.expectedSlaves)
        masterPollNextSlave = 1;

    NodeSnapshot &node = nodes[id];

    masterChallengeCounter++;
    const uint16_t challenge =
        (uint16_t)(masterChallengeCounter & 0xFFFFU);

    const uint32_t pollStartedUs = micros();
    const uint32_t writeStartedUs = micros();

    bool writeOk =
        JWPLC_ModbusRTU.writeSingleRegisterSync(
            id,
            HR_CHALLENGE,
            challenge,
            MODBUS_TIMEOUT_MS);

    modbusTiming.lastWriteUs =
        micros() - writeStartedUs;

    if (modbusTiming.lastWriteUs > modbusTiming.maxWriteUs)
        modbusTiming.maxWriteUs = modbusTiming.lastWriteUs;

    uint16_t regs[HR_COUNT] = {};

    bool readOk = false;

    if (writeOk)
    {
        const uint32_t readStartedUs = micros();

        readOk =
            JWPLC_ModbusRTU.readHoldingRegistersSync(
                id,
                0,
                HR_COUNT,
                regs,
                MODBUS_TIMEOUT_MS);

        modbusTiming.lastReadUs =
            micros() - readStartedUs;

        if (modbusTiming.lastReadUs > modbusTiming.maxReadUs)
            modbusTiming.maxReadUs = modbusTiming.lastReadUs;
    }
    else
    {
        modbusTiming.lastReadUs = 0;
    }

    modbusTiming.lastPollUs =
        micros() - pollStartedUs;

    if (modbusTiming.lastPollUs > modbusTiming.maxPollUs)
        modbusTiming.maxPollUs = modbusTiming.lastPollUs;

    const bool valid =
        writeOk &&
        readOk &&
        regs[HR_SIGNATURE] == MODBUS_SIGNATURE &&
        regs[HR_PROTOCOL_VERSION] == MODBUS_PROTOCOL_VERSION &&
        regs[HR_NODE_ID] == id &&
        regs[HR_CHALLENGE] == challenge;

    if (valid)
    {
        modbusTiming.pollOk++;

        memcpy(node.regs, regs, sizeof(regs));

        node.seen = true;
        node.lastSeenMs = millis();
        node.consecutivePollFails = 0;
        node.lastChallengeSent = challenge;

        const uint32_t boot =
            get32(regs, HR_BOOT_COUNT_HI);

        if (!node.bootKnown)
        {
            node.bootKnown = true;
            node.lastBootCount = boot;
        }
        else if (boot != node.lastBootCount)
        {
            const uint32_t oldBoot =
                node.lastBootCount;

            node.lastBootCount = boot;

            if (masterSoakRunning)
            {
                char detail[80];
                snprintf(
                    detail,
                    sizeof(detail),
                    "S%u boot %lu->%lu",
                    id,
                    (unsigned long)oldBoot,
                    (unsigned long)boot);

                latchError(ERR_RESET, detail);

                // Permite seguir diagnosticando luego de la recuperacion.
                (void)sendSlaveCommand(
                    id,
                    CMD_START_SOAK);

                (void)masterWriteRegister(
                    id,
                    HR_ETH_OWNER,
                    ethWindow.owner);
            }
        }
    }
    else
    {
        modbusTiming.pollFail++;
        node.consecutivePollFails++;

        if (masterSoakRunning &&
            node.consecutivePollFails == 3)
        {
            char detail[64];
            snprintf(
                detail,
                sizeof(detail),
                "S%u poll fail x3",
                id);

            latchError(ERR_MODBUS, detail);
        }
    }
}


static void recordMasterRuntimePollFailure(
    NodeSnapshot &node,
    uint8_t id,
    uint8_t category,
    JWPLCModbusRTUError result)
{
    modbusTiming.pollFail++;
    node.consecutivePollFails++;
    masterRuntimeLastResult = result;

    if (category == 1)
        modbusTiming.writeFail++;
    else if (category == 2)
        modbusTiming.readFail++;
    else
        modbusTiming.validateFail++;

    if (masterSoakRunning &&
        node.consecutivePollFails == 3)
    {
        char detail[80];
        snprintf(
            detail,
            sizeof(detail),
            "S%u async poll fail x3 cat=%u mb=%u",
            id,
            category,
            (unsigned)result);

        latchError(ERR_MODBUS, detail);
    }
}

static void commitMasterRuntimeSnapshot(
    NodeSnapshot &node,
    uint8_t id,
    uint16_t challenge)
{
    modbusTiming.pollOk++;

    memcpy(
        node.regs,
        masterRuntimePollRegs,
        sizeof(masterRuntimePollRegs));

    node.seen = true;
    node.lastSeenMs = millis();
    node.consecutivePollFails = 0;
    node.lastChallengeSent = challenge;

    const uint32_t boot =
        get32(masterRuntimePollRegs, HR_BOOT_COUNT_HI);

    if (!node.bootKnown)
    {
        node.bootKnown = true;
        node.lastBootCount = boot;
    }
    else if (boot != node.lastBootCount)
    {
        const uint32_t oldBoot =
            node.lastBootCount;

        node.lastBootCount = boot;

        if (masterSoakRunning)
        {
            char detail[80];
            snprintf(
                detail,
                sizeof(detail),
                "S%u boot %lu->%lu",
                id,
                (unsigned long)oldBoot,
                (unsigned long)boot);

            latchError(ERR_RESET, detail);

            // La recuperación completa se hará en el siguiente hueco Sync.
            // No abrir una operación bloqueante desde dentro del poll async.
        }
    }
}

static void finishMasterRuntimePoll()
{
    modbusTiming.lastPollUs =
        micros() - masterRuntimePollStartedUs;

    if (modbusTiming.lastPollUs > modbusTiming.maxPollUs)
        modbusTiming.maxPollUs = modbusTiming.lastPollUs;

    masterRuntimePollPhase = MASTER_RT_POLL_IDLE;
    masterRuntimePollSlave = 0;
    lastMasterPollMs = millis();
}

static void masterPollOneSlaveCooperative()
{
    if (!isMaster() ||
        !modbusReady ||
        cfg.expectedSlaves == 0)
    {
        return;
    }

    // Fase 1: iniciar nuevo FC06 únicamente cuando toca.
    if (masterRuntimePollPhase == MASTER_RT_POLL_IDLE)
    {
        if (JWPLC_ModbusRTU.masterBusy())
            return;

        if (JWPLC_ModbusRTU.masterDone())
            JWPLC_ModbusRTU.clearMasterResult();

        const uint32_t now = millis();

        if ((uint32_t)(now - lastMasterPollMs) <
            MASTER_POLL_GAP_MS)
        {
            return;
        }

        const uint8_t id = masterPollNextSlave;

        masterPollNextSlave++;

        if (masterPollNextSlave > cfg.expectedSlaves)
            masterPollNextSlave = 1;

        masterChallengeCounter++;

        masterRuntimePollSlave = id;
        masterRuntimePollChallenge =
            (uint16_t)(masterChallengeCounter & 0xFFFFU);

        memset(
            masterRuntimePollRegs,
            0,
            sizeof(masterRuntimePollRegs));

        masterRuntimePollStartedUs = micros();
        masterRuntimeOpStartedUs = micros();

        const bool accepted =
            JWPLC_ModbusRTU.requestWriteSingleRegister(
                id,
                HR_CHALLENGE,
                masterRuntimePollChallenge,
                MODBUS_TIMEOUT_MS);

        if (!accepted)
        {
            NodeSnapshot &node = nodes[id];

            recordMasterRuntimePollFailure(
                node,
                id,
                1,
                JWPLC_ModbusRTU.lastError());

            finishMasterRuntimePoll();
            return;
        }

        masterRuntimePollPhase =
            MASTER_RT_POLL_WAIT_WRITE;

        return;
    }

    // task() ya se llamó al inicio del loop. Mientras siga BUSY no hacemos nada.
    if (JWPLC_ModbusRTU.masterBusy())
        return;

    if (!JWPLC_ModbusRTU.masterDone())
        return;

    const bool succeeded =
        JWPLC_ModbusRTU.masterSucceeded();

    const JWPLCModbusRTUError result =
        JWPLC_ModbusRTU.masterResult();

    masterRuntimeLastResult = result;

    if (masterRuntimePollPhase ==
        MASTER_RT_POLL_WAIT_WRITE)
    {
        modbusTiming.lastWriteUs =
            micros() - masterRuntimeOpStartedUs;

        if (modbusTiming.lastWriteUs >
            modbusTiming.maxWriteUs)
        {
            modbusTiming.maxWriteUs =
                modbusTiming.lastWriteUs;
        }

        JWPLC_ModbusRTU.clearMasterResult();

        if (!succeeded)
        {
            NodeSnapshot &node =
                nodes[masterRuntimePollSlave];

            recordMasterRuntimePollFailure(
                node,
                masterRuntimePollSlave,
                1,
                result);

            finishMasterRuntimePoll();
            return;
        }

        masterRuntimeOpStartedUs = micros();

        const bool accepted =
            JWPLC_ModbusRTU.requestReadHoldingRegisters(
                masterRuntimePollSlave,
                0,
                HR_COUNT,
                masterRuntimePollRegs,
                MODBUS_TIMEOUT_MS);

        if (!accepted)
        {
            NodeSnapshot &node =
                nodes[masterRuntimePollSlave];

            recordMasterRuntimePollFailure(
                node,
                masterRuntimePollSlave,
                2,
                JWPLC_ModbusRTU.lastError());

            finishMasterRuntimePoll();
            return;
        }

        masterRuntimePollPhase =
            MASTER_RT_POLL_WAIT_READ;

        return;
    }

    if (masterRuntimePollPhase ==
        MASTER_RT_POLL_WAIT_READ)
    {
        modbusTiming.lastReadUs =
            micros() - masterRuntimeOpStartedUs;

        if (modbusTiming.lastReadUs >
            modbusTiming.maxReadUs)
        {
            modbusTiming.maxReadUs =
                modbusTiming.lastReadUs;
        }

        JWPLC_ModbusRTU.clearMasterResult();

        NodeSnapshot &node =
            nodes[masterRuntimePollSlave];

        if (!succeeded)
        {
            recordMasterRuntimePollFailure(
                node,
                masterRuntimePollSlave,
                2,
                result);

            finishMasterRuntimePoll();
            return;
        }

        const bool valid =
            masterRuntimePollRegs[HR_SIGNATURE] ==
                MODBUS_SIGNATURE &&
            masterRuntimePollRegs[HR_PROTOCOL_VERSION] ==
                MODBUS_PROTOCOL_VERSION &&
            masterRuntimePollRegs[HR_NODE_ID] ==
                masterRuntimePollSlave &&
            masterRuntimePollRegs[HR_CHALLENGE] ==
                masterRuntimePollChallenge;

        if (!valid)
        {
            recordMasterRuntimePollFailure(
                node,
                masterRuntimePollSlave,
                3,
                JWPLC_MODBUS_INVALID_RESPONSE);

            finishMasterRuntimePoll();
            return;
        }

        commitMasterRuntimeSnapshot(
            node,
            masterRuntimePollSlave,
            masterRuntimePollChallenge);

        masterRuntimeLastResult =
            JWPLC_MODBUS_OK;

        finishMasterRuntimePoll();
    }
}

// ============================================================================
// Commissioning Master
// ============================================================================

static bool localCommissioningReady()
{
    return
        cfg.roleValid &&
        cfg.bleQualified &&
        wifiPass &&
        framReady &&
        rtcPresent &&
        modbusReady;
}

static bool nodeOnline(uint8_t id)
{
    if (id == 0 || id > cfg.expectedSlaves)
        return false;

    const NodeSnapshot &node = nodes[id];

    return
        node.seen &&
        (uint32_t)(millis() - node.lastSeenMs) < 1500UL;
}

static bool slaveCommissioningReady(uint8_t id)
{
    if (!nodeOnline(id))
        return false;

    const uint16_t flags =
        nodes[id].regs[HR_STATUS_FLAGS];

    const uint16_t required =
        ST_ROLE |
        ST_BLE_PASS |
        ST_WIFI_PASS |
        ST_MODBUS_READY |
        ST_FRAM_READY;

    return (flags & required) == required;
}

static bool allNodesCommissioned()
{
    if (!isMaster() || !localCommissioningReady())
        return false;

    for (uint8_t id = 1; id <= cfg.expectedSlaves; ++id)
    {
        if (!slaveCommissioningReady(id))
            return false;
    }

    return true;
}

static bool allNodesRtcSynced()
{
    if (!rtcSynced)
        return false;

    for (uint8_t id = 1; id <= cfg.expectedSlaves; ++id)
    {
        if (!nodeOnline(id))
            return false;

        if ((nodes[id].regs[HR_STATUS_FLAGS] &
             ST_RTC_SYNCED) == 0)
        {
            return false;
        }

        const uint32_t remoteEpoch =
            get32(
                nodes[id].regs,
                HR_RTC_EPOCH_HI);

        const int32_t delta =
            (int32_t)remoteEpoch -
            (int32_t)rtcEpoch;

        if (delta < -3 || delta > 3)
            return false;
    }

    return true;
}

static void printCommissioningTable()
{
    if (!isMaster())
        return;

    Serial.println();
    Serial.println(F("---- COMMISSIONING ----"));

    Serial.print(F("LOCAL "));
    Serial.print(shortRoleName());
    Serial.print(F(" BLE="));
    Serial.print(cfg.bleQualified ? F("OK") : F("--"));
    Serial.print(F(" WIFI="));
    Serial.print(wifiPass ? F("OK") : F("--"));
    Serial.print(F(" FRAM="));
    Serial.print(framReady ? F("OK") : F("--"));
    Serial.print(F(" RTC="));
    Serial.print(rtcPresent ? F("OK") : F("--"));
    Serial.print(F(" ETH="));
    Serial.println(JWPLC_Ethernet.isReady() ? F("READY") : F("WAIT"));

    for (uint8_t id = 1; id <= cfg.expectedSlaves; ++id)
    {
        Serial.print(F("S"));
        Serial.print(id);
        Serial.print(F(" online="));
        Serial.print(nodeOnline(id) ? F("YES") : F("NO"));

        if (nodes[id].seen)
        {
            const uint16_t flags =
                nodes[id].regs[HR_STATUS_FLAGS];

            Serial.print(F(" BLE="));
            Serial.print((flags & ST_BLE_PASS) ? F("OK") : F("--"));

            Serial.print(F(" WIFI="));
            Serial.print((flags & ST_WIFI_PASS) ? F("OK") : F("--"));

            Serial.print(F(" FRAM="));
            Serial.print((flags & ST_FRAM_READY) ? F("OK") : F("--"));

            Serial.print(F(" RTC="));
            Serial.print((flags & ST_RTC_SYNCED) ? F("SYNC") : F("--"));
        }

        Serial.println();
    }
}

static void startInitialRtcSyncMaster()
{
    if (!isMaster())
        return;

    if (!allNodesCommissioned())
        return;

    if (!JWPLC_Ethernet.isReady())
    {
        soakState = SOAK_WAIT_RTC_SYNC;

        Serial.println();
        Serial.println(F("[RTC SYNC] Todos los nodos listos."));
        Serial.println(F("[RTC SYNC] Conecta Ethernet al MASTER."));
        Serial.println(F("[RTC SYNC] Esperando W5500 READY + NTP..."));

        return;
    }

    const uint32_t now = millis();

    if (lastInitialNtpAttemptMs != 0 &&
        (uint32_t)(now - lastInitialNtpAttemptMs) < 10000UL)
    {
        return;
    }

    lastInitialNtpAttemptMs = now;

    uint32_t ntpLocal = 0;

    Serial.println();
    Serial.println(F("[RTC SYNC] querying NTP via MASTER Ethernet..."));

    if (!queryNtpEthernet(ntpLocal))
    {
        initialNtpFailCount++;

        Serial.print(F("[RTC SYNC] NTP attempt FAIL #"));
        Serial.println(initialNtpFailCount);

        if (initialNtpFailCount == 3)
            latchError(ERR_NTP, "initial Ethernet NTP failed x3");

        return;
    }

    initialNtpFailCount = 0;
    syncLocalRtc(ntpLocal);

    if (!rtcSynced)
        return;

    for (uint8_t id = 1; id <= cfg.expectedSlaves; ++id)
    {
        uint32_t currentEpoch = rtcEpoch;
        (void)JWPLC_RTC.readUnix(currentEpoch);

        (void)sendSlaveCommand(
            id,
            CMD_SYNC_RTC,
            0,
            0,
            currentEpoch);
    }

    masterInitialRtcSyncSent = true;
    soakState = SOAK_WAIT_RTC_SYNC;

    Serial.println(F("[RTC SYNC] commands sent to all Slaves"));
}

static void serviceMasterCommissioning()
{
    if (!isMaster() || masterSoakRunning)
        return;

    const uint32_t now = millis();

    if ((uint32_t)(now - lastCommissionPrintMs) >=
        MASTER_COMMISSION_PRINT_MS)
    {
        lastCommissionPrintMs = now;
        printCommissioningTable();
    }

    if (!masterInitialRtcSyncSent)
    {
        if (allNodesCommissioned())
            startInitialRtcSyncMaster();

        return;
    }

    if (allNodesRtcSynced())
    {
        if (soakState != SOAK_READY_TO_START)
        {
            soakState = SOAK_READY_TO_START;

            Serial.println();
            printRule();
            Serial.println(F("ALL_COMMISSIONED=PASS"));
            Serial.println(F("RTC_NETWORK_SYNC=PASS"));
            Serial.println(F("READY_FOR_START"));
            Serial.println(F("Verifica Q->I, 24V y RS-485."));
            Serial.println(F("Luego envia START al MASTER."));
            printRule();

            tripleBeep();
        }
    }
}

// ============================================================================
// Ethernet window local + drift
// ============================================================================

static void finalizeLocalEthernetWindow()
{
    if (!ethWindow.localWasOwner)
        return;

    const uint32_t okDelta =
        ethHttp.ok - ethWindow.startOk;

    const uint32_t failDelta =
        ethHttp.fail - ethWindow.startFail;

    const bool pass =
        okDelta > 0 &&
        failDelta == 0;

    if (pass)
    {
        ethWindow.windowsOk++;
    }
    else
    {
        ethWindow.windowsFail++;
        latchError(ERR_ETH, "Ethernet window failed");
    }

    Serial.print(F("[ETH WINDOW] node="));
    Serial.print(shortRoleName());
    Serial.print(F(" result="));
    Serial.print(pass ? F("PASS") : F("FAIL"));
    Serial.print(F(" ok="));
    Serial.print(okDelta);
    Serial.print(F(" fail="));
    Serial.println(failDelta);

    ethWindow.localWasOwner = false;
    ethWindow.ntpDone = false;
    ethWindow.activity = false;
}

static void beginLocalEthernetWindow()
{
    ethWindow.localWasOwner = true;
    ethWindow.ntpDone = false;
    ethWindow.activity = false;

    ethWindow.startOk = ethHttp.ok;
    ethWindow.startFail = ethHttp.fail;

    Serial.print(F("[ETH WINDOW] local node "));
    Serial.print(shortRoleName());
    Serial.println(F(" selected; waiting LINK/READY"));
}

static void serviceLocalEthernetWindow()
{
    const bool ownerNow =
        soakState == SOAK_RUNNING &&
        ethWindow.owner == localNodeOrdinal();

    if (ownerNow && !ethWindow.localWasOwner)
        beginLocalEthernetWindow();

    if (!ownerNow && ethWindow.localWasOwner)
    {
        finalizeLocalEthernetWindow();
        return;
    }

    if (!ownerNow)
        return;

    if (!JWPLC_Ethernet.isReady())
        return;

    if (!ethWindow.ntpDone)
    {
        ethWindow.ntpDone = true;

        uint32_t ntpLocal = 0;

        if (queryNtpEthernet(ntpLocal))
        {
            ntpValid = true;

            uint32_t localRtc = 0;

            if (JWPLC_RTC.readUnix(localRtc))
            {
                int32_t drift =
                    (int32_t)localRtc -
                    (int32_t)ntpLocal;

                if (drift < -32768) drift = -32768;
                if (drift > 32767) drift = 32767;

                ntpDriftSeconds = (int16_t)drift;

                Serial.print(F("[NTP DRIFT] node="));
                Serial.print(shortRoleName());
                Serial.print(F(" drift="));
                Serial.print(ntpDriftSeconds);
                Serial.println(F("s"));
            }
        }
        else
        {
            Serial.println(F("[NTP] window query failed (HTTP LAN test continues)"));
        }
    }

    const uint32_t now = millis();

    if ((uint32_t)(now - lastEthHttpMs) >=
        ETH_HTTP_PERIOD_MS)
    {
        lastEthHttpMs = now;
        recordEthHttpResult(sendEthernetTelemetry());
    }
}

// ============================================================================
// Rotacion Ethernet Master
// ============================================================================

static uint8_t totalNetworkNodes()
{
    if (!isMaster())
        return 1;

    return (uint8_t)(cfg.expectedSlaves + 1U);
}

static void configureEthernetRotation()
{
    const uint8_t totalNodes =
        totalNetworkNodes();

    const uint32_t twoRoundWindow =
        SOAK_TARGET_MS /
        ((uint32_t)totalNodes * 2UL);

    if (twoRoundWindow >= ETH_MIN_WINDOW_MS)
        rotationRounds = 2;
    else
        rotationRounds = 1;

    rotationTotalSlots =
        (uint16_t)totalNodes *
        rotationRounds;

    ethWindowDurationMs =
        SOAK_TARGET_MS /
        rotationTotalSlots;

    if (ethWindowDurationMs > ETH_MAX_WINDOW_MS)
        ethWindowDurationMs = ETH_MAX_WINDOW_MS;

    rotationSlot = 0;
    ethWindowStartMs = 0;
    rotationWaitingForReady = true;

    Serial.print(F("[ETH ROTATION] nodes="));
    Serial.print(totalNodes);
    Serial.print(F(" rounds="));
    Serial.print(rotationRounds);
    Serial.print(F(" slots="));
    Serial.print(rotationTotalSlots);
    Serial.print(F(" window_min="));
    Serial.println(ethWindowDurationMs / 60000UL);
}

static uint16_t ownerForRotationSlot(uint16_t slot)
{
    return
        (uint16_t)(
            slot %
            totalNetworkNodes());
}

static bool ethernetOwnerReady(uint16_t owner)
{
    if (owner == 0)
        return JWPLC_Ethernet.isReady();

    if (owner < 1 || owner > cfg.expectedSlaves)
        return false;

    if (!nodeOnline((uint8_t)owner))
        return false;

    return
        (nodes[owner].regs[HR_STATUS_FLAGS] &
         ST_ETH_READY) != 0;
}

static void announceEthernetMove(uint16_t nextOwner)
{
    Serial.println();
    printRule();
    Serial.print(F("MOVE_ETHERNET_CABLE_TO="));

    if (nextOwner == 0)
        Serial.println(F("MASTER"));
    else
    {
        Serial.print(F("S"));
        Serial.println(nextOwner);
    }

    Serial.println(F("La ventana empieza solo cuando LINK/READY sea detectado."));
    printRule();

    tripleBeep();

    if (nextOwner > 0 &&
        nextOwner <= cfg.expectedSlaves)
    {
        (void)sendSlaveCommand(
            (uint8_t)nextOwner,
            CMD_BEEP);
    }
}

static void completeSoakMaster();

static void advanceEthernetRotation()
{
    // Forzar cierre de la ventana local/remote anterior.
    setEthernetOwner(ETH_OWNER_NONE);

    rotationSlot++;
    ethWindowStartMs = 0;
    rotationWaitingForReady = true;

    if (rotationSlot >= rotationTotalSlots)
    {
        completeSoakMaster();
        return;
    }

    const uint16_t nextOwner =
        ownerForRotationSlot(rotationSlot);

    announceEthernetMove(nextOwner);
    setEthernetOwner(nextOwner);
}

static void serviceMasterEthernetRotation()
{
    if (!isMaster() ||
        !masterSoakRunning ||
        rotationSlot >= rotationTotalSlots)
    {
        return;
    }

    const uint16_t owner =
        ownerForRotationSlot(rotationSlot);

    if (ethWindow.owner != owner)
        setEthernetOwner(owner);

    if (ethWindowStartMs == 0)
    {
        if (!ethernetOwnerReady(owner))
            return;

        ethWindowStartMs = millis();
        rotationWaitingForReady = false;

        Serial.print(F("[ETH ROTATION] slot "));
        Serial.print(rotationSlot + 1);
        Serial.print('/');
        Serial.print(rotationTotalSlots);
        Serial.print(F(" START owner="));

        if (owner == 0)
            Serial.print(F("MASTER"));
        else
        {
            Serial.print('S');
            Serial.print(owner);
        }

        Serial.print(F(" duration="));
        Serial.print(ethWindowDurationMs / 60000UL);
        Serial.println(F("min"));

        beep(1900, 100);
        return;
    }

    if ((uint32_t)(millis() - ethWindowStartMs) >=
        ethWindowDurationMs)
    {
        advanceEthernetRotation();
    }
}

// ============================================================================
// microSD Master
// ============================================================================

static bool initMasterSD()
{
    if (!isMaster())
        return false;

    sdReady =
        JWPLCSD::isReady() ||
        JWPLCSD::begin();

    if (!sdReady)
    {
        Serial.print(F("[SD] FAIL: "));
        Serial.println(JWPLCSD::lastErrorString());
        latchError(ERR_SD, "Master microSD not ready");
        return false;
    }

    Serial.println(F("[SD] READY"));

    if (!JWPLC_SD.exists(SD_SNAPSHOT_PATH))
    {
        auto f =
            JWPLC_SD.open(
                SD_SNAPSHOT_PATH,
                FILE_APPEND);

        if (f)
        {
            f.println(
                "rtc,node,boot,state,status,q,i,io_err,"
                "fram_ok,fram_fail,wifi_ok,wifi_fail,"
                "eth_ok,eth_fail,first_err,err_count");
            f.close();
        }
    }

    if (!JWPLC_SD.exists(SD_EVENTS_PATH))
    {
        auto f =
            JWPLC_SD.open(
                SD_EVENTS_PATH,
                FILE_APPEND);

        if (f)
        {
            f.println("rtc,uptime,node,event,detail");
            f.close();
        }
    }

    return true;
}

static void sdLogEvent(const char *eventName, const char *detail)
{
    if (!isMaster() || !sdReady)
        return;

    auto f =
        JWPLC_SD.open(
            SD_EVENTS_PATH,
            FILE_APPEND);

    if (!f)
    {
        sdFail++;
        return;
    }

    f.print(rtcEpoch);
    f.print(',');
    f.print(millis() / 1000UL);
    f.print(',');
    f.print(shortRoleName());
    f.print(',');
    f.print(eventName ? eventName : "");
    f.print(',');
    f.println(detail ? detail : "");
    f.close();

    sdOk++;
}

static void writeSnapshotRow(
    JWPLCFile &f,
    const char *name,
    const uint16_t *regs)
{
    f.print(get32(regs, HR_RTC_EPOCH_HI));
    f.print(',');
    f.print(name);
    f.print(',');
    f.print(get32(regs, HR_BOOT_COUNT_HI));
    f.print(',');
    f.print(regs[HR_SOAK_STATE]);
    f.print(',');
    f.print(regs[HR_STATUS_FLAGS]);
    f.print(',');
    f.print(regs[HR_Q_BITMAP]);
    f.print(',');
    f.print(regs[HR_I_BITMAP]);
    f.print(',');
    f.print(get32(regs, HR_IO_MISMATCH_HI));
    f.print(',');
    f.print(get32(regs, HR_FRAM_OK_HI));
    f.print(',');
    f.print(get32(regs, HR_FRAM_FAIL_HI));
    f.print(',');
    f.print(get32(regs, HR_WIFI_HTTP_OK_HI));
    f.print(',');
    f.print(get32(regs, HR_WIFI_HTTP_FAIL_HI));
    f.print(',');
    f.print(get32(regs, HR_ETH_HTTP_OK_HI));
    f.print(',');
    f.print(get32(regs, HR_ETH_HTTP_FAIL_HI));
    f.print(',');
    f.print(regs[HR_FIRST_ERROR]);
    f.print(',');
    f.println(regs[HR_ERROR_COUNT]);
}

static void logMasterSnapshotsToSD()
{
    if (!isMaster() || !sdReady)
        return;

    refreshHoldingRegisters();

    auto f =
        JWPLC_SD.open(
            SD_SNAPSHOT_PATH,
            FILE_APPEND);

    if (!f)
    {
        sdFail++;
        latchError(ERR_SD, "snapshot open failed");
        return;
    }

    writeSnapshotRow(
        f,
        shortRoleName().c_str(),
        holdingRegisters);

    for (uint8_t id = 1; id <= cfg.expectedSlaves; ++id)
    {
        if (!nodes[id].seen)
            continue;

        String name = String("S") + String(id);

        writeSnapshotRow(
            f,
            name.c_str(),
            nodes[id].regs);
    }

    f.close();
    sdOk++;
}

static void runSdReadWriteProbe()
{
    if (!isMaster() || !sdReady)
        return;

    sdRwSequence++;

    JWPLC_SD.remove(SD_RW_PATH);

    {
        auto f =
            JWPLC_SD.open(
                SD_RW_PATH,
                FILE_WRITE);

        if (!f)
        {
            sdFail++;
            latchError(ERR_SD, "RW write open failed");
            return;
        }

        f.print(F("JWPLC_SOAK,"));
        f.println(sdRwSequence);
        f.close();
    }

    String restored;

    {
        auto f =
            JWPLC_SD.open(
                SD_RW_PATH,
                FILE_READ);

        if (!f)
        {
            sdFail++;
            latchError(ERR_SD, "RW read open failed");
            return;
        }

        while (f.available() && restored.length() < 64)
            restored += (char)f.read();

        f.close();
    }

    const String expected =
        String("JWPLC_SOAK,") +
        String(sdRwSequence);

    if (restored.indexOf(expected) >= 0)
    {
        sdOk++;
    }
    else
    {
        sdFail++;
        latchError(ERR_SD, "RW compare failed");
    }
}

static void serviceMasterSD()
{
    if (!isMaster() ||
        !sdReady ||
        soakState != SOAK_RUNNING)
    {
        return;
    }

    const uint32_t now = millis();

    if ((uint32_t)(now - lastSdLogMs) >=
        SD_LOG_PERIOD_MS)
    {
        lastSdLogMs = now;
        logMasterSnapshotsToSD();
    }

    if ((uint32_t)(now - lastSdRwMs) >=
        SD_RW_PERIOD_MS)
    {
        lastSdRwMs = now;
        runSdReadWriteProbe();
    }
}

// ============================================================================
// Start / Stop soak
// ============================================================================

static void startSoakMaster()
{
    if (!isMaster())
        return;

    if (soakState != SOAK_READY_TO_START)
    {
        Serial.println(F("START rejected: network not READY."));
        return;
    }

    Serial.println();
    printRule();
    Serial.println(F("STARTING DISTRIBUTED SOAK"));
    printRule();

    for (uint8_t id = 1; id <= cfg.expectedSlaves; ++id)
    {
        if (!sendSlaveCommand(id, CMD_START_SOAK))
        {
            latchError(
                ERR_MODBUS,
                "START command failed");
        }
    }

    soakState = SOAK_RUNNING;
    masterSoakRunning = true;
    masterSoakStartMs = millis();

    startIoStress();

    configureEthernetRotation();

    setEthernetOwner(
        ownerForRotationSlot(0));

    ethWindowStartMs = 0;

    if (sdReady)
        sdLogEvent("SOAK_START", "");

    Serial.println(F("SOAK_RUNNING=YES"));
    Serial.println(F("IDLE: observa Q/I + BUS + ETH."));
    Serial.println(F("USER: dashboard stress."));
}

static void stopAllSlaves()
{
    if (!isMaster())
        return;

    for (uint8_t id = 1; id <= cfg.expectedSlaves; ++id)
        (void)sendSlaveCommand(id, CMD_STOP_SOAK);
}

static void completeSoakMaster()
{
    if (!isMaster() || !masterSoakRunning)
        return;

    Serial.println();
    printRule();
    Serial.println(F("ETH ROTATION COMPLETE"));
    Serial.println(F("Stopping distributed soak..."));
    printRule();

    setEthernetOwner(ETH_OWNER_NONE);
    stopAllSlaves();

    stopIoStress();

    masterSoakRunning = false;
    soakState = SOAK_COMPLETE;

    restoreFRAM();

    if (sdReady)
    {
        logMasterSnapshotsToSD();
        sdLogEvent("SOAK_COMPLETE", "");
    }

    tripleBeep();
    delay(250);
    tripleBeep();

    Serial.println();
    printRule();
    Serial.println(F("SOAK_COMPLETE=YES"));
    Serial.print(F("RESULT="));
    Serial.println(
        firstError == ERR_NONE
            ? F("PASS")
            : F("FAIL/REVIEW"));
    Serial.print(F("FIRST_ERROR="));
    Serial.println(errorName(firstError));
    Serial.print(F("ERROR_COUNT="));
    Serial.println(errorCount);
    printRule();
}

static void manualStopMaster()
{
    if (!isMaster())
        return;

    setEthernetOwner(ETH_OWNER_NONE);
    stopAllSlaves();
    stopIoStress();

    masterSoakRunning = false;
    soakState = SOAK_STOPPED;

    restoreFRAM();

    if (sdReady)
        sdLogEvent("SOAK_STOPPED", "manual");

    Serial.println(F("SOAK_STOPPED"));
}

// ============================================================================
// USER UI
// ============================================================================

static void uiHeader(
    Adafruit_ST7789 &tft,
    const char *title)
{
    tft.fillScreen(ST77XX_BLACK);

    tft.setTextSize(1);
    tft.setTextColor(ST77XX_CYAN);
    tft.setCursor(5, 5);
    tft.print(nodeName());

    tft.setTextColor(ST77XX_WHITE);
    tft.setCursor(5, 18);
    tft.print(title);

    tft.drawFastHLine(
        0,
        30,
        tft.width(),
        ST77XX_BLUE);
}

static void uiRow(
    Adafruit_ST7789 &tft,
    int16_t y,
    const char *label,
    const String &value,
    uint16_t valueColor = ST77XX_WHITE)
{
    tft.setTextSize(1);

    tft.setTextColor(ST77XX_CYAN);
    tft.setCursor(5, y);
    tft.print(label);

    tft.setTextColor(valueColor);
    tft.setCursor(100, y);
    tft.print(value);
}

static String formatDuration(uint32_t ms)
{
    const uint32_t sec = ms / 1000UL;

    char buffer[24];

    snprintf(
        buffer,
        sizeof(buffer),
        "%02lu:%02lu:%02lu",
        (unsigned long)(sec / 3600UL),
        (unsigned long)((sec % 3600UL) / 60UL),
        (unsigned long)(sec % 60UL));

    return String(buffer);
}

static String formatRtc()
{
    JWRTCDateTime dt;

    if (!JWPLC_RTC.read(dt))
        return "RTC ERR";

    char buffer[28];

    snprintf(
        buffer,
        sizeof(buffer),
        "%02u:%02u:%02u",
        dt.hour,
        dt.minute,
        dt.second);

    return String(buffer);
}

static void drawUserPage0(Adafruit_ST7789 &tft)
{
    uiHeader(tft, "STATUS / RTC / STORAGE");

    uiRow(
        tft,
        42,
        "STATE",
        soakStateName(soakState));

    uiRow(
        tft,
        55,
        "UP",
        formatDuration(millis()));

    uiRow(
        tft,
        68,
        "RTC",
        formatRtc(),
        rtcPresent ? ST77XX_GREEN : ST77XX_RED);

    uiRow(
        tft,
        81,
        "RTC SYNC",
        rtcSynced ? "PASS" : "WAIT",
        rtcSynced ? ST77XX_GREEN : ST77XX_YELLOW);

    uiRow(
        tft,
        94,
        "NTP DRIFT",
        String(ntpDriftSeconds) + " s");

    uiRow(
        tft,
        107,
        "RTC TEMP",
        String(rtcTemperatureCenti / 100.0f, 2) + " C");

    uiRow(
        tft,
        120,
        "FRAM",
        String(framOk) + "/" + String(framFail),
        framFail == 0 ? ST77XX_GREEN : ST77XX_RED);

    if (isMaster())
    {
        uiRow(
            tft,
            133,
            "SD",
            String(sdOk) + "/" + String(sdFail),
            sdReady && sdFail == 0
                ? ST77XX_GREEN
                : ST77XX_RED);
    }
    else
    {
        uiRow(tft, 133, "SD", "N/A");
    }

    uiRow(
        tft,
        146,
        "ERR",
        String(errorName(firstError)) +
            " (" + String(errorCount) + ")",
        firstError == ERR_NONE
            ? ST77XX_GREEN
            : ST77XX_RED);
}

// IMPORTANTE:
// Los callbacks USER de la TFT se ejecutan mientras JWPLC_Display mantiene
// adquirido el mutex SPI para la pantalla. Por tanto, NO se debe llamar aquí
// JWPLC_Ethernet.statusString(), linkUp(), linkStatus(), localIP(), etc. porque
// esas APIs vuelven a intentar adquirir el mismo bus.
//
// Este helper usa únicamente estado ya cacheado por JWPLC_Ethernet.service().
static String ethernetCachedUiStatus()
{
    const JWPLCEthernetRuntimeState state =
        JWPLC_Ethernet.runtimeState();

    const JWPLCEthernetError error =
        JWPLC_Ethernet.lastError();

    if (JWPLC_Ethernet.isReady())
    {
        if (error == JWPLC_ETH_BUS_LOCK_TIMEOUT)
            return "READY / SPI WAIT";

        if (error == JWPLC_ETH_DHCP_FAILED)
            return "READY / DHC";

        return "READY";
    }

    switch (state)
    {
    case JWPLC_ETH_STATE_NOT_STARTED:
        return "Not started";

    case JWPLC_ETH_STATE_PROBING:
        return "Starting";

    case JWPLC_ETH_STATE_PHY_READY:
        return "PHY ready";

    case JWPLC_ETH_STATE_LINK_OFF:
        return "Link OFF";

    case JWPLC_ETH_STATE_DHCP_PENDING:
        return "DHCP pending";

    case JWPLC_ETH_STATE_ERROR:
        break;

    case JWPLC_ETH_STATE_READY:
    default:
        break;
    }

    switch (error)
    {
    case JWPLC_ETH_BUS_LOCK_TIMEOUT:
        return "SPI lock timeout";

    case JWPLC_ETH_NO_HARDWARE:
        return "No hardware";

    case JWPLC_ETH_LINK_OFF:
        return "Link OFF";

    case JWPLC_ETH_DHCP_FAILED:
        return "DHCP failed";

    case JWPLC_ETH_INVALID_IP:
        return "Invalid IP";

    case JWPLC_ETH_SPI_NOT_READY:
        return "SPI not ready";

    case JWPLC_ETH_DISABLED:
        return "Disabled";

    case JWPLC_ETH_OK:
    default:
        return "WAIT";
    }
}

static uint16_t ethernetCachedUiColor()
{
    if (JWPLC_Ethernet.isReady())
    {
        const JWPLCEthernetError error =
            JWPLC_Ethernet.lastError();

        if (error == JWPLC_ETH_BUS_LOCK_TIMEOUT ||
            error == JWPLC_ETH_DHCP_FAILED)
        {
            return ST77XX_YELLOW;
        }

        return ST77XX_GREEN;
    }

    const JWPLCEthernetRuntimeState state =
        JWPLC_Ethernet.runtimeState();

    if (state == JWPLC_ETH_STATE_ERROR)
        return ST77XX_RED;

    return ST77XX_YELLOW;
}

static void drawUserPage1(Adafruit_ST7789 &tft)
{
    uiHeader(tft, "BLE / WIFI / ETHERNET");

    uiRow(
        tft,
        42,
        "BLE",
        cfg.bleQualified ? "PASS" : "WAIT",
        cfg.bleQualified ? ST77XX_GREEN : ST77XX_YELLOW);

    uiRow(
        tft,
        55,
        "BLE CONN",
        String(bleConnections));

    uiRow(
        tft,
        68,
        "WIFI",
        WiFi.status() == WL_CONNECTED
            ? WiFi.localIP().toString()
            : "DISCONNECTED",
        wifiPass ? ST77XX_GREEN : ST77XX_YELLOW);

    uiRow(
        tft,
        81,
        "RSSI",
        WiFi.status() == WL_CONNECTED
            ? String(WiFi.RSSI()) + " dBm"
            : "-");

    uiRow(
        tft,
        94,
        "WIFI HTTP",
        String(wifiHttp.ok) +
            "/" + String(wifiHttp.fail),
        wifiHttp.fail == 0
            ? ST77XX_GREEN
            : ST77XX_YELLOW);

    uiRow(
        tft,
        107,
        "ETH",
        ethernetCachedUiStatus(),
        ethernetCachedUiColor());

    uiRow(
        tft,
        120,
        "ETH HTTP",
        String(ethHttp.ok) +
            "/" + String(ethHttp.fail),
        ethHttp.fail == 0
            ? ST77XX_GREEN
            : ST77XX_YELLOW);

    String owner;

    if (ethWindow.owner == ETH_OWNER_NONE)
        owner = "NONE";
    else if (ethWindow.owner == 0)
        owner = "MASTER";
    else
        owner = String("S") + String(ethWindow.owner);

    uiRow(tft, 133, "ETH OWNER", owner);

    uiRow(
        tft,
        146,
        "ETH WIN",
        String(ethWindow.windowsOk) +
            "/" + String(ethWindow.windowsFail));
}

static void drawUserPage2(Adafruit_ST7789 &tft)
{
    uiHeader(tft, "I/O LOOPBACK COUNTERS");

    int16_t y = 38;

    for (uint8_t i = 0; i < 8; ++i)
    {
        String label =
            String("Q") + String(i) +
            "/I" + String(i);

        String value =
            String(ioStress.qPulses[i]) +
            "/" +
            String(ioStress.iPulses[i]);

        const bool ok =
            ioStress.qPulses[i] ==
            ioStress.iPulses[i];

        uiRow(
            tft,
            y,
            label.c_str(),
            value,
            ok ? ST77XX_GREEN : ST77XX_RED);

        y += 14;
    }

    uiRow(
        tft,
        152,
        "MISMATCH",
        String(ioStress.mismatches),
        ioStress.mismatches == 0
            ? ST77XX_GREEN
            : ST77XX_RED);
}

static void drawUserPage3(Adafruit_ST7789 &tft)
{
    uiHeader(tft, "MODBUS / MASTER VIEW");

    if (!isMaster())
    {
        const JWPLCModbusRTUStats &stats =
            JWPLC_ModbusRTU.stats();

        uiRow(tft, 42, "RX", String(stats.rxFrames));
        uiRow(tft, 55, "TX", String(stats.txFrames));
        uiRow(tft, 68, "REQ OK", String(stats.requestsOk));
        uiRow(tft, 81, "CRC ERR", String(stats.crcErrors));
        uiRow(tft, 94, "EXC", String(stats.exceptionsSent));
        uiRow(tft, 107, "BUS", JWPLC_RS485.hasRecentActivity(1500) ? "ACTIVE" : "IDLE");
        return;
    }

    int16_t y = 42;

    for (uint8_t id = 1; id <= cfg.expectedSlaves && id <= 6; ++id)
    {
        String label =
            String("S") + String(id);

        String value;

        if (!nodeOnline(id))
        {
            value = "OFFLINE";
        }
        else
        {
            value =
                String("Q") +
                String(nodes[id].regs[HR_Q_BITMAP], HEX) +
                " I" +
                String(nodes[id].regs[HR_I_BITMAP], HEX) +
                " E" +
                String(nodes[id].regs[HR_ERROR_COUNT]);
        }

        uiRow(
            tft,
            y,
            label.c_str(),
            value,
            nodeOnline(id)
                ? ST77XX_GREEN
                : ST77XX_RED);

        y += 16;
    }

    if (masterSoakRunning &&
        rotationSlot < rotationTotalSlots)
    {
        const uint16_t owner =
            ownerForRotationSlot(rotationSlot);

        String ownerText =
            owner == 0
                ? "MASTER"
                : String("S") + String(owner);

        uiRow(
            tft,
            145,
            "ETH SLOT",
            String(rotationSlot + 1) +
                "/" +
                String(rotationTotalSlots) +
                " " +
                ownerText);
    }
}

static void drawUserPage4(Adafruit_ST7789 &tft)
{
    uiHeader(tft, "RUNTIME / CORE DIAG");

    uiRow(tft, 42, "LOOP CORE", String((int)xPortGetCoreID()));

    uiRow(
        tft,
        55,
        "WIFI CORE",
        wifiWorkerObservedCore >= 0
            ? String((int)wifiWorkerObservedCore)
            : "-");

    uiRow(
        tft,
        68,
        "MAX LOOP",
        String(maxLoopUs / 1000.0f, 1) + " ms",
        maxLoopUs < LONG_LOOP_WARN_US
            ? ST77XX_GREEN
            : ST77XX_YELLOW);

    if (!isMaster())
    {
        const JWPLCModbusRTUStats &stats =
            JWPLC_ModbusRTU.stats();

        uiRow(tft, 81, ">50ms", String(longLoop50msCount));

        uiRow(
            tft,
            94,
            ">250ms",
            String(longLoop250msCount),
            longLoop250msCount == 0
                ? ST77XX_GREEN
                : ST77XX_RED);

        uiRow(tft, 107, "CRC TOTAL", String(stats.crcErrors));

        uiRow(
            tft,
            120,
            "CRC RUN",
            String(
                stats.crcErrors >= modbusCrcBaseline
                    ? stats.crcErrors - modbusCrcBaseline
                    : stats.crcErrors),
            stats.crcErrors == modbusCrcBaseline
                ? ST77XX_GREEN
                : ST77XX_RED);

        uiRow(
            tft,
            133,
            "CRC AGE",
            modbusLastCrcMs == 0
                ? "-"
                : String((millis() - modbusLastCrcMs) / 1000UL) + " s");

        uiRow(
            tft,
            146,
            "WIFI MAX",
            String(wifiHttp.maxLatencyMs) + " ms");

        return;
    }

    uiRow(
        tft,
        81,
        ">50/>250",
        String(longLoop50msCount) + "/" + String(longLoop250msCount));

    uiRow(
        tft,
        94,
        "MB POLL MAX",
        String(modbusTiming.maxPollUs / 1000.0f, 1) + " ms");

    uiRow(
        tft,
        107,
        "MB FAIL",
        String(modbusTiming.pollFail),
        modbusTiming.pollFail == 0
            ? ST77XX_GREEN
            : ST77XX_RED);

    for (uint8_t id = 1;
         id <= cfg.expectedSlaves && id <= 2;
         ++id)
    {
        const int16_t y = id == 1 ? 120 : 133;
        String label = String("S") + String(id);
        String value = "OFFLINE";
        uint16_t color = ST77XX_RED;

        if (nodes[id].seen)
        {
            const uint32_t crcRun =
                get32(nodes[id].regs, HR_MODBUS_CRC_DELTA_HI);

            const uint32_t slaveLoopUs =
                get32(nodes[id].regs, HR_MAX_LOOP_US_HI);

            value =
                String("C") + String(crcRun) +
                " L" + String(slaveLoopUs / 1000UL) + "ms";

            color =
                crcRun == 0
                    ? ST77XX_GREEN
                    : ST77XX_RED;
        }

        uiRow(tft, y, label.c_str(), value, color);
    }

    uiRow(
        tft,
        146,
        "NET MAX",
        String(wifiHttp.maxLatencyMs) +
            "/" +
            String(ethHttp.maxLatencyMs) +
            " ms");
}

extern "C" bool jwplcUserDisplayRefreshNeededCallback(
    const JWPLC_IOState *io,
    const JWPLC_RTCState *rtc)
{
    (void)io;
    (void)rtc;
    return true;
}

extern "C" void jwplcUserDisplayEnterCallback(void)
{
    userPage = 0;
    lastUserPageChangeMs = millis();

    auto &tft = JWPLC_Display.tft();
    tft.fillScreen(ST77XX_BLACK);
}

extern "C" void jwplcUserDisplayRefreshCallback(
    const JWPLC_IOState *io,
    const JWPLC_RTCState *rtc)
{
    (void)io;
    (void)rtc;

    const uint32_t now = millis();

    if ((uint32_t)(now - lastUserPageChangeMs) >=
        USER_PAGE_MS)
    {
        lastUserPageChangeMs = now;
        userPage = (uint8_t)((userPage + 1U) % 5U);
    }

    auto &tft = JWPLC_Display.tft();

    switch (userPage)
    {
    case 0: drawUserPage0(tft); break;
    case 1: drawUserPage1(tft); break;
    case 2: drawUserPage2(tft); break;
    case 3: drawUserPage3(tft); break;
    case 4: drawUserPage4(tft); break;
    default: drawUserPage0(tft); break;
    }
}

extern "C" void jwplcUserDisplayExitCallback(void)
{
}

// ============================================================================
// Serial status / commands
// ============================================================================

static void printLocalStatus()
{
    Serial.println();
    printRule();
    Serial.print(F("NODE="));
    Serial.println(nodeName());

    Serial.print(F("ROLE="));
    Serial.println(shortRoleName());

    Serial.print(F("STATE="));
    Serial.println(soakStateName(soakState));

    Serial.print(F("BOOT_COUNT="));
    Serial.println(cfg.bootCount);

    Serial.print(F("BLE_PASS="));
    Serial.println(cfg.bleQualified ? F("YES") : F("NO"));

    Serial.print(F("WIFI_PASS="));
    Serial.println(wifiPass ? F("YES") : F("NO"));

    Serial.print(F("WIFI_IP="));
    Serial.println(
        WiFi.status() == WL_CONNECTED
            ? WiFi.localIP().toString()
            : String("NONE"));

    Serial.print(F("MODBUS_READY="));
    Serial.println(modbusReady ? F("YES") : F("NO"));

    Serial.print(F("FRAM="));
    Serial.print(framOk);
    Serial.print('/');
    Serial.println(framFail);

    Serial.print(F("RTC_EPOCH="));
    Serial.println(rtcEpoch);

    Serial.print(F("RTC_SYNCED="));
    Serial.println(rtcSynced ? F("YES") : F("NO"));

    Serial.print(F("Q=0x"));
    Serial.println(ioStress.outputBitmap, HEX);

    Serial.print(F("I=0x"));
    Serial.println(ioStress.inputBitmap, HEX);

    Serial.print(F("IO_MISMATCH="));
    Serial.println(ioStress.mismatches);

    Serial.print(F("WIFI_HTTP="));
    Serial.print(wifiHttp.ok);
    Serial.print('/');
    Serial.println(wifiHttp.fail);

    Serial.print(F("ETH_HTTP="));
    Serial.print(ethHttp.ok);
    Serial.print('/');
    Serial.println(ethHttp.fail);

    Serial.print(F("FIRST_ERROR="));
    Serial.println(errorName(firstError));

    Serial.print(F("ERROR_COUNT="));
    Serial.println(errorCount);

    Serial.print(F("LOOP_CORE="));
    Serial.println((int)xPortGetCoreID());

    Serial.print(F("WIFI_WORKER_CORE="));
    Serial.println((int)wifiWorkerObservedCore);

    Serial.print(F("MAX_LOOP_US="));
    Serial.println(maxLoopUs);

    Serial.print(F("LONG_LOOP_50MS="));
    Serial.println(longLoop50msCount);

    Serial.print(F("LONG_LOOP_250MS="));
    Serial.println(longLoop250msCount);

    const JWPLCModbusRTUStats &statusMb =
        JWPLC_ModbusRTU.stats();

    Serial.print(F("MODBUS_CRC_TOTAL="));
    Serial.println(statusMb.crcErrors);

    Serial.print(F("MODBUS_CRC_RUN="));
    Serial.println(
        statusMb.crcErrors >= modbusCrcBaseline
            ? statusMb.crcErrors - modbusCrcBaseline
            : statusMb.crcErrors);

    if (isMaster())
        printCommissioningTable();

    printRule();
}

static void forceResyncMaster()
{
    if (!isMaster())
        return;

    masterInitialRtcSyncSent = false;
    masterInitialRtcSyncRequested = true;
    lastInitialNtpAttemptMs = 0;
    initialNtpFailCount = 0;
    rtcSynced = false;
    soakState = SOAK_WAIT_RTC_SYNC;

    Serial.println(F("RTC RESYNC requested."));
}

static void handleSerialCommand(String line)
{
    line.trim();

    if (line.length() == 0)
        return;

    String upper = line;
    upper.toUpperCase();

    if (upper == "STATUS" || upper == "?")
    {
        printLocalStatus();
        return;
    }

    if (upper == "DIAG")
    {
        printRuntimeDiagnostics();
        return;
    }

    if (upper == "CLEAR")
    {
        clearPersistentConfig();
        return;
    }

    if (upper == "WIFI")
    {
        if (bleInitialized && !cfg.bleQualified)
        {
            Serial.println(F("WIFI deferred: complete BLE qualification first."));
            return;
        }

        if (wifiStaAttemptIssued && WiFi.status() != WL_CONNECTED)
        {
            WiFi.disconnect(false, false);
            delay(100);
            wifiStaAttemptIssued = false;
            wifiStaStartMs = 0;
        }

        startProvisioningAP();
        return;
    }

    if (upper == "START")
    {
        if (isMaster())
            startSoakMaster();
        else
            Serial.println(F("START solo se envia al Master."));
        return;
    }

    if (upper == "STOP")
    {
        if (isMaster())
            manualStopMaster();
        else
        {
            stopIoStress();
            soakState = SOAK_STOPPED;
        }
        return;
    }

    if (upper == "RESYNC")
    {
        forceResyncMaster();
        return;
    }

    if (upper == "ETHNEXT")
    {
        if (isMaster() && masterSoakRunning)
            advanceEthernetRotation();

        return;
    }

    if (upper.charAt(0) == 'M' ||
        upper.charAt(0) == 'S')
    {
        if (!parseAndSaveRole(upper))
        {
            Serial.println(F("Rol invalido. Ejemplos: M2, S1, S2"));
        }

        return;
    }

    Serial.println(F("Comando desconocido."));
    Serial.println(F("Usa M2/S1/S2..., STATUS, START, STOP, WIFI, RESYNC, ETHNEXT, CLEAR."));
}

static void serviceSerial()
{
    while (Serial.available() > 0)
    {
        const char c = (char)Serial.read();

        if (c == '\r' || c == '\n')
        {
            if (serialLine.length() > 0)
            {
                handleSerialCommand(serialLine);
                serialLine = "";
            }
        }
        else if (serialLine.length() < 64)
        {
            serialLine += c;
        }
    }
}

// ============================================================================
// Setup helpers
// ============================================================================

static void printBootInstructions()
{
    Serial.println();
    printRule();
    Serial.println(F("JWPLC DISTRIBUTED SOAK 2H - alpha.7"));
    Serial.println(F("GATE_RT_REV=3 7NB-async-master"));
    printRule();

    if (!cfg.roleValid)
    {
        Serial.println(F("ROLE NOT ASSIGNED."));
        Serial.println(F("Ejemplos:"));
        Serial.println(F("  M2  -> Master + 2 Slaves"));
        Serial.println(F("  S1  -> Slave ID 1"));
        Serial.println(F("  S2  -> Slave ID 2"));
        Serial.println();
        Serial.println(F("Envia el rol por Serial y el JWPLC reiniciara."));
    }
    else
    {
        Serial.print(F("NODE="));
        Serial.println(nodeName());

        Serial.print(F("BOOT_COUNT="));
        Serial.println(cfg.bootCount);

        Serial.println(F("Commissioning:"));
        Serial.println(F("  1) conectar BLE al nombre JWPLC_XX"));
        Serial.println(F("  2) provisionar WiFi en JWPLC_XX_SETUP si aparece"));
        Serial.println(F("  3) mantener PC HTTP server activo"));
        Serial.println(F("  4) formar RS-485"));
        Serial.println(F("  5) Ethernet inicialmente en Master"));
    }

    printRule();
}

// ============================================================================
// Main setup / loop
// ============================================================================

void setup()
{
    Serial.begin(SERIAL_BAUD);
    delay(1200);

    Serial.print(F("[CORE] Arduino setup/loop core="));
    Serial.println((int)xPortGetCoreID());

    pinMode(BUZZER_PIN, OUTPUT);
    noTone(BUZZER_PIN);

    // Estado seguro.
    digitalWriteBlock(Q0_X, 0x00);

    loadPersistentConfig();

    printBootInstructions();

    // Display / diagnosticos.
    JWPLC_Display.setRunLed(true);
    JWPLC_Display.setErrCode("");
    JWPLC_Display.setBusLedAuto(true);
    JWPLC_Display.setEthLedAuto(true);

    JWPLC_Display.setIdleWakeMode(
        IDLE_WAKE_ANY_BUTTON);

    JWPLC_Display.setIdleReturnMode(
        IDLE_RETURN_ESC_ONLY);

    JWPLC_Display.setUserRefreshPeriodMs(
        USER_REFRESH_MS);

    JWPLC_Display.goIdle();

    minFreeHeap = ESP.getFreeHeap();

    // Primer snapshot IO aun sin stress.
    ioStress.inputBitmap =
        digitalReadBlock(I0_X);

    // RTC existe dentro del autoload.
    rtcPresent = JWPLC_RTC.isPresent();

    uint32_t bootRtc = 0;

    if (rtcPresent && JWPLC_RTC.readUnix(bootRtc))
    {
        rtcEpoch = bootRtc;
        lastRtcEpochObserved = bootRtc;
    }
    else
    {
        // RTC fisicamente presente pero aun sin hora valida.
        // Es aceptable durante commissioning: NTP lo sincronizara.
        rtcEpoch = 0;
        lastRtcEpochObserved = 0;
    }

    // FRAM es obligatoria.
    if (!prepareFRAM())
        latchError(ERR_FRAM, "FRAM precheck failed");

    if (!cfg.roleValid)
    {
        soakState = SOAK_NEED_ROLE;
        return;
    }

    soakState = SOAK_COMMISSIONING;

    // ------------------------------------------------------------------------
    // Alpha7 - commissioning de radios en fases separadas.
    //
    // Fase A:
    //   bleQualified == false
    //   -> solo BLE
    //   -> una conexion real guarda BLE PASS
    //   -> reboot controlado
    //
    // Fase B:
    //   bleQualified == true
    //   -> NO se inicia Bluedroid
    //   -> WiFi SoftAP/STA + HTTP
    //
    // Esto evita el fallo observado al intentar mantener SoftAP (~75 KB heap
    // libre) y luego inicializar Bluedroid en el mismo boot.
    // ------------------------------------------------------------------------

    if (!cfg.bleQualified)
    {
        Serial.print(F("[HEAP] before BLE = "));
        Serial.println(ESP.getFreeHeap());

        startBLE();

        Serial.print(F("[HEAP] after BLE = "));
        Serial.println(ESP.getFreeHeap());

        Serial.println(F("[RADIO] phase=BLE_QUALIFICATION"));
        Serial.println(F("[RADIO] WiFi will start automatically after BLE PASS reboot."));
    }
    else
    {
        Serial.println(F("[BLE] qualification already PASS; stack not started this boot"));

        Serial.print(F("[HEAP] before WIFI = "));
        Serial.println(ESP.getFreeHeap());

        if (cfg.wifiValid)
            startWifiSTA();
        else
            startProvisioningAP();

        Serial.print(F("[HEAP] after WIFI = "));
        Serial.println(ESP.getFreeHeap());

        if (!startWifiTelemetryWorker())
        {
            Serial.println(F("[WIFI WORKER] unavailable; WIFI HTTP gate cannot run"));
        }
        else
        {
            // Dar oportunidad al worker de arrancar y registrar su core.
            delay(10);

            Serial.print(F("[CORE] WiFi HTTP worker observed core="));
            Serial.println((int)wifiWorkerObservedCore);
        }

        Serial.print(F("[HEAP] after WIFI worker = "));
        Serial.println(ESP.getFreeHeap());

        Serial.println(F("[RADIO] phase=WIFI_QUALIFICATION"));
    }

    beginModbus();

    if (isMaster())
        (void)initMasterSD();

    refreshHoldingRegisters();

    Serial.println(F("BOOT=PASS"));
}

void loop()
{
    loopStartUs = micros();

    serviceSerial();
    serviceBleQualificationRestart();

    // Orden importante:
    //  - IO/RTC cortos primero.
    //  - task() Modbus con alta frecuencia para cualquier rol.
    //  - Red/FRAM/SD despues.
    serviceIO();
    serviceRTC();

    if (modbusReady)
    {
        if (isSlave())
            refreshHoldingRegisters();

        JWPLC_ModbusRTU.task();
        serviceModbusDiagnostics();
    }

    if (isSlave() && modbusReady)
    {
        processSlaveCommand();

        // HR_ETH_OWNER es escrito directamente por Master.
        if (holdingRegisters[HR_ETH_OWNER] != ethWindow.owner)
        {
            const uint16_t previousOwner =
                ethWindow.owner;

            ethWindow.owner =
                holdingRegisters[HR_ETH_OWNER];

            if (previousOwner == localNodeOrdinal() &&
                ethWindow.owner != localNodeOrdinal())
            {
                finalizeLocalEthernetWindow();
            }
        }
    }

    if (isMaster())
    {
        refreshHoldingRegisters();

        if (soakState == SOAK_RUNNING)
            masterPollOneSlaveCooperative();
        else
            masterPollOneSlaveSyncCommissioning();

        serviceMasterCommissioning();
    }

    serviceWiFi();

    {
        const uint32_t opUs = micros();
        serviceFRAM();
        const uint32_t elapsedUs = micros() - opUs;

        if (elapsedUs > framServiceMaxUs)
            framServiceMaxUs = elapsedUs;
    }

    {
        const uint32_t opUs = micros();
        serviceLocalEthernetWindow();
        const uint32_t elapsedUs = micros() - opUs;

        if (elapsedUs > ethernetServiceMaxUs)
            ethernetServiceMaxUs = elapsedUs;
    }

    if (isMaster())
    {
        serviceMasterEthernetRotation();

        const uint32_t opUs = micros();
        serviceMasterSD();
        const uint32_t elapsedUs = micros() - opUs;

        if (elapsedUs > sdServiceMaxUs)
            sdServiceMaxUs = elapsedUs;
    }

    refreshHoldingRegisters();
    servicePeriodicRuntimeDiagnostics();

    // Telemetria del loop/runtime.
    const uint32_t freeHeap =
        ESP.getFreeHeap();

    if (freeHeap < minFreeHeap)
        minFreeHeap = freeHeap;

    const uint32_t loopUs =
        micros() - loopStartUs;

    lastLoopUs = loopUs;

    if (loopUs > maxLoopUs)
        maxLoopUs = loopUs;

    if (loopUs >= LONG_LOOP_WARN_US)
        longLoop50msCount++;

    if (loopUs >= LONG_LOOP_CRIT_US)
        longLoop250msCount++;

    delay(1);
}
