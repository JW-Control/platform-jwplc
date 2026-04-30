/*
  Ethernet_SPI_Coexistence

  Test de coexistencia SPI para JWPLC Basic.

  Valida:
  - Ethernet W5500
  - Display ST7789
  - FRAM SPI
  - microSD SPI
  - Indicadores ETH / ERR en pantalla IDLE
  - Pantalla USER con resumen de estado

  Regla importante:
  - En callbacks gráficos NO se consultan periféricos SPI.
  - Ethernet / SD / FRAM se consultan en loop().
  - Los callbacks USER solo dibujan variables cacheadas.
*/

#include <JWPLC_Ethernet.h>
#include <JWPLC_Display.h>
#include <JWPLC_GlobalPeripherals.h>

bool ethernetStarted = false;
bool displayConfigured = false;

unsigned long lastCheckMs = 0;
unsigned long lastRetryMs = 0;
unsigned long lastLogMs = 0;
unsigned long lastCacheMs = 0;

const unsigned long CHECK_PERIOD_MS = 1000;
const unsigned long RETRY_PERIOD_MS = 5000;
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

// =====================================================
// Helpers seguros de texto/cache
// =====================================================

void copyText(char *dst, size_t dstSize, const char *src)
{
    if (!src)
    {
        src = "";
    }

    strncpy(dst, src, dstSize - 1);
    dst[dstSize - 1] = '\0';
}

void ipToText(IPAddress ip, char *out, size_t len)
{
    snprintf(out, len, "%u.%u.%u.%u", ip[0], ip[1], ip[2], ip[3]);
}

void updateCachedStatus()
{
    ethLinkCached = ethernetStarted && JWPLC_Ethernet.linkUp();
    ethOkCached = ethernetStarted && ethLinkCached;

    copyText(ethStatusText, sizeof(ethStatusText), JWPLC_Ethernet.statusString());
    ipToText(JWPLC_Ethernet.localIP(), ethIpText, sizeof(ethIpText));

    copyText(sdStatusText, sizeof(sdStatusText), sdOk ? "OK" : JWPLC_SD.lastErrorString());
    copyText(framStatusText, sizeof(framStatusText), framOk ? "OK" : "NO");
}

// =====================================================
// Pantalla USER
// =====================================================

extern "C" void jwplcUserDisplayEnterCallback()
{
    auto &tft = JWPLC_Display.tft();

    tft.fillScreen(ST77XX_BLACK);

    tft.setTextWrap(false);

    tft.setTextSize(2);
    tft.setTextColor(ST77XX_CYAN, ST77XX_BLACK);
    tft.setCursor(10, 12);
    tft.print("SPI TEST");

    tft.setTextSize(1);
    tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);

    tft.setCursor(10, 48);
    tft.print("ETH:");

    tft.setCursor(10, 68);
    tft.print("IP:");

    tft.setCursor(10, 88);
    tft.print("SD:");

    tft.setCursor(10, 108);
    tft.print("FRAM:");

    tft.setCursor(10, 128);
    tft.print("Logs:");

    tft.setCursor(10, 154);
    tft.print("Retorna a IDLE en 8s");
}

extern "C" void jwplcUserDisplayRefreshCallback(const JWPLC_IOState *io, const JWPLC_RTCState *rtc)
{
    (void)io;
    (void)rtc;

    static unsigned long lastDrawMs = 0;
    unsigned long now = millis();

    if (now - lastDrawMs < 500)
    {
        return;
    }

    lastDrawMs = now;

    auto &tft = JWPLC_Display.tft();

    // Limpiar únicamente el área de valores.
    tft.fillRect(70, 45, 240, 100, ST77XX_BLACK);

    tft.setTextWrap(false);
    tft.setTextSize(1);

    // Ethernet
    tft.setCursor(70, 48);
    tft.setTextColor(ethOkCached ? ST77XX_GREEN : ST77XX_RED, ST77XX_BLACK);
    tft.print(ethStatusText);

    // IP
    tft.setCursor(70, 68);
    tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
    tft.print(ethIpText);

    // SD
    tft.setCursor(70, 88);
    tft.setTextColor(sdOk ? ST77XX_GREEN : ST77XX_RED, ST77XX_BLACK);
    tft.print(sdStatusText);

    // FRAM
    tft.setCursor(70, 108);
    tft.setTextColor(framOk ? ST77XX_GREEN : ST77XX_RED, ST77XX_BLACK);
    tft.print(framStatusText);

    tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
    tft.print(" Boot:");
    tft.print(bootCounter);

    // Logs
    tft.setCursor(70, 128);
    tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
    tft.print(logCounter);
}

