/*
  A14 H3E.3B - JWPLC_Display runtime physical qualification over JWPLC_TFT

  Gate de hardware. Usa JWPLC_Display desde source temporal y JWPLC_TFT
  precompilado oficial. No es firmware de produccion.
*/

#include <Arduino.h>
#include <JWPLC_Display.h>

enum FieldId : uint8_t
{
    FIELD_COUNTER = 1,
    FIELD_RUN = 2,
    FIELD_BTN_P1 = 3,
    FIELD_BAR = 4,
    FIELD_LEVEL = 5,
    FIELD_BTN_P2 = 6
};

static const JWPLC_UIField FIELDS[] =
{
    JWPLC_UIValueField(
        FIELD_COUNTER,
        12,
        52,
        "COUNT",
        nullptr,
        JWPLC_UIValueFormat(5, 0, false, false),
        0),

    JWPLC_UIBoolField(
        FIELD_RUN,
        170,
        52,
        "STATE",
        JWPLC_UIBoolText("STOP", "RUN"),
        0),

    JWPLC_UITextField(
        FIELD_BTN_P1,
        12,
        108,
        "BTN",
        10,
        0),

    JWPLC_UIBarField(
        FIELD_BAR,
        12,
        58,
        "LEVEL",
        JWPLC_UIRange(0.0f, 100.0f),
        280,
        28,
        1),

    JWPLC_UIValueField(
        FIELD_LEVEL,
        12,
        112,
        "VALUE",
        "%",
        JWPLC_UIValueFormat(3, 0, false, false),
        1),

    JWPLC_UITextField(
        FIELD_BTN_P2,
        170,
        112,
        "BTN",
        10,
        1)
};

static uint32_t g_counter = 0;
static bool g_run = false;
static float g_level = 50.0f;
static uint32_t g_lastDynamicMs = 0;

static void setLastButton(const char *name)
{
    JWPLC_Display.setText(FIELD_BTN_P1, name);
    JWPLC_Display.setText(FIELD_BTN_P2, name);
}

extern "C" void jwplcUIEnter()
{
    Serial.println("H3E3B_UI_ENTER=YES");
}

extern "C" void jwplcUIPageEnter(uint8_t page)
{
    JWPLC_TFTClass &tft = JWPLC_Display.tft();

    tft.setTextWrap(false);
    tft.setTextSize(2);
    tft.setTextColor(
        JWPLC_TFT_CYAN,
        JWPLC_TFT_BLACK);
    tft.setCursor(8, 18);

    if (page == 0)
    {
        tft.print("H3E3B PAGE 1");

        tft.setTextSize(1);
        tft.setTextColor(
            JWPLC_TFT_WHITE,
            JWPLC_TFT_BLACK);
        tft.setCursor(8, 145);
        tft.print("RIGHT -> PAGE 2 | ESC -> IDLE");
    }
    else
    {
        tft.print("H3E3B PAGE 2");

        tft.setTextSize(1);
        tft.setTextColor(
            JWPLC_TFT_WHITE,
            JWPLC_TFT_BLACK);
        tft.setCursor(8, 145);
        tft.print("UP/DOWN LEVEL | LEFT -> PAGE 1");
    }

    Serial.print("H3E3B_PAGE_ENTER=");
    Serial.println((unsigned)page);
}

extern "C" void jwplcUIExit()
{
    Serial.println("H3E3B_UI_EXIT=YES");
}

