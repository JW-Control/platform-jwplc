#include <JWPLC_Display.h>

static constexpr int16_t TEST_X = 20;
static constexpr int16_t TEST_Y = 20;
static constexpr uint8_t TEST_TEXT_SIZE = 2;
static constexpr uint16_t TEST_FOREGROUND = ST77XX_RED;
static constexpr uint16_t TEST_BACKGROUND = ST77XX_WHITE;
static const char TEST_TEXT[] = "TEMP: 25.6 C";

static void drawParitySample()
{
    auto &tft = JWPLC_Display.tft();

    tft.fillScreen(ST77XX_BLACK);
    tft.setTextWrap(false);
    tft.setTextSize(TEST_TEXT_SIZE);
    tft.setTextColor(TEST_FOREGROUND, TEST_BACKGROUND);
    tft.setCursor(TEST_X, TEST_Y);
    tft.print(TEST_TEXT);

    Serial.print("[A11-2] x=");
    Serial.print(TEST_X);
    Serial.print(" y=");
    Serial.print(TEST_Y);
    Serial.print(" size=");
    Serial.print(TEST_TEXT_SIZE);
    Serial.print(" fg=0x");
    Serial.print(TEST_FOREGROUND, HEX);
    Serial.print(" bg=0x");
    Serial.print(TEST_BACKGROUND, HEX);
    Serial.print(" text=\"");
    Serial.print(TEST_TEXT);
    Serial.println("\"");
    Serial.println("[A11-2] RAW GFX: fondo de celda nativo, sin padding simetrico JWPLC");
}

extern "C" void jwplcUIEnter()
{
    Serial.println("[A11-2] USER UI enter");
}

extern "C" void jwplcUIPageEnter(uint8_t page)
{
    (void)page;
    drawParitySample();
}

extern "C" void jwplcUIUpdate()
{
}

extern "C" void jwplcUIExit()
{
    Serial.println("[A11-2] USER UI exit");
}

void setup()
{
    Serial.begin(115200);
    delay(300);

    Serial.println();
    Serial.println("=== ALPHA11 A11-2 GFX RAW TEXT PARITY ===");
    Serial.println("Canonical sample: TEMP: 25.6 C / size 2 / RED on WHITE");
    Serial.println("Position: x=20 y=20");
    Serial.println("Expected GFX cell bounds: 144 x 16 px");
    Serial.println("Goal: compare the exact GFX cell pixels against Designer");

    JWPLC_Display.setUserRefreshMode(USER_REFRESH_ON_DEMAND);
    JWPLC_Display.setIdleReturnMode(IDLE_RETURN_DISABLED);
    JWPLC_Display.setUserPage(0);
    JWPLC_Display.enterUserUI();
}

void loop()
{
    delay(5);
}
