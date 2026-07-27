/*
  Ethernet_Continuous_Stress_TFT

  Prueba continua y diagnostico por capas del puerto Ethernet del JWPLC Basic.

  Mejoras v3:
  - evita redibujar filas TFT cuyo texto y color no cambiaron;
  - diferencia LINK desconocido de LINK OFF durante el arranque;
  - espera una ventana de estabilizacion antes de reportar la primera falla;
  - usa intervalos medidos de inicio a inicio, incluido modo continuo;
  - reutiliza la IP DNS y refresca DNS periodicamente para exigir TCP/HTTP;
  - conserva las pruebas auxiliares contra IP cacheada y referencia LAN.

  Controles:
  LEFT/RIGHT = pagina | OK = pausa/reanuda | UP/DOWN = intervalo | ESC = ACK
*/

#include <JWPLC_Display.h>
#include <JWPLC_GlobalPeripherals.h>
#include <Ethernet.h>
#include <Dns.h>
#include "jwplc_spi_bus.h"

#include <stdlib.h>
#include <string.h>

// -----------------------------------------------------------------------------
// Destino principal: prueba DNS + TCP + HTTP + contenido.
// Para pruebas prolongadas y agresivas se recomienda un servidor HTTP local.
// -----------------------------------------------------------------------------
static const char STRESS_HOST[] = "example.com";
static const char STRESS_PATH[] = "/";
static const char EXPECTED_TOKEN[] = "Example Domain";
static const uint16_t STRESS_PORT = 80;

// -----------------------------------------------------------------------------
// Referencia LAN opcional.
// -----------------------------------------------------------------------------
static const bool ENABLE_LOCAL_REFERENCE = false;
static IPAddress LOCAL_REFERENCE_IP(192, 168, 0, 4);
static const uint16_t LOCAL_REFERENCE_PORT = 8080;

// -----------------------------------------------------------------------------
// Politica de arranque y estres.
// -----------------------------------------------------------------------------
static const uint32_t STARTUP_QUALIFY_TIMEOUT_MS = 20000UL;
static const uint32_t STARTUP_STABLE_MS = 750UL;
static const uint32_t STATUS_POLL_MS = 50UL;
static const uint32_t SERIAL_LOG_MS = 5000UL;
static const uint16_t DNS_STEP_TIMEOUT_MS = 1500;
static const uint16_t TCP_CONNECT_TIMEOUT_MS = 1500;
static const uint32_t RESPONSE_TIMEOUT_MS = 5000UL;
static const uint8_t DNS_REFRESH_EVERY_TESTS = 20;
static const size_t RESPONSE_BUFFER_SIZE = 1536;

// 0 ms = continuo. El intervalo se mide de inicio a inicio.
static const uint32_t TEST_INTERVALS_MS[] = {
    0UL, 50UL, 100UL, 250UL, 500UL, 1000UL, 2000UL, 5000UL, 10000UL};
static const uint8_t TEST_INTERVAL_COUNT =
    sizeof(TEST_INTERVALS_MS) / sizeof(TEST_INTERVALS_MS[0]);
static const uint8_t DEFAULT_INTERVAL_INDEX = 4; // 500 ms; DOWN acelera.

static const uint8_t PAGE_COUNT = 4;
static const uint8_t UI_ROWS_PER_PAGE = 7;

// -----------------------------------------------------------------------------
// Tipos de diagnostico.
// -----------------------------------------------------------------------------
enum StressError : uint8_t
{
  STRESS_OK = 0,
  STRESS_ERR_ETH_DISABLED,
  STRESS_ERR_NOT_READY,
  STRESS_ERR_SPI_LOCK,
  STRESS_ERR_NO_HARDWARE,
  STRESS_ERR_LINK_DOWN,
  STRESS_ERR_DHCP_IP,
  STRESS_ERR_DNS_TIMEOUT,
  STRESS_ERR_DNS_SERVER,
  STRESS_ERR_DNS_RESPONSE,
  STRESS_ERR_DNS_OTHER,
  STRESS_ERR_TCP_CONNECT,
  STRESS_ERR_NO_RESPONSE,
  STRESS_ERR_RX_TIMEOUT,
  STRESS_ERR_BAD_STATUS,
  STRESS_ERR_HTTP_CODE,
  STRESS_ERR_CONTENT
};

struct StressStats
{
  uint32_t tests = 0;
  uint32_t ok = 0;
  uint32_t failed = 0;
  uint32_t failStreak = 0;
  uint32_t maxFailStreak = 0;

  uint32_t spi = 0;
  uint32_t noHardware = 0;
  uint32_t link = 0;
  uint32_t dhcpIp = 0;
  uint32_t notReady = 0;

  uint32_t dnsTimeout = 0;
  uint32_t dnsServer = 0;
  uint32_t dnsResponse = 0;
  uint32_t dnsOther = 0;
  uint32_t tcpConnect = 0;
  uint32_t noResponse = 0;
  uint32_t timeout = 0;
  uint32_t badStatus = 0;
  uint32_t httpCode = 0;
  uint32_t content = 0;

  uint32_t cachedProbeOk = 0;
  uint32_t cachedProbeFail = 0;
  uint32_t localProbeOk = 0;
  uint32_t localProbeFail = 0;

  uint32_t hardwareLoss = 0;
  uint32_t hardwareRecovery = 0;
  uint32_t linkDrop = 0;
  uint32_t linkRecovery = 0;

  uint64_t totalBytes = 0;
  uint64_t successfulLatencyTotal = 0;
  uint32_t minLatency = 0;
  uint32_t maxLatency = 0;
  uint32_t lastBytes = 0;
  uint16_t lastCode = 0;
};

struct UiRowCache
{
  bool valid = false;
  uint16_t color = 0;
  char label[24] = {};
  char value[96] = {};
};

// -----------------------------------------------------------------------------
// Estado global.
// -----------------------------------------------------------------------------
StressStats stats;
UiRowCache uiRows[PAGE_COUNT][UI_ROWS_PER_PAGE];
EthernetClient stressClient;
EthernetClient probeClient;
DNSClient dnsClient;

bool displayReady = false;
bool running = true;
bool busy = false;
bool alarmLatched = false;
bool errLedState = false;

bool stateValid = false;
bool transitionsArmed = false;
bool hardwarePresent = false;
bool linkKnown = false;
bool linkOn = false;
bool ipValid = false;
EthernetHardwareStatus hardwareState = EthernetNoHardware;
EthernetLinkStatus rawLinkState = Unknown;

bool startupQualified = false;
bool startupTimeoutExpired = false;
uint32_t startupReadySinceMs = 0;

uint8_t page = 0;
uint8_t intervalIndex = DEFAULT_INTERVAL_INDEX;
StressError currentError = STRESS_OK;
StressError lastError = STRESS_OK;

char hardwareText[16] = "INICIANDO";
char linkText[12] = "LEYENDO";
char ipText[20] = "0.0.0.0";
char dnsServerText[20] = "0.0.0.0";
char resolvedIpText[20] = "0.0.0.0";
char currentResult[64] = "ARRANQUE: ESPERANDO ETHERNET";
char lastErrorName[28] = "NINGUNO";
char lastErrorDetail[192] = "Sin fallas registradas";
char lastErrorTime[24] = "-";
char lastStatusLine[64] = "-";
char likelySource[48] = "SIN FALLAS";
char lastLikelySource[48] = "SIN FALLAS";
char cachedProbeText[32] = "NO EJECUTADA";
char localProbeText[32] = "DESHABILITADA";

IPAddress lastResolvedIp(0, 0, 0, 0);
IPAddress cachedResolvedIp(0, 0, 0, 0);
bool cachedResolvedValid = false;
uint32_t lastDnsRefreshTest = 0;

