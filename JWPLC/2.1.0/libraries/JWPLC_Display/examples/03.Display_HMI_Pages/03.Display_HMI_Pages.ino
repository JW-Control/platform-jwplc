/*
  03.Display_HMI_Pages

  Ejemplo de HMI con dos páginas y datos cacheados del runtime.

  Página 0: entradas digitales.
  Página 1: hora del RTC.

  Funciones mostradas:
  - setUserPage(page): cambia la página USER.
  - userPage(): consulta la página seleccionada.
  - JWPLC_IO / JWPLC_Time: datos cacheados, sin transacciones extra.

  Controles:
  - OK    : entra a USER.
  - LEFT  : página 0.
  - RIGHT : página 1.
  - ESC   : retorna a IDLE.
*/

#include <JWPLC_Display.h>

enum FieldId : uint8_t
{
    FIELD_INPUTS = 1,
    FIELD_INPUT0,
    FIELD_TIME,
    FIELD_RTC_VALID
};

static const JWPLC_UIField FIELDS[] = {
    JWPLC_UIValueField(
        FIELD_INPUTS, 20, 45, "Entradas", "",
        JWPLC_UIValueFormat(3, 0, false, false), 0),

    JWPLC_UIBoolField(
        FIELD_INPUT0, 20, 90, "I0_0",
        JWPLC_UIBoolText("LOW", "HIGH"), 0),

    JWPLC_UITextField(
        FIELD_TIME, 20, 45, "Hora", 12, 1),

    JWPLC_UIBoolField(
        FIELD_RTC_VALID, 20, 90, "RTC",
        JWPLC_UIBoolText("INVALID", "OK"), 1)};

static char timeText[12] = "--:--:--";

void setup()
{
    Serial.begin(115200);
    delay(300);

    JWPLC_Display.setIdleWakeMode(IDLE_WAKE_DISABLED);
    JWPLC_Display.setIdleReturnMode(IDLE_RETURN_ESC_ONLY);
    JWPLC_Display.setUserRefreshMode(USER_REFRESH_ON_DEMAND);

    if (!JWPLC_Display.setFields(FIELDS, sizeof(FIELDS) / sizeof(FIELDS[0])))
    {
        Serial.println("ERROR: setFields");
        return;
    }

    JWPLC_Display.setUserPage(0);
    JWPLC_Buttons.clearPendingInput();

    Serial.println("JWPLC Basic - HMI pages");
    Serial.println("OK=USER | LEFT=P0 | RIGHT=P1 | ESC=IDLE");
}

void loop()
{
    if (JWPLC_Buttons.pressed(BTN_OK) && JWPLC_Display.isIdleMode())
    {
        JWPLC_Display.enterUserUI();
    }

    if (JWPLC_Buttons.pressed(BTN_LEFT))
    {
        JWPLC_Display.setUserPage(0);
        Serial.println("PAGE 0");
    }

    if (JWPLC_Buttons.pressed(BTN_RIGHT))
    {
        JWPLC_Display.setUserPage(1);
        Serial.println("PAGE 1");
    }

    static uint32_t lastUpdateMs = 0;
    if (millis() - lastUpdateMs >= 500)
    {
        lastUpdateMs = millis();

        JWPLC_Display.setValue(FIELD_INPUTS, JWPLC_IO.inputs());
        JWPLC_Display.setBool(FIELD_INPUT0, JWPLC_IO.input(0));

        if (JWPLC_Time.valid())
        {
            snprintf(
                timeText, sizeof(timeText), "%02u:%02u:%02u",
                JWPLC_Time.hour(), JWPLC_Time.minute(), JWPLC_Time.second());
        }
        else
        {
            snprintf(timeText, sizeof(timeText), "--:--:--");
        }

        JWPLC_Display.setText(FIELD_TIME, timeText);
        JWPLC_Display.setBool(FIELD_RTC_VALID, JWPLC_Time.valid());
    }

    delay(5);
}
