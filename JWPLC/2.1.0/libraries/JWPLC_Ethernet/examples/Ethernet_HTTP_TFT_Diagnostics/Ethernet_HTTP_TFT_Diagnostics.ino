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
  - La TFT usa fields declarativos con dirty redraw automatico.
  - setText()/setValue() no redibujan cuando el valor no cambia.
  - EthernetClient usa el mutex SPI compartido del JWPLC.
  - El ejemplo escribe su bloque de prueba desde la direccion FRAM 0x0100.
*/

#include <JWPLC_Display.h>

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

void syncDiagnosticUi();

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
    syncDiagnosticUi();
    return false;
  }

  if (!JWPLC_FRAM.writeBlock(FRAM_ADDR, data, FRAM_VERSION))
  {
    framOk = false;
    copyText(framSaveText, sizeof(framSaveText), "ERROR ESCRITURA");
    syncDiagnosticUi();
    return false;
  }

  DiagnosticData check = {};
  if (!JWPLC_FRAM.readBlock(FRAM_ADDR, check, FRAM_VERSION) ||
      memcmp(&data, &check, sizeof(data)) != 0)
  {
    framOk = false;
    copyText(framSaveText, sizeof(framSaveText), "ERROR VERIFICACION");
    syncDiagnosticUi();
    return false;
  }

  data = check;
  framOk = true;
  copyText(framSaveText, sizeof(framSaveText), "GUARDADO OK");
  syncDiagnosticUi();
  return true;
}