int lastDnsResult = 0;
bool lastDnsUsedCache = false;
bool lastDnsOk = false;
bool lastTcpOk = false;
bool lastHttpOk = false;
bool lastCachedProbeAttempted = false;
bool lastCachedProbeOk = false;
bool lastLocalProbeAttempted = false;
bool lastLocalProbeOk = false;
uint32_t lastDnsMs = 0;
uint32_t lastTcpMs = 0;
uint32_t lastFirstByteMs = 0;
uint32_t lastHttpMs = 0;
uint32_t lastCachedProbeMs = 0;
uint32_t lastLocalProbeMs = 0;

uint32_t bootMs = 0;
uint32_t lastTestStartedMs = 0;
uint32_t lastPollMs = 0;
uint32_t lastLogMs = 0;
uint32_t lastUiSecondMs = 0;
uint32_t lastSuccessMs = 0;
uint32_t lastErrorUptimeMs = 0;

portMUX_TYPE uiMux = portMUX_INITIALIZER_UNLOCKED;
volatile bool uiFrameDirty = true;
volatile bool uiContentDirty = true;

// -----------------------------------------------------------------------------
// Helpers de texto y UI.
// -----------------------------------------------------------------------------
void requestUi(bool frame = false)
{
  portENTER_CRITICAL(&uiMux);
  uiContentDirty = true;
  if (frame)
    uiFrameDirty = true;
  portEXIT_CRITICAL(&uiMux);
}

void takeUi(bool &frame, bool &content)
{
  portENTER_CRITICAL(&uiMux);
  frame = uiFrameDirty;
  content = uiContentDirty;
  uiFrameDirty = false;
  uiContentDirty = false;
  portEXIT_CRITICAL(&uiMux);
}

void copyText(char *dst, size_t size, const char *src)
{
  if (!dst || size == 0)
    return;
  if (!src)
    src = "";
  strncpy(dst, src, size - 1);
  dst[size - 1] = '\0';
}

bool setTextIfChanged(char *dst, size_t size, const char *src)
{
  if (!src)
    src = "";
  if (strncmp(dst, src, size) == 0)
    return false;
  copyText(dst, size, src);
  return true;
}

void appendText(char *dst, size_t size, const char *src)
{
  if (!dst || !src || size == 0)
    return;
  size_t used = strlen(dst);
  if (used >= size - 1)
    return;
  strncat(dst, src, size - used - 1);
}

bool isValidIP(IPAddress ip)
{
  return !(ip == IPAddress(0, 0, 0, 0)) &&
         !(ip == IPAddress(255, 255, 255, 255));
}

void formatIP(IPAddress ip, char *out, size_t size)
{
  snprintf(out, size, "%u.%u.%u.%u", ip[0], ip[1], ip[2], ip[3]);
}

void formatDuration(uint32_t ms, char *out, size_t size)
{
  uint32_t sec = ms / 1000UL;
  snprintf(out, size, "%luh %02lum %02lus",
           (unsigned long)(sec / 3600UL),
           (unsigned long)((sec % 3600UL) / 60UL),
           (unsigned long)(sec % 60UL));
}

void formatInterval(char *out, size_t size)
{
  uint32_t interval = TEST_INTERVALS_MS[intervalIndex];
  if (interval == 0)
    copyText(out, size, "CONT");
  else
    snprintf(out, size, "%lums", (unsigned long)interval);
}

void invalidatePageCache(uint8_t pageIndex)
{
  if (pageIndex >= PAGE_COUNT)
    return;
  for (uint8_t row = 0; row < UI_ROWS_PER_PAGE; ++row)
    uiRows[pageIndex][row].valid = false;
}

void invalidateAllUiCache()
{
  for (uint8_t p = 0; p < PAGE_COUNT; ++p)
    invalidatePageCache(p);
}

void clearRowArea(Adafruit_ST7789 &tft, int16_t y, int16_t height = 13)
{
  tft.fillRect(0, y - 2, 320, height, ST77XX_BLACK);
}

void drawCachedRow(Adafruit_ST7789 &tft,
                   uint8_t rowIndex,
                   int16_t y,
                   const char *label,
                   const char *value,
                   uint16_t color = ST77XX_WHITE,
                   int16_t height = 13)
{
  if (page >= PAGE_COUNT || rowIndex >= UI_ROWS_PER_PAGE)
    return;
  if (!label)
    label = "";
  if (!value)
    value = "";

  UiRowCache &cache = uiRows[page][rowIndex];
  bool unchanged = cache.valid &&
                   cache.color == color &&
                   strcmp(cache.label, label) == 0 &&
                   strcmp(cache.value, value) == 0;
  if (unchanged)
    return;

  copyText(cache.label, sizeof(cache.label), label);
  copyText(cache.value, sizeof(cache.value), value);
  cache.color = color;
  cache.valid = true;

  clearRowArea(tft, y, height);
  tft.setTextSize(1);
  tft.setCursor(6, y);
  tft.setTextColor(ST77XX_CYAN, ST77XX_BLACK);
  tft.print(label);
  tft.setTextColor(color, ST77XX_BLACK);
  tft.print(value);
}

void splitText3(const char *source,
                char *line1, size_t size1,
                char *line2, size_t size2,
                char *line3, size_t size3)
{
  if (!source)
    source = "";
  size_t len = strlen(source);
  size_t p1 = len < size1 - 1 ? len : size1 - 1;
  memcpy(line1, source, p1);
  line1[p1] = '\0';

  size_t offset = p1;
  if (offset < len)
  {
    size_t remain = len - offset;
    size_t p2 = remain < size2 - 1 ? remain : size2 - 1;
    memcpy(line2, source + offset, p2);
    line2[p2] = '\0';
    offset += p2;
  }
  else
    line2[0] = '\0';

  if (offset < len)
    copyText(line3, size3, source + offset);
  else
    line3[0] = '\0';
}

// -----------------------------------------------------------------------------
// Nombres y contadores.
// -----------------------------------------------------------------------------
const char *errorName(StressError error)
{
  switch (error)
  {
  case STRESS_OK:
    return "NINGUNO";
  case STRESS_ERR_ETH_DISABLED:
    return "ETH DESHABILITADO";
  case STRESS_ERR_NOT_READY:
    return "ETH NO LISTO";
  case STRESS_ERR_SPI_LOCK:
    return "SPI LOCK";
  case STRESS_ERR_NO_HARDWARE:
    return "W5500 NO DETECTADO";
  case STRESS_ERR_LINK_DOWN:
    return "LINK OFF";
  case STRESS_ERR_DHCP_IP:
    return "DHCP/IP";
  case STRESS_ERR_DNS_TIMEOUT:
    return "DNS TIMEOUT";
  case STRESS_ERR_DNS_SERVER:
    return "DNS SERVIDOR";
  case STRESS_ERR_DNS_RESPONSE:
    return "DNS RESPUESTA";
  case STRESS_ERR_DNS_OTHER:
    return "DNS OTRO";
  case STRESS_ERR_TCP_CONNECT:
    return "TCP CONNECT";
  case STRESS_ERR_NO_RESPONSE:
    return "SIN RESPUESTA";
  case STRESS_ERR_RX_TIMEOUT:
    return "TIMEOUT RX";
  case STRESS_ERR_BAD_STATUS:
    return "HTTP INVALIDO";
  case STRESS_ERR_HTTP_CODE:
    return "CODIGO HTTP";
  case STRESS_ERR_CONTENT:
    return "CONTENIDO";
  default:
    return "DESCONOCIDO";
  }
}

const char *dnsResultName(int result)
{
  switch (result)
  {
  case 1:
    return "OK";
  case -1:
    return "TIMEOUT";
  case -2:
    return "SERVIDOR INVALIDO";
  case -3:
    return "TRUNCADA";
  case -4:
    return "RESPUESTA INVALIDA";
  case -5:
    return "SERVIDOR RECHAZO";
  case 0:
    return "SIN SOCKET/ENVIO";
  default:
    return "OTRO";
  }
}