void setup()
{
    Serial.begin(115200);
    delay(250);

    Serial.println();
    Serial.println("H3E3B_BOOT=YES");
    Serial.print("H3E3B_DISPLAY_READY_AT_SETUP=");
    Serial.println(JWPLC_Display.isReady() ? "YES" : "NO");
    Serial.print("H3E3B_BUTTONS_READY_AT_SETUP=");
    Serial.println(JWPLC_Display.buttonsReady() ? "YES" : "NO");

    const bool fieldsOk =
        JWPLC_Display.setFields(
            FIELDS,
            sizeof(FIELDS) / sizeof(FIELDS[0]));

    Serial.print("H3E3B_SET_FIELDS=");
    Serial.println(fieldsOk ? "PASS" : "FAIL");

    // Para este gate el sketch conserva RIGHT/LEFT/UP/DOWN/OK.
    // El Display solo observa ESC para volver a IDLE.
    JWPLC_Display.setUserPageCount(1);
    JWPLC_Display.setUserPage(0);
    JWPLC_Display.setUserRefreshMode(
        USER_REFRESH_ON_DEMAND);
    JWPLC_Display.setUserRefreshPeriodMs(40);

    JWPLC_Display.setIdleWakeMode(
        IDLE_WAKE_DISABLED);
    JWPLC_Display.setIdleReturnMode(
        IDLE_RETURN_ESC_ONLY);
    JWPLC_Display.clearPendingInput();

    JWPLC_Display.setValue(
        FIELD_COUNTER,
        g_counter);
    JWPLC_Display.setBool(
        FIELD_RUN,
        g_run);
    JWPLC_Display.setBar(
        FIELD_BAR,
        g_level);
    JWPLC_Display.setValue(
        FIELD_LEVEL,
        g_level);
    setLastButton("NONE");

    Serial.println("H3E3B_EXPECT=IDLE");
}

void loop()
{
    const uint32_t now = millis();

    if ((uint32_t)(now - g_lastDynamicMs) >= 750U)
    {
        g_lastDynamicMs = now;

        ++g_counter;
        g_run = !g_run;

        JWPLC_Display.setValue(
            FIELD_COUNTER,
            g_counter);
        JWPLC_Display.setBool(
            FIELD_RUN,
            g_run);
    }

    if (JWPLC_Buttons.pressed(BTN_OK))
    {
        setLastButton("OK");
        Serial.println("H3E3B_BUTTON=OK");

        if (JWPLC_Display.isIdleMode())
        {
            JWPLC_Display.enterUserUI();
        }
    }

    if (JWPLC_Buttons.pressed(BTN_RIGHT))
    {
        setLastButton("RIGHT");
        Serial.println("H3E3B_BUTTON=RIGHT");

        if (!JWPLC_Display.isIdleMode())
        {
            JWPLC_Display.setUserPage(1);
        }
    }

    if (JWPLC_Buttons.pressed(BTN_LEFT))
    {
        setLastButton("LEFT");
        Serial.println("H3E3B_BUTTON=LEFT");

        if (!JWPLC_Display.isIdleMode())
        {
            JWPLC_Display.setUserPage(0);
        }
    }

    if (JWPLC_Buttons.pressed(BTN_UP))
    {
        setLastButton("UP");
        Serial.println("H3E3B_BUTTON=UP");

        if (!JWPLC_Display.isIdleMode())
        {
            g_level += 10.0f;

            if (g_level > 100.0f)
            {
                g_level = 100.0f;
            }

            JWPLC_Display.setBar(
                FIELD_BAR,
                g_level);
            JWPLC_Display.setValue(
                FIELD_LEVEL,
                g_level);
        }
    }

    if (JWPLC_Buttons.pressed(BTN_DOWN))
    {
        setLastButton("DOWN");
        Serial.println("H3E3B_BUTTON=DOWN");

        if (!JWPLC_Display.isIdleMode())
        {
            g_level -= 10.0f;

            if (g_level < 0.0f)
            {
                g_level = 0.0f;
            }

            JWPLC_Display.setBar(
                FIELD_BAR,
                g_level);
            JWPLC_Display.setValue(
                FIELD_LEVEL,
                g_level);
        }
    }

    if (JWPLC_Buttons.pressed(BTN_ESC))
    {
        setLastButton("ESC");
        Serial.println("H3E3B_BUTTON=ESC");
        // No llamar goIdle(): el retorno debe hacerlo JWPLC_Display.
    }

    delay(5);
}
