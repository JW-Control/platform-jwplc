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
  - UP / DOWN: modifica un valor persistente en FRAM.
  - ESC: vuelve a la pantalla IDLE.

  Importante:
  - No llama JWPLC_Ethernet.begin() ni maintain().
  - Ethernet arranca y se mantiene desde el runtime JWPLC.
  - Los callbacks graficos solo dibujan variables cacheadas.
  - EthernetClient usa el mutex SPI compartido del JWPLC.
  - El ejemplo escribe su bloque de prueba desde la direccion FRAM 0x0100.
*/

#include <JWPLC_Display.h>
#include <JWPLC_GlobalPeripherals.h>
#include <Ethernet.h>
#include "jwplc_spi_bus.h"

#include <string.h>
#include <stdlib.h>

// =====================================================
// Configuracion
// =====================================================

static const char HTTP_HOST[] = "example.com";
static const uint16_t HTTP_PORT = 80;
static const uint32_t HTTP_TIMEOUT_MS = 7000;
static const size_t HTTP_BUFFER_SIZE = 1536;

static const uint32_t FRAM_ADDR = 0x0100;
static const uint8_t FRAM_VERSION = 1;
static const uint32_t DATA_MAGIC = 0x4A574854UL; // "JWHT"

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

// =====================================================
// Estado cacheado para Serial y TFT
// =====================================================

bool displayReady = false;
bool framOk = false;
bool rtcOk = false;
bool ethOk = false;
bool httpOk = false;
bool httpBusy = false;
bool httpAttempted = false;

uint8_t page = 0;
bool fullRedraw = true;

uint32_t buttonCount[BTN_COUNT] = {};
char lastButton[12] = "NINGUNO";

char ethStatus[32] = "Not started";
char ethIp[20] = "0.0.0.0";
char rtcText[24] = "RTC no leido";
char httpState[28] = "LISTO - PULSE OK";
char httpLine[64] = "-";
char httpTitle[64] = "-";

uint16_t httpCode = 0;
size_t httpBytes = 0;

uint32_t lastEthUpdateMs = 0;
uint32_t lastRtcUpdateMs = 0;
uint32_t lastLogMs = 0;

// =====================================================
// Helpers
// =====================================================

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

// =====================================================
// FRAM
// =====================================================

bool saveData()
{
  const size_t total = sizeof(JW_FRAM::BlockHeader) + sizeof(DiagnosticData);

  if (JWPLC_FRAM.size() == 0 ||
      !JWPLC_FRAM.isAddressValid(FRAM_ADDR, total))
  {
    framOk = false;
    return false;
  }

  if (!JWPLC_FRAM.writeBlock(FRAM_ADDR, data, FRAM_VERSION))
  {
    framOk = false;
    return false;
  }

  DiagnosticData check = {};

  if (!JWPLC_FRAM.readBlock(FRAM_ADDR, check, FRAM_VERSION) ||
      memcmp(&data, &check, sizeof(data)) != 0)
  {
    framOk = false;
    return false;
  }

  data = check;
  framOk = true;
  return true;
}

