#include "JWPLC_Display_ST7789.h"

#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>

extern "C" {
  #include "jwplc_peripherals.h"
}

// =====================================================
// SPI compartido
// =====================================================
#define SPI_MOSI 23
#define SPI_MISO 19
#define SPI_SCK  18

// TFT
#define TFT_CS   33
#define TFT_DC   25
#define TFT_RST  14

// SD
#define SD_CS    32

// FRAM
#define FRAM_CS  13

// Ethernet W5500
#define ETH_CS   5

static Adafruit_ST7789 tft(TFT_CS, TFT_DC, TFT_RST);

// =====================================================
// Layout REPOSO
// =====================================================
static const int SCREEN_W = 320;
static const int SCREEN_H = 170;

static const int TITLE_X = 10;
static const int TITLE_Y = 6;

static const int RTC_DATE_X = 212;
static const int RTC_DATE_Y = 8;
static const int RTC_TIME_X = 228;
static const int RTC_TIME_Y = 22;

static const int DI_LABEL_X = 10;
static const int DI_LABEL_Y = 34;
static const int DI_ROW_Y   = 54;

static const int DO_LABEL_X = 10;
static const int DO_LABEL_Y = 96;
static const int DO_ROW_Y   = 116;

static const int BOX_X0   = 12;
static const int BOX_W    = 28;
static const int BOX_H    = 24;
static const int BOX_GAP  = 10;
static const int BOX_STEP = BOX_W + BOX_GAP;

static const int IDLE_TEXT_X = 250;
static const int IDLE_TEXT_Y = 154;

// =====================================================
// Estado interno
// =====================================================
enum DisplayMode : uint8_t
{
    DISPLAY_MODE_IDLE = 0,
    DISPLAY_MODE_USER = 1
};

static bool g_tftReady = false;
static bool g_forceFullRedraw = true;

static DisplayMode g_displayMode = DISPLAY_MODE_IDLE;
static JWPLCDisplay::IdleReturnMode g_idleReturnMode = JWPLCDisplay::IDLE_RETURN_ESC_ONLY;
static uint32_t g_idleTimeoutMs = 15000;
static uint32_t g_lastActivityMs = 0;

// cache IO
static uint8_t lastDI = 0xFF;
static uint8_t lastDO = 0xFF;

// cache RTC
static bool lastRtcValid = false;
static uint8_t lastRtcHour = 0xFF;
static uint8_t lastRtcMinute = 0xFF;
static uint8_t lastRtcSecond = 0xFF;
static uint8_t lastRtcDay = 0xFF;
static uint8_t lastRtcMonth = 0xFF;
static uint16_t lastRtcYear = 0xFFFF;

// navegación previa
static bool prevLeft = false;
static bool prevRight = false;
static bool prevUp = false;
static bool prevDown = false;
static bool prevOk = false;
static bool prevEsc = false;

// =====================================================
// Hooks débiles
// =====================================================
extern "C" bool __attribute__((weak)) jwplcNavLeftPressed(void)  { return false; }
extern "C" bool __attribute__((weak)) jwplcNavRightPressed(void) { return false; }
extern "C" bool __attribute__((weak)) jwplcNavUpPressed(void)    { return false; }
extern "C" bool __attribute__((weak)) jwplcNavDownPressed(void)  { return false; }
extern "C" bool __attribute__((weak)) jwplcNavOkPressed(void)    { return false; }
extern "C" bool __attribute__((weak)) jwplcNavEscPressed(void)   { return false; }

extern "C" bool __attribute__((weak)) jwplcCanReturnToIdle(void) { return true; }

extern "C" void __attribute__((weak)) jwplcUserDisplayEnterCallback(void) {}
extern "C" void __attribute__((weak)) jwplcUserDisplayRefreshCallback(const JWPLC_IOState* io, const JWPLC_RTCState* rtc)
{
  (void)io;
  (void)rtc;
}
extern "C" void __attribute__((weak)) jwplcUserDisplayExitCallback(void) {}