StressError dnsErrorFromResult(int result)
{
  if (result == -1)
    return STRESS_ERR_DNS_TIMEOUT;
  if (result == -2)
    return STRESS_ERR_DNS_SERVER;
  if (result == -3 || result == -4 || result == -5)
    return STRESS_ERR_DNS_RESPONSE;
  return STRESS_ERR_DNS_OTHER;
}

const char *hardwareName(EthernetHardwareStatus status)
{
  switch (status)
  {
  case EthernetW5100:
    return "W5100";
  case EthernetW5200:
    return "W5200";
  case EthernetW5500:
    return "W5500";
  default:
    return "NO HW";
  }
}

void incrementError(StressError error)
{
  switch (error)
  {
  case STRESS_ERR_SPI_LOCK:
    stats.spi++;
    break;
  case STRESS_ERR_NO_HARDWARE:
    stats.noHardware++;
    break;
  case STRESS_ERR_LINK_DOWN:
    stats.link++;
    break;
  case STRESS_ERR_DHCP_IP:
    stats.dhcpIp++;
    break;
  case STRESS_ERR_ETH_DISABLED:
  case STRESS_ERR_NOT_READY:
    stats.notReady++;
    break;
  case STRESS_ERR_DNS_TIMEOUT:
    stats.dnsTimeout++;
    break;
  case STRESS_ERR_DNS_SERVER:
    stats.dnsServer++;
    break;
  case STRESS_ERR_DNS_RESPONSE:
    stats.dnsResponse++;
    break;
  case STRESS_ERR_DNS_OTHER:
    stats.dnsOther++;
    break;
  case STRESS_ERR_TCP_CONNECT:
    stats.tcpConnect++;
    break;
  case STRESS_ERR_NO_RESPONSE:
    stats.noResponse++;
    break;
  case STRESS_ERR_RX_TIMEOUT:
    stats.timeout++;
    break;
  case STRESS_ERR_BAD_STATUS:
    stats.badStatus++;
    break;
  case STRESS_ERR_HTTP_CODE:
    stats.httpCode++;
    break;
  case STRESS_ERR_CONTENT:
    stats.content++;
    break;
  default:
    break;
  }
}

// -----------------------------------------------------------------------------
// SPI, alarma y fecha.
// -----------------------------------------------------------------------------
bool lockEthernet(uint32_t timeoutMs)
{
  if (!jwplcSPI_acquire(timeoutMs))
    return false;
  jwplcSPI_deselectAll();
  return true;
}

void unlockEthernet()
{
  jwplcSPI_release();
}

void updateErrLed()
{
  bool newState = alarmLatched || currentError != STRESS_OK;
  if (newState != errLedState)
  {
    errLedState = newState;
    JWPLC_Display.setErrLed(errLedState);
    requestUi();
  }
}

void formatLastErrorTime()
{
  JWRTCDateTime now = JWPLC_RTC.now();
  if (now.valid)
  {
    snprintf(lastErrorTime, sizeof(lastErrorTime),
             "%04u-%02u-%02u %02u:%02u:%02u",
             now.year, now.month, now.day,
             now.hour, now.minute, now.second);
  }
  else
  {
    char uptime[20] = {};
    formatDuration(lastErrorUptimeMs, uptime, sizeof(uptime));
    snprintf(lastErrorTime, sizeof(lastErrorTime), "T+%s", uptime);
  }
}

void rememberError(StressError error, const char *detail)
{
  currentError = error;
  lastError = error;
  alarmLatched = true;
  copyText(lastErrorName, sizeof(lastErrorName), errorName(error));
  copyText(lastErrorDetail, sizeof(lastErrorDetail), detail);
  copyText(lastLikelySource, sizeof(lastLikelySource), likelySource);
  lastErrorUptimeMs = millis() - bootMs;
  formatLastErrorTime();
  snprintf(currentResult, sizeof(currentResult), "ERROR: %s", errorName(error));
  updateErrLed();
  requestUi();
}

void recordFailure(StressError error, const char *detail)
{
  stats.failed++;
  stats.failStreak++;
  if (stats.failStreak > stats.maxFailStreak)
    stats.maxFailStreak = stats.failStreak;
  incrementError(error);
  rememberError(error, detail);

  Serial.print("[ETH-STRESS][ERROR] #");
  Serial.print(stats.tests);
  Serial.print(" ");
  Serial.print(errorName(error));
  Serial.print(" | ");
  Serial.println(detail);
}

void recordObservedFault(StressError error, const char *detail)
{
  incrementError(error);
  rememberError(error, detail);
  Serial.print("[ETH-STRESS][EVENTO] ");
  Serial.print(errorName(error));
  Serial.print(" | ");
  Serial.println(detail);
}

void recordSuccess(uint16_t code, uint32_t bytes)
{
  currentError = STRESS_OK;
  lastHttpOk = true;
  stats.ok++;
  stats.failStreak = 0;
  stats.successfulLatencyTotal += lastHttpMs;
  if (stats.minLatency == 0 || lastHttpMs < stats.minLatency)
    stats.minLatency = lastHttpMs;
  if (lastHttpMs > stats.maxLatency)
    stats.maxLatency = lastHttpMs;
  lastSuccessMs = millis();
  copyText(likelySource, sizeof(likelySource), "SIN FALLAS");

  snprintf(currentResult, sizeof(currentResult),
           "OK HTTP %u | %lums | %luB",
           code, (unsigned long)lastHttpMs, (unsigned long)bytes);
  updateErrLed();
  requestUi();

  Serial.print("[ETH-STRESS][OK] #");
  Serial.print(stats.tests);
  Serial.print(" DNS ");
  Serial.print(lastDnsUsedCache ? "CACHE" : "LIVE");
  Serial.print("/");
  Serial.print(lastDnsMs);
  Serial.print("ms -> ");
  Serial.print(resolvedIpText);
  Serial.print(" | TCP ");
  Serial.print(lastTcpMs);
  Serial.print("ms | 1B ");
  Serial.print(lastFirstByteMs);
  Serial.print("ms | TOTAL ");
  Serial.print(lastHttpMs);
  Serial.print("ms | HTTP ");
  Serial.print(code);
  Serial.print(" | ");
  Serial.print(bytes);
  Serial.println(" bytes");
}

void acknowledgeAlarm()
{
  if (currentError == STRESS_OK)
    alarmLatched = false;
  updateErrLed();
  requestUi();
  Serial.println(currentError == STRESS_OK
                     ? "[ETH-STRESS] Alarma reconocida"
                     : "[ETH-STRESS] La falla sigue activa");
}

