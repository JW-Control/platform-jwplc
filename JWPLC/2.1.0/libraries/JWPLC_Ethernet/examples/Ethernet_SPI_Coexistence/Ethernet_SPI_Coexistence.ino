/*
  Ethernet_SPI_Coexistence

  Test de coexistencia SPI para JWPLC Basic.

  Valida:
  - Ethernet W5500 automático por runtime.
  - Display ST7789.
  - FRAM SPI.
  - microSD SPI.
  - Indicadores ETH / ERR en pantalla IDLE.
  - Pantalla USER con resumen de estado.

  Importante:
  - No incluye JWPLC_Ethernet manualmente.
  - Usa fields declarativos de JWPLC_Display; no dibuja la TFT directamente.
  - No llama JWPLC_Ethernet.begin().
  - No llama JWPLC_Ethernet.maintain().
  - Ethernet arranca desde el runtime JWPLC.
  - setText() sólo invalida el field cuando cambia su contenido.
*/

#include <JWPLC_Display.h>

bool displayConfigured = false;

unsigned long lastCheckMs = 0;
unsigned long lastLogMs = 0;
unsigned long lastCacheMs = 0;

const unsigned long CHECK_PERIOD_MS = 1000;
const unsigned long LOG_PERIOD_MS = 5000;
const unsigned long CACHE_PERIOD_MS = 500;

const uint32_t FRAM_BOOT_COUNTER_ADDR = 0;

uint32_t bootCounter = 0;
uint32_t logCounter = 0;

bool framOk = false;
bool sdOk = false;

bool ethOkCached = false;
bool ethLinkCached = false;

char ethStatusText[32] = "Not started";
char ethIpText[20] = "0.0.0.0";
char sdStatusText[32] = "Unknown";
char framStatusText[8] = "NO";

void copyText(char *dst, size_t dstSize, const char *src) {
  if (!src) {
    src = "";
  }

  strncpy(dst, src, dstSize - 1);
  dst[dstSize - 1] = '\0';
}

void ipToText(IPAddress ip, char *out, size_t len) {
  snprintf(out, len, "%u.%u.%u.%u", ip[0], ip[1], ip[2], ip[3]);
}

enum SpiUiFieldId : uint8_t
{
  SPI_FIELD_TITLE = 1,
  SPI_FIELD_ETH,
  SPI_FIELD_IP,
  SPI_FIELD_SD,
  SPI_FIELD_FRAM,
  SPI_FIELD_LOGS,
  SPI_FIELD_INFO
};

static const JWPLC_UIField SPI_FIELDS[] = {
    JWPLC_UITextField(
        SPI_FIELD_TITLE,
        JWPLC_UIRect(8, 4),
        JWPLC_UIText(nullptr, nullptr, 24),
        JWPLC_UITextFieldStyle(2, 1, false, JWPLC_UI_LAYOUT_INLINE, JWPLC_UI_ALIGN_LEFT),
        0,
        JWPLC_UIColors(ST77XX_CYAN, ST77XX_CYAN, ST77XX_BLACK, ST77XX_CYAN)),
    JWPLC_UITextField(SPI_FIELD_ETH, 8, 34, "ETH", 31),
    JWPLC_UITextField(SPI_FIELD_IP, 8, 54, "IP", 20),
    JWPLC_UITextField(SPI_FIELD_SD, 8, 74, "SD", 31),
    JWPLC_UITextField(SPI_FIELD_FRAM, 8, 94, "FRAM", 31),
    JWPLC_UITextField(SPI_FIELD_LOGS, 8, 114, "Logs", 16),
    JWPLC_UITextField(SPI_FIELD_INFO, 8, 140, nullptr, 28)};

void syncSpiUi()
{
  char framLine[32] = {};
  char logsLine[16] = {};

  snprintf(framLine, sizeof(framLine), "%s  Boot:%lu",
 framStatusText, (unsigned long)bootCounter);
  snprintf(logsLine, sizeof(logsLine), "%lu", (unsigned long)logCounter);

  JWPLC_Display.setText(SPI_FIELD_TITLE, "SPI COEXISTENCIA");
  JWPLC_Display.setText(SPI_FIELD_ETH, ethStatusText);
  JWPLC_Display.setText(SPI_FIELD_IP, ethIpText);
  JWPLC_Display.setText(SPI_FIELD_SD, sdStatusText);
  JWPLC_Display.setText(SPI_FIELD_FRAM, framLine);
  JWPLC_Display.setText(SPI_FIELD_LOGS, logsLine);
  JWPLC_Display.setText(SPI_FIELD_INFO, "IDLE AUTOMATICO EN 8s");
}

bool ethernetHasFault() {
  if (!JWPLC_Ethernet.isEnabled()) {
    return false;
  }

  if (strcmp(ethStatusText, "OK") == 0) {
    return false;
  }

  if (strcmp(ethStatusText, "Link OFF") == 0) {
    return false;
  }

  if (strcmp(ethStatusText, "Not started") == 0) {
    return false;
  }

  if (strcmp(ethStatusText, "Ethernet disabled") == 0) {
    return false;
  }

  if (strcmp(ethStatusText, "SPI lock timeout") == 0) {
    return false;
  }

  return true;
}