// =====================================================
// SPI helpers
// =====================================================
static void deselectAllSPI()
{
  pinMode(TFT_CS, OUTPUT);
  pinMode(SD_CS, OUTPUT);
  pinMode(FRAM_CS, OUTPUT);
  pinMode(ETH_CS, OUTPUT);

  digitalWrite(TFT_CS, HIGH);
  digitalWrite(SD_CS, HIGH);
  digitalWrite(FRAM_CS, HIGH);
  digitalWrite(ETH_CS, HIGH);
}

static void prepareForTFT()
{
  digitalWrite(SD_CS, HIGH);
  digitalWrite(FRAM_CS, HIGH);
  digitalWrite(ETH_CS, HIGH);
  digitalWrite(TFT_CS, HIGH);
}

// =====================================================
// Helpers visuales
// =====================================================
static void formatTime(const JWPLC_RTCState* rtc, char out[16])
{
  if (!rtc->valid)
  {
    snprintf(out, 16, "--:--:--");
    return;
  }

  snprintf(out, 16, "%02u:%02u:%02u", rtc->hour, rtc->minute, rtc->second);
}

static void formatDate(const JWPLC_RTCState* rtc, char out[16])
{
  if (!rtc->valid)
  {
    snprintf(out, 16, "--/--/----");
    return;
  }

  snprintf(out, 16, "%02u/%02u/%04u", rtc->day, rtc->month, rtc->year);
}

static void drawBitBox(int index, int rowY, bool state, bool isOutput)
{
  int x = BOX_X0 + index * BOX_STEP;
  int y = rowY;

  uint16_t fillColor = state
    ? (isOutput ? ST77XX_RED : ST77XX_GREEN)
    : 0x2104;

  tft.fillRoundRect(x, y, BOX_W, BOX_H, 5, fillColor);
  tft.drawRoundRect(x, y, BOX_W, BOX_H, 5, ST77XX_WHITE);

  char txt[2];
  txt[0] = '0' + index;
  txt[1] = '\0';

  int16_t x1, y1;
  uint16_t tw, th;

  tft.setTextSize(2);
  tft.setTextColor(ST77XX_WHITE);
  tft.getTextBounds(txt, 0, 0, &x1, &y1, &tw, &th);

  int tx = x + (BOX_W - tw) / 2;
  int ty = y + (BOX_H - th) / 2 - 1;

  // ajuste fino validado por ti
  tft.setCursor(tx + 1, ty + 2);
  tft.print(txt);
}

static void updateChangedBoxes(uint8_t oldValue, uint8_t newValue, int rowY, bool isOutput)
{
  uint8_t diff = oldValue ^ newValue;

  for (int i = 0; i < 8; i++)
  {
    if (diff & (1 << i))
    {
      bool state = (newValue >> i) & 0x01;
      drawBitBox(i, rowY, state, isOutput);
    }
  }
}

static void drawIdleStaticUI()
{
  tft.fillScreen(ST77XX_BLACK);
  tft.drawRect(0, 0, SCREEN_W, SCREEN_H, ST77XX_WHITE);

  // título
  tft.setTextSize(2);
  tft.setTextColor(ST77XX_CYAN);
  tft.setCursor(TITLE_X, TITLE_Y);
  tft.print("JWPLC Basic");

  // etiquetas
  tft.setTextSize(2);
  tft.setTextColor(ST77XX_YELLOW);
  tft.setCursor(DI_LABEL_X, DI_LABEL_Y);
  tft.print("Entradas:");

  tft.setCursor(DO_LABEL_X, DO_LABEL_Y);
  tft.print("Salidas:");

  // indicador de reposo
  tft.setTextSize(1);
  tft.setTextColor(ST77XX_WHITE);
  tft.setCursor(IDLE_TEXT_X, IDLE_TEXT_Y);
  tft.print("REPOSO");

  // recuadros base
  for (int i = 0; i < 8; i++)
  {
    drawBitBox(i, DI_ROW_Y, false, false);
    drawBitBox(i, DO_ROW_Y, false, true);
  }
}

