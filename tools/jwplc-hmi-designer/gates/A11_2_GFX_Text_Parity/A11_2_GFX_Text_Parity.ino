#include <JWPLC_Display.h>

static constexpr uint8_t PAGE_RAW_EDGE = 0;
static constexpr uint8_t PAGE_SAFE_AREA = 1;

static constexpr uint8_t TEST_TEXT_SIZE = 2;
static constexpr uint16_t TEST_FOREGROUND = ST77XX_RED;
static constexpr uint16_t TEST_BACKGROUND = ST77XX_WHITE;
static const char TEST_TEXT[] = "TEMP: 25.6 C";

static void drawParitySample(uint8_t page)
{
    auto &tft = JWPLC_Display.tft();

    const int16_t x = (page == PAGE_RAW_EDGE) ? 0 : 3;
    const int16_t y = (page == PAGE_RAW_EDGE) ? 0 : 3;

    tft.fillScreen(ST77XX_BLACK);
    tft.setTextWrap(false);
    tft.setTextSize(TEST_TEXT_SIZE);
    tft.setTextColor(TEST_FOREGROUND, TEST_BACKGROUND);
    tft.setCursor(x, y);
    tft.print(TEST_TEXT);

    Serial.print("[A11-2] page=");
    Serial.print(page);
    Serial.print(" x=");
    Serial.print(x);
    Serial.print(" y=");
    Serial.print(y);
    Serial.print(" size=");
    Serial.print(TEST_TEXT_SIZE);
    Serial.print(" fg=0x");
    Serial.print(TEST_FOREGROUND, HEX);
    Serial.print(" bg=0x");
    Serial.print(TEST_BACKGROUND, HEX);
    Serial.print(" text=\"");
    Serial.print(TEST_TEXT);
    Serial.println("\"");
}

extern "C" void jwplcUIEnter()
{
    Serial.println("[A11-2] USER UI enter");
}

extern "C" void jwplcUIPageEnter(uint8_t page)
{
    drawParitySample(page);
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
    Serial.println("=== ALPHA11 A11-2 GFX TEXT PARITY ===");
    Serial.println("LEFT  -> page 0: RAW edge x=0 y=0");
    Serial.println("RIGHT -> page 1: safe area x=3 y=3");
    Serial.println("Canonical sample: TEMP: 25.6 C / size 2 / RED on WHITE");
    Serial.println("Expected logical text cell bounds: 144 x 16 px");

    JWPLC_Display.setUserRefreshMode(USER_REFRESH_ON_DEMAND);
    JWPLC_Display.setIdleReturnMode(IDLE_RETURN_DISABLED);
    JWPLC_Display.setUserPage(PAGE_SAFE_AREA);
    JWPLC_Display.enterUserUI();
}

void loop()
{
    if (JWPLC_Buttons.pressed(BTN_LEFT))
    {
        JWPLC_Display.setUserPage(PAGE_RAW_EDGE);
    }

    if (JWPLC_Buttons.pressed(BTN_RIGHT))
    {
        JWPLC_Display.setUserPage(PAGE_SAFE_AREA);
    }

    delay(5);
}
