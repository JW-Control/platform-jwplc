#include <JWPLC_Display.h>

enum UIFieldId : uint8_t
{
    FIELD_COUNT = 1,
    FIELD_INPUTS = 2,
    FIELD_TEMP = 3,
    FIELD_RUN = 4,
    FIELD_BAR = 5
};

const JWPLC_UIField UI_FIELDS[] =
{
    // Helper agrupado: rect, texto, formato y estilo se configuran una vez.
    JWPLC_UIValueField(
        FIELD_COUNT,
        JWPLC_UIRect(10, 38),
        JWPLC_UIText("Contador"),
        JWPLC_UIValueFormat(6, 0, false, false),
        JWPLC_UIValueStyle(2, 1, true),
        0),

    // Helper corto: suficiente cuando AUTO y estilo default son adecuados.
    JWPLC_UIValueField(
        FIELD_INPUTS,
        170, 38,
        "Entradas",
        nullptr,
        JWPLC_UIValueFormat(3, 0, false, false),
        0),

    // Inicializacion directa agrupada: una fila por grupo del struct.
    {
        {FIELD_TEMP, 0, JWPLC_UI_FIELD_VALUE},
        {10, 95, JWPLC_UI_AUTO, JWPLC_UI_AUTO},
        {"Temp", "C"},
        {1, 2, true, JWPLC_UI_LAYOUT_INLINE, JWPLC_UI_ALIGN_RIGHT},
        {ST77XX_WHITE, ST77XX_WHITE, ST77XX_BLACK, ST77XX_WHITE},
        {3, 1, true, false},
        {}
    },

    JWPLC_UIBoolField(
        FIELD_RUN,
        JWPLC_UIRect(170, 95),
        JWPLC_UIText("Estado"),
        JWPLC_UIBoolText("STOP", "RUN"),
        JWPLC_UIBoolStyle(2, 1, true),
        0),

    JWPLC_UIBarField(
        FIELD_BAR,
        JWPLC_UIRect(10, 80, 280, 28),
        JWPLC_UIText("Nivel"),
        JWPLC_UIRange(0.0f, 100.0f),
        JWPLC_UIBarStyle(1, true),
        1)
};

uint32_t counter = 0;
float simulatedTemp = 20.0f;
bool runState = false;
float barValue = 0.0f;

extern "C" void jwplcUIPageEnter(uint8_t page)
{
    auto &tft = JWPLC_Display.tft();

    tft.setTextColor(ST77XX_CYAN, ST77XX_BLACK);
    tft.setTextSize(2);
    tft.setCursor(8, 8);

    if (page == 0)
    {
        tft.print("ALPHA8 HMI");
    }
    else
    {
        tft.print("ALPHA8 PAGE 2");
    }
}

extern "C" void jwplcUIExit()
{
    Serial.println("[UI] USER -> IDLE");
}

void setup()
{
    Serial.begin(115200);
    delay(300);

    Serial.println();
    Serial.println("=== ALPHA8 A-D BUTTONS + TFT GATE ===");
    Serial.println("Default esperado: IDLE_WAKE_DISABLED");
    Serial.println("OK = entrar USER");
    Serial.println("ESC = volver IDLE");
    Serial.println("RIGHT/LEFT = cambiar pagina");
    Serial.println("UP/DOWN = variar barra");

    if (!JWPLC_Display.setFields(
            UI_FIELDS,
            sizeof(UI_FIELDS) / sizeof(UI_FIELDS[0])))
    {
        Serial.println("[FAIL] setFields()");
    }

    // Alpha8: ON_DEMAND es el default. Se fija explicitamente en este gate.
    JWPLC_Display.setUserRefreshMode(USER_REFRESH_ON_DEMAND);
    JWPLC_Display.setIdleReturnMode(IDLE_RETURN_ESC_ONLY);

    Serial.print("[BOOT] wakeMode=");
    Serial.println((int)JWPLC_Display.idleWakeMode());
}

void loop()
{
    static uint32_t lastSecond = 0;

    // Puede llamarse cada loop: si el valor no cambia no hay redibujado TFT.
    JWPLC_Display.setValue(FIELD_INPUTS, JWPLC_IO.inputs());

    if (millis() - lastSecond >= 1000)
    {
        lastSecond = millis();

        counter++;
        simulatedTemp += 0.1f;

        if (simulatedTemp > 29.9f)
        {
            simulatedTemp = 20.0f;
        }

        runState = !runState;

        JWPLC_Display.setValue(FIELD_COUNT, counter);
        JWPLC_Display.setValue(FIELD_TEMP, simulatedTemp);
        JWPLC_Display.setBool(FIELD_RUN, runState);

        Serial.print("[HB] t=");
        Serial.print(millis());
        Serial.print(" idle=");
        Serial.print(JWPLC_Display.isIdleMode() ? "YES" : "NO");
        Serial.print(" inputs=0x");
        Serial.print(JWPLC_IO.inputs(), HEX);
        Serial.print(" rtc=");

        if (JWPLC_Time.valid())
        {
            Serial.print(JWPLC_Time.hour());
            Serial.print(':');
            Serial.print(JWPLC_Time.minute());
            Serial.print(':');
            Serial.println(JWPLC_Time.second());
        }
        else
        {
            Serial.println("INVALID");
        }
    }

    if (JWPLC_Buttons.pressed(BTN_OK))
    {
        Serial.println("[BTN] OK");
        JWPLC_Display.enterUserUI();
    }

    if (JWPLC_Buttons.pressed(BTN_RIGHT))
    {
        Serial.println("[BTN] RIGHT -> PAGE 1");
        JWPLC_Display.setUserPage(1);
    }

    if (JWPLC_Buttons.pressed(BTN_LEFT))
    {
        Serial.println("[BTN] LEFT -> PAGE 0");
        JWPLC_Display.setUserPage(0);
    }

    if (JWPLC_Buttons.pressed(BTN_UP))
    {
        barValue += 10.0f;
        if (barValue > 100.0f)
        {
            barValue = 100.0f;
        }

        JWPLC_Display.setBar(FIELD_BAR, barValue);
        Serial.println("[BTN] UP");
    }

    if (JWPLC_Buttons.pressed(BTN_DOWN))
    {
        barValue -= 10.0f;
        if (barValue < 0.0f)
        {
            barValue = 0.0f;
        }

        JWPLC_Display.setBar(FIELD_BAR, barValue);
        Serial.println("[BTN] DOWN");
    }

    delay(5);
}
