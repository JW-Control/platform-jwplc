/*
  Ethernet_Continuous_Stress_TFT

  Prueba continua del puerto Ethernet del JWPLC Basic.

  - Genera trafico HTTP repetitivo mediante el W5500.
  - Vigila hardware, link, DHCP/IP, TCP, HTTP y contenido recibido.
  - Mantiene el ultimo error visible aunque la comunicacion se recupere.
  - El LED ERR queda latcheado hasta pulsar ESC.
  - No reinicia automaticamente el W5500 para no ocultar fallas fisicas.

  Controles:
  LEFT/RIGHT = pagina | OK = pausa/reanuda | UP/DOWN = intervalo | ESC = ACK
*/

#include <JWPLC_Display.h>
#include <JWPLC_GlobalPeripherals.h>
#include <Ethernet.h>
#include "jwplc_spi_bus.h"

#include <stdlib.h>
#include <string.h>

// Destino predeterminado. Para aislar Internet, usa una PC local con servidor HTTP.
static const char STRESS_HOST[] = "example.com";
static const char STRESS_PATH[] = "/";
static const char EXPECTED_TOKEN[] = "Example Domain";
static const uint16_t STRESS_PORT = 80;

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
static const uint8_t PAGE_COUNT = 3;

enum StressError : uint8_t
{
  STRESS_OK = 0,
  STRESS_ERR_ETH_DISABLED,
  STRESS_ERR_NOT_READY,
  STRESS_ERR_SPI_LOCK,
  STRESS_ERR_NO_HARDWARE,
  STRESS_ERR_LINK_DOWN,
  STRESS_ERR_DHCP_IP,
  STRESS_ERR_CONNECT_DNS,
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
  uint32_t connect = 0;
  uint32_t noResponse = 0;
  uint32_t timeout = 0;
  uint32_t badStatus = 0;
  uint32_t httpCode = 0;
  uint32_t content = 0;

  uint32_t hardwareLoss = 0;
  uint32_t hardwareRecovery = 0;
  uint32_t linkDrop = 0;
  uint32_t linkRecovery = 0;

  uint64_t totalBytes = 0;
  uint64_t successfulLatencyTotal = 0;
  uint32_t lastLatency = 0;
  uint32_t minLatency = 0;
  uint32_t maxLatency = 0;
  uint32_t lastBytes = 0;
  uint16_t lastCode = 0;
};

StressStats stats;
EthernetClient stressClient;

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
char currentResult[48] = "ESPERANDO DHCP";
char lastErrorName[28] = "NINGUNO";
char lastErrorDetail[80] = "Sin fallas registradas";
char lastErrorTime[24] = "-";
char lastStatusLine[64] = "-";

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
  case STRESS_ERR_CONNECT_DNS:
    return "CONNECT/DNS";
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
  case STRESS_ERR_CONNECT_DNS:
    stats.connect++;
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

void rememberError(StressError error, const char *detail, const char *prefix)
{
  currentError = error;
  lastError = error;
  alarmLatched = true;
  copyText(lastErrorName, sizeof(lastErrorName), errorName(error));
  copyText(lastErrorDetail, sizeof(lastErrorDetail), detail);
  lastErrorUptimeMs = millis() - bootMs;
  formatLastErrorTime();
  snprintf(currentResult, sizeof(currentResult), "%s%s", prefix, errorName(error));
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
  rememberError(error, detail, "ERROR: ");

  Serial.print("[ETH-STRESS][ERROR] #");
  Serial.print(stats.tests);
  Serial.print(" ");
  Serial.print(errorName(error));
  Serial.print(" | ");
  Serial.println(detail);
}

void recordObservedFault(StressError error, const char *detail)
{
  rememberError(error, detail, "OBSERVADO: ");
  Serial.print("[ETH-STRESS][EVENTO] ");
  Serial.print(errorName(error));
  Serial.print(" | ");
  Serial.println(detail);
}