// -----------------------------------------------------------------------------
// Muestreo fisico y cualificacion de arranque.
// -----------------------------------------------------------------------------
bool sampleEthernet(bool countTransitions)
{
  if (!JWPLC_Ethernet.isEnabled())
  {
    hardwarePresent = false;
    linkKnown = false;
    linkOn = false;
    ipValid = false;
    setTextIfChanged(hardwareText, sizeof(hardwareText), "DESHABILITADO");
    setTextIfChanged(linkText, sizeof(linkText), "?");
    setTextIfChanged(ipText, sizeof(ipText), "0.0.0.0");
    requestUi();
    return true;
  }

  if (!lockEthernet(250))
    return false;

  EthernetHardwareStatus newHardware = Ethernet.hardwareStatus();
  EthernetLinkStatus newLink = Unknown;
  IPAddress newIp(0, 0, 0, 0);
  if (newHardware != EthernetNoHardware)
  {
    newLink = Ethernet.linkStatus();
    newIp = Ethernet.localIP();
  }
  unlockEthernet();

  bool oldHardwarePresent = hardwarePresent;
  bool oldLinkKnown = linkKnown;
  bool oldLinkOn = linkOn;

  bool newHardwarePresent = newHardware != EthernetNoHardware;
  bool newLinkKnown = newLink != Unknown;
  bool newLinkOn = newLink == LinkON;
  bool newIpValid = isValidIP(newIp);

  if (stateValid && countTransitions)
  {
    if (oldHardwarePresent && !newHardwarePresent)
    {
      stats.hardwareLoss++;
      recordObservedFault(
          STRESS_ERR_NO_HARDWARE,
          "W5500 desaparecio: revisar alimentacion, SPI, CS y soldadura");
    }
    else if (!oldHardwarePresent && newHardwarePresent)
    {
      stats.hardwareRecovery++;
    }

    if (oldHardwarePresent && newHardwarePresent && oldLinkKnown && newLinkKnown)
    {
      if (oldLinkOn && !newLinkOn)
      {
        stats.linkDrop++;
        recordObservedFault(
            STRESS_ERR_LINK_DOWN,
            "Caida de link: revisar RJ45, magneticos, pares, cable y soldadura");
      }
      else if (!oldLinkOn && newLinkOn)
      {
        stats.linkRecovery++;
      }
    }
  }

  char newIpText[20] = {};
  formatIP(newIp, newIpText, sizeof(newIpText));
  const char *newLinkText = !newLinkKnown ? "?" : (newLinkOn ? "ON" : "OFF");

  bool changed = !stateValid ||
                 hardwareState != newHardware ||
                 rawLinkState != newLink ||
                 hardwarePresent != newHardwarePresent ||
                 linkKnown != newLinkKnown ||
                 linkOn != newLinkOn ||
                 ipValid != newIpValid ||
                 strcmp(ipText, newIpText) != 0;

  hardwareState = newHardware;
  rawLinkState = newLink;
  hardwarePresent = newHardwarePresent;
  linkKnown = newLinkKnown;
  linkOn = newLinkOn;
  ipValid = newIpValid;
  stateValid = true;

  setTextIfChanged(hardwareText, sizeof(hardwareText), hardwareName(newHardware));
  setTextIfChanged(linkText, sizeof(linkText), newLinkText);
  setTextIfChanged(ipText, sizeof(ipText), newIpText);

  if (changed)
    requestUi();
  return true;
}

const char *startupWaitReason()
{
  if (!JWPLC_Ethernet.isEnabled())
    return "ARRANQUE: ETH DESHABILITADO";
  if (!hardwarePresent)
    return "ARRANQUE: ESPERANDO W5500";
  if (!linkKnown)
    return "ARRANQUE: LEYENDO LINK";
  if (!linkOn)
    return "ARRANQUE: ESPERANDO LINK ON";
  if (!JWPLC_Ethernet.isReady())
    return "ARRANQUE: ESPERANDO RUNTIME";
  if (!ipValid)
    return "ARRANQUE: ESPERANDO DHCP/IP";
  return "ARRANQUE: ESTABILIZANDO";
}

void updateStartupQualification(uint32_t now)
{
  if (startupQualified)
    return;

  bool readyNow = JWPLC_Ethernet.isEnabled() &&
                  hardwarePresent &&
                  linkKnown &&
                  linkOn &&
                  JWPLC_Ethernet.isReady() &&
                  ipValid;

  if (readyNow)
  {
    if (startupReadySinceMs == 0)
      startupReadySinceMs = now;

    if ((uint32_t)(now - startupReadySinceMs) >= STARTUP_STABLE_MS)
    {
      startupQualified = true;
      transitionsArmed = true;
      setTextIfChanged(currentResult, sizeof(currentResult),
                       "ARRANQUE OK: INICIANDO ESTRES");
      requestUi();
      Serial.println("[ETH-STRESS] Arranque Ethernet cualificado");
      return;
    }
  }
  else
  {
    startupReadySinceMs = 0;
  }

  startupTimeoutExpired =
      (uint32_t)(now - bootMs) >= STARTUP_QUALIFY_TIMEOUT_MS;
  if (!startupTimeoutExpired)
  {
    if (setTextIfChanged(currentResult, sizeof(currentResult), startupWaitReason()))
      requestUi();
  }
  else
  {
    if (setTextIfChanged(currentResult, sizeof(currentResult),
                         "ARRANQUE: TIMEOUT, DIAGNOSTICANDO"))
      requestUi();
  }
}

StressError checkPrerequisites(char *detail, size_t detailSize)
{
  if (!JWPLC_Ethernet.isEnabled())
  {
    copyText(detail, detailSize, "Ethernet no esta habilitado en esta variante");
    return STRESS_ERR_ETH_DISABLED;
  }
  if (!sampleEthernet(transitionsArmed))
  {
    copyText(detail, detailSize, "No se pudo adquirir el bus SPI compartido");
    return STRESS_ERR_SPI_LOCK;
  }
  if (!hardwarePresent)
  {
    copyText(detail, detailSize,
             "W5500 ausente: revisar alimentacion, SPI, CS y soldadura");
    return STRESS_ERR_NO_HARDWARE;
  }
  if (!linkKnown)
  {
    copyText(detail, detailSize,
             "Estado LINK aun desconocido; PHY no respondio de forma valida");
    return STRESS_ERR_NOT_READY;
  }
  if (!linkOn)
  {
    copyText(detail, detailSize,
             "PHY/RJ45 sin enlace: revisar cable, magneticos y pares");
    return STRESS_ERR_LINK_DOWN;
  }
  if (!JWPLC_Ethernet.isReady())
  {
    JWPLCEthernetError error = JWPLC_Ethernet.lastError();
    if (error == JWPLC_ETH_DHCP_FAILED || error == JWPLC_ETH_INVALID_IP)
    {
      copyText(detail, detailSize, JWPLC_Ethernet.lastErrorString());
      return STRESS_ERR_DHCP_IP;
    }
    if (error == JWPLC_ETH_BUS_LOCK_TIMEOUT)
    {
      copyText(detail, detailSize, "Runtime Ethernet no obtuvo el mutex SPI");
      return STRESS_ERR_SPI_LOCK;
    }
    copyText(detail, detailSize, JWPLC_Ethernet.lastErrorString());
    return STRESS_ERR_NOT_READY;
  }
  if (!ipValid)
  {
    copyText(detail, detailSize, "IP 0.0.0.0: DHCP o red invalida");
    return STRESS_ERR_DHCP_IP;
  }
  return STRESS_OK;
}

// -----------------------------------------------------------------------------
// DNS, TCP y HTTP.
// -----------------------------------------------------------------------------
bool probeTcp(IPAddress ip, uint16_t port, uint32_t &latency)
{
  latency = 0;
  if (!isValidIP(ip))
    return false;
  if (!lockEthernet(1000))
    return false;

  probeClient.stop();
  probeClient.setConnectionTimeout(TCP_CONNECT_TIMEOUT_MS);
  uint32_t started = millis();
  bool connected = probeClient.connect(ip, port) == 1;
  latency = millis() - started;
  probeClient.stop();
  unlockEthernet();
  return connected;
}

void runAuxiliaryProbes(StressError primaryError)
{
  lastCachedProbeAttempted = false;
  lastCachedProbeOk = false;
  lastCachedProbeMs = 0;
  copyText(cachedProbeText, sizeof(cachedProbeText), "NO EJECUTADA");

  bool dnsError = primaryError == STRESS_ERR_DNS_TIMEOUT ||
                  primaryError == STRESS_ERR_DNS_SERVER ||
                  primaryError == STRESS_ERR_DNS_RESPONSE ||
                  primaryError == STRESS_ERR_DNS_OTHER;

  if (dnsError && cachedResolvedValid)
  {
    lastCachedProbeAttempted = true;
    lastCachedProbeOk = probeTcp(cachedResolvedIp, STRESS_PORT, lastCachedProbeMs);
    if (lastCachedProbeOk)
    {
      stats.cachedProbeOk++;
      snprintf(cachedProbeText, sizeof(cachedProbeText),
               "TCP CACHE OK %lums", (unsigned long)lastCachedProbeMs);
    }
    else
    {
      stats.cachedProbeFail++;
      snprintf(cachedProbeText, sizeof(cachedProbeText),
               "TCP CACHE FAIL %lums", (unsigned long)lastCachedProbeMs);
    }
  }

  lastLocalProbeAttempted = false;
  lastLocalProbeOk = false;
  lastLocalProbeMs = 0;
  copyText(localProbeText, sizeof(localProbeText),
           ENABLE_LOCAL_REFERENCE ? "PENDIENTE" : "DESHABILITADA");

  if (ENABLE_LOCAL_REFERENCE)
  {
    lastLocalProbeAttempted = true;
    lastLocalProbeOk = probeTcp(
        LOCAL_REFERENCE_IP, LOCAL_REFERENCE_PORT, lastLocalProbeMs);
    if (lastLocalProbeOk)
    {
      stats.localProbeOk++;
      snprintf(localProbeText, sizeof(localProbeText),
               "LAN OK %lums", (unsigned long)lastLocalProbeMs);
    }
    else
    {
      stats.localProbeFail++;
      snprintf(localProbeText, sizeof(localProbeText),
               "LAN FAIL %lums", (unsigned long)lastLocalProbeMs);
    }
  }
}