static void updateIdleRTC(const JWPLC_RTCState* rtc)
{
  bool changed =
      g_forceFullRedraw ||
      (rtc->valid != lastRtcValid) ||
      (rtc->hour != lastRtcHour) ||
      (rtc->minute != lastRtcMinute) ||
      (rtc->second != lastRtcSecond) ||
      (rtc->day != lastRtcDay) ||
      (rtc->month != lastRtcMonth) ||
      (rtc->year != lastRtcYear);

  if (!changed)
  {
    return;
  }

  char dateBuf[16];
  char timeBuf[16];
  formatDate(rtc, dateBuf);
  formatTime(rtc, timeBuf);

  tft.fillRect(RTC_DATE_X, RTC_DATE_Y, 100, 12, ST77XX_BLACK);
  tft.fillRect(RTC_TIME_X, RTC_TIME_Y, 80, 12, ST77XX_BLACK);

  tft.setTextSize(1);
  tft.setTextColor(ST77XX_WHITE);

  tft.setCursor(RTC_DATE_X, RTC_DATE_Y);
  tft.print(dateBuf);

  tft.setCursor(RTC_TIME_X, RTC_TIME_Y);
  tft.print(timeBuf);

  lastRtcValid = rtc->valid;
  lastRtcHour = rtc->hour;
  lastRtcMinute = rtc->minute;
  lastRtcSecond = rtc->second;
  lastRtcDay = rtc->day;
  lastRtcMonth = rtc->month;
  lastRtcYear = rtc->year;
}

static void updateIdleIO(const JWPLC_IOState* io)
{
  if (g_forceFullRedraw)
  {
    updateChangedBoxes(0x00, io->di_logical_bank0, DI_ROW_Y, false);
    updateChangedBoxes(0x00, io->do_bank1, DO_ROW_Y, true);
    lastDI = io->di_logical_bank0;
    lastDO = io->do_bank1;
    return;
  }

  if (io->di_logical_bank0 != lastDI)
  {
    updateChangedBoxes(lastDI, io->di_logical_bank0, DI_ROW_Y, false);
    lastDI = io->di_logical_bank0;
  }

  if (io->do_bank1 != lastDO)
  {
    updateChangedBoxes(lastDO, io->do_bank1, DO_ROW_Y, true);
    lastDO = io->do_bank1;
  }
}

// =====================================================
// Navegación / actividad
// =====================================================
static void handleNavigation()
{
  bool left  = jwplcNavLeftPressed();
  bool right = jwplcNavRightPressed();
  bool up    = jwplcNavUpPressed();
  bool down  = jwplcNavDownPressed();
  bool ok    = jwplcNavOkPressed();
  bool esc   = jwplcNavEscPressed();

  bool leftEdge  = left  && !prevLeft;
  bool rightEdge = right && !prevRight;
  bool upEdge    = up    && !prevUp;
  bool downEdge  = down  && !prevDown;
  bool okEdge    = ok    && !prevOk;
  bool escEdge   = esc   && !prevEsc;

  bool anyEdge = leftEdge || rightEdge || upEdge || downEdge || okEdge || escEdge;

  if (anyEdge)
  {
    JWPLCDisplay::notifyActivity();

    if (g_displayMode == DISPLAY_MODE_IDLE)
    {
      JWPLCDisplay::enterUserUI();
    }
    else
    {
      // modo usuario activo
      if ((g_idleReturnMode == JWPLCDisplay::IDLE_RETURN_ESC_ONLY) &&
          escEdge &&
          jwplcCanReturnToIdle())
      {
        JWPLCDisplay::goIdle();
      }
    }
  }

  if ((g_displayMode == DISPLAY_MODE_USER) &&
      (g_idleReturnMode == JWPLCDisplay::IDLE_RETURN_TIMEOUT) &&
      (g_idleTimeoutMs > 0) &&
      jwplcCanReturnToIdle())
  {
    uint32_t now = millis();
    if ((uint32_t)(now - g_lastActivityMs) >= g_idleTimeoutMs)
    {
      JWPLCDisplay::goIdle();
    }
  }

  prevLeft = left;
  prevRight = right;
  prevUp = up;
  prevDown = down;
  prevOk = ok;
  prevEsc = esc;
}

