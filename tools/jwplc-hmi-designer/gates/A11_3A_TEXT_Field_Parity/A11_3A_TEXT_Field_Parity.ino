#include <JWPLC_Display.h>

// Alpha11 A11-3A
// Gate de paridad Designer <-> runtime usando exclusivamente API publica.
// No usa JWPLC_Display.tft() ni llamadas tft.*.

enum HMIFieldId : uint8_t
{
    FIELD_STATUS = 1
};

char estadoTexto[13] = "READY";

static const JWPLC_UIField HMI_FIELDS[] =
{
    JWPLC_UITextField(
        FIELD_STATUS,
        JWPLC_UIRect(20, 20),
        JWPLC_UIText("Estado", nullptr, 12),
        JWPLC_UITextFieldStyle(
            2,
            1,
            false,
            JWPLC_UI_LAYOUT_INLINE,
            JWPLC_UI_ALIGN_LEFT),
        0,
        JWPLC_UIColors(
            ST77XX_WHITE,
            ST77XX_CYAN,
            ST77XX_BLACK,
            ST77XX_WHITE))
};

void setup()
{
    Serial.begin(115200);
    delay(250);

    Serial.println();
    Serial.println("=== ALPHA11 A11-3A TEXT FIELD PARITY ===");
    Serial.println("Solo API publica JWPLC_Display / JWPLC_UITextField");
    Serial.println("Field: FIELD_STATUS / variable: estadoTexto[13]");
    Serial.println("X=20 Y=20 capacity=12 preview=READY");
    Serial.println("label=Estado valueSize=2 labelSize=1 INLINE LEFT frame=false");
    Serial.println("colors: label WHITE / value CYAN / bg BLACK / frame WHITE");

    const bool fieldsOk = JWPLC_Display.setFields(
        HMI_FIELDS,
        sizeof(HMI_FIELDS) / sizeof(HMI_FIELDS[0]));

    JWPLC_Display.setUserRefreshMode(USER_REFRESH_ON_DEMAND);
    JWPLC_Display.setUserPage(0);

    const bool textOk = JWPLC_Display.setText(FIELD_STATUS, estadoTexto);

    Serial.printf("setFields=%s\n", fieldsOk ? "PASS" : "FAIL");
    Serial.printf("setText=%s\n", textOk ? "PASS" : "FAIL");

    JWPLC_Display.enterUserUI();
}

void loop()
{
    // El Designer no genera jwplcUIUpdate().
    // Este gate mantiene el valor fijo para comparar geometria/pixeles.
    delay(50);
}