void classifyLikelySource(StressError error)
{
  if (error == STRESS_ERR_SPI_LOCK || error == STRESS_ERR_NO_HARDWARE)
  {
    copyText(likelySource, sizeof(likelySource),
             "POSIBLE SPI/W5500/SOLDADURA");
    return;
  }
  if (error == STRESS_ERR_LINK_DOWN)
  {
    copyText(likelySource, sizeof(likelySource),
             "POSIBLE RJ45/CABLE/MAGNETICOS");
    return;
  }
  if (error == STRESS_ERR_DHCP_IP || error == STRESS_ERR_NOT_READY)
  {
    copyText(likelySource, sizeof(likelySource),
             "RED LOCAL/RUNTIME; REVISAR");
    return;
  }

  bool dnsError = error == STRESS_ERR_DNS_TIMEOUT ||
                  error == STRESS_ERR_DNS_SERVER ||
                  error == STRESS_ERR_DNS_RESPONSE ||
                  error == STRESS_ERR_DNS_OTHER;

  if (dnsError && lastCachedProbeAttempted && lastCachedProbeOk)
  {
    copyText(likelySource, sizeof(likelySource),
             "DNS; TCP/W5500 SIGUIO OPERATIVO");
    return;
  }
  if (lastLocalProbeAttempted && lastLocalProbeOk)
  {
    copyText(likelySource, sizeof(likelySource),
             dnsError ? "DNS/INTERNET; LAN LOCAL OK"
                      : "INTERNET/SERVIDOR; LAN LOCAL OK");
    return;
  }
  if (lastLocalProbeAttempted && !lastLocalProbeOk &&
      hardwarePresent && linkKnown && linkOn && ipValid)
  {
    copyText(likelySource, sizeof(likelySource),
             "LAN/W5500/CABLE/SWITCH: REVISAR");
    return;
  }
  if (dnsError)
  {
    copyText(likelySource, sizeof(likelySource),
             "PROBABLE DNS; SIN PRUEBA LAN");
    return;
  }
  if (hardwarePresent && linkKnown && linkOn && ipValid)
  {
    copyText(likelySource, sizeof(likelySource),
             "NO CONCLUYENTE; FISICO SIGUE ON");
    return;
  }
  copyText(likelySource, sizeof(likelySource), "NO CONCLUYENTE");
}

void appendPostFailureSnapshot(char *detail, size_t detailSize)
{
  bool sampled = sampleEthernet(false);
  char suffix[80] = {};
  if (!sampled)
  {
    copyText(suffix, sizeof(suffix), " | POST: SPI LOCK");
  }
  else
  {
    snprintf(suffix, sizeof(suffix),
             " | POST HW=%s LINK=%s IP=%s",
             hardwareText, linkText, ipText);
  }
  appendText(detail, detailSize, suffix);
}

uint16_t parseHttpCode(const char *line)
{
  if (!line || strncmp(line, "HTTP/", 5) != 0)
    return 0;
  const char *space = strchr(line, ' ');
  if (!space)
    return 0;
  int code = atoi(space + 1);
  return code >= 100 && code <= 599 ? (uint16_t)code : 0;
}

bool dnsRefreshDue()
{
  if (!cachedResolvedValid)
    return true;
  return (uint32_t)(stats.tests - lastDnsRefreshTest) >= DNS_REFRESH_EVERY_TESTS;
}

StressError resolveTarget(IPAddress &remoteIp, char *detail, size_t detailSize)
{
  lastDnsResult = 0;
  lastDnsMs = 0;
  lastDnsOk = false;
  lastDnsUsedCache = false;
  lastResolvedIp = IPAddress(0, 0, 0, 0);

  if (!dnsRefreshDue())
  {
    remoteIp = cachedResolvedIp;
    lastResolvedIp = remoteIp;
    lastDnsResult = 1;
    lastDnsOk = true;
    lastDnsUsedCache = true;
    formatIP(remoteIp, resolvedIpText, sizeof(resolvedIpText));
    return STRESS_OK;
  }

  if (!lockEthernet(1000))
  {
    copyText(detail, detailSize, "Timeout SPI antes de DNS");
    return STRESS_ERR_SPI_LOCK;
  }

  IPAddress dnsServer = Ethernet.dnsServerIP();
  formatIP(dnsServer, dnsServerText, sizeof(dnsServerText));
  dnsClient.begin(dnsServer);

  uint32_t started = millis();
  lastDnsResult = dnsClient.getHostByName(
      STRESS_HOST, remoteIp, DNS_STEP_TIMEOUT_MS);
  lastDnsMs = millis() - started;
  unlockEthernet();

  if (lastDnsResult != 1 || !isValidIP(remoteIp))
  {
    snprintf(detail, detailSize,
             "DNS fallo raw=%d %s; server=%s; %lums",
             lastDnsResult, dnsResultName(lastDnsResult), dnsServerText,
             (unsigned long)lastDnsMs);
    return dnsErrorFromResult(lastDnsResult);
  }

  lastDnsOk = true;
  lastResolvedIp = remoteIp;
  cachedResolvedIp = remoteIp;
  cachedResolvedValid = true;
  lastDnsRefreshTest = stats.tests;
  formatIP(remoteIp, resolvedIpText, sizeof(resolvedIpText));
  return STRESS_OK;
}