void loadData()
{
  framOk = JWPLC_FRAM.size() > 0;
  if (!framOk)
  {
    copyText(framSaveText, sizeof(framSaveText), "NO DISPONIBLE");
    syncDiagnosticUi();
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
    syncDiagnosticUi();
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
    syncDiagnosticUi();
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
    syncDiagnosticUi();
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
    syncDiagnosticUi();
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
    syncDiagnosticUi();
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
  syncDiagnosticUi();
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
  syncDiagnosticUi();
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
  syncDiagnosticUi();

  Serial.print("Button: ");
  Serial.print(lastButton);
  Serial.print(" | Count: ");
  Serial.println(buttonCount[id]);

  switch (id)
  {
  case BTN_LEFT:
    page = page == 0 ? PAGE_COUNT - 1 : page - 1;
    JWPLC_Display.setUserPage(page);
    syncDiagnosticUi();
    break;
  case BTN_RIGHT:
    page = (uint8_t)((page + 1) % PAGE_COUNT);
    JWPLC_Display.setUserPage(page);
    syncDiagnosticUi();
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

enum DiagnosticFieldId : uint8_t
{
  DIAG_P0_TITLE = 1,
  DIAG_P0_USB,
  DIAG_P0_ETH,
  DIAG_P0_IP,
  DIAG_P0_RTC,
  DIAG_P0_FRAM,
  DIAG_P0_HTTP,
  DIAG_P0_RESPONSE,
  DIAG_P0_STATS,
  DIAG_P0_ERROR,

  DIAG_P1_TITLE,
  DIAG_P1_BUTTONS_A,
  DIAG_P1_BUTTONS_B,
  DIAG_P1_LAST,
  DIAG_P1_FRAM,
  DIAG_P1_SAVE,
  DIAG_P1_ERROR,
  DIAG_P1_HELP
};

static const JWPLC_UIField DIAGNOSTIC_FIELDS[] = {
    JWPLC_UITextField(
        DIAG_P0_TITLE, JWPLC_UIRect(6, 4), JWPLC_UIText(nullptr, nullptr, 28),
        JWPLC_UITextFieldStyle(2, 1, false, JWPLC_UI_LAYOUT_INLINE, JWPLC_UI_ALIGN_LEFT),
        0, JWPLC_UIColors(ST77XX_CYAN, ST77XX_CYAN, ST77XX_BLACK, ST77XX_CYAN)),
    JWPLC_UITextField(DIAG_P0_USB, 6, 28, "USB", 24, 0),
    JWPLC_UITextField(DIAG_P0_ETH, 6, 44, "ETH", 31, 0),
    JWPLC_UITextField(DIAG_P0_IP, 6, 60, "IP", 20, 0),
    JWPLC_UITextField(DIAG_P0_RTC, 6, 76, "RTC", 24, 0),
    JWPLC_UITextField(DIAG_P0_FRAM, 6, 92, "FRAM", 31, 0),
    JWPLC_UITextField(DIAG_P0_HTTP, 6, 108, "HTTP", 28, 0),
    JWPLC_UITextField(DIAG_P0_RESPONSE, 6, 124, "Resp", 31, 0),
    JWPLC_UITextField(DIAG_P0_STATS, 6, 140, "GET", 31, 0),
    JWPLC_UITextField(DIAG_P0_ERROR, 6, 156, "ERR", 31, 0),

    JWPLC_UITextField(
        DIAG_P1_TITLE, JWPLC_UIRect(6, 4), JWPLC_UIText(nullptr, nullptr, 28),
        JWPLC_UITextFieldStyle(2, 1, false, JWPLC_UI_LAYOUT_INLINE, JWPLC_UI_ALIGN_LEFT),
        1, JWPLC_UIColors(ST77XX_CYAN, ST77XX_CYAN, ST77XX_BLACK, ST77XX_CYAN)),
    JWPLC_UITextField(DIAG_P1_BUTTONS_A, 6, 34, nullptr, 39, 1),
    JWPLC_UITextField(DIAG_P1_BUTTONS_B, 6, 52, nullptr, 39, 1),
    JWPLC_UITextField(DIAG_P1_LAST, 6, 70, "Ultimo", 16, 1),
    JWPLC_UITextField(DIAG_P1_FRAM, 6, 90, "FRAM", 31, 1),
    JWPLC_UITextField(DIAG_P1_SAVE, 6, 110, "Save", 31, 1),
    JWPLC_UITextField(DIAG_P1_ERROR, 6, 130, "ERR", 31, 1),
    JWPLC_UITextField(DIAG_P1_HELP, 6, 150, nullptr, 34, 1)};

void syncDiagnosticUi()
{
  char line[40] = {};

  JWPLC_Display.setText(DIAG_P0_TITLE, "DIAGNOSTICO HTTP 1/2");
  JWPLC_Display.setText(DIAG_P0_USB, "Serial 115200 activo");
  JWPLC_Display.setText(DIAG_P0_ETH, ethStatus);
  JWPLC_Display.setText(DIAG_P0_IP, ethIp);
  JWPLC_Display.setText(DIAG_P0_RTC, rtcText);

  snprintf(line, sizeof(line), "Boot %lu Val %ld %s",
 (unsigned long)data.bootCount,
 (long)data.framValue,
 framSaveText);
  JWPLC_Display.setText(DIAG_P0_FRAM, line);
  JWPLC_Display.setText(DIAG_P0_HTTP, httpState);
  JWPLC_Display.setText(DIAG_P0_RESPONSE, httpTitle);

  snprintf(line, sizeof(line), "%lu/%lu  %luB",
 (unsigned long)data.successCount,
 (unsigned long)data.requestCount,
 (unsigned long)httpBytes);
  JWPLC_Display.setText(DIAG_P0_STATS, line);
  JWPLC_Display.setText(DIAG_P0_ERROR, errReason);

  JWPLC_Display.setText(DIAG_P1_TITLE, "BOTONERA Y FRAM 2/2");
  snprintf(line, sizeof(line), "LEFT %lu  UP %lu  RIGHT %lu",
 (unsigned long)buttonCount[BTN_LEFT],
 (unsigned long)buttonCount[BTN_UP],
 (unsigned long)buttonCount[BTN_RIGHT]);
  JWPLC_Display.setText(DIAG_P1_BUTTONS_A, line);

  snprintf(line, sizeof(line), "ESC %lu  OK %lu  DOWN %lu",
 (unsigned long)buttonCount[BTN_ESC],
 (unsigned long)buttonCount[BTN_OK],
 (unsigned long)buttonCount[BTN_DOWN]);
  JWPLC_Display.setText(DIAG_P1_BUTTONS_B, line);
  JWPLC_Display.setText(DIAG_P1_LAST, lastButton);

  snprintf(line, sizeof(line), "Valor %ld  Boot %lu",
 (long)data.framValue,
 (unsigned long)data.bootCount);
  JWPLC_Display.setText(DIAG_P1_FRAM, line);
  JWPLC_Display.setText(DIAG_P1_SAVE, framSaveText);
  JWPLC_Display.setText(DIAG_P1_ERROR, errReason);
  JWPLC_Display.setText(DIAG_P1_HELP, "UP/DOWN cambia y guarda");
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
  JWPLC_Display.setUserRefreshMode(USER_REFRESH_ON_DEMAND);
  JWPLC_Display.setRunLed(true);
  JWPLC_Display.setEthLedAuto(true);
  JWPLC_Display.setUserPage(0);

  if (!JWPLC_Display.setFields(
DIAGNOSTIC_FIELDS,
sizeof(DIAGNOSTIC_FIELDS) / sizeof(DIAGNOSTIC_FIELDS[0])))
  {
    Serial.println("ERROR: no se pudieron registrar fields de diagnostico");
  }

  syncDiagnosticUi();
  loadData();
  updateRTC();
  updateEthernet();
  updateIndicators();
  syncDiagnosticUi();
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
