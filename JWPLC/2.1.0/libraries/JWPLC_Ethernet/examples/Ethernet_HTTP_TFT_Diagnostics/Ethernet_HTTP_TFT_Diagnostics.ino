/*
  Ethernet_HTTP_TFT_Diagnostics

  Diagnostico integrado del JWPLC Basic.

  Valida:
  - USB Serial a 115200 baudios.
  - Botonera completa.
  - TFT ST7789 en pantalla USER.
  - RTC mediante JWPLC_RTC.
  - FRAM mediante lectura, escritura y verificacion persistente.
  - Ethernet W5500 automatico por DHCP.
  - GET HTTP a http://example.com/ mediante EthernetClient.

  Controles:
  - OK: ejecuta la consulta HTTP.
  - LEFT / RIGHT: cambia entre las dos paginas.
  - UP / DOWN: modifica y guarda inmediatamente el valor en FRAM.
  - ESC: vuelve a la pantalla IDLE.

  Importante:
  - No llama JWPLC_Ethernet.begin() ni maintain().
  - Ethernet arranca y se mantiene desde el runtime JWPLC.
  - Los callbacks graficos solo dibujan variables cacheadas.
  - La TFT solo redibuja las filas cuyo contenido cambio.
  - EthernetClient usa el mutex SPI compartido del JWPLC.
  - El ejemplo escribe su bloque de prueba desde la direccion FRAM 0x0100.
*/

#include <JWPLC_Display.h>
#include <JWPLC_GlobalPeripherals.h>
#include <Ethernet.h>
#include "jwplc_spi_bus.h"

#include <string.h>
#include <stdlib.h>

static const char HTTP_HOST[] = "example.com";
static const uint16_t HTTP_PORT = 80;
static const uint32_t HTTP_TIMEOUT_MS = 7000;
static const size_t HTTP_BUFFER_SIZE = 1536;

static const uint32_t FRAM_ADDR = 0x0100;
static const uint8_t FRAM_VERSION = 1;
static const uint32_t DATA_MAGIC = 0x4A574854UL;
static const uint8_t PAGE_COUNT = 2;

struct DiagnosticData
{
  uint32_t magic;
  uint16_t schema;
  uint16_t reserved;
  uint32_t bootCount;
  uint32_t requestCount;
  uint32_t successCount;
  int32_t framValue;
  uint16_t lastHttpCode;
  uint16_t reserved2;
  uint32_t lastTestUnix;
};

DiagnosticData data = {};
EthernetClient httpClient;

bool displayReady = false;
bool framOk = false;
bool rtcPresent = false;
bool rtcTimeValid = false;
bool ethOk = false;
bool httpOk = false;
bool httpBusy = false;
bool httpAttempted = false;
bool errLedInitialized = false;
bool errLedState = false;

uint8_t page = 0;
uint32_t buttonCount[BTN_COUNT] = {};
char lastButton[12] = "NINGUNO";
char ethStatus[32] = "Not started";
char ethIp[20] = "0.0.0.0";
char rtcText[24] = "RTC no leido";
char framSaveText[20] = "SIN COMPROBAR";
char httpState[28] = "LISTO - PULSE OK";
char httpLine[64] = "-";
char httpTitle[64] = "-";
char errReason[40] = "NINGUNO";

uint16_t httpCode = 0;
size_t httpBytes = 0;
uint32_t lastEthUpdateMs = 0;
uint32_t lastRtcUpdateMs = 0;
uint32_t lastLogMs = 0;

enum UiDirty : uint32_t
{
  UI_DIRTY_FRAME = 1UL << 0,
  UI_DIRTY_ETH = 1UL << 1,
  UI_DIRTY_RTC = 1UL << 2,
  UI_DIRTY_FRAM = 1UL << 3,
  UI_DIRTY_HTTP = 1UL << 4,
  UI_DIRTY_BUTTONS = 1UL << 5,
  UI_DIRTY_ERROR = 1UL << 6,
  UI_DIRTY_ALL = 0x7FUL
};

portMUX_TYPE uiDirtyMux = portMUX_INITIALIZER_UNLOCKED;
volatile uint32_t uiDirty = UI_DIRTY_ALL;

void markUiDirty(uint32_t mask)
{
  portENTER_CRITICAL(&uiDirtyMux);
  uiDirty |= mask;
  portEXIT_CRITICAL(&uiDirtyMux);
}