// =====================================================
// API pública
// =====================================================
namespace JWPLCDisplay
{
    bool isReady()
    {
        return g_tftReady;
    }

    bool isIdleMode()
    {
        return (g_displayMode == DISPLAY_MODE_IDLE);
    }

    void forceRedraw()
    {
        g_forceFullRedraw = true;
        jwplcSystemForceDisplayRefresh();
    }

    void notifyActivity()
    {
        g_lastActivityMs = millis();
    }

    void enterUserUI()
    {
        if (g_displayMode == DISPLAY_MODE_USER)
        {
            g_lastActivityMs = millis();
            return;
        }

        g_displayMode = DISPLAY_MODE_USER;
        g_lastActivityMs = millis();

        prepareForTFT();
        tft.fillScreen(ST77XX_BLACK);

        jwplcUserDisplayEnterCallback();
        jwplcSystemForceDisplayRefresh();
    }

    void goIdle()
    {
        if (g_displayMode == DISPLAY_MODE_USER)
        {
            jwplcUserDisplayExitCallback();
        }

        g_displayMode = DISPLAY_MODE_IDLE;
        g_forceFullRedraw = true;
        g_lastActivityMs = millis();
        jwplcSystemForceDisplayRefresh();
    }

    void setIdleReturnMode(IdleReturnMode mode)
    {
        g_idleReturnMode = mode;
    }

    IdleReturnMode idleReturnMode()
    {
        return g_idleReturnMode;
    }

    void setIdleTimeoutMs(uint32_t timeoutMs)
    {
        g_idleTimeoutMs = timeoutMs;
    }

    uint32_t idleTimeoutMs()
    {
        return g_idleTimeoutMs;
    }

    Adafruit_ST7789& display()
    {
        return tft;
    }
}

// =====================================================
// Hooks del sistema JWPLC
// =====================================================
extern "C" bool jwplcDisplayBeginCallback(void)
{
  deselectAllSPI();
  SPI.begin(SPI_SCK, SPI_MISO, SPI_MOSI);

  prepareForTFT();
  digitalWrite(TFT_CS, LOW);

  tft.init(170, 320);
  tft.setRotation(3);

  digitalWrite(TFT_CS, HIGH);

  g_tftReady = true;
  g_forceFullRedraw = true;
  g_displayMode = DISPLAY_MODE_IDLE;
  g_idleReturnMode = JWPLCDisplay::IDLE_RETURN_ESC_ONLY;
  g_idleTimeoutMs = 15000;
  g_lastActivityMs = millis();

  lastDI = 0xFF;
  lastDO = 0xFF;

  lastRtcValid = false;
  lastRtcHour = 0xFF;
  lastRtcMinute = 0xFF;
  lastRtcSecond = 0xFF;
  lastRtcDay = 0xFF;
  lastRtcMonth = 0xFF;
  lastRtcYear = 0xFFFF;

  Serial.println("JWPLC_Display_ST7789 alpha13 listo");
  return true;
}

extern "C" void jwplcDisplayRefreshCallback(const JWPLC_IOState* io, const JWPLC_RTCState* rtc)
{
  if (!g_tftReady)
  {
    return;
  }

  handleNavigation();

  prepareForTFT();

  if (g_displayMode == DISPLAY_MODE_IDLE)
  {
    if (g_forceFullRedraw)
    {
      drawIdleStaticUI();
    }

    updateIdleRTC(rtc);
    updateIdleIO(io);
    g_forceFullRedraw = false;
    return;
  }

  // modo usuario
  jwplcUserDisplayRefreshCallback(io, rtc);
}