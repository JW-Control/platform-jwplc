#include <JWPLC_Display.h>
#include <cstdio>

enum UIFieldId : uint8_t
{
    FIELD_COUNT = 1,
    FIELD_INPUTS = 2,
    FIELD_TEMP = 3,
    FIELD_RUN = 4,
    FIELD_BAR = 5,
    FIELD_RTC = 6
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

    // Inicializacion directa: una fila por grupo, sin grupos anidados.
    {
        {FIELD_TEMP, 0, JWPLC_UI_FIELD_VALUE},
        {10, 95, JWPLC_UI_AUTO, JWPLC_UI_AUTO},
        {"Temp", "C"},
        {1, 2, true, JWPLC_UI_LAYOUT_INLINE, JWPLC_UI_ALIGN_RIGHT},
        {ST77XX_WHITE, ST77XX_WHITE, ST77XX_BLACK, ST77XX_WHITE},
        {3, 1, true, false}
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
        JWPLC_UIRect(10, 72, 280, 28),
        JWPLC_UIText("Nivel"),
        JWPLC_UIRange(0.0f, 100.0f),
        JWPLC_UIBarStyle(1, true),
        1),

    JWPLC_UITextField(
        FIELD_RTC,
        JWPLC_UIRect(10, 118),
        JWPLC_UIText("RTC", nullptr, 8),
        JWPLC_UITextFieldStyle(2, 1, true),
        1)
};

static uint32_t counter = 0;
static float simulatedTemp = 20.0f;
static bool runState = false;
static float barValue = 0.0f;
static char rtcText[9] = "--:--:--";

static bool g_expectUserEntry = false;
static bool g_userInteractionStarted = false;
static uint32_t g_unexpectedUserEntries = 0;
static uint32_t g_buttonPressCount[BTN_COUNT] = {};

static void printButtonPress(const char *name, uint8_t id)
{
    g_userInteractionStarted = true;
    g_buttonPressCount[id]++;

    Serial.print("[BTN PRESS] ");
    Serial.print(name);
    Serial.print(" count=");
    Serial.println(g_buttonPressCount[id]);
}

static void printButtonRelease(const char *name)
{
    Serial.print("[BTN RELEASE] ");
    Serial.println(name);
}

static void updateRTCText()
{
    if (!JWPLC_Time.valid())
    {
        snprintf(rtcText, sizeof(rtcText), "--:--:--");
        return;
    }

    snprintf(
        rtcText,
        sizeof(rtcText),
        "%02u:%02u:%02u",
        (unsigned)JWPLC_Time.hour(),
        (unsigned)JWPLC_Time.minute(),
        (unsigned)JWPLC_Time.second());
}

extern "C" void jwplcUIEnter()
{
    if (g_expectUserEntry)
    {
        Serial.println("[UI ENTER] EXPECTED_BY_SKETCH");
        g_expectUserEntry = false;
        return;
    }

    g_unexpectedUserEntries++;
    Serial.print("[FAIL] UNEXPECTED_USER_ENTRY count=");
    Serial.println(g_unexpectedUserEntries);
}

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

    Serial.print("[UI PAGE] ");
    Serial.println(page);
}

extern "C" void jwplcUIExit()
{
    Serial.println("[UI EXIT] USER -> IDLE");
}

void setup()
{
    Serial.begin(115200);
    delay(300);

    Serial.println();
    Serial.println("=== ALPHA8 HARDWARE GATE - HMI + BUTTONS ===");
    Serial.println("FASE 1: dejar 3 minutos SIN tocar botones.");
    Serial.println("Esperado: IDLE estable y UNEXPECTED_USER_ENTRY=0.");
    Serial.println("FASE 2: OK = entrar USER desde el sketch.");
    Serial.println("FASE 3: RIGHT/LEFT = paginas 1/0.");
    Serial.println("FASE 4: en pagina 1, UP/DOWN = variar barra.");
    Serial.println("FASE 5: ESC = volver IDLE y tambien llegar al sketch.");
    Serial.println("Observar: sin flicker, congelamientos ni resets.");

    if (!JWPLC_Display.setFields(
            UI_FIELDS,
            sizeof(UI_FIELDS) / sizeof(UI_FIELDS[0])))
    {
        Serial.println("[FAIL] setFields()");
    }
    else
    {
        Serial.println("[PASS] setFields()");
    }

    // ON_DEMAND y ESC_ONLY forman parte del gate. No se fuerza wakeMode:
    // se valida que el default Alpha8 sea realmente IDLE_WAKE_DISABLED.
    JWPLC_Display.setUserRefreshMode(USER_REFRESH_ON_DEMAND);
    JWPLC_Display.setIdleReturnMode(IDLE_RETURN_ESC_ONLY);

    // Precargar valores mientras IDLE valida cache sin forzar USER/TFT HMI.
    updateRTCText();
    JWPLC_Display.setValue(FIELD_COUNT, counter);
    JWPLC_Display.setValue(FIELD_INPUTS, JWPLC_IO.inputs());
    JWPLC_Display.setValue(FIELD_TEMP, simulatedTemp);
    JWPLC_Display.setBool(FIELD_RUN, runState);
    JWPLC_Display.setBar(FIELD_BAR, barValue);
    JWPLC_Display.setText(FIELD_RTC, rtcText);

    Serial.print("[BOOT] displayReady=");
    Serial.print(JWPLC_Display.isReady() ? "YES" : "NO");
    Serial.print(" buttonsReady=");
    Serial.print(JWPLC_Display.buttonsReady() ? "YES" : "NO");
    Serial.print(" ioReady=");
    Serial.print(JWPLC_IO.ready() ? "YES" : "NO");
    Serial.print(" rtcPresent=");
    Serial.print(JWPLC_Time.present() ? "YES" : "NO");
    Serial.print(" rtcValid=");
    Serial.print(JWPLC_Time.valid() ? "YES" : "NO");
    Serial.print(" wakeMode=");
    Serial.println((int)JWPLC_Display.idleWakeMode());

    if (JWPLC_Display.idleWakeMode() == IDLE_WAKE_DISABLED)
    {
        Serial.println("[PASS] DEFAULT_IDLE_WAKE_DISABLED");
    }
    else
    {
        Serial.println("[FAIL] DEFAULT_IDLE_WAKE_DISABLED");
    }
}