void recordSuccess(uint16_t code, uint32_t bytes, uint32_t latency)
{
  currentError = STRESS_OK;
  stats.ok++;
  stats.failStreak = 0;
  stats.successfulLatencyTotal += latency;
  if (stats.minLatency == 0 || latency < stats.minLatency)
    stats.minLatency = latency;
  if (latency > stats.maxLatency)
    stats.maxLatency = latency;
  lastSuccessMs = millis();

  snprintf(currentResult, sizeof(currentResult),
           "OK HTTP %u | %lums | %luB",
           code, (unsigned long)latency, (unsigned long)bytes);
  updateErrLed();
  requestUi();

  Serial.print("[ETH-STRESS][OK] #");
  Serial.print(stats.tests);
  Serial.print(" HTTP ");
  Serial.print(code);
  Serial.print(" | ");
  Serial.print(latency);
  Serial.print(" ms | ");
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
  bool newIpValid = !(newIp == IPAddress(0, 0, 0, 0));

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
            "Caida de link: revisar RJ45, magneticos, pares y cable");
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

StressError executeTransaction(
    char *detail,
    size_t detailSize,
    uint16_t &code,
    uint32_t &bytes,
    uint32_t &latency)
{
  StressError precondition = checkPrerequisites(detail, detailSize);
  if (precondition != STRESS_OK)
    return precondition;

  char response[RESPONSE_BUFFER_SIZE + 1] = {};
  size_t stored = 0;
  bool connected = false;
  bool received = false;
  bool timedOut = false;
  uint32_t started = millis();

  if (!lockEthernet(1000))
  {
    copyText(detail, detailSize, "Timeout SPI antes de connect()");
    return STRESS_ERR_SPI_LOCK;
  }

  stressClient.stop();
  connected = stressClient.connect(STRESS_HOST, STRESS_PORT) == 1;
  if (connected)
  {
    stressClient.print("GET ");
    stressClient.print(STRESS_PATH);
    stressClient.println(" HTTP/1.1");
    stressClient.print("Host: ");
    stressClient.println(STRESS_HOST);
    stressClient.println("User-Agent: JWPLC-Basic-Ethernet-Stress");
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
  }
  unlockEthernet();

  latency = millis() - started;
  response[stored] = '\0';

  if (!connected)
  {
    copyText(detail, detailSize,
             "connect() fallo: DNS, ruta, TX, servidor o falso contacto");
    return STRESS_ERR_CONNECT_DNS;
  }
  if (!received)
  {
    copyText(detail, detailSize,
             "TCP conecto pero no llegaron bytes: revisar RX o red");
    return timedOut ? STRESS_ERR_RX_TIMEOUT : STRESS_ERR_NO_RESPONSE;
  }
  if (timedOut)
  {
    copyText(detail, detailSize, "Respuesta no finalizo dentro del timeout");
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
    snprintf(detail, detailSize, "No aparece token esperado: %s", EXPECTED_TOKEN);
    return STRESS_ERR_CONTENT;
  }

  copyText(detail, detailSize, "HTTP y contenido verificados");
  return STRESS_OK;
}

