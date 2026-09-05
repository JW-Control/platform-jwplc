#include <JWPLC_Display.h>

enum HMIFieldId : uint8_t
{
    FIELD_TEST_TEXT = 1
};

static const JWPLC_UIField HMI_FIELDS[] =
{
    JWPLC_UITextField(
        FIELD_TEST_TEXT,
        JWPLC_UIRect(20, 20),
        JWPLC_UIText(nullptr, nullptr, 12),
        JWPLC_UITextFieldStyle(
            2,
            1,
            false,
            JWPLC_UI_LAYOUT_INLINE,
            JWPLC_UI_ALIGN_LEFT),
        0,
        JWPLC_UIColors(
            ST77XX_RED,
            ST77XX_RED,
            ST77XX_WHITE,
            ST77XX_WHITE))
};

void setup()
{
    Serial.begin(115200);
    delay(300);

    Serial.println();
    Serial.println("=== ALPHA11 A11-2B PUBLIC API TEXT FIELD ===");
    Serial.println("Objetivo: validar borde/fondo usando solo API publica JWPLC_Display");
    Serial.println("Sin JWPLC_Display.tft() y sin llamadas tft.*");
    Serial.println("Field x=20 y=20 / textSize=2 / RED sobre WHITE / capacity=12");
    Serial.println("Geometria esperada actual: celda 144x16 + FIELD_PADDING=3 => field 150x22");

    const bool fieldsOk = JWPLC_Display.setFields(
        HMI_FIELDS,
        sizeof(HMI_FIELDS) / sizeof(HMI_FIELDS[0]));

    const bool textOk = JWPLC_Display.setText(
        FIELD_TEST_TEXT,
        "TEMP: 25.6 C");

    Serial.print("setFields=");
    Serial.println(fieldsOk ? "PASS" : "FAIL");
    Serial.print("setText=");
    Serial.println(textOk ? "PASS" : "FAIL");

    JWPLC_Display.setUserRefreshMode(USER_REFRESH_ON_DEMAND);
    JWPLC_Display.setIdleReturnMode(IDLE_RETURN_DISABLED);
    JWPLC_Display.setUserPage(0);
    JWPLC_Display.enterUserUI();
}

void loop()
{
    delay(5);
}