extern "C" void jwplcUserDisplayExitCallback()
{
    Serial.println("Saliendo de USER hacia IDLE");
}

// =====================================================
// Lógica de periféricos
// =====================================================

void loadBootCounterFromFRAM()
{
    framOk = (JWPLC_FRAM.size() > 0);

    if (!framOk)
    {
        Serial.println("FRAM not available");
        return;
    }

    JWPLC_FRAM.get(FRAM_BOOT_COUNTER_ADDR, bootCounter);
    bootCounter++;

    JWPLC_FRAM.put(FRAM_BOOT_COUNTER_ADDR, bootCounter);

    Serial.print("FRAM bootCounter: ");
    Serial.println(bootCounter);
}

void updateSDStatus()
{
    sdOk = JWPLC_SD.isEnabled() &&
           JWPLC_SD.isCardPresent() &&
           JWPLC_SD.isReady();

    Serial.print("SD enabled: ");
    Serial.println(JWPLC_SD.isEnabled() ? "yes" : "no");

    Serial.print("SD present: ");
    Serial.println(JWPLC_SD.isCardPresent() ? "yes" : "no");

    Serial.print("SD ready: ");
    Serial.println(JWPLC_SD.isReady() ? "yes" : "no");

    Serial.print("SD status: ");
    Serial.println(JWPLC_SD.lastErrorString());
}

void writeSDLog()
{
    if (!JWPLC_SD.isEnabled() || !JWPLC_SD.isCardPresent() || !JWPLC_SD.isReady())
    {
        sdOk = false;
        Serial.print("SD log skipped: ");
        Serial.println(JWPLC_SD.lastErrorString());
        return;
    }

    JWPLCFile f = JWPLC_SD.open("/spi_coexistence.csv", FILE_APPEND);

    if (!f)
    {
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

void setupEthernet()
{
    ethernetStarted = JWPLC_Ethernet.begin();

    Serial.println(ethernetStarted ? "Ethernet begin OK" : "Ethernet begin failed");
    JWPLC_Ethernet.printStatus(Serial);
}

// =====================================================
// Arduino setup / loop
// =====================================================

void setup()
{
    Serial.begin(115200);
    delay(1200);

    Serial.println();
    Serial.println("JWPLC Ethernet + SD + FRAM + Display SPI coexistence test");

    loadBootCounterFromFRAM();
    updateSDStatus();
    setupEthernet();

    updateCachedStatus();
}

void loop()
{
    unsigned long now = millis();

    if (ethernetStarted)
    {
        JWPLC_Ethernet.maintain();
    }

    if (!displayConfigured && JWPLC_Display.isReady())
    {
        displayConfigured = true;

        JWPLC_Display.setIdleReturnMode(IDLE_RETURN_TIMEOUT);
        JWPLC_Display.setIdleTimeoutMs(8000);
        JWPLC_Display.setUserRefreshPeriodMs(250);

        Serial.println("Display ready");
    }

    // Reintento Ethernet solo si el periférico está habilitado.
    if (JWPLC_Ethernet.isEnabled() &&
        !ethernetStarted &&
        (now - lastRetryMs >= RETRY_PERIOD_MS))
    {
        lastRetryMs = now;

        Serial.println("Retry Ethernet begin...");
        ethernetStarted = JWPLC_Ethernet.begin();

        Serial.print("Retry result: ");
        Serial.println(ethernetStarted ? "OK" : JWPLC_Ethernet.statusString());
    }

    // Actualizar estado cacheado fuera del callback gráfico.
    if (now - lastCacheMs >= CACHE_PERIOD_MS)
    {
        lastCacheMs = now;
        updateCachedStatus();
    }

    // Log periódico en SD.
    if (now - lastLogMs >= LOG_PERIOD_MS)
    {
        lastLogMs = now;
        writeSDLog();
        updateCachedStatus();
    }

    // Estado periódico + LEDs del IDLE.
    if (now - lastCheckMs >= CHECK_PERIOD_MS)
    {
        lastCheckMs = now;

        if (displayConfigured)
        {
            JWPLC_Display.setEthLed(ethOkCached);
            JWPLC_Display.setErrLed(!ethOkCached || !sdOk || !framOk);
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