void runTest()
{
  if (!running || busy)
    return;

  busy = true;
  stats.tests++;
  currentError = STRESS_OK;
  copyText(currentResult, sizeof(currentResult), "PROBANDO...");
  copyText(lastStatusLine, sizeof(lastStatusLine), "-");
  requestUi();
  delay(80);

  char detail[80] = {};
  uint16_t code = 0;
  uint32_t bytes = 0;
  uint32_t latency = 0;
  StressError result = executeTransaction(
      detail, sizeof(detail), code, bytes, latency);

  stats.lastCode = code;
  stats.lastBytes = bytes;
  stats.lastLatency = latency;
  stats.totalBytes += bytes;

  if (result == STRESS_OK)
    recordSuccess(code, bytes, latency);
  else
    recordFailure(result, detail);

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

void drawFrame(Adafruit_ST7789 &tft)
{
  static const char *titles[PAGE_COUNT] = {
      "ETH STRESS TEST", "CONTADORES DE FALLA", "ULTIMO ERROR"};
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
  row(tft, 100, "HTTP: ", lastStatusLine);

  clearRow(tft, 116);
  tft.setCursor(6, 116);
  tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
  tft.print("Racha ");
  tft.print(stats.failStreak);
  tft.print("  Max ");
  tft.print(stats.maxFailStreak);
  tft.print("  Lat ");
  tft.print(stats.lastLatency);
  tft.print("ms  RX ");
  tft.print(stats.lastBytes);
  tft.print("B");

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
  tft.print("  ERR:");
  tft.setTextColor(errLedState ? ST77XX_RED : ST77XX_GREEN, ST77XX_BLACK);
  tft.print(errLedState ? "LAT" : "NO");
}

void drawPage1(Adafruit_ST7789 &tft)
{
  clearRow(tft, 38);
  tft.setTextSize(1);
  tft.setCursor(6, 38);
  tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
  tft.print("SPI ");
  tft.print(stats.spi);
  tft.print("  HW ");
  tft.print(stats.noHardware);
  tft.print("  LINK ");
  tft.print(stats.link);
  tft.print("  DHCP/IP ");
  tft.print(stats.dhcpIp);

  clearRow(tft, 58);
  tft.setCursor(6, 58);
  tft.print("NOTRDY ");
  tft.print(stats.notReady);
  tft.print("  CONN ");
  tft.print(stats.connect);
  tft.print("  SINRX ");
  tft.print(stats.noResponse);
  tft.print("  TMO ");
  tft.print(stats.timeout);

  clearRow(tft, 78);
  tft.setCursor(6, 78);
  tft.print("BADHTTP ");
  tft.print(stats.badStatus);
  tft.print("  CODE ");
  tft.print(stats.httpCode);
  tft.print("  DATA ");
  tft.print(stats.content);

  clearRow(tft, 98);
  tft.setCursor(6, 98);
  tft.print("Link OFF/ON ");
  tft.print(stats.linkDrop);
  tft.print("/");
  tft.print(stats.linkRecovery);
  tft.print("  HW OFF/ON ");
  tft.print(stats.hardwareLoss);
  tft.print("/");
  tft.print(stats.hardwareRecovery);

  clearRow(tft, 118);
  tft.setCursor(6, 118);
  tft.print("RX total ");
  tft.print((unsigned long)(stats.totalBytes / 1024ULL));
  tft.print("KiB  Lat min/max ");
  tft.print(stats.minLatency);
  tft.print("/");
  tft.print(stats.maxLatency);
  tft.print("ms");

  clearRow(tft, 138, 11);
  tft.setCursor(6, 138);
  uint32_t average = stats.ok > 0
                         ? (uint32_t)(stats.successfulLatencyTotal / stats.ok)
                         : 0;
  tft.print("Lat promedio ");
  tft.print(average);
  tft.print("ms  Fallas ");
  tft.print(stats.failed);
  tft.print("  RachaMax ");
  tft.print(stats.maxFailStreak);
}

void drawPage2(Adafruit_ST7789 &tft)
{
  row(tft, 38, "Tipo: ", lastErrorName,
      lastError == STRESS_OK ? ST77XX_GREEN : ST77XX_RED);
  row(tft, 56, "Fecha: ", lastErrorTime);

  char uptime[20] = {};
  formatDuration(lastErrorUptimeMs, uptime, sizeof(uptime));
  row(tft, 74, "Uptime: ", uptime);

  char line1[48] = {};
  char line2[48] = {};
  size_t len = strlen(lastErrorDetail);
  size_t first = len < sizeof(line1) - 1 ? len : sizeof(line1) - 1;
  memcpy(line1, lastErrorDetail, first);
  line1[first] = '\0';
  if (len > first)
    copyText(line2, sizeof(line2), lastErrorDetail + first);
  row(tft, 92, "Detalle: ", line1, ST77XX_YELLOW);
  row(tft, 108, "         ", line2, ST77XX_YELLOW);
  row(tft, 124, "HTTP: ", lastStatusLine);

  clearRow(tft, 140, 9);
  tft.setCursor(6, 140);
  tft.setTextColor(errLedState ? ST77XX_RED : ST77XX_GREEN, ST77XX_BLACK);
  tft.print(errLedState ? "ALARMA ACTIVA/LATCHEADA" : "ALARMA RECONOCIDA");
  tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
  tft.print("  ESC=ACK");
}

void drawContent(Adafruit_ST7789 &tft)
{
  if (page == 0)
    drawPage0(tft);
  else if (page == 1)
    drawPage1(tft);
  else
    drawPage2(tft);
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
  Serial.print(" | UPTIME ");
  Serial.print(runtime);
  Serial.print(" | ULT ERR ");
  Serial.println(lastErrorName);
}

void setup()
{
  Serial.begin(115200);
  delay(1200);
  bootMs = millis();
  lastTestMs = bootMs;

  Serial.println();
  Serial.println("JWPLC Basic - Ethernet Continuous Stress TFT");
  Serial.print("Destino: http://");
  Serial.print(STRESS_HOST);
  Serial.print(":");
  Serial.print(STRESS_PORT);
  Serial.println(STRESS_PATH);
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
      stats.spi++;
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