StressError executeHttpTransaction(
    IPAddress remoteIp,
    char *detail,
    size_t detailSize,
    uint16_t &code,
    uint32_t &bytes)
{
  char response[RESPONSE_BUFFER_SIZE + 1] = {};
  size_t stored = 0;
  bool received = false;
  bool timedOut = false;

  lastTcpOk = false;
  lastHttpOk = false;
  lastTcpMs = 0;
  lastFirstByteMs = 0;
  lastHttpMs = 0;

  if (!lockEthernet(1000))
  {
    copyText(detail, detailSize, "Timeout SPI antes de TCP connect");
    return STRESS_ERR_SPI_LOCK;
  }

  stressClient.stop();
  stressClient.setConnectionTimeout(TCP_CONNECT_TIMEOUT_MS);

  uint32_t totalStarted = millis();
  uint32_t tcpStarted = millis();
  bool connected = stressClient.connect(remoteIp, STRESS_PORT) == 1;
  lastTcpMs = millis() - tcpStarted;

  if (!connected)
  {
    stressClient.stop();
    unlockEthernet();
    lastHttpMs = millis() - totalStarted;
    snprintf(detail, detailSize,
             "TCP connect fallo a %s:%u en %lums",
             resolvedIpText, STRESS_PORT, (unsigned long)lastTcpMs);
    return STRESS_ERR_TCP_CONNECT;
  }

  lastTcpOk = true;
  stressClient.print("GET ");
  stressClient.print(STRESS_PATH);
  stressClient.println(" HTTP/1.1");
  stressClient.print("Host: ");
  stressClient.println(STRESS_HOST);
  stressClient.println("User-Agent: JWPLC-Basic-Ethernet-Stress-v3");
  stressClient.println("Accept: text/html");
  stressClient.println("Cache-Control: no-cache");
  stressClient.println("Connection: close");
  stressClient.println();

  uint32_t responseStarted = millis();
  while ((uint32_t)(millis() - responseStarted) < RESPONSE_TIMEOUT_MS)
  {
    while (stressClient.available() > 0)
    {
      int value = stressClient.read();
      if (value < 0)
        break;
      if (!received)
        lastFirstByteMs = millis() - totalStarted;
      received = true;
      bytes++;
      if (stored < RESPONSE_BUFFER_SIZE)
        response[stored++] = (char)value;
    }
    if (!stressClient.connected() && stressClient.available() == 0)
      break;
    delay(1);
  }

  timedOut = stressClient.connected() || stressClient.available() > 0;
  stressClient.stop();
  unlockEthernet();

  lastHttpMs = millis() - totalStarted;
  response[stored] = '\0';

  if (!received)
  {
    copyText(detail, detailSize,
             "TCP conecto pero no llegaron bytes HTTP");
    return timedOut ? STRESS_ERR_RX_TIMEOUT : STRESS_ERR_NO_RESPONSE;
  }
  if (timedOut)
  {
    copyText(detail, detailSize,
             "Respuesta HTTP no finalizo dentro del timeout");
    return STRESS_ERR_RX_TIMEOUT;
  }

  const char *lineEnd = strstr(response, "\r\n");
  if (!lineEnd)
  {
    copyText(lastStatusLine, sizeof(lastStatusLine), "SIN LINEA HTTP");
    copyText(detail, detailSize, "Respuesta sin linea HTTP completa");
    return STRESS_ERR_BAD_STATUS;
  }

  size_t length = (size_t)(lineEnd - response);
  if (length >= sizeof(lastStatusLine))
    length = sizeof(lastStatusLine) - 1;
  memcpy(lastStatusLine, response, length);
  lastStatusLine[length] = '\0';

  code = parseHttpCode(lastStatusLine);
  if (code == 0)
  {
    copyText(detail, detailSize, "Linea HTTP invalida o corrupta");
    return STRESS_ERR_BAD_STATUS;
  }
  if (code < 200 || code >= 400)
  {
    snprintf(detail, detailSize, "Servidor respondio HTTP %u", code);
    return STRESS_ERR_HTTP_CODE;
  }
  if (EXPECTED_TOKEN[0] != '\0' && strstr(response, EXPECTED_TOKEN) == nullptr)
  {
    snprintf(detail, detailSize,
             "No aparece token esperado: %s", EXPECTED_TOKEN);
    return STRESS_ERR_CONTENT;
  }

  copyText(detail, detailSize,
           "DNS/cache, TCP, HTTP y contenido verificados");
  lastHttpOk = true;
  return STRESS_OK;
}

void resetLayerResult()
{
  lastDnsResult = 0;
  lastDnsUsedCache = false;
  lastDnsOk = false;
  lastTcpOk = false;
  lastHttpOk = false;
  lastDnsMs = 0;
  lastTcpMs = 0;
  lastFirstByteMs = 0;
  lastHttpMs = 0;
  lastCachedProbeAttempted = false;
  lastCachedProbeOk = false;
  lastCachedProbeMs = 0;
  lastLocalProbeAttempted = false;
  lastLocalProbeOk = false;
  lastLocalProbeMs = 0;
  copyText(cachedProbeText, sizeof(cachedProbeText), "NO EJECUTADA");
  copyText(localProbeText, sizeof(localProbeText),
           ENABLE_LOCAL_REFERENCE ? "PENDIENTE" : "DESHABILITADA");
}

void runTest()
{
  if (!running || busy)
    return;

  busy = true;
  lastTestStartedMs = millis();
  stats.tests++;
  currentError = STRESS_OK;
  copyText(currentResult, sizeof(currentResult), "PROBANDO CAPAS...");
  copyText(lastStatusLine, sizeof(lastStatusLine), "-");
  resetLayerResult();
  requestUi();

  char detail[192] = {};
  uint16_t code = 0;
  uint32_t bytes = 0;
  IPAddress remoteIp(0, 0, 0, 0);

  StressError result = checkPrerequisites(detail, sizeof(detail));
  if (result == STRESS_OK)
    result = resolveTarget(remoteIp, detail, sizeof(detail));
  if (result == STRESS_OK)
    result = executeHttpTransaction(
        remoteIp, detail, sizeof(detail), code, bytes);

  stats.lastCode = code;
  stats.lastBytes = bytes;
  stats.totalBytes += bytes;

  if (result == STRESS_OK)
  {
    recordSuccess(code, bytes);
  }
  else
  {
    runAuxiliaryProbes(result);
    appendPostFailureSnapshot(detail, sizeof(detail));
    classifyLikelySource(result);

    char diagnosis[192] = {};
    copyText(diagnosis, sizeof(diagnosis), detail);
    appendText(diagnosis, sizeof(diagnosis), " | DIAG: ");
    appendText(diagnosis, sizeof(diagnosis), likelySource);
    recordFailure(result, diagnosis);

    Serial.print("[ETH-STRESS][CAPAS] DNS=");
    Serial.print(lastDnsUsedCache ? "CACHE" : "LIVE");
    Serial.print(" raw=");
    Serial.print(lastDnsResult);
    Serial.print(" ");
    Serial.print(dnsResultName(lastDnsResult));
    Serial.print(" ");
    Serial.print(lastDnsMs);
    Serial.print("ms server=");
    Serial.print(dnsServerText);
    Serial.print(" resolved=");
    Serial.print(resolvedIpText);
    Serial.print(" | TCP=");
    Serial.print(lastTcpOk ? "OK" : "FAIL/NO");
    Serial.print(" ");
    Serial.print(lastTcpMs);
    Serial.print("ms | CACHE=");
    Serial.print(cachedProbeText);
    Serial.print(" | LOCAL=");
    Serial.println(localProbeText);
  }

  busy = false;
  requestUi();
}

// -----------------------------------------------------------------------------
// Botonera.
// -----------------------------------------------------------------------------
void handleButton(uint8_t id)
{
  switch (id)
  {
  case BTN_LEFT:
    page = page == 0 ? PAGE_COUNT - 1 : page - 1;
    requestUi(true);
    break;
  case BTN_RIGHT:
    page = (uint8_t)((page + 1) % PAGE_COUNT);
    requestUi(true);
    break;
  case BTN_OK:
    running = !running;
    if (running)
    {
      uint32_t interval = TEST_INTERVALS_MS[intervalIndex];
      lastTestStartedMs = millis() - interval;
    }
    requestUi();
    break;
  case BTN_UP:
    if (intervalIndex + 1 < TEST_INTERVAL_COUNT)
      intervalIndex++;
    requestUi();
    break;
  case BTN_DOWN:
    if (intervalIndex > 0)
      intervalIndex--;
    requestUi();
    break;
  case BTN_ESC:
    acknowledgeAlarm();
    break;
  }
}

void readButtons()
{
  if (!displayReady || !JWPLCButtons::isReady() || busy)
    return;
  for (uint8_t id = 0; id < BTN_COUNT; id++)
    if (JWPLC_Buttons.pressed(id))
      handleButton(id);
}

// -----------------------------------------------------------------------------
// TFT con cache por filas.
// -----------------------------------------------------------------------------
uint16_t stateColor(bool ok)
{
  return ok ? ST77XX_GREEN : ST77XX_RED;
}