uint32_t takeUiDirty()
{
  portENTER_CRITICAL(&uiDirtyMux);
  uint32_t dirty = uiDirty;
  uiDirty = 0;
  portEXIT_CRITICAL(&uiDirtyMux);
  return dirty;
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

bool textChanged(const char *a, const char *b)
{
  if (!a)
    a = "";
  if (!b)
    b = "";
  return strcmp(a, b) != 0;
}

void formatIP(IPAddress ip, char *out, size_t size)
{
  snprintf(out, size, "%u.%u.%u.%u", ip[0], ip[1], ip[2], ip[3]);
}

const char *buttonName(uint8_t id)
{
  switch (id)
  {
  case BTN_LEFT:
    return "LEFT";
  case BTN_UP:
    return "UP";
  case BTN_RIGHT:
    return "RIGHT";
  case BTN_ESC:
    return "ESC";
  case BTN_OK:
    return "OK";
  case BTN_DOWN:
    return "DOWN";
  default:
    return "?";
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

bool saveData()
{
  const size_t total = sizeof(JW_FRAM::BlockHeader) + sizeof(DiagnosticData);

  if (JWPLC_FRAM.size() == 0 ||
      !JWPLC_FRAM.isAddressValid(FRAM_ADDR, total))
  {
    framOk = false;
    copyText(framSaveText, sizeof(framSaveText), "ERROR DE RANGO");
    markUiDirty(UI_DIRTY_FRAM | UI_DIRTY_ERROR);
    return false;
  }

  if (!JWPLC_FRAM.writeBlock(FRAM_ADDR, data, FRAM_VERSION))
  {
    framOk = false;
    copyText(framSaveText, sizeof(framSaveText), "ERROR ESCRITURA");
    markUiDirty(UI_DIRTY_FRAM | UI_DIRTY_ERROR);
    return false;
  }

  DiagnosticData check = {};
  if (!JWPLC_FRAM.readBlock(FRAM_ADDR, check, FRAM_VERSION) ||
      memcmp(&data, &check, sizeof(data)) != 0)
  {
    framOk = false;
    copyText(framSaveText, sizeof(framSaveText), "ERROR VERIFICACION");
    markUiDirty(UI_DIRTY_FRAM | UI_DIRTY_ERROR);
    return false;
  }

  data = check;
  framOk = true;
  copyText(framSaveText, sizeof(framSaveText), "GUARDADO OK");
  markUiDirty(UI_DIRTY_FRAM | UI_DIRTY_ERROR);
  return true;
}

void loadData()
{
  framOk = JWPLC_FRAM.size() > 0;
  if (!framOk)
  {
    copyText(framSaveText, sizeof(framSaveText), "NO DISPONIBLE");
    markUiDirty(UI_DIRTY_FRAM | UI_DIRTY_ERROR);
    Serial.println("FRAM: no disponible");
    return;
  }

  DiagnosticData stored = {};
  bool valid = JWPLC_FRAM.readBlock(FRAM_ADDR, stored, FRAM_VERSION);

  if (valid && stored.magic == DATA_MAGIC && stored.schema == FRAM_VERSION)
  {
    data = stored;
    Serial.println("FRAM: bloque persistente recuperado");
  }
  else
  {
    memset(&data, 0, sizeof(data));
    data.magic = DATA_MAGIC;
    data.schema = FRAM_VERSION;
    Serial.println("FRAM: creando bloque de diagnostico");
  }

  data.bootCount++;
  if (saveData())
  {
    Serial.print("FRAM: OK | Boot #");
    Serial.println(data.bootCount);
  }
  else
  {
    Serial.println("FRAM: fallo de escritura/verificacion");
  }
}

void updateRTC()
{
  bool oldPresent = rtcPresent;
  bool oldValid = rtcTimeValid;
  char oldText[sizeof(rtcText)] = {};
  copyText(oldText, sizeof(oldText), rtcText);

  JWRTCDateTime now = JWPLC_RTC.now();
  rtcTimeValid = now.valid;
  rtcPresent = now.valid || JWPLC_RTC.isPresent();

  if (!rtcPresent)
    copyText(rtcText, sizeof(rtcText), "RTC AUSENTE");
  else if (!rtcTimeValid)
    copyText(rtcText, sizeof(rtcText), "HORA INVALIDA");
  else
    snprintf(rtcText, sizeof(rtcText),
             "%04u-%02u-%02u %02u:%02u:%02u",
             now.year, now.month, now.day,
             now.hour, now.minute, now.second);

  if (oldPresent != rtcPresent ||
      oldValid != rtcTimeValid ||
      textChanged(oldText, rtcText))
  {
    markUiDirty(UI_DIRTY_RTC | UI_DIRTY_ERROR);
  }
}

void updateEthernet()
{
  char newStatus[sizeof(ethStatus)] = {};
  char newIp[sizeof(ethIp)] = {};
  copyText(newStatus, sizeof(newStatus), JWPLC_Ethernet.statusString());
  formatIP(JWPLC_Ethernet.localIP(), newIp, sizeof(newIp));

  bool newOk = JWPLC_Ethernet.isEnabled() &&
               JWPLC_Ethernet.isReady() &&
               JWPLC_Ethernet.linkUp();

  if (newOk != ethOk ||
      textChanged(newStatus, ethStatus) ||
      textChanged(newIp, ethIp))
  {
    ethOk = newOk;
    copyText(ethStatus, sizeof(ethStatus), newStatus);
    copyText(ethIp, sizeof(ethIp), newIp);
    markUiDirty(UI_DIRTY_ETH);
  }
}

void buildErrorReason(bool httpFault)
{
  char newReason[sizeof(errReason)] = {};

  if (!framOk && !rtcPresent && httpFault)
    copyText(newReason, sizeof(newReason), "FRAM + RTC + HTTP");
  else if (!framOk && !rtcPresent)
    copyText(newReason, sizeof(newReason), "FRAM + RTC");
  else if (!framOk && httpFault)
    copyText(newReason, sizeof(newReason), "FRAM + HTTP");
  else if (!rtcPresent && httpFault)
    copyText(newReason, sizeof(newReason), "RTC + HTTP");
  else if (!framOk)
    copyText(newReason, sizeof(newReason), "FRAM");
  else if (!rtcPresent)
    copyText(newReason, sizeof(newReason), "RTC AUSENTE");
  else if (httpFault)
    copyText(newReason, sizeof(newReason), "HTTP");
  else
    copyText(newReason, sizeof(newReason), "NINGUNO");

  if (textChanged(newReason, errReason))
  {
    copyText(errReason, sizeof(errReason), newReason);
    markUiDirty(UI_DIRTY_ERROR);
  }
}

void updateIndicators()
{
  bool httpFault = httpAttempted && !httpBusy && !httpOk;
  bool newErrState = !framOk || !rtcPresent || httpFault;
  buildErrorReason(httpFault);

  if (!errLedInitialized || newErrState != errLedState)
  {
    errLedInitialized = true;
    errLedState = newErrState;
    JWPLC_Display.setErrLed(errLedState);
    markUiDirty(UI_DIRTY_ERROR);
  }
}

void parseResponse(char *response)
{
  copyText(httpLine, sizeof(httpLine), "-");
  copyText(httpTitle, sizeof(httpTitle), "-");
  httpCode = 0;

  char *lineEnd = strstr(response, "\r\n");
  if (lineEnd)
  {
    size_t length = (size_t)(lineEnd - response);
    if (length >= sizeof(httpLine))
      length = sizeof(httpLine) - 1;
    memcpy(httpLine, response, length);
    httpLine[length] = '\0';
  }

  char *space = strchr(httpLine, ' ');
  if (space)
    httpCode = (uint16_t)atoi(space + 1);

  char *titleStart = strstr(response, "<title>");
  if (titleStart)
  {
    titleStart += 7;
    char *titleEnd = strstr(titleStart, "</title>");
    if (titleEnd)
    {
      size_t length = (size_t)(titleEnd - titleStart);
      if (length >= sizeof(httpTitle))
        length = sizeof(httpTitle) - 1;
      memcpy(httpTitle, titleStart, length);
      httpTitle[length] = '\0';
    }
  }

  httpOk = httpCode >= 200 && httpCode < 400;
}

void storeHttpResult()
{
  data.lastHttpCode = httpCode;
  uint32_t unixTime = 0;
  data.lastTestUnix = JWPLC_RTC.readUnix(unixTime) ? unixTime : 0;
  if (httpOk)
    data.successCount++;
  saveData();
}

void runHttpTest()
{
  if (httpBusy)
    return;

  if (!ethOk)
  {
    httpAttempted = true;
    httpOk = false;
    httpCode = 0;
    copyText(httpState, sizeof(httpState), "ETHERNET NO LISTO");
    copyText(httpLine, sizeof(httpLine), "-");
    copyText(httpTitle, sizeof(httpTitle), "-");
    page = 0;
    markUiDirty(UI_DIRTY_FRAME | UI_DIRTY_HTTP | UI_DIRTY_ERROR);
    updateIndicators();
    Serial.println("HTTP: Ethernet no esta listo");
    return;
  }

  httpAttempted = true;
  httpBusy = true;
  httpOk = false;
  httpCode = 0;
  httpBytes = 0;
  data.requestCount++;
  copyText(httpState, sizeof(httpState), "CONECTANDO...");
  copyText(httpLine, sizeof(httpLine), "-");
  copyText(httpTitle, sizeof(httpTitle), "-");
  page = 0;
  markUiDirty(UI_DIRTY_FRAME | UI_DIRTY_HTTP | UI_DIRTY_FRAM | UI_DIRTY_ERROR);
  updateIndicators();

  Serial.println();
  Serial.println("HTTP: GET http://example.com/");
  delay(180);

  char response[HTTP_BUFFER_SIZE + 1] = {};
  size_t stored = 0;
  bool connected = false;
  bool received = false;
  bool busLocked = lockEthernet(1000);

  if (busLocked)
  {
    connected = httpClient.connect(HTTP_HOST, HTTP_PORT) == 1;
    if (connected)
    {
      httpClient.println("GET / HTTP/1.1");
      httpClient.println("Host: example.com");
      httpClient.println("User-Agent: JWPLC-Basic-Diagnostic");
      httpClient.println("Accept: text/html");
      httpClient.println("Connection: close");
      httpClient.println();

      uint32_t started = millis();
      while ((uint32_t)(millis() - started) < HTTP_TIMEOUT_MS)
      {
        while (httpClient.available() > 0)
        {
          int value = httpClient.read();
          if (value < 0)
            break;
          received = true;
          httpBytes++;
          if (stored < HTTP_BUFFER_SIZE)
            response[stored++] = (char)value;
        }

        if (!httpClient.connected() && httpClient.available() == 0)
          break;
        delay(2);
      }
      httpClient.stop();
    }
    unlockEthernet();
  }

  response[stored] = '\0';

  if (!busLocked)
    copyText(httpState, sizeof(httpState), "SPI LOCK TIMEOUT");
  else if (!connected)
    copyText(httpState, sizeof(httpState), "FALLO CONNECT/DNS");
  else if (!received)
    copyText(httpState, sizeof(httpState), "SIN RESPUESTA HTTP");
  else
  {
    parseResponse(response);
    if (httpOk)
      snprintf(httpState, sizeof(httpState), "HTTP %u OK", httpCode);
    else
      snprintf(httpState, sizeof(httpState), "HTTP %u ERROR", httpCode);
  }

  if (!received)
  {
    httpOk = false;
    httpCode = 0;
    copyText(httpLine, sizeof(httpLine), "-");
    copyText(httpTitle, sizeof(httpTitle), "-");
  }

  storeHttpResult();
  httpBusy = false;
  markUiDirty(UI_DIRTY_HTTP | UI_DIRTY_FRAM | UI_DIRTY_ERROR);
  updateIndicators();

  Serial.println("=== Resultado HTTP ===");
  Serial.print("Estado: ");
  Serial.println(httpState);
  Serial.print("Linea: ");
  Serial.println(httpLine);
  Serial.print("Titulo: ");
  Serial.println(httpTitle);
  Serial.print("Bytes: ");
  Serial.println(httpBytes);
  Serial.print("FRAM GET/OK: ");
  Serial.print(data.requestCount);
  Serial.print("/");
  Serial.println(data.successCount);
}

void changeFramValue(int32_t delta)
{
  data.framValue += delta;
  if (data.framValue < -9999)
    data.framValue = -9999;
  if (data.framValue > 9999)
    data.framValue = 9999;

  bool saved = saveData();
  updateIndicators();
  Serial.print("FRAM value: ");
  Serial.print(data.framValue);
  Serial.print(" | ");
  Serial.println(saved ? "guardado y verificado" : "ERROR al guardar");
}

void handleButton(uint8_t id)
{
  buttonCount[id]++;
  copyText(lastButton, sizeof(lastButton), buttonName(id));
  markUiDirty(UI_DIRTY_BUTTONS);

  Serial.print("Button: ");
  Serial.print(lastButton);
  Serial.print(" | Count: ");
  Serial.println(buttonCount[id]);

  switch (id)
  {
  case BTN_LEFT:
    page = page == 0 ? PAGE_COUNT - 1 : page - 1;
    markUiDirty(UI_DIRTY_FRAME | UI_DIRTY_ALL);
    break;
  case BTN_RIGHT:
    page = (uint8_t)((page + 1) % PAGE_COUNT);
    markUiDirty(UI_DIRTY_FRAME | UI_DIRTY_ALL);
    break;
  case BTN_UP:
    changeFramValue(1);
    break;
  case BTN_DOWN:
    changeFramValue(-1);
    break;
  case BTN_OK:
    runHttpTest();
    break;
  case BTN_ESC:
    JWPLC_Display.goIdle();
    break;
  }
}

void readButtons()
{
  if (!displayReady || JWPLC_Display.isIdleMode() ||
      !JWPLCButtons::isReady() || httpBusy)
    return;

  for (uint8_t id = 0; id < BTN_COUNT; id++)
  {
    if (JWPLC_Buttons.pressed(id))
      handleButton(id);
  }
}

uint16_t statusColor(bool ok)
{
  return ok ? ST77XX_GREEN : ST77XX_RED;
}

uint16_t rtcStatusColor()
{
  if (!rtcPresent)
    return ST77XX_RED;
  return rtcTimeValid ? ST77XX_GREEN : ST77XX_YELLOW;
}

void clearRow(Adafruit_ST7789 &tft, int16_t y, int16_t height = 13)
{
  tft.fillRect(0, y - 2, 320, height, ST77XX_BLACK);
}

void drawFrame(Adafruit_ST7789 &tft)
{
  tft.fillScreen(ST77XX_BLACK);
  tft.setTextWrap(false);
  tft.setTextSize(2);
  tft.setTextColor(ST77XX_CYAN, ST77XX_BLACK);
  tft.setCursor(8, 7);
  tft.print(page == 0 ? "DIAGNOSTICO HTTP" : "BOTONERA Y FRAM");

  tft.setTextSize(1);
  tft.setTextColor(ST77XX_YELLOW, ST77XX_BLACK);
  tft.setCursor(286, 10);
  tft.print(page + 1);
  tft.print("/");
  tft.print(PAGE_COUNT);
  tft.drawFastHLine(0, 29, 320, ST77XX_BLUE);
  tft.drawFastHLine(0, 148, 320, ST77XX_BLUE);
  tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
  tft.setCursor(5, 157);
  tft.print("OK=GET  </>=PAG  UP/DN=GUARDA  ESC=IDLE");
}

void drawUsbRow(Adafruit_ST7789 &tft)
{
  clearRow(tft, 36);
  tft.setTextSize(1);
  tft.setCursor(6, 36);
  tft.setTextColor(ST77XX_GREEN, ST77XX_BLACK);
  tft.print("USB:");
  tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
  tft.print(" Serial 115200 activo");
}

void drawEthernetRow(Adafruit_ST7789 &tft)
{
  clearRow(tft, 52);
  tft.setTextSize(1);
  tft.setCursor(6, 52);
  tft.setTextColor(statusColor(ethOk), ST77XX_BLACK);
  tft.print("ETH:");
  tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
  tft.print(" ");
  tft.print(ethStatus);
  tft.print("  ");
  tft.print(ethIp);
}

void drawRtcRow(Adafruit_ST7789 &tft)
{
  clearRow(tft, 68);
  tft.setTextSize(1);
  tft.setCursor(6, 68);
  tft.setTextColor(rtcStatusColor(), ST77XX_BLACK);
  tft.print("RTC:");
  tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
  tft.print(" ");
  tft.print(rtcText);
}

void drawFramHttpRow(Adafruit_ST7789 &tft)
{
  clearRow(tft, 84);
  tft.setTextSize(1);
  tft.setCursor(6, 84);
  tft.setTextColor(statusColor(framOk), ST77XX_BLACK);
  tft.print("FRAM:");
  tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
  tft.print(" Boot ");
  tft.print(data.bootCount);
  tft.print("  Valor ");
  tft.print(data.framValue);
  tft.print("  ");
  tft.print(framSaveText);
}

void drawHttpStateRow(Adafruit_ST7789 &tft)
{
  clearRow(tft, 100);
  tft.setTextSize(1);
  tft.setCursor(6, 100);
  uint16_t color = ST77XX_WHITE;
  if (httpBusy)
    color = ST77XX_YELLOW;
  else if (httpAttempted)
    color = statusColor(httpOk);
  tft.setTextColor(color, ST77XX_BLACK);
  tft.print("HTTP:");
  tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
  tft.print(" ");
  tft.print(httpState);
}

void drawHttpTitleRow(Adafruit_ST7789 &tft)
{
  clearRow(tft, 116);
  tft.setTextSize(1);
  tft.setCursor(6, 116);
  tft.setTextColor(ST77XX_YELLOW, ST77XX_BLACK);
  tft.print("Titulo:");
  tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
  tft.print(" ");
  tft.print(httpTitle);
}

void drawHttpStatsRow(Adafruit_ST7789 &tft)
{
  clearRow(tft, 132);
  tft.setTextSize(1);
  tft.setCursor(6, 132);
  tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
  tft.print("GET ");
  tft.print(data.successCount);
  tft.print("/");
  tft.print(data.requestCount);
  tft.print("  Bytes ");
  tft.print(httpBytes);
  tft.print("  ERR: ");
  tft.setTextColor(errLedState ? ST77XX_RED : ST77XX_GREEN, ST77XX_BLACK);
  tft.print(errReason);
}

void drawButtonCounts(Adafruit_ST7789 &tft)
{
  clearRow(tft, 38, 16);
  clearRow(tft, 60, 16);
  tft.setTextSize(1);
  tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
  tft.setCursor(8, 38);
  tft.print("LEFT ");
  tft.print(buttonCount[BTN_LEFT]);
  tft.setCursor(112, 38);
  tft.print("UP ");
  tft.print(buttonCount[BTN_UP]);
  tft.setCursor(210, 38);
  tft.print("RIGHT ");
  tft.print(buttonCount[BTN_RIGHT]);
  tft.setCursor(8, 60);
  tft.print("ESC ");
  tft.print(buttonCount[BTN_ESC]);
  tft.setCursor(112, 60);
  tft.print("OK ");
  tft.print(buttonCount[BTN_OK]);
  tft.setCursor(210, 60);
  tft.print("DOWN ");
  tft.print(buttonCount[BTN_DOWN]);
}

void drawLastButtonRow(Adafruit_ST7789 &tft)
{
  clearRow(tft, 84);
  tft.setTextSize(1);
  tft.setCursor(8, 84);
  tft.setTextColor(ST77XX_YELLOW, ST77XX_BLACK);
  tft.print("Ultimo boton:");
  tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
  tft.print(" ");
  tft.print(lastButton);
}

void drawFramValueRow(Adafruit_ST7789 &tft)
{
  clearRow(tft, 102);
  tft.setTextSize(1);
  tft.setCursor(8, 102);
  tft.setTextColor(statusColor(framOk), ST77XX_BLACK);
  tft.print("Valor FRAM:");
  tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
  tft.print(" ");
  tft.print(data.framValue);
  tft.print("  UP/DOWN cambia y guarda");
}

void drawFramSaveRow(Adafruit_ST7789 &tft)
{
  clearRow(tft, 120);
  tft.setTextSize(1);
  tft.setCursor(8, 120);
  tft.setTextColor(statusColor(framOk), ST77XX_BLACK);
  tft.print("Persistencia:");
  tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
  tft.print(" ");
  tft.print(framSaveText);
  tft.print("  Boot ");
  tft.print(data.bootCount);
}

void drawErrorRow(Adafruit_ST7789 &tft)
{
  clearRow(tft, 138, 11);
  tft.setTextSize(1);
  tft.setCursor(8, 138);
  tft.setTextColor(errLedState ? ST77XX_RED : ST77XX_GREEN, ST77XX_BLACK);
  tft.print("ERR:");
  tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
  tft.print(" ");
  tft.print(errReason);
}

void drawPage0(Adafruit_ST7789 &tft, uint32_t dirty)
{
  if (dirty & UI_DIRTY_FRAME)
    drawUsbRow(tft);
  if (dirty & (UI_DIRTY_FRAME | UI_DIRTY_ETH))
    drawEthernetRow(tft);
  if (dirty & (UI_DIRTY_FRAME | UI_DIRTY_RTC))
    drawRtcRow(tft);
  if (dirty & (UI_DIRTY_FRAME | UI_DIRTY_FRAM))
    drawFramHttpRow(tft);
  if (dirty & (UI_DIRTY_FRAME | UI_DIRTY_HTTP))
  {
    drawHttpStateRow(tft);
    drawHttpTitleRow(tft);
  }
  if (dirty & (UI_DIRTY_FRAME | UI_DIRTY_HTTP | UI_DIRTY_FRAM | UI_DIRTY_ERROR))
    drawHttpStatsRow(tft);
}

void drawPage1(Adafruit_ST7789 &tft, uint32_t dirty)
{
  if (dirty & (UI_DIRTY_FRAME | UI_DIRTY_BUTTONS))
  {
    drawButtonCounts(tft);
    drawLastButtonRow(tft);
  }
  if (dirty & (UI_DIRTY_FRAME | UI_DIRTY_FRAM))
  {
    drawFramValueRow(tft);
    drawFramSaveRow(tft);
  }
  if (dirty & (UI_DIRTY_FRAME | UI_DIRTY_ERROR))
    drawErrorRow(tft);
}

extern "C" void jwplcUserDisplayEnterCallback()
{
  auto &tft = JWPLC_Display.tft();
  markUiDirty(UI_DIRTY_ALL);
  uint32_t dirty = takeUiDirty();
  drawFrame(tft);
  if (page == 0)
    drawPage0(tft, dirty | UI_DIRTY_FRAME);
  else
    drawPage1(tft, dirty | UI_DIRTY_FRAME);
}

extern "C" void jwplcUserDisplayRefreshCallback(
    const JWPLC_IOState *io,
    const JWPLC_RTCState *rtc)
{
  (void)io;
  (void)rtc;
  uint32_t dirty = takeUiDirty();
  if (dirty == 0)
    return;

  auto &tft = JWPLC_Display.tft();
  if (dirty & UI_DIRTY_FRAME)
    drawFrame(tft);
  if (page == 0)
    drawPage0(tft, dirty);
  else
    drawPage1(tft, dirty);
}

extern "C" void jwplcUserDisplayExitCallback()
{
  Serial.println("Display: USER -> IDLE");
}

void printStatus()
{
  Serial.print("ETH: ");
  Serial.print(ethStatus);
  Serial.print(" | IP: ");
  Serial.print(ethIp);
  Serial.print(" | RTC: ");
  Serial.print(rtcText);
  Serial.print(" | FRAM: ");
  Serial.print(framOk ? framSaveText : "ERROR");
  Serial.print(" | HTTP: ");
  Serial.print(httpState);
  Serial.print(" | GET OK: ");
  Serial.print(data.successCount);
  Serial.print("/");
  Serial.print(data.requestCount);
  Serial.print(" | ERR: ");
  Serial.println(errReason);
}

void setup()
{
  Serial.begin(115200);
  delay(1200);

  Serial.println();
  Serial.println("JWPLC Basic - Ethernet HTTP TFT Diagnostics");
  Serial.println("Host: http://example.com/");
  Serial.println("OK=GET | LEFT/RIGHT=pagina | UP/DOWN=cambia+guarda FRAM | ESC=IDLE");
  Serial.println("Ethernet automatico: no se llama begin() ni maintain().");

  JWPLC_Display.setIdleWakeMode(IDLE_WAKE_ANY_BUTTON);
  JWPLC_Display.setIdleReturnMode(IDLE_RETURN_DISABLED);
  JWPLC_Display.setUserRefreshPeriodMs(100);
  JWPLC_Display.setRunLed(true);
  JWPLC_Display.setEthLedAuto(true);

  loadData();
  updateRTC();
  updateEthernet();
  updateIndicators();
  markUiDirty(UI_DIRTY_ALL);
}

void loop()
{
  uint32_t now = millis();

  if (!displayReady && JWPLC_Display.isReady())
  {
    displayReady = true;
    JWPLC_Display.enterUserUI();
    Serial.println("Display: diagnostico USER activo");
  }

  readButtons();

  if ((uint32_t)(now - lastEthUpdateMs) >= 1000)
  {
    lastEthUpdateMs = now;
    updateEthernet();
    updateIndicators();
  }

  if ((uint32_t)(now - lastRtcUpdateMs) >= 1000)
  {
    lastRtcUpdateMs = now;
    updateRTC();
    updateIndicators();
  }

  if ((uint32_t)(now - lastLogMs) >= 5000)
  {
    lastLogMs = now;
    printStatus();
  }

  delay(5);
}
