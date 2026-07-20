/*
  Ethernet_Continuous_Stress_TFT

  Prueba continua y diagnostico por capas del puerto Ethernet del JWPLC Basic.

  Separa:
  - presencia del W5500 y bus SPI;
  - enlace fisico RJ45/PHY;
  - DHCP e IP local;
  - resolucion DNS;
  - conexion TCP al IP resuelto;
  - recepcion HTTP, codigo y contenido;
  - prueba TCP auxiliar contra IP cacheada;
  - prueba local opcional para aislar Internet/DNS del hardware.

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
// -----------------------------------------------------------------------------
static const char STRESS_HOST[] = "example.com";
static const char STRESS_PATH[] = "/";
static const char EXPECTED_TOKEN[] = "Example Domain";
static const uint16_t STRESS_PORT = 80;

// -----------------------------------------------------------------------------
// Referencia LAN opcional.
//
// Para aislar soldadura/W5500/cable/switch de Internet y DNS:
// 1. En una PC de la misma red, servir JWPLC_STRESS_OK por HTTP.
// 2. Cambiar ENABLE_LOCAL_REFERENCE a true.
// 3. Ajustar LOCAL_REFERENCE_IP y puerto.
// -----------------------------------------------------------------------------
static const bool ENABLE_LOCAL_REFERENCE = false;
static IPAddress LOCAL_REFERENCE_IP(192, 168, 0, 4);
static const uint16_t LOCAL_REFERENCE_PORT = 8080;

static const uint16_t DNS_STEP_TIMEOUT_MS = 1500;
static const uint16_t TCP_CONNECT_TIMEOUT_MS = 1500;
static const uint32_t RESPONSE_TIMEOUT_MS = 5000UL;
static const uint32_t STARTUP_GRACE_MS = 8000UL;
static const uint32_t STATUS_POLL_MS = 250UL;
static const uint32_t SERIAL_LOG_MS = 5000UL;
static const size_t RESPONSE_BUFFER_SIZE = 1536;

static const uint32_t TEST_INTERVALS_MS[] = {
    250UL, 500UL, 1000UL, 2000UL, 5000UL, 10000UL};
static const uint8_t TEST_INTERVAL_COUNT =
    sizeof(TEST_INTERVALS_MS) / sizeof(TEST_INTERVALS_MS[0]);
static const uint8_t DEFAULT_INTERVAL_INDEX = 2;
static const uint8_t PAGE_COUNT = 4;

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
  uint32_t lastTotalLatency = 0;
  uint32_t minLatency = 0;
  uint32_t maxLatency = 0;
  uint32_t lastBytes = 0;
  uint16_t lastCode = 0;
};

StressStats stats;
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
bool linkOn = false;
bool ipValid = false;
EthernetHardwareStatus hardwareState = EthernetNoHardware;

uint8_t page = 0;
uint8_t intervalIndex = DEFAULT_INTERVAL_INDEX;
StressError currentError = STRESS_OK;
StressError lastError = STRESS_OK;

char hardwareText[16] = "INICIANDO";
char ipText[20] = "0.0.0.0";
char dnsServerText[20] = "0.0.0.0";
char resolvedIpText[20] = "0.0.0.0";
char currentResult[52] = "ESPERANDO DHCP";
char lastErrorName[28] = "NINGUNO";
char lastErrorDetail[160] = "Sin fallas registradas";
char lastErrorTime[24] = "-";
char lastStatusLine[64] = "-";
char likelySource[40] = "SIN FALLAS";
char lastLikelySource[40] = "SIN FALLAS";
char cachedProbeText[28] = "NO EJECUTADA";
char localProbeText[28] = "DESHABILITADA";

IPAddress lastResolvedIp(0, 0, 0, 0);
IPAddress cachedResolvedIp(0, 0, 0, 0);
bool cachedResolvedValid = false;

int lastDnsResult = 0;
bool lastDnsNumeric = false;
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
uint32_t lastTestMs = 0;
uint32_t lastPollMs = 0;
uint32_t lastLogMs = 0;
uint32_t lastUiSecondMs = 0;
uint32_t lastSuccessMs = 0;
uint32_t lastErrorUptimeMs = 0;

portMUX_TYPE uiMux = portMUX_INITIALIZER_UNLOCKED;
volatile bool uiFrameDirty = true;
volatile bool uiContentDirty = true;

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

void rememberError(StressError error, const char *detail)
{
  currentError = error;
  lastError = error;
  alarmLatched = true;
  copyText(lastErrorName, sizeof(lastErrorName), errorName(error));
  copyText(lastErrorDetail, sizeof(lastErrorDetail), detail);
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

bool sampleEthernet(bool countTransitions)
{
  if (!JWPLC_Ethernet.isEnabled())
  {
    hardwarePresent = false;
    linkOn = false;
    ipValid = false;
    copyText(hardwareText, sizeof(hardwareText), "DESHABILITADO");
    copyText(ipText, sizeof(ipText), "0.0.0.0");
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

  bool newHardwarePresent = newHardware != EthernetNoHardware;
  bool newLinkOn = newLink == LinkON;
  bool newIpValid = isValidIP(newIp);

  if (stateValid && countTransitions)
  {
    if (hardwarePresent && !newHardwarePresent)
    {
      stats.hardwareLoss++;
      recordObservedFault(
          STRESS_ERR_NO_HARDWARE,
          "W5500 desaparecio: revisar alimentacion, SPI, CS y soldadura");
    }
    else if (!hardwarePresent && newHardwarePresent)
      stats.hardwareRecovery++;

    if (hardwarePresent && newHardwarePresent)
    {
      if (linkOn && !newLinkOn)
      {
        stats.linkDrop++;
        recordObservedFault(
            STRESS_ERR_LINK_DOWN,
            "Caida de link: revisar RJ45, magneticos, pares, cable y soldadura");
      }
      else if (!linkOn && newLinkOn)
        stats.linkRecovery++;
    }
  }

  char newIpText[20] = {};
  formatIP(newIp, newIpText, sizeof(newIpText));
  bool changed = !stateValid ||
                 hardwareState != newHardware ||
                 hardwarePresent != newHardwarePresent ||
                 linkOn != newLinkOn ||
                 ipValid != newIpValid ||
                 strcmp(ipText, newIpText) != 0;

  hardwareState = newHardware;
  hardwarePresent = newHardwarePresent;
  linkOn = newLinkOn;
  ipValid = newIpValid;
  stateValid = true;
  copyText(hardwareText, sizeof(hardwareText), hardwareName(newHardware));
  copyText(ipText, sizeof(ipText), newIpText);
  if (changed)
    requestUi();
  return true;
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

  if ((primaryError == STRESS_ERR_DNS_TIMEOUT ||
       primaryError == STRESS_ERR_DNS_SERVER ||
       primaryError == STRESS_ERR_DNS_RESPONSE ||
       primaryError == STRESS_ERR_DNS_OTHER) &&
      cachedResolvedValid)
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
    copyText(likelySource, sizeof(likelySource), "POSIBLE SPI/W5500/SOLDADURA");
    return;
  }
  if (error == STRESS_ERR_LINK_DOWN)
  {
    copyText(likelySource, sizeof(likelySource), "POSIBLE RJ45/CABLE/MAGNETICOS");
    return;
  }
  if (error == STRESS_ERR_DHCP_IP || error == STRESS_ERR_NOT_READY)
  {
    copyText(likelySource, sizeof(likelySource), "RED LOCAL/DHCP; REVISAR ESTABILIDAD");
    return;
  }

  bool dnsError = error == STRESS_ERR_DNS_TIMEOUT ||
                  error == STRESS_ERR_DNS_SERVER ||
                  error == STRESS_ERR_DNS_RESPONSE ||
                  error == STRESS_ERR_DNS_OTHER;

  if (dnsError && lastCachedProbeAttempted && lastCachedProbeOk)
  {
    copyText(likelySource, sizeof(likelySource), "DNS; TCP/W5500 SIGUIO OPERATIVO");
    return;
  }
  if (lastLocalProbeAttempted && lastLocalProbeOk)
  {
    copyText(likelySource, sizeof(likelySource),
             dnsError ? "DNS/INTERNET; LAN LOCAL OK" : "INTERNET/SERVIDOR; LAN LOCAL OK");
    return;
  }
  if (lastLocalProbeAttempted && !lastLocalProbeOk && hardwarePresent && linkOn && ipValid)
  {
    copyText(likelySource, sizeof(likelySource), "LAN/W5500/CABLE/SWITCH: REVISAR");
    return;
  }
  if (dnsError)
  {
    copyText(likelySource, sizeof(likelySource), "PROBABLE DNS; SIN PRUEBA LAN");
    return;
  }
  if (hardwarePresent && linkOn && ipValid)
  {
    copyText(likelySource, sizeof(likelySource), "NO CONCLUYENTE; FISICO SIGUE ON");
    return;
  }
  copyText(likelySource, sizeof(likelySource), "NO CONCLUYENTE");
}

void appendPostFailureSnapshot(char *detail, size_t detailSize)
{
  bool sampled = sampleEthernet(false);
  char suffix[70] = {};
  if (!sampled)
  {
    copyText(suffix, sizeof(suffix), " | POST: SPI LOCK");
  }
  else
  {
    snprintf(suffix, sizeof(suffix), " | POST HW=%s LINK=%s IP=%s",
             hardwareText, linkOn ? "ON" : "OFF", ipText);
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

StressError resolveTarget(IPAddress &remoteIp, char *detail, size_t detailSize)
{
  lastDnsResult = 0;
  lastDnsMs = 0;
  lastDnsNumeric = false;
  lastDnsOk = false;
  lastResolvedIp = IPAddress(0, 0, 0, 0);
  copyText(resolvedIpText, sizeof(resolvedIpText), "0.0.0.0");

  if (!lockEthernet(1000))
  {
    copyText(detail, detailSize, "Timeout SPI antes de DNS");
    return STRESS_ERR_SPI_LOCK;
  }

  IPAddress dnsServer = Ethernet.dnsServerIP();
  formatIP(dnsServer, dnsServerText, sizeof(dnsServerText));
  dnsClient.begin(dnsServer);

  IPAddress numericIp(0, 0, 0, 0);
  lastDnsNumeric = dnsClient.inet_aton(STRESS_HOST, numericIp) == 1;

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
  stressClient.println("User-Agent: JWPLC-Basic-Ethernet-Stress-Layers");
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
    copyText(detail, detailSize, "Respuesta HTTP no finalizo dentro del timeout");
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

  copyText(detail, detailSize, "DNS, TCP, HTTP y contenido verificados");
  lastHttpOk = true;
  return STRESS_OK;
}

void resetLayerResult()
{
  lastDnsResult = 0;
  lastDnsNumeric = false;
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
  stats.tests++;
  currentError = STRESS_OK;
  copyText(currentResult, sizeof(currentResult), "PROBANDO CAPAS...");
  copyText(lastStatusLine, sizeof(lastStatusLine), "-");
  resetLayerResult();
  requestUi();
  delay(50);

  char detail[160] = {};
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
  stats.lastTotalLatency = lastHttpMs;
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

    char diagnosis[160] = {};
    copyText(diagnosis, sizeof(diagnosis), detail);
    appendText(diagnosis, sizeof(diagnosis), " | DIAG: ");
    appendText(diagnosis, sizeof(diagnosis), likelySource);
    copyText(lastLikelySource, sizeof(lastLikelySource), likelySource);
    recordFailure(result, diagnosis);

    Serial.print("[ETH-STRESS][CAPAS] DNS raw=");
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
  transitionsArmed = true;
  lastTestMs = millis();
  requestUi();
}

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
      lastTestMs = millis() - TEST_INTERVALS_MS[intervalIndex];
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

uint16_t stateColor(bool ok)
{
  return ok ? ST77XX_GREEN : ST77XX_RED;
}

void clearRow(Adafruit_ST7789 &tft, int16_t y, int16_t height = 13)
{
  tft.fillRect(0, y - 2, 320, height, ST77XX_BLACK);
}

void row(Adafruit_ST7789 &tft,
         int16_t y,
         const char *label,
         const char *value,
         uint16_t color = ST77XX_WHITE)
{
  clearRow(tft, y);
  tft.setTextSize(1);
  tft.setCursor(6, y);
  tft.setTextColor(ST77XX_CYAN, ST77XX_BLACK);
  tft.print(label);
  tft.setTextColor(color, ST77XX_BLACK);
  tft.print(value);
}

void splitText(const char *source,
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

void drawFrame(Adafruit_ST7789 &tft)
{
  static const char *titles[PAGE_COUNT] = {
      "ETH STRESS TEST", "DIAGNOSTICO CAPAS",
      "CONTADORES DE FALLA", "ULTIMO ERROR"};
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
  tft.print("</>=PAG  OK=PAUSA  UP/DN=INTERV  ESC=ACK");
}

void drawPage0(Adafruit_ST7789 &tft)
{
  clearRow(tft, 36);
  tft.setTextSize(1);
  tft.setCursor(6, 36);
  tft.setTextColor(running ? ST77XX_GREEN : ST77XX_YELLOW, ST77XX_BLACK);
  tft.print(busy ? "PROBANDO" : (running ? "RUN" : "PAUSA"));
  tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
  tft.print("  Int ");
  tft.print(TEST_INTERVALS_MS[intervalIndex]);
  tft.print("ms  Host ");
  tft.print(STRESS_HOST);

  clearRow(tft, 52);
  tft.setCursor(6, 52);
  tft.setTextColor(stateColor(hardwarePresent), ST77XX_BLACK);
  tft.print("HW:");
  tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
  tft.print(hardwareText);
  tft.print("  ");
  tft.setTextColor(stateColor(linkOn), ST77XX_BLACK);
  tft.print("LINK:");
  tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
  tft.print(linkOn ? "ON" : "OFF");
  tft.print("  IP:");
  tft.print(ipText);

  clearRow(tft, 68);
  tft.setCursor(6, 68);
  tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
  tft.print("Tests ");
  tft.print(stats.tests);
  tft.setTextColor(ST77XX_GREEN, ST77XX_BLACK);
  tft.print("  OK ");
  tft.print(stats.ok);
  tft.setTextColor(ST77XX_RED, ST77XX_BLACK);
  tft.print("  ERR ");
  tft.print(stats.failed);

  row(tft, 84, "Ultimo: ", currentResult,
      currentError == STRESS_OK ? ST77XX_GREEN : ST77XX_RED);
  row(tft, 100, "Origen probable: ", likelySource,
      currentError == STRESS_OK ? ST77XX_GREEN : ST77XX_YELLOW);

  clearRow(tft, 116);
  tft.setCursor(6, 116);
  tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
  tft.print("DNS ");
  tft.print(lastDnsMs);
  tft.print("ms TCP ");
  tft.print(lastTcpMs);
  tft.print("ms 1B ");
  tft.print(lastFirstByteMs);
  tft.print("ms TOT ");
  tft.print(lastHttpMs);
  tft.print("ms");

  char runtime[20] = {};
  formatDuration(millis() - bootMs, runtime, sizeof(runtime));
  clearRow(tft, 132);
  tft.setCursor(6, 132);
  tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
  tft.print("Tiempo ");
  tft.print(runtime);
  tft.print("  Ult OK ");
  if (lastSuccessMs == 0)
    tft.print("nunca");
  else
  {
    tft.print((millis() - lastSuccessMs) / 1000UL);
    tft.print("s");
  }
  tft.print(" ERR:");
  tft.setTextColor(errLedState ? ST77XX_RED : ST77XX_GREEN, ST77XX_BLACK);
  tft.print(errLedState ? "LAT" : "NO");
}

void drawPage1(Adafruit_ST7789 &tft)
{
  char dnsLine[64] = {};
  snprintf(dnsLine, sizeof(dnsLine), "%s raw=%d %lums",
           dnsResultName(lastDnsResult), lastDnsResult,
           (unsigned long)lastDnsMs);
  row(tft, 38, "DNS: ", dnsLine,
      lastDnsOk ? ST77XX_GREEN : ST77XX_YELLOW);

  char serverLine[64] = {};
  snprintf(serverLine, sizeof(serverLine), "%s -> %s",
           dnsServerText, resolvedIpText);
  row(tft, 56, "Servidor/IP: ", serverLine);

  char tcpLine[64] = {};
  snprintf(tcpLine, sizeof(tcpLine), "%s %lums a puerto %u",
           lastTcpOk ? "OK" : "FAIL/NO",
           (unsigned long)lastTcpMs, STRESS_PORT);
  row(tft, 74, "TCP: ", tcpLine,
      lastTcpOk ? ST77XX_GREEN : ST77XX_YELLOW);

  char httpLine[64] = {};
  snprintf(httpLine, sizeof(httpLine), "%s code=%u RX=%luB",
           lastHttpOk ? "OK" : "FAIL/NO",
           stats.lastCode, (unsigned long)stats.lastBytes);
  row(tft, 92, "HTTP: ", httpLine,
      lastHttpOk ? ST77XX_GREEN : ST77XX_YELLOW);

  char timingLine[64] = {};
  snprintf(timingLine, sizeof(timingLine), "1B=%lums total=%lums",
           (unsigned long)lastFirstByteMs,
           (unsigned long)lastHttpMs);
  row(tft, 110, "Tiempos: ", timingLine);

  row(tft, 126, "Cache: ", cachedProbeText,
      lastCachedProbeAttempted
          ? (lastCachedProbeOk ? ST77XX_GREEN : ST77XX_RED)
          : ST77XX_WHITE);

  clearRow(tft, 140, 9);
  tft.setCursor(6, 140);
  tft.setTextColor(ST77XX_CYAN, ST77XX_BLACK);
  tft.print("LAN local: ");
  tft.setTextColor(
      lastLocalProbeAttempted
          ? (lastLocalProbeOk ? ST77XX_GREEN : ST77XX_RED)
          : ST77XX_WHITE,
      ST77XX_BLACK);
  tft.print(localProbeText);
}

void drawPage2(Adafruit_ST7789 &tft)
{
  clearRow(tft, 38);
  tft.setTextSize(1);
  tft.setCursor(6, 38);
  tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
  tft.print("SPI ");
  tft.print(stats.spi);
  tft.print(" HW ");
  tft.print(stats.noHardware);
  tft.print(" LINK ");
  tft.print(stats.link);
  tft.print(" DHCP/IP ");
  tft.print(stats.dhcpIp);
  tft.print(" NOTRDY ");
  tft.print(stats.notReady);

  clearRow(tft, 58);
  tft.setCursor(6, 58);
  tft.print("DNS TMO ");
  tft.print(stats.dnsTimeout);
  tft.print(" SRV ");
  tft.print(stats.dnsServer);
  tft.print(" RESP ");
  tft.print(stats.dnsResponse);
  tft.print(" OTRO ");
  tft.print(stats.dnsOther);

  clearRow(tft, 78);
  tft.setCursor(6, 78);
  tft.print("TCP ");
  tft.print(stats.tcpConnect);
  tft.print(" SINRX ");
  tft.print(stats.noResponse);
  tft.print(" TMO-RX ");
  tft.print(stats.timeout);
  tft.print(" BADHTTP ");
  tft.print(stats.badStatus);

  clearRow(tft, 98);
  tft.setCursor(6, 98);
  tft.print("CODE ");
  tft.print(stats.httpCode);
  tft.print(" DATA ");
  tft.print(stats.content);
  tft.print(" CACHE OK/F ");
  tft.print(stats.cachedProbeOk);
  tft.print("/");
  tft.print(stats.cachedProbeFail);

  clearRow(tft, 118);
  tft.setCursor(6, 118);
  tft.print("Link OFF/ON ");
  tft.print(stats.linkDrop);
  tft.print("/");
  tft.print(stats.linkRecovery);
  tft.print(" HW OFF/ON ");
  tft.print(stats.hardwareLoss);
  tft.print("/");
  tft.print(stats.hardwareRecovery);

  clearRow(tft, 138, 11);
  tft.setCursor(6, 138);
  uint32_t average = stats.ok > 0
                         ? (uint32_t)(stats.successfulLatencyTotal / stats.ok)
                         : 0;
  tft.print("Lat min/avg/max ");
  tft.print(stats.minLatency);
  tft.print("/");
  tft.print(average);
  tft.print("/");
  tft.print(stats.maxLatency);
  tft.print("ms  RachaMax ");
  tft.print(stats.maxFailStreak);
}

void drawPage3(Adafruit_ST7789 &tft)
{
  row(tft, 38, "Tipo: ", lastErrorName,
      lastError == STRESS_OK ? ST77XX_GREEN : ST77XX_RED);
  row(tft, 55, "Fecha: ", lastErrorTime);
  row(tft, 72, "Origen: ", lastLikelySource, ST77XX_YELLOW);

  char line1[48] = {};
  char line2[48] = {};
  char line3[48] = {};
  splitText(lastErrorDetail,
            line1, sizeof(line1),
            line2, sizeof(line2),
            line3, sizeof(line3));
  row(tft, 89, "Detalle: ", line1, ST77XX_YELLOW);
  row(tft, 105, "         ", line2, ST77XX_YELLOW);
  row(tft, 121, "         ", line3, ST77XX_YELLOW);

  clearRow(tft, 140, 9);
  tft.setCursor(6, 140);
  tft.setTextColor(errLedState ? ST77XX_RED : ST77XX_GREEN, ST77XX_BLACK);
  tft.print(errLedState ? "ALARMA ACTIVA/LATCHEADA" : "ALARMA RECONOCIDA");
  tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
  tft.print(" ESC=ACK");
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

void printStatus()
{
  char runtime[20] = {};
  formatDuration(millis() - bootMs, runtime, sizeof(runtime));
  Serial.print("[ETH-STRESS] ");
  Serial.print(running ? "RUN" : "PAUSA");
  Serial.print(" | HW ");
  Serial.print(hardwareText);
  Serial.print(" | LINK ");
  Serial.print(linkOn ? "ON" : "OFF");
  Serial.print(" | IP ");
  Serial.print(ipText);
  Serial.print(" | TEST ");
  Serial.print(stats.tests);
  Serial.print(" OK ");
  Serial.print(stats.ok);
  Serial.print(" ERR ");
  Serial.print(stats.failed);
  Serial.print(" | DNS ");
  Serial.print(lastDnsResult);
  Serial.print("/");
  Serial.print(lastDnsMs);
  Serial.print("ms TCP ");
  Serial.print(lastTcpMs);
  Serial.print("ms | UPTIME ");
  Serial.print(runtime);
  Serial.print(" | ULT ERR ");
  Serial.print(lastErrorName);
  Serial.print(" | ORIGEN ULT ERR ");
  Serial.println(lastLikelySource);
}

void setup()
{
  Serial.begin(115200);
  delay(1200);
  bootMs = millis();
  lastTestMs = bootMs;

  Serial.println();
  Serial.println("JWPLC Basic - Ethernet Continuous Stress TFT v2");
  Serial.print("Destino: http://");
  Serial.print(STRESS_HOST);
  Serial.print(":");
  Serial.print(STRESS_PORT);
  Serial.println(STRESS_PATH);
  Serial.print("DNS timeout por intento: ");
  Serial.print(DNS_STEP_TIMEOUT_MS);
  Serial.println(" ms");
  Serial.print("TCP connect timeout: ");
  Serial.print(TCP_CONNECT_TIMEOUT_MS);
  Serial.println(" ms");
  Serial.print("Referencia LAN: ");
  if (ENABLE_LOCAL_REFERENCE)
  {
    Serial.print(LOCAL_REFERENCE_IP);
    Serial.print(":");
    Serial.println(LOCAL_REFERENCE_PORT);
  }
  else
  {
    Serial.println("DESHABILITADA");
  }
  Serial.println("Inicio despues de 8 s para permitir DHCP.");
  Serial.println("LEFT/RIGHT=pagina | OK=pausa | UP/DOWN=intervalo | ESC=ACK");
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
      recordObservedFault(
          STRESS_ERR_SPI_LOCK,
          "Timeout SPI durante el sondeo periodico del W5500");
    }
  }

  if ((uint32_t)(now - lastUiSecondMs) >= 1000UL)
  {
    lastUiSecondMs = now;
    requestUi();
  }

  bool graceFinished = (uint32_t)(now - bootMs) >= STARTUP_GRACE_MS;
  bool intervalFinished =
      (uint32_t)(now - lastTestMs) >= TEST_INTERVALS_MS[intervalIndex];
  if (running && !busy && graceFinished && intervalFinished)
    runTest();

  if ((uint32_t)(now - lastLogMs) >= SERIAL_LOG_MS)
  {
    lastLogMs = now;
    printStatus();
  }

  delay(5);
}