void drawFrame(Adafruit_ST7789 &tft)
{
  static const char *titles[PAGE_COUNT] = {
      "ETH STRESS TEST", "DIAGNOSTICO CAPAS",
      "CONTADORES DE FALLA", "ULTIMO ERROR"};

  invalidatePageCache(page);
  tft.fillScreen(ST77XX_BLACK);
  tft.setTextWrap(false);
  tft.setTextSize(2);
  tft.setTextColor(ST77XX_CYAN, ST77XX_BLACK);
  tft.setCursor(7, 7);
  tft.print(titles[page]);
  tft.setTextSize(1);
  tft.setTextColor(ST77XX_YELLOW, ST77XX_BLACK);
  tft.setCursor(286, 10);
  tft.print(page + 1);
  tft.print("/");
  tft.print(PAGE_COUNT);
  tft.drawFastHLine(0, 29, 320, ST77XX_BLUE);
  tft.drawFastHLine(0, 148, 320, ST77XX_BLUE);
  tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
  tft.setCursor(4, 157);
  tft.print("</>=PAG OK=PAUSA UP/DN=VELOCIDAD ESC=ACK");
}

void drawPage0(Adafruit_ST7789 &tft)
{
  char intervalText[16] = {};
  formatInterval(intervalText, sizeof(intervalText));

  char line[96] = {};
  snprintf(line, sizeof(line), "%s Int=%s Host=%s",
           busy ? "PROBANDO" : (running ? "RUN" : "PAUSA"),
           intervalText, STRESS_HOST);
  drawCachedRow(tft, 0, 36, "", line,
                running ? ST77XX_GREEN : ST77XX_YELLOW);

  snprintf(line, sizeof(line), "HW=%s LINK=%s IP=%s",
           hardwareText, linkText, ipText);
  uint16_t hwColor = hardwarePresent && linkKnown && linkOn
                         ? ST77XX_GREEN
                         : ST77XX_YELLOW;
  drawCachedRow(tft, 1, 52, "", line, hwColor);

  snprintf(line, sizeof(line), "Tests %lu OK %lu ERR %lu Racha %lu",
           (unsigned long)stats.tests,
           (unsigned long)stats.ok,
           (unsigned long)stats.failed,
           (unsigned long)stats.failStreak);
  drawCachedRow(tft, 2, 68, "", line, ST77XX_WHITE);

  drawCachedRow(tft, 3, 84, "Ultimo: ", currentResult,
                currentError == STRESS_OK ? ST77XX_GREEN : ST77XX_RED);

  drawCachedRow(tft, 4, 100, "Origen: ", likelySource,
                currentError == STRESS_OK ? ST77XX_GREEN : ST77XX_YELLOW);

  snprintf(line, sizeof(line), "DNS %s/%lums TCP %lums 1B %lums TOT %lums",
           lastDnsUsedCache ? "CACHE" : "LIVE",
           (unsigned long)lastDnsMs,
           (unsigned long)lastTcpMs,
           (unsigned long)lastFirstByteMs,
           (unsigned long)lastHttpMs);
  drawCachedRow(tft, 5, 116, "", line, ST77XX_WHITE);

  char runtime[20] = {};
  formatDuration(millis() - bootMs, runtime, sizeof(runtime));
  unsigned long lastOkAge = lastSuccessMs == 0
                                ? 0UL
                                : (unsigned long)((millis() - lastSuccessMs) / 1000UL);
  snprintf(line, sizeof(line), "Tiempo %s UltOK=%s%lus ERR=%s",
           runtime,
           lastSuccessMs == 0 ? "NUNCA/" : "",
           lastOkAge,
           errLedState ? "LAT" : "NO");
  drawCachedRow(tft, 6, 132, "", line,
                errLedState ? ST77XX_YELLOW : ST77XX_WHITE);
}

void drawPage1(Adafruit_ST7789 &tft)
{
  char line[96] = {};
  snprintf(line, sizeof(line), "%s raw=%d %s %lums",
           lastDnsUsedCache ? "CACHE" : "LIVE",
           lastDnsResult,
           dnsResultName(lastDnsResult),
           (unsigned long)lastDnsMs);
  drawCachedRow(tft, 0, 36, "DNS: ", line,
                lastDnsOk ? ST77XX_GREEN : ST77XX_YELLOW);

  snprintf(line, sizeof(line), "%s -> %s",
           dnsServerText, resolvedIpText);
  drawCachedRow(tft, 1, 52, "Servidor/IP: ", line, ST77XX_WHITE);

  snprintf(line, sizeof(line), "%s %lums a puerto %u",
           lastTcpOk ? "OK" : "FAIL/NO",
           (unsigned long)lastTcpMs, STRESS_PORT);
  drawCachedRow(tft, 2, 68, "TCP: ", line,
                lastTcpOk ? ST77XX_GREEN : ST77XX_YELLOW);

  snprintf(line, sizeof(line), "%s code=%u RX=%luB",
           lastHttpOk ? "OK" : "FAIL/NO",
           stats.lastCode, (unsigned long)stats.lastBytes);
  drawCachedRow(tft, 3, 84, "HTTP: ", line,
                lastHttpOk ? ST77XX_GREEN : ST77XX_YELLOW);

  snprintf(line, sizeof(line), "1B=%lums total=%lums DNS cada %u tests",
           (unsigned long)lastFirstByteMs,
           (unsigned long)lastHttpMs,
           DNS_REFRESH_EVERY_TESTS);
  drawCachedRow(tft, 4, 100, "Tiempos: ", line, ST77XX_WHITE);

  drawCachedRow(tft, 5, 116, "Cache: ", cachedProbeText,
                lastCachedProbeAttempted
                    ? (lastCachedProbeOk ? ST77XX_GREEN : ST77XX_RED)
                    : ST77XX_WHITE);

  drawCachedRow(tft, 6, 132, "LAN local: ", localProbeText,
                lastLocalProbeAttempted
                    ? (lastLocalProbeOk ? ST77XX_GREEN : ST77XX_RED)
                    : ST77XX_WHITE);
}

void drawPage2(Adafruit_ST7789 &tft)
{
  char line[96] = {};
  snprintf(line, sizeof(line), "SPI %lu HW %lu LINK %lu DHCP %lu NOTRDY %lu",
           (unsigned long)stats.spi,
           (unsigned long)stats.noHardware,
           (unsigned long)stats.link,
           (unsigned long)stats.dhcpIp,
           (unsigned long)stats.notReady);
  drawCachedRow(tft, 0, 36, "", line, ST77XX_WHITE);

  snprintf(line, sizeof(line), "DNS TMO %lu SRV %lu RESP %lu OTRO %lu",
           (unsigned long)stats.dnsTimeout,
           (unsigned long)stats.dnsServer,
           (unsigned long)stats.dnsResponse,
           (unsigned long)stats.dnsOther);
  drawCachedRow(tft, 1, 52, "", line, ST77XX_WHITE);

  snprintf(line, sizeof(line), "TCP %lu SINRX %lu TMO-RX %lu BADHTTP %lu",
           (unsigned long)stats.tcpConnect,
           (unsigned long)stats.noResponse,
           (unsigned long)stats.timeout,
           (unsigned long)stats.badStatus);
  drawCachedRow(tft, 2, 68, "", line, ST77XX_WHITE);

  snprintf(line, sizeof(line), "CODE %lu DATA %lu CACHE OK/F %lu/%lu",
           (unsigned long)stats.httpCode,
           (unsigned long)stats.content,
           (unsigned long)stats.cachedProbeOk,
           (unsigned long)stats.cachedProbeFail);
  drawCachedRow(tft, 3, 84, "", line, ST77XX_WHITE);

  snprintf(line, sizeof(line), "LAN OK/F %lu/%lu RX=%luKiB",
           (unsigned long)stats.localProbeOk,
           (unsigned long)stats.localProbeFail,
           (unsigned long)(stats.totalBytes / 1024ULL));
  drawCachedRow(tft, 4, 100, "", line, ST77XX_WHITE);

  snprintf(line, sizeof(line), "Link OFF/ON %lu/%lu HW OFF/ON %lu/%lu",
           (unsigned long)stats.linkDrop,
           (unsigned long)stats.linkRecovery,
           (unsigned long)stats.hardwareLoss,
           (unsigned long)stats.hardwareRecovery);
  drawCachedRow(tft, 5, 116, "", line, ST77XX_WHITE);

  uint32_t average = stats.ok > 0
                         ? (uint32_t)(stats.successfulLatencyTotal / stats.ok)
                         : 0;
  snprintf(line, sizeof(line), "Lat min/avg/max %lu/%lu/%lums RachaMax %lu",
           (unsigned long)stats.minLatency,
           (unsigned long)average,
           (unsigned long)stats.maxLatency,
           (unsigned long)stats.maxFailStreak);
  drawCachedRow(tft, 6, 132, "", line, ST77XX_WHITE);
}

