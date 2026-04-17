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
// Layout
// =====================================================
static const int SCREEN_W = 320;
static const int SCREEN_H = 170;

static const int TITLE_X = 10;
static const int TITLE_Y = 6;

static const int RTC_X   = 210;
static const int RTC_Y   = 10;

static const int DI_LABEL_X = 10;
static const int DI_LABEL_Y = 30;
static const int DI_ROW_Y   = 50;
static const int DI_TEXT_Y  = 82;

static const int DO_LABEL_X = 10;
static const int DO_LABEL_Y = 98;
static const int DO_ROW_Y   = 118;
static const int DO_TEXT_Y  = 150;

static const int BOX_X0   = 12;
static const int BOX_W    = 28;
static const int BOX_H    = 24;
static const int BOX_GAP  = 10;
static const int BOX_STEP = BOX_W + BOX_GAP;

static const int BANK_TEXT_X  = 10;
static const int BANK_VALUE_X = 78;
static const int BANK_VALUE_W = 100;
static const int BANK_VALUE_H = 10;

// =====================================================
// Estado local TFT
// =====================================================
static bool g_tftReady = false;
static uint8_t lastDI = 0x00;
static uint8_t lastDO = 0x00;
static bool lastRtcValid = false;
static uint8_t lastRtcHour = 0xFF;
static uint8_t lastRtcMinute = 0xFF;
static uint8_t lastRtcSecond = 0xFF;

// =====================================================
// Utilidades SPI
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
static String bankToStringLSBFirst(uint8_t value)
{
  String s;
  s.reserve(8);

  for (int i = 0; i < 8; i++)
  {
    s += ((value >> i) & 0x01) ? '1' : '0';
  }

  return s;
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

  // ajuste fino validado
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

static void drawBankTextStatic()
{
  tft.setTextSize(1);
  tft.setTextColor(ST77XX_WHITE);

  tft.setCursor(BANK_TEXT_X, DI_TEXT_Y);
  tft.print("DI[0..7] =");

  tft.setCursor(BANK_TEXT_X, DO_TEXT_Y);
  tft.print("DO[0..7] =");
}

static void updateBankValue(int y, uint8_t value)
{
  tft.fillRect(BANK_VALUE_X, y, BANK_VALUE_W, BANK_VALUE_H, ST77XX_BLACK);

  tft.setTextSize(1);
  tft.setTextColor(ST77XX_WHITE);
  tft.setCursor(BANK_VALUE_X, y);
  tft.print(bankToStringLSBFirst(value));
}

static void drawRTCStatic()
{
  tft.setTextSize(1);
  tft.setTextColor(ST77XX_WHITE);
  tft.setCursor(RTC_X, RTC_Y);
  tft.print("RTC --:--:--");
}

static void updateRTC(const JWPLC_RTCState* rtc)
{
  tft.fillRect(RTC_X, RTC_Y, 100, 10, ST77XX_BLACK);

  tft.setTextSize(1);
  tft.setTextColor(ST77XX_WHITE);
  tft.setCursor(RTC_X, RTC_Y);

  if (!rtc->valid)
  {
    tft.print("RTC --:--:--");
    return;
  }

  char buf[16];
  snprintf(buf, sizeof(buf), "RTC %02u:%02u:%02u", rtc->hour, rtc->minute, rtc->second);
  tft.print(buf);
}

static void drawStaticUI()
{
  tft.fillScreen(ST77XX_BLACK);
  tft.drawRect(0, 0, SCREEN_W, SCREEN_H, ST77XX_WHITE);

  tft.setTextSize(2);
  tft.setTextColor(ST77XX_CYAN);
  tft.setCursor(TITLE_X, TITLE_Y);
  tft.print("JWPLC Basic");

  drawRTCStatic();

  tft.setTextColor(ST77XX_YELLOW);
  tft.setCursor(DI_LABEL_X, DI_LABEL_Y);
  tft.print("Entradas:");

  tft.setCursor(DO_LABEL_X, DO_LABEL_Y);
  tft.print("Salidas:");

  drawBankTextStatic();

  for (int i = 0; i < 8; i++)
  {
    drawBitBox(i, DI_ROW_Y, false, false);
    drawBitBox(i, DO_ROW_Y, false, true);
  }

  updateBankValue(DI_TEXT_Y, 0x00);
  updateBankValue(DO_TEXT_Y, 0x00);
}

// =====================================================
// API pública pequeña
// =====================================================
namespace JWPLCDisplay
{
    bool isReady()
    {
        return g_tftReady;
    }

    void forceRedraw()
    {
        jwplcSystemForceDisplayRefresh();
    }
}

// =====================================================
// Hooks que consume el core alpha10
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

  drawStaticUI();

  g_tftReady = true;
  lastDI = 0x00;
  lastDO = 0x00;
  lastRtcValid = false;
  lastRtcHour = 0xFF;
  lastRtcMinute = 0xFF;
  lastRtcSecond = 0xFF;

  Serial.println("JWPLC_Display_ST7789 listo");
  return true;
}

extern "C" void jwplcDisplayRefreshCallback(const JWPLC_IOState* io, const JWPLC_RTCState* rtc)
{
  if (!g_tftReady)
  {
    return;
  }

  prepareForTFT();

  uint8_t di = io->di_logical_bank0;
  uint8_t doShadow = io->do_bank1;

  if (di != lastDI)
  {
    updateChangedBoxes(lastDI, di, DI_ROW_Y, false);
    updateBankValue(DI_TEXT_Y, di);
    lastDI = di;
  }

  if (doShadow != lastDO)
  {
    updateChangedBoxes(lastDO, doShadow, DO_ROW_Y, true);
    updateBankValue(DO_TEXT_Y, doShadow);
    lastDO = doShadow;
  }

  if ((rtc->valid != lastRtcValid) ||
      (rtc->hour != lastRtcHour) ||
      (rtc->minute != lastRtcMinute) ||
      (rtc->second != lastRtcSecond))
  {
    updateRTC(rtc);
    lastRtcValid = rtc->valid;
    lastRtcHour = rtc->hour;
    lastRtcMinute = rtc->minute;
    lastRtcSecond = rtc->second;
  }
}