void loop()
{
    static uint32_t lastSecond = 0;
    static bool lastIdle = true;
    static bool soak60Printed = false;
    static bool soak120Printed = false;
    static bool soak180Printed = false;

    const uint32_t now = millis();
    const bool idleNow = JWPLC_Display.isIdleMode();

    if (idleNow != lastIdle)
    {
        Serial.print("[MODE] ");
        Serial.println(idleNow ? "IDLE" : "USER");
        lastIdle = idleNow;
    }

    // Puede llamarse en cada loop: si el valor no cambia no hay redibujado TFT.
    JWPLC_Display.setValue(FIELD_INPUTS, JWPLC_IO.inputs());

    if ((uint32_t)(now - lastSecond) >= 1000)
    {
        lastSecond = now;

        counter++;
        simulatedTemp += 0.1f;

        if (simulatedTemp > 29.9f)
        {
            simulatedTemp = 20.0f;
        }

        runState = !runState;
        updateRTCText();

        JWPLC_Display.setValue(FIELD_COUNT, counter);
        JWPLC_Display.setValue(FIELD_TEMP, simulatedTemp);
        JWPLC_Display.setBool(FIELD_RUN, runState);
        JWPLC_Display.setText(FIELD_RTC, rtcText);

        Serial.print("[HB] t=");
        Serial.print(now);
        Serial.print(" mode=");
        Serial.print(idleNow ? "IDLE" : "USER");
        Serial.print(" inputs=0x");
        Serial.print(JWPLC_IO.inputs(), HEX);
        Serial.print(" rtc=");
        Serial.print(rtcText);
        Serial.print(" unexpectedUser=");
        Serial.println(g_unexpectedUserEntries);
    }

    // Soak inicial: solo es valido mientras no hubo interaccion fisica.
    if (!g_userInteractionStarted && idleNow)
    {
        if (!soak60Printed && now >= 60000UL)
        {
            soak60Printed = true;
            Serial.println("[SOAK] 60s IDLE estable");
        }

        if (!soak120Printed && now >= 120000UL)
        {
            soak120Printed = true;
            Serial.println("[SOAK] 120s IDLE estable");
        }

        if (!soak180Printed && now >= 180000UL)
        {
            soak180Printed = true;

            if (g_unexpectedUserEntries == 0)
            {
                Serial.println("[PASS] IDLE_SOAK_180S_NO_AUTOWAKE");
            }
            else
            {
                Serial.println("[FAIL] IDLE_SOAK_180S_NO_AUTOWAKE");
            }
        }
    }

    // Cada pressed() se consume una sola vez por el sketch. El Display observa
    // estados fisicos por separado, por lo que estas acciones validan que no
    // se roban latches entre HMI y aplicacion.
    if (JWPLC_Buttons.pressed(BTN_OK))
    {
        printButtonPress("OK", BTN_OK);

        if (JWPLC_Display.isIdleMode())
        {
            g_expectUserEntry = true;
            JWPLC_Display.enterUserUI();
        }
        else
        {
            Serial.println("[INFO] OK recibido con USER ya activo");
        }
    }

    if (JWPLC_Buttons.pressed(BTN_RIGHT))
    {
        printButtonPress("RIGHT", BTN_RIGHT);
        JWPLC_Display.setUserPage(1);
    }

    if (JWPLC_Buttons.pressed(BTN_LEFT))
    {
        printButtonPress("LEFT", BTN_LEFT);
        JWPLC_Display.setUserPage(0);
    }

    if (JWPLC_Buttons.pressed(BTN_UP))
    {
        printButtonPress("UP", BTN_UP);

        barValue += 10.0f;
        if (barValue > 100.0f)
        {
            barValue = 100.0f;
        }

        JWPLC_Display.setBar(FIELD_BAR, barValue);

        Serial.print("[BAR] ");
        Serial.println(barValue, 1);
    }

    if (JWPLC_Buttons.pressed(BTN_DOWN))
    {
        printButtonPress("DOWN", BTN_DOWN);

        barValue -= 10.0f;
        if (barValue < 0.0f)
        {
            barValue = 0.0f;
        }

        JWPLC_Display.setBar(FIELD_BAR, barValue);

        Serial.print("[BAR] ");
        Serial.println(barValue, 1);
    }

    if (JWPLC_Buttons.pressed(BTN_ESC))
    {
        // No llamamos goIdle(): el Display debe observar ESC por su cuenta y,
        // aun asi, el mismo press debe seguir llegando al sketch.
        printButtonPress("ESC", BTN_ESC);
    }

    if (JWPLC_Buttons.released(BTN_OK))
        printButtonRelease("OK");
    if (JWPLC_Buttons.released(BTN_RIGHT))
        printButtonRelease("RIGHT");
    if (JWPLC_Buttons.released(BTN_LEFT))
        printButtonRelease("LEFT");
    if (JWPLC_Buttons.released(BTN_UP))
        printButtonRelease("UP");
    if (JWPLC_Buttons.released(BTN_DOWN))
        printButtonRelease("DOWN");
    if (JWPLC_Buttons.released(BTN_ESC))
        printButtonRelease("ESC");

    delay(5);
}
