/*
  02.Display_HMI_Fields

  HMI USER compacta con cuatro tipos de campo Alpha8:
  - VALUE: valor numérico.
  - TEXT : texto.
  - BOOL : estado booleano.
  - BAR  : barra de 0 a 100 %.

  La HMI usa dirty redraw: setValue()/setText()/setBool()/setBar() sólo
  invalidan el campo cuando cambia su contenido formateado.

  Controles:
  - OK    : entra a USER desde IDLE.
  - RIGHT : alterna MOTOR OFF/ON.
  - UP    : aumenta NIVEL.
  - DOWN  : disminuye NIVEL.
  - ESC   : retorna a IDLE mediante el manejo interno del Display.
*/

#include <JWPLC_Display.h>

enum FieldId : uint8_t
{
    FIELD_COUNT = 1,
    FIELD_TEXT,
    FIELD_MOTOR,
    FIELD_LEVEL
};

static const JWPLC_UIField FIELDS[] = {
    JWPLC_UIValueField(
        FIELD_COUNT, 10, 18, "Contador", "",
        JWPLC_UIValueFormat(5, 0, false, false)),

    JWPLC_UITextField(
        FIELD_TEXT, 10, 52, "Estado", 12),

    JWPLC_UIBoolField(
        FIELD_MOTOR, 10, 86, "Motor",
        JWPLC_UIBoolText("OFF", "ON")),

    JWPLC_UIBarField(
        FIELD_LEVEL, 10, 120, "Nivel",
        JWPLC_UIRange(0.0f, 100.0f), 200, 28)};

static uint32_t counter = 0;
static bool motorOn = false;
static float level = 40.0f;

void setup()
{
    Serial.begin(115200);
    delay(300);

    JWPLC_Display.setIdleWakeMode(IDLE_WAKE_DISABLED);
    JWPLC_Display.setIdleReturnMode(IDLE_RETURN_ESC_ONLY);

    // ON_DEMAND evita refrescos gráficos si ningún campo cambió.
    JWPLC_Display.setUserRefreshMode(USER_REFRESH_ON_DEMAND);

    if (!JWPLC_Display.setFields(FIELDS, sizeof(FIELDS) / sizeof(FIELDS[0])))
    {
        Serial.println("ERROR: no se pudieron registrar los campos HMI");
        return;
    }

    JWPLC_Display.setValue(FIELD_COUNT, counter);
    JWPLC_Display.setText(FIELD_TEXT, "LISTO");
    JWPLC_Display.setBool(FIELD_MOTOR, motorOn);
    JWPLC_Display.setBar(FIELD_LEVEL, level);

    JWPLC_Buttons.clearPendingInput();

    Serial.println("JWPLC Basic - HMI fields");
    Serial.println("OK=USER | RIGHT=MOTOR | UP/DOWN=NIVEL | ESC=IDLE");
}

void loop()
{
    if (JWPLC_Buttons.pressed(BTN_OK) && JWPLC_Display.isIdleMode())
    {
        // Entrada USER explícita. El wake automático sigue deshabilitado.
        JWPLC_Display.enterUserUI();
    }

    if (JWPLC_Buttons.pressed(BTN_RIGHT))
    {
        motorOn = !motorOn;
        JWPLC_Display.setBool(FIELD_MOTOR, motorOn);
    }

    if (JWPLC_Buttons.pressed(BTN_UP))
    {
        level += 10.0f;
        if (level > 100.0f) level = 100.0f;
        JWPLC_Display.setBar(FIELD_LEVEL, level);
    }

    if (JWPLC_Buttons.pressed(BTN_DOWN))
    {
        level -= 10.0f;
        if (level < 0.0f) level = 0.0f;
        JWPLC_Display.setBar(FIELD_LEVEL, level);
    }

    static uint32_t lastUpdateMs = 0;
    if (millis() - lastUpdateMs >= 1000)
    {
        lastUpdateMs = millis();
        counter++;

        JWPLC_Display.setValue(FIELD_COUNT, counter);
        JWPLC_Display.setText(FIELD_TEXT, motorOn ? "MARCHA" : "PARADO");
    }

    delay(5);
}