void updateCachedStatus() {
  ethLinkCached = JWPLC_Ethernet.isReady() && JWPLC_Ethernet.linkUp();
  ethOkCached = ethLinkCached;

  copyText(ethStatusText, sizeof(ethStatusText), JWPLC_Ethernet.statusString());
  ipToText(JWPLC_Ethernet.localIP(), ethIpText, sizeof(ethIpText));

  copyText(sdStatusText, sizeof(sdStatusText), sdOk ? "OK" : JWPLC_SD.lastErrorString());
  copyText(framStatusText, sizeof(framStatusText), framOk ? "OK" : "NO");
  syncSpiUi();
}

void loadBootCounterFromFRAM() {
  framOk = (JWPLC_FRAM.size() > 0);

  if (!framOk) {
    Serial.println("FRAM not available");
    return;
  }

  JWPLC_FRAM.get(FRAM_BOOT_COUNTER_ADDR, bootCounter);
  bootCounter++;

  JWPLC_FRAM.put(FRAM_BOOT_COUNTER_ADDR, bootCounter);

  Serial.print("FRAM bootCounter: ");
  Serial.println(bootCounter);
}

void updateSDStatus() {
  sdOk = JWPLC_SD.isEnabled() && JWPLC_SD.isCardPresent() && JWPLC_SD.isReady();

  Serial.print("SD enabled: ");
  Serial.println(JWPLC_SD.isEnabled() ? "yes" : "no");

  Serial.print("SD present: ");
  Serial.println(JWPLC_SD.isCardPresent() ? "yes" : "no");

  Serial.print("SD ready: ");
  Serial.println(JWPLC_SD.isReady() ? "yes" : "no");

  Serial.print("SD status: ");
  Serial.println(JWPLC_SD.lastErrorString());
}

void writeSDLog() {
  if (!JWPLC_SD.isEnabled() || !JWPLC_SD.isCardPresent() || !JWPLC_SD.isReady()) {
    sdOk = false;
    Serial.print("SD log skipped: ");
    Serial.println(JWPLC_SD.lastErrorString());
    return;
  }

  JWPLCFile f = JWPLC_SD.open("/spi_coexistence.csv", FILE_APPEND);

  if (!f) {
    sdOk = false;
    Serial.println("SD log failed: open failed");
    return;
  }

  logCounter++;

  f.print(millis());
  f.print(",");
  f.print("boot=");
  f.print(bootCounter);
  f.print(",");
  f.print("log=");
  f.print(logCounter);
  f.print(",");
  f.print("eth=");
  f.print(ethStatusText);
  f.print(",");
  f.print("ip=");
  f.println(ethIpText);

  f.close();

  sdOk = true;

  Serial.print("SD log OK #");
  Serial.println(logCounter);
}

void setup() {
  Serial.begin(115200);
  delay(1200);

  Serial.println();
  Serial.println("JWPLC Ethernet + SD + FRAM + Display SPI coexistence test");
  Serial.println("Runtime auto Ethernet. No begin() called.");

  JWPLC_Display.setIdleWakeButton(BTN_OK);
  JWPLC_Display.setIdleWakeMode(IDLE_WAKE_BUTTON_ONLY);
  JWPLC_Display.setIdleReturnMode(IDLE_RETURN_TIMEOUT);
  JWPLC_Display.setIdleTimeoutMs(8000);
  JWPLC_Display.setUserRefreshMode(USER_REFRESH_ON_DEMAND);
  JWPLC_Display.clearPendingInput();

  if (!JWPLC_Display.setFields(SPI_FIELDS, sizeof(SPI_FIELDS) / sizeof(SPI_FIELDS[0]))) {
    Serial.println("ERROR: no se pudieron registrar fields SPI");
  }

  loadBootCounterFromFRAM();
  updateSDStatus();

  updateCachedStatus();
}

void loop() {
  unsigned long now = millis();

  if (!displayConfigured && JWPLC_Display.isReady()) {
    displayConfigured = true;
    syncSpiUi();
    Serial.println("Display ready");
  }

  if (now - lastCacheMs >= CACHE_PERIOD_MS) {
    lastCacheMs = now;
    updateCachedStatus();
  }

  if (now - lastLogMs >= LOG_PERIOD_MS) {
    lastLogMs = now;
    writeSDLog();
    updateCachedStatus();
  }

  if (now - lastCheckMs >= CHECK_PERIOD_MS) {
    lastCheckMs = now;

    if (displayConfigured) {

      JWPLC_Display.setErrLed(ethernetHasFault() || !sdOk || !framOk);
      JWPLC_Display.setRunLed(true);
    }

    Serial.print("ETH: ");
    Serial.print(ethStatusText);

    Serial.print(" | Link: ");
    Serial.print(ethLinkCached ? "UP" : "DOWN");

    Serial.print(" | IP: ");
    Serial.print(ethIpText);

    Serial.print(" | SD: ");
    Serial.print(sdStatusText);

    Serial.print(" | FRAM: ");
    Serial.print(framStatusText);

    Serial.print(" | Boot: ");
    Serial.print(bootCounter);

    Serial.print(" | Logs: ");
    Serial.println(logCounter);
  }
}