void drawPage3(Adafruit_ST7789 &tft)
{
  drawCachedRow(tft, 0, 36, "Tipo: ", lastErrorName,
                lastError == STRESS_OK ? ST77XX_GREEN : ST77XX_RED);
  drawCachedRow(tft, 1, 52, "Fecha: ", lastErrorTime, ST77XX_WHITE);
  drawCachedRow(tft, 2, 68, "Origen: ", lastLikelySource, ST77XX_YELLOW);

  char line1[48] = {};
  char line2[48] = {};
  char line3[48] = {};
  splitText3(lastErrorDetail,
             line1, sizeof(line1),
             line2, sizeof(line2),
             line3, sizeof(line3));

  drawCachedRow(tft, 3, 84, "Detalle: ", line1, ST77XX_YELLOW);
  drawCachedRow(tft, 4, 100, "         ", line2, ST77XX_YELLOW);
  drawCachedRow(tft, 5, 116, "         ", line3, ST77XX_YELLOW);
  drawCachedRow(tft, 6, 132, "Alarma: ",
                errLedState ? "ACTIVA/LATCHEADA" : "RECONOCIDA",
                errLedState ? ST77XX_RED : ST77XX_GREEN);
}

void drawContent(Adafruit_ST7789 &tft)
{
  if (page == 0)
    drawPage0(tft);
  else if (page == 1)
    drawPage1(tft);
  else if (page == 2)
    drawPage2(tft);
  else
    drawPage3(tft);
}

extern "C" void jwplcUserDisplayEnterCallback()
{
  bool frame = false;
  bool content = false;
  takeUi(frame, content);
  auto &tft = JWPLC_Display.tft();
  drawFrame(tft);
  drawContent(tft);
}

extern "C" void jwplcUserDisplayRefreshCallback(
    const JWPLC_IOState *io,
    const JWPLC_RTCState *rtc)
{
  (void)io;
  (void)rtc;
  bool frame = false;
  bool content = false;
  takeUi(frame, content);
  if (!frame && !content)
    return;
  auto &tft = JWPLC_Display.tft();
  if (frame)
    drawFrame(tft);
  drawContent(tft);
}

extern "C" void jwplcUserDisplayExitCallback()
{
  Serial.println("[ETH-STRESS] Display USER -> IDLE");
}

// -----------------------------------------------------------------------------
// Log y ciclo principal.
// -----------------------------------------------------------------------------
void printStatus()
{
  char runtime[20] = {};
  char intervalText[16] = {};
  formatDuration(millis() - bootMs, runtime, sizeof(runtime));
  formatInterval(intervalText, sizeof(intervalText));

  Serial.print("[ETH-STRESS] ");
  Serial.print(running ? "RUN" : "PAUSA");
  Serial.print(" | ARRANQUE ");
  Serial.print(startupQualified ? "OK" : (startupTimeoutExpired ? "TIMEOUT" : "ESPERA"));
  Serial.print(" | INT ");
  Serial.print(intervalText);
  Serial.print(" | HW ");
  Serial.print(hardwareText);
  Serial.print(" | LINK ");
  Serial.print(linkText);
  Serial.print(" | IP ");
  Serial.print(ipText);
  Serial.print(" | TEST ");
  Serial.print(stats.tests);
  Serial.print(" OK ");
  Serial.print(stats.ok);
  Serial.print(" ERR ");
  Serial.print(stats.failed);
  Serial.print(" | DNS ");
  Serial.print(lastDnsUsedCache ? "CACHE" : "LIVE");
  Serial.print("/");
  Serial.print(lastDnsMs);
  Serial.print("ms TCP ");
  Serial.print(lastTcpMs);
  Serial.print("ms | UPTIME ");
  Serial.print(runtime);
  Serial.print(" | ULT ERR ");
  Serial.print(lastErrorName);
  Serial.print(" | ORIGEN ");
  Serial.println(lastLikelySource);
}

void setup()
{
  Serial.begin(115200);
  delay(1200);
  bootMs = millis();
  lastTestStartedMs = bootMs;
  invalidateAllUiCache();

  Serial.println();
  Serial.println("JWPLC Basic - Ethernet Continuous Stress TFT v3");
  Serial.print("Destino: http://");
  Serial.print(STRESS_HOST);
  Serial.print(":");
  Serial.print(STRESS_PORT);
  Serial.println(STRESS_PATH);
  Serial.print("Arranque: espera hasta ");
  Serial.print(STARTUP_QUALIFY_TIMEOUT_MS);
  Serial.print(" ms y exige estabilidad de ");
  Serial.print(STARTUP_STABLE_MS);
  Serial.println(" ms");
  Serial.print("Sondeo HW/LINK/IP: ");
  Serial.print(STATUS_POLL_MS);
  Serial.println(" ms");
  Serial.print("DNS live cada ");
  Serial.print(DNS_REFRESH_EVERY_TESTS);
  Serial.println(" pruebas; el resto usa IP cacheada");
  Serial.println("Intervalo desde inicio a inicio; CONT ejecuta sin pausa adicional.");
  Serial.println("Para CONT/50/100/250 ms usar preferentemente servidor LAN local.");
  Serial.println("LEFT/RIGHT=pagina | OK=pausa | UP/DOWN=velocidad | ESC=ACK");
  Serial.println("No se reinicia automaticamente el W5500 al fallar.");

  JWPLC_Display.setIdleWakeMode(IDLE_WAKE_ANY_BUTTON);
  JWPLC_Display.setIdleReturnMode(IDLE_RETURN_DISABLED);
  JWPLC_Display.setUserRefreshPeriodMs(100);
  JWPLC_Display.setRunLed(true);
  JWPLC_Display.setEthLedAuto(true);
  JWPLC_Display.setErrLed(false);
  requestUi(true);
}

void loop()
{
  uint32_t now = millis();

  if (!displayReady && JWPLC_Display.isReady())
  {
    displayReady = true;
    JWPLC_Display.enterUserUI();
  }

  readButtons();

  if (!busy && (uint32_t)(now - lastPollMs) >= STATUS_POLL_MS)
  {
    lastPollMs = now;
    if (!sampleEthernet(transitionsArmed) && transitionsArmed)
    {
      copyText(likelySource, sizeof(likelySource),
               "POSIBLE SPI/W5500/SOLDADURA");
      recordObservedFault(
          STRESS_ERR_SPI_LOCK,
          "Timeout SPI durante el sondeo periodico del W5500");
    }
    updateStartupQualification(now);
  }

  if ((uint32_t)(now - lastUiSecondMs) >= 1000UL)
  {
    lastUiSecondMs = now;
    requestUi();
  }

  bool startupAllowsTest = startupQualified || startupTimeoutExpired;
  uint32_t interval = TEST_INTERVALS_MS[intervalIndex];
  bool intervalFinished = interval == 0 ||
                          (uint32_t)(now - lastTestStartedMs) >= interval;

  if (running && !busy && startupAllowsTest && intervalFinished)
    runTest();

  if ((uint32_t)(now - lastLogMs) >= SERIAL_LOG_MS)
  {
    lastLogMs = now;
    printStatus();
  }

  delay(2);
}