void loadData()
{
  framOk = JWPLC_FRAM.size() > 0;

  if (!framOk)
  {
    Serial.println("FRAM: no disponible");
    return;
  }

  DiagnosticData stored = {};
  bool valid = JWPLC_FRAM.readBlock(FRAM_ADDR, stored, FRAM_VERSION);

  if (valid &&
      stored.magic == DATA_MAGIC &&
      stored.schema == FRAM_VERSION)
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

// =====================================================
// Estado de RTC y Ethernet
// =====================================================

void updateRTC()
{
  JWRTCDateTime now = JWPLC_RTC.now();
  rtcOk = now.valid;

  if (!rtcOk)
  {
    copyText(rtcText, sizeof(rtcText), "RTC INVALIDA");
    return;
  }

  snprintf(
      rtcText, sizeof(rtcText),
      "%04u-%02u-%02u %02u:%02u:%02u",
      now.year, now.month, now.day,
      now.hour, now.minute, now.second);
}

void updateEthernet()
{
  copyText(ethStatus, sizeof(ethStatus), JWPLC_Ethernet.statusString());
  formatIP(JWPLC_Ethernet.localIP(), ethIp, sizeof(ethIp));

  ethOk =
      JWPLC_Ethernet.isEnabled() &&
      JWPLC_Ethernet.isReady() &&
      JWPLC_Ethernet.linkUp();
}

void updateIndicators()
{
  bool httpFault = httpAttempted && !httpBusy && !httpOk;
  JWPLC_Display.setErrLed(!framOk || !rtcOk || httpFault);
}

// =====================================================
// HTTP
// =====================================================

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
    fullRedraw = true;
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
  fullRedraw = true;
  JWPLC_Display.forceRedraw();

  Serial.println();
  Serial.println("HTTP: GET http://example.com/");
  delay(250); // Permite mostrar "CONECTANDO..." antes de ocupar el bus SPI.

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
  {
    copyText(httpState, sizeof(httpState), "SPI LOCK TIMEOUT");
  }
  else if (!connected)
  {
    copyText(httpState, sizeof(httpState), "FALLO CONNECT/DNS");
  }
  else if (!received)
  {
    copyText(httpState, sizeof(httpState), "SIN RESPUESTA HTTP");
  }
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
  fullRedraw = true;
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

// =====================================================
// Botonera
// =====================================================

void changeFramValue(int32_t delta)
{
  data.framValue += delta;

  if (data.framValue < -9999)
    data.framValue = -9999;
  if (data.framValue > 9999)
    data.framValue = 9999;

  saveData();

  Serial.print("FRAM value: ");
  Serial.println(data.framValue);
}

void handleButton(uint8_t id)
{
  buttonCount[id]++;
  copyText(lastButton, sizeof(lastButton), buttonName(id));

  Serial.print("Button: ");
  Serial.print(lastButton);
  Serial.print(" | Count: ");
  Serial.println(buttonCount[id]);

  switch (id)
  {
  case BTN_LEFT:
    page = page == 0 ? PAGE_COUNT - 1 : page - 1;
    fullRedraw = true;
    break;

  case BTN_RIGHT:
    page = (uint8_t)((page + 1) % PAGE_COUNT);
    fullRedraw = true;
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
  if (!displayReady ||
      JWPLC_Display.isIdleMode() ||
      !JWPLCButtons::isReady() ||
      httpBusy)
  {
    return;
  }

  for (uint8_t id = 0; id < BTN_COUNT; id++)
  {
    if (JWPLC_Buttons.pressed(id))
      handleButton(id);
  }
}

// =====================================================
// TFT
// =====================================================

uint16_t statusColor(bool ok)
{
  return ok ? ST77XX_GREEN : ST77XX_RED;
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
  tft.print("OK=GET  </>=PAG  UP/DN=FRAM  ESC=IDLE");
}

void drawHttpPage(Adafruit_ST7789 &tft)
{
  tft.fillRect(0, 31, 320, 116, ST77XX_BLACK);
  tft.setTextSize(1);

  tft.setCursor(6, 36);
  tft.setTextColor(ST77XX_GREEN, ST77XX_BLACK);
  tft.print("USB:");
  tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
  tft.print(" Serial 115200 activo");

  tft.setCursor(6, 52);
  tft.setTextColor(statusColor(ethOk), ST77XX_BLACK);
  tft.print("ETH:");
  tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
  tft.print(" ");
  tft.print(ethStatus);
  tft.print("  ");
  tft.print(ethIp);

  tft.setCursor(6, 68);
  tft.setTextColor(statusColor(rtcOk), ST77XX_BLACK);
  tft.print("RTC:");
  tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
  tft.print(" ");
  tft.print(rtcText);

  tft.setCursor(6, 84);
  tft.setTextColor(statusColor(framOk), ST77XX_BLACK);
  tft.print("FRAM:");
  tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
  tft.print(" Boot ");
  tft.print(data.bootCount);
  tft.print("  Valor ");
  tft.print(data.framValue);

  tft.setCursor(6, 100);
  uint16_t httpTextColor = ST77XX_WHITE;
  if (httpBusy)
    httpTextColor = ST77XX_YELLOW;
  else if (httpAttempted)
    httpTextColor = statusColor(httpOk);

  tft.setTextColor(httpTextColor, ST77XX_BLACK);
  tft.print("HTTP:");
  tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
  tft.print(" ");
  tft.print(httpState);

  tft.setCursor(6, 116);
  tft.setTextColor(ST77XX_YELLOW, ST77XX_BLACK);
  tft.print("Titulo:");
  tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
  tft.print(" ");
  tft.print(httpTitle);

  tft.setCursor(6, 132);
  tft.print("GET ");
  tft.print(data.requestCount);
  tft.print("  OK ");
  tft.print(data.successCount);
  tft.print("  Bytes ");
  tft.print(httpBytes);
}

void drawButtonPage(Adafruit_ST7789 &tft)
{
  tft.fillRect(0, 31, 320, 116, ST77XX_BLACK);
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

  tft.setCursor(8, 84);
  tft.setTextColor(ST77XX_YELLOW, ST77XX_BLACK);
  tft.print("Ultimo:");
  tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
  tft.print(" ");
  tft.print(lastButton);

  tft.setCursor(8, 104);
  tft.setTextColor(statusColor(framOk), ST77XX_BLACK);
  tft.print("Valor FRAM:");
  tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
  tft.print(" ");
  tft.print(data.framValue);
  tft.print("  (UP/DOWN)");

  tft.setCursor(8, 124);
  tft.setTextColor(ST77XX_CYAN, ST77XX_BLACK);
  tft.print("Host HTTP:");
  tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
  tft.print(" example.com");
}

void drawCurrentPage(Adafruit_ST7789 &tft)
{
  if (page == 0)
    drawHttpPage(tft);
  else
    drawButtonPage(tft);
}

extern "C" void jwplcUserDisplayEnterCallback()
{
  auto &tft = JWPLC_Display.tft();
  fullRedraw = true;
  drawFrame(tft);
  drawCurrentPage(tft);
  fullRedraw = false;
}

extern "C" void jwplcUserDisplayRefreshCallback(
    const JWPLC_IOState *io,
    const JWPLC_RTCState *rtc)
{
  (void)io;
  (void)rtc;

  auto &tft = JWPLC_Display.tft();

  if (fullRedraw)
  {
    drawFrame(tft);
    fullRedraw = false;
  }

  drawCurrentPage(tft);
}

extern "C" void jwplcUserDisplayExitCallback()
{
  Serial.println("Display: USER -> IDLE");
}

// =====================================================
// Serial
// =====================================================

void printStatus()
{
  Serial.print("ETH: ");
  Serial.print(ethStatus);
  Serial.print(" | IP: ");
  Serial.print(ethIp);
  Serial.print(" | RTC: ");
  Serial.print(rtcText);
  Serial.print(" | FRAM: ");
  Serial.print(framOk ? "OK" : "ERROR");
  Serial.print(" | HTTP: ");
  Serial.print(httpState);
  Serial.print(" | GET OK: ");
  Serial.print(data.successCount);
  Serial.print("/");
  Serial.println(data.requestCount);
}

// =====================================================
// Arduino
// =====================================================

void setup()
{
  Serial.begin(115200);
  delay(1200);

  Serial.println();
  Serial.println("JWPLC Basic - Ethernet HTTP TFT Diagnostics");
  Serial.println("Host: http://example.com/");
  Serial.println("OK=GET | LEFT/RIGHT=pagina | UP/DOWN=FRAM | ESC=IDLE");
  Serial.println("Ethernet automatico: no se llama begin() ni maintain().");

  JWPLC_Display.setIdleWakeMode(IDLE_WAKE_ANY_BUTTON);

  // ESC se procesa aqui para contabilizarlo antes de volver a IDLE.
  JWPLC_Display.setIdleReturnMode(IDLE_RETURN_DISABLED);
  JWPLC_Display.setUserRefreshPeriodMs(250);
  JWPLC_Display.setRunLed(true);
  JWPLC_Display.setEthLedAuto(true);

  loadData();
  updateRTC();
  updateEthernet();
  updateIndicators();
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

  if ((uint32_t)(now - lastEthUpdateMs) >= 500)
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